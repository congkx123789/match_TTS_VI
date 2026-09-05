#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Sử dụng trực tiếp thư viện C++ native lib/ và các đường dẫn hệ thống
export LD_LIBRARY_PATH="$DIR/lib:$DIR/cpp/lib:$LD_LIBRARY_PATH"

# Tự động biên dịch nếu file binary chưa tồn tại
if [ ! -f "$DIR/bin/matcha_tts_cli" ]; then
    echo "⚙️  Đang biên dịch C++ Native..."
    make -C "$DIR/cpp" ORT_LIB_DIR="$DIR/lib"
fi

mkdir -p "$DIR/outputs"

# Chạy trực tiếp C++ Native CLI với INT8 tối ưu
exec "$DIR/bin/matcha_tts_cli" --model-type int8 "$@"
