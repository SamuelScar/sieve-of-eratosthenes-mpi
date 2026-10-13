#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * Converte o argumento recebido em um limite valido para o crivo.
 *
 * @return O limite convertido ou -1 quando o valor for invalido.
 */
static int ler_limite(const char *valor)
{
    char *fim;
    long limite = strtol(valor, &fim, 10);

    const bool limite_invalido =
        *valor == '\0' || *fim != '\0' || limite < 2 || limite > INT_MAX;

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

        // Multiplos menores que primo * primo ja foram eliminados anteriormente.
        for (size_t multiplo = primo * primo; multiplo <= limite; multiplo += primo) {
            compostos[multiplo] = true;
        }
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

    bool *compostos = criar_crivo((size_t)limite);

    if (compostos == NULL) {
        fprintf(stderr, "Nao foi possivel alocar memoria.\n");
        return EXIT_FAILURE;
    }

    const int quantidade_primos = contar_primos(compostos, (size_t)limite);

    printf("Quantidade de primos ate %d: %d\n", limite, quantidade_primos);

    free(compostos);
    return EXIT_SUCCESS;
}
