import os

# 配置项
HEADER_FILE = "include/version.h"

# 预定义的头部注释内容
header_comment = (
    "// 本文件为编译时自动更新的文件，用于更新软件版本号\n"
    "// 应在main.cpp中#include本文件\n"
    "//\n"
    "// 软件版本格式为 1.0.001，其中后三位固定为纯数字，为版本流水号，每次编译时自增1；\n"
    "// 用户可自定义后三位之前的版本号格式\n"
    )
macro_prefix = '#define SW_VERSION'
default_version = '"1.0.001"\n'

def update_version():
    lines = []
    new_lines = []
    version_found = False

    # 1. 检查文件是否存在
    if os.path.exists(HEADER_FILE):
        with open(HEADER_FILE, "r", encoding="utf-8") as f:
            lines = f.readlines()

        for i, line in enumerate(lines):
            new_lines.append(line)

            # 如果有以 #define SW_VERSION 开头的行
            if line.strip().startswith(macro_prefix):
                version_found = True

                # 读取宏定义的当前版本号，并去掉所有双引号
                version_str = line.strip()[len(macro_prefix):].strip().replace('"', '')
                
                # 分割版本号并处理末尾流水号
                version_parts = version_str.split(".")

                # 尝试将最后一部分转为整数并自增
                try:
                    serial_num = int(version_parts[-1]) + 1

                # 如果转换整数失败，则给这一行打注释，新增一行，从001开始（但前半部分仍可用）
                except ValueError:
                    new_lines[i] = '// ' + line
                    new_lines.append(line)
                    serial_num = 1

                # 重新格式化最后一段为至少3位数字
                version_parts[-1] = f"{serial_num:03d}"
                new_version = ".".join(version_parts)
                
                # 更新该行，统一补上双引号规范格式
                new_lines[-1] = f'{macro_prefix} "{new_version}"\n'

                break

        # 2. 如果文件存在但没找到该行，在末尾追加
        if not version_found:
            if new_lines and not new_lines[-1].endswith('\n'):
                new_lines.append('\n')
            new_lines.append(f'{macro_prefix} "{new_version}"\n')

    # 3. 如果文件不存在，生成完整文件
    else:
        new_lines = [header_comment, f'{macro_prefix} "1.0.001"\n']

    # 写入
    with open(HEADER_FILE, "w", encoding="utf-8") as f:
        f.writelines(new_lines)

update_version()