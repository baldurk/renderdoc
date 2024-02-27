/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#if ENABLED(ENABLE_UNIT_TESTS)

#include "catch/catch.hpp"

void WriteAllBasicTypes(WriteSerialiser &ser)
{
  int64_t a = -1;
  uint64_t b = 2;
  int32_t c = -3;
  uint32_t d = 4;
  int16_t e = -5;
  uint16_t f = 6;
  int8_t g = -7;
  uint8_t h = 8;

  bool i = true;

  char j = 'j';

  double k = 11.11011011;
  float l = 12.12012012f;

  rdcstr m = "mmmm";
  char n[5] = "nnnn";
  const char *s = "ssss";

  int t[4] = {20, 20, 20, 20};

  SERIALISE_ELEMENT(a);
  SERIALISE_ELEMENT(b);
  SERIALISE_ELEMENT(c);
  SERIALISE_ELEMENT(d);
  SERIALISE_ELEMENT(e);
  SERIALISE_ELEMENT(f);
  SERIALISE_ELEMENT(g);
  SERIALISE_ELEMENT(h);
  SERIALISE_ELEMENT(i);
  SERIALISE_ELEMENT(j);
  SERIALISE_ELEMENT(k);
  SERIALISE_ELEMENT(l);
  SERIALISE_ELEMENT(m);
  SERIALISE_ELEMENT(n);
  SERIALISE_ELEMENT(s);
  SERIALISE_ELEMENT(t);
}

TEST_CASE("Read/write basic types", "[serialiser][structured]")
{
  StreamWriter *buf = new StreamWriter(StreamWriter::DefaultScratchSize);

  // write basic types, verify that we didn't write too much (rough factor of total data size +
  // overhead - it's OK to update this value if serialisation changed as long as it's incremental).
  {
    WriteSerialiser ser(buf, Ownership::Nothing);

    {
      SCOPED_SERIALISE_CHUNK(5);

      WriteAllBasicTypes(ser);
    }

    CHECK(buf->GetOffset() <= 128);

    REQUIRE_FALSE(ser.IsErrored());
  }

  {
    ReadSerialiser ser(new StreamReader(buf->GetData(), buf->GetOffset()), Ownership::Stream);

    uint32_t chunkID = ser.ReadChunk<uint32_t>();

    CHECK(chunkID == 5);

    int64_t a;
    uint64_t b;
    int32_t c;
    uint32_t d;
    int16_t e;
    uint16_t f;
    int8_t g;
    uint8_t h;

    bool i;

    char j;

    double k;
    float l;

    rdcstr m;
    char n[5];
    const char *s;

    int t[4] = {0, 0, 0, 0};

    SERIALISE_ELEMENT(a);
    SERIALISE_ELEMENT(b);
    SERIALISE_ELEMENT(c);
    SERIALISE_ELEMENT(d);
    SERIALISE_ELEMENT(e);
    SERIALISE_ELEMENT(f);
    SERIALISE_ELEMENT(g);
    SERIALISE_ELEMENT(h);
    SERIALISE_ELEMENT(i);
    SERIALISE_ELEMENT(j);
    SERIALISE_ELEMENT(k);
    SERIALISE_ELEMENT(l);
    SERIALISE_ELEMENT(m);
    SERIALISE_ELEMENT(n);
    SERIALISE_ELEMENT(s);
    SERIALISE_ELEMENT(t);

    ser.EndChunk();

    REQUIRE_FALSE(ser.IsErrored());

    CHECK(ser.GetReader()->AtEnd());

    CHECK(a == -1);
    CHECK(b == 2);
    CHECK(c == -3);
    CHECK(d == 4);
    CHECK(e == -5);
    CHECK(f == 6);
    CHECK(g == -7);
    CHECK(h == 8);

    CHECK(i == true);

    CHECK(j == 'j');

    CHECK(k == 11.11011011);
    CHECK(l == 12.12012012f);

    CHECK(m == "mmmm");
    CHECK(rdcstr(n) == "nnnn");
    CHECK(rdcstr(s) == "ssss");

    CHECK(t[0] == 20);
    CHECK(t[1] == 20);
    CHECK(t[2] == 20);
    CHECK(t[3] == 20);
  }

  delete buf;
};

TEST_CASE("Read/write via structured of basic types", "[serialiser]")
{
  StreamWriter *buf = new StreamWriter(StreamWriter::DefaultScratchSize);

  // write basic types, verify that we didn't write too much (rough factor of total data size +
  // overhead - it's OK to update this value if serialisation changed as long as it's incremental).
  {
    WriteSerialiser ser(buf, Ownership::Nothing);
    ser.WriteChunk(5);
    WriteAllBasicTypes(ser);
    ser.EndChunk();
  }

  {
    ReadSerialiser ser(new StreamReader(buf->GetData(), buf->GetOffset()), Ownership::Stream);

    ChunkLookup testChunkLoop = [](uint32_t) -> rdcstr { return "TestChunk"; };

    ser.ConfigureStructuredExport(testChunkLoop, true, 0, 1.0);

    int64_t a;
    uint64_t b;
    int32_t c;
    uint32_t d;
    int16_t e;
    uint16_t f;
    int8_t g;
    uint8_t h;

    bool i;

    char j;

    double k;
    float l;

    rdcstr m;
    char n[5];
    const char *s;

    int t[4];

    ser.ReadChunk<uint32_t>();

    SERIALISE_ELEMENT(a);
    SERIALISE_ELEMENT(b);
    SERIALISE_ELEMENT(c);
    SERIALISE_ELEMENT(d);
    SERIALISE_ELEMENT(e);
    SERIALISE_ELEMENT(f);
    SERIALISE_ELEMENT(g);
    SERIALISE_ELEMENT(h);
    SERIALISE_ELEMENT(i);
    SERIALISE_ELEMENT(j);
    SERIALISE_ELEMENT(k);
    SERIALISE_ELEMENT(l);
    SERIALISE_ELEMENT(m);
    SERIALISE_ELEMENT(n);
    SERIALISE_ELEMENT(s);
    SERIALISE_ELEMENT(t);

    ser.EndChunk();

    REQUIRE_FALSE(ser.IsErrored());

    CHECK(ser.GetReader()->AtEnd());

    const SDFile &structFile = ser.GetStructuredFile();

    CHECK(structFile.chunks.size() == 1);
    CHECK(structFile.buffers.size() == 0);

    REQUIRE(structFile.chunks[0]);

    const SDChunk &chunk = *structFile.chunks[0];

    CHECK(chunk.name == testChunkLoop(0));
    CHECK(chunk.metadata.chunkID == 5);
    CHECK(chunk.metadata.length == chunk.type.byteSize);
    CHECK(chunk.metadata.length < ser.GetReader()->GetSize());
    CHECK(chunk.type.basetype == SDBasic::Chunk);
    CHECK(chunk.type.name == "Chunk");
    CHECK(chunk.NumChildren() == 16);

    for(const SDObject *o : chunk)
      REQUIRE(o);

    int childIdx = 0;

    {
      const SDObject &o = *chunk.GetChild(childIdx++);

      CHECK(o.name == "a");
      CHECK(o.type.name == "int64_t");
      CHECK(o.type.basetype == SDBasic::SignedInteger);
      CHECK(o.type.byteSize == 8);
      CHECK(o.type.flags == SDTypeFlags::NoFlags);

      CHECK(o.data.basic.i == -1);
    }

    {
      const SDObject &o = *chunk.GetChild(childIdx++);

      CHECK(o.name == "b");
      CHECK(o.type.name == "uint64_t");
      CHECK(o.type.basetype == SDBasic::UnsignedInteger);
      CHECK(o.type.byteSize == 8);
      CHECK(o.type.flags == SDTypeFlags::NoFlags);

      CHECK(o.data.basic.u == 2);
    }

    {
      const SDObject &o = *chunk.GetChild(childIdx++);

      CHECK(o.name == "c");
      CHECK(o.type.name == "int32_t");
      CHECK(o.type.basetype == SDBasic::SignedInteger);
      CHECK(o.type.byteSize == 4);
      CHECK(o.type.flags == SDTypeFlags::NoFlags);

      CHECK(o.data.basic.i == -3);
    }

    {
      const SDObject &o = *chunk.GetChild(childIdx++);

      CHECK(o.name == "d");
      CHECK(o.type.name == "uint32_t");
      CHECK(o.type.basetype == SDBasic::UnsignedInteger);
      CHECK(o.type.byteSize == 4);
      CHECK(o.type.flags == SDTypeFlags::NoFlags);

      CHECK(o.data.basic.u == 4);
    }

    {
      const SDObject &o = *chunk.GetChild(childIdx++);

      CHECK(o.name == "e");
      CHECK(o.type.name == "int16_t");
      CHECK(o.type.basetype == SDBasic::SignedInteger);
      CHECK(o.type.byteSize == 2);
      CHECK(o.type.flags == SDTypeFlags::NoFlags);

      CHECK(o.data.basic.i == -5);
    }

    {
      const SDObject &o = *chunk.GetChild(childIdx++);

      CHECK(o.name == "f");
      CHECK(o.type.name == "uint16_t");
      CHECK(o.type.basetype == SDBasic::UnsignedInteger);
      CHECK(o.type.byteSize == 2);
      CHECK(o.type.flags == SDTypeFlags::NoFlags);

      CHECK(o.data.basic.u == 6);
    }

    {
      const SDObject &o = *chunk.GetChild(childIdx++);

      CHECK(o.name == "g");
      CHECK(o.type.name == "int8_t");
      CHECK(o.type.basetype == SDBasic::SignedInteger);
      CHECK(o.type.byteSize == 1);
      CHECK(o.type.flags == SDTypeFlags::NoFlags);

      CHECK(o.data.basic.i == -7);
    }

    {
      const SDObject &o = *chunk.GetChild(childIdx++);

      CHECK(o.name == "h");
      CHECK(o.type.name == "uint8_t");
      CHECK(o.type.basetype == SDBasic::UnsignedInteger);
      CHECK(o.type.byteSize == 1);
      CHECK(o.type.flags == SDTypeFlags::NoFlags);

      CHECK(o.data.basic.u == 8);
    }

    {
      const SDObject &o = *chunk.GetChild(childIdx++);

      CHECK(o.name == "i");
      CHECK(o.type.name == "bool");
      CHECK(o.type.basetype == SDBasic::Boolean);
      CHECK(o.type.byteSize == 1);
      CHECK(o.type.flags == SDTypeFlags::NoFlags);

      CHECK(o.data.basic.b == true);
    }

    {
      const SDObject &o = *chunk.GetChild(childIdx++);

      CHECK(o.name == "j");
      CHECK(o.type.name == "char");
      CHECK(o.type.basetype == SDBasic::Character);
      CHECK(o.type.byteSize == 1);
      CHECK(o.type.flags == SDTypeFlags::NoFlags);

      CHECK(o.data.basic.c == 'j');
    }

    {
      const SDObject &o = *chunk.GetChild(childIdx++);

      CHECK(o.name == "k");
      CHECK(o.type.name == "double");
      CHECK(o.type.basetype == SDBasic::Float);
      CHECK(o.type.byteSize == 8);
      CHECK(o.type.flags == SDTypeFlags::NoFlags);

      CHECK(o.data.basic.d == 11.11011011);
    }

    {
      const SDObject &o = *chunk.GetChild(childIdx++);

      CHECK(o.name == "l");
      CHECK(o.type.name == "float");
      CHECK(o.type.basetype == SDBasic::Float);
      CHECK(o.type.byteSize == 4);
      CHECK(o.type.flags == SDTypeFlags::NoFlags);

      CHECK(o.data.basic.d == Approx(12.12012012));
    }

    {
      const SDObject &o = *chunk.GetChild(childIdx++);

      CHECK(o.name == "m");
      CHECK(o.type.name == "string");
      CHECK(o.type.basetype == SDBasic::String);
      CHECK(o.type.byteSize == 4);
      CHECK(o.type.flags == SDTypeFlags::NoFlags);

      CHECK(o.data.str == "mmmm");
    }

    {
      const SDObject &o = *chunk.GetChild(childIdx++);

      CHECK(o.name == "n");
      CHECK(o.type.name == "string");
      CHECK(o.type.basetype == SDBasic::String);
      CHECK(o.type.byteSize == 4);
      CHECK(o.type.flags == SDTypeFlags::NoFlags);

      CHECK(o.data.str == "nnnn");
    }

    {
      const SDObject &o = *chunk.GetChild(childIdx++);

      CHECK(o.name == "s");
      CHECK(o.type.name == "string");
      CHECK(o.type.basetype == SDBasic::String);
      CHECK(o.type.byteSize == 4);
      CHECK(o.type.flags == SDTypeFlags::NoFlags);

      CHECK(o.data.str == "ssss");
    }

    {
      const SDObject &o = *chunk.GetChild(childIdx++);

      CHECK(o.name == "t");
      CHECK(o.type.name == "int32_t");
      CHECK(o.type.basetype == SDBasic::Array);
      CHECK(o.type.byteSize == 4);
      CHECK(o.type.flags == SDTypeFlags::FixedArray);

      CHECK(o.NumChildren() == 4);

      CHECK(o.GetChild(0)->data.basic.i == 20);
      CHECK(o.GetChild(1)->data.basic.c == 20);
      CHECK(o.GetChild(2)->data.basic.c == 20);
      CHECK(o.GetChild(3)->data.basic.c == 20);
    }

    StreamWriter *rewriteBuf = new StreamWriter(StreamWriter::DefaultScratchSize);

    {
      WriteSerialiser rewrite(rewriteBuf, Ownership::Nothing);

      rewrite.WriteStructuredFile(structFile, NULL);
    }

    // must be bitwise identical to the original serialised data.
    REQUIRE(rewriteBuf->GetOffset() == buf->GetOffset());
    CHECK_FALSE(memcmp(rewriteBuf->GetData(), buf->GetData(), (size_t)rewriteBuf->GetOffset()));

    delete rewriteBuf;
  }

  delete buf;
};

TEST_CASE("Read/writing large buffers", "[serialiser]")
{
  rdcstr filename = FileIO::GetTempFolderFilename() + "/scratch.bin";

  bytebuf buffer;
  buffer.resize(40 * 1024 * 1024);
  for(size_t i = 0; i < buffer.size(); i++)
    buffer[i] = byte((rand() & 0xff0) >> 4);

  {
    WriteSerialiser ser(new StreamWriter(StreamWriter::DefaultScratchSize), Ownership::Stream);
    WriteSerialiser fileser(
        new StreamWriter(FileIO::fopen(filename, FileIO::WriteBinary), Ownership::Stream),
        Ownership::Stream);

    uint32_t dummy1 = 99;
    uint32_t dummy2 = 123;

    ser.WriteChunk(1);
    ser.Serialise("dummy"_lit, dummy1);
    ser.EndChunk();

    Chunk *c;

    c = Chunk::Create(ser, 1);
    c->Write(fileser);
    c->Delete();

    ser.WriteChunk(2);
    ser.Serialise("buffer"_lit, buffer);
    ser.EndChunk();

    c = Chunk::Create(ser, 1);
    c->Write(fileser);
    c->Delete();

    ser.WriteChunk(3);
    ser.Serialise("buffer"_lit, buffer);
    ser.EndChunk();

    c = Chunk::Create(ser, 1);
    c->Write(fileser);
    c->Delete();

    ser.WriteChunk(4);
    ser.Serialise("dummy"_lit, dummy2);
    ser.EndChunk();

    c = Chunk::Create(ser, 1);
    c->Write(fileser);
    c->Delete();
  }

  for(size_t pass = 0; pass < 2; pass++)
  {
    StreamReader reader(FileIO::fopen(filename, FileIO::ReadBinary));

    ReadSerialiser ser(&reader, Ownership::Nothing);

    uint32_t c = 0;

    c = ser.ReadChunk<uint32_t>();
    CHECK(c == 1);
    {
      uint32_t dummy = 0;
      ser.Serialise("dummy"_lit, dummy);

      CHECK(dummy == 99);
    }
    ser.EndChunk();

    CHECK(reader.GetOffset() == 64 * 1);

    c = ser.ReadChunk<uint32_t>();
    if(pass == 0)
    {
      CHECK(c == 2);

      bytebuf readbuf;
      ser.Serialise("buffer"_lit, readbuf);

      CHECK((readbuf == buffer));
    }
    else
    {
      ser.SkipCurrentChunk();
    }
    ser.EndChunk();

    CHECK(reader.GetOffset() == 40 * 1024 * 1024 + 64 * 2);

    c = ser.ReadChunk<uint32_t>();
    {
      CHECK(c == 3);

      bytebuf readbuf;
      ser.Serialise("buffer"_lit, readbuf);

      CHECK((readbuf == buffer));
    }
    ser.EndChunk();

    CHECK(reader.GetOffset() == 80 * 1024 * 1024 + 64 * 3);

    c = ser.ReadChunk<uint32_t>();
    CHECK(c == 4);
    {
      uint32_t dummy = 0;
      ser.Serialise("dummy"_lit, dummy);

      CHECK(dummy == 123);
    }
    ser.EndChunk();

    CHECK(reader.GetOffset() == 80 * 1024 * 1024 + 64 * 4);
  }

  FileIO::Delete(filename);
};

TEST_CASE("Read/write chunk metadata", "[serialiser]")
{
  StreamWriter *buf = new StreamWriter(StreamWriter::DefaultScratchSize);

  // write basic types, verify that we didn't write too much (rough factor of total data size +
  // overhead - it's OK to update this value if serialisation changed as long as it's incremental).
  {
    WriteSerialiser ser(buf, Ownership::Nothing);

    ser.SetChunkMetadataRecording(WriteSerialiser::ChunkCallstack | WriteSerialiser::ChunkDuration |
                                  WriteSerialiser::ChunkThreadID | WriteSerialiser::ChunkTimestamp);

    ser.ChunkMetadata().threadID = 12345ULL;
    ser.ChunkMetadata().durationMicro = 445566ULL;
    ser.ChunkMetadata().timestampMicro = 987654321ULL;
    ser.ChunkMetadata().callstack.resize(4);
    ser.ChunkMetadata().callstack[0] = 101;
    ser.ChunkMetadata().callstack[1] = 102;
    ser.ChunkMetadata().callstack[2] = 103;
    ser.ChunkMetadata().callstack[3] = 104;

    ser.WriteChunk(1);

    uint32_t dummy = 99;
    ser.Serialise("dummy"_lit, dummy);

    ser.EndChunk();

    REQUIRE_FALSE(ser.IsErrored());
  }

  {
    ReadSerialiser ser(new StreamReader(buf->GetData(), buf->GetOffset()), Ownership::Stream);

    ser.ReadChunk<uint32_t>();

    CHECK(ser.ChunkMetadata().threadID == 12345ULL);
    CHECK(ser.ChunkMetadata().durationMicro == 445566ULL);
    CHECK(ser.ChunkMetadata().timestampMicro == 987654321ULL);
    REQUIRE(ser.ChunkMetadata().callstack.size() == 4);
    CHECK(ser.ChunkMetadata().callstack[0] == 101);
    CHECK(ser.ChunkMetadata().callstack[1] == 102);
    CHECK(ser.ChunkMetadata().callstack[2] == 103);
    CHECK(ser.ChunkMetadata().callstack[3] == 104);

    ser.SkipCurrentChunk();

    ser.EndChunk();

    REQUIRE_FALSE(ser.IsErrored());

    CHECK(ser.GetReader()->AtEnd());
  }

  {
    ReadSerialiser ser(new StreamReader(buf->GetData(), buf->GetOffset()), Ownership::Stream);

    ChunkLookup testChunkLoop = [](uint32_t) -> rdcstr { return "TestChunk"; };

    ser.ConfigureStructuredExport(testChunkLoop, true, 0, 1.0);

    ser.ReadChunk<uint32_t>();

    uint32_t dummy;
    ser.Serialise("dummy"_lit, dummy);

    ser.EndChunk();

    SDChunkMetaData &md = ser.GetStructuredFile().chunks[0]->metadata;

    CHECK(md.threadID == 12345ULL);
    CHECK(md.durationMicro == 445566ULL);
    CHECK(md.timestampMicro == 987654321ULL);
    REQUIRE(md.callstack.size() == 4);
    CHECK(md.callstack[0] == 101);
    CHECK(md.callstack[1] == 102);
    CHECK(md.callstack[2] == 103);
    CHECK(md.callstack[3] == 104);

    REQUIRE_FALSE(ser.IsErrored());

    CHECK(ser.GetReader()->AtEnd());
  }

  delete buf;
};

TEST_CASE("Verify multiple chunks can be merged", "[serialiser][chunks]")
{
  StreamWriter *buf = new StreamWriter(StreamWriter::DefaultScratchSize);

  enum ChunkType
  {
    FLOAT4 = 5,
    TWO_INTS,
    BOOL_INT_FLOAT,
    STRING_AND_INT,
  };

  // write some chunks individually
  rdcarray<Chunk *> chunks;
  {
    WriteSerialiser ser(new StreamWriter(StreamWriter::DefaultScratchSize), Ownership::Stream);

    {
      SCOPED_SERIALISE_CHUNK(TWO_INTS);

      int first = 123;
      int second = 456;

      SERIALISE_ELEMENT(first);
      SERIALISE_ELEMENT(second);

      chunks.push_back(scope.Get());
      REQUIRE(chunks.back());
    }

    {
      SCOPED_SERIALISE_CHUNK(STRING_AND_INT);

      rdcstr s = "string in STRING_AND_INT";
      int i = 4096;

      SERIALISE_ELEMENT(s);
      SERIALISE_ELEMENT(i);

      chunks.push_back(scope.Get());
      REQUIRE(chunks.back());
    }

    REQUIRE_FALSE(ser.IsErrored());
    REQUIRE(chunks.size() == 2);
  }

  // now write the previous chunks, then some more in-line
  {
    WriteSerialiser ser(buf, Ownership::Nothing);

    for(Chunk *c : chunks)
      c->Write(ser);

    {
      SCOPED_SERIALISE_CHUNK(BOOL_INT_FLOAT);

      bool flag = false;
      int data = 10000;
      float value = 3.141592f;

      SERIALISE_ELEMENT(flag);
      SERIALISE_ELEMENT(data);
      SERIALISE_ELEMENT(value);
    }

    {
      SCOPED_SERIALISE_CHUNK(FLOAT4);

      float vec4[4] = {1.1f, 2.2f, 3.3f, 4.4f};

      SERIALISE_ELEMENT(vec4);
    }

    REQUIRE_FALSE(ser.IsErrored());
    CHECK(buf->GetOffset() <= 256);
  }

  for(Chunk *c : chunks)
    c->Delete();

  // now read the data "dynamically" and ensure it's all correct
  {
    ReadSerialiser ser(new StreamReader(buf->GetData(), buf->GetOffset()), Ownership::Stream);

    while(!ser.GetReader()->AtEnd())
    {
      uint32_t chunkID = ser.ReadChunk<uint32_t>();
      ChunkType chunk = (ChunkType)chunkID;

      switch(chunk)
      {
        case FLOAT4:
        {
          float vec4[4];

          SERIALISE_ELEMENT(vec4);

          CHECK(vec4[0] == 1.1f);
          CHECK(vec4[1] == 2.2f);
          CHECK(vec4[2] == 3.3f);
          CHECK(vec4[3] == 4.4f);
          break;
        }
        case TWO_INTS:
        {
          int first = 0;
          int second = 0;

          SERIALISE_ELEMENT(first);
          SERIALISE_ELEMENT(second);

          CHECK(first == 123);
          CHECK(second == 456);
          break;
        }
        case BOOL_INT_FLOAT:
        {
          bool flag = true;
          int data = 0;
          float value = 0.0f;

          SERIALISE_ELEMENT(flag);
          SERIALISE_ELEMENT(data);
          SERIALISE_ELEMENT(value);

          CHECK(flag == false);
          CHECK(data == 10000);
          CHECK(value == 3.141592f);
          break;
        }
        case STRING_AND_INT:
        {
          rdcstr s;
          int i = 0;

          SERIALISE_ELEMENT(s);
          SERIALISE_ELEMENT(i);

          CHECK(s == "string in STRING_AND_INT");
          CHECK(i == 4096);
          break;
        }
        default:
        {
          CAPTURE(chunkID);
          FAIL("Unexpected chunk type");
          break;
        }
      }

      ser.EndChunk();
    }
  }

  delete buf;
};

TEST_CASE("Read/write container types", "[serialiser][structured]")
{
  StreamWriter *buf = new StreamWriter(StreamWriter::DefaultScratchSize);

  // write basic types, verify that we didn't write too much (rough factor of total data size +
  // overhead - it's OK to update this value if serialisation changed as long as it's incremental).
  {
    WriteSerialiser ser(buf, Ownership::Nothing);

    {
      SCOPED_SERIALISE_CHUNK(5);

      rdcarray<int> v;
      rdcpair<float, rdcstr> p;

      v.push_back(1);
      v.push_back(1);
      v.push_back(2);
      v.push_back(3);
      v.push_back(5);
      v.push_back(8);

      p = {3.14159f, "M_PI"};

      SERIALISE_ELEMENT(v);
      SERIALISE_ELEMENT(p);
    }

    CHECK(buf->GetOffset() <= 128);

    REQUIRE_FALSE(ser.IsErrored());
  }

  {
    ReadSerialiser ser(new StreamReader(buf->GetData(), buf->GetOffset()), Ownership::Stream);

    uint32_t chunkID = ser.ReadChunk<uint32_t>();

    CHECK(chunkID == 5);

    rdcarray<int> v;
    rdcpair<float, rdcstr> p;

    SERIALISE_ELEMENT(v);
    SERIALISE_ELEMENT(p);

    ser.EndChunk();

    REQUIRE_FALSE(ser.IsErrored());

    CHECK(ser.GetReader()->AtEnd());

    REQUIRE(v.size() == 6);

    CHECK(v[0] == 1);
    CHECK(v[1] == 1);
    CHECK(v[2] == 2);
    CHECK(v[3] == 3);
    CHECK(v[4] == 5);
    CHECK(v[5] == 8);

    CHECK(p.first == 3.14159f);
    CHECK(p.second == "M_PI");
  }

  {
    ReadSerialiser ser(new StreamReader(buf->GetData(), buf->GetOffset()), Ownership::Stream);

    ser.ConfigureStructuredExport([](uint32_t) -> rdcstr { return "TestChunk"; }, true, 0, 1.0);

    ser.ReadChunk<uint32_t>();
    {
      rdcarray<int32_t> v;
      rdcpair<float, rdcstr> p;

      SERIALISE_ELEMENT(v);
      SERIALISE_ELEMENT(p);
    }
    ser.EndChunk();

    REQUIRE_FALSE(ser.IsErrored());

    CHECK(ser.GetReader()->AtEnd());

    const SDFile &structData = ser.GetStructuredFile();

    CHECK(structData.chunks.size() == 1);
    CHECK(structData.buffers.size() == 0);

    REQUIRE(structData.chunks[0]);

    const SDChunk &chunk = *structData.chunks[0];

    CHECK(chunk.NumChildren() == 2);

    for(const SDObject *o : chunk)
      REQUIRE(o);

    int childIdx = 0;

    {
      const SDObject &o = *chunk.GetChild(childIdx++);

      CHECK(o.name == "v");
      CHECK(o.type.basetype == SDBasic::Array);
      CHECK(o.type.byteSize == 6);
      CHECK(o.type.flags == SDTypeFlags::NoFlags);
      CHECK(o.NumChildren() == 6);

      for(const SDObject *child : o)
      {
        CHECK(child->type.basetype == SDBasic::SignedInteger);
        CHECK(child->type.byteSize == 4);
      }

      CHECK(o.GetChild(0)->data.basic.i == 1);
      CHECK(o.GetChild(1)->data.basic.i == 1);
      CHECK(o.GetChild(2)->data.basic.i == 2);
      CHECK(o.GetChild(3)->data.basic.i == 3);
      CHECK(o.GetChild(4)->data.basic.i == 5);
      CHECK(o.GetChild(5)->data.basic.i == 8);
    }

    {
      const SDObject &o = *chunk.GetChild(childIdx++);

      CHECK(o.name == "p");
      CHECK(o.type.name == "pair");
      CHECK(o.type.basetype == SDBasic::Struct);
      CHECK(o.type.byteSize == 2);
      CHECK(o.type.flags == SDTypeFlags::NoFlags);
      CHECK(o.NumChildren() == 2);

      {
        const SDObject &first = *o.GetChild(0);

        CHECK(first.name == "first");
        CHECK(first.type.name == "float");
        CHECK(first.type.basetype == SDBasic::Float);
        CHECK(first.type.byteSize == 4);
        CHECK(first.type.flags == SDTypeFlags::NoFlags);

        CHECK(first.data.basic.d == 3.14159f);
      }

      {
        const SDObject &second = *o.GetChild(1);

        CHECK(second.name == "second");
        CHECK(second.type.name == "string");
        CHECK(second.type.basetype == SDBasic::String);
        CHECK(second.type.byteSize == 4);
        CHECK(second.type.flags == SDTypeFlags::NoFlags);

        CHECK(second.data.str == "M_PI");
      }
    }

    StreamWriter *rewriteBuf = new StreamWriter(StreamWriter::DefaultScratchSize);

    {
      WriteSerialiser rewrite(rewriteBuf, Ownership::Nothing);

      rewrite.WriteStructuredFile(structData, NULL);
    }

    // must be bitwise identical to the original serialised data.
    REQUIRE(rewriteBuf->GetOffset() == buf->GetOffset());
    CHECK_FALSE(memcmp(rewriteBuf->GetData(), buf->GetData(), (size_t)rewriteBuf->GetOffset()));

    delete rewriteBuf;
  }

  delete buf;
};

TEST_CASE("Read/write container of container types", "[serialiser][structured]")
{
  StreamWriter *buf = new StreamWriter(StreamWriter::DefaultScratchSize);

  {
    WriteSerialiser ser(buf, Ownership::Nothing);

    {
      SCOPED_SERIALISE_CHUNK(5);

      rdcarray<rdcarray<int>> v;

      v.push_back({1, 2, 3});
      v.push_back({4, 5});
      v.push_back({6, 7, 8, 9});

      SERIALISE_ELEMENT(v);
    }

    CHECK(buf->GetOffset() <= 128);

    REQUIRE_FALSE(ser.IsErrored());
  }

  {
    ReadSerialiser ser(new StreamReader(buf->GetData(), buf->GetOffset()), Ownership::Stream);

    uint32_t chunkID = ser.ReadChunk<uint32_t>();

    CHECK(chunkID == 5);

    rdcarray<rdcarray<int>> v;

    SERIALISE_ELEMENT(v);

    ser.EndChunk();

    REQUIRE_FALSE(ser.IsErrored());

    CHECK(ser.GetReader()->AtEnd());

    REQUIRE(v.size() == 3);
    REQUIRE(v[0].size() == 3);
    REQUIRE(v[1].size() == 2);
    REQUIRE(v[2].size() == 4);

    CHECK(v[0][0] == 1);
    CHECK(v[0][1] == 2);
    CHECK(v[0][2] == 3);
    CHECK(v[1][0] == 4);
    CHECK(v[1][1] == 5);
    CHECK(v[2][0] == 6);
    CHECK(v[2][1] == 7);
    CHECK(v[2][2] == 8);
    CHECK(v[2][3] == 9);
  }

  {
    ReadSerialiser ser(new StreamReader(buf->GetData(), buf->GetOffset()), Ownership::Stream);

    ser.ConfigureStructuredExport([](uint32_t) -> rdcstr { return "TestChunk"; }, true, 0, 1.0);

    ser.ReadChunk<uint32_t>();
    {
      rdcarray<rdcarray<int32_t>> v;

      SERIALISE_ELEMENT(v);
    }
    ser.EndChunk();

    REQUIRE_FALSE(ser.IsErrored());

    CHECK(ser.GetReader()->AtEnd());

    const SDFile &structData = ser.GetStructuredFile();

    CHECK(structData.chunks.size() == 1);
    CHECK(structData.buffers.size() == 0);

    REQUIRE(structData.chunks[0]);

    const SDChunk &chunk = *structData.chunks[0];

    CHECK(chunk.NumChildren() == 1);

    for(const SDObject *o : chunk)
      REQUIRE(o);

    int childIdx = 0;

    {
      const SDObject &o = *chunk.GetChild(childIdx++);

      CHECK(o.name == "v");
      CHECK(o.type.basetype == SDBasic::Array);
      CHECK(o.type.byteSize == 3);
      CHECK(o.type.flags == SDTypeFlags::NoFlags);
      CHECK(o.NumChildren() == 3);

      // v[0]
      {
        const SDObject *child = o.GetChild(0);
        CHECK(child->name == "$el");
        CHECK(child->type.basetype == SDBasic::Array);
        CHECK(child->type.byteSize == 3);

        for(size_t i = 0; i < child->NumChildren(); ++i)
        {
          const SDObject *grandChild = child->GetChild(i);

          CHECK(grandChild->type.basetype == SDBasic::SignedInteger);
          CHECK(grandChild->type.byteSize == 4);
        }

        CHECK(child->GetChild(0)->data.basic.i == 1);
        CHECK(child->GetChild(1)->data.basic.i == 2);
        CHECK(child->GetChild(2)->data.basic.i == 3);
      }

      // v[1]
      {
        const SDObject *child = o.GetChild(1);
        CHECK(child->name == "$el");
        CHECK(child->type.basetype == SDBasic::Array);
        CHECK(child->type.byteSize == 2);

        for(size_t i = 0; i < child->NumChildren(); ++i)
        {
          const SDObject *grandChild = child->GetChild(i);

          CHECK(grandChild->type.basetype == SDBasic::SignedInteger);
          CHECK(grandChild->type.byteSize == 4);
        }

        CHECK(child->GetChild(0)->data.basic.i == 4);
        CHECK(child->GetChild(1)->data.basic.i == 5);
      }

      // v[2]
      {
        const SDObject *child = o.GetChild(2);
        CHECK(child->name == "$el");
        CHECK(child->type.basetype == SDBasic::Array);
        CHECK(child->type.byteSize == 4);

        for(size_t i = 0; i < child->NumChildren(); ++i)
        {
          const SDObject *grandChild = child->GetChild(i);

          CHECK(grandChild->type.basetype == SDBasic::SignedInteger);
          CHECK(grandChild->type.byteSize == 4);
        }

        CHECK(child->GetChild(0)->data.basic.i == 6);
        CHECK(child->GetChild(1)->data.basic.i == 7);
        CHECK(child->GetChild(2)->data.basic.i == 8);
        CHECK(child->GetChild(3)->data.basic.i == 9);
      }
    }

    StreamWriter *rewriteBuf = new StreamWriter(StreamWriter::DefaultScratchSize);

    {
      WriteSerialiser rewrite(rewriteBuf, Ownership::Nothing);

      rewrite.WriteStructuredFile(structData, NULL);
    }

    // must be bitwise identical to the original serialised data.
    REQUIRE(rewriteBuf->GetOffset() == buf->GetOffset());
    CHECK_FALSE(memcmp(rewriteBuf->GetData(), buf->GetData(), (size_t)rewriteBuf->GetOffset()));

    delete rewriteBuf;
  }

  delete buf;
}

struct struct1
{
  struct1() : x(0.0f), y(0.0f), width(0.0f), height(0.0f) {}
  struct1(float X, float Y, float W, float H) : x(X), y(Y), width(W), height(H) {}
  float x, y, width, height;
};

DECLARE_REFLECTION_STRUCT(struct1);

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, struct1 &el)
{
  SERIALISE_MEMBER(x);
  SERIALISE_MEMBER(y);
  SERIALISE_MEMBER(width);
  SERIALISE_MEMBER(height);
}

struct struct2
{
  rdcstr name;
  rdcarray<float> floats;
  rdcarray<struct1> viewports;
};

DECLARE_REFLECTION_STRUCT(struct2);

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, struct2 &el)
{
  SERIALISE_MEMBER(name);
  SERIALISE_MEMBER(floats);
  SERIALISE_MEMBER(viewports);
}

enum MySpecialEnum
{
  FirstEnumValue,
  SecondEnumValue,
  AnotherEnumValue,
  TheLastEnumValue,
};

DECLARE_REFLECTION_ENUM(MySpecialEnum);

template <>
rdcstr DoStringise(const MySpecialEnum &el)
{
  BEGIN_ENUM_STRINGISE(MySpecialEnum);
  {
    STRINGISE_ENUM(FirstEnumValue);
    STRINGISE_ENUM(SecondEnumValue);
    STRINGISE_ENUM(AnotherEnumValue);
    STRINGISE_ENUM(TheLastEnumValue);
  }
  END_ENUM_STRINGISE();
}

enum MySpecialEnum8 : uint8_t
{
  AnotherEnum8Value = UINT8_MAX,
};

DECLARE_REFLECTION_ENUM(MySpecialEnum8);

template <>
rdcstr DoStringise(const MySpecialEnum8 &el)
{
  BEGIN_ENUM_STRINGISE(MySpecialEnum8);
  {
    STRINGISE_ENUM(AnotherEnum8Value);
  }
  END_ENUM_STRINGISE();
}

enum MySpecialEnum16 : uint16_t
{
  AnotherEnum16Value = UINT16_MAX,
};

DECLARE_REFLECTION_ENUM(MySpecialEnum16);

template <>
rdcstr DoStringise(const MySpecialEnum16 &el)
{
  BEGIN_ENUM_STRINGISE(MySpecialEnum16);
  {
    STRINGISE_ENUM(AnotherEnum16Value);
  }
  END_ENUM_STRINGISE();
}

enum MySpecialEnum64 : uint64_t
{
  AnotherEnum64Value = UINT64_MAX,
};

DECLARE_REFLECTION_ENUM(MySpecialEnum64);

template <>
rdcstr DoStringise(const MySpecialEnum64 &el)
{
  BEGIN_ENUM_STRINGISE(MySpecialEnum64);
  {
    STRINGISE_ENUM(AnotherEnum64Value);
  }
  END_ENUM_STRINGISE();
}

TEST_CASE("Read/write complex types", "[serialiser][structured]")
{
  StreamWriter *buf = new StreamWriter(StreamWriter::DefaultScratchSize);

  {
    WriteSerialiser ser(buf, Ownership::Nothing);

    SCOPED_SERIALISE_CHUNK(5);

    MySpecialEnum8 enum8Val = AnotherEnum8Value;
    SERIALISE_ELEMENT(enum8Val);

    MySpecialEnum16 enum16Val = AnotherEnum16Value;
    SERIALISE_ELEMENT(enum16Val);

    MySpecialEnum64 enum64Val = AnotherEnum64Value;
    SERIALISE_ELEMENT(enum64Val);

    MySpecialEnum enumVal = AnotherEnumValue;

    SERIALISE_ELEMENT(enumVal);

    rdcarray<MySpecialEnum> enumArray = {TheLastEnumValue, AnotherEnumValue, SecondEnumValue,
                                         FirstEnumValue, FirstEnumValue};

    SERIALISE_ELEMENT(enumArray);

    rdcarray<struct1> sparseStructArray;

    sparseStructArray.resize(10);

    sparseStructArray[5] = struct1(1.0f, 2.0f, 3.0f, 4.0f);
    sparseStructArray[8] = struct1(10.0f, 20.0f, 30.0f, 40.0f);

    SERIALISE_ELEMENT(sparseStructArray);

    struct2 complex;
    complex.name = "A complex object";
    complex.floats = {1.2f, 3.4f, 5.6f};
    complex.viewports.resize(4);
    complex.viewports[0] = struct1(512.0f, 0.0f, 256.0f, 256.0f);

    SERIALISE_ELEMENT(complex);

    const struct1 *inputParam1 = new struct1(9.0f, 9.9f, 9.99f, 9.999f);
    const struct1 *inputParam2 = NULL;

    SERIALISE_ELEMENT_OPT(inputParam1);
    SERIALISE_ELEMENT_OPT(inputParam2);

    CHECK(buf->GetOffset() <= 512);

    REQUIRE_FALSE(ser.IsErrored());

    delete inputParam1;
  }

  {
    ReadSerialiser ser(new StreamReader(buf->GetData(), buf->GetOffset()), Ownership::Stream);

    uint32_t chunkID = ser.ReadChunk<uint32_t>();

    CHECK(chunkID == 5);

    MySpecialEnum8 enum8Val;
    SERIALISE_ELEMENT(enum8Val);

    MySpecialEnum16 enum16Val;
    SERIALISE_ELEMENT(enum16Val);

    MySpecialEnum64 enum64Val;
    SERIALISE_ELEMENT(enum64Val);

    MySpecialEnum enumVal;

    SERIALISE_ELEMENT(enumVal);

    rdcarray<MySpecialEnum> enumArray;

    SERIALISE_ELEMENT(enumArray);

    rdcarray<struct1> sparseStructArray;

    SERIALISE_ELEMENT(sparseStructArray);

    struct2 complex;

    SERIALISE_ELEMENT(complex);

    const struct1 *inputParam1;
    const struct1 *inputParam2;

    SERIALISE_ELEMENT_OPT(inputParam1);
    SERIALISE_ELEMENT_OPT(inputParam2);

    ser.EndChunk();

    REQUIRE_FALSE(ser.IsErrored());

    CHECK(ser.GetReader()->AtEnd());

    CHECK(enum8Val == AnotherEnum8Value);
    CHECK(enum16Val == AnotherEnum16Value);
    CHECK(enum64Val == AnotherEnum64Value);
    CHECK(enumVal == AnotherEnumValue);

    CHECK(enumArray[0] == TheLastEnumValue);
    CHECK(enumArray[1] == AnotherEnumValue);
    CHECK(enumArray[2] == SecondEnumValue);
    CHECK(enumArray[3] == FirstEnumValue);
    CHECK(enumArray[4] == FirstEnumValue);

    CHECK(sparseStructArray[0].x == 0.0f);
    CHECK(sparseStructArray[0].y == 0.0f);
    CHECK(sparseStructArray[0].width == 0.0f);
    CHECK(sparseStructArray[0].height == 0.0f);

    CHECK(sparseStructArray[5].x == 1.0f);
    CHECK(sparseStructArray[5].y == 2.0f);
    CHECK(sparseStructArray[5].width == 3.0f);
    CHECK(sparseStructArray[5].height == 4.0f);

    CHECK(sparseStructArray[8].x == 10.0f);
    CHECK(sparseStructArray[8].y == 20.0f);
    CHECK(sparseStructArray[8].width == 30.0f);
    CHECK(sparseStructArray[8].height == 40.0f);

    CHECK(complex.name == "A complex object");
    REQUIRE(complex.floats.size() == 3);
    CHECK(complex.floats[0] == 1.2f);
    CHECK(complex.floats[1] == 3.4f);
    CHECK(complex.floats[2] == 5.6f);
    REQUIRE(complex.viewports.size() == 4);

    CHECK(complex.viewports[0].x == 512.0f);
    CHECK(complex.viewports[0].y == 0.0f);
    CHECK(complex.viewports[0].width == 256.0f);
    CHECK(complex.viewports[0].height == 256.0f);

    CHECK(inputParam1->x == 9.0f);
    CHECK(inputParam1->y == 9.9f);
    CHECK(inputParam1->width == 9.99f);
    CHECK(inputParam1->height == 9.999f);

    CHECK(inputParam2 == NULL);
  }

  {
    ReadSerialiser ser(new StreamReader(buf->GetData(), buf->GetOffset()), Ownership::Stream);

    ser.ConfigureStructuredExport([](uint32_t) -> rdcstr { return "TestChunk"; }, true, 0, 1.0);

    ser.ReadChunk<uint32_t>();
    {
      MySpecialEnum8 enum8Val;
      SERIALISE_ELEMENT(enum8Val);

      MySpecialEnum16 enum16Val;
      SERIALISE_ELEMENT(enum16Val);

      MySpecialEnum64 enum64Val;
      SERIALISE_ELEMENT(enum64Val);

      MySpecialEnum enumVal;

      SERIALISE_ELEMENT(enumVal);

      rdcarray<MySpecialEnum> enumArray;

      SERIALISE_ELEMENT(enumArray);

      rdcarray<struct1> sparseStructArray;

      SERIALISE_ELEMENT(sparseStructArray);

      struct2 complex;

      SERIALISE_ELEMENT(complex);

      const struct1 *inputParam1;
      const struct1 *inputParam2;

      SERIALISE_ELEMENT_OPT(inputParam1);
      SERIALISE_ELEMENT_OPT(inputParam2);
    }
    ser.EndChunk();

    REQUIRE_FALSE(ser.IsErrored());

    CHECK(ser.GetReader()->AtEnd());

    const SDFile &structData = ser.GetStructuredFile();

    REQUIRE(structData.chunks.size() == 1);
    CHECK(structData.buffers.size() == 0);

    REQUIRE(structData.chunks[0]);

    const SDChunk &chunk = *structData.chunks[0];

    REQUIRE(chunk.NumChildren() == 9);

    for(const SDObject *o : chunk)
      REQUIRE(o);

    int childIdx = 0;
    {
      const SDObject &o = *chunk.GetChild(childIdx++);

      CHECK(o.name == "enum8Val");
      CHECK(o.type.basetype == SDBasic::Enum);
      CHECK(o.type.byteSize == 1);
      CHECK(o.type.flags == SDTypeFlags::HasCustomString);

      CHECK(o.data.basic.u == AnotherEnum8Value);
      CHECK(o.data.str == "AnotherEnum8Value");
    }

    {
      const SDObject &o = *chunk.GetChild(childIdx++);

      CHECK(o.name == "enum16Val");
      CHECK(o.type.basetype == SDBasic::Enum);
      CHECK(o.type.byteSize == 2);
      CHECK(o.type.flags == SDTypeFlags::HasCustomString);

      CHECK(o.data.basic.u == AnotherEnum16Value);
      CHECK(o.data.str == "AnotherEnum16Value");
    }

    {
      const SDObject &o = *chunk.GetChild(childIdx++);

      CHECK(o.name == "enum64Val");
      CHECK(o.type.basetype == SDBasic::Enum);
      CHECK(o.type.byteSize == 8);
      CHECK(o.type.flags == SDTypeFlags::HasCustomString);

      CHECK(o.data.basic.u == AnotherEnum64Value);
      CHECK(o.data.str == "AnotherEnum64Value");
    }

    {
      const SDObject &o = *chunk.GetChild(childIdx++);

      CHECK(o.name == "enumVal");
      CHECK(o.type.basetype == SDBasic::Enum);
      CHECK(o.type.byteSize == 4);
      CHECK(o.type.flags == SDTypeFlags::HasCustomString);

      CHECK(o.data.basic.u == AnotherEnumValue);
      CHECK(o.data.str == "AnotherEnumValue");
    }

    {
      const SDObject &o = *chunk.GetChild(childIdx++);

      CHECK(o.name == "enumArray");
      CHECK(o.type.basetype == SDBasic::Array);
      REQUIRE(o.NumChildren() == 5);

      CHECK(o.GetChild(0)->type.basetype == SDBasic::Enum);
      CHECK(o.GetChild(0)->type.flags == SDTypeFlags::HasCustomString);
      CHECK(o.GetChild(0)->data.basic.u == TheLastEnumValue);
      CHECK(o.GetChild(0)->data.str == "TheLastEnumValue");

      CHECK(o.GetChild(1)->type.basetype == SDBasic::Enum);
      CHECK(o.GetChild(1)->type.flags == SDTypeFlags::HasCustomString);
      CHECK(o.GetChild(1)->data.basic.u == AnotherEnumValue);
      CHECK(o.GetChild(1)->data.str == "AnotherEnumValue");

      CHECK(o.GetChild(2)->type.basetype == SDBasic::Enum);
      CHECK(o.GetChild(2)->type.flags == SDTypeFlags::HasCustomString);
      CHECK(o.GetChild(2)->data.basic.u == SecondEnumValue);
      CHECK(o.GetChild(2)->data.str == "SecondEnumValue");

      CHECK(o.GetChild(3)->type.basetype == SDBasic::Enum);
      CHECK(o.GetChild(3)->type.flags == SDTypeFlags::HasCustomString);
      CHECK(o.GetChild(3)->data.basic.u == FirstEnumValue);
      CHECK(o.GetChild(3)->data.str == "FirstEnumValue");

      CHECK(o.GetChild(4)->type.basetype == SDBasic::Enum);
      CHECK(o.GetChild(4)->type.flags == SDTypeFlags::HasCustomString);
      CHECK(o.GetChild(4)->data.basic.u == FirstEnumValue);
      CHECK(o.GetChild(4)->data.str == "FirstEnumValue");
    }

    {
      const SDObject &o = *chunk.GetChild(childIdx++);

      CHECK(o.name == "sparseStructArray");
      CHECK(o.type.basetype == SDBasic::Array);
      CHECK(o.type.flags == SDTypeFlags::NoFlags);
      REQUIRE(o.NumChildren() == 10);

      for(const SDObject *child : o)
      {
        CHECK(child->type.basetype == SDBasic::Struct);
        CHECK(child->type.name == "struct1");
        CHECK(child->type.byteSize == sizeof(struct1));
        CHECK(child->NumChildren() == 4);
        CHECK(child->GetChild(0)->type.basetype == SDBasic::Float);
        CHECK(child->GetChild(0)->type.byteSize == 4);
        CHECK(child->GetChild(0)->name == "x");
        CHECK(child->GetChild(1)->type.basetype == SDBasic::Float);
        CHECK(child->GetChild(1)->type.byteSize == 4);
        CHECK(child->GetChild(1)->name == "y");
        CHECK(child->GetChild(2)->type.basetype == SDBasic::Float);
        CHECK(child->GetChild(2)->type.byteSize == 4);
        CHECK(child->GetChild(2)->name == "width");
        CHECK(child->GetChild(3)->type.basetype == SDBasic::Float);
        CHECK(child->GetChild(3)->type.byteSize == 4);
        CHECK(child->GetChild(3)->name == "height");
      }

      CHECK(o.GetChild(0)->GetChild(0)->data.basic.d == 0.0f);
      CHECK(o.GetChild(0)->GetChild(1)->data.basic.d == 0.0f);
      CHECK(o.GetChild(0)->GetChild(2)->data.basic.d == 0.0f);
      CHECK(o.GetChild(0)->GetChild(3)->data.basic.d == 0.0f);

      CHECK(o.GetChild(5)->GetChild(0)->data.basic.d == 1.0f);
      CHECK(o.GetChild(5)->GetChild(1)->data.basic.d == 2.0f);
      CHECK(o.GetChild(5)->GetChild(2)->data.basic.d == 3.0f);
      CHECK(o.GetChild(5)->GetChild(3)->data.basic.d == 4.0f);

      CHECK(o.GetChild(8)->GetChild(0)->data.basic.d == 10.0f);
      CHECK(o.GetChild(8)->GetChild(1)->data.basic.d == 20.0f);
      CHECK(o.GetChild(8)->GetChild(2)->data.basic.d == 30.0f);
      CHECK(o.GetChild(8)->GetChild(3)->data.basic.d == 40.0f);
    }

    {
      const SDObject &o = *chunk.GetChild(childIdx++);

      CHECK(o.name == "complex");
      CHECK(o.type.name == "struct2");
      CHECK(o.type.basetype == SDBasic::Struct);
      CHECK(o.type.byteSize == sizeof(struct2));
      CHECK(o.type.flags == SDTypeFlags::NoFlags);
      REQUIRE(o.NumChildren() == 3);

      {
        const SDObject &c = *o.GetChild(0);

        CHECK(c.name == "name");
        CHECK(c.type.name == "string");
        CHECK(c.type.basetype == SDBasic::String);
        CHECK(c.type.flags == SDTypeFlags::NoFlags);

        CHECK(c.data.str == "A complex object");
      }

      {
        const SDObject &c = *o.GetChild(1);

        CHECK(c.name == "floats");
        CHECK(c.type.basetype == SDBasic::Array);
        CHECK(c.type.flags == SDTypeFlags::NoFlags);
        CHECK(c.NumChildren() == 3);
        for(const SDObject *ch : c)
        {
          CHECK(ch->type.basetype == SDBasic::Float);
          CHECK(ch->type.byteSize == 4);
        }

        CHECK(c.GetChild(0)->data.basic.d == 1.2f);
        CHECK(c.GetChild(1)->data.basic.d == 3.4f);
        CHECK(c.GetChild(2)->data.basic.d == 5.6f);
      }

      {
        const SDObject &c = *o.GetChild(2);

        CHECK(c.name == "viewports");
        CHECK(c.type.basetype == SDBasic::Array);
        CHECK(c.type.flags == SDTypeFlags::NoFlags);
        CHECK(c.NumChildren() == 4);
        for(const SDObject *ch : c)
        {
          CHECK(ch->type.basetype == SDBasic::Struct);
          CHECK(ch->type.name == "struct1");
        }

        CHECK(c.GetChild(0)->GetChild(0)->data.basic.d == 512.0f);
        CHECK(c.GetChild(0)->GetChild(1)->data.basic.d == 0.0f);
        CHECK(c.GetChild(0)->GetChild(2)->data.basic.d == 256.0f);
        CHECK(c.GetChild(0)->GetChild(3)->data.basic.d == 256.0f);
      }
    }

    {
      const SDObject &o = *chunk.GetChild(childIdx++);

      CHECK(o.name == "inputParam1");
      CHECK(o.type.basetype == SDBasic::Struct);
      CHECK(o.type.flags == SDTypeFlags::Nullable);

      CHECK(o.GetChild(0)->data.basic.d == 9.0f);
      CHECK(o.GetChild(1)->data.basic.d == 9.9f);
      CHECK(o.GetChild(2)->data.basic.d == 9.99f);
      CHECK(o.GetChild(3)->data.basic.d == 9.999f);
    }

    {
      const SDObject &o = *chunk.GetChild(childIdx++);

      CHECK(o.name == "inputParam2");
      CHECK(o.type.basetype == SDBasic::Null);
      CHECK(o.type.flags == SDTypeFlags::Nullable);
    }

    StreamWriter *rewriteBuf = new StreamWriter(StreamWriter::DefaultScratchSize);

    {
      WriteSerialiser rewrite(rewriteBuf, Ownership::Nothing);

      rewrite.WriteStructuredFile(structData, NULL);
    }

    // must be bitwise identical to the original serialised data.
    REQUIRE(rewriteBuf->GetOffset() == buf->GetOffset());
    CHECK_FALSE(memcmp(rewriteBuf->GetData(), buf->GetData(), (size_t)rewriteBuf->GetOffset()));

    delete rewriteBuf;
  }

  delete buf;
};

enum class TestEnumClass
{
  A = 1,
  B = 2,
  C = 2,
};

DECLARE_REFLECTION_ENUM(TestEnumClass);

template <>
rdcstr DoStringise(const TestEnumClass &el)
{
  BEGIN_ENUM_STRINGISE(TestEnumClass)
  {
    STRINGISE_ENUM_CLASS(A);
    STRINGISE_ENUM_CLASS_NAMED(B, "Beta");
    // can't add this because B == C
    // STRINGISE_ENUM_CLASS_NAMED(C, "Charlie");
  }
  END_ENUM_STRINGISE();
}

enum TestEnum
{
  TestA = 1,
  TestB = 2,
  TestC = 2,
};

DECLARE_REFLECTION_ENUM(TestEnum);

template <>
rdcstr DoStringise(const TestEnum &el)
{
  BEGIN_ENUM_STRINGISE(TestEnum)
  {
    STRINGISE_ENUM(TestA);
    STRINGISE_ENUM_NAMED(TestB, "Beta");
    // can't add this because B == C
    // STRINGISE_ENUM_NAMED(TestC, "Charlie");
  }
  END_ENUM_STRINGISE();
}

enum class TestBitfieldClass
{
  A = 1,
  B = 2,
  AandB = 3,
  C = 4,
  AandC = 5,
  Dupe = 4,
};

DECLARE_REFLECTION_ENUM(TestBitfieldClass);

BITMASK_OPERATORS(TestBitfieldClass);

template <>
rdcstr DoStringise(const TestBitfieldClass &el)
{
  BEGIN_BITFIELD_STRINGISE(TestBitfieldClass)
  {
    STRINGISE_BITFIELD_CLASS_VALUE(AandB);
    STRINGISE_BITFIELD_CLASS_VALUE_NAMED(AandC, "A and C");
    STRINGISE_BITFIELD_CLASS_BIT(A);
    STRINGISE_BITFIELD_CLASS_BIT_NAMED(B, "Beta");
    STRINGISE_BITFIELD_CLASS_BIT(C);
    // this duplicated bit should be displayed as well
    STRINGISE_BITFIELD_CLASS_BIT(Dupe);
  }
  END_BITFIELD_STRINGISE();
}

enum TestBitfield
{
  TestBitA = 1,
  TestBitB = 2,
  TestAandB = 3,
  TestBitC = 4,
  TestAandC = 5,
  TestBitDupe = 4,
};

DECLARE_REFLECTION_ENUM(TestBitfield);

template <>
rdcstr DoStringise(const TestBitfield &el)
{
  BEGIN_BITFIELD_STRINGISE(TestBitfield)
  {
    STRINGISE_BITFIELD_VALUE(TestAandB);
    STRINGISE_BITFIELD_VALUE_NAMED(TestAandC, "A and C");
    STRINGISE_BITFIELD_BIT(TestBitA);
    STRINGISE_BITFIELD_BIT_NAMED(TestBitB, "Beta");
    STRINGISE_BITFIELD_BIT(TestBitC);
    // this duplicated bit should be displayed as well
    STRINGISE_BITFIELD_BIT(TestBitDupe);
  }
  END_BITFIELD_STRINGISE();
}

void test(const char *aasd)
{
  RDCLOG("got a test of %s", aasd);
}

TEST_CASE("Test stringification works as expected", "[tostr]")
{
  SECTION("Enum classes")
  {
    TestEnumClass foo = TestEnumClass::A;

    CHECK(ToStr(foo) == "A");

    foo = TestEnumClass::B;

    CHECK(ToStr(foo) == "Beta");

    // identical enum value, will be identified as the first entry
    foo = TestEnumClass::C;

    CHECK(ToStr(foo) == "Beta");

    // unknown value
    foo = (TestEnumClass)0;

    CHECK(ToStr(foo) == "TestEnumClass(0)");
  };

  SECTION("integers")
  {
    uint16_t a = 54;
    uint32_t b = 22;
    uint8_t c = 99;

    test(ToStr(a).c_str());
    test(ToStr(b).c_str());
    test(ToStr(c).c_str());
  };

  SECTION("plain enums")
  {
    TestEnum foo = TestA;

    CHECK(ToStr(foo) == "TestA");

    foo = TestB;

    CHECK(ToStr(foo) == "Beta");

    // identical enum value, will be identified as the first entry
    foo = TestC;

    CHECK(ToStr(foo) == "Beta");

    // unknown value
    foo = (TestEnum)0;

    CHECK(ToStr(foo) == "TestEnum(0)");
  };

  SECTION("Enum class bitfields")
  {
    TestBitfieldClass foo = TestBitfieldClass::A;

    CHECK(ToStr(foo) == "A");

    foo = TestBitfieldClass::A | TestBitfieldClass::B;

    // special cased combo
    CHECK(ToStr(foo) == "AandB");

    foo = TestBitfieldClass::A | TestBitfieldClass::C;

    // special cased combo
    CHECK(ToStr(foo) == "A and C");

    // auto-generated combo
    foo = TestBitfieldClass::A | TestBitfieldClass::B | TestBitfieldClass::C;

    CHECK(ToStr(foo) == "A | Beta | C | Dupe");

    // duplicate bit will be printed as first entry
    foo = TestBitfieldClass::A | TestBitfieldClass::B | TestBitfieldClass::Dupe;

    CHECK(ToStr(foo) == "A | Beta | C | Dupe");

    // unknown bits will be appended
    foo = TestBitfieldClass::A | TestBitfieldClass::B | TestBitfieldClass(0x800);

    CHECK(ToStr(foo) == "A | Beta | TestBitfieldClass(2048)");
  };

  SECTION("plain enum bitfields")
  {
    TestBitfield foo = TestBitA;

    CHECK(ToStr(foo) == "TestBitA");

    foo = TestBitfield(TestBitA | TestBitB);

    // special cased combo
    CHECK(ToStr(foo) == "TestAandB");

    foo = TestBitfield(TestBitA | TestBitC);

    // special cased combo
    CHECK(ToStr(foo) == "A and C");

    // auto-generated combo
    foo = TestBitfield(TestBitA | TestBitB | TestBitC);

    CHECK(ToStr(foo) == "TestBitA | Beta | TestBitC | TestBitDupe");

    // duplicate bit will be printed as first entry
    foo = TestBitfield(TestBitA | TestBitB | TestBitDupe);

    CHECK(ToStr(foo) == "TestBitA | Beta | TestBitC | TestBitDupe");

    // unknown bits will be appended
    foo = TestBitfield(TestBitA | TestBitB | 0x800);

    CHECK(ToStr(foo) == "TestBitA | Beta | TestBitfield(2048)");
  };
};

#endif    // ENABLED(ENABLE_UNIT_TESTS)
