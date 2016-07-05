

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.00.0613 */
/* @@MIDL_FILE_HEADING(  ) */



/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 500
#endif

/* verify that the <rpcsal.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCSAL_H_VERSION__
#define __REQUIRED_RPCSAL_H_VERSION__ 100
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif /* __RPCNDR_H_VERSION__ */

#ifndef COM_NO_WINDOWS_H
#include "windows.h"
#include "ole2.h"
#endif /*COM_NO_WINDOWS_H*/

#ifndef __dxgi1_5_h__
#define __dxgi1_5_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */ 

#ifndef __IDXGIOutput5_FWD_DEFINED__
#define __IDXGIOutput5_FWD_DEFINED__
typedef interface IDXGIOutput5 IDXGIOutput5;

#endif 	/* __IDXGIOutput5_FWD_DEFINED__ */


/* header files for imported files */
#include "dxgi1_4.h"

#ifdef __cplusplus
extern "C"{
#endif 


/* interface __MIDL_itf_dxgi1_5_0000_0000 */
/* [local] */ 

#include "winapifamily.h"
#pragma region App Family
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP)


extern RPC_IF_HANDLE __MIDL_itf_dxgi1_5_0000_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_dxgi1_5_0000_0000_v0_0_s_ifspec;

#ifndef __IDXGIOutput5_INTERFACE_DEFINED__
#define __IDXGIOutput5_INTERFACE_DEFINED__

/* interface IDXGIOutput5 */
/* [unique][local][uuid][object] */ 


EXTERN_C const IID IID_IDXGIOutput5;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("80A07424-AB52-42EB-833C-0C42FD282D98")
    IDXGIOutput5 : public IDXGIOutput4
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE DuplicateOutput1( 
            /* [annotation][in] */ 
            _In_  IUnknown *pDevice,
            /* [in] */ UINT Flags,
            /* [annotation][in] */ 
            _In_  UINT SupportedFormatsCount,
            /* [annotation][in] */ 
            _In_reads_(SupportedFormatsCount)  const DXGI_FORMAT *pSupportedFormats,
            /* [annotation][out] */ 
            _COM_Outptr_  IDXGIOutputDuplication **ppOutputDuplication) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IDXGIOutput5Vtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IDXGIOutput5 * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IDXGIOutput5 * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IDXGIOutput5 * This);
        
        HRESULT ( STDMETHODCALLTYPE *SetPrivateData )( 
            IDXGIOutput5 * This,
            /* [annotation][in] */ 
            _In_  REFGUID Name,
            /* [in] */ UINT DataSize,
            /* [annotation][in] */ 
            _In_reads_bytes_(DataSize)  const void *pData);
        
        HRESULT ( STDMETHODCALLTYPE *SetPrivateDataInterface )( 
            IDXGIOutput5 * This,
            /* [annotation][in] */ 
            _In_  REFGUID Name,
            /* [annotation][in] */ 
            _In_  const IUnknown *pUnknown);
        
        HRESULT ( STDMETHODCALLTYPE *GetPrivateData )( 
            IDXGIOutput5 * This,
            /* [annotation][in] */ 
            _In_  REFGUID Name,
            /* [annotation][out][in] */ 
            _Inout_  UINT *pDataSize,
            /* [annotation][out] */ 
            _Out_writes_bytes_(*pDataSize)  void *pData);
        
        HRESULT ( STDMETHODCALLTYPE *GetParent )( 
            IDXGIOutput5 * This,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][retval][out] */ 
            _COM_Outptr_  void **ppParent);
        
        HRESULT ( STDMETHODCALLTYPE *GetDesc )( 
            IDXGIOutput5 * This,
            /* [annotation][out] */ 
            _Out_  DXGI_OUTPUT_DESC *pDesc);
        
        HRESULT ( STDMETHODCALLTYPE *GetDisplayModeList )( 
            IDXGIOutput5 * This,
            /* [in] */ DXGI_FORMAT EnumFormat,
            /* [in] */ UINT Flags,
            /* [annotation][out][in] */ 
            _Inout_  UINT *pNumModes,
            /* [annotation][out] */ 
            _Out_writes_to_opt_(*pNumModes,*pNumModes)  DXGI_MODE_DESC *pDesc);
        
        HRESULT ( STDMETHODCALLTYPE *FindClosestMatchingMode )( 
            IDXGIOutput5 * This,
            /* [annotation][in] */ 
            _In_  const DXGI_MODE_DESC *pModeToMatch,
            /* [annotation][out] */ 
            _Out_  DXGI_MODE_DESC *pClosestMatch,
            /* [annotation][in] */ 
            _In_opt_  IUnknown *pConcernedDevice);
        
        HRESULT ( STDMETHODCALLTYPE *WaitForVBlank )( 
            IDXGIOutput5 * This);
        
        HRESULT ( STDMETHODCALLTYPE *TakeOwnership )( 
            IDXGIOutput5 * This,
            /* [annotation][in] */ 
            _In_  IUnknown *pDevice,
            BOOL Exclusive);
        
        void ( STDMETHODCALLTYPE *ReleaseOwnership )( 
            IDXGIOutput5 * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetGammaControlCapabilities )( 
            IDXGIOutput5 * This,
            /* [annotation][out] */ 
            _Out_  DXGI_GAMMA_CONTROL_CAPABILITIES *pGammaCaps);
        
        HRESULT ( STDMETHODCALLTYPE *SetGammaControl )( 
            IDXGIOutput5 * This,
            /* [annotation][in] */ 
            _In_  const DXGI_GAMMA_CONTROL *pArray);
        
        HRESULT ( STDMETHODCALLTYPE *GetGammaControl )( 
            IDXGIOutput5 * This,
            /* [annotation][out] */ 
            _Out_  DXGI_GAMMA_CONTROL *pArray);
        
        HRESULT ( STDMETHODCALLTYPE *SetDisplaySurface )( 
            IDXGIOutput5 * This,
            /* [annotation][in] */ 
            _In_  IDXGISurface *pScanoutSurface);
        
        HRESULT ( STDMETHODCALLTYPE *GetDisplaySurfaceData )( 
            IDXGIOutput5 * This,
            /* [annotation][in] */ 
            _In_  IDXGISurface *pDestination);
        
        HRESULT ( STDMETHODCALLTYPE *GetFrameStatistics )( 
            IDXGIOutput5 * This,
            /* [annotation][out] */ 
            _Out_  DXGI_FRAME_STATISTICS *pStats);
        
        HRESULT ( STDMETHODCALLTYPE *GetDisplayModeList1 )( 
            IDXGIOutput5 * This,
            /* [in] */ DXGI_FORMAT EnumFormat,
            /* [in] */ UINT Flags,
            /* [annotation][out][in] */ 
            _Inout_  UINT *pNumModes,
            /* [annotation][out] */ 
            _Out_writes_to_opt_(*pNumModes,*pNumModes)  DXGI_MODE_DESC1 *pDesc);
        
        HRESULT ( STDMETHODCALLTYPE *FindClosestMatchingMode1 )( 
            IDXGIOutput5 * This,
            /* [annotation][in] */ 
            _In_  const DXGI_MODE_DESC1 *pModeToMatch,
            /* [annotation][out] */ 
            _Out_  DXGI_MODE_DESC1 *pClosestMatch,
            /* [annotation][in] */ 
            _In_opt_  IUnknown *pConcernedDevice);
        
        HRESULT ( STDMETHODCALLTYPE *GetDisplaySurfaceData1 )( 
            IDXGIOutput5 * This,
            /* [annotation][in] */ 
            _In_  IDXGIResource *pDestination);
        
        HRESULT ( STDMETHODCALLTYPE *DuplicateOutput )( 
            IDXGIOutput5 * This,
            /* [annotation][in] */ 
            _In_  IUnknown *pDevice,
            /* [annotation][out] */ 
            _COM_Outptr_  IDXGIOutputDuplication **ppOutputDuplication);
        
        BOOL ( STDMETHODCALLTYPE *SupportsOverlays )( 
            IDXGIOutput5 * This);
        
        HRESULT ( STDMETHODCALLTYPE *CheckOverlaySupport )( 
            IDXGIOutput5 * This,
            /* [annotation][in] */ 
            _In_  DXGI_FORMAT EnumFormat,
            /* [annotation][out] */ 
            _In_  IUnknown *pConcernedDevice,
            /* [annotation][out] */ 
            _Out_  UINT *pFlags);
        
        HRESULT ( STDMETHODCALLTYPE *CheckOverlayColorSpaceSupport )( 
            IDXGIOutput5 * This,
            /* [annotation][in] */ 
            _In_  DXGI_FORMAT Format,
            /* [annotation][in] */ 
            _In_  DXGI_COLOR_SPACE_TYPE ColorSpace,
            /* [annotation][in] */ 
            _In_  IUnknown *pConcernedDevice,
            /* [annotation][out] */ 
            _Out_  UINT *pFlags);
        
        HRESULT ( STDMETHODCALLTYPE *DuplicateOutput1 )( 
            IDXGIOutput5 * This,
            /* [annotation][in] */ 
            _In_  IUnknown *pDevice,
            /* [in] */ UINT Flags,
            /* [annotation][in] */ 
            _In_  UINT SupportedFormatsCount,
            /* [annotation][in] */ 
            _In_reads_(SupportedFormatsCount)  const DXGI_FORMAT *pSupportedFormats,
            /* [annotation][out] */ 
            _COM_Outptr_  IDXGIOutputDuplication **ppOutputDuplication);
        
        END_INTERFACE
    } IDXGIOutput5Vtbl;

    interface IDXGIOutput5
    {
        CONST_VTBL struct IDXGIOutput5Vtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IDXGIOutput5_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IDXGIOutput5_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IDXGIOutput5_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IDXGIOutput5_SetPrivateData(This,Name,DataSize,pData)	\
    ( (This)->lpVtbl -> SetPrivateData(This,Name,DataSize,pData) ) 

#define IDXGIOutput5_SetPrivateDataInterface(This,Name,pUnknown)	\
    ( (This)->lpVtbl -> SetPrivateDataInterface(This,Name,pUnknown) ) 

#define IDXGIOutput5_GetPrivateData(This,Name,pDataSize,pData)	\
    ( (This)->lpVtbl -> GetPrivateData(This,Name,pDataSize,pData) ) 

#define IDXGIOutput5_GetParent(This,riid,ppParent)	\
    ( (This)->lpVtbl -> GetParent(This,riid,ppParent) ) 


#define IDXGIOutput5_GetDesc(This,pDesc)	\
    ( (This)->lpVtbl -> GetDesc(This,pDesc) ) 

#define IDXGIOutput5_GetDisplayModeList(This,EnumFormat,Flags,pNumModes,pDesc)	\
    ( (This)->lpVtbl -> GetDisplayModeList(This,EnumFormat,Flags,pNumModes,pDesc) ) 

#define IDXGIOutput5_FindClosestMatchingMode(This,pModeToMatch,pClosestMatch,pConcernedDevice)	\
    ( (This)->lpVtbl -> FindClosestMatchingMode(This,pModeToMatch,pClosestMatch,pConcernedDevice) ) 

#define IDXGIOutput5_WaitForVBlank(This)	\
    ( (This)->lpVtbl -> WaitForVBlank(This) ) 

#define IDXGIOutput5_TakeOwnership(This,pDevice,Exclusive)	\
    ( (This)->lpVtbl -> TakeOwnership(This,pDevice,Exclusive) ) 

#define IDXGIOutput5_ReleaseOwnership(This)	\
    ( (This)->lpVtbl -> ReleaseOwnership(This) ) 

#define IDXGIOutput5_GetGammaControlCapabilities(This,pGammaCaps)	\
    ( (This)->lpVtbl -> GetGammaControlCapabilities(This,pGammaCaps) ) 

#define IDXGIOutput5_SetGammaControl(This,pArray)	\
    ( (This)->lpVtbl -> SetGammaControl(This,pArray) ) 

#define IDXGIOutput5_GetGammaControl(This,pArray)	\
    ( (This)->lpVtbl -> GetGammaControl(This,pArray) ) 

#define IDXGIOutput5_SetDisplaySurface(This,pScanoutSurface)	\
    ( (This)->lpVtbl -> SetDisplaySurface(This,pScanoutSurface) ) 

#define IDXGIOutput5_GetDisplaySurfaceData(This,pDestination)	\
    ( (This)->lpVtbl -> GetDisplaySurfaceData(This,pDestination) ) 

#define IDXGIOutput5_GetFrameStatistics(This,pStats)	\
    ( (This)->lpVtbl -> GetFrameStatistics(This,pStats) ) 


#define IDXGIOutput5_GetDisplayModeList1(This,EnumFormat,Flags,pNumModes,pDesc)	\
    ( (This)->lpVtbl -> GetDisplayModeList1(This,EnumFormat,Flags,pNumModes,pDesc) ) 

#define IDXGIOutput5_FindClosestMatchingMode1(This,pModeToMatch,pClosestMatch,pConcernedDevice)	\
    ( (This)->lpVtbl -> FindClosestMatchingMode1(This,pModeToMatch,pClosestMatch,pConcernedDevice) ) 

#define IDXGIOutput5_GetDisplaySurfaceData1(This,pDestination)	\
    ( (This)->lpVtbl -> GetDisplaySurfaceData1(This,pDestination) ) 

#define IDXGIOutput5_DuplicateOutput(This,pDevice,ppOutputDuplication)	\
    ( (This)->lpVtbl -> DuplicateOutput(This,pDevice,ppOutputDuplication) ) 


#define IDXGIOutput5_SupportsOverlays(This)	\
    ( (This)->lpVtbl -> SupportsOverlays(This) ) 


#define IDXGIOutput5_CheckOverlaySupport(This,EnumFormat,pConcernedDevice,pFlags)	\
    ( (This)->lpVtbl -> CheckOverlaySupport(This,EnumFormat,pConcernedDevice,pFlags) ) 


#define IDXGIOutput5_CheckOverlayColorSpaceSupport(This,Format,ColorSpace,pConcernedDevice,pFlags)	\
    ( (This)->lpVtbl -> CheckOverlayColorSpaceSupport(This,Format,ColorSpace,pConcernedDevice,pFlags) ) 


#define IDXGIOutput5_DuplicateOutput1(This,pDevice,Flags,SupportedFormatsCount,pSupportedFormats,ppOutputDuplication)	\
    ( (This)->lpVtbl -> DuplicateOutput1(This,pDevice,Flags,SupportedFormatsCount,pSupportedFormats,ppOutputDuplication) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IDXGIOutput5_INTERFACE_DEFINED__ */


/* interface __MIDL_itf_dxgi1_5_0000_0001 */
/* [local] */ 

#endif /* WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP) */
#pragma endregion
DEFINE_GUID(IID_IDXGIOutput5,0x80A07424,0xAB52,0x42EB,0x83,0x3C,0x0C,0x42,0xFD,0x28,0x2D,0x98);


extern RPC_IF_HANDLE __MIDL_itf_dxgi1_5_0000_0001_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_dxgi1_5_0000_0001_v0_0_s_ifspec;

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


