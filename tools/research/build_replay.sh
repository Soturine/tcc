#!/usr/bin/env bash
# Compila o replay offline com as MESMAS fontes do detector do firmware.
# Requer g++ com C++17 (Linux/CI). No Windows sem toolchain, use:
#   scripts/pio-docker.sh research
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
out_dir="${repo_root}/build/research"
mkdir -p "${out_dir}"

"${CXX:-g++}" -std=gnu++17 -O2 -Wall -Wextra -Werror \
  -I "${repo_root}/include" \
  -I "${repo_root}/test/native_stubs" \
  "${repo_root}/src/angle_math.cpp" \
  "${repo_root}/src/fall_detector.cpp" \
  "${repo_root}/src/sensor_signal.cpp" \
  "${repo_root}/tools/research/replay/fall_replay.cpp" \
  -o "${out_dir}/fall_replay"

echo "${out_dir}/fall_replay"
