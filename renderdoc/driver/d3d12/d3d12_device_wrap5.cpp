/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Baldur Karlsson
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

#include "d3d12_device.h"
#include "d3d12_resources.h"

HRESULT WrappedID3D12Device::CreateLifetimeTracker(_In_ ID3D12LifetimeOwner *pOwner, REFIID riid,
                                                   _COM_Outptr_ void **ppvTracker)
{
  // without a spec it's really unclear how this is used.
  return E_NOINTERFACE;
}

void WrappedID3D12Device::RemoveDevice()
{
  return m_pDevice5->RemoveDevice();
}

HRESULT WrappedID3D12Device::EnumerateMetaCommands(_Inout_ UINT *pNumMetaCommands,
                                                   _Out_writes_opt_(*pNumMetaCommands)
                                                       D3D12_META_COMMAND_DESC *pDescs)
{
  // we pretend there are no meta commands, as we do not support capturing/replaying them
  UINT numCommands = 0;
  m_pDevice5->EnumerateMetaCommands(&numCommands, NULL);

  RDCLOG("Suppressing the report of %u meta commands", numCommands);

  if(pDescs)
    memset(pDescs, 0, sizeof(D3D12_META_COMMAND_DESC) * (*pNumMetaCommands));
  if(pNumMetaCommands)
    pNumMetaCommands = 0;

  return S_OK;
}

HRESULT WrappedID3D12Device::EnumerateMetaCommandParameters(
    _In_ REFGUID CommandId, _In_ D3D12_META_COMMAND_PARAMETER_STAGE Stage,
    _Out_opt_ UINT *pTotalStructureSizeInBytes, _Inout_ UINT *pParameterCount,
    _Out_writes_opt_(*pParameterCount) D3D12_META_COMMAND_PARAMETER_DESC *pParameterDescs)
{
  RDCERR("EnumerateMetaCommandParameters called but no meta commands reported!");
  return E_INVALIDARG;
}

HRESULT WrappedID3D12Device::CreateMetaCommand(_In_ REFGUID CommandId, _In_ UINT NodeMask,
                                               _In_reads_bytes_opt_(CreationParametersDataSizeInBytes)
                                                   const void *pCreationParametersData,
                                               _In_ SIZE_T CreationParametersDataSizeInBytes,
                                               REFIID riid, _COM_Outptr_ void **ppMetaCommand)
{
  RDCERR("CreateMetaCommand called but no meta commands reported!");
  return E_INVALIDARG;
}

HRESULT WrappedID3D12Device::CreateStateObject(const D3D12_STATE_OBJECT_DESC *pDesc, REFIID riid,
                                               _COM_Outptr_ void **ppStateObject)
{
  RDCERR("CreateStateObject called but raytracing is not supported!");
  return E_INVALIDARG;
}

void WrappedID3D12Device::GetRaytracingAccelerationStructurePrebuildInfo(
    _In_ const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS *pDesc,
    _Out_ D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO *pInfo)
{
  RDCERR("GetRaytracingAccelerationStructurePrebuildInfo called raytracing is not supported!");
  if(pInfo)
    *pInfo = {0, 0, 0};
  return;
}

D3D12_DRIVER_MATCHING_IDENTIFIER_STATUS WrappedID3D12Device::CheckDriverMatchingIdentifier(
    _In_ D3D12_SERIALIZED_DATA_TYPE SerializedDataType,
    _In_ const D3D12_SERIALIZED_DATA_DRIVER_MATCHING_IDENTIFIER *pIdentifierToCheck)
{
  // always say the data is unrecognised
  return D3D12_DRIVER_MATCHING_IDENTIFIER_UNRECOGNIZED;
}