#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

LNS_BIN="$ROOT_DIR/lns"
if [[ ! -x "$LNS_BIN" ]]; then
    echo "Erro: executavel nao encontrado em $LNS_BIN. Compile o projeto antes de rodar este script."
    exit 1
fi

MAP_NAME="empty-8-8"
MAP_FILE="$ROOT_DIR/maps-scen/empty-8-8/empty-8-8.map"
SCEN_FILE="$ROOT_DIR/maps-scen/empty-8-8/empty-8-8.map-scen-random/scen-random/empty-8-8-random-1.scen"
OUTPUT_FILE="$ROOT_DIR/results/test_neighbor_size.csv"

DESTROY_STRATEGY="Adaptive"
AGENTS=16
TIME_LIMIT=60
NEIGHBOR_SIZES=(2 8)

mkdir -p "$(dirname "$OUTPUT_FILE")"
rm -f "$OUTPUT_FILE"

echo "Iniciando teste pequeno variando apenas --neighborSize"
echo "Mapa: $MAP_NAME"
echo "Agentes: $AGENTS"
echo "Destroy strategy: $DESTROY_STRATEGY"
echo "Tempo limite: $TIME_LIMIT"
echo "Saida: $OUTPUT_FILE"
echo

for NEIGHBOR_SIZE in "${NEIGHBOR_SIZES[@]}"; do
    "$LNS_BIN" \
        -m "$MAP_FILE" \
        -a "$SCEN_FILE" \
        -o "$OUTPUT_FILE" \
        -k "$AGENTS" \
        -t "$TIME_LIMIT" \
        --destoryStrategy "$DESTROY_STRATEGY" \
        --neighborSize "$NEIGHBOR_SIZE"

    echo "Concluido: map=$MAP_NAME agents=$AGENTS destroyStrategy=$DESTROY_STRATEGY neighborSize=$NEIGHBOR_SIZE timeLimit=$TIME_LIMIT output=$OUTPUT_FILE"
    echo "=========================================================================="
done

echo
echo "Teste finalizado. Resultados em: $OUTPUT_FILE"