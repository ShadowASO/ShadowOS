/*--------------------------------------------------------------------------
 *  File name:  device.c
 *  Author:  Aldenor Sombra de Oliveira
 *  Data de criação: 15-10-2022
 *--------------------------------------------------------------------------
 *  Rotinas para inserir um caractere em uma strings, com ajuste no tamanho.
 *--------------------------------------------------------------------------*/
#pragma once

#ifndef _DEVICE_H
#define _DEVICE_H

#include "../include/libc/stdint.h"
#include "../include/libc/stddef.h"
#include "../include/libc/stdbool.h"

#include "ktypes.h"

struct __fs_t;

typedef enum __device_type
{
    DEVICE_UNKNOWN = 0,
    DEVICE_CHAR = 1,
    DEVICE_BLOCK = 2,
} device_type;

typedef struct
{
    u32_t id;
    const char *name;

    device_type dev_type;
    struct __fs_t *fs;
    uint8_t (*read)(uint8_t *buffer, uint32_t offset, uint32_t len, void *dev);
    uint8_t (*write)(uint8_t *buffer, uint32_t offset, uint32_t len, void *dev);
    void *priv;
} device_t;

extern void device_print_out(void);

extern void device_init(void);
extern int device_add(device_t *dev);
extern device_t *device_get(uint32_t id);
device_t *device_get_by_id(uint32_t id);
extern int device_getnumber(void);

#endif
