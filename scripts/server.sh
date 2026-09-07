#!/usr/bin/env bash
# ============================================================================
# server.sh —— 服务管理脚本（角色 A）
#
# 用法：./scripts/server.sh {start|stop|restart|status}
#
#   start    后台启动 smart_home_server，记录 PID 到 server/server.pid
#   stop     发送 SIGTERM 优雅退出（服务器 main.cc 已注册该信号），并清理 PID
#   restart  stop + start
#   status   显示服务运行状态
#
# 说明：
#   - 脚本约定从项目根目录执行（内部自动 cd 到根目录）。
#   - 可执行文件由 CMake 输出到项目根目录（见 server/CMakeLists.txt）。
#   - 日志追加写入 server/log/server.log。
# ============================================================================
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

SERVER_BIN="$ROOT_DIR/smart_home_server"
PID_FILE="$ROOT_DIR/server/server.pid"
LOG_FILE="$ROOT_DIR/server/log/server.log"

usage() {
    echo "用法: $0 {start|stop|restart|status}"
    exit 1
}

# 判断服务是否在运行：PID 文件存在且进程存活。
is_running() {
    if [ -f "$PID_FILE" ]; then
        local pid
        pid="$(cat "$PID_FILE" 2>/dev/null || true)"
        if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
            return 0
        fi
    fi
    return 1
}

do_start() {
    if is_running; then
        echo "服务已在运行 (pid=$(cat "$PID_FILE"))"
        return 0
    fi
    if [ ! -x "$SERVER_BIN" ]; then
        echo "找不到可执行文件: $SERVER_BIN"
        echo "请先运行: ./scripts/deploy.sh"
        exit 1
    fi
    mkdir -p server/log server/data
    echo "启动服务..."
    nohup "$SERVER_BIN" >>"$LOG_FILE" 2>&1 &
    echo $! > "$PID_FILE"
    sleep 1
    if is_running; then
        echo "服务已启动 (pid=$(cat "$PID_FILE"))"
    else
        echo "服务启动失败，请查看日志: $LOG_FILE"
        rm -f "$PID_FILE"
        exit 1
    fi
}

do_stop() {
    if ! is_running; then
        echo "服务未运行"
        rm -f "$PID_FILE"
        return 0
    fi
    local pid
    pid="$(cat "$PID_FILE")"
    echo "停止服务 (pid=$pid)..."
    kill "$pid" 2>/dev/null || true
    # 等待优雅退出，最多 10 秒
    for _ in $(seq 1 10); do
        if ! kill -0 "$pid" 2>/dev/null; then
            break
        fi
        sleep 1
    done
    if kill -0 "$pid" 2>/dev/null; then
        echo "进程未退出，强制停止..."
        kill -9 "$pid" 2>/dev/null || true
    fi
    rm -f "$PID_FILE"
    echo "服务已停止"
}

do_status() {
    if is_running; then
        echo "服务运行中 (pid=$(cat "$PID_FILE"))"
    else
        echo "服务未运行"
    fi
}

case "${1:-}" in
    start)   do_start ;;
    stop)    do_stop ;;
    restart) do_stop; do_start ;;
    status)  do_status ;;
    *)       usage ;;
esac
