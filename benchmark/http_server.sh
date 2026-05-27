#!/bin/bash
# HTTP Server 压测脚本
# 自动对比单进程 vs 多进程，可选 OPcache/JIT
#
# 依赖: wrk (brew install wrk)
# 用法: ./benchmark/http_server.sh [选项]
#   -w <workers>   Worker 进程数（默认: CPU 核心数）
#   -c <conns>     并发连接数（默认: 200）
#   -d <duration>  持续时间（默认: 10s）
#   -j             启用 JIT
#   --no-opcache   禁用 OPcache

__DIR__=$(cd "$(dirname "$0")" || exit 1; pwd); [ -z "${__DIR__}" ] && exit 1
PROJECT_ROOT=$(cd "${__DIR__}/.." || exit 1; pwd)

# 默认参数
WORKERS=$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 8)
CONNS=200
DURATION=10s
HOST=127.0.0.1
PORT=9764
ENABLE_JIT=0
ENABLE_OPCACHE=1

# 解析参数
while [[ $# -gt 0 ]]; do
    case $1 in
        -w) WORKERS="$2"; shift 2 ;;
        -c) CONNS="$2"; shift 2 ;;
        -d) DURATION="$2"; shift 2 ;;
        -j) ENABLE_JIT=1; shift ;;
        --no-opcache) ENABLE_OPCACHE=0; shift ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

# 检测 wrk
if ! command -v wrk &>/dev/null; then
    echo "Error: wrk not found. Install with: brew install wrk"
    exit 1
fi

# 构建 PHP 参数
PHP_OPTS=(-d extension=swow)

# 尝试加载 OPcache
if [[ $ENABLE_OPCACHE -eq 1 ]]; then
    OPCACHE_SO=$(php -r 'echo PHP_EXTENSION_DIR;' 2>/dev/null)/opcache.so
    if [[ -f "$OPCACHE_SO" ]]; then
        PHP_OPTS+=(-d "zend_extension=${OPCACHE_SO}" -d opcache.enable=1 -d opcache.enable_cli=1)
        if [[ $ENABLE_JIT -eq 1 ]]; then
            PHP_OPTS+=(-d opcache.jit=tracing -d opcache.jit_buffer_size=64M)
        else
            PHP_OPTS+=(-d opcache.jit=off)
        fi
    else
        ENABLE_OPCACHE=0
    fi
fi

# 压测函数
run_bench() {
    local label=$1
    local server_script=$2
    local wrk_threads=$3

    echo "────────────────────────────────────────"
    echo "  ${label}"
    echo "────────────────────────────────────────"

    php "${PHP_OPTS[@]}" "${server_script}" &>/dev/null &
    local pid=$!
    disown "$pid"
    sleep 2

    if ! kill -0 "$pid" 2>/dev/null; then
        echo "  [FAIL] Server failed to start"
        return 1
    fi

    wrk -t"${wrk_threads}" -c"${CONNS}" -d"${DURATION}" "http://${HOST}:${PORT}/" 2>&1 | \
        grep -E "Requests/sec|Latency|Socket errors"

    kill "$pid" 2>/dev/null
    # 多进程模式需要杀子进程
    pkill -P "$pid" 2>/dev/null
    wait "$pid" 2>/dev/null
    sleep 1
    echo ""
}

# 生成临时服务器脚本
SERVER_SINGLE=$(mktemp /tmp/swow_bench_single.XXXXXX.php)
SERVER_MULTI=$(mktemp /tmp/swow_bench_multi.XXXXXX.php)

cat > "${SERVER_SINGLE}" << PHP
<?php
require '${PROJECT_ROOT}/vendor/autoload.php';

use Swow\Psr7\Server\EventDriver;
use Swow\Psr7\Server\Server;
use Swow\Psr7\Message\ServerRequestPlusInterface;
use Swow\Psr7\Server\ServerConnection;

\$host = getenv('SERVER_HOST') ?: '127.0.0.1';
\$port = (int) (getenv('SERVER_PORT') ?: 9764);

(new EventDriver(new Server()))
    ->withRequestHandler(static function (ServerConnection \$c, ServerRequestPlusInterface \$r): string {
        return 'OK';
    })
    ->startOn(\$host, \$port);
PHP

cat > "${SERVER_MULTI}" << PHP
<?php
require '${PROJECT_ROOT}/vendor/autoload.php';

use Swow\Psr7\Server\EventDriver;
use Swow\Psr7\Server\Server;
use Swow\Psr7\Message\ServerRequestPlusInterface;
use Swow\Psr7\Server\ServerConnection;

\$host = getenv('SERVER_HOST') ?: '127.0.0.1';
\$port = (int) (getenv('SERVER_PORT') ?: 9764);
\$workers = (int) (getenv('SERVER_WORKERS') ?: ${WORKERS});

(new EventDriver(new Server()))
    ->withWorkerCount(\$workers)
    ->withRequestHandler(static function (ServerConnection \$c, ServerRequestPlusInterface \$r): string {
        return 'OK';
    })
    ->startOn(\$host, \$port);
PHP

trap 'rm -f "${SERVER_SINGLE}" "${SERVER_MULTI}"' EXIT

# 输出测试环境信息
echo ""
echo "╔══════════════════════════════════════════╗"
echo "║     Swow HTTP Server Benchmark          ║"
echo "╚══════════════════════════════════════════╝"
echo ""
echo "  PHP:       $(php -v 2>&1 | head -1)"
echo "  Swow:      $(php "${PHP_OPTS[@]}" -r 'echo phpversion("swow");' 2>/dev/null)"
echo "  OPcache:   $([ $ENABLE_OPCACHE -eq 1 ] && echo 'ON' || echo 'OFF')"
echo "  JIT:       $([ $ENABLE_JIT -eq 1 ] && echo 'ON (tracing)' || echo 'OFF')"
echo "  Workers:   ${WORKERS}"
echo "  wrk:       -c${CONNS} -d${DURATION}"
echo "  Host:      ${HOST}:${PORT}"
echo ""

export SERVER_HOST=$HOST SERVER_PORT=$PORT SERVER_WORKERS=$WORKERS

# wrk 线程数：单进程用 1 线程减少客户端占用，多进程用 2 线程
run_bench "Single Process" "${SERVER_SINGLE}" 1
run_bench "Multi Process (${WORKERS} workers)" "${SERVER_MULTI}" 2

echo "Done."
