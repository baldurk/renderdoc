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
#pragma once

#include <unordered_map>

typedef std::unordered_map<void *, VkLayerDispatchTable *> device_table_map;
typedef std::unordered_map<void *, VkLayerInstanceDispatchTable *> instance_table_map;
VkLayerDispatchTable * initDeviceTable(const VkBaseLayerObject *devw);
VkLayerDispatchTable * initDeviceTable(device_table_map &map, const VkBaseLayerObject *devw);
VkLayerInstanceDispatchTable * initInstanceTable(const VkBaseLayerObject *instancew);
VkLayerInstanceDispatchTable * initInstanceTable(instance_table_map &map, const VkBaseLayerObject *instancew);

typedef void *dispatch_key;

static inline dispatch_key get_dispatch_key(const void* object)
{
    return (dispatch_key) *(VkLayerDispatchTable **) object;
}

VkLayerDispatchTable *device_dispatch_table(void* object);

VkLayerInstanceDispatchTable *instance_dispatch_table(void* object);

VkLayerDispatchTable *get_dispatch_table(device_table_map &map, void* object);

VkLayerInstanceDispatchTable *get_dispatch_table(instance_table_map &map, void* object);

void destroy_device_dispatch_table(dispatch_key key);
void destroy_instance_dispatch_table(dispatch_key key);
