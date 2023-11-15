/*--------------------------------------------------------------------------
*  File name:  binbit.c
*  Author:  Aldenor Sombra de Oliveira
*  Data de criação: 24-12-2020
*--------------------------------------------------------------------------
Este aplicativo recebe um número inteiro e o imprime em binários
--------------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>

char *itobs(int, char *);
void show_bstr(char *);
void inverte(char *str);

int main(void)
{
    char bin_str[8 * sizeof(int) + 1];
    int number;

    puts("Digite um número inteiro: ");
    while (scanf("%d", &number) == 1)
    {
        itobs(number, bin_str);
        printf("\nRepresentação binária do Número -> %d\n", number);
        show_bstr(bin_str);
        putchar('\n');
    }
    return 0;
}
char *itobs(int n, char *ps)
{
    int i;
    static int size = 8 * sizeof(int);
    for (i = size - 1; i >= 0; i--, n >>= 1)
    {
        ps[i] = (01 & n) + '0';
    }
    ps[size] = '\0';
    return ps;
}
void show_bstr(char *str)
{
    int i = 0;
    printf("Bin-4: ");
    while (str[i])
    {
        putchar(str[i]);
        if (++i % 4 == 0 && str[i])
            putchar('.');
    }

    //Imprime em grupos de 3 bits
    i = 0;
    int size = 8 * sizeof(int);
    int modulo = (size % 3) - 1;
    printf("\nBin-3: ");
    while (str[i])
    {
        putchar(str[i]);
        if ((++i + modulo) % 3 == 0 && str[i])
            putchar('.');
    }
}
void inverte(char *str)
{
    char tmp;
    int i = 0;
    int size = strlen(str) - 1;

    //printf("\n%d", size);
    while (str[i])
    {
        if (i < size)
        {
            tmp = str[i];
            str[i] = str[size];

            str[size] = tmp;
        }
        //putchar(str[i]);
        i++;
        size--;
    }
    return;
}
