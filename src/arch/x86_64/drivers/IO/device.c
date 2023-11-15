#include <stdint.h>
#include "stdio.h"
#include "device.h"
#include "mm/kmalloc.h"
#include "string.h"

// MODULE("DEV");

device_t *devices = 0;
uint8_t lastid = 0;

void device_init()
{
    devices = (device_t *)kmalloc(64 * sizeof(device_t));
    memset(devices, 0, 64 * sizeof(device_t));
    lastid = 0;
    kprintf("Device Manager initialized.\n");
    //_kill();
}

void device_print_out()
{
    for (int i = 0; i < lastid; i++)
    {
        // if(!devices[lastid]) return;
        kprintf("id: %d, unique: %d, %s, %s\n", i, devices[i].id,
                devices[i].dev_type == DEVICE_CHAR ? "CHAR" : "BLOCK", devices[i].name);
    }
}

int device_add(device_t *dev)
{
    devices[lastid] = *dev;
    kprintf("Registered Device %s (%d) as Device#%d\n", dev->name, dev->id, lastid);
    lastid++;
    return lastid - 1;
}

device_t *device_get_by_id(uint32_t id)
{
    for (int i = 0; i < 64; i++)
    {
        if (devices[i].id == id)
            return &devices[i];
    }
    return 0;
}

int device_getnumber()
{
    return lastid;
}

device_t *device_get(uint32_t id)
{
    return &devices[id];
}