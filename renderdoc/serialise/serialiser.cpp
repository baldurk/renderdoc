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


#include "serialiser.h"

#include "core/core.h"

#include "serialise/string_utils.h"

#ifdef _MSC_VER
#pragma warning (disable : 4422) // warning C4422: 'snprintf' : too many arguments passed for format string
                                 // false positive as VS is trying to parse renderdoc's custom format strings
#endif

#if !defined(RELEASE)

int64_t Chunk::m_LiveChunks = 0;
int64_t Chunk::m_TotalMem = 0;
int64_t Chunk::m_MaxChunks = 0;

#endif

const uint32_t Serialiser::MAGIC_HEADER = MAKE_FOURCC('R', 'D', 'O', 'C');
const size_t Serialiser::BufferAlignment = 16;

Chunk::Chunk(Serialiser *ser, uint32_t chunkType, size_t alignment, bool temporary)
{
	m_Length = (uint32_t)ser->GetOffset();

	RDCASSERT(ser->GetOffset() < 0xffffffff);

	m_ChunkType = chunkType;

	m_Temporary = temporary;

	if(alignment)
	{
		m_Data = Serialiser::AllocAlignedBuffer(m_Length, alignment);
		m_AlignedData = true;
	}
	else if(ser->HasAlignedData())
	{
		m_Data = Serialiser::AllocAlignedBuffer(m_Length);
		m_AlignedData = true;
	}
	else
	{
		m_Data = new byte[m_Length];
		m_AlignedData = false;
	}

	memcpy(m_Data, ser->GetRawPtr(0), m_Length);

	m_DebugStr = ser->GetDebugStr();

	ser->Rewind();
	
#if !defined(RELEASE)
	int64_t newval = Atomic::Inc64(&m_LiveChunks);
	Atomic::ExchAdd64(&m_TotalMem, m_Length);

	if(newval > m_MaxChunks)
	{
		int breakpointme=0;
	}

	m_MaxChunks = RDCMAX(newval, m_MaxChunks);
#endif
}

Chunk::~Chunk()
{
#if !defined(RELEASE)
	Atomic::Dec64(&m_LiveChunks);
	Atomic::ExchAdd64(&m_TotalMem, -int64_t(m_Length));
#endif

	if(m_AlignedData)
	{
		if(m_Data)
			Serialiser::FreeAlignedBuffer(m_Data);

		m_Data = NULL;
	}
	else
	{
		SAFE_DELETE_ARRAY(m_Data);
	}
}

void Serialiser::Reset()
{
	if(m_ResolverThread != 0)
	{
		m_ResolverThreadKillSignal = true;

		Threading::JoinThread(m_ResolverThread);
		Threading::CloseThread(m_ResolverThread);
		m_ResolverThread = 0;
	}

	m_DebugText = "";

	m_HasError = false;
	m_ErrorCode = eSerError_None;

	m_Mode = NONE;

	m_Indent = 0;

	SAFE_DELETE_ARRAY(m_pCallstack);
	SAFE_DELETE_ARRAY(m_pResolver);
	if(m_Buffer)
	{
		FreeAlignedBuffer(m_Buffer);
		m_Buffer = NULL;
	}
	
	m_ChunkLookup = NULL;

	m_HasResolver = false;

	m_AlignedData = false;
	
	m_ReadFileHandle = NULL;

	m_Buffer = NULL;
	m_BufferSize = 0;
	m_BufferHead = NULL;
}

Serialiser::Serialiser(size_t length, const byte *memoryBuf, bool fileheader)
	: m_pCallstack(NULL), m_pResolver(NULL), m_Buffer(NULL)
{
	m_ResolverThread = 0; 

	Reset();
	
	m_DebugTextWriting = false;

	m_Mode = READING;
	m_DebugEnabled = false;

	if(!fileheader)
	{
		m_HasResolver = false;

		m_FileStartOffset = 0;

		m_BufferSize = length;
		m_CurrentBufferSize = (size_t)m_BufferSize;
		m_BufferHead = m_Buffer = AllocAlignedBuffer(m_CurrentBufferSize);
		m_ReadOffset = 0;

		memcpy(m_Buffer, memoryBuf + m_FileStartOffset, m_CurrentBufferSize);
		return;
	}
	
	DebuggerHeader *header = (DebuggerHeader *)memoryBuf;

	if(length < sizeof(DebuggerHeader))
	{
		RDCERR("Can't read from in-memory buffer, truncated header");
		m_ErrorCode = eSerError_Corrupt;
		m_HasError = true;
		return;
	}

	if(header->magic != MAGIC_HEADER)
	{
		char magicRef[5] = { 0 };
		char magicFile[5] = { 0 };
		memcpy(magicRef, &MAGIC_HEADER, sizeof(uint32_t));
		memcpy(magicFile, &header->magic, sizeof(uint32_t));
		RDCWARN("Invalid in-memory buffer. Expected magic %s, got %s", magicRef, magicFile);

		m_ErrorCode = eSerError_Corrupt;
		m_HasError = true;
		return;
	}

	if(header->version != SERIALISE_VERSION)
	{
		RDCERR("Capture file from wrong version. This program is on logfile version %llu, file is logfile version %llu", SERIALISE_VERSION, header->version);
		
		m_ErrorCode = eSerError_UnsupportedVersion;
		m_HasError = true;
		return;
	}

	if(header->fileSize < length)
	{
		RDCERR("Overlong in-memory buffer. Expected length 0x016llx, got 0x016llx", header->fileSize, length);
		
		m_ErrorCode = eSerError_Corrupt;
		m_HasError = true;
		return;
	}
	
	m_HasResolver = header->resolveDBSize > 0;

	m_FileStartOffset = AlignUp16(sizeof(DebuggerHeader) + header->resolveDBSize);

	m_BufferSize = length-m_FileStartOffset;
	m_CurrentBufferSize = (size_t)m_BufferSize;
	m_BufferHead = m_Buffer = AllocAlignedBuffer(m_CurrentBufferSize);
	m_ReadOffset = 0;

	memcpy(m_Buffer, memoryBuf + m_FileStartOffset, m_CurrentBufferSize);
}

Serialiser::Serialiser(const char *path, Mode mode, bool debugMode)
	: m_pCallstack(NULL), m_pResolver(NULL), m_Buffer(NULL)
{
	m_ResolverThread = 0; 

	Reset();

	m_Filename = path ? path : "";

	m_DebugTextWriting = false;
	
	m_Mode = mode;
	m_DebugEnabled = debugMode;

	DebuggerHeader header;

	if(mode == READING)
	{
		m_ReadFileHandle = FileIO::fopen(m_Filename.c_str(), "rb");

		if(!m_ReadFileHandle)
		{
			RDCERR("Can't open capture file '%s' for read - errno %d", m_Filename.c_str(), errno);
			m_ErrorCode = eSerError_FileIO;
			m_HasError = true;
			return;
		}

		RDCDEBUG("Opened capture file for read");
		
		FileIO::fread(&header, 1, sizeof(DebuggerHeader), m_ReadFileHandle);

		if(header.magic != MAGIC_HEADER)
		{
			RDCWARN("Invalid capture file. Expected magic %08x, got %08x", MAGIC_HEADER, (uint32_t)header.magic);
			
			m_ErrorCode = eSerError_Corrupt;
			m_HasError = true;
			FileIO::fclose(m_ReadFileHandle);
			m_ReadFileHandle = 0;
			return;
		}
		
		if(header.version != SERIALISE_VERSION)
		{
			RDCERR("Capture file from wrong version. This program is logfile version %llu, file is logfile version %llu", SERIALISE_VERSION, header.version);
			
			m_ErrorCode = eSerError_UnsupportedVersion;
			m_HasError = true;
			FileIO::fclose(m_ReadFileHandle);
			m_ReadFileHandle = 0;
			return;
		}

		FileIO::fseek64(m_ReadFileHandle, 0, SEEK_END);

		uint64_t realLength = FileIO::ftell64(m_ReadFileHandle);
		if(header.fileSize != realLength)
		{
			RDCERR("Truncated/overlong capture file. Expected length 0x016llx, got 0x016llx", header.fileSize, realLength);
			
			m_ErrorCode = eSerError_Corrupt;
			m_HasError = true;
			FileIO::fclose(m_ReadFileHandle);
			m_ReadFileHandle = 0;
			return;
		}

		m_HasResolver = header.resolveDBSize > 0;

		m_FileStartOffset = AlignUp16(sizeof(DebuggerHeader) + header.resolveDBSize);

		m_BufferSize = realLength-m_FileStartOffset;
		m_CurrentBufferSize = (size_t)RDCMIN(m_BufferSize, (uint64_t)64*1024);
		m_BufferHead = m_Buffer = AllocAlignedBuffer(m_CurrentBufferSize);
		m_ReadOffset = 0;

		ReadFromFile(0, m_CurrentBufferSize);
	}
	else
	{
		m_pResolver = NULL;

		if(m_Filename != "")
		{
			m_BufferSize = 0;
			m_BufferHead = m_Buffer = NULL;
		}
		else
		{
			m_BufferSize = 128*1024;
			m_BufferHead = m_Buffer = AllocAlignedBuffer((size_t)m_BufferSize);
		}

		m_ReadOffset = 0;
		m_FileStartOffset = 0;
	}
}

void Serialiser::ReadFromFile(uint64_t destOffs, size_t chunkLen)
{
	RDCASSERT(m_ReadFileHandle);

	if(m_ReadFileHandle == NULL)
		return;

	FileIO::fseek64(m_ReadFileHandle, m_FileStartOffset+destOffs, SEEK_SET);
	FileIO::fread(m_Buffer + destOffs - m_ReadOffset, 1, chunkLen, m_ReadFileHandle);
}

byte *Serialiser::AllocAlignedBuffer(size_t size, size_t alignment)
{
	byte *rawAlloc = NULL;
	
#if defined(__EXCEPTIONS) || defined(_CPPUNWIND)
	try
#endif
	{
		rawAlloc = new byte[size+sizeof(byte*)+alignment];
	}
#if defined(__EXCEPTIONS) || defined(_CPPUNWIND)
	catch(std::bad_alloc&)
	{
		rawAlloc = NULL;
	}
#endif

	if(rawAlloc == NULL)
		RDCFATAL("Allocation for %llu bytes failed", (uint64_t)size);

	RDCASSERT(rawAlloc);

	byte *alignedAlloc = (byte *)AlignUp((size_t)(rawAlloc+sizeof(byte*)), alignment);

	byte **realPointer = (byte **)alignedAlloc;
	realPointer[-1] = rawAlloc;

	return alignedAlloc;
}

void Serialiser::FreeAlignedBuffer(byte *buf)
{
	if(buf == NULL) return;

	byte **realPointer = (byte **)buf;
	byte *rawAlloc = realPointer[-1];

	delete[] rawAlloc;
}

void Serialiser::InitCallstackResolver()
{
	if(m_pResolver == NULL && m_ResolverThread == 0)
	{
		m_ResolverThreadKillSignal = false;
		m_ResolverThread = Threading::CreateThread(&Serialiser::CreateResolver, (void *)this);
	}
}

void Serialiser::SetCallstack(uint64_t *levels, size_t numLevels)
{
	SAFE_DELETE(m_pCallstack);

	if(levels != NULL && numLevels != 0)
		m_pCallstack = Callstack::Load(levels, numLevels);
}

void Serialiser::CreateResolver(void *ths)
{
	Serialiser *ser = (Serialiser *)ths;
	FILE *binFile = FileIO::fopen(ser->m_Filename.c_str(), "rb");

	if(!binFile)
	{
		RDCERR("Can't open capture file '%s' for read - errno %d", ser->m_Filename.c_str(), errno);
		return;
	}
	
	DebuggerHeader header;
	FileIO::fread(&header, 1, sizeof(DebuggerHeader), binFile);

	if(header.magic != MAGIC_HEADER)
	{
		RDCERR("Invalid capture file. Expected 0x%08lx, got 0x%08llx", MAGIC_HEADER, header.magic);
		FileIO::fclose(binFile);
		return;
	}

	if(header.version != SERIALISE_VERSION)
	{
		RDCERR("Capture file from wrong version. This program is logfile version %llu, file is logfile version %llu", SERIALISE_VERSION, header.version);
		FileIO::fclose(binFile);
		return;
	}

	FileIO::fseek64(binFile, 0, SEEK_END);

	uint64_t realLength = FileIO::ftell64(binFile);
	if(header.fileSize != realLength)
	{
		RDCERR("Truncated/overlong capture file. Expected length 0x08llx, got 0x08lx", header.fileSize, realLength);
		FileIO::fclose(binFile);
		return;
	}

	if(header.resolveDBSize == 0)
	{
		RDCWARN("Trying to create resolver when no resolve DB is in the capture file");
		FileIO::fclose(binFile);
		return;
	}

	FileIO::fseek64(binFile, sizeof(DebuggerHeader), SEEK_SET);

	RDCASSERT(header.resolveDBSize < 0xffffffff);

	char *resolveDB = new char[(size_t)header.resolveDBSize];

	FileIO::fread(resolveDB, 1, (size_t)header.resolveDBSize, binFile);

	FileIO::fclose(binFile);
		
	string dir = dirname(ser->m_Filename);

	Callstack::StackResolver *resolver = Callstack::MakeResolver(resolveDB, (size_t)header.resolveDBSize, dir, &ser->m_ResolverThreadKillSignal);

	ser->m_pResolver = resolver;

	SAFE_DELETE_ARRAY(resolveDB);
}

uint64_t Serialiser::FlushToDisk()
{
	if(m_Filename != "" && !m_HasError && m_Mode == WRITING)
	{
		RDCDEBUG("writing capture files");

		if(m_DebugEnabled && !m_DebugText.empty())
		{
			FILE *dbgFile = FileIO::fopen((m_Filename + ".txt").c_str(), "wb");

			if(!dbgFile)
			{
				RDCERR("Can't open debug capture file '%s'", (m_Filename + ".txt").c_str());
			}
			else
			{
				FileIO::fwrite(m_DebugText.c_str(), 1, m_DebugText.length(), dbgFile);

				FileIO::fclose(dbgFile);
			}
		}

		FILE *binFile = FileIO::fopen(m_Filename.c_str(), "wb");

		if(!binFile)
		{
			RDCERR("Can't open capture file '%s' for write, errno %d", m_Filename.c_str(), errno);
			m_ErrorCode = eSerError_FileIO;
			m_HasError = true;
			return 0;
		}

		RDCDEBUG("Opened capture file for write");

		DebuggerHeader header;
		
		char *symbolDB = NULL;
		size_t symbolDBSize = 0;

		if(RenderDoc::Inst().GetCaptureOptions().CaptureCallstacks ||
			RenderDoc::Inst().GetCaptureOptions().CaptureCallstacksOnlyDraws)
		{
			// get symbol database
			Callstack::GetLoadedModules(symbolDB, symbolDBSize);

			symbolDB = new char[symbolDBSize];
			symbolDBSize = 0;

			Callstack::GetLoadedModules(symbolDB, symbolDBSize);
		}

		header.resolveDBSize = symbolDBSize;

		// write header
		FileIO::fwrite(&header, 1, sizeof(DebuggerHeader), binFile);
		
		// write symbol database
		if(symbolDBSize > 0)
			FileIO::fwrite(symbolDB, 1, symbolDBSize, binFile);
		
		static const byte padding[BufferAlignment] = {0};

		size_t offs = sizeof(DebuggerHeader) + symbolDBSize;
		size_t alignedoffs = AlignUp16(offs);

		FileIO::fwrite(padding, 1, alignedoffs-offs, binFile);

		offs = alignedoffs;

		// write serialise contents
		for(size_t i=0; i < m_Chunks.size(); i++)
		{
			Chunk *chunk = m_Chunks[i];

			alignedoffs = AlignUp16(offs);

			if(offs != alignedoffs && chunk->IsAligned())
			{
				uint16_t chunkIdx = 0; // write a '0' chunk that indicates special behaviour
				FileIO::fwrite(&chunkIdx, sizeof(chunkIdx), 1, binFile);
				offs += sizeof(chunkIdx);

				uint8_t controlByte = 0; // control byte 0 indicates padding
				FileIO::fwrite(&controlByte, sizeof(controlByte), 1, binFile);
				offs += sizeof(controlByte);

				offs++; // we will have to write out a byte indicating how much padding exists, so add 1
				alignedoffs = AlignUp16(offs);

				RDCCOMPILE_ASSERT(BufferAlignment < 0x100, "Buffer alignment must be less than 256"); // with a byte at most indicating how many bytes to pad,
				// this is our maximal representable alignment

				uint8_t padLength = (alignedoffs-offs)&0xff;
				FileIO::fwrite(&padLength, sizeof(padLength), 1, binFile);

				// we might have padded with the control bytes, so only write some bytes if we need to
				if(padLength > 0)
				{
					FileIO::fwrite(padding, 1, alignedoffs-offs, binFile);
					offs += alignedoffs-offs;
				}
			}
			
			FileIO::fwrite(chunk->GetData(), 1, chunk->GetLength(), binFile);

			offs += chunk->GetLength();

			if(chunk->IsTemporary())
				SAFE_DELETE(chunk);
		}

		m_Chunks.clear();
		
		header.fileSize = offs;

		FileIO::fseek64(binFile, 0, SEEK_SET);
		
		// write header with correct filesize (accounting for all padding)
		FileIO::fwrite(&header, 1, sizeof(DebuggerHeader), binFile);

		FileIO::fclose(binFile);

		SAFE_DELETE_ARRAY(symbolDB);

		return header.fileSize;
	}

	return 0;
}

Serialiser::~Serialiser()
{
	if(m_ResolverThread != 0)
	{
		m_ResolverThreadKillSignal = true;
		Threading::JoinThread(m_ResolverThread);
		Threading::CloseThread(m_ResolverThread);
		m_ResolverThread = 0;
	}

	if(m_ReadFileHandle)
	{
		FileIO::fclose(m_ReadFileHandle);
		m_ReadFileHandle = 0;
	}
	
	for(size_t i=0; i < m_Chunks.size(); i++)
	{
		if(m_Chunks[i]->IsTemporary())
			SAFE_DELETE(m_Chunks[i]);
	}

	m_Chunks.clear();

	SAFE_DELETE(m_pResolver);
	SAFE_DELETE(m_pCallstack);
	if(m_Buffer)
	{
		FreeAlignedBuffer(m_Buffer);
		m_Buffer = NULL;
	}
	m_Buffer = NULL;
	m_BufferHead = NULL;
}

void Serialiser::DebugPrint(const char *fmt, ...)
{
	if(m_HasError)
	{
		RDCERR("Debug printing with error state serialiser");
		return;
	}

	char tmpBuf[1024];

	va_list args;
	va_start(args, fmt);
	StringFormat::vsnprintf( tmpBuf, 1023, fmt, args );
	tmpBuf[1023] = '\0';
	va_end(args);
	
	m_DebugText += GetIndent();
	m_DebugText += tmpBuf;

#ifdef DEBUG_TEXT_SERIALISER
	FILE *f = FileIO::fopen(m_Filename.c_str(), "ab");

	if(f)
	{
		FileIO::fwrite(tmpBuf, 1, strlen(tmpBuf), f);

		FileIO::fclose(f);
	}
#endif
}

uint32_t Serialiser::PushContext(const char *name, uint32_t chunkIdx, bool smallChunk)
{
	// if writing, and chunkidx isn't 0 (debug non-scope), then either we're nested
	// or we should be writing into the start of the serialiser. A serialiser should
	// only ever have one chunk in it
	RDCASSERT(m_Mode < WRITING || m_Indent > 0 || GetOffset() == 0 || chunkIdx == 0);

	// we should not be pushing contexts directly into a file serialiser
	RDCASSERT(m_Mode < WRITING || m_Filename.empty());

	if(m_Mode >= WRITING)
	{
		if(chunkIdx > 0)
		{
			uint16_t c = chunkIdx&0x3fff;
			RDCASSERT(chunkIdx <= 0x3fff);

			/////////////////

			Callstack::Stackwalk *call = NULL;

			if(m_Indent == 0)
			{
				if(RenderDoc::Inst().GetCaptureOptions().CaptureCallstacks &&
					!RenderDoc::Inst().GetCaptureOptions().CaptureCallstacksOnlyDraws)
				{			
					call = Callstack::Collect();

					RDCASSERT(call->NumLevels() < 0xff);
				}
			}

			if(call)
				c |= 0x8000;
			if(smallChunk)
				c |= 0x4000;

			WriteFrom(c);

			if(call)
			{
				uint8_t numLevels = call->NumLevels()&0xff;
				WriteFrom(numLevels);

				if(call->NumLevels())
				{
					WriteBytes((byte *)call->GetAddrs(), sizeof(uint64_t)*numLevels);
				}

				SAFE_DELETE(call);
			}
			
			// will be fixed up in PopContext
			if(smallChunk)
			{
				uint16_t chunkSize = 0xbeeb;
				m_ChunkFixups.push_back(0x8000000000000000ULL | GetOffset());
				WriteFrom(chunkSize);
			}
			else
			{
				uint32_t chunkSize = 0xbeebfeed;
				m_ChunkFixups.push_back(GetOffset() & ~0x8000000000000000ULL);
				WriteFrom(chunkSize);
			}
		}

		if(m_DebugTextWriting)
		{
			DebugPrint("%s (%d)\n", name, chunkIdx);
			DebugPrint("{\n");
		}
	}
	else
	{
		// reset debug text
		m_DebugText = "";

		if(chunkIdx > 0)
		{
			uint16_t c = 0;
			ReadInto(c);

			// chunk index 0 is not allowed in normal situations.
			// allows us to indicate some control bytes
			while(c == 0)
			{
				uint8_t *controlByte = (uint8_t *)ReadBytes(1);

				if(*controlByte == 0x0)
				{
					// padding
					uint8_t *padLength = (uint8_t *)ReadBytes(1);

					// might have padded with these 5 control bytes,
					// so a pad length of 0 IS VALID.
					if(*padLength > 0)
					{
						ReadBytes((size_t)*padLength);
					}
				}
				else
				{
					RDCERR("Unexpected control byte: %x", (uint32_t)*controlByte);
				}
				
				ReadInto(c);
			}
		
			chunkIdx = c&0x3fff;	
			bool callstack = (c&0x8000) > 0;
			bool smallchunk = (c&0x4000) > 0;

			/////////////////
			
			if(m_Indent == 0)
			{
				if(callstack)
				{
					uint8_t callLen = 0;
					ReadInto(callLen);

					uint64_t *calls = (uint64_t *)ReadBytes(callLen*sizeof(uint64_t));
					SetCallstack(calls, callLen);
				}
				else
				{
					SetCallstack(NULL, 0);
				}
			}

			/////////////////

			if(smallchunk)
			{
				uint16_t miniSize = 0xbeeb;
				ReadInto(miniSize);

				m_LastChunkLen = miniSize;
			}
			else
			{
				uint32_t chunkSize = 0xbeebfeed;
				ReadInto(chunkSize);

				m_LastChunkLen = chunkSize;
			}
		}

		if(!name && m_ChunkLookup)
			name = m_ChunkLookup(chunkIdx);
		
		if(m_DebugTextWriting)
		{
			DebugPrint("%s\n", name ? name : "Unknown");
			DebugPrint("{\n");
		}
	}

	m_Indent++;

	return chunkIdx;
}

void Serialiser::PopContext(const char *name, uint32_t chunkIdx)
{
	m_Indent = RDCMAX(m_Indent-1, 0);

	if(m_Mode >= WRITING)
	{
		if(chunkIdx > 0 && m_Mode == WRITING)
		{
			// fix up the latest PushContext (guaranteed to match this one as Pushes and Pops match)
			RDCASSERT(!m_ChunkFixups.empty());

			uint64_t chunkOffset = m_ChunkFixups.back();m_ChunkFixups.pop_back();

			bool smallchunk = (chunkOffset & 0x8000000000000000ULL) > 0;
			chunkOffset &= ~0x8000000000000000ULL;

			uint64_t curOffset = GetOffset();

			RDCASSERT(curOffset > chunkOffset);

			uint64_t chunkLength = (curOffset-chunkOffset) - (smallchunk ? sizeof(uint16_t) : sizeof(uint32_t));

			RDCASSERT(chunkLength < 0xffffffff);

			uint32_t chunklen = (uint32_t)chunkLength;

			byte *head = m_BufferHead;
			SetOffset(chunkOffset);
			if(smallchunk)
			{
				uint16_t miniSize = (chunklen&0xffff);
				RDCASSERT(chunklen <= 0xffff);
				WriteFrom(miniSize);
			}
			else
			{
				WriteFrom(chunklen);
			}
			m_BufferHead = head;
		}
		
		if(m_DebugTextWriting)
			DebugPrint("} // %s\n", name);
	}
	else
	{
		if(m_DebugTextWriting)
			DebugPrint("}\n");
	}
}

/////////////////////////////////////////////////////////////
// Serialise functions

/////////////////////////////////////////////////////////////
// generic

void Serialiser::SerialiseString(const char *name, string &el)
{
	uint32_t len = (uint32_t)el.length();

	Serialise(NULL, len);

	if(m_Mode == READING)
		el.resize(len);

	if(m_Mode >= WRITING)
	{
		WriteBytes((byte *)el.c_str(), len);
		
		if(m_DebugTextWriting)
		{
			string s = el;
			if(s.length() > 64)
				s = s.substr(0, 60) + "...";
			DebugPrint("%s: \"%s\"\n", name, s.c_str());
		}
	}
	else
	{
		memcpy(&el[0], ReadBytes(len), len);
		
		if(m_DebugTextWriting)
		{
			string s = el;
			if(s.length() > 64)
				s = s.substr(0, 60) + "...";
			DebugPrint("%s: \"%s\"\n", name, s.c_str());
		}
	}
}

void Serialiser::Insert(Chunk *chunk)
{
	m_Chunks.push_back(chunk);

	m_DebugText += chunk->GetDebugString();
}

void Serialiser::SkipBuffer()
{
	RDCASSERT(m_Mode < WRITING);

	uint32_t len;
	ReadInto(len);

	// ensure byte alignment
	uint64_t offs = GetOffset();
	uint64_t alignedoffs = AlignUp16(offs);

	if(offs != alignedoffs)
	{
		ReadBytes((size_t)(alignedoffs-offs));
	}

	ReadBytes(len);
}

void Serialiser::AlignNextBuffer(const size_t alignment)
{
	// this is a super hack but it's the easiest way to align a buffer to a larger pow2 alignment
	// than the default 16-bytes, while still able to be backwards compatible with old logs that
	// weren't so aligned. We know that SerialiseBuffer will align to the nearest 16-byte boundary
	// after serialising 4 bytes of length, so we pad up to exactly 4 bytes before the desired
	// alignment, then after the 4 byte length there's nothing for the other padding to do.
	//
	// Note the chunk still needs to be aligned when the memory is allocated - this just ensures
	// the offset from the start is also aligned

	uint32_t len = 0;

	if(m_Mode >= WRITING)
	{
		// add sizeof(uint32_t) since we'll be serialising out how much padding is here,
		// then another sizeof(uint32_t) so we're aligning the offset after the buffer's
		// serialised length
		uint64_t curoffs = GetOffset() + sizeof(uint32_t)*2;
		uint64_t alignedoffs = AlignUp(curoffs, (uint64_t)alignment);

		len = uint32_t(alignedoffs - curoffs);
	}

	// avoid dynamically allocating
	RDCASSERT(alignment <= 128);
	byte padding[128] = {0};
	byte *p = &padding[0];

	if(m_Mode >= WRITING)
	{
		WriteFrom(len);
		WriteBytes(&padding[0], (size_t)len);
	}
	else
	{
		ReadInto(len);
		ReadBytes(len);
	}
}

void Serialiser::SerialiseBuffer(const char *name, byte *&buf, size_t &len)
{
	uint32_t bufLen = (uint32_t)len;

	if(m_Mode >= WRITING)
	{
		WriteFrom(bufLen);

		// ensure byte alignment
		uint64_t offs = GetOffset();
		uint64_t alignedoffs = AlignUp16(offs);

		if(offs != alignedoffs)
		{
			static const byte padding[BufferAlignment] = {0};
			WriteBytes(&padding[0], (size_t)(alignedoffs-offs));
		}
		
		RDCASSERT((GetOffset()%BufferAlignment)==0);

		WriteBytes(buf, bufLen);

		m_AlignedData = true;
	}
	else
	{
		ReadInto(bufLen);

		// ensure byte alignment
		uint64_t offs = GetOffset();
		uint64_t alignedoffs = AlignUp16(offs);
		
		if(offs != alignedoffs)
		{
			ReadBytes((size_t)(alignedoffs-offs));
		}

		if(buf == NULL)
			buf = new byte[bufLen];
		memcpy(buf, ReadBytes(bufLen), bufLen);
	}

	len = (size_t)bufLen;
	
	if(m_DebugTextWriting && name && name[0])
	{
		const char *ellipsis = "...";

		float *fbuf = new float[4];
		fbuf[0] = fbuf[1] = fbuf[2] = fbuf[3] = 0.0f;
		uint32_t *lbuf = (uint32_t *)fbuf;

		memcpy(fbuf, buf, RDCMIN(len, 4*sizeof(float)));

		if(bufLen <= 16)
		{
			ellipsis = "   ";
		}
		
		DebugPrint("%s: RawBuffer % 5d:< 0x%08x 0x%08x 0x%08x 0x%08x %s   %  8.4ff %  8.4ff %  8.4ff %  8.4ff %s >\n"
						, name
						, bufLen, lbuf[0], lbuf[1], lbuf[2], lbuf[3], ellipsis
						, fbuf[0], fbuf[1], fbuf[2], fbuf[3], ellipsis);

		SAFE_DELETE_ARRAY(fbuf);
	}

}

template<> void Serialiser::Serialise(const char *name, string &el)
{
	SerialiseString(name, el);
}

// floats need aligned reads
template<> void Serialiser::ReadInto(float &f)
{
	if(m_HasError)
	{
		RDCERR("Reading into with error state serialiser");
		return;
	}

	char *data = (char *)ReadBytes(sizeof(float));

	memcpy(&f, data, sizeof(float));
}

/////////////////////////////////////////////////////////////
// String conversions for debug log.

/////////////////////////////////////////////////////////////
// Basic types

template<>
string ToStrHelper<false, void *>::Get(void* const &el)
{
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "0x%p", el);

	return tostrBuf;
}

template<>
string ToStrHelper<false, int64_t>::Get(const int64_t &el)
{
	char tostrBuf[256] = {0};

	StringFormat::snprintf(tostrBuf, 255, "%lld", el);

	return tostrBuf;
}

template<>
string ToStrHelper<false, uint64_t>::Get(const uint64_t &el)
{
	char tostrBuf[256] = {0};

	StringFormat::snprintf(tostrBuf, 255, "%llu", el);

	return tostrBuf;
}

template<>
string ToStrHelper<false, uint32_t>::Get(const uint32_t &el)
{
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "%u", el);

	return tostrBuf;
}

template<>
string ToStrHelper<false, char>::Get(const char &el)
{
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "'%c'", el);

	return tostrBuf;
}

template<>
string ToStrHelper<false, wchar_t>::Get(const wchar_t &el)
{
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "'%lc'", el);

	return tostrBuf;
}

template<>
string ToStrHelper<false, byte>::Get(const byte &el)
{
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "%08hhb", el);

	return tostrBuf;
}

template<>
string ToStrHelper<false, uint16_t>::Get(const uint16_t &el)
{
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "%04d", el);

	return tostrBuf;
}

template<>
string ToStrHelper<false, int32_t>::Get(const int &el)
{
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "%d", el);

	return tostrBuf;
}

template<>
string ToStrHelper<false, int16_t>::Get(const short &el)
{
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "%04d", el);

	return tostrBuf;
}

template<>
string ToStrHelper<false, float>::Get(const float &el)
{
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "%0.4f", el);

	return tostrBuf;
}

template<>
string ToStrHelper<false, double>::Get(const double &el)
{
	char tostrBuf[256] = {0};
	StringFormat::snprintf(tostrBuf, 255, "%0.4lf", el);

	return tostrBuf;
}

template<>
string ToStrHelper<false, bool>::Get(const bool &el)
{
	if(el)
		return "True";

	return "False";
}
