// SPDX-License-Identifier: MulanPSL-2.0

/*
 * Vesper Entry
 *
 * 创建于 2023年12月26日 上海市嘉定区安亭镇
 */

#include <iostream>
#include <iomanip>
#include <sstream>
#include <thread>
#include <semaphore>
#include <memory>
#include <map>
#include <set>
#include <vector>
#include <string>

#include <signal.h>
#include <fcntl.h>
#include <unistd.h>

#include <wlr/version.h>

#include <rfb/rfb.h>

#include "config.h"
#include "log/Log.h"
#include "desktop/server/Server.h"
#include "vnc/Server.h"
#include "control/Server.h"

#include "./utils/wlroots-cpp.h"
#include "./common/MouseButton.h"

#include <pixman-1/pixman-version.h>
#include <wayland-version.h>

#include <sys/stat.h>
#include <sys/wait.h>

using namespace std;
using namespace vesper;
using namespace vesper::common;


/* ------------ 全局变量 ------------ */

/**
 * 外部传入的参数。
 */
static struct {
    /** example: --resolution-width 1920 */
    map<string, string> values;

    /** example: --headless */
    set<string> flags;
    
    /** 跟随在后面的独立值。例如，g++ -Wall -o lab lab.cpp lab2.cpp 中的 lab.cpp 和 lab2.cpp */
    vector<string> paimons;
    
    /** 环境变量。 */
    map<string, string> env;
} args;

static struct {
    desktop::server::Server desktop;
    vnc::Server vnc;
    control::Server control;
} servers;

static struct {
    bool enableVnc;
    bool enableControl;

    bool daemonize;
} options;

static vector<thread> activeThreads;  

/* ------------ 处理外部传入参数 ------------ */

static struct {
    bool initialized = false;
    set<string> flagKeys;
    set<string> valueKeys;
} argKeys;

static void loadPredefinedArgKeys() {
    struct {
        const char* key;
        bool isFlag = false;
    } keys[] = {
        { "--version", true },
        { "--usage", true },
        { "--help", true },

        { "--daemonize", true },
        { "--no-color", true },

        { "--headless", true },
        { "--add-virtual-display" },
        { "--use-pixman-renderer", true },
        { "--exec-cmds" },
        
        { "--enable-vnc", true },
        { "--vnc-auth-passwd" },
        { "--vnc-port" },
        { "--libvncserver-passwd-file" },

        { "--enable-ctrl", true },
        { "--ctrl-domain-socket" },
        
    };

    for (unsigned int i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        auto& it = keys[i];
        (it.isFlag ? &argKeys.flagKeys : &argKeys.valueKeys)->insert(it.key);
    }

    argKeys.initialized = true;
}


static int parseArgs(int argc, const char** argv) {

    if (!argKeys.initialized) {
        loadPredefinedArgKeys();
    }

    const auto& isKey = [] (string& s) {
        return s.starts_with("--");
    };

    auto& flags = args.flags;
    auto& values = args.values;
    auto& paimons = args.paimons;


    for (int idx = 1; idx < argc; idx++) {
        string key = argv[idx];
        if (argKeys.flagKeys.contains(key)) {
            if (flags.contains(key)) {
                LOG_WARN("flag redefined: ", key);
            } else {
                flags.insert(key);
            }

            continue;
        } else if (argKeys.valueKeys.contains(key)) {
            if (idx + 1 == argc) {
                LOG_ERROR("no value for key ", key);
                return -1;
            }

            string value = argv[++idx];
            if (isKey(value)) {
                LOG_ERROR("key ", key, " followed by another key ", value);
                return -1;
            }

            if (values.contains(key)) {
                LOG_WARN("key ", key, " redefined. ");
                LOG_WARN("  value |", values[key], "| is replaced by |", value, "|.");
            }

            values[key] = value;
        } else if (isKey(key)) {
            LOG_ERROR("key ", key, " is not defined.")
            return -1;
        } else {
            paimons.push_back(key);
        }
    }


    return 0;
}

static void processEnvVars(const char** env) {
    for (int i = 0; env[i]; i++) {
        string it = env[i];
        auto splitPos = it.find('=');
        if (splitPos == string::npos) {
            LOG_WARN("no '=' in env variable: ", it);
            continue;
        }
        args.env[it.substr(0, splitPos)] = it.substr(splitPos + 1);
    }
}


/* ------------ 方法 ------------ */

static void usage() {
    cout << "vesper core " << VESPER_VERSION_NAME << endl;
    cout << "check ";
    ConsoleColorPad::setCoutColor(0x2f, 0x90, 0xb9);
    cout << "https://github.com/FlowerBlackG/vesper";
    ConsoleColorPad::setCoutColor();
    cout << " for detailed usage." << endl;
}


static void version(bool useLog) {
    if (useLog) {
        LOG_INFO(
            "vesper version : ", 
            VESPER_VERSION_NAME, 
            " (", VESPER_VERSION_CODE,")"
        );

        LOG_INFO("wlroots version: ", WLR_VERSION_STR);
        LOG_INFO("pixman version : ", PIXMAN_VERSION_STRING);
        LOG_INFO("wayland version: ", WAYLAND_VERSION);

    } else {
        
        cout << "vesper core " << VESPER_VERSION_NAME 
            << " (" << VESPER_VERSION_CODE << ")" << endl;
        cout << "------------" << endl;
        cout << "wlroots: " << WLR_VERSION_STR << endl;
        cout << "pixman : " << PIXMAN_VERSION_STRING << endl;
        cout << "wayland: " << WAYLAND_VERSION << endl;
    
    }
}


static int processPureQueryCmds() {
    if (args.flags.contains("--version")) {
        version(false);
        return 1;
    } 
    
    if (args.flags.contains("--usage") || args.flags.contains("--help")) {
        usage();
        return 1;
    }
    return 0;
}


static int buildDesktopOptions() {
    auto& globalOpts = options;
    auto& options = servers.desktop.options;

    options.backend.headless = args.flags.contains("--headless");

    options.backend.virtualOutput.add = args.values.contains("--add-virtual-display");
    if (options.backend.headless && !options.backend.virtualOutput.add) {
        LOG_WARN("--headless should be paired with --add-virtual-display.");
    }

    if (options.backend.virtualOutput.add) {
        string vDisplayResolution = args.values["--add-virtual-display"];
        size_t posOfStar = vDisplayResolution.find('*');
        
        if (posOfStar == string::npos) {
            LOG_ERROR("failed to parse --add-virtual-display");
            return 1;
        }

        string widthStr = vDisplayResolution.substr(0, posOfStar);
        string heightStr = vDisplayResolution.substr(posOfStar + 1);

        try {
            options.backend.virtualOutput.width = stoi(widthStr);
            options.backend.virtualOutput.height = stoi(heightStr);
        } catch(...) {
            LOG_ERROR("failed to convert virtual display resolution.")
            return 1;
        }

    }

    options.renderer.pixman = options.backend.headless 
        || args.flags.contains("--use-pixman-renderer");

    // exec cmds

    if (args.values.contains("--exec-cmds")) {
        try {
            string execCmds = args.values["--exec-cmds"];
            size_t pos = execCmds.find(',');
            if (pos == string::npos) {
                throw 0;
            }

            int n = stoi(execCmds.substr(0, pos));
            execCmds = execCmds.substr(pos + 1);
            pos = execCmds.find_last_of(',');

            if (pos == string::npos) {
                throw 0;
            }

            string packedCmds = execCmds.substr(pos + 1);
            execCmds = execCmds.substr(0, pos); // 此时 execCmds 是命令长度序列。

            size_t lenSum = 0;
            
            while (n--) {
                pos = execCmds.find(',');
                size_t len;
                if (n == 0) {
                    pos = stoi(execCmds);
                } else if (pos == string::npos) {
                    throw 0;
                } else {
                    len = stoi(execCmds.substr(0, pos));
                    execCmds = execCmds.substr(pos + 1);
                }

                if (lenSum + len > packedCmds.length()) {
                    throw 0;
                }

                options.launch.apps.push_back(packedCmds.substr(lenSum, len));
                lenSum += len;
            }

        } catch(...) {
            LOG_ERROR("failed to parse --exec-cmds");
            return -1;
        }   
    } // if (args.values.contains("--exec-cmds"))

    options.output.alwaysRenderEntireScreen = true;
    options.output.exportScreenBuffer = globalOpts.enableVnc;
    options.output.forceRenderSoftwareCursor = globalOpts.enableVnc;

    return 0;

}

static int buildVncOptions() {

    auto& options = servers.vnc.options;

    // 分辨率需要等虚拟屏幕创建后才能用。

    options.screenBuffer.width = 0;
    options.screenBuffer.height = 0;

    options.net.port = 5900;
    if (args.values.contains("--vnc-port")) {
        try {
            int port = stoi(args.values["--vnc-port"]);
            options.net.port = port;
        } catch(...) {
            LOG_WARN("failed to parse --vnc-port. using default one.");
        }
    }

    if (args.values.contains("--vnc-auth-passwd")) {
        const char* libvncserverPasswdFile = "--libvncserver-passwd-file";
        if (!args.values.contains(libvncserverPasswdFile)) {
            LOG_ERROR("--vnc-auth-passwd should be paired with ", libvncserverPasswdFile);
            return 1;
        }

        options.auth.password = args.values["--vnc-auth-passwd"];
        options.auth.libvncserverPasswdFile = args.env["XDG_RUNTIME_DIR"];
        options.auth.libvncserverPasswdFile += '/';
        options.auth.libvncserverPasswdFile += args.values[libvncserverPasswdFile];
    }
    
    options.screenBuffer.getBuffer = [] () {
        void* buf = servers.desktop.getFramebuffer(0);
        return buf;
    };

    options.screenBuffer.recycleBuffer = [] (void* buf) {
        servers.desktop.recycleFramebuffer(buf, 0);
    };

    servers.vnc.options.eventHandlers.mouse.motion = [] (
        bool absolute, double absoluteX, double absoluteY,
        bool delta, int deltaX, int deltaY
    ) {
        servers.desktop.moveCursorAsyncArgsMutex.acquire();
        auto& args = servers.desktop.moveCursorAsyncArgs;
        args.absolute = absolute;
        args.absoluteX = absoluteX;
        args.absoluteY = absoluteY;
        args.delta = delta;
        args.deltaX = deltaX;
        args.deltaY = deltaY;
        servers.desktop.moveCursorAsync();
        servers.desktop.moveCursorAsyncArgsMutex.release();
    };

    servers.vnc.options.eventHandlers.mouse.button = [] (
        bool press, MouseButton button
    ) {
        servers.desktop.pressMouseButtonAsyncArgsMutex.acquire();
        auto& args = servers.desktop.pressMouseButtonAsyncArgs;
        args.press = press;
        args.button = button;
        servers.desktop.pressMouseButtonAsync();
        servers.desktop.pressMouseButtonAsyncArgsMutex.release();
    };

    servers.vnc.options.eventHandlers.mouse.axis = [] (
        bool vertical, double delta, int32_t deltaDiscrete
    ) {
        servers.desktop.scrollAsyncArgsMutex.acquire();
        auto& args = servers.desktop.scrollAsyncArgs;
        args.vertical = vertical;
        args.delta = delta;
        args.deltaDiscrete = deltaDiscrete;
        servers.desktop.scrollAsync();
        servers.desktop.scrollAsyncArgsMutex.release();
    };

    servers.vnc.options.eventHandlers.keyboard.key = [] (
        bool pressed, xkb_keysym_t keysym
    ) {
        servers.desktop.keyboardInputAsyncArgsMutex.acquire();
        auto& args = servers.desktop.keyboardInputAsyncArgs;
        args.keysym = keysym;
        args.pressed = pressed;
        servers.desktop.keyboardInputAsync();
        servers.desktop.keyboardInputAsyncArgsMutex.release();
    };

    return 0;
}

static int buildControlOptions() {

    /* set ctrl domain socket path */

    const char* xdgRuntimeDir = "XDG_RUNTIME_DIR";
    if (args.env.count(xdgRuntimeDir) == 0) {
        LOG_ERROR("$", xdgRuntimeDir, " not found!")
        return 1;
    }

    const char* ctrlDomainSocket = "--ctrl-domain-socket";
    if (args.values.count(ctrlDomainSocket) == 0) {
        LOG_ERROR(ctrlDomainSocket, " not set!")
        return 1;
    }

    auto& options = servers.control.options;
    
    options.socketPath = args.env[xdgRuntimeDir];
    options.socketPath += '/';
    options.socketPath += args.values[ctrlDomainSocket];


    /* hooks */

    options.hooks.terminateVesper = [] () {
        servers.desktop.terminateAsync();
        servers.vnc.terminate();
    };

    return 0;
}


static int buildOptions() {

    // 确保环境变量 XDG_RUNTIME_DIR 正常。

    const char* xdgRuntimeDir = "XDG_RUNTIME_DIR";
    if (!args.env.contains(xdgRuntimeDir)) {
        LOG_ERROR("$", xdgRuntimeDir, " is required!");
        return 1;
    }

    options.enableVnc = args.flags.contains("--enable-vnc");
    options.enableControl = args.flags.contains("--enable-ctrl");
    options.daemonize = args.flags.contains("--daemonize");

    if (buildDesktopOptions()) {
        return 1;
    }

    if (options.enableVnc
        && servers.desktop.options.backend.headless 
        && !servers.desktop.options.backend.virtualOutput.add
    ) {
        LOG_WARN("compositor comes with no display!")
        LOG_WARN("VNC disabled due to this error!")
        options.enableVnc = false;
    }

    if (options.enableVnc && buildVncOptions()) {
        return 2;
    }

    if (options.enableControl && buildControlOptions()) {
        return 3;
    }

    return 0;
}


/* ------------ 守护进程 ------------ */

static int daemonize() {
    pid_t pid = fork();
    if (pid < 0) {
        LOG_ERROR("failed to daemonize!");
        return -1;
    }

    // 父进程结束运行。
    if (pid != 0) {
        exit(0);
    }

    // 创建新会话。
    if ((pid = setsid()) < -1) {
        LOG_ERROR("failed to set sid!");
        return -1;
    }

    // 更改当前工作目录。
    chdir("/");

    umask(0);
    signal(SIGTERM, [] (int arg) {
        servers.desktop.terminateAsync();
        servers.vnc.terminate();
        servers.control.terminate();
    });

    return 0;
}



/* ------------ "析构函数" ------------ */

static void cleanup() {

}

/* ------------ 程序进入点 ------------ */

int main(int argc, const char* argv[], const char* env[]) {

    ConsoleColorPad::disableColor();

    /* 命令行解析。 */

    if (int res = parseArgs(argc, argv)) {
        usage();
        return res;
    }

    ConsoleColorPad::setNoColor(args.flags.contains("--no-color"));

    processEnvVars(env);

    if (int res = processPureQueryCmds()) {
        return res;
    }

    if (int res = buildOptions()) { 
        usage(); 
        return res; 
    }

    if (options.daemonize) {
        daemonize();
    }

    version(true);

    LOG_INFO("creating desktop server")

    /* 启动各模块。 */

    auto& vnc = servers.vnc;
    auto& desktop = servers.desktop;
    auto& control = servers.control;
    

    auto& desktopResult = desktop.options.result;
    auto& vncResult = vnc.options.result;
    auto& controlResult = control.options.result;

    /* launch Desktop module */

    activeThreads.emplace_back(
        [] () {
            int res = desktop.run();
            LOG_INFO("desktop server exited with code: ", res);
            control.terminate();
            vnc.terminate();
        }
    );
    
    desktopResult.signals.serverLaunched.acquire();
    
    if (desktopResult.code) {
        LOG_ERROR("server failed with code: ", desktopResult.code);
        return -1;
    } else {
        LOG_INFO("desktop launched with code: ", desktopResult.code)
    }


    /* launch Control module */

    if (options.enableControl) {
        activeThreads.emplace_back(
            [] () {
                int res = control.run();
                LOG_INFO("control server exited with code: ", res);
            }
        );

        controlResult.serverLaunchedSignal.acquire();
        LOG_INFO("control launched with code: ", controlResult.code)
    }


    /* launch VNC module */

    if (options.enableVnc) {
        desktopResult.signals.firstDisplayAttached.acquire();

        auto& vncOptsSB = servers.vnc.options.screenBuffer;
        vncOptsSB.width = desktopResult.firstDisplayResolution.width;
        vncOptsSB.height = desktopResult.firstDisplayResolution.height;
        
        activeThreads.emplace_back(
            [] () {
                int res = vnc.run();
                LOG_INFO("vnc server exited with code: ", res);
            }
        );

        vncResult.serverLaunchedSignal.acquire();
        LOG_INFO("vnc launched with code: ", vncResult.code)
    }
    
    
    for (auto& it : activeThreads) {
        it.join();
    }

    LOG_INFO("bye~");

    cleanup();
    return 0;
}
