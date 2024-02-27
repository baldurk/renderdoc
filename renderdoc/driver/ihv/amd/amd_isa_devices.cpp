/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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
#include "official/RGA/Common/AsicReg/devices.h"

const GCNISA::asic GCNISA::asicInfo[] = {
    // Southern Islands
    {"GCN (Tahiti)", "6", FAMILY_SI, SI_TAHITI_P_B1, "gfx600"},
    {"GCN (Pitcairn)", "6", FAMILY_SI, SI_PITCAIRN_PM_A1, "gfx601"},
    {"GCN (Capeverde)", "6", FAMILY_SI, SI_CAPEVERDE_M_A1, "gfx601"},
    {"GCN (Oland)", "6", FAMILY_SI, SI_OLAND_M_A0, "gfx601"},
    {"GCN (Hainan)", "6", FAMILY_SI, SI_HAINAN_V_A0, "gfx601"},
    // Sea Islands
    {"GCN (Bonaire)", "7", FAMILY_CI, CI_BONAIRE_M_A0, "gfx704"},
    {"GCN (Hawaii)", "7", FAMILY_CI, CI_HAWAII_P_A0, "gfx701"},
    {"GCN (Spectre)", "7", FAMILY_CI, KV_SPECTRE_A0, "gfx700"},
    {"GCN (Spooky)", "7", FAMILY_CI, KV_SPOOKY_A0, "gfx700"},
    {"GCN (Kalindi)", "7.x", FAMILY_CI, CI_BONAIRE_M_A0, "gfx703"},
    {"GCN (Mullins)", "7", FAMILY_CI, CI_BONAIRE_M_A0, "gfx704"},
    // Volcanic Islands
    {"GCN (Iceland)", "8", FAMILY_VI, VI_ICELAND_M_A0, "gfx802"},
    {"GCN (Tonga)", "8", FAMILY_VI, VI_TONGA_P_A0, "gfx802"},
    {"GCN (Carrizo)", "8", FAMILY_VI, CARRIZO_A0, "gfx801"},
    {"GCN (Bristol Ridge)", "8", FAMILY_VI, CARRIZO_A0, "gfx801"},
    {"GCN (Carrizo)", "8", FAMILY_VI, CARRIZO_A0, "gfx801"},
    {"GCN (Fiji)", "8", FAMILY_VI, VI_FIJI_P_A0, "gfx803"},
    {"GCN (Stoney)", "8.1", FAMILY_VI, STONEY_A0, "gfx810"},
    {"GCN (Ellesmere)", "8", FAMILY_VI, VI_ELLESMERE_P_A0, "gfx803"},
    {"GCN (Baffin)", "8", FAMILY_VI, VI_BAFFIN_M_A0, "gfx803"},
    {"GCN (gfx804)", "8", FAMILY_VI, VI_LEXA_V_A0, "gfx804"},
    // Arctic Islands
    {"GCN (gfx900)", "900", FAMILY_AI, AI_GD_P0, "gfx900"},
    {"GCN (gfx902)", "902", FAMILY_AI, AI_GD_P0, "gfx902"},
    {"GCN (gfx906)", "906", FAMILY_AI, AI_VEGA20_P_A0, "gfx906"},
    // Navi
    {"RDNA (gfx1010)", "1010", FAMILY_NV, NV_NAVI10_P_A0, "gfx1010"},
    {"RDNA (gfx1012)", "1012", FAMILY_NV, NV_NAVI14_M_A0, "gfx1012"},
    {"RDNA2 (gfx1030)", "1030", FAMILY_NV, NV_NAVI21_P_A0, "gfx1030"},
    {"RDNA2 (gfx1031)", "1031", FAMILY_NV, NV_NAVI22_P_A0, "gfx1031"},
    {"RDNA2 (gfx1032)", "1032", FAMILY_NV, NV_NAVI23_P_A0, "gfx1032"},
    {"RDNA2 (gfx1034)", "1034", FAMILY_NV, NV_NAVI24_P_A0, "gfx1034"},
    {"RDNA2 (gfx1035)", "1035", FAMILY_RMB, REMBRANDT_A0, "gfx1035"},
};

RDCCOMPILE_ASSERT(ARRAY_COUNT(GCNISA::asicInfo) == GCNISA::asicCount, "Mismatched array count");
