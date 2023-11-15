//#include <stdlib.h>
#include <stdio.h>
#include "keyboard.h"
#include <ascii.h>
#include "ktypes.h"

static u64_t stoi(char *s) // the message and then the line #
{
    u64_t i;
    i = 0;
    while (*s >= '0' && *s <= '9')
    {
        i = i * 10 + (*s - '0');
        s++;
    }
    return i;
}
static int strcmp1(const char *p, const char *q)
{
    while (*p || *q)
    {
        if (*p != *q)
            return -1;
        p++, q++;
    }
    return 0;
}

void scanf(char *str, void *buf)
{
    char temp_buf[100];

    int i = 0;
    char c;
    while ((c = kgetchar()) != KEY_ENTER)
    {
        temp_buf[i] = c;
        i++;
    }
    temp_buf[i] = '\0';

    if (strcmp1(str, "%s") == 0)
    {
        buf = (char *)buf;
        //        printf("----- %c", temp_buf[0]);
        //        printf("in");
        // buf = temp_buf;
        i = 0;
        while (temp_buf[i] != '\0')
        {
            ((char *)buf)[i] = temp_buf[i];
            i++;
        }
        //((char *)buf)[i] = '\0';
        //        printf("%d", i);
    }
    else if (strcmp1(str, "%c") == 0)
    {
        *(char *)buf = temp_buf[0];
    }
    else if (strcmp1(str, "%d") == 0)
    {
        *(int *)buf = stoi(temp_buf);
    }
    else if (strcmp1(str, "%x") == 0)
    {
        // buf = temp_buf;
    }
}
