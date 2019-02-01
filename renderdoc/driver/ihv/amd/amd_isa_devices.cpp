/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2019 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "amd_isa_devices.h"
#include "common/common.h"
#include "official/RGA/Common/asic_reg/devices.h"

static constexpr int allAPIs = (1 << (int)GraphicsAPI::D3D11) | (1 << (int)GraphicsAPI::D3D12) |
                               (1 << (int)GraphicsAPI::Vulkan) | (1 << (int)GraphicsAPI::OpenGL);

// gfx900 isn't currently supported on GL
static constexpr int notOpenGL = (1 << (int)GraphicsAPI::D3D11) | (1 << (int)GraphicsAPI::D3D12) |
                                 (1 << (int)GraphicsAPI::Vulkan);

const GCNISA::asic GCNISA::asicInfo[] = {
    // Southern Islands
    {"GCN (Tahiti)", "6", FAMILY_SI, SI_TAHITI_P_B1, allAPIs},
    {"GCN (Pitcairn)", "6", FAMILY_SI, SI_PITCAIRN_PM_A1, allAPIs},
    {"GCN (Capeverde)", "6", FAMILY_SI, SI_CAPEVERDE_M_A1, allAPIs},
    {"GCN (Oland)", "6", FAMILY_SI, SI_OLAND_M_A0, allAPIs},
    {"GCN (Hainan)", "6", FAMILY_SI, SI_HAINAN_V_A0, allAPIs},
    // Sea Islands
    {"GCN (Bonaire)", "7", FAMILY_CI, CI_BONAIRE_M_A0, allAPIs},
    {"GCN (Hawaii)", "7", FAMILY_CI, CI_HAWAII_P_A0, allAPIs},
    {"GCN (Spectre)", "7", FAMILY_CI, KV_SPECTRE_A0, allAPIs},
    {"GCN (Spooky)", "7", FAMILY_CI, KV_SPOOKY_A0, allAPIs},
    {"GCN (Kalindi)", "7.x", FAMILY_CI, CI_BONAIRE_M_A0, allAPIs},
    {"GCN (Mullins)", "7", FAMILY_CI, CI_BONAIRE_M_A0, allAPIs},
    // Volcanic Islands
    {"GCN (Iceland)", "8", FAMILY_VI, VI_ICELAND_M_A0, allAPIs},
    {"GCN (Tonga)", "8", FAMILY_VI, VI_TONGA_P_A0, allAPIs},
    {"GCN (Carrizo)", "8", FAMILY_VI, CARRIZO_A0, allAPIs},
    {"GCN (Bristol Ridge)", "8", FAMILY_VI, CARRIZO_A0, allAPIs},
    {"GCN (Carrizo)", "8", FAMILY_VI, CARRIZO_A0, allAPIs},
    {"GCN (Fiji)", "8", FAMILY_VI, VI_FIJI_P_A0, allAPIs},
    {"GCN (Stoney)", "8.1", FAMILY_VI, STONEY_A0, allAPIs},
    {"GCN (Ellesmere)", "8", FAMILY_VI, VI_ELLESMERE_P_A0, allAPIs},
    {"GCN (Baffin)", "8", FAMILY_VI, VI_BAFFIN_M_A0, allAPIs},
    {"GCN (gfx804)", "8", FAMILY_VI, VI_LEXA_V_A0, allAPIs},
    // Arctic Islands
    {"GCN (gfx900)", "900", FAMILY_AI, AI_GD_P0, notOpenGL},
    {"GCN (gfx902)", "902", FAMILY_AI, AI_GD_P0, notOpenGL},
};

RDCCOMPILE_ASSERT(ARRAY_COUNT(GCNISA::asicInfo) == GCNISA::asicCount, "Mismatched array count");