# 添加一个控制报文

添加的协议仅用于接收来自外部的信息。返回信息统一使用 `Response` 报文。

## 编写说明

前往 `vesper/doc/vesper-control-protocol.md` 编写协议说明

## Vesper Control 协议实现

### control/Protocols.h

新建一个继承自 `Base` 的类，登记 `typeCode`，并实现必要的成员。

在开头处 `VESPER_CTRL_EXPAND_RECV_PROTOS` 下面登记协议。

### control/Server.cpp

实现函数 `int Server::processXXX(protocol::XXX*, int connFd)`

## Vesper Center 协议实现

vesper 协议模块位于：`com.gardilily.vespercenter.service.vesperprotocol`

### VesperControlProtocols.kt

实现 `class XXX : Base()`，并登记 `typeCode`。

模板：

```kotlin
class MyNewProto : Base() {
    companion object {
        const val typeCode = TODO()  // unsigned Int
    }

    override val type get() = typeCode
    override val bodyLength get() = TODO()  // unsigned Long

    override fun encodeBody(container: ArrayList<Byte>) {
        // TODO
    }
}
```
