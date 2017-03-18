/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015-2017 Baldur Karlsson
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
 
layout(binding = 0) uniform sampler2DArray srcDepthArray;
layout(binding = 1) uniform usampler2DArray srcStencilArray;
// binding = 2 used as an image in the colour copy compute shaders

#ifdef VULKAN

layout(push_constant) uniform multisamplePush
{
	int numMultiSamples;
	int currentSample;
	int currentSlice;
	uint currentStencil;
} mscopy;

#define numMultiSamples (mscopy.numMultiSamples)
#define currentSample (mscopy.currentSample)
#define currentSlice (mscopy.currentSlice)
#define currentStencil (mscopy.currentStencil)

#else

uniform uvec4 mscopy;

#define numMultiSamples (int(mscopy.x))
#define currentSample (mscopy.y)
#define currentSlice (mscopy.z)
#define currentStencil (mscopy.w)

#endif

void main()
{
	ivec3 srcCoord = ivec3(int(gl_FragCoord.x), int(gl_FragCoord.y), currentSlice*numMultiSamples + gl_SampleID);

	if(currentStencil < 256)
	{
		uint stencil = texelFetch(srcStencilArray, srcCoord, 0).x;

		if(stencil != currentStencil)
			discard;
	}

	gl_FragDepth = texelFetch(srcDepthArray, srcCoord, 0).x;
}
