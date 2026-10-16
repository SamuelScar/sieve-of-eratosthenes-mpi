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
`resultados/paralelo/primos.txt`, com um número por linha.

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

## Limpeza

Remova binários e dados gerados pelo profiling:

```bash
docker compose run --rm dev make clean
```
