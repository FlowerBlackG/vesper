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
#include "./bindings/pixman.h"

#include <pixman-1/pixman-version.h>
#include <wayland-version.h>

#include <sys/stat.h>
#include <sys/wait.h>

using namespace std;
using namespace vesper;
using namespace vesper::common;
using namespace vesper::bindings;


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
        { "--log-to", false },


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

        LOG_INFO("vesper-core complication time: ", VESPER_BUILD_TIME_HUMAN_READABLE);

        LOG_INFO("wlroots version: ", WLR_VERSION_STR);
        LOG_INFO("pixman version : ", PIXMAN_VERSION_STRING);
        LOG_INFO("wayland version: ", WAYLAND_VERSION);

    } else {
        
        cout << "vesper-core " << VESPER_VERSION_NAME 
            << " (" << VESPER_VERSION_CODE << ")" << endl
            << "" << VESPER_BUILD_TIME_HUMAN_READABLE << endl;
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


static int parseExecCmds(const string& fullCmd, vector<string>& container) {
    const auto isBlank = [] (int ch) { 
        return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
    }; // const auto isBlank = [] (int ch)


    const auto addCmd = [&isBlank, &container] (string& cmd) {
        while (cmd.length() && isBlank(cmd.back())) {
            cmd.pop_back();
        }

        if (cmd.length()) {
            container.push_back(cmd);
        }
    }; // const auto addCmd = [&isBlank] (string& cmd, vector<string>& container)


    string cmd;
    size_t idx = 0;

    while (idx < fullCmd.length()) {
        int ch = fullCmd[idx++];

        if (cmd.empty() && isBlank(ch)) {
            continue;
        }

        if (ch == '+') {
            int nextCh = (idx == fullCmd.length()) ? -1 : fullCmd[idx];
            if (nextCh == ',' || nextCh == '+') {
                cmd.push_back(nextCh);
                idx++; // consumes nextCh
            } else {
                cmd.push_back(ch); // keep the original ch: '+'
            }
        } else if (ch != ',') { // also, ch is not '+'
            cmd.push_back(ch);
        } else { // ch is exactly ',', split command.
            addCmd(cmd);
            cmd.clear();
        }

    } // while (idx < fullCmd.length())

    addCmd(cmd);

    return 0;

} // static int parseExecCmds(const string& fullCmd, vector<string>& container)


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
        if (parseExecCmds(args.values["--exec-cmds"], options.launch.apps)) {
            LOG_ERROR("failed to parse --exec-cmds !");
            return -1;
        }
    } // if (args.values.contains("--exec-cmds"))

    if (globalOpts.enableVnc) {
        options.runtimeCtrl.enabled = true;
    }

    options.output.alwaysRenderEntireScreen = false;  // todo

    options.output.exportScreenBuffer = globalOpts.enableVnc;
    options.output.forceRenderSoftwareCursor = false;  // todo

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
    
    options.screenBuffer.getBuffer = [] (pixman::Region32& damage) -> void* {
        return servers.desktop.getFramebuffer(0, damage);
    };

    options.screenBuffer.recycleBuffer = [] (void* buf) {
        servers.desktop.recycleFramebuffer(buf, 0);
    };

    servers.vnc.options.eventHandlers.mouse.motion = [] (
        bool absolute, double absoluteX, double absoluteY,
        bool delta, int deltaX, int deltaY
    ) {
        servers.desktop.moveCursorAsync(
            absolute, absoluteX, absoluteY, delta, deltaX, deltaY
        );
    };

    servers.vnc.options.eventHandlers.mouse.button = [] (
        bool press, MouseButton button
    ) {
        servers.desktop.pressMouseButtonAsync(press, button);
    };

    servers.vnc.options.eventHandlers.mouse.axis = [] (
        bool vertical, double delta, int32_t deltaDiscrete
    ) {
        servers.desktop.scrollAsync(vertical, delta, deltaDiscrete);
    };

    servers.vnc.options.eventHandlers.keyboard.key = [] (
        bool pressed, xkb_keysym_t keysym
    ) {
        servers.desktop.keyboardInputAsync(keysym, pressed);
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

    options.hooks.getVNCPort = [&] () {
        return servers.vnc.options.net.port;
    };


    options.hooks.getVNCPassword = [&] () -> string& {
        return servers.vnc.options.auth.password;
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
    chdir("~");

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
    if (args.values.contains("--log-to")) {
        string& logTo = args.values["--log-to"];
        freopen(logTo.c_str(), "w", stderr);
    }

    processEnvVars(env);

    if (processPureQueryCmds()) {
        return 0;
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
