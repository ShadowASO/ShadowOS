/*--------------------------------------------------------------------------
*  File name:  reverse.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 22-07-2020
*--------------------------------------------------------------------------*/
//Rotinas para reverter uma strings.
//--------------------------------------------------------------------------
#include "string.h"

/**
 * @brief Recebe um ponteiro para uma string e o tamanho, e inverte a ordem
 * dos elementos. O tamanho da string deve incluir o caractere null.
 * 
 * 
 * @param str 
 * @param length 
 */
void reverse(char str[], int len)
{
    char *p = str;
    int start = 0;
    int end = len - 1; //porque a cópia começa no index 0(zero)
    char tmp;

    while (start < end)
    {
        tmp = p[start];
        p[start] = p[end];
        p[end] = tmp;

        start++;
        end--;
    }
}