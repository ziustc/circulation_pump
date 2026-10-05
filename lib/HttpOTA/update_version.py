import glob
import os

# 配置项
HEADER_FILE = "include/version.h"

# 版本号模板：工程里没有 include/version.h 时（新工程、或刚清过）由它复制一份过去。
# 里面的版本号就是"起始版本"——复制之后本次不再自增，所以第一个版本就是它写的
# 1.0.001，而不是 1.0.002。想让自己以后的工程从别的版本起算，改模板这一行即可。
#
# 路径和 HEADER_FILE 一样是相对工程根目录的（PlatformIO 跑本脚本时的工作目录）
TEMPLATE_FILE = "lib/HttpOTA/version_template.h"

# 同名的 version.h 出现在别的搜索路径上会盖住上面这个 —— PlatformIO 会把 lib/
# 下每个库的目录也加进头文件搜索路径，而且排在 include/ 前面。
# 一旦 lib/某库/version.h 存在，#include "version.h" 就撞上它，编译进去的是
# 它的版本号，而本脚本自增的却是 include/version.h —— 现象是"烧的是新版本，
# 日志打的是旧版本"，且完全静默。每次编译都查一遍，见到就喊。
# 告警用英文：构建控制台不一定是 UTF-8，中文会乱码成一个看不出内容的东西
def check_shadowed():
    for path in sorted(glob.glob("lib/*/version.h")):
        print("")
        print("!! [update_version] %s shadows %s on the include path (lib/ comes first)." % (path, HEADER_FILE))
        print("!! The firmware gets the version number from THAT file, while this script")
        print("!! keeps bumping %s -- delete the shadowing file." % HEADER_FILE)
        print("")

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
        # 【这里原来写的是 "new_version"】：那个变量只在上面找到宏的分支里赋值，
        # 走到这条路径就是 UnboundLocalError，构建会以 Python 报错中止。
        # 用现成的 default_version（'"1.0.001"\n'）
        if not version_found:
            if new_lines and not new_lines[-1].endswith('\n'):
                new_lines.append('\n')
            new_lines.append(f'{macro_prefix} {default_version}')

    # 3. 如果文件不存在：从模板复制一份（模板不在就退回脚本内置的默认内容）。
    #    本次不自增 —— 第一个版本的流水号就是模板里的起始值
    else:
        if os.path.exists(TEMPLATE_FILE):
            with open(TEMPLATE_FILE, "r", encoding="utf-8") as f:
                new_lines = [f.read()]
        else:
            print("!! [update_version] %s not found, falling back to the built-in default" % TEMPLATE_FILE)
            new_lines = [header_comment, f'{macro_prefix} {default_version}']

    # 写入
    with open(HEADER_FILE, "w", encoding="utf-8") as f:
        f.writelines(new_lines)

check_shadowed()
update_version()