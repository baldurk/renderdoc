/************************************************************************************

Filename    :   VrApi_Ext.h
Content     :   VrApi extensions support
Created     :   February 3, 2016
Authors     :   Cass Everitt

Copyright   :   Copyright 2016 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#ifndef OVR_VrApi_Ext_h
#define OVR_VrApi_Ext_h

#include "VrApi_Types.h"
#include "string.h"				// for memset()

//-----------------------------------------------------------------
// Basic Ext Types
//-----------------------------------------------------------------

// This type is just to make parm chain traversal simple
typedef struct ovrFrameParmsExtBase
{
	ovrStructureType Type;
	OVR_VRAPI_PADDING_64_BIT( 4 );
	ovrFrameParmsExtBase * Next;
} ovrFrameParmsExtBase;


/* ovrStructureType allocations */

static inline ovrFrameParms * vrapi_GetFrameParms( ovrFrameParmsExtBase * frameParmsChain )
{
	while ( frameParmsChain != NULL && frameParmsChain->Type != VRAPI_STRUCTURE_TYPE_FRAME_PARMS )
	{
		frameParmsChain = frameParmsChain->Next;
	}

	return 	(ovrFrameParms *)frameParmsChain;
}

static inline const ovrFrameParms * vrapi_GetFrameParmsConst( const ovrFrameParmsExtBase * frameParmsChain )
{
	while ( frameParmsChain != NULL && frameParmsChain->Type != VRAPI_STRUCTURE_TYPE_FRAME_PARMS )
	{
		frameParmsChain = frameParmsChain->Next;
	}

	return 	(const ovrFrameParms *)frameParmsChain;
}


#endif // OVR_VrApi_Ext_h
