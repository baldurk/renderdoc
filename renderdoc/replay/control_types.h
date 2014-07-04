/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2014 Crytek
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

#pragma once

struct OutputConfig
{
	OutputType m_Type;
};

struct MeshDisplay
{
	MeshDataStage type;

	bool32 arcballCamera;
	FloatVector cameraPos;
	FloatVector cameraRot;

	bool32 ortho;
	float fov, aspect, nearPlane, farPlane;

	bool32 thisDrawOnly;

	bool32 showVerts;
	FloatVector hilightVerts[3];

	FloatVector prevMeshColour;
	FloatVector currentMeshColour;

	SolidShadeMode solidShadeMode;
	bool32 wireframeDraw;
};

struct TextureDisplay
{
	ResourceId texid;
	float rangemin;
	float rangemax;
	float scale;
	bool32 Red, Green, Blue, Alpha;
	float HDRMul;
	bool32 linearDisplayAsGamma;
	ResourceId CustomShader;
	uint32_t mip;
	uint32_t sliceFace;
	bool32 rawoutput;

	float offx, offy;
	
	FloatVector lightBackgroundColour;
	FloatVector darkBackgroundColour;

	TextureDisplayOverlay overlay;
};

struct RemoteMessage
{
	RemoteMessage() {}

	RemoteMessageType Type;

	struct NewCaptureData
	{
		uint32_t ID;
		uint64_t timestamp;
		rdctype::array<byte> thumbnail;
		rdctype::wstr localpath;
	} NewCapture;

	struct RegisterAPIData
	{
		rdctype::wstr APIName;
	} RegisterAPI;

	struct BusyData
	{
		rdctype::wstr ClientName;
	} Busy;
};
