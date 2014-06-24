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

#include "core/resource_manager.h"

#include "driver/gl/gl_common.h"

size_t GetByteSize(GLsizei w, GLsizei h, GLsizei d, GLenum format, GLenum type, int level, int align);

enum GLNamespace
{
	eResUnknown = 0,
	eResSpecial,
	eResTexture,
	eResSampler,
	eResFramebuffer,
	eResBuffer,
	eResVertexArray,
	eResShader,
	eResProgram,
	eResProgramPipe,
	eResQuery,
	eResSync,
};

enum GLSpecialResource
{
	eSpecialResDevice = 0,
	eSpecialResContext = 0,
};

enum NullInitialiser { MakeNullResource };

struct GLResource
{
	GLResource() { Namespace = eResUnknown; name = ~0U; }
	GLResource(NullInitialiser) { Namespace = eResUnknown; name = ~0U; }
	GLResource(GLNamespace n, GLuint i) { Namespace = n; name = i; }
	GLNamespace Namespace;
	GLuint name;

	bool operator ==(const GLResource &o) const
	{
		return Namespace == o.Namespace && name == o.name;
	}

	bool operator !=(const GLResource &o) const
	{
		return !(*this == o);
	}

	bool operator <(const GLResource &o) const
	{
		if(Namespace != o.Namespace) return Namespace < o.Namespace;
		return name < o.name;
	}
};

inline GLResource TextureRes(GLuint i) { return GLResource(eResTexture, i); }
inline GLResource SamplerRes(GLuint i) { return GLResource(eResSampler, i); }
inline GLResource FramebufferRes(GLuint i) { return GLResource(eResFramebuffer, i); }
inline GLResource BufferRes(GLuint i) { return GLResource(eResBuffer, i); }
inline GLResource VertexArrayRes(GLuint i) { return GLResource(eResVertexArray, i); }
inline GLResource ShaderRes(GLuint i) { return GLResource(eResShader, i); }
inline GLResource ProgramRes(GLuint i) { return GLResource(eResProgram, i); }
inline GLResource ProgramPipeRes(GLuint i) { return GLResource(eResProgramPipe, i); }
inline GLResource QueryRes(GLuint i) { return GLResource(eResQuery, i); }
inline GLResource SyncRes(GLuint i) { return GLResource(eResSync, i); }

struct GLResourceRecord : public ResourceRecord
{
	static const NullInitialiser NullResource = MakeNullResource;

	GLResourceRecord(ResourceId id) :
	ResourceRecord(id, true),
		datatype(eGL_UNKNOWN_ENUM),
		usage(eGL_UNKNOWN_ENUM)
	{
		RDCEraseEl(ShadowPtr);
	}

	~GLResourceRecord()
	{
		FreeShadowStorage();
	}

	enum MapStatus
	{
		Unmapped,
		Mapped_Read,
		Mapped_Read_Real,
		Mapped_Write,
		Mapped_Write_Real,
		Mapped_Ignore_Real,
	};

	struct
	{
		GLintptr offset;
		GLsizeiptr length;
		GLbitfield access;
		MapStatus status;
		bool invalidate;
		byte *ptr;
	} Map;

	GLenum datatype;
	GLenum usage;

	void AllocShadowStorage(size_t size)
	{
		if(ShadowPtr[0] == NULL)
		{
			ShadowPtr[0] = Serialiser::AllocAlignedBuffer(size);
			ShadowPtr[1] = Serialiser::AllocAlignedBuffer(size);
		}
	}

	void FreeShadowStorage()
	{
		if(ShadowPtr[0] != NULL)
		{
			Serialiser::FreeAlignedBuffer(ShadowPtr[0]);
			Serialiser::FreeAlignedBuffer(ShadowPtr[1]);
		}
		ShadowPtr[0] = ShadowPtr[1] = NULL;
	}
	
	byte *GetShadowPtr(int p)
	{
		return ShadowPtr[p];
	}
	
private:
	byte *ShadowPtr[2];
};

namespace TrackedResource
{
	ResourceId GetNewUniqueID();
	void SetReplayResourceIDs();
};
