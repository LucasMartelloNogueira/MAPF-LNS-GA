#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

LNS_BIN="$ROOT_DIR/lns"
if [[ ! -x "$LNS_BIN" ]]; then
    echo "Erro: executavel nao encontrado em $LNS_BIN. Compile o projeto antes de rodar este script."
    exit 1
fi

OUTPUT_FILE="$ROOT_DIR/results/all_simulations.csv"
LOG_FILE="$ROOT_DIR/results/all_simulations.log"

DESTROY_STRATEGIES=(Adaptive GeneticAlgo)
NEIGHBOR_SIZES=(2 8)
TIME_LIMITS=(60 120)

GA_POP_SIZE=8
GA_GENERATIONS=3
GA_MUTATION_RATE=0.20

MAP_NAMES=(
    empty-8-8
    empty-32-32
    random-32-32-20
    warehouse-10-20-10-2-1
    ost003d
    den520d
)

declare -A MAP_FILES=(
    [empty-8-8]="$ROOT_DIR/maps-scen/empty-8-8/empty-8-8.map"
    [empty-32-32]="$ROOT_DIR/maps-scen/empty-32-32/empty-32-32.map"
    [random-32-32-20]="$ROOT_DIR/maps-scen/random-32-32-20 /random-32-32-20.map"
    [warehouse-10-20-10-2-1]="$ROOT_DIR/maps-scen/warehouse-10-20-10-2-1/warehouse-10-20-10-2-1.map"
    [ost003d]="$ROOT_DIR/maps-scen/ost003d/ost003d.map"
    [den520d]="$ROOT_DIR/maps-scen/den520d/den520d.map"
)

declare -A SCEN_FILES=(
    [empty-8-8]="$ROOT_DIR/maps-scen/empty-8-8/empty-8-8.map-scen-random/scen-random/empty-8-8-random-1.scen"
    [empty-32-32]="$ROOT_DIR/maps-scen/empty-32-32/empty-32-32.map-scen-random/scen-random/empty-32-32-random-1.scen"
    [random-32-32-20]="$ROOT_DIR/maps-scen/random-32-32-20 /random-32-32-20.map-scen-random/scen-random/random-32-32-20-random-1.scen"
    [warehouse-10-20-10-2-1]="$ROOT_DIR/maps-scen/warehouse-10-20-10-2-1/warehouse-10-20-10-2-1.map-scen-random/scen-random/warehouse-10-20-10-2-1-random-1.scen"
    [ost003d]="$ROOT_DIR/maps-scen/ost003d/ost003d.map-scen-random/scen-random/ost003d-random-1.scen"
    [den520d]="$ROOT_DIR/maps-scen/den520d/den520d.map-scen-random/scen-random/den520d-random-1.scen"
)

declare -A AGENT_COUNTS=(
    [empty-8-8]="16 32 48"
    [empty-32-32]="300 400 500"
    [random-32-32-20]="50 150 150"
    [warehouse-10-20-10-2-1]="150 250 350"
    [ost003d]="100 300 500"
    [den520d]="500 700 900"
)

mkdir -p "$(dirname "$OUTPUT_FILE")"
rm -f "$OUTPUT_FILE" "$LOG_FILE"

TOTAL_EXPERIMENTS=$(( ${#MAP_NAMES[@]} * ${#DESTROY_STRATEGIES[@]} * 3 * ${#NEIGHBOR_SIZES[@]} * ${#TIME_LIMITS[@]} ))
EXPERIMENT=0

echo "Iniciando simulacoes completas" | tee -a "$LOG_FILE"
echo "Total de experimentos: $TOTAL_EXPERIMENTS" | tee -a "$LOG_FILE"
echo "Saida CSV: $OUTPUT_FILE" | tee -a "$LOG_FILE"
echo "Log: $LOG_FILE" | tee -a "$LOG_FILE"
echo | tee -a "$LOG_FILE"

for MAP_NAME in "${MAP_NAMES[@]}"; do
    MAP_FILE="${MAP_FILES[$MAP_NAME]}"
    SCEN_FILE="${SCEN_FILES[$MAP_NAME]}"

    if [[ ! -f "$MAP_FILE" ]]; then
        echo "Erro: mapa nao encontrado: $MAP_FILE" | tee -a "$LOG_FILE"
        exit 1
    fi
    if [[ ! -f "$SCEN_FILE" ]]; then
        echo "Erro: cenario nao encontrado: $SCEN_FILE" | tee -a "$LOG_FILE"
        exit 1
    fi

    read -r -a MAP_AGENT_COUNTS <<< "${AGENT_COUNTS[$MAP_NAME]}"
    for AGENTS in "${MAP_AGENT_COUNTS[@]}"; do
        for DESTROY_STRATEGY in "${DESTROY_STRATEGIES[@]}"; do
            for NEIGHBOR_SIZE in "${NEIGHBOR_SIZES[@]}"; do
                for TIME_LIMIT in "${TIME_LIMITS[@]}"; do
                    EXPERIMENT=$((EXPERIMENT + 1))
                    echo "[$EXPERIMENT/$TOTAL_EXPERIMENTS] Iniciando: map=$MAP_NAME agents=$AGENTS destroyStrategy=$DESTROY_STRATEGY neighborSize=$NEIGHBOR_SIZE timeLimit=$TIME_LIMIT" | tee -a "$LOG_FILE"

                    "$LNS_BIN" \
                        -m "$MAP_FILE" \
                        -a "$SCEN_FILE" \
                        -o "$OUTPUT_FILE" \
                        -k "$AGENTS" \
                        -t "$TIME_LIMIT" \
                        --destoryStrategy "$DESTROY_STRATEGY" \
                        --neighborSize "$NEIGHBOR_SIZE" \
                        --gaPopSize "$GA_POP_SIZE" \
                        --gaGenerations "$GA_GENERATIONS" \
                        --gaMutationRate "$GA_MUTATION_RATE"

                    echo "[$EXPERIMENT/$TOTAL_EXPERIMENTS] Concluido: map=$MAP_NAME agents=$AGENTS destroyStrategy=$DESTROY_STRATEGY neighborSize=$NEIGHBOR_SIZE timeLimit=$TIME_LIMIT output=$OUTPUT_FILE" | tee -a "$LOG_FILE"
                    echo "==========================================================================" | tee -a "$LOG_FILE"
                done
            done
        done
    done
done

echo | tee -a "$LOG_FILE"
echo "Simulacoes finalizadas. Resultados em: $OUTPUT_FILE" | tee -a "$LOG_FILE"