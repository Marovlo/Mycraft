#!/bin/bash
# 运行Mycraft测试
export PATH="/opt/homebrew/bin:/usr/local/bin:$PATH"
pkill -9 -f "Mycraft" 2>/dev/null
sleep 1
cd /Users/yukirazhang/work/mycraft
./build/Mycraft > /tmp/mycraft_vanilla_test.log 2>&1 &
PID=$!
echo "PID: $PID"
sleep 8
kill -9 $PID 2>/dev/null
sleep 1
echo "=== done ==="
pgrep -f "Mycraft" || echo "进程已终止"
echo "=== 日志最后20行 ==="
tail -20 /tmp/mycraft_vanilla_test.log
