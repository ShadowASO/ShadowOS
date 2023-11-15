/*--------------------------------------------------------------------------
*  File name:  abnt2.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 16-12-2020
-------------------------------------------------------------------------
Este arquivo fonte possui o handler das interrupções de teclado e diversos
recursos relacionados com a manipulação das entradas do teclado.
--------------------------------------------------------------------------*/
#include <stdint.h>
#include "keyboard.h"
#include "io.h"
#include "screen.h"
#include "stdio.h"
#include "isr.h"
#include "x86_64.h"
#include "abnt2.h"

void keyboard_abnt2(scvk_t *keymap)
{
    for (int i = 0; i < 256; i++)
    {
        int sc = keymap_abnt2[i].sc;
        keymap[sc] = (scvk_t)keymap_abnt2[i];
    }
}