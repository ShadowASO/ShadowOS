/*--------------------------------------------------------------------------
*  File name:  stdio.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 06-12-2020
*--------------------------------------------------------------------------
Este arquivo fonte possui o handler das interrupções de teclado e diversos 
recursos relacionados com a manipulação das entradas do teclado.
--------------------------------------------------------------------------*/

#include "keyboard.h"
#include "stdio.h"

uint8_t kgetchar()
{
    return kgetch();
}
