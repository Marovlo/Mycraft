#!/bin/bash
# 运行 Mycraft 并在几秒后自动关闭
cd /Users/yukirazhang/work/mycraft

# 先杀掉可能残留的进程
pkill -f "Mycraft" 2>/dev/null
sleep 1

# 启动程序
./build/Mycraft > /tmp/mycraft_test_v2.log 2>&1 &
PID=$!
echo "启动 Mycraft PID: $PID"

# 等待5秒让程序初始化
sleep 5

# 检查进程是否还在运行
if kill -0 $PID 2>/dev/null; then
    echo "程序运行正常，正在关闭..."
    kill $PID 2>/dev/null
    sleep 2
    kill -9 $PID 2>/dev/null
else
    echo "程序已退出"
fi

echo "=== 程序日志 ==="
cat /tmp/mycraft_test_v2.log
echo "=== 日志结束 ==="
