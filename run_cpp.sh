#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="/home/alida/.local/lib/python3.12/site-packages/onnxruntime/capi:/home/alida/.local/lib/python3.12/site-packages/nvidia/cudnn/lib:/home/alida/.local/lib/python3.12/site-packages/nvidia/cu13/lib:$LD_LIBRARY_PATH"

# Tự động biên dịch nếu file binary chưa tồn tại
if [ ! -f "$DIR/bin/matcha_tts_cli" ]; then
    echo "⚙️  Chưa tìm thấy binary CLI, đang tiến hành biên dịch C++..."
    make -C "$DIR/cpp" ../bin/matcha_tts_cli
fi

mkdir -p "$DIR/outputs"

exec "$DIR/bin/matcha_tts_cli" "$@"
