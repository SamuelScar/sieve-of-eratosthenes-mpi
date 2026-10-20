# Crivo de Eratóstenes com MPI

Implementações sequencial e paralela do Crivo de Eratóstenes em C. A versão
paralela utiliza Open MPI no modelo mestre-escravo e divide o intervalo
`[2, N]` em blocos contínuos.

## Execução rápida

### Linux com Open MPI

No Ubuntu, instale as dependências:

```bash
sudo apt update
sudo apt install -y gcc make libopenmpi-dev openmpi-bin
```

Compile as duas versões:

```bash
make
```

Execute a versão sequencial:

```bash
make run-sequential N=100
```

Execute a versão paralela com quatro processos:

```bash
make run NP=4 N=100
```

`N` é o limite até o qual os números primos serão procurados. `NP` é a
quantidade total de processos MPI: um mestre e `NP - 1` escravos.

### Docker

Como alternativa, o projeto possui uma imagem com todas as dependências:

```bash
docker compose build
docker compose run --rm dev make
```

Execute as versões pelos mesmos alvos do `Makefile`:

```bash
docker compose run --rm dev make run-sequential N=100
docker compose run --rm dev make run NP=4 N=100
```

## Funcionamento

A versão sequencial cria um único crivo, marca os números compostos e conta os
primos encontrados.

Na versão paralela, o processo mestre:

- calcula os primos-base até `sqrt(N)`;
- divide o intervalo entre os escravos;
- distribui os dados e recebe os resultados;
- reúne os primos na ordem correta.

Cada escravo processa somente o bloco recebido. Ao final, o programa mostra a
quantidade de primos e o tempo de execução.

## Arquivos gerados

Cada execução acrescenta seus resultados aos arquivos:

- `resultados/sequencial/execucoes.csv`;
- `resultados/paralelo/execucoes.csv`.

A versão paralela também grava a lista completa em
`resultados/paralelo/primos.txt`, com um primo por linha. Os arquivos são
gravados depois da medição do tempo.

Os executáveis são criados em `bin/`:

- `bin/sequencial`;
- `bin/paralelo`.

## Série de experimentos

O script abaixo compila os programas e executa a série completa usada no
relatório:

```bash
bash scripts/executar_experimentos.sh
```

A configuração padrão utiliza 70 limites, seis quantidades de processos e
cinco repetições, totalizando 2450 execuções. Para uma verificação menor, use:

```bash
LIMITES="1000 10000" PROCESSOS="2 4" REPETICOES=1 \
  bash scripts/executar_experimentos.sh
```

Durante a série, a gravação de `primos.txt` fica desabilitada para evitar
arquivos muito grandes. O cálculo e a comunicação dos primos continuam sendo
realizados. Para manter o arquivo, defina `SALVAR_PRIMOS=1`.

Ao final, o script gera `resultados/resumo_execucao.md` com informações do
ambiente, tempos médios, speedup e eficiência.

## GitHub Actions

O workflow **Experimentos MPI** pode ser iniciado manualmente na aba
**Actions**. Antes da execução, é possível informar os limites, as quantidades
de processos e o número de repetições.

O resumo é exibido na própria execução e os CSVs ficam disponíveis como
artefato por 30 dias.

## Relatório

O relatório técnico está em `docs/relatorio.tex`. A imagem utilizada por ele
está em `docs/imagens/`.

## Limpeza

Remova os executáveis e o arquivo de perfil, caso exista:

```bash
make clean
```
