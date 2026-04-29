#!/bin/bash
export PATH="/opt/homebrew/bin:/opt/homebrew/Cellar/cmake/4.2.0/bin:$PATH"

pkill -9 -f "Mycraft" 2>/dev/null
sleep 1

cd /Users/yukirazhang/work/mycraft

echo "=== Building ===" > /tmp/mc_out.log
cmake --build build --parallel 2>&1 | tail -20 >> /tmp/mc_out.log
echo "Build exit: $?" >> /tmp/mc_out.log

echo "=== Running ===" >> /tmp/mc_out.log
./build/Mycraft >> /tmp/mc_out.log 2>&1 &
MPID=$!
echo "PID: $MPID" >> /tmp/mc_out.log
sleep 5

kill -9 $MPID 2>/dev/null
sleep 1
echo "=== Done ===" >> /tmp/mc_out.log

cat /tmp/mc_out.log
