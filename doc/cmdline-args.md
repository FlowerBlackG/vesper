# Vesper Core 命令行参数

## 通用参数

### --version

显示 vesper 及依赖组件的版本并退出。

当检测到此 flag 时，其他所有的命令行参数将被忽略。

### --usage / --help

显示使用帮助。

当此 flag 设置，且未设置 --version 时，将忽略其他所有命令行参数。

### --daemonize

以守护进程模式运行。

### --no-color

不要输出带颜色的日志信息。

### --log-to [value]

将日志输出到指定地点（如指定文件）。

value: 目标文件路径。

## Vesper Desktop 参数

### --headless

离屏渲染。需要额外传入 --add-virtual-display [resolution] 才有效。

### --add-virtual-display [width*height]

适用于 --headless 启用的情况。

width 和 height 需要是合理的整数。

例：

```bash
./vesper --headless --add-virtual-display 1280*720
```

### --use-pixman-renderer

--headless 启用时，该参数自动被启用。

### --exec-cmds [cmds]

应用启动指令。cmds 需要是一整个命令行参数被传入。

cmds 格式如下：

```bash
CMD1,CMD2,CMD3,...
```

即，启动命令之间以 `,` 隔开。

例：

```bash
./vesper --exec-cmds "konsole, dolphin, LIBGL_ALWAYS_SOFTWARE=1 kgx"
```

如果启动命令中包含 `,` ，可加入一个 `+` 来转译。

如果启动命令中包含 `+` ，则可在其前加入一个 `+` 来转译。若单个 `+` 之后跟随的不是 `,`，其也会被正常当作一个 `+` 识别。

`,` 前后的空格会被忽略。

## Vesper VNC 参数

### --enable-vnc

启用 vnc 功能。

### --vnc-auth-passwd [value]

vnc 登录密码。不设置此参数时，无密码。

### --vnc-port [value]

vnc 监听端口号。

### --libvncserver-passwd-file [value]

libVNCServer 存储密码文件的路径。

实际路径：`${XDG_RUNTIME_DIR}/[value]`

## Vesper Control 参数

### --enable-ctrl

启用 ctrl 功能。

### --ctrl-domain-socket [value]

指定 vesper control 创建的 domain socket 名称。实际的 domain socket 文件将位于 `${XDG_RUNTIME_DIR}/[value]`

例：

```bash
./vesper --control-domain-socket vesper.sock
```
