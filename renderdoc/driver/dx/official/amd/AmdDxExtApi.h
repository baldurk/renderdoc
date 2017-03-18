/*
***************************************************************************************************
*
*  Copyright (c) 2008-2016 Advanced Micro Devices, Inc. All rights reserved.
*
***************************************************************************************************
*/
/**
***************************************************************************************************
* @file  amddxextapi.h
* @brief AMD D3D Exension API include file. This is the main include file for apps using extensions.
***************************************************************************************************
*/
#ifndef _AMDDXEXTAPI_H_
#define _AMDDXEXTAPI_H_

#include "AmdDxExt.h"
#include "AmdDxExtIface.h"

// forward declaration of main extension interface defined lower in file
class IAmdDxExt;

// forward declaration for d3d specific interfaces
interface ID3D10Device;
interface ID3D11Device;
interface ID3D10Resource;
interface ID3D11Resource;

// App must use GetProcAddress, etc. to retrive this exported function
// The associated typedef provides a convenient way to define the function pointer
HRESULT __cdecl AmdDxExtCreate(ID3D10Device* pDevice, IAmdDxExt** ppExt);
typedef HRESULT(__cdecl* PFNAmdDxExtCreate)(ID3D10Device* pDevice, IAmdDxExt** ppExt);

HRESULT __cdecl AmdDxExtCreate11(ID3D11Device* pDevice, IAmdDxExt** ppExt);
typedef HRESULT(__cdecl* PFNAmdDxExtCreate11)(ID3D11Device* pDevice, IAmdDxExt** ppExt);

// Extension version information
struct AmdDxExtVersion
{
    unsigned int        majorVersion;
    unsigned int        minorVersion;
};

// forward declaration of classes referenced by IAmdDxExt
class IAmdDxExtInterface;

/**
***************************************************************************************************
* @brief This class serves as the main extension interface.
*
* AmdDxExtCreate returns a pointer to an instantiation of this interface.
* This object is used to retrieve extension version information
* and to get specific extension interfaces desired.
***************************************************************************************************
*/
class IAmdDxExt : public IAmdDxExtInterface
{
public:
    virtual HRESULT             GetVersion(AmdDxExtVersion* pExtVer) = 0;
    virtual IAmdDxExtInterface* GetExtInterface(unsigned int iface) = 0;

    // General extensions
    virtual HRESULT             IaSetPrimitiveTopology(unsigned int topology) = 0;
    virtual HRESULT             IaGetPrimitiveTopology(AmdDxExtPrimitiveTopology* pExtTopology) = 0;
    virtual HRESULT             SetSingleSampleRead(ID3D10Resource* pResource,
                                                    BOOL singleSample) = 0;
    virtual HRESULT             SetSingleSampleRead11(ID3D11Resource* pResource,
                                                      BOOL singleSample) = 0;

    // Supported in version 9.0 and above.
    virtual HRESULT QueryFeatureSupport(unsigned int featureToken,
                                        void* pData, unsigned int dataSize) = 0;

protected:
    IAmdDxExt() {};
    virtual ~IAmdDxExt() = 0 {};
};

#endif // _AMDDXEXTAPI_H_
