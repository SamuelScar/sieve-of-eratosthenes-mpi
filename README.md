# crivo-eratostenes-mpi

Ambiente de desenvolvimento em C com OpenMPI para a implementação paralela do
Crivo de Eratóstenes usando o modelo mestre-escravo.

Nesta etapa, o projeto contém a implementação sequencial do Crivo de
Eratóstenes. A versão paralela com MPI ainda será desenvolvida.

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

Compile a versão sequencial:

```bash
docker compose run --rm dev make
```

Os executáveis são gerados na pasta `bin/`.

## Limpeza

Remova binários e dados gerados pelo profiling:

```bash
docker compose run --rm dev make clean
```
