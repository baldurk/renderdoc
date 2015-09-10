#ifndef VKICD_H
#define VKICD_H

#include <stdint.h>
#include <stdbool.h>
#include "vk_platform.h"

/*
 * The ICD must reserve space for a pointer for the loader's dispatch
 * table, at the start of <each object>.
 * The ICD must initialize this variable using the SET_LOADER_MAGIC_VALUE macro.
 */

#define ICD_LOADER_MAGIC   0x01CDC0DE

typedef union _VK_LOADER_DATA {
  uintptr_t loaderMagic;
  void *loaderData;
} VK_LOADER_DATA;

static inline void set_loader_magic_value(void* pNewObject) {
    VK_LOADER_DATA *loader_info = (VK_LOADER_DATA *) pNewObject;
    loader_info->loaderMagic = ICD_LOADER_MAGIC;
}

static inline bool valid_loader_magic_value(void* pNewObject) {
    const VK_LOADER_DATA *loader_info = (VK_LOADER_DATA *) pNewObject;
    return (loader_info->loaderMagic & 0xffffffff) == ICD_LOADER_MAGIC;
}

#endif // VKICD_H

