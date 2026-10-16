#!/usr/bin/env bash

set -euo pipefail

if [[ -n "${LIMITES:-}" ]]; then
    read -r -a limites <<< "$LIMITES"
else
    limites=(
        1000 2000 3000 4000 5000 7500 10000 15000 20000 30000
        40000 50000 75000 100000 150000 200000 300000 400000 500000
        750000 1000000 1250000 1500000 1750000 2000000 2500000
        3000000 3500000 4000000 5000000 6000000 7000000 8000000
        9000000 10000000 12000000 14000000 16000000 18000000
        20000000 25000000 30000000 35000000 40000000 45000000
        50000000 60000000 70000000 80000000 100000000
        120000000 140000000 160000000 180000000 200000000
        250000000 300000000 350000000 400000000 450000000
        500000000 550000000 600000000 650000000 700000000
        750000000 800000000 850000000 900000000 1000000000
    )
fi

read -r -a quantidades_processos <<< "${PROCESSOS:-2 3 4 6 8 12}"
repeticoes="${REPETICOES:-5}"
salvar_primos="${SALVAR_PRIMOS:-0}"

if [[ ! "$repeticoes" =~ ^[1-9][0-9]*$ ]]; then
    echo "REPETICOES deve ser um inteiro maior que zero." >&2
    exit 1
fi

if [[ ! "$salvar_primos" =~ ^[01]$ ]]; then
    echo "SALVAR_PRIMOS deve ser 0 ou 1." >&2
    exit 1
fi

processadores_disponiveis="$(nproc)"
total_execucoes=$((
    ${#limites[@]} * (1 + ${#quantidades_processos[@]}) * repeticoes
))

mkdir -p resultados
printf '%s\n%s\n' \
    "$(date -u '+%Y-%m-%dT%H:%M:%SZ')" \
    "$(date +%s)" \
    > resultados/inicio_execucao.txt

printf 'Processadores logicos disponiveis: %d\n' "$processadores_disponiveis"
printf 'Total de execucoes planejadas: %d\n' "$total_execucoes"

if [[ "$salvar_primos" == "0" ]]; then
    printf 'A lista primos.txt nao sera gerada durante os experimentos.\n'

    if [[ -f resultados/paralelo/primos.txt ]]; then
        printf 'Aviso: o primos.txt existente nao sera atualizado.\n'
    fi
fi

for processos in "${quantidades_processos[@]}"; do
    if ((processos > processadores_disponiveis)); then
        printf 'Aviso: %d processos usarao oversubscription.\n' "$processos"
    fi
done

make

for limite in "${limites[@]}"; do
    for ((rodada = 1; rodada <= repeticoes; rodada++)); do
        printf 'Sequencial | N=%s | rodada=%d/%d\n' \
            "$limite" "$rodada" "$repeticoes"
        bin/sequencial "$limite"
    done

    for processos in "${quantidades_processos[@]}"; do
        for ((rodada = 1; rodada <= repeticoes; rodada++)); do
            printf 'Paralelo | N=%s | processos=%s | rodada=%d/%d\n' \
                "$limite" "$processos" "$rodada" "$repeticoes"
            mpirun \
                --allow-run-as-root \
                --oversubscribe \
                -x SALVAR_PRIMOS="$salvar_primos" \
                -np "$processos" \
                bin/paralelo "$limite"
        done
    done
done

printf '%s\n' \
    'Experimentos finalizados.' \
    'Resultados sequenciais: resultados/sequencial/execucoes.csv' \
    'Resultados paralelos: resultados/paralelo/execucoes.csv'

STATUS_EXECUCAO=sucesso bash scripts/gerar_relatorio.sh
