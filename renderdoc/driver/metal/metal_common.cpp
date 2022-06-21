/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 Baldur Karlsson
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

#include "metal_common.h"

BlendMultiplier MakeBlendMultiplier(MTL::BlendFactor blend)
{
  switch(blend)
  {
    case MTL::BlendFactorZero: return BlendMultiplier::Zero;
    case MTL::BlendFactorOne: return BlendMultiplier::One;
    case MTL::BlendFactorSourceColor: return BlendMultiplier::SrcCol;
    case MTL::BlendFactorOneMinusSourceColor: return BlendMultiplier::InvSrcCol;
    case MTL::BlendFactorDestinationColor: return BlendMultiplier::DstCol;
    case MTL::BlendFactorOneMinusDestinationColor: return BlendMultiplier::InvDstCol;
    case MTL::BlendFactorSourceAlpha: return BlendMultiplier::SrcAlpha;
    case MTL::BlendFactorOneMinusSourceAlpha: return BlendMultiplier::InvSrcAlpha;
    case MTL::BlendFactorDestinationAlpha: return BlendMultiplier::DstAlpha;
    case MTL::BlendFactorOneMinusDestinationAlpha: return BlendMultiplier::InvDstAlpha;
    case MTL::BlendFactorBlendColor: return BlendMultiplier::FactorRGB;
    case MTL::BlendFactorOneMinusBlendColor: return BlendMultiplier::InvFactorRGB;
    case MTL::BlendFactorBlendAlpha: return BlendMultiplier::FactorAlpha;
    case MTL::BlendFactorOneMinusBlendAlpha: return BlendMultiplier::InvFactorAlpha;
    case MTL::BlendFactorSourceAlphaSaturated: return BlendMultiplier::SrcAlphaSat;
    case MTL::BlendFactorSource1Color: return BlendMultiplier::Src1Col;
    case MTL::BlendFactorOneMinusSource1Color: return BlendMultiplier::InvSrc1Col;
    case MTL::BlendFactorSource1Alpha: return BlendMultiplier::Src1Alpha;
    case MTL::BlendFactorOneMinusSource1Alpha: return BlendMultiplier::InvSrc1Alpha;
    default: break;
  }

  return BlendMultiplier::One;
}

BlendOperation MakeBlendOp(MTL::BlendOperation op)
{
  switch(op)
  {
    case MTL::BlendOperationAdd: return BlendOperation::Add;
    case MTL::BlendOperationSubtract: return BlendOperation::Subtract;
    case MTL::BlendOperationReverseSubtract: return BlendOperation::ReversedSubtract;
    case MTL::BlendOperationMin: return BlendOperation::Minimum;
    case MTL::BlendOperationMax: return BlendOperation::Maximum;
    default: break;
  }

  return BlendOperation::Add;
}

byte MakeWriteMask(MTL::ColorWriteMask mask)
{
  byte ret = 0;

  if(mask & MTL::ColorWriteMaskRed)
    ret |= 0x1;
  if(mask & MTL::ColorWriteMaskGreen)
    ret |= 0x2;
  if(mask & MTL::ColorWriteMaskBlue)
    ret |= 0x4;
  if(mask & MTL::ColorWriteMaskAlpha)
    ret |= 0x8;

  return ret;
}
