//=================================================================
// Copyright 2017 Advanced Micro Devices, Inc. All rights reserved.
//=================================================================
#ifndef _CI_ID_H
#define _CI_ID_H

enum
{

    CI_BONAIRE_M_A0 = 20,
    CI_BONAIRE_M_A1 = 21,

    CI_HAWAII_P_A0  = 40,

    CI_MAUI_P_A0    = 60,

    CI_UNKNOWN      = 0xFF
};

#endif  // _CI_ID_H


#ifndef _CZ_ID_H
#define _CZ_ID_H

enum
{
    CARRIZO_A0      = 0x01,
    CARRIZO_A1      = 0x02,
    STONEY_A0       = 0x61,
    CZ_UNKNOWN      = 0xFF
};

#endif // _CZ_ID_H

#ifndef KV_ID_H
#define KV_ID_H

// SW revision section
enum
{
    KV_SPECTRE_A0      = 0x01,       // KV1 with Spectre GFX core, 8-8-1-2 (CU-Pix-Primitive-RB)
    KV_SPOOKY_A0       = 0x41,       // KV2 with Spooky GFX core, including downgraded from Spectre core, 3-4-1-1 (CU-Pix-Primitive-RB)
    KB_KALINDI_A0      = 0x81,       // KB with Kalindi GFX core, 2-4-1-1 (CU-Pix-Primitive-RB)
    KB_KALINDI_A1      = 0x82,       // KB with Kalindi GFX core, 2-4-1-1 (CU-Pix-Primitive-RB)
    BV_KALINDI_A2      = 0x85,       // BV with Kalindi GFX core, 2-4-1-1 (CU-Pix-Primitive-RB)
    ML_GODAVARI_A0     = 0xa1,      // ML with Godavari GFX core, 2-4-1-1 (CU-Pix-Primitive-RB)
    ML_GODAVARI_A1     = 0xa2,      // ML with Godavari GFX core, 2-4-1-1 (CU-Pix-Primitive-RB)
    KV_UNKNOWN = 0xFF
};

#endif  // KV_ID_H

#ifndef _SI_ID_H
#define _SI_ID_H

enum
{
    SI_TAHITI_P_A11      = 1,
    SI_TAHITI_P_A0       = SI_TAHITI_P_A11,      //A0 is alias of A11
    SI_TAHITI_P_A21      = 5,
    SI_TAHITI_P_B0       = SI_TAHITI_P_A21,      //B0 is alias of A21
    SI_TAHITI_P_A22      = 6,
    SI_TAHITI_P_B1       = SI_TAHITI_P_A22,      //B1 is alias of A22

    SI_PITCAIRN_PM_A11   = 20,
    SI_PITCAIRN_PM_A0    = SI_PITCAIRN_PM_A11,   //A0 is alias of A11
    SI_PITCAIRN_PM_A12   = 21,
    SI_PITCAIRN_PM_A1    = SI_PITCAIRN_PM_A12,   //A1 is alias of A12

    SI_CAPEVERDE_M_A11   = 40,
    SI_CAPEVERDE_M_A0    = SI_CAPEVERDE_M_A11,   //A0 is alias of A11
    SI_CAPEVERDE_M_A12   = 41,
    SI_CAPEVERDE_M_A1    = SI_CAPEVERDE_M_A12,   //A1 is alias of A12

    SI_OLAND_M_A0        = 60,

    SI_HAINAN_V_A0       = 70,

    SI_UNKNOWN           = 0xFF
};

#endif  // _SI_ID_H

#ifndef _VI_ID_H
#define _VI_ID_H
#endif

enum {
    VI_ICELAND_M_A0   = 1,

    VI_TONGA_P_A0     = 20,
    VI_TONGA_P_A1     = 21,

    VI_BERMUDA_P_A0   = 40,

    VI_FIJI_P_A0      = 60,

    VI_ELLESMERE_P_A0 = 80,
    VI_ELLESMERE_P_A1 = 81,

    VI_BAFFIN_M_A0    = 90,
    VI_BAFFIN_M_A1    = 91,

    VI_LEXA_V_A0     = 100,

    VI_UNKNOWN        = 0xFF
};

enum {
    NV_NAVI10_P_A0 = 1,
    NV_NAVI12_P_A0 = 10,
    NV_NAVI14_M_A0 = 20,
    NV_NAVI21_P_A0 = 40,
    NV_NAVI22_P_A0 = 50,
    NV_NAVI23_P_A0 = 60,
    NV_NAVI24_P_A0 = 70
};

enum {
    AI_GD_P0       = 1,
    AI_GD_P1       = 2,
    AI_VEGA12_P_A0 = 20,
    AI_VEGA20_P_A0 = 40,
    AI_UNKNOWN     = 0xFF
};

enum
{
    REMBRANDT_A0 = 1
};

#ifndef _ATIID_H
#define _ATIID_H


#define FAMILY_NI                      100
#define FAMILY_NORTHERNISLAND          FAMILY_NI

#define FAMILY_SI                      110

#define FAMILY_TN                      105

#define FAMILY_CI                      120

#define FAMILY_KV                      125

#define FAMILY_VI                      130

#define FAMILY_CZ                      135

#define FAMILY_PI                      140

#define FAMILY_AI                      141

#define FAMILY_NV                      143

#define FAMILY_RMB                     146

#define ATI_VENDOR_ID                   0x1002

#endif  // _ATIID_H
