#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>

#define DIRETORIO_RESULTADOS "resultados"
#define DIRETORIO_SEQUENCIAL DIRETORIO_RESULTADOS "/sequencial"
#define ARQUIVO_RESULTADOS DIRETORIO_SEQUENCIAL "/execucoes.csv"

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
 * Cria o crivo e marca os numeros compostos ate o limite informado.
 *
 * @return O crivo preenchido ou NULL quando nao for possivel alocar memoria.
 */
static bool *criar_crivo(size_t limite)
{
    bool *compostos = calloc(limite + 1, sizeof(*compostos));

    if (compostos == NULL)
        return NULL;

    for (size_t primo = 2; primo <= limite / primo; primo++) {
        if (compostos[primo])
            continue;

        // Os multiplos menores que primo * primo ja foram eliminados.
        for (size_t multiplo = primo * primo; multiplo <= limite; multiplo += primo)
            compostos[multiplo] = true;
    }

    return compostos;
}

/**
 * Conta os numeros primos representados no crivo.
 */
static int contar_primos(const bool *compostos, size_t limite)
{
    int quantidade_primos = 0;

    for (size_t numero = 2; numero <= limite; numero++) {
        quantidade_primos += compostos[numero] ? 0 : 1;
    }

    return quantidade_primos;
}

static double obter_tempo_atual(void)
{
    struct timespec tempo;

    clock_gettime(CLOCK_MONOTONIC, &tempo);
    return (double)tempo.tv_sec + (double)tempo.tv_nsec / 1e9;
}

/**
 * Acrescenta os dados da execucao ao historico de resultados.
 */
static int registrar_resultado(
    int limite,
    int quantidade_primos,
    double tempo_execucao
)
{
    if (mkdir(DIRETORIO_RESULTADOS, 0775) != 0 && errno != EEXIST) {
        fprintf(stderr, "Nao foi possivel criar o diretorio de resultados.\n");
        return EXIT_FAILURE;
    }

    if (mkdir(DIRETORIO_SEQUENCIAL, 0775) != 0 && errno != EEXIST) {
        fprintf(stderr, "Nao foi possivel criar o diretorio sequencial.\n");
        return EXIT_FAILURE;
    }

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
            1,
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

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <limite>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const int limite = ler_limite(argv[1]);

    if (limite == -1) {
        fprintf(stderr, "O limite deve ser um inteiro maior ou igual a 2.\n");
        return EXIT_FAILURE;
    }

    const double inicio = obter_tempo_atual();
    bool *compostos = criar_crivo((size_t)limite);

    if (compostos == NULL) {
        fprintf(stderr, "Nao foi possivel alocar memoria.\n");
        return EXIT_FAILURE;
    }

    const int quantidade_primos = contar_primos(compostos, (size_t)limite);
    const double tempo_execucao = obter_tempo_atual() - inicio;

    printf("Quantidade de primos ate %d: %d\n", limite, quantidade_primos);
    printf("Tempo sequencial: %.6f segundos\n", tempo_execucao);

    const int status_registro = registrar_resultado(
        limite,
        quantidade_primos,
        tempo_execucao
    );

    free(compostos);
    return status_registro;
}
