/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2023 Baldur Karlsson
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

template <>
rdcstr DoStringise(const SDBasic &el)
{
  BEGIN_ENUM_STRINGISE(SDBasic);
  {
    STRINGISE_ENUM_CLASS(Chunk);
    STRINGISE_ENUM_CLASS(Struct);
    STRINGISE_ENUM_CLASS(Array);
    STRINGISE_ENUM_CLASS(Null);
    STRINGISE_ENUM_CLASS(Buffer);
    STRINGISE_ENUM_CLASS(String);
    STRINGISE_ENUM_CLASS(Enum);
    STRINGISE_ENUM_CLASS(UnsignedInteger);
    STRINGISE_ENUM_CLASS(SignedInteger);
    STRINGISE_ENUM_CLASS(Float);
    STRINGISE_ENUM_CLASS(Boolean);
    STRINGISE_ENUM_CLASS(Character);
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const ResultCode &el)
{
  BEGIN_ENUM_STRINGISE(ResultCode)
  {
    STRINGISE_ENUM_CLASS_NAMED(Succeeded, "Success");
    STRINGISE_ENUM_CLASS_NAMED(UnknownError, "Unknown error");
    STRINGISE_ENUM_CLASS_NAMED(InternalError, "Internal error");
    STRINGISE_ENUM_CLASS_NAMED(FileNotFound, "File not found");
    STRINGISE_ENUM_CLASS_NAMED(InjectionFailed, "RenderDoc injection failed");
    STRINGISE_ENUM_CLASS_NAMED(IncompatibleProcess,
                               "Process is incompatible with this build of RenderDoc");
    STRINGISE_ENUM_CLASS_NAMED(NetworkIOFailed, "Network I/O operation failed");
    STRINGISE_ENUM_CLASS_NAMED(NetworkRemoteBusy, "Remote side of network connection is busy");
    STRINGISE_ENUM_CLASS_NAMED(NetworkVersionMismatch, "Incompatible version");
    STRINGISE_ENUM_CLASS_NAMED(FileIOFailed, "File I/O failed");
    STRINGISE_ENUM_CLASS_NAMED(
        FileIncompatibleVersion,
        "Capture file incompatible due to being made on an different major version of RenderDoc");
    STRINGISE_ENUM_CLASS_NAMED(FileCorrupted, "File is corrupted");
    STRINGISE_ENUM_CLASS_NAMED(
        ImageUnsupported, "The image file or format is unrecognised or not supported in this form");
    STRINGISE_ENUM_CLASS_NAMED(APIUnsupported, "API used in this capture is unsupported");
    STRINGISE_ENUM_CLASS_NAMED(APIInitFailed,
                               "API initialisation failed while loading the capture");
    STRINGISE_ENUM_CLASS_NAMED(
        APIIncompatibleVersion,
        "Captured API data was made on a newer incompatible version of RenderDoc");
    STRINGISE_ENUM_CLASS_NAMED(
        APIHardwareUnsupported,
        "Current replaying hardware unsupported or incompatible with captured hardware");
    STRINGISE_ENUM_CLASS_NAMED(APIDataCorrupted,
                               "Replaying the capture encountered invalid/corrupted data");
    STRINGISE_ENUM_CLASS_NAMED(APIReplayFailed, "Replaying the capture failed at the API level");
    STRINGISE_ENUM_CLASS_NAMED(JDWPFailure, "JDWP debugger connection could not be established");
    STRINGISE_ENUM_CLASS_NAMED(
        AndroidGrantPermissionsFailed,
        "Failed to grant runtime permissions when installing Android remote server");
    STRINGISE_ENUM_CLASS_NAMED(
        AndroidABINotFound,
        "Couldn't determine supported ABIs when installing Android remote server");
    STRINGISE_ENUM_CLASS_NAMED(
        AndroidAPKFolderNotFound,
        "Couldn't find the folder which contains the Android remote server APK");
    STRINGISE_ENUM_CLASS_NAMED(AndroidAPKInstallFailed,
                               "Failed to install Android remote server for unknown reasons");
    STRINGISE_ENUM_CLASS_NAMED(AndroidAPKVerifyFailed,
                               "Failed to verify installed Android remote server");
    STRINGISE_ENUM_CLASS_NAMED(RemoteServerConnectionLost, "Connection lost to remote server");
    STRINGISE_ENUM_CLASS_NAMED(ReplayOutOfMemory, "Encountered an out of memory error");
    STRINGISE_ENUM_CLASS_NAMED(ReplayDeviceLost, "Encountered a GPU device lost error");
    STRINGISE_ENUM_CLASS_NAMED(DataNotAvailable,
                               "Data was requested through RenderDoc's API which is not available");
    STRINGISE_ENUM_CLASS_NAMED(InvalidParameter,
                               "An invalid parameter was passed to RenderDoc's API");
    STRINGISE_ENUM_CLASS_NAMED(CompressionFailed, "Compression or decompression failed");
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const WindowingSystem &el)
{
  BEGIN_ENUM_STRINGISE(WindowingSystem)
  {
    STRINGISE_ENUM_CLASS(Unknown);
    STRINGISE_ENUM_CLASS(Headless);
    STRINGISE_ENUM_CLASS(Win32);
    STRINGISE_ENUM_CLASS(Xlib);
    STRINGISE_ENUM_CLASS(XCB);
    STRINGISE_ENUM_CLASS(Android);
    STRINGISE_ENUM_CLASS(MacOS);
    STRINGISE_ENUM_CLASS(GGP);
    STRINGISE_ENUM_CLASS(Wayland);
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const ResourceFormatType &el)
{
  BEGIN_ENUM_STRINGISE(ResourceFormatType)
  {
    STRINGISE_ENUM_CLASS(Regular);
    STRINGISE_ENUM_CLASS(Undefined);
    STRINGISE_ENUM_CLASS(BC1);
    STRINGISE_ENUM_CLASS(BC2);
    STRINGISE_ENUM_CLASS(BC3);
    STRINGISE_ENUM_CLASS(BC4);
    STRINGISE_ENUM_CLASS(BC5);
    STRINGISE_ENUM_CLASS(BC6);
    STRINGISE_ENUM_CLASS(BC7);
    STRINGISE_ENUM_CLASS(ETC2);
    STRINGISE_ENUM_CLASS(EAC);
    STRINGISE_ENUM_CLASS(ASTC);
    STRINGISE_ENUM_CLASS(R10G10B10A2);
    STRINGISE_ENUM_CLASS(R11G11B10);
    STRINGISE_ENUM_CLASS(R5G6B5);
    STRINGISE_ENUM_CLASS(R5G5B5A1);
    STRINGISE_ENUM_CLASS(R9G9B9E5);
    STRINGISE_ENUM_CLASS(R4G4B4A4);
    STRINGISE_ENUM_CLASS(R4G4);
    STRINGISE_ENUM_CLASS(D16S8);
    STRINGISE_ENUM_CLASS(D24S8);
    STRINGISE_ENUM_CLASS(D32S8);
    STRINGISE_ENUM_CLASS(S8);
    STRINGISE_ENUM_CLASS(YUV8);
    STRINGISE_ENUM_CLASS(YUV10);
    STRINGISE_ENUM_CLASS(YUV12);
    STRINGISE_ENUM_CLASS(YUV16);
    STRINGISE_ENUM_CLASS(PVRTC);
    STRINGISE_ENUM_CLASS(A8);
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const CompType &el)
{
  BEGIN_ENUM_STRINGISE(CompType)
  {
    STRINGISE_ENUM_CLASS(Typeless);
    STRINGISE_ENUM_CLASS(Float);
    STRINGISE_ENUM_CLASS(UNorm);
    STRINGISE_ENUM_CLASS(SNorm);
    STRINGISE_ENUM_CLASS(UInt);
    STRINGISE_ENUM_CLASS(SInt);
    STRINGISE_ENUM_CLASS(UScaled);
    STRINGISE_ENUM_CLASS(SScaled);
    STRINGISE_ENUM_CLASS_NAMED(Depth, "Depth/Stencil");
    STRINGISE_ENUM_CLASS_NAMED(UNormSRGB, "sRGB");
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const FileType &el)
{
  BEGIN_ENUM_STRINGISE(FileType)
  {
    STRINGISE_ENUM_CLASS(DDS);
    STRINGISE_ENUM_CLASS(PNG);
    STRINGISE_ENUM_CLASS(JPG);
    STRINGISE_ENUM_CLASS(BMP);
    STRINGISE_ENUM_CLASS(TGA);
    STRINGISE_ENUM_CLASS(HDR);
    STRINGISE_ENUM_CLASS(EXR);
    STRINGISE_ENUM_CLASS(Raw);
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const AlphaMapping &el)
{
  BEGIN_ENUM_STRINGISE(AlphaMapping)
  {
    STRINGISE_ENUM_CLASS_NAMED(Discard, "Discard");
    STRINGISE_ENUM_CLASS_NAMED(BlendToColor, "Blend to Color");
    STRINGISE_ENUM_CLASS_NAMED(BlendToCheckerboard, "Blend to Checkerboard");
    STRINGISE_ENUM_CLASS_NAMED(Preserve, "Preserve");
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const EnvMod &el)
{
  BEGIN_ENUM_STRINGISE(EnvMod)
  {
    STRINGISE_ENUM_CLASS(Set);
    STRINGISE_ENUM_CLASS(Append);
    STRINGISE_ENUM_CLASS(Prepend);
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const EnvSep &el)
{
  BEGIN_ENUM_STRINGISE(EnvSep)
  {
    STRINGISE_ENUM_CLASS_NAMED(Platform, "Platform style");
    STRINGISE_ENUM_CLASS_NAMED(SemiColon, "Semi-colon (;)");
    STRINGISE_ENUM_CLASS_NAMED(Colon, "Colon (:)");
    STRINGISE_ENUM_CLASS_NAMED(NoSep, "No Separator");
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const LogType &el)
{
  BEGIN_ENUM_STRINGISE(LogType)
  {
    STRINGISE_ENUM_CLASS(Debug);
    STRINGISE_ENUM_CLASS_NAMED(Comment, "Log");
    STRINGISE_ENUM_CLASS(Warning);
    STRINGISE_ENUM_CLASS(Error);
    STRINGISE_ENUM_CLASS(Fatal);
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const Topology &el)
{
  BEGIN_ENUM_STRINGISE(Topology)
  {
    STRINGISE_ENUM_CLASS_NAMED(Unknown, "Unknown");
    STRINGISE_ENUM_CLASS_NAMED(PointList, "Point List");
    STRINGISE_ENUM_CLASS_NAMED(LineList, "Line List");
    STRINGISE_ENUM_CLASS_NAMED(LineStrip, "Line Strip");
    STRINGISE_ENUM_CLASS_NAMED(LineLoop, "Line Loop");
    STRINGISE_ENUM_CLASS_NAMED(TriangleList, "Triangle List");
    STRINGISE_ENUM_CLASS_NAMED(TriangleStrip, "Triangle Strip");
    STRINGISE_ENUM_CLASS_NAMED(TriangleFan, "Triangle Fan");
    STRINGISE_ENUM_CLASS_NAMED(LineList_Adj, "Line List with Adjacency");
    STRINGISE_ENUM_CLASS_NAMED(LineStrip_Adj, "Line Strip with Adjacency");
    STRINGISE_ENUM_CLASS_NAMED(TriangleList_Adj, "Triangle List with Adjacency");
    STRINGISE_ENUM_CLASS_NAMED(TriangleStrip_Adj, "Triangle Strip with Adjacency");
    STRINGISE_ENUM_CLASS_NAMED(PatchList_1CPs, "Patch List 1 CP");
    STRINGISE_ENUM_CLASS_NAMED(PatchList_2CPs, "Patch List 2 CPs");
    STRINGISE_ENUM_CLASS_NAMED(PatchList_3CPs, "Patch List 3 CPs");
    STRINGISE_ENUM_CLASS_NAMED(PatchList_4CPs, "Patch List 4 CPs");
    STRINGISE_ENUM_CLASS_NAMED(PatchList_5CPs, "Patch List 5 CPs");
    STRINGISE_ENUM_CLASS_NAMED(PatchList_6CPs, "Patch List 6 CPs");
    STRINGISE_ENUM_CLASS_NAMED(PatchList_7CPs, "Patch List 7 CPs");
    STRINGISE_ENUM_CLASS_NAMED(PatchList_8CPs, "Patch List 8 CPs");
    STRINGISE_ENUM_CLASS_NAMED(PatchList_9CPs, "Patch List 9 CPs");
    STRINGISE_ENUM_CLASS_NAMED(PatchList_10CPs, "Patch List 10 CPs");
    STRINGISE_ENUM_CLASS_NAMED(PatchList_11CPs, "Patch List 11 CPs");
    STRINGISE_ENUM_CLASS_NAMED(PatchList_12CPs, "Patch List 12 CPs");
    STRINGISE_ENUM_CLASS_NAMED(PatchList_13CPs, "Patch List 13 CPs");
    STRINGISE_ENUM_CLASS_NAMED(PatchList_14CPs, "Patch List 14 CPs");
    STRINGISE_ENUM_CLASS_NAMED(PatchList_15CPs, "Patch List 15 CPs");
    STRINGISE_ENUM_CLASS_NAMED(PatchList_16CPs, "Patch List 16 CPs");
    STRINGISE_ENUM_CLASS_NAMED(PatchList_17CPs, "Patch List 17 CPs");
    STRINGISE_ENUM_CLASS_NAMED(PatchList_18CPs, "Patch List 18 CPs");
    STRINGISE_ENUM_CLASS_NAMED(PatchList_19CPs, "Patch List 19 CPs");
    STRINGISE_ENUM_CLASS_NAMED(PatchList_20CPs, "Patch List 20 CPs");
    STRINGISE_ENUM_CLASS_NAMED(PatchList_21CPs, "Patch List 21 CPs");
    STRINGISE_ENUM_CLASS_NAMED(PatchList_22CPs, "Patch List 22 CPs");
    STRINGISE_ENUM_CLASS_NAMED(PatchList_23CPs, "Patch List 23 CPs");
    STRINGISE_ENUM_CLASS_NAMED(PatchList_24CPs, "Patch List 24 CPs");
    STRINGISE_ENUM_CLASS_NAMED(PatchList_25CPs, "Patch List 25 CPs");
    STRINGISE_ENUM_CLASS_NAMED(PatchList_26CPs, "Patch List 26 CPs");
    STRINGISE_ENUM_CLASS_NAMED(PatchList_27CPs, "Patch List 27 CPs");
    STRINGISE_ENUM_CLASS_NAMED(PatchList_28CPs, "Patch List 28 CPs");
    STRINGISE_ENUM_CLASS_NAMED(PatchList_29CPs, "Patch List 29 CPs");
    STRINGISE_ENUM_CLASS_NAMED(PatchList_30CPs, "Patch List 30 CPs");
    STRINGISE_ENUM_CLASS_NAMED(PatchList_31CPs, "Patch List 31 CPs");
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const FillMode &el)
{
  BEGIN_ENUM_STRINGISE(FillMode)
  {
    STRINGISE_ENUM_CLASS(Solid);
    STRINGISE_ENUM_CLASS(Wireframe);
    STRINGISE_ENUM_CLASS(Point);
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const CullMode &el)
{
  BEGIN_ENUM_STRINGISE(CullMode)
  {
    STRINGISE_ENUM_CLASS_NAMED(NoCull, "None");
    STRINGISE_ENUM_CLASS_NAMED(Front, "Front");
    STRINGISE_ENUM_CLASS_NAMED(Back, "Back");
    STRINGISE_ENUM_CLASS_NAMED(FrontAndBack, "Front & Back");
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const ConservativeRaster &el)
{
  BEGIN_ENUM_STRINGISE(ConservativeRaster)
  {
    STRINGISE_ENUM_CLASS(Disabled);
    STRINGISE_ENUM_CLASS(Underestimate);
    STRINGISE_ENUM_CLASS(Overestimate);
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const ShadingRateCombiner &el)
{
  BEGIN_ENUM_STRINGISE(ShadingRateCombiner)
  {
    STRINGISE_ENUM_CLASS(Keep);
    STRINGISE_ENUM_CLASS(Replace);
    STRINGISE_ENUM_CLASS(Min);
    STRINGISE_ENUM_CLASS(Max);
    STRINGISE_ENUM_CLASS(Multiply);
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const LineRaster &el)
{
  BEGIN_ENUM_STRINGISE(LineRaster)
  {
    STRINGISE_ENUM_CLASS(Default);
    STRINGISE_ENUM_CLASS(Rectangular);
    STRINGISE_ENUM_CLASS(Bresenham);
    STRINGISE_ENUM_CLASS(RectangularSmooth);
    STRINGISE_ENUM_CLASS(RectangularD3D);
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const FilterMode &el)
{
  BEGIN_ENUM_STRINGISE(FilterMode)
  {
    STRINGISE_ENUM_CLASS_NAMED(NoFilter, "None");
    STRINGISE_ENUM_CLASS_NAMED(Point, "Point");
    STRINGISE_ENUM_CLASS_NAMED(Linear, "Linear");
    STRINGISE_ENUM_CLASS_NAMED(Cubic, "Cubic");
    STRINGISE_ENUM_CLASS_NAMED(Anisotropic, "Anisotropic");
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const FilterFunction &el)
{
  BEGIN_ENUM_STRINGISE(FilterFunction)
  {
    STRINGISE_ENUM_CLASS(Normal);
    STRINGISE_ENUM_CLASS(Comparison);
    STRINGISE_ENUM_CLASS(Minimum);
    STRINGISE_ENUM_CLASS(Maximum);
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const CompareFunction &el)
{
  BEGIN_ENUM_STRINGISE(CompareFunction)
  {
    STRINGISE_ENUM_CLASS_NAMED(Never, "Never");
    STRINGISE_ENUM_CLASS_NAMED(AlwaysTrue, "Always");
    STRINGISE_ENUM_CLASS_NAMED(Less, "Less");
    STRINGISE_ENUM_CLASS_NAMED(LessEqual, "Less Equal");
    STRINGISE_ENUM_CLASS_NAMED(Greater, "Greater");
    STRINGISE_ENUM_CLASS_NAMED(GreaterEqual, "Greater Equal");
    STRINGISE_ENUM_CLASS_NAMED(Equal, "Equal");
    STRINGISE_ENUM_CLASS_NAMED(NotEqual, "NotEqual");
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const BlendMultiplier &el)
{
  BEGIN_ENUM_STRINGISE(BlendMultiplier)
  {
    STRINGISE_ENUM_CLASS_NAMED(Zero, "Zero");
    STRINGISE_ENUM_CLASS_NAMED(One, "One");
    STRINGISE_ENUM_CLASS_NAMED(SrcCol, "Src Col");
    STRINGISE_ENUM_CLASS_NAMED(InvSrcCol, "1 - Src Col");
    STRINGISE_ENUM_CLASS_NAMED(DstCol, "Dst Col");
    STRINGISE_ENUM_CLASS_NAMED(InvDstCol, "1 - Dst Col");
    STRINGISE_ENUM_CLASS_NAMED(SrcAlpha, "Src Alpha");
    STRINGISE_ENUM_CLASS_NAMED(InvSrcAlpha, "1 - Src Alpha");
    STRINGISE_ENUM_CLASS_NAMED(DstAlpha, "Dst Alpha");
    STRINGISE_ENUM_CLASS_NAMED(InvDstAlpha, "1 - Dst Alpha");
    STRINGISE_ENUM_CLASS_NAMED(SrcAlphaSat, "Src Alpha Sat");
    STRINGISE_ENUM_CLASS_NAMED(FactorRGB, "Constant RGB");
    STRINGISE_ENUM_CLASS_NAMED(InvFactorRGB, "1 - Constant RGB");
    STRINGISE_ENUM_CLASS_NAMED(FactorAlpha, "Constant A");
    STRINGISE_ENUM_CLASS_NAMED(InvFactorAlpha, "1 - Constant A");
    STRINGISE_ENUM_CLASS_NAMED(Src1Col, "Src1 Col");
    STRINGISE_ENUM_CLASS_NAMED(InvSrc1Col, "1 - Src1 Col");
    STRINGISE_ENUM_CLASS_NAMED(Src1Alpha, "Src1 Alpha");
    STRINGISE_ENUM_CLASS_NAMED(InvSrc1Alpha, "1 - Src1 Alpha");
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const BlendOperation &el)
{
  BEGIN_ENUM_STRINGISE(BlendOperation)
  {
    STRINGISE_ENUM_CLASS_NAMED(Add, "Add");
    STRINGISE_ENUM_CLASS_NAMED(Subtract, "Subtract");
    STRINGISE_ENUM_CLASS_NAMED(ReversedSubtract, "Rev. Subtract");
    STRINGISE_ENUM_CLASS_NAMED(Minimum, "Minimum");
    STRINGISE_ENUM_CLASS_NAMED(Maximum, "Maximum");
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const StencilOperation &el)
{
  BEGIN_ENUM_STRINGISE(StencilOperation)
  {
    STRINGISE_ENUM_CLASS_NAMED(Keep, "Keep");
    STRINGISE_ENUM_CLASS_NAMED(Zero, "Zero");
    STRINGISE_ENUM_CLASS_NAMED(Replace, "Replace");
    STRINGISE_ENUM_CLASS_NAMED(IncSat, "Inc Sat");
    STRINGISE_ENUM_CLASS_NAMED(DecSat, "Dec Sat");
    STRINGISE_ENUM_CLASS_NAMED(IncWrap, "Inc Wrap");
    STRINGISE_ENUM_CLASS_NAMED(DecWrap, "Dec Wrap");
    STRINGISE_ENUM_CLASS_NAMED(Invert, "Invert");
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const LogicOperation &el)
{
  BEGIN_ENUM_STRINGISE(LogicOperation)
  {
    STRINGISE_ENUM_CLASS_NAMED(NoOp, "No-Op");
    STRINGISE_ENUM_CLASS_NAMED(Clear, "Clear");
    STRINGISE_ENUM_CLASS_NAMED(Set, "Set");
    STRINGISE_ENUM_CLASS_NAMED(Copy, "Copy");
    STRINGISE_ENUM_CLASS_NAMED(CopyInverted, "Copy Inverted");
    STRINGISE_ENUM_CLASS_NAMED(Invert, "Invert");
    STRINGISE_ENUM_CLASS_NAMED(And, "And");
    STRINGISE_ENUM_CLASS_NAMED(Nand, "Nand");
    STRINGISE_ENUM_CLASS_NAMED(Or, "Or");
    STRINGISE_ENUM_CLASS_NAMED(Xor, "Xor");
    STRINGISE_ENUM_CLASS_NAMED(Nor, "Nor");
    STRINGISE_ENUM_CLASS_NAMED(Equivalent, "Equivalent");
    STRINGISE_ENUM_CLASS_NAMED(AndReverse, "And Reverse");
    STRINGISE_ENUM_CLASS_NAMED(AndInverted, "And Inverted");
    STRINGISE_ENUM_CLASS_NAMED(OrReverse, "Or Reverse");
    STRINGISE_ENUM_CLASS_NAMED(OrInverted, "Or Inverted");
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const QualityHint &el)
{
  BEGIN_ENUM_STRINGISE(QualityHint)
  {
    STRINGISE_ENUM_CLASS_NAMED(DontCare, "Don't Care");
    STRINGISE_ENUM_CLASS_NAMED(Nicest, "Nicest");
    STRINGISE_ENUM_CLASS_NAMED(Fastest, "Fastest");
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const TextureFilter &el)
{
  rdcstr filter = "";
  rdcstr filtPrefix = "";
  rdcstr filtVal = "";

  rdcstr filters[] = {ToStr(el.minify), ToStr(el.magnify), ToStr(el.mip)};
  rdcstr filterPrefixes[] = {"Min", "Mag", "Mip"};

  for(int a = 0; a < 3; a++)
  {
    if(a == 0 || filters[a] == filters[a - 1])
    {
      if(filtPrefix != "")
        filtPrefix += "&";
      filtPrefix += filterPrefixes[a];
    }
    else
    {
      filter += filtPrefix + ": " + filtVal + ", ";

      filtPrefix = filterPrefixes[a];
    }
    filtVal = filters[a];
  }

  filter += filtPrefix + ": " + filtVal;

  return filter;
}

template <>
rdcstr DoStringise(const AddressMode &el)
{
  BEGIN_ENUM_STRINGISE(AddressMode);
  {
    STRINGISE_ENUM_CLASS(Wrap);
    STRINGISE_ENUM_CLASS(Mirror);
    STRINGISE_ENUM_CLASS(MirrorOnce);
    STRINGISE_ENUM_CLASS(ClampEdge);
    STRINGISE_ENUM_CLASS(ClampBorder);
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const YcbcrConversion &el)
{
  BEGIN_ENUM_STRINGISE(YcbcrConversion);
  {
    STRINGISE_ENUM_CLASS(Raw);
    STRINGISE_ENUM_CLASS_NAMED(RangeOnly, "Range Only");
    STRINGISE_ENUM_CLASS_NAMED(BT709, "BT.709");
    STRINGISE_ENUM_CLASS_NAMED(BT601, "BT.601");
    STRINGISE_ENUM_CLASS_NAMED(BT2020, "BT.2020");
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const YcbcrRange &el)
{
  BEGIN_ENUM_STRINGISE(YcbcrRange);
  {
    STRINGISE_ENUM_CLASS_NAMED(ITUFull, "Full");
    STRINGISE_ENUM_CLASS_NAMED(ITUNarrow, "Narrow");
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const ChromaSampleLocation &el)
{
  BEGIN_ENUM_STRINGISE(ChromaSampleLocation);
  {
    STRINGISE_ENUM_CLASS_NAMED(CositedEven, "Even");
    STRINGISE_ENUM_CLASS_NAMED(Midpoint, "Mid");
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const ResourceType &el)
{
  BEGIN_ENUM_STRINGISE(ResourceType)
  {
    STRINGISE_ENUM_CLASS(Unknown);
    STRINGISE_ENUM_CLASS(Device);
    STRINGISE_ENUM_CLASS(Queue);
    STRINGISE_ENUM_CLASS(CommandBuffer);
    STRINGISE_ENUM_CLASS(Texture);
    STRINGISE_ENUM_CLASS(Buffer);
    STRINGISE_ENUM_CLASS(View);
    STRINGISE_ENUM_CLASS(Sampler);
    STRINGISE_ENUM_CLASS(SwapchainImage);
    STRINGISE_ENUM_CLASS(Memory);
    STRINGISE_ENUM_CLASS(Shader);
    STRINGISE_ENUM_CLASS(ShaderBinding);
    STRINGISE_ENUM_CLASS(PipelineState);
    STRINGISE_ENUM_CLASS(StateObject);
    STRINGISE_ENUM_CLASS(RenderPass);
    STRINGISE_ENUM_CLASS(Query);
    STRINGISE_ENUM_CLASS(Sync);
    STRINGISE_ENUM_CLASS(Pool);
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const TextureType &el)
{
  BEGIN_ENUM_STRINGISE(TextureType)
  {
    STRINGISE_ENUM_CLASS_NAMED(Unknown, "Unknown");
    STRINGISE_ENUM_CLASS_NAMED(Buffer, "Buffer");
    STRINGISE_ENUM_CLASS_NAMED(Texture1D, "Texture 1D");
    STRINGISE_ENUM_CLASS_NAMED(Texture1DArray, "Texture 1D Array");
    STRINGISE_ENUM_CLASS_NAMED(Texture2D, "Texture 2D");
    STRINGISE_ENUM_CLASS_NAMED(TextureRect, "Texture Rect");
    STRINGISE_ENUM_CLASS_NAMED(Texture2DArray, "Texture 2D Array");
    STRINGISE_ENUM_CLASS_NAMED(Texture2DMS, "Texture 2D MS");
    STRINGISE_ENUM_CLASS_NAMED(Texture2DMSArray, "Texture 2D MS Array");
    STRINGISE_ENUM_CLASS_NAMED(Texture3D, "Texture 3D");
    STRINGISE_ENUM_CLASS_NAMED(TextureCube, "Texture Cube");
    STRINGISE_ENUM_CLASS_NAMED(TextureCubeArray, "Texture Cube Array");
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const ShaderBuiltin &el)
{
  BEGIN_ENUM_STRINGISE(ShaderBuiltin)
  {
    STRINGISE_ENUM_CLASS_NAMED(Undefined, "Undefined");
    STRINGISE_ENUM_CLASS_NAMED(Position, "Position");
    STRINGISE_ENUM_CLASS_NAMED(PointSize, "Point Size");
    STRINGISE_ENUM_CLASS_NAMED(ClipDistance, "Clip Distance");
    STRINGISE_ENUM_CLASS_NAMED(CullDistance, "Cull Distance");
    STRINGISE_ENUM_CLASS_NAMED(RTIndex, "RT Index");
    STRINGISE_ENUM_CLASS_NAMED(ViewportIndex, "Viewport Index");
    STRINGISE_ENUM_CLASS_NAMED(VertexIndex, "Vertex Index");
    STRINGISE_ENUM_CLASS_NAMED(PrimitiveIndex, "Primitive Index");
    STRINGISE_ENUM_CLASS_NAMED(InstanceIndex, "Instance Index");
    STRINGISE_ENUM_CLASS_NAMED(DispatchSize, "Dispatch Size");
    STRINGISE_ENUM_CLASS_NAMED(DispatchThreadIndex, "Dispatch Thread Index");
    STRINGISE_ENUM_CLASS_NAMED(GroupIndex, "Group Index");
    STRINGISE_ENUM_CLASS_NAMED(GroupSize, "Group Size");
    STRINGISE_ENUM_CLASS_NAMED(GroupFlatIndex, "Group Flat Index");
    STRINGISE_ENUM_CLASS_NAMED(GroupThreadIndex, "Group Thread Index");
    STRINGISE_ENUM_CLASS_NAMED(GSInstanceIndex, "GS Instance Index");
    STRINGISE_ENUM_CLASS_NAMED(OutputControlPointIndex, "Output Control Point Index");
    STRINGISE_ENUM_CLASS_NAMED(DomainLocation, "Domain Location");
    STRINGISE_ENUM_CLASS_NAMED(IsFrontFace, "Is FrontFace");
    STRINGISE_ENUM_CLASS_NAMED(MSAACoverage, "MSAA Coverage");
    STRINGISE_ENUM_CLASS_NAMED(MSAASamplePosition, "MSAA Sample Position");
    STRINGISE_ENUM_CLASS_NAMED(MSAASampleIndex, "MSAA Sample Index");
    STRINGISE_ENUM_CLASS_NAMED(PatchNumVertices, "Patch NumVertices");
    STRINGISE_ENUM_CLASS_NAMED(OuterTessFactor, "Outer TessFactor");
    STRINGISE_ENUM_CLASS_NAMED(InsideTessFactor, "Inside TessFactor");
    STRINGISE_ENUM_CLASS_NAMED(ColorOutput, "Color Output");
    STRINGISE_ENUM_CLASS_NAMED(DepthOutput, "Depth Output");
    STRINGISE_ENUM_CLASS_NAMED(DepthOutputGreaterEqual, "Depth Output (GEqual)");
    STRINGISE_ENUM_CLASS_NAMED(DepthOutputLessEqual, "Depth Output (LEqual)");
    STRINGISE_ENUM_CLASS_NAMED(BaseVertex, "Base Vertex");
    STRINGISE_ENUM_CLASS_NAMED(BaseInstance, "Base Instance");
    STRINGISE_ENUM_CLASS_NAMED(DrawIndex, "Draw Index");
    STRINGISE_ENUM_CLASS_NAMED(StencilReference, "Stencil Ref Value");
    STRINGISE_ENUM_CLASS_NAMED(PointCoord, "Point Co-ord");
    STRINGISE_ENUM_CLASS_NAMED(IsHelper, "Is Helper");
    STRINGISE_ENUM_CLASS_NAMED(SubgroupSize, "Subgroup Size");
    STRINGISE_ENUM_CLASS_NAMED(NumSubgroups, "Num Subgroups");
    STRINGISE_ENUM_CLASS_NAMED(SubgroupIndexInWorkgroup, "Subgroup Index in Workgroup");
    STRINGISE_ENUM_CLASS_NAMED(IndexInSubgroup, "Index in Subgroup");
    STRINGISE_ENUM_CLASS_NAMED(SubgroupEqualMask, "Subgroup Equal Mask");
    STRINGISE_ENUM_CLASS_NAMED(SubgroupGreaterEqualMask, "Subgroup Greater-Equal Mask");
    STRINGISE_ENUM_CLASS_NAMED(SubgroupGreaterMask, "Subgroup Greater Mask");
    STRINGISE_ENUM_CLASS_NAMED(SubgroupLessEqualMask, "Subgroup Less-Equal Mask");
    STRINGISE_ENUM_CLASS_NAMED(SubgroupLessMask, "Subgroup Less Mask");
    STRINGISE_ENUM_CLASS_NAMED(DeviceIndex, "Device Index");
    STRINGISE_ENUM_CLASS_NAMED(IsFullyCovered, "Is Fully Covered");
    STRINGISE_ENUM_CLASS_NAMED(FragAreaSize, "Fragment Area Size");
    STRINGISE_ENUM_CLASS_NAMED(FragInvocationCount, "Fragment Invocation Count");
    STRINGISE_ENUM_CLASS_NAMED(PackedFragRate, "Packed Fragment Rate");
    STRINGISE_ENUM_CLASS_NAMED(Barycentrics, "Barycentrics");
    STRINGISE_ENUM_CLASS_NAMED(CullPrimitive, "Cull Primitive Output");
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const BindType &el)
{
  BEGIN_ENUM_STRINGISE(BindType)
  {
    STRINGISE_ENUM_CLASS_NAMED(Unknown, "Unknown");
    STRINGISE_ENUM_CLASS_NAMED(ConstantBuffer, "Constants");
    STRINGISE_ENUM_CLASS_NAMED(Sampler, "Sampler");
    STRINGISE_ENUM_CLASS_NAMED(ImageSampler, "Image&Sampler");
    STRINGISE_ENUM_CLASS_NAMED(ReadOnlyImage, "Image");
    STRINGISE_ENUM_CLASS_NAMED(ReadWriteImage, "RW Image");
    STRINGISE_ENUM_CLASS_NAMED(ReadOnlyTBuffer, "TexBuffer");
    STRINGISE_ENUM_CLASS_NAMED(ReadWriteTBuffer, "RW TexBuffer");
    STRINGISE_ENUM_CLASS_NAMED(ReadOnlyBuffer, "Buffer");
    STRINGISE_ENUM_CLASS_NAMED(ReadWriteBuffer, "RW Buffer");
    STRINGISE_ENUM_CLASS_NAMED(ReadOnlyResource, "Resource");
    STRINGISE_ENUM_CLASS_NAMED(ReadWriteResource, "RW Resource");
    STRINGISE_ENUM_CLASS_NAMED(InputAttachment, "Input");
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const MessageSource &el)
{
  BEGIN_ENUM_STRINGISE(MessageSource)
  {
    STRINGISE_ENUM_CLASS_NAMED(API, "API");
    STRINGISE_ENUM_CLASS_NAMED(RedundantAPIUse, "Redundant API Use");
    STRINGISE_ENUM_CLASS_NAMED(IncorrectAPIUse, "Incorrect API Use");
    STRINGISE_ENUM_CLASS_NAMED(GeneralPerformance, "General Performance");
    STRINGISE_ENUM_CLASS_NAMED(GCNPerformance, "GCN Performance");
    STRINGISE_ENUM_CLASS_NAMED(RuntimeWarning, "Runtime Warning");
    STRINGISE_ENUM_CLASS_NAMED(UnsupportedConfiguration, "Unsupported Configuration");
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const MessageSeverity &el)
{
  BEGIN_ENUM_STRINGISE(MessageSeverity)
  {
    STRINGISE_ENUM_CLASS(High);
    STRINGISE_ENUM_CLASS(Medium);
    STRINGISE_ENUM_CLASS(Low);
    STRINGISE_ENUM_CLASS(Info);
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const MessageCategory &el)
{
  BEGIN_ENUM_STRINGISE(MessageCategory)
  {
    STRINGISE_ENUM_CLASS_NAMED(Application_Defined, "Application Defined");
    STRINGISE_ENUM_CLASS_NAMED(Miscellaneous, "Miscellaneous");
    STRINGISE_ENUM_CLASS_NAMED(Initialization, "Initialization");
    STRINGISE_ENUM_CLASS_NAMED(Cleanup, "Cleanup");
    STRINGISE_ENUM_CLASS_NAMED(Compilation, "Compilation");
    STRINGISE_ENUM_CLASS_NAMED(State_Creation, "State Creation");
    STRINGISE_ENUM_CLASS_NAMED(State_Setting, "State Setting");
    STRINGISE_ENUM_CLASS_NAMED(State_Getting, "State Getting");
    STRINGISE_ENUM_CLASS_NAMED(Resource_Manipulation, "Resource Manipulation");
    STRINGISE_ENUM_CLASS_NAMED(Execution, "Execution");
    STRINGISE_ENUM_CLASS_NAMED(Shaders, "Shaders");
    STRINGISE_ENUM_CLASS_NAMED(Deprecated, "Deprecated");
    STRINGISE_ENUM_CLASS_NAMED(Undefined, "Undefined");
    STRINGISE_ENUM_CLASS_NAMED(Portability, "Portability");
    STRINGISE_ENUM_CLASS_NAMED(Performance, "Performance");
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const TextureSwizzle &el)
{
  BEGIN_ENUM_STRINGISE(TextureSwizzle)
  {
    STRINGISE_ENUM_CLASS_NAMED(Red, "R");
    STRINGISE_ENUM_CLASS_NAMED(Green, "G");
    STRINGISE_ENUM_CLASS_NAMED(Blue, "B");
    STRINGISE_ENUM_CLASS_NAMED(Alpha, "A");
    STRINGISE_ENUM_CLASS_NAMED(Zero, "0");
    STRINGISE_ENUM_CLASS_NAMED(One, "1");
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const ResourceUsage &el)
{
  BEGIN_ENUM_STRINGISE(ResourceUsage)
  {
    STRINGISE_ENUM_CLASS_NAMED(Unused, "Unused");

    STRINGISE_ENUM_CLASS_NAMED(VertexBuffer, "Vertex Buffer");
    STRINGISE_ENUM_CLASS_NAMED(IndexBuffer, "Index Buffer");

    STRINGISE_ENUM_CLASS_NAMED(VS_Constants, "VS - Constants");
    STRINGISE_ENUM_CLASS_NAMED(HS_Constants, "HS - Constants");
    STRINGISE_ENUM_CLASS_NAMED(DS_Constants, "DS - Constants");
    STRINGISE_ENUM_CLASS_NAMED(GS_Constants, "GS - Constants");
    STRINGISE_ENUM_CLASS_NAMED(PS_Constants, "PS - Constants");
    STRINGISE_ENUM_CLASS_NAMED(CS_Constants, "CS - Constants");

    STRINGISE_ENUM_CLASS_NAMED(All_Constants, "All Stages - Constants");

    STRINGISE_ENUM_CLASS_NAMED(StreamOut, "Stream-Out");

    STRINGISE_ENUM_CLASS_NAMED(VS_Resource, "VS - Read-only Resource");
    STRINGISE_ENUM_CLASS_NAMED(HS_Resource, "HS - Read-only Resource");
    STRINGISE_ENUM_CLASS_NAMED(DS_Resource, "DS - Read-only Resource");
    STRINGISE_ENUM_CLASS_NAMED(GS_Resource, "GS - Read-only Resource");
    STRINGISE_ENUM_CLASS_NAMED(PS_Resource, "PS - Read-only Resource");
    STRINGISE_ENUM_CLASS_NAMED(CS_Resource, "CS - Read-only Resource");

    STRINGISE_ENUM_CLASS_NAMED(All_Resource, "All Stages - Read-only Resource");

    STRINGISE_ENUM_CLASS_NAMED(VS_RWResource, "VS - Read-write Resource");
    STRINGISE_ENUM_CLASS_NAMED(HS_RWResource, "HS - Read-write Resource");
    STRINGISE_ENUM_CLASS_NAMED(DS_RWResource, "DS - Read-write Resource");
    STRINGISE_ENUM_CLASS_NAMED(GS_RWResource, "GS - Read-write Resource");
    STRINGISE_ENUM_CLASS_NAMED(PS_RWResource, "PS - Read-write Resource");
    STRINGISE_ENUM_CLASS_NAMED(CS_RWResource, "CS - Read-write Resource");

    STRINGISE_ENUM_CLASS_NAMED(All_RWResource, "All Stages - Read-write Resource");

    STRINGISE_ENUM_CLASS_NAMED(InputTarget, "Input target");
    STRINGISE_ENUM_CLASS_NAMED(ColorTarget, "Color target");
    STRINGISE_ENUM_CLASS_NAMED(DepthStencilTarget, "Depth/stencil target");

    STRINGISE_ENUM_CLASS_NAMED(Indirect, "Indirect parameters");

    STRINGISE_ENUM_CLASS_NAMED(Clear, "Clear");
    STRINGISE_ENUM_CLASS_NAMED(Discard, "Discard");

    STRINGISE_ENUM_CLASS_NAMED(GenMips, "Mip Generation");
    STRINGISE_ENUM_CLASS_NAMED(Resolve, "Resolve - Source&Dest");
    STRINGISE_ENUM_CLASS_NAMED(ResolveSrc, "Resolve - Source");
    STRINGISE_ENUM_CLASS_NAMED(ResolveDst, "Resolve - Destination");
    STRINGISE_ENUM_CLASS_NAMED(Copy, "Copy - Source&Dest");
    STRINGISE_ENUM_CLASS_NAMED(CopySrc, "Copy - Source");
    STRINGISE_ENUM_CLASS_NAMED(CopyDst, "Copy - Destination");

    STRINGISE_ENUM_CLASS_NAMED(Barrier, "Barrier");

    STRINGISE_ENUM_CLASS_NAMED(CPUWrite, "CPU Write");
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const VarType &el)
{
  BEGIN_ENUM_STRINGISE(VarType)
  {
    STRINGISE_ENUM_CLASS_NAMED(Float, "float");
    STRINGISE_ENUM_CLASS_NAMED(Double, "double");
    STRINGISE_ENUM_CLASS_NAMED(Half, "half");
    STRINGISE_ENUM_CLASS_NAMED(SInt, "int");
    STRINGISE_ENUM_CLASS_NAMED(UInt, "uint");
    STRINGISE_ENUM_CLASS_NAMED(SShort, "short");
    STRINGISE_ENUM_CLASS_NAMED(UShort, "ushort");
    STRINGISE_ENUM_CLASS_NAMED(SLong, "long");
    STRINGISE_ENUM_CLASS_NAMED(ULong, "ulong");
    STRINGISE_ENUM_CLASS_NAMED(SByte, "byte");
    STRINGISE_ENUM_CLASS_NAMED(UByte, "ubyte");
    STRINGISE_ENUM_CLASS_NAMED(Bool, "bool");
    STRINGISE_ENUM_CLASS_NAMED(Enum, "enum");
    STRINGISE_ENUM_CLASS_NAMED(Struct, "struct");
    STRINGISE_ENUM_CLASS_NAMED(GPUPointer, "pointer");
    STRINGISE_ENUM_CLASS_NAMED(ConstantBlock, "cbuffer");
    STRINGISE_ENUM_CLASS_NAMED(ReadOnlyResource, "resource");
    STRINGISE_ENUM_CLASS_NAMED(ReadWriteResource, "rwresource");
    STRINGISE_ENUM_CLASS_NAMED(Sampler, "sampler");
    STRINGISE_ENUM_CLASS_NAMED(Unknown, "unknown");
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const DebugVariableType &el)
{
  BEGIN_ENUM_STRINGISE(DebugVariableType)
  {
    STRINGISE_ENUM_CLASS(Undefined);
    STRINGISE_ENUM_CLASS(Input);
    STRINGISE_ENUM_CLASS(Constant);
    STRINGISE_ENUM_CLASS(Variable);
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const GPUCounter &el)
{
  if(IsAMDCounter(el))
    return "AMD Counter " + ToStr((uint32_t)el);

  if(IsNvidiaCounter(el))
    return "Nvidia Counter " + ToStr((uint32_t)el);

  if(IsIntelCounter(el))
    return "Intel Counter " + ToStr((uint32_t)el);

  BEGIN_ENUM_STRINGISE(GPUCounter)
  {
    STRINGISE_ENUM_CLASS(EventGPUDuration);
    STRINGISE_ENUM_CLASS(InputVerticesRead);
    STRINGISE_ENUM_CLASS(IAPrimitives);
    STRINGISE_ENUM_CLASS(GSPrimitives);
    STRINGISE_ENUM_CLASS(RasterizerInvocations);
    STRINGISE_ENUM_CLASS(RasterizedPrimitives);
    STRINGISE_ENUM_CLASS(SamplesPassed);
    STRINGISE_ENUM_CLASS(VSInvocations);
    STRINGISE_ENUM_CLASS(HSInvocations);
    STRINGISE_ENUM_CLASS(DSInvocations);
    STRINGISE_ENUM_CLASS(GSInvocations);
    STRINGISE_ENUM_CLASS(PSInvocations);
    STRINGISE_ENUM_CLASS(CSInvocations);
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const CounterUnit &el)
{
  BEGIN_ENUM_STRINGISE(CounterUnit)
  {
    STRINGISE_ENUM_CLASS(Absolute);
    STRINGISE_ENUM_CLASS(Seconds);
    STRINGISE_ENUM_CLASS(Percentage);
    STRINGISE_ENUM_CLASS(Ratio);
    STRINGISE_ENUM_CLASS(Bytes);
    STRINGISE_ENUM_CLASS(Cycles);
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const ShaderStage &el)
{
  BEGIN_ENUM_STRINGISE(ShaderStage)
  {
    STRINGISE_ENUM_CLASS(Vertex);
    STRINGISE_ENUM_CLASS(Hull);
    STRINGISE_ENUM_CLASS(Domain);
    STRINGISE_ENUM_CLASS(Geometry);
    STRINGISE_ENUM_CLASS(Pixel);
    STRINGISE_ENUM_CLASS(Compute);
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const MeshDataStage &el)
{
  BEGIN_ENUM_STRINGISE(MeshDataStage)
  {
    STRINGISE_ENUM_CLASS(Unknown);
    STRINGISE_ENUM_CLASS(VSIn);
    STRINGISE_ENUM_CLASS(VSOut);
    STRINGISE_ENUM_CLASS(GSOut);
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const DebugOverlay &el)
{
  BEGIN_ENUM_STRINGISE(DebugOverlay)
  {
    STRINGISE_ENUM_CLASS(NoOverlay);
    STRINGISE_ENUM_CLASS(Drawcall);
    STRINGISE_ENUM_CLASS(Wireframe);
    STRINGISE_ENUM_CLASS(Depth);
    STRINGISE_ENUM_CLASS(Stencil);
    STRINGISE_ENUM_CLASS(BackfaceCull);
    STRINGISE_ENUM_CLASS(ViewportScissor);
    STRINGISE_ENUM_CLASS(NaN);
    STRINGISE_ENUM_CLASS(Clipping);
    STRINGISE_ENUM_CLASS(ClearBeforePass);
    STRINGISE_ENUM_CLASS(ClearBeforeDraw);
    STRINGISE_ENUM_CLASS(QuadOverdrawPass);
    STRINGISE_ENUM_CLASS(QuadOverdrawDraw);
    STRINGISE_ENUM_CLASS(TriangleSizePass);
    STRINGISE_ENUM_CLASS(TriangleSizeDraw);
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const GPUVendor &el)
{
  BEGIN_ENUM_STRINGISE(GPUVendor)
  {
    STRINGISE_ENUM_CLASS(Unknown);
    STRINGISE_ENUM_CLASS(ARM);
    STRINGISE_ENUM_CLASS(AMD);
    STRINGISE_ENUM_CLASS(Broadcom);
    STRINGISE_ENUM_CLASS(Imagination);
    STRINGISE_ENUM_CLASS(Intel);
    STRINGISE_ENUM_CLASS(nVidia);
    STRINGISE_ENUM_CLASS(Qualcomm);
    STRINGISE_ENUM_CLASS(Verisilicon);
    STRINGISE_ENUM_CLASS(Software);
    STRINGISE_ENUM_CLASS(Samsung);
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const GraphicsAPI &el)
{
  BEGIN_ENUM_STRINGISE(GraphicsAPI)
  {
    STRINGISE_ENUM_CLASS(D3D11);
    STRINGISE_ENUM_CLASS(D3D12);
    STRINGISE_ENUM_CLASS(OpenGL);
    STRINGISE_ENUM_CLASS(Vulkan);
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const ShaderEncoding &el)
{
  BEGIN_ENUM_STRINGISE(ShaderEncoding)
  {
    STRINGISE_ENUM_CLASS(Unknown);
    STRINGISE_ENUM_CLASS(DXBC);
    STRINGISE_ENUM_CLASS(GLSL);
    STRINGISE_ENUM_CLASS_NAMED(SPIRV, "SPIR-V");
    STRINGISE_ENUM_CLASS_NAMED(SPIRVAsm, "SPIR-V Asm");
    STRINGISE_ENUM_CLASS(HLSL);
    STRINGISE_ENUM_CLASS(DXIL);
    STRINGISE_ENUM_CLASS_NAMED(OpenGLSPIRV, "SPIR-V (OpenGL)");
    STRINGISE_ENUM_CLASS_NAMED(OpenGLSPIRVAsm, "SPIR-V Asm (OpenGL)");
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const KnownShaderTool &el)
{
  BEGIN_ENUM_STRINGISE(KnownShaderTool);
  {
    STRINGISE_ENUM_CLASS_NAMED(Unknown, "Custom Tool");
    STRINGISE_ENUM_CLASS_NAMED(SPIRV_Cross, "SPIRV-Cross");
    STRINGISE_ENUM_CLASS_NAMED(spirv_dis, "spirv-dis");
    STRINGISE_ENUM_CLASS_NAMED(glslangValidatorGLSL, "glslang (GLSL)");
    STRINGISE_ENUM_CLASS_NAMED(glslangValidatorHLSL, "glslang (HLSL)");
    STRINGISE_ENUM_CLASS_NAMED(spirv_as, "spirv-as");
    STRINGISE_ENUM_CLASS_NAMED(dxcSPIRV, "dxc (SPIR-V)");
    STRINGISE_ENUM_CLASS_NAMED(dxcDXIL, "dxc (DXIL)");
    STRINGISE_ENUM_CLASS_NAMED(fxc, "fxc");
    STRINGISE_ENUM_CLASS_NAMED(glslangValidatorGLSL_OpenGL, "glslang (GLSL to OpenGL SPIR-V)");
    STRINGISE_ENUM_CLASS_NAMED(SPIRV_Cross_OpenGL, "SPIRV-Cross (OpenGL SPIR-V)");
    STRINGISE_ENUM_CLASS_NAMED(spirv_as_OpenGL, "spirv-as (OpenGL SPIR-V)");
    STRINGISE_ENUM_CLASS_NAMED(spirv_dis_OpenGL, "spirv-dis (OpenGL SPIR-V)");
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const SectionType &el)
{
  BEGIN_ENUM_STRINGISE(SectionType);
  {
    STRINGISE_ENUM_CLASS_NAMED(FrameCapture, "renderdoc/internal/framecapture");
    STRINGISE_ENUM_CLASS_NAMED(ResolveDatabase, "renderdoc/internal/resolvedb");
    STRINGISE_ENUM_CLASS_NAMED(Bookmarks, "renderdoc/ui/bookmarks");
    STRINGISE_ENUM_CLASS_NAMED(Notes, "renderdoc/ui/notes");
    STRINGISE_ENUM_CLASS_NAMED(ResourceRenames, "renderdoc/ui/resrenames");
    STRINGISE_ENUM_CLASS_NAMED(AMDRGPProfile, "amd/rgp/profile");
    STRINGISE_ENUM_CLASS_NAMED(ExtendedThumbnail, "renderdoc/internal/exthumb");
    STRINGISE_ENUM_CLASS_NAMED(EmbeddedLogfile, "renderdoc/internal/logfile");
    STRINGISE_ENUM_CLASS_NAMED(EditedShaders, "renderdoc/ui/edits");
    STRINGISE_ENUM_CLASS_NAMED(D3D12Core, "renderdoc/internal/d3d12core");
    STRINGISE_ENUM_CLASS_NAMED(D3D12SDKLayers, "renderdoc/internal/d3d12sdklayers");
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const ReplayOptimisationLevel &el)
{
  BEGIN_ENUM_STRINGISE(ReplayOptimisationLevel);
  {
    STRINGISE_ENUM_CLASS_NAMED(NoOptimisation, "No Optimisation");
    STRINGISE_ENUM_CLASS(Conservative);
    STRINGISE_ENUM_CLASS(Balanced);
    STRINGISE_ENUM_CLASS(Fastest);
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const D3DBufferViewFlags &el)
{
  BEGIN_BITFIELD_STRINGISE(D3DBufferViewFlags);
  {
    STRINGISE_BITFIELD_CLASS_VALUE_NAMED(NoFlags, "");

    STRINGISE_BITFIELD_CLASS_BIT(Raw);
    STRINGISE_BITFIELD_CLASS_BIT(Append);
    STRINGISE_BITFIELD_CLASS_BIT(Counter);
  }
  END_BITFIELD_STRINGISE();
}

template <>
rdcstr DoStringise(const PathProperty &el)
{
  BEGIN_BITFIELD_STRINGISE(PathProperty);
  {
    STRINGISE_BITFIELD_CLASS_VALUE_NAMED(ErrorUnknown, "Unknown Error");
    STRINGISE_BITFIELD_CLASS_VALUE_NAMED(ErrorAccessDenied, "Access Denied");
    STRINGISE_BITFIELD_CLASS_VALUE_NAMED(ErrorInvalidPath, "Invalid Path");
    STRINGISE_BITFIELD_CLASS_VALUE_NAMED(NoFlags, "No Flags");

    STRINGISE_BITFIELD_CLASS_BIT(Directory);
    STRINGISE_BITFIELD_CLASS_BIT(Hidden);
    STRINGISE_BITFIELD_CLASS_BIT(Executable);
  }
  END_BITFIELD_STRINGISE();
}

template <>
rdcstr DoStringise(const SectionFlags &el)
{
  BEGIN_BITFIELD_STRINGISE(SectionFlags);
  {
    STRINGISE_BITFIELD_CLASS_VALUE_NAMED(NoFlags, "No Flags");

    STRINGISE_BITFIELD_CLASS_BIT_NAMED(ASCIIStored, "Stored as ASCII");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(LZ4Compressed, "Compressed with LZ4");
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(ZstdCompressed, "Compressed with Zstd");
  }
  END_BITFIELD_STRINGISE();
}

template <>
rdcstr DoStringise(const ShaderVariableFlags &el)
{
  BEGIN_BITFIELD_STRINGISE(ShaderVariableFlags);
  {
    STRINGISE_BITFIELD_CLASS_VALUE_NAMED(NoFlags, "None");

    STRINGISE_BITFIELD_CLASS_BIT(RowMajorMatrix);
    STRINGISE_BITFIELD_CLASS_BIT(HexDisplay);
    STRINGISE_BITFIELD_CLASS_BIT(RGBDisplay);
    STRINGISE_BITFIELD_CLASS_BIT(R11G11B10);
    STRINGISE_BITFIELD_CLASS_BIT(R10G10B10A2);
    STRINGISE_BITFIELD_CLASS_BIT(UNorm);
    STRINGISE_BITFIELD_CLASS_BIT(SNorm);
    STRINGISE_BITFIELD_CLASS_BIT(Truncated);
  }
  END_BITFIELD_STRINGISE();
}

template <>
rdcstr DoStringise(const ShaderEvents &el)
{
  BEGIN_BITFIELD_STRINGISE(ShaderEvents);
  {
    STRINGISE_BITFIELD_CLASS_VALUE_NAMED(NoEvent, "None");

    STRINGISE_BITFIELD_CLASS_BIT(SampleLoadGather);
    STRINGISE_BITFIELD_CLASS_BIT(GeneratedNanOrInf);
  }
  END_BITFIELD_STRINGISE();
}

template <>
rdcstr DoStringise(const TextureCategory &el)
{
  BEGIN_BITFIELD_STRINGISE(TextureCategory);
  {
    STRINGISE_BITFIELD_CLASS_VALUE_NAMED(NoFlags, "None");

    STRINGISE_BITFIELD_CLASS_BIT(ShaderRead);
    STRINGISE_BITFIELD_CLASS_BIT(ColorTarget);
    STRINGISE_BITFIELD_CLASS_BIT(DepthTarget);
    STRINGISE_BITFIELD_CLASS_BIT(ShaderReadWrite);
    STRINGISE_BITFIELD_CLASS_BIT(SwapBuffer);
  }
  END_BITFIELD_STRINGISE();
}

template <>
rdcstr DoStringise(const BufferCategory &el)
{
  BEGIN_BITFIELD_STRINGISE(BufferCategory);
  {
    STRINGISE_BITFIELD_CLASS_VALUE_NAMED(NoFlags, "None");

    STRINGISE_BITFIELD_CLASS_BIT(Vertex);
    STRINGISE_BITFIELD_CLASS_BIT(Index);
    STRINGISE_BITFIELD_CLASS_BIT(Constants);
    STRINGISE_BITFIELD_CLASS_BIT(ReadWrite);
    STRINGISE_BITFIELD_CLASS_BIT(Indirect);
  }
  END_BITFIELD_STRINGISE();
}

template <>
rdcstr DoStringise(const ActionFlags &el)
{
  BEGIN_BITFIELD_STRINGISE(ActionFlags);
  {
    STRINGISE_BITFIELD_CLASS_VALUE_NAMED(NoFlags, "None");

    STRINGISE_BITFIELD_CLASS_BIT(Clear);
    STRINGISE_BITFIELD_CLASS_BIT(Drawcall);
    STRINGISE_BITFIELD_CLASS_BIT(Dispatch);
    STRINGISE_BITFIELD_CLASS_BIT(CmdList);
    STRINGISE_BITFIELD_CLASS_BIT(SetMarker);
    STRINGISE_BITFIELD_CLASS_BIT(PushMarker);
    STRINGISE_BITFIELD_CLASS_BIT(PopMarker);
    STRINGISE_BITFIELD_CLASS_BIT(Present);
    STRINGISE_BITFIELD_CLASS_BIT(MultiAction);
    STRINGISE_BITFIELD_CLASS_BIT(Copy);
    STRINGISE_BITFIELD_CLASS_BIT(Resolve);
    STRINGISE_BITFIELD_CLASS_BIT(GenMips);
    STRINGISE_BITFIELD_CLASS_BIT(PassBoundary);

    STRINGISE_BITFIELD_CLASS_BIT(Indexed);
    STRINGISE_BITFIELD_CLASS_BIT(Instanced);
    STRINGISE_BITFIELD_CLASS_BIT(Auto);
    STRINGISE_BITFIELD_CLASS_BIT(Indirect);
    STRINGISE_BITFIELD_CLASS_BIT(ClearColor);
    STRINGISE_BITFIELD_CLASS_BIT(ClearDepthStencil);
    STRINGISE_BITFIELD_CLASS_BIT(BeginPass);
    STRINGISE_BITFIELD_CLASS_BIT(EndPass);
    STRINGISE_BITFIELD_CLASS_BIT(CommandBufferBoundary);
  }
  END_BITFIELD_STRINGISE();
}

template <>
rdcstr DoStringise(const ShaderStageMask &el)
{
  BEGIN_BITFIELD_STRINGISE(ShaderStageMask);
  {
    STRINGISE_BITFIELD_CLASS_VALUE(Unknown);
    STRINGISE_BITFIELD_CLASS_VALUE(All);

    STRINGISE_BITFIELD_CLASS_BIT(Vertex);
    STRINGISE_BITFIELD_CLASS_BIT(Hull);
    STRINGISE_BITFIELD_CLASS_BIT(Domain);
    STRINGISE_BITFIELD_CLASS_BIT(Geometry);
    STRINGISE_BITFIELD_CLASS_BIT(Pixel);
    STRINGISE_BITFIELD_CLASS_BIT(Compute);
  }
  END_BITFIELD_STRINGISE();
}
