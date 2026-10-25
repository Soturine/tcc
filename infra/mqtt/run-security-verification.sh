#!/usr/bin/env bash
# Verificacao NEGATIVA e positiva do perfil staging (TLS + credencial por device + ACL)
# contra um Mosquitto REAL e descartavel. Nada aqui e segredo de ambiente real:
# CA, certificados e senhas sao efemeros, gerados em diretorio temporario e apagados.
#
# Uso (da raiz do repositorio; requer docker, openssl e backend/node_modules):
#   bash infra/mqtt/run-security-verification.sh
# Saida: relatorio JSON sanitizado em backend/logs/security/ (sem senhas/chaves).
set -euo pipefail
# Git Bash (Windows) converteria "/CN=..." e "/src" em caminhos do Windows.
export MSYS_NO_PATHCONV=1

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
IMAGE="${MOSQUITTO_IMAGE:-eclipse-mosquitto:2@sha256:38c0da4f2ef84284d47b3b3eeea1cb3bdeabe81ee10caf0cd5c5ff61ee3ea408}"
NAME="tcc-mqtt-security-$$"
PORT_TLS="${SECURITY_TLS_PORT:-18883}"
PORT_PLAIN="${SECURITY_PLAIN_PORT:-18831}"
WORK="$(mktemp -d)"
if command -v cygpath >/dev/null 2>&1; then
  # Ferramentas nativas do Windows (openssl, docker, node) precisam do caminho C:/...
  WORK="$(cygpath -m "$WORK")"
fi
WORK_MOUNT="$WORK"
trap 'docker rm -f "$NAME" >/dev/null 2>&1 || true; rm -rf "$WORK"' EXIT

rand() { openssl rand -hex 16; }
export PB="$(rand)" PA="$(rand)" PBB="$(rand)" PR="$(rand)"

# CA de teste + certificado do broker para mqtt.test (SAN), e uma CA alheia.
openssl req -x509 -newkey rsa:2048 -nodes -days 2 -subj "/CN=tcc-test-ca" \
  -keyout "$WORK/ca.key" -out "$WORK/ca.crt" >/dev/null 2>&1
openssl req -x509 -newkey rsa:2048 -nodes -days 2 -subj "/CN=tcc-rogue-ca" \
  -keyout "$WORK/rogue.key" -out "$WORK/rogue-ca.crt" >/dev/null 2>&1
openssl req -newkey rsa:2048 -nodes -subj "/CN=mqtt.test" \
  -keyout "$WORK/server.key" -out "$WORK/server.csr" >/dev/null 2>&1
printf 'subjectAltName=DNS:mqtt.test\n' > "$WORK/san.ext"
openssl x509 -req -in "$WORK/server.csr" -CA "$WORK/ca.crt" -CAkey "$WORK/ca.key" \
  -CAcreateserial -days 2 -extfile "$WORK/san.ext" -out "$WORK/server.crt" >/dev/null 2>&1
cp "$ROOT/infra/mqtt/mosquitto-staging.conf" "$ROOT/infra/mqtt/mosquitto-acl.example" "$WORK/"

# Config/senhas gerados DENTRO do container (bind mount do Windows nao preserva o dono).
# A porta 1883 tambem e publicada para provar que nenhum listener plaintext responde.
docker run -d --name "$NAME" \
  -p "127.0.0.1:${PORT_TLS}:8883" -p "127.0.0.1:${PORT_PLAIN}:1883" \
  -v "$WORK_MOUNT:/src:ro" -e PB -e PA -e PBB -e PR "$IMAGE" sh -c '
    set -e
    mkdir -p /mosquitto/config/certs
    cp /src/mosquitto-staging.conf /mosquitto/config/mosquitto.conf
    cp /src/mosquitto-acl.example /mosquitto/config/acl
    cp /src/server.crt /src/server.key /mosquitto/config/certs/
    mosquitto_passwd -c -b /mosquitto/config/passwd queda-backend "$PB"
    mosquitto_passwd -b /mosquitto/config/passwd esp32_a "$PA"
    mosquitto_passwd -b /mosquitto/config/passwd esp32_b "$PBB"
    mosquitto_passwd -b /mosquitto/config/passwd esp32_revoked "$PR"
    # Dono mosquitto: o broker le chave/ACL apos baixar privilegios e o SIGHUP relê o passwd.
    # mosquitto_passwd 2.1.2 avisa que versoes futuras exigirao dono root; root:root 0600
    # impediu o broker de subir neste perfil (ver docs/quality) -> decidir no staging real.
    chown -R mosquitto:mosquitto /mosquitto/config
    chmod 0700 /mosquitto/config/passwd /mosquitto/config/acl /mosquitto/config/certs/server.key
    exec mosquitto -c /mosquitto/config/mosquitto.conf' >/dev/null

for _ in $(seq 1 30); do
  if docker logs "$NAME" 2>&1 | grep -q "running"; then break; fi
  sleep 1
done
docker exec "$NAME" mosquitto -h 2>&1 | head -1 | sed 's/^/broker: /'

export BROKER_HOST=127.0.0.1 PORT_TLS PORT_PLAIN
export CA_FILE="$WORK_MOUNT/ca.crt" ROGUE_CA_FILE="$WORK_MOUNT/rogue-ca.crt"
export MQTT_CONTAINER="$NAME"
cd "$ROOT/backend"
status=0
node ../infra/mqtt/verify-broker-security.js || status=$?

# Logs do broker sanitizados (sem senhas: o broker nao as registra) para diagnostico.
if [ "$status" -ne 0 ]; then
  docker logs "$NAME" 2>&1 | tail -40
fi
exit "$status"
