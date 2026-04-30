#!/usr/bin/env python3
"""
从 Minecraft 本地安装中提取音效文件到项目 assets/sounds/ 目录。
排除 music/（背景音乐，有版权）和 records/（唱片，有版权）。
"""

import json
import os
import shutil
import sys

# MC assets 路径
MC_ASSETS_DIR = os.path.expanduser("~/Library/Application Support/minecraft/assets")
INDEX_FILE = os.path.join(MC_ASSETS_DIR, "indexes", "30.json")
OBJECTS_DIR = os.path.join(MC_ASSETS_DIR, "objects")

# 输出目录
PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUTPUT_DIR = os.path.join(PROJECT_DIR, "assets", "sounds")

# 排除的目录（有版权的音乐内容）
EXCLUDED_PREFIXES = [
    "minecraft/sounds/music/",   # C418/Lena Raine 背景音乐
    "minecraft/sounds/records/", # 唱片音乐
]


def main():
    if not os.path.exists(INDEX_FILE):
        print(f"错误: 找不到索引文件 {INDEX_FILE}")
        print("请确保 Minecraft 已安装并下载了资源文件。")
        sys.exit(1)

    with open(INDEX_FILE, "r") as f:
        index_data = json.load(f)

    objects = index_data["objects"]

    # 筛选音效文件
    sound_entries = {}
    for key, value in objects.items():
        if not key.startswith("minecraft/sounds/"):
            continue
        # 排除有版权的音乐
        if any(key.startswith(prefix) for prefix in EXCLUDED_PREFIXES):
            continue
        sound_entries[key] = value

    print(f"找到 {len(sound_entries)} 个音效文件需要提取")

    # 创建输出目录
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    copied = 0
    skipped = 0
    errors = 0

    for key, value in sorted(sound_entries.items()):
        hash_str = value["hash"]
        # MC 存储方式: objects/前两位/完整hash
        hash_prefix = hash_str[:2]
        source_path = os.path.join(OBJECTS_DIR, hash_prefix, hash_str)

        # 输出路径: assets/sounds/ambient/cave/cave1.ogg
        relative_path = key.replace("minecraft/sounds/", "")
        dest_path = os.path.join(OUTPUT_DIR, relative_path)

        # 如果目标文件已存在且大小匹配，跳过
        if os.path.exists(dest_path) and os.path.getsize(dest_path) == value["size"]:
            skipped += 1
            continue

        if not os.path.exists(source_path):
            print(f"  警告: 源文件不存在 {source_path} (for {key})")
            errors += 1
            continue

        # 创建目标目录并复制
        os.makedirs(os.path.dirname(dest_path), exist_ok=True)
        shutil.copy2(source_path, dest_path)
        copied += 1

    print(f"\n完成!")
    print(f"  复制: {copied} 个文件")
    print(f"  跳过(已存在): {skipped} 个文件")
    print(f"  错误: {errors} 个文件")

    # 同时创建 music 目录结构（空的，为将来使用）
    music_dir = os.path.join(OUTPUT_DIR, "music")
    os.makedirs(os.path.join(music_dir, "game"), exist_ok=True)
    os.makedirs(os.path.join(music_dir, "menu"), exist_ok=True)
    print(f"\n已创建空的 music/ 目录结构（供将来添加无版权音乐使用）")


if __name__ == "__main__":
    main()
