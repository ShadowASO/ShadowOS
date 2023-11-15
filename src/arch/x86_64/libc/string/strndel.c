/*--------------------------------------------------------------------------
*  File name:  strndel.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 08-02-2021
*--------------------------------------------------------------------------
*  Rotinas para deletar um caractere de uma strings, com ajuste no tamanho.
*--------------------------------------------------------------------------*/
#include "string.h"

/**
 * @brief Deleta o caractere da string, na posição indicada em 'n'
 * 
 * @param str 
 * @param n 
 * @return char* 
 */
char *strndel(char *str, size_t n)
{
    char *d = str;
    size_t dest = 0;
    size_t orig = 0;

    while (d[orig])
    {
        if (orig == n)
        {
            orig++;
        }
        d[dest] = d[orig];

        dest++;
        orig++;
    }
    d[dest] = NULL_CHAR;
    return str;
}