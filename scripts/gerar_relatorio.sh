#!/usr/bin/env bash

set -euo pipefail

diretorio_resultados="resultados"
arquivo_sequencial="$diretorio_resultados/sequencial/execucoes.csv"
arquivo_paralelo="$diretorio_resultados/paralelo/execucoes.csv"
arquivo_inicio="$diretorio_resultados/inicio_execucao.txt"
arquivo_relatorio="$diretorio_resultados/resumo_execucao.md"

mkdir -p "$diretorio_resultados"

inicio_iso="indisponivel"
inicio_epoch=""

if [[ -f "$arquivo_inicio" ]]; then
    inicio_iso="$(sed -n '1p' "$arquivo_inicio")"
    inicio_epoch="$(sed -n '2p' "$arquivo_inicio")"
fi

fim_iso="$(date -u '+%Y-%m-%dT%H:%M:%SZ')"
fim_epoch="$(date +%s)"
duracao="indisponivel"

if [[ "$inicio_epoch" =~ ^[0-9]+$ ]]; then
    duracao="$((fim_epoch - inicio_epoch)) segundos"
fi

modelo_cpu="$(awk -F: '/model name/ { gsub(/^[[:space:]]+/, "", $2); print $2; exit }' /proc/cpuinfo)"
cpus_logicas="$(nproc)"
nucleos_fisicos="$(
    lscpu -p=CORE,SOCKET 2>/dev/null \
        | awk -F, '!/^#/ { chave=$1 "," $2; if (!vistos[chave]++) quantidade++ } END { print quantidade + 0 }'
)"
memoria_total="$(
    awk '/MemTotal/ { printf "%.2f GiB", $2 / 1048576 }' /proc/meminfo
)"
sistema_operacional="$(. /etc/os-release && printf '%s' "$PRETTY_NAME")"
versao_gcc="$(gcc --version 2>/dev/null | sed -n '1p' || true)"
versao_mpi="$(mpirun --version 2>/dev/null | sed -n '1p' || true)"
versao_gcc="${versao_gcc:-indisponivel}"
versao_mpi="${versao_mpi:-indisponivel}"
status_execucao="${STATUS_EXECUCAO:-concluida}"
execucoes_sequenciais=0
execucoes_paralelas=0

if [[ -f "$arquivo_sequencial" ]]; then
    execucoes_sequenciais="$(awk 'END { print (NR > 0 ? NR - 1 : 0) }' "$arquivo_sequencial")"
fi

if [[ -f "$arquivo_paralelo" ]]; then
    execucoes_paralelas="$(awk 'END { print (NR > 0 ? NR - 1 : 0) }' "$arquivo_paralelo")"
fi

{
    printf '# Resumo da execução dos experimentos\n\n'
    printf -- '- Status: `%s`\n' "$status_execucao"
    printf -- '- Início: `%s`\n' "$inicio_iso"
    printf -- '- Término: `%s`\n' "$fim_iso"
    printf -- '- Duração total: `%s`\n\n' "$duracao"
    printf '| Tipo | Execuções registradas |\n'
    printf '|---|---:|\n'
    printf '| Sequencial | %s |\n' "$execucoes_sequenciais"
    printf '| Paralelo | %s |\n\n' "$execucoes_paralelas"

    printf '## Ambiente de execução\n\n'
    printf '| Propriedade | Valor |\n'
    printf '|---|---|\n'
    printf '| Sistema operacional | %s |\n' "$sistema_operacional"
    printf '| Kernel | %s |\n' "$(uname -sr)"
    printf '| Arquitetura | %s |\n' "$(uname -m)"
    printf '| Processador | %s |\n' "$modelo_cpu"
    printf '| Núcleos físicos | %s |\n' "$nucleos_fisicos"
    printf '| Processadores lógicos | %s |\n' "$cpus_logicas"
    printf '| Memória total | %s |\n' "$memoria_total"
    printf '| Compilador | %s |\n' "$versao_gcc"
    printf '| MPI | %s |\n' "$versao_mpi"

    if [[ "${GITHUB_ACTIONS:-false}" == "true" ]]; then
        printf '| Repositório | %s |\n' "${GITHUB_REPOSITORY:-indisponivel}"
        printf '| Commit | `%s` |\n' "${GITHUB_SHA:-indisponivel}"
        printf '| Execução do Actions | %s, tentativa %s |\n' \
            "${GITHUB_RUN_ID:-indisponivel}" \
            "${GITHUB_RUN_ATTEMPT:-indisponivel}"
        printf '| Runner | %s |\n' "${RUNNER_NAME:-indisponivel}"
    fi

    printf '\n## Resultados sequenciais\n\n'

    if [[ -s "$arquivo_sequencial" ]]; then
        printf '| N | Rodadas | Média (s) | Mínimo (s) | Máximo (s) |\n'
        printf '|---:|---:|---:|---:|---:|\n'
        awk -F, '
            FNR > 1 {
                limite = $1
                quantidade[limite]++
                soma[limite] += $4

                if (!(limite in minimo) || $4 < minimo[limite])
                    minimo[limite] = $4

                if (!(limite in maximo) || $4 > maximo[limite])
                    maximo[limite] = $4
            }

            END {
                for (limite in quantidade) {
                    media = soma[limite] / quantidade[limite]
                    printf "%d,%d,%.9f,%.9f,%.9f\n",
                        limite,
                        quantidade[limite],
                        media,
                        minimo[limite],
                        maximo[limite]
                }
            }
        ' "$arquivo_sequencial" \
            | sort -t, -k1,1n \
            | awk -F, '{
                printf "| %d | %d | %.9f | %.9f | %.9f |\n",
                    $1, $2, $3, $4, $5
            }'
    else
        printf 'Nenhum resultado sequencial foi encontrado.\n'
    fi

    printf '\n## Resultados paralelos\n\n'

    if [[ -s "$arquivo_sequencial" && -s "$arquivo_paralelo" ]]; then
        printf '| N | Processos | Rodadas | Média (s) | Speedup | Eficiência |\n'
        printf '|---:|---:|---:|---:|---:|---:|\n'
        awk -F, '
            FNR == 1 { next }

            FILENAME == ARGV[1] {
                soma_sequencial[$1] += $4
                quantidade_sequencial[$1]++
                next
            }

            {
                chave = $1 SUBSEP $2
                quantidade[chave]++
                soma[chave] += $4
            }

            END {
                for (chave in quantidade) {
                    split(chave, campos, SUBSEP)
                    limite = campos[1]
                    processos = campos[2]

                    if (quantidade_sequencial[limite] == 0)
                        continue

                    media = soma[chave] / quantidade[chave]
                    media_sequencial = soma_sequencial[limite] / quantidade_sequencial[limite]
                    speedup = media_sequencial / media
                    eficiencia = speedup / processos

                    printf "%d,%d,%d,%.9f,%.6f,%.6f\n",
                        limite,
                        processos,
                        quantidade[chave],
                        media,
                        speedup,
                        eficiencia
                }
            }
        ' "$arquivo_sequencial" "$arquivo_paralelo" \
            | sort -t, -k1,1n -k2,2n \
            | awk -F, '{
                printf "| %d | %d | %d | %.9f | %.6f | %.2f%% |\n",
                    $1, $2, $3, $4, $5, $6 * 100
            }'
    else
        printf 'Não há resultados suficientes para calcular speedup e eficiência.\n'
    fi

    printf '\n## Observações\n\n'
    printf -- '- Os tempos dos CSVs não incluem a gravação dos arquivos de saída.\n'
    printf -- '- Configurações acima de %s processos utilizam oversubscription neste ambiente.\n' \
        "$cpus_logicas"
    printf -- '- A série de experimentos não gera `primos.txt` por padrão devido ao tamanho potencial.\n'
    printf -- '- As execuções individuais continuam gerando a lista completa de primos.\n'
} > "$arquivo_relatorio"

printf 'Relatorio gerado em %s\n' "$arquivo_relatorio"
