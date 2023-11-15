/*--------------------------------------------------------------------------
*  File name:  strtrim.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 07-02-2021
*--------------------------------------------------------------------------*/
//Rotinas para eliminar espaços em branco na string
//--------------------------------------------------------------------------
#include "string.h"
#include "ascii.h"

char *strTrim(char *str)
{
    strLeftTrim(str);
    strRightTrim(str);
    return str;
}

char *strLeftTrim(char *str)
{
    int orig = 0;
    int dest = 0;
    while (str[orig] != '\0' && str[orig] == KEY_SPACE)
    {
        orig++;
    }

    while (str[orig] != '\0' && str[orig] != KEY_SPACE)
    {
        str[dest] = str[orig];
        dest++;
        orig++;
    }
    str[dest] = '\0';
    return str;
}

char *strRightTrim(char *str)
{
    int last = strlen(str) - 1;

    while (last >= 0 && str[last] == KEY_SPACE)
    {       
        str[last] = '\0';       
        last--;
    }

    return str;
}