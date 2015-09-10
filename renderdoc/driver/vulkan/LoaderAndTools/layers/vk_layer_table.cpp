/*
 * Vulkan
 *
 * Copyright (C) 2014 LunarG, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include <assert.h>
#include <unordered_map>
#include "vk_dispatch_table_helper.h"
#include "vk_layer.h"
#include "vk_layer_table.h"
static device_table_map tableMap;
static instance_table_map tableInstanceMap;

#define DISPATCH_MAP_DEBUG 0

// Map lookup must be thread safe
VkLayerDispatchTable *device_dispatch_table(void* object)
{
    dispatch_key key = get_dispatch_key(object);
    device_table_map::const_iterator it = tableMap.find((void *) key);
    assert(it != tableMap.end() && "Not able to find device dispatch entry");
    return it->second;
}

VkLayerInstanceDispatchTable *instance_dispatch_table(void* object)
{
    dispatch_key key = get_dispatch_key(object);
    instance_table_map::const_iterator it = tableInstanceMap.find((void *) key);
#if DISPATCH_MAP_DEBUG
    if (it != tableInstanceMap.end()) {
        fprintf(stderr, "instance_dispatch_table: map: %p, object: %p, key: %p, table: %p\n", &tableInstanceMap, object, key, it->second);
    } else {
        fprintf(stderr, "instance_dispatch_table: map: %p, object: %p, key: %p, table: UNKNOWN\n", &tableInstanceMap, object, key);
    }
#endif
    assert(it != tableInstanceMap.end() && "Not able to find instance dispatch entry");
    return it->second;
}

void destroy_dispatch_table(device_table_map &map, dispatch_key key)
{
    device_table_map::const_iterator it = map.find((void *) key);
#if DISPATCH_MAP_DEBUG
    if (it != map.end()) {
        fprintf(stderr, "destroy device dispatch_table: map: %p, key: %p, table: %p\n", &map, key, it->second);
    } else {
        fprintf(stderr, "destroy device dispatch table: map: %p, key: %p, table: UNKNOWN\n", &map, key);
        assert(it != map.end());
    }
#endif
    map.erase(key);
}

void destroy_dispatch_table(instance_table_map &map, dispatch_key key)
{
    instance_table_map::const_iterator it = map.find((void *) key);
#if DISPATCH_MAP_DEBUG
    if (it != map.end()) {
        fprintf(stderr, "destroy instance dispatch_table: map: %p, key: %p, table: %p\n", &map, key, it->second);
    } else {
        fprintf(stderr, "destroy instance dispatch table: map: %p, key: %p, table: UNKNOWN\n", &map, key);
        assert(it != map.end());
    }
#endif
    map.erase(key);
}

void destroy_device_dispatch_table(dispatch_key key)
{
    destroy_dispatch_table(tableMap, key);
}

void destroy_instance_dispatch_table(dispatch_key key)
{
    destroy_dispatch_table(tableInstanceMap, key);
}

VkLayerDispatchTable *get_dispatch_table(device_table_map &map, void* object)
{
    dispatch_key key = get_dispatch_key(object);
    device_table_map::const_iterator it = map.find((void *) key);
#if DISPATCH_MAP_DEBUG
    if (it != map.end()) {
        fprintf(stderr, "instance_dispatch_table: map: %p, object: %p, key: %p, table: %p\n", &tableInstanceMap, object, key, it->second);
    } else {
        fprintf(stderr, "instance_dispatch_table: map: %p, object: %p, key: %p, table: UNKNOWN\n", &tableInstanceMap, object, key);
    }
#endif
    assert(it != map.end() && "Not able to find device dispatch entry");
    return it->second;
}

VkLayerInstanceDispatchTable *get_dispatch_table(instance_table_map &map, void* object)
{
//    VkLayerInstanceDispatchTable *pDisp = *(VkLayerInstanceDispatchTable **) object;
    dispatch_key key = get_dispatch_key(object);
    instance_table_map::const_iterator it = map.find((void *) key);
#if DISPATCH_MAP_DEBUG
    if (it != map.end()) {
        fprintf(stderr, "instance_dispatch_table: map: %p, object: %p, key: %p, table: %p\n", &tableInstanceMap, object, key, it->second);
    } else {
        fprintf(stderr, "instance_dispatch_table: map: %p, object: %p, key: %p, table: UNKNOWN\n", &tableInstanceMap, object, key);
    }
#endif
    assert(it != map.end() && "Not able to find instance dispatch entry");
    return it->second;
}

/* Various dispatchable objects will use the same underlying dispatch table if they
 * are created from that "parent" object. Thus use pointer to dispatch table
 * as the key to these table maps.
 *    Instance -> PhysicalDevice
 *    Device -> CmdBuffer or Queue
 * If use the object themselves as key to map then implies Create entrypoints have to be intercepted
 * and a new key inserted into map */
VkLayerInstanceDispatchTable * initInstanceTable(instance_table_map &map, const VkBaseLayerObject *instancew)
{
    VkLayerInstanceDispatchTable *pTable;
    assert(instancew);
    VkLayerInstanceDispatchTable **ppDisp = (VkLayerInstanceDispatchTable **) instancew->baseObject;

    std::unordered_map<void *, VkLayerInstanceDispatchTable *>::const_iterator it = map.find((void *) *ppDisp);
    if (it == map.end())
    {
        pTable =  new VkLayerInstanceDispatchTable;
        map[(void *) *ppDisp] = pTable;
#if DISPATCH_MAP_DEBUG
        fprintf(stderr, "New, Instance: map: %p, base object: %p, key: %p, table: %p\n", &map, instancew, *ppDisp, pTable);
#endif
        assert(map.size() <= 1 && "Instance dispatch table map has more than one entry");
    } else
    {
#if DISPATCH_MAP_DEBUG
        fprintf(stderr, "Instance: map: %p, base object: %p, key: %p, table: %p\n", &map, instancew, *ppDisp, it->second);
#endif
        return it->second;
    }

    layer_init_instance_dispatch_table(pTable, instancew);

    return pTable;
}

VkLayerInstanceDispatchTable * initInstanceTable(const VkBaseLayerObject *instancew)
{
    return initInstanceTable(tableInstanceMap, instancew);
}

VkLayerDispatchTable * initDeviceTable(device_table_map &map, const VkBaseLayerObject *devw)
{
    VkLayerDispatchTable *layer_device_table = NULL;
    assert(devw);
    VkLayerDispatchTable **ppDisp = (VkLayerDispatchTable **) (devw->baseObject);
    VkLayerDispatchTable *base_device_table = *ppDisp;

    std::unordered_map<void *, VkLayerDispatchTable *>::const_iterator it = map.find((void *) base_device_table);
    if (it == map.end())
    {
        layer_device_table =  new VkLayerDispatchTable;
        map[(void *) base_device_table] = layer_device_table;
#if DISPATCH_MAP_DEBUG
        fprintf(stderr, "New, Device: map: %p, base object: %p, key: %p, table: %p\n", &map, devw, *ppDisp, layer_device_table);
#endif
    } else
    {
#if DISPATCH_MAP_DEBUG
        fprintf(stderr, "Device: map: %p, base object: %p, key: %p, table: %p\n", &map, devw, *ppDisp, it->second);
#endif
        return it->second;
    }

    layer_initialize_dispatch_table(layer_device_table, devw);

    return layer_device_table;
}

VkLayerDispatchTable * initDeviceTable(const VkBaseLayerObject *devw)
{
    return initDeviceTable(tableMap, devw);
}
