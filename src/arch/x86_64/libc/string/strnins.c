/*--------------------------------------------------------------------------
*  File name:  strnins.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 08-02-2021
*--------------------------------------------------------------------------
*  Rotinas para inserir um caractere em uma strings, com ajuste no tamanho.
*--------------------------------------------------------------------------*/
#include "string.h"

/**
 * @brief Insere um caractere na posição n de uma string, deslocando o restante da string.
 * Cabe ao usuário garantir que a string possua espaço suficiente para a inserção.
 * 
 * @param str 
 * @param c 
 * @param n 
 * @return char* 
 */
char *strnins(char str[], char c, size_t n)
{
    char *d = str;
    char tmp[256] = {0};

    size_t dest = 0;
    size_t orig = 0;
    while (d[orig] != NULL_CHAR)
    {
        if (dest == n)
        {
            tmp[dest] = c;
            dest++;
        }
        tmp[dest] = d[orig];

        dest++;
        orig++;
    }
    tmp[dest] = NULL_CHAR;

    strcpy(str, tmp);
    return str;
}