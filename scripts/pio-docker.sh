#!/usr/bin/env bash
# Reproduz localmente, em Linux/Docker, os passos de firmware e pesquisa do CI.
# Util no Windows sem gcc/g++ nativo. Copia o repositorio para dentro do
# container (somente leitura na origem) para nao misturar artefatos .pio.
#
# Uso: scripts/pio-docker.sh [native|esp32|research|all]
set -euo pipefail

target="${1:-all}"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if command -v cygpath >/dev/null 2>&1; then
  mount_source="$(cygpath -m "${repo_root}")"
else
  mount_source="${repo_root}"
fi

docker run --rm \
  -v "${mount_source}:/src:ro" \
  -v tcc-pio-cache:/root/.platformio \
  -v tcc-pip-cache:/root/.cache/pip \
  -e TARGET="${target}" \
  python:3.12 bash -c '
    set -euo pipefail
    mkdir /work
    (cd /src && tar --exclude=.pio --exclude=node_modules --exclude=.git --exclude=build -cf - .) | tar -xf - -C /work
    cd /work
    if [ "$TARGET" = native ] || [ "$TARGET" = esp32 ] || [ "$TARGET" = all ]; then
      pip install -q platformio==6.1.19
    fi
    if [ "$TARGET" = native ] || [ "$TARGET" = all ]; then
      platformio test -e native
    fi
    if [ "$TARGET" = esp32 ] || [ "$TARGET" = all ]; then
      platformio run -e esp32dev
    fi
    if [ "$TARGET" = research ] || [ "$TARGET" = all ]; then
      bash tools/research/build_replay.sh
      (cd tools/research && FALLBENCH_REQUIRE_REPLAY=1 python -m unittest discover -s tests -v)
    fi
  '
