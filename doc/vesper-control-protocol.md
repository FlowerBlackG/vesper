# Vesper Control 通信协议

vesper control 模块是外层控制 vesper 工作的重要窗口。

本节中，协议格式表格的对齐单位都是 Byte。协议规定所有数字采用 Big Endian 格式存储。

表格中，从左往右、从上往下是字节流发送数据。即：第一行最左侧是最早发送的数据；第二行最左侧的信息紧随在第一行最右侧数据后发送。

**通用返回格式**是服务器向客户端发送应答时使用的。其他协议都是客户端向服务器发送数据时使用的。

## 协议头（Header）

每个命令的开头都需要传输一个协议头。

```
    4B        4B
+---------+---------+
|  magic  |  type   |
+---------+---------+
|       length      |
+-------------------+
```

* magic (uint32): 协议魔数。固定为 `KpBL`
* type (uint32): 指令识别码。见具体指令
* length (uint64): 报文总长度（不含 header）

## 0xA001: 通用返回格式

`Response`

```
  8 Bytes
+-------------------+
|       header      |
+-------------------+
|       header      |
+---------+---------+
|  code   | msg len |
+---------+---------+
|      msg          |
|      ...          |

```

* type (uint32): `0xA001`
* code (uint32): 状态码。0 表示正常
* msg len (uint32): 返回信息长度。单位为 Byte
* msg (byte array): 返回信息。

通用返回报文可以用于传递数据。当 code 非 0 时，数据内容仅作为报错信息；当 code 为 0 时，msg 的含义与解析方式依不同协议而不同。

## 0x0001: 关闭 vesper

`TerminateVesper`

```
    8 Bytes
+----------------+
|     header     |
+----------------+
|     header     |
+----------------+
```

* type (uint32): `0x0001`

该指令正确执行后，vesper 会返回一个空 response 报文（当然，返回报文的 code 应该是 0）。

## 0x0101: 读取 VNC 端口

`GetVNCPort`

```
    8 Bytes
+----------------+
|     header     |
+----------------+
|     header     |
+----------------+
```

response: msg 为用字符串表示的端口号

## 0x0102: 读取 VNC 密码

`GetVNCPassword`

```
    8 Bytes
+----------------+
|     header     |
+----------------+
|     header     |
+----------------+
```

response: msg 为一个字符串

## 0x0201: 获取版本信息

GetVesperVersion

```
    8 Bytes
+----------------+
|     header     |
+----------------+
|     header     |
+----------------+

```

response: 字符串。其组成为：`${version name},${version code},${build time}`

response 例：

```
0.0.1-dev,1,
```
