# crivo-eratostenes-mpi

Implementações sequencial e paralela do Crivo de Eratóstenes em C. A versão
paralela utiliza OpenMPI e o modelo mestre-escravo para dividir o intervalo em
blocos contínuos.

## Requisitos

- Docker
- Docker Compose

## Preparação do ambiente

Construa a imagem de desenvolvimento:

```bash
docker compose build
```

Os arquivos do repositório são montados em `/workspace` no container. Assim,
alterações feitas localmente em `src/` ficam disponíveis imediatamente dentro
do ambiente Docker.

Para abrir um terminal no container:

```bash
docker compose run --rm dev
```

## Compilação

Compile as versões sequencial e paralela:

```bash
docker compose run --rm dev make
```

Os executáveis são gerados na pasta `bin/`.

Execute a versão sequencial:

```bash
docker compose run --rm dev make run-sequential N=100
```

Execute a versão paralela com quatro processos, sendo um mestre e três
escravos:

```bash
docker compose run --rm dev make run NP=4 N=100
```

A versão paralela grava os números primos encontrados no arquivo
`primos.txt`, com um número por linha, conforme especificado no enunciado.

## Resultados

Cada execução acrescenta uma linha ao arquivo correspondente:

- `resultados/sequencial/execucoes.csv` para a versão sequencial;
- `resultados/paralelo/execucoes.csv` para a versão paralela.

Os dois arquivos utilizam as mesmas colunas:

- limite;
- quantidade de processos;
- quantidade de primos;
- tempo em segundos.

A gravação dos arquivos acontece depois da medição e, portanto, não interfere
no tempo registrado.

## Experimentos

O script `scripts/executar_experimentos.sh` compila os programas e executa uma
série de experimentos. Por padrão, são utilizadas cinco repetições para cada
combinação de 70 limites entre `1000` e `1000000000` com as seguintes
quantidades de processos MPI:

```text
2, 3, 4, 6, 8 e 12 processos
```

O conjunto padrão totaliza 2450 execuções. O script detecta a quantidade de
processadores lógicos disponíveis e avisa quando uma configuração utiliza
oversubscription, mas mantém a execução para permitir a análise desse
comportamento.

Execute a série padrão dentro do container:

```bash
docker compose run --rm dev bash scripts/executar_experimentos.sh
```

Os valores podem ser personalizados por variáveis de ambiente:

```bash
docker compose run --rm \
  -e LIMITES="1000000 10000000" \
  -e PROCESSOS="2 4" \
  -e REPETICOES=3 \
  dev bash scripts/executar_experimentos.sh
```

O script acrescenta novas linhas aos CSVs existentes e não remove resultados
de execuções anteriores. Ao final, também gera
`resultados/resumo_execucao.md`, contendo o hardware utilizado, a duração total,
as médias, os speedups e as eficiências calculadas.

Para evitar a gravação repetida de arquivos com milhões de linhas, o script não
gera `primos.txt` por padrão. Esse comportamento afeta somente a saída em disco,
pois os primos continuam sendo calculados e enviados ao mestre. Para manter a
lista durante os experimentos no Docker, acrescente `-e SALVAR_PRIMOS=1` ao
comando. As execuções individuais com `make run` continuam gerando o arquivo
normalmente.

## GitHub Actions

O workflow `Experimentos MPI` pode ser iniciado manualmente na aba
`Actions`. Os limites, as quantidades de processos e o número de repetições
podem ser alterados antes da execução.

Ao terminar, o workflow:

- apresenta o relatório na página de resumo da execução;
- disponibiliza os CSVs e o relatório como um artefato por 30 dias;
- registra processador, núcleos, memória, sistema operacional, compilador e
  versão do MPI utilizados pelo runner.

O arquivo `primos.txt` não é gerado na série padrão nem enviado como artefato,
pois pode ocupar centenas de megabytes nos maiores limites. Os runners
hospedados podem variar entre execuções, portanto comparações diretas devem
considerar o hardware registrado em cada relatório.

## Limpeza

Remova binários e dados gerados pelo profiling:

```bash
docker compose run --rm dev make clean
```
