#!/bin/bash
# 运行Mycraft并捕获输出，超时后自动杀掉
BINARY="/Users/yukirazhang/work/mycraft/build/Mycraft"
OUTFILE="/Users/yukirazhang/work/mycraft/tools/run_output.txt"
TIMEOUT=8

# 先清理残留进程
pkill -9 -f "Mycraft" 2>/dev/null
sleep 0.5

# 运行程序，stdout和stderr都捕获
$BINARY > "$OUTFILE" 2>&1 &
PID=$!
echo "启动PID: $PID"

# 等待指定秒数
sleep $TIMEOUT

# 检查进程状态
if kill -0 $PID 2>/dev/null; then
    echo "程序运行中，正在杀掉..."
    kill -9 $PID 2>/dev/null
    wait $PID 2>/dev/null
    echo "已杀掉"
else
    echo "程序已自行退出"
fi

# 输出日志
echo "=== 程序输出 ==="
cat "$OUTFILE"
echo "=== 输出结束 ==="

# 确认清理
pkill -9 -f "Mycraft" 2>/dev/null
echo "清理完成"
