#!/bin/bash
set -e

# 无论从哪里调用，都切到工程根目录（script/ 的上一级）
cd "$(dirname "$0")/.."

if [ ! -d build ]; then
    cmake -G Ninja -B build -S .
fi

cmake --build build

./bin/lexer_test
./bin/parser_test
./bin/codegen_test
