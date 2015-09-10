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
 *
 * Authors:
 *   Courtney Goeltzenleuchter <courtney@lunarg.com>
 */

#include "vk_layer.h"

#ifndef LAYER_EXTENSION_UTILS_H
#define LAYER_EXTENSION_UTILS_H

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

/*
 * This file contains static functions for the generated layers
 */
extern "C" {

VkResult util_GetExtensionProperties(
        const uint32_t count,
        const VkExtensionProperties *layer_extensions,
        uint32_t* pCount,
        VkExtensionProperties* pProperties);

VkResult util_GetLayerProperties(
        const uint32_t count,
        const VkLayerProperties *layer_properties,
        uint32_t* pCount,
        VkLayerProperties* pProperties);

} // extern "C"
#endif // LAYER_EXTENSION_UTILS_H

