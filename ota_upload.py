import http.server
import socketserver
import threading
import requests
import sys
import time
import os
import socket

# ==========================
# 用户配置
# ==========================
DEVICE_IP = "192.168.0.197"   # 修改为你的ESP32 IP
PORT = 8000
# ==========================

# 获取固件路径
FIRMWARE_PATH = sys.argv[1]
FILENAME = os.path.basename(FIRMWARE_PATH)

# 自动获取本机IP
def get_local_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
    finally:
        s.close()
    return ip

LOCAL_IP = get_local_ip()

# 切换目录到固件所在目录
os.chdir(os.path.dirname(FIRMWARE_PATH))

# ==========================
# HTTP 服务器（带进度显示）
# ==========================
class OTAHandler(http.server.SimpleHTTPRequestHandler):

    def log_message(self, format, *args):
        # 关闭默认日志输出（更干净）
        return

    @staticmethod
    def format_size(num_bytes):
        """三位有效数字，自动单位 B / kB / MB"""
        if num_bytes < 1000:
            return f"{num_bytes:.3g} B"
        elif num_bytes < 1000 ** 2:
            return f"{num_bytes / 1000:.3g} kB"
        else:
            return f"{num_bytes / (1000 ** 2):.3g} MB"

    def copyfile(self, source, outputfile):
        total = os.path.getsize(FILENAME)
        sent = 0
        bar_length = 40  # 进度条长度

        last_percent = -1

        while True:
            chunk = source.read(4096)
            if not chunk:
                break

            outputfile.write(chunk)
            sent += len(chunk)

            percent = sent / total
            filled = int(bar_length * percent)
            if filled < bar_length:
                bar = "=" * filled + ">" + " " * (bar_length - filled - 1)
            else:
                bar = "=" * filled

            progress_text = (
                f"\r[{bar}] {percent*100:5.1f}%  "
                f"{self.format_size(sent)} / {self.format_size(total)}"
            )
            print(progress_text, end="", flush=True)

        print("\r\nSend Complete")


# ==========================
# 启动服务器
# ==========================
httpd = socketserver.TCPServer(("", PORT), OTAHandler)

server_thread = threading.Thread(target=httpd.serve_forever)
server_thread.start()

time.sleep(1)

# ==========================
# 触发ESP32升级
# ==========================
trigger_url = f"http://{DEVICE_IP}/update?url=http://{LOCAL_IP}:{PORT}/{FILENAME}"

print("Trigger OTA...")

try:
    requests.get(trigger_url, timeout=2)
except requests.exceptions.RequestException:
    # ESP32 会在升级完成后重启，中断连接是正常的
    pass


# 等待下载完成（局域网一般几秒足够）
time.sleep(5)

# ==========================
# 关闭服务器并退出
# ==========================
httpd.shutdown()
httpd.server_close()

print("Upload task finished.")