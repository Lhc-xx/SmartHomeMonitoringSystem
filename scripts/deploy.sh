#!/usr/bin/env bash
# ============================================================================
# deploy.sh —— 一键部署脚本（角色 A）
#
# 功能：
#   1. 依赖检查（cmake / g++ / mysqlclient / log4cpp / openssl）
#   2. CMake 构建（cmake -S . -B build && cmake --build build）
#   3. 初始化运行目录（server/log、server/data）
#   4. 检查配置文件（server/conf/server.conf）
#
# 用法：./scripts/deploy.sh
# 说明：脚本约定从项目根目录执行（内部会自动 cd 到根目录）。
# ============================================================================
set -euo pipefail

# 项目根目录：本脚本位于 scripts/ 下，上级目录即项目根目录
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

echo "=== SmartHome 部署开始 ==="

# ---------------------------------------------------------------------------
# 1. 依赖检查
# ---------------------------------------------------------------------------
echo "[1/4] 检查依赖..."

check_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "  [FAIL] 缺少命令: $1"
        exit 1
    fi
    echo "  [ok]   命令 $1"
}
check_cmd cmake
check_cmd g++

# 关键头文件检查（缺失仅警告，因为路径可能因发行版而异）
check_header() {
    if [ -e "$1" ]; then
        echo "  [ok]   头文件 $1"
    else
        echo "  [WARN] 缺少头文件 $1（按实际安装路径调整）"
    fi
}
check_header /usr/include/mysql/mysql.h
check_header /usr/local/include/log4cpp/Category.hh
check_header /usr/include/openssl/evp.h

# ---------------------------------------------------------------------------
# 2. CMake 构建
# ---------------------------------------------------------------------------
echo "[2/4] CMake 构建..."
cmake -S . -B build
cmake --build build -j"$(nproc 2>/dev/null || echo 2)"
echo "  构建完成"

# ---------------------------------------------------------------------------
# 3. 初始化运行目录
# ---------------------------------------------------------------------------
echo "[3/4] 初始化运行目录..."
mkdir -p server/log server/data
echo "  server/log、server/data 就绪"

# ---------------------------------------------------------------------------
# 4. 配置检查
# ---------------------------------------------------------------------------
echo "[4/4] 检查配置..."
if [ ! -f server/conf/server.conf ]; then
    echo "  [FAIL] 缺少 server/conf/server.conf"
    exit 1
fi
echo "  server/conf/server.conf 存在"

echo "=== 部署完成 ==="
echo "启动服务:   ./scripts/server.sh start"
echo "停止服务:   ./scripts/server.sh stop"
echo "重启服务:   ./scripts/server.sh restart"
echo "查看状态:   ./scripts/server.sh status"
