import sys
import collections

def analyze_file(filename):
    # 读取文件内容
    with open(filename, 'r') as file:
        lines = file.readlines()

    # 用于存储统计信息
    ip_count = collections.defaultdict(int)
    ip_total_size = collections.defaultdict(int)

    # 处理每一行
    for line in lines:
        line = line.strip()
        if line:
            ip, value = line.split('-')
            value = int(value)
            
            # 统计每个 IP 出现的次数
            ip_count[ip] += 1
            
            # 统计每个 IP 的总大小
            ip_total_size[ip] += value

    # 输出统计结果
    for ip in ip_count:
        print(f"{ip} 次数: {ip_count[ip]} 总大小: {ip_total_size[ip]}")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python3 log_analyzer.py <filename>")
    else:
        filename = sys.argv[1]
        analyze_file(filename)
