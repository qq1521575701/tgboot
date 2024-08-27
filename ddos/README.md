
### 添加权限 编译全部c文件

    chmod +x *.sh && for file in *.c; do gcc "$file" -o "${file%.c}" -pthread; done && rm -rf *.c

### 反射文件类型
根据不同的协议类型，项目需要提供以下反射文件：

### SYN 协议:
文件: synip.txt
描述: 伪造的 IP 地址列表，反射攻击时将显示为源 IP。

### TCP 协议:
文件: tcpip.txt
描述: 中间盒反射 IP 地址列表，用于 TCP 协议的反射操作。

### UDP 协议:
文件: udpip.txt
描述: Meme 反射源 IP 地址列表，用于 UDP 协议的反射操作。
