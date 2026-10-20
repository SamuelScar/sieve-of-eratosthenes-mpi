#include <errno.h>
#include <limits.h>
#include <math.h>
#include <mpi.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#define RANK_MESTRE 0
#define DIRETORIO_RESULTADOS "resultados"
#define DIRETORIO_PARALELO DIRETORIO_RESULTADOS "/paralelo"
#define ARQUIVO_RESULTADOS DIRETORIO_PARALELO "/execucoes.csv"
#define ARQUIVO_PRIMOS DIRETORIO_PARALELO "/primos.txt"

enum TagMensagem {
    TAG_INTERVALO,
    TAG_QUANTIDADE_PRIMOS,
    TAG_LISTA_PRIMOS
};

/**
 * Converte o argumento recebido em um limite valido para o crivo.
 *
 * @return O limite convertido ou -1 quando o valor for invalido.
 */
static int ler_limite(const char *valor)
{
    char *fim_conversao;
    long limite = strtol(valor, &fim_conversao, 10);

    const bool limite_invalido =
        *valor == '\0' || *fim_conversao != '\0' || limite < 2 || limite > INT_MAX;

    return limite_invalido ? -1 : (int)limite;
}

/**
 * Valida os dados que devem ser lidos somente pelo processo mestre.
 */
static int validar_entrada(
    int argc,
    char *argv[],
    int quantidade_processos,
    int *limite
)
{
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <limite>\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (quantidade_processos < 2) {
        fprintf(stderr, "A execucao paralela exige pelo menos dois processos.\n");
        return EXIT_FAILURE;
    }

    *limite = ler_limite(argv[1]);

    if (*limite == -1) {
        fprintf(stderr, "O limite deve ser um inteiro maior ou igual a 2.\n");
        return EXIT_FAILURE;
    }

    const int quantidade_escravos = quantidade_processos - 1;

    if (quantidade_escravos > *limite - 1) {
        fprintf(stderr, "A quantidade de escravos excede o intervalo informado.\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/**
 * Encerra todos os processos quando ocorre uma falha irrecuperavel.
 */
static void abortar_execucao(int rank, const char *mensagem)
{
    fprintf(stderr, "Processo %d: %s\n", rank, mensagem);
    MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    exit(EXIT_FAILURE);
}

/**
 * Calcula os primos necessarios para processar os blocos dos escravos.
 *
 * @return A lista de primos ou NULL quando a alocacao falhar.
 */
static int *calcular_primos_base(int limite, int *quantidade_primos_base)
{
    const int limite_base = (int)sqrt((double)limite);
    bool *compostos = calloc((size_t)limite_base + 1, sizeof(*compostos));
    int *primos = malloc(((size_t)limite_base + 1) * sizeof(*primos));

    if (compostos == NULL || primos == NULL) {
        free(compostos);
        free(primos);
        return NULL;
    }

    for (int primo = 2; primo <= limite_base / primo; primo++) {
        if (compostos[primo])
            continue;

        for (int multiplo = primo * primo; multiplo <= limite_base; multiplo += primo) {
            compostos[multiplo] = true;
        }
    }

    for (int numero = 2; numero <= limite_base; numero++) {
        if (!compostos[numero])
            primos[(*quantidade_primos_base)++] = numero;
    }

    free(compostos);
    return primos;
}

/**
 * Prepara no mestre a lista compartilhada de primos-base.
 */
static int preparar_primos_base(
    int rank,
    int limite,
    int **primos_base,
    int *quantidade_primos_base
)
{
    int status = EXIT_SUCCESS;

    if (rank == RANK_MESTRE) {
        *primos_base = calcular_primos_base(limite, quantidade_primos_base);

        if (*primos_base == NULL) {
            fprintf(stderr, "Nao foi possivel calcular os primos-base.\n");
            status = EXIT_FAILURE;
        }
    }

    MPI_Bcast(&status, 1, MPI_INT, RANK_MESTRE, MPI_COMM_WORLD);
    return status;
}

/**
 * Distribui os primos-base calculados pelo mestre.
 */
static void distribuir_primos_base(
    int rank,
    int **primos_base,
    int *quantidade_primos_base
)
{
    MPI_Bcast(quantidade_primos_base, 1, MPI_INT, RANK_MESTRE, MPI_COMM_WORLD);

    if (rank != RANK_MESTRE && *quantidade_primos_base > 0) {
        *primos_base = malloc(
            (size_t)*quantidade_primos_base * sizeof(**primos_base)
        );

        if (*primos_base == NULL)
            abortar_execucao(rank, "Nao foi possivel receber os primos-base.");
    }

    MPI_Bcast(
        *primos_base,
        *quantidade_primos_base,
        MPI_INT,
        RANK_MESTRE,
        MPI_COMM_WORLD
    );
}

/**
 * Calcula o bloco estatico atribuido a um processo escravo.
 */
static void calcular_intervalo(
    int rank_escravo,
    int quantidade_processos,
    int limite,
    int intervalo[2]
)
{
    const int quantidade_escravos = quantidade_processos - 1;
    const int quantidade_numeros = limite - 1;
    const int tamanho_base = quantidade_numeros / quantidade_escravos;
    const int indice_escravo = rank_escravo - 1;

    intervalo[0] = 2 + indice_escravo * tamanho_base;
    intervalo[1] = rank_escravo == quantidade_processos - 1
        ? limite
        : intervalo[0] + tamanho_base - 1;
}

/**
 * Envia um bloco continuo para cada processo escravo.
 */
static void distribuir_intervalos(int quantidade_processos, int limite)
{
    for (int rank = 1; rank < quantidade_processos; rank++) {
        int intervalo[2];

        calcular_intervalo(rank, quantidade_processos, limite, intervalo);
        MPI_Send(intervalo, 2, MPI_INT, rank, TAG_INTERVALO, MPI_COMM_WORLD);
    }
}

/**
 * Marca os numeros compostos existentes em um intervalo local.
 *
 * @return O vetor de marcacoes ou NULL quando a alocacao falhar.
 */
static bool *criar_crivo_local(
    int inicio,
    int fim,
    const int *primos_base,
    int quantidade_primos_base
)
{
    const size_t tamanho = (size_t)(fim - inicio + 1);
    bool *compostos = calloc(tamanho, sizeof(*compostos));

    if (compostos == NULL)
        return NULL;

    for (int indice = 0; indice < quantidade_primos_base; indice++) {
        const int primo = primos_base[indice];
        const long long quadrado = (long long)primo * primo;
        long long primeiro_multiplo =
            ((long long)inicio + primo - 1) / primo * primo;

        primeiro_multiplo = primeiro_multiplo < quadrado
            ? quadrado
            : primeiro_multiplo;

        for (
            long long multiplo = primeiro_multiplo;
            multiplo <= fim;
            multiplo += primo
        ) {
            compostos[multiplo - inicio] = true;
        }
    }

    return compostos;
}

/**
 * Cria a lista de primos encontrados em um bloco.
 *
 * @return A lista de primos ou NULL quando estiver vazia ou a alocacao falhar.
 */
static int *listar_primos(
    const bool *compostos,
    int inicio,
    int fim,
    int *quantidade_primos
)
{
    for (int numero = inicio; numero <= fim; numero++)
        *quantidade_primos += compostos[numero - inicio] ? 0 : 1;

    if (*quantidade_primos == 0)
        return NULL;

    int *primos = malloc((size_t)*quantidade_primos * sizeof(*primos));

    if (primos == NULL)
        return NULL;

    int indice_primo = 0;

    for (int numero = inicio; numero <= fim; numero++) {
        if (!compostos[numero - inicio])
            primos[indice_primo++] = numero;
    }

    return primos;
}

/**
 * Processa e devolve o bloco atribuido a um escravo.
 */
static void executar_escravo(
    int rank,
    const int *primos_base,
    int quantidade_primos_base
)
{
    int intervalo[2];

    MPI_Recv(
        intervalo,
        2,
        MPI_INT,
        RANK_MESTRE,
        TAG_INTERVALO,
        MPI_COMM_WORLD,
        MPI_STATUS_IGNORE
    );

    bool *compostos = criar_crivo_local(
        intervalo[0],
        intervalo[1],
        primos_base,
        quantidade_primos_base
    );

    if (compostos == NULL)
        abortar_execucao(rank, "Nao foi possivel criar o crivo local.");

    int quantidade_primos = 0;
    int *primos = listar_primos(
        compostos,
        intervalo[0],
        intervalo[1],
        &quantidade_primos
    );

    if (quantidade_primos > 0 && primos == NULL)
        abortar_execucao(rank, "Nao foi possivel criar a lista local de primos.");

    MPI_Send(
        &quantidade_primos,
        1,
        MPI_INT,
        RANK_MESTRE,
        TAG_QUANTIDADE_PRIMOS,
        MPI_COMM_WORLD
    );

    if (quantidade_primos > 0) {
        MPI_Send(
            primos,
            quantidade_primos,
            MPI_INT,
            RANK_MESTRE,
            TAG_LISTA_PRIMOS,
            MPI_COMM_WORLD
        );
    }

    free(compostos);
    free(primos);
}

/**
 * Recebe as listas locais na ordem dos ranks.
 */
static int **receber_resultados(
    int quantidade_processos,
    int **quantidades_por_escravo
)
{
    const int quantidade_escravos = quantidade_processos - 1;
    int **primos_por_escravo = calloc(
        (size_t)quantidade_escravos,
        sizeof(*primos_por_escravo)
    );

    *quantidades_por_escravo = calloc(
        (size_t)quantidade_escravos,
        sizeof(**quantidades_por_escravo)
    );

    if (primos_por_escravo == NULL || *quantidades_por_escravo == NULL) {
        free(primos_por_escravo);
        free(*quantidades_por_escravo);
        abortar_execucao(RANK_MESTRE, "Nao foi possivel receber os resultados.");
    }

    for (int rank = 1; rank < quantidade_processos; rank++) {
        const int indice = rank - 1;

        MPI_Recv(
            &(*quantidades_por_escravo)[indice],
            1,
            MPI_INT,
            rank,
            TAG_QUANTIDADE_PRIMOS,
            MPI_COMM_WORLD,
            MPI_STATUS_IGNORE
        );

        const int quantidade_recebida = (*quantidades_por_escravo)[indice];

        if (quantidade_recebida == 0)
            continue;

        primos_por_escravo[indice] = malloc(
            (size_t)quantidade_recebida * sizeof(*primos_por_escravo[indice])
        );

        if (primos_por_escravo[indice] == NULL)
            abortar_execucao(RANK_MESTRE, "Nao foi possivel armazenar os resultados.");

        MPI_Recv(
            primos_por_escravo[indice],
            quantidade_recebida,
            MPI_INT,
            rank,
            TAG_LISTA_PRIMOS,
            MPI_COMM_WORLD,
            MPI_STATUS_IGNORE
        );
    }

    return primos_por_escravo;
}

static int criar_diretorio_resultados(void)
{
    if (mkdir(DIRETORIO_RESULTADOS, 0775) != 0 && errno != EEXIST) {
        fprintf(stderr, "Nao foi possivel criar o diretorio de resultados.\n");
        return EXIT_FAILURE;
    }

    if (mkdir(DIRETORIO_PARALELO, 0775) == 0 || errno == EEXIST)
        return EXIT_SUCCESS;

    fprintf(stderr, "Nao foi possivel criar o diretorio paralelo.\n");
    return EXIT_FAILURE;
}

/**
 * Mantem a lista de primos nas execucoes comuns e permite omiti-la nos testes.
 */
static bool deve_salvar_primos(void)
{
    const char *valor = getenv("SALVAR_PRIMOS");

    return valor == NULL || *valor != '0';
}

/**
 * Escreve a lista final de primos, mantendo a ordem dos blocos.
 */
static int salvar_primos(
    int **primos_por_escravo,
    const int *quantidades_por_escravo,
    int quantidade_escravos
)
{
    if (criar_diretorio_resultados() != EXIT_SUCCESS)
        return EXIT_FAILURE;

    FILE *arquivo = fopen(ARQUIVO_PRIMOS, "w");

    if (arquivo == NULL) {
        fprintf(stderr, "Nao foi possivel criar %s.\n", ARQUIVO_PRIMOS);
        return EXIT_FAILURE;
    }

    for (int indice = 0; indice < quantidade_escravos; indice++) {
        for (
            int indice_primo = 0;
            indice_primo < quantidades_por_escravo[indice];
            indice_primo++
        ) {
            const int primo = primos_por_escravo[indice][indice_primo];

            if (fprintf(arquivo, "%d\n", primo) < 0) {
                fclose(arquivo);
                fprintf(stderr, "Nao foi possivel escrever em %s.\n", ARQUIVO_PRIMOS);
                return EXIT_FAILURE;
            }
        }
    }

    if (fclose(arquivo) != 0) {
        fprintf(stderr, "Nao foi possivel finalizar %s.\n", ARQUIVO_PRIMOS);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/**
 * Acrescenta os dados da execucao ao historico de resultados.
 */
static int registrar_resultado(
    int limite,
    int quantidade_processos,
    int quantidade_primos,
    double tempo_execucao
)
{
    if (criar_diretorio_resultados() != EXIT_SUCCESS)
        return EXIT_FAILURE;

    FILE *arquivo = fopen(ARQUIVO_RESULTADOS, "a+");

    if (arquivo == NULL) {
        fprintf(stderr, "Nao foi possivel abrir %s.\n", ARQUIVO_RESULTADOS);
        return EXIT_FAILURE;
    }

    if (fseek(arquivo, 0, SEEK_END) != 0) {
        fclose(arquivo);
        fprintf(stderr, "Nao foi possivel consultar %s.\n", ARQUIVO_RESULTADOS);
        return EXIT_FAILURE;
    }

    const long tamanho_arquivo = ftell(arquivo);

    if (tamanho_arquivo < 0) {
        fclose(arquivo);
        fprintf(stderr, "Nao foi possivel consultar %s.\n", ARQUIVO_RESULTADOS);
        return EXIT_FAILURE;
    }

    bool escrita_falhou = false;

    if (tamanho_arquivo == 0) {
        escrita_falhou = fprintf(
            arquivo,
            "limite,processos,quantidade_primos,tempo_segundos\n"
        ) < 0;
    }

    if (!escrita_falhou) {
        escrita_falhou = fprintf(
            arquivo,
            "%d,%d,%d,%.9f\n",
            limite,
            quantidade_processos,
            quantidade_primos,
            tempo_execucao
        ) < 0;
    }

    if (fclose(arquivo) != 0 || escrita_falhou) {
        fprintf(stderr, "Nao foi possivel salvar o resultado.\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static void liberar_resultados(
    int **primos_por_escravo,
    int *quantidades_por_escravo,
    int quantidade_escravos
)
{
    for (int indice = 0; indice < quantidade_escravos; indice++)
        free(primos_por_escravo[indice]);

    free(primos_por_escravo);
    free(quantidades_por_escravo);
}

/**
 * Coordena a distribuicao, recebe os resultados e gera a saida final.
 */
static int executar_mestre(
    int limite,
    int quantidade_processos,
    double inicio_tempo
)
{
    const int quantidade_escravos = quantidade_processos - 1;

    distribuir_intervalos(quantidade_processos, limite);

    int *quantidades_por_escravo = NULL;
    int **primos_por_escravo = receber_resultados(
        quantidade_processos,
        &quantidades_por_escravo
    );

    const double tempo_execucao = MPI_Wtime() - inicio_tempo;
    int quantidade_total = 0;

    for (int indice = 0; indice < quantidade_escravos; indice++)
        quantidade_total += quantidades_por_escravo[indice];

    int status = EXIT_SUCCESS;

    if (deve_salvar_primos()) {
        status = salvar_primos(
            primos_por_escravo,
            quantidades_por_escravo,
            quantidade_escravos
        );
    }

    if (status == EXIT_SUCCESS) {
        printf("Quantidade de primos ate %d: %d\n", limite, quantidade_total);
        printf("Tempo paralelo: %.6f segundos\n", tempo_execucao);

        status = registrar_resultado(
            limite,
            quantidade_processos,
            quantidade_total,
            tempo_execucao
        );
    }

    liberar_resultados(
        primos_por_escravo,
        quantidades_por_escravo,
        quantidade_escravos
    );

    return status;
}

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);

    int rank;
    int quantidade_processos;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &quantidade_processos);

    int limite = 0;
    int status = EXIT_SUCCESS;

    if (rank == RANK_MESTRE)
        status = validar_entrada(argc, argv, quantidade_processos, &limite);

    MPI_Bcast(&status, 1, MPI_INT, RANK_MESTRE, MPI_COMM_WORLD);

    if (status == EXIT_SUCCESS)
        MPI_Bcast(&limite, 1, MPI_INT, RANK_MESTRE, MPI_COMM_WORLD);

    int quantidade_primos_base = 0;
    int *primos_base = NULL;

    if (status == EXIT_SUCCESS) {
        status = preparar_primos_base(
            rank,
            limite,
            &primos_base,
            &quantidade_primos_base
        );
    }

    if (status == EXIT_SUCCESS) {
        MPI_Barrier(MPI_COMM_WORLD);

        const double inicio = rank == RANK_MESTRE ? MPI_Wtime() : 0.0;

        distribuir_primos_base(
            rank,
            &primos_base,
            &quantidade_primos_base
        );

        if (rank == RANK_MESTRE)
            status = executar_mestre(limite, quantidade_processos, inicio);
        else
            executar_escravo(rank, primos_base, quantidade_primos_base);

        MPI_Bcast(&status, 1, MPI_INT, RANK_MESTRE, MPI_COMM_WORLD);
    }

    free(primos_base);
    MPI_Finalize();
    return status;
}
