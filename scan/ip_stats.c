#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_IP_LENGTH 16
#define INITIAL_CAPACITY 1000 // 初始容量

typedef struct Node {
    char ip[MAX_IP_LENGTH];
    int count;
} Node;

Node *nodes = NULL; // 动态数组
int total_nodes = 0;
int capacity = INITIAL_CAPACITY;

void insert_or_increment(const char *ip) {
    // 扩展数组容量
    if (total_nodes >= capacity) {
        capacity *= 2;
        nodes = realloc(nodes, capacity * sizeof(Node));
        if (nodes == NULL) {
            perror("内存扩展失败");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < total_nodes; i++) {
        if (strcmp(nodes[i].ip, ip) == 0) {
            nodes[i].count++;
            return;
        }
    }

    // 如果没有找到，则插入新节点
    strncpy(nodes[total_nodes].ip, ip, MAX_IP_LENGTH);
    nodes[total_nodes].count = 1;
    total_nodes++;
}

int compare(const void *a, const void *b) {
    Node *nodeA = (Node *)a;
    Node *nodeB = (Node *)b;
    return nodeB->count - nodeA->count; // 按照出现次数降序排序
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("用法: %s <文件名>\n", argv[0]);
        return EXIT_FAILURE;
    }

    FILE *file = fopen(argv[1], "r");
    if (file == NULL) {
        perror("无法打开文件");
        return EXIT_FAILURE;
    }

    // 动态分配初始数组
    nodes = malloc(INITIAL_CAPACITY * sizeof(Node));
    if (nodes == NULL) {
        perror("内存分配失败");
        return EXIT_FAILURE;
    }

    char ip[MAX_IP_LENGTH];

    // 逐行读取文件
    while (fgets(ip, MAX_IP_LENGTH, file) != NULL) {
        ip[strcspn(ip, "\n")] = 0; // 去掉换行符
        if (strlen(ip) == 0) {
            continue; // 跳过空行
        }
        insert_or_increment(ip);
    }

    fclose(file);

    // 排序结果
    qsort(nodes, total_nodes, sizeof(Node), compare);

    // 输出结果
    for (int i = 0; i < total_nodes; i++) {
        printf("%d - %s\n", nodes[i].count, nodes[i].ip);
    }

    // 释放内存
    free(nodes);
    return EXIT_SUCCESS;
}
