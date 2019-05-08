/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2019 Baldur Karlsson
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

#include <utility>
#include "common/common.h"
#include "serialise/rdcfile.h"

#include "3rdparty/miniz/miniz.h"
#include "3rdparty/pugixml/pugixml.hpp"

struct ThumbTypeAndData
{
  FileType format;
  bytebuf data;
};

static const char *typeNames[] = {
    "chunk", "struct", "array", "null", "buffer", "string",     "enum",
    "uint",  "int",    "float", "bool", "char",   "ResourceId",
};

template <typename inttype>
std::string GetBufferName(inttype i)
{
  return StringFormat::Fmt("%06u", (uint32_t)i);
}

inline float BufferProgress(float progress)
{
  return 0.2f * progress;
}

inline float StructuredProgress(float progress)
{
  return 0.2f + 0.8f * progress;
}

struct xml_file_writer : pugi::xml_writer
{
  StreamWriter stream;

  xml_file_writer(const char *filename) : stream(FileIO::fopen(filename, "wb"), Ownership::Stream)
  {
  }

  void write(const void *data, size_t size) { stream.Write(data, size); }
};

// avoid &, <, and > since they throw off the ascii alignment
static constexpr bool IsXMLPrintable(const char c)
{
  return (c >= ' ' && c <= '~' && c != '&' && c != '<' && c != '>');
}

static constexpr bool IsHex(const char c)
{
  return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

static constexpr byte FromHex(const char c)
{
  return c >= '0' && c <= '9'
             ? byte(c - '0')
             : (c >= 'A' && c <= 'F' ? byte(c - 'A') + 10
                                     : (c >= 'a' && c <= 'f' ? byte(c - 'a') + 10 : 0));
}

static void HexEncode(const std::vector<byte> &in, std::string &out)
{
  const size_t bytesPerLine = 32;
  const size_t bytesPerGroup = 4;

  const char digit[] = "0123456789ABCDEF";
  // Reserve rough required size:
  // - 3 characters per byte (two for hex, 1 for ascii),
  // - 4 characters per line (3x space between hex and ascii, newline)
  // - 1 character per group (space)
  // - 2 characters for leading/trailing newline
  out.reserve(in.size() * 3 + (in.size() / bytesPerLine) * 4 + (in.size() / bytesPerGroup) + 2);

  // leading newline
  out = "\n";

  // accumulate ascii representation for each line
  std::string ascii;

  size_t i = 0;
  for(byte c : in)
  {
    out.push_back(digit[(c & 0xf0) >> 4]);
    out.push_back(digit[(c & 0x0f) >> 0]);

    if(IsXMLPrintable((char)c))
      ascii.push_back((char)c);
    else
      ascii.push_back('.');

    i++;
    if((i % bytesPerLine) == 0)
    {
      out += StringFormat::Fmt("   %s\n", ascii.c_str());
      ascii.clear();
    }
    else if((i % bytesPerGroup) == 0)
    {
      out.push_back(' ');
    }
  }

  // add remaining part of a line, if we didn't end by completing one
  size_t lastLineLength = i % bytesPerLine;
  if(lastLineLength > 0)
  {
    for(i = lastLineLength; i < bytesPerLine; i++)
    {
      // print 2 spaces where there would be characters
      out.push_back(' ');
      out.push_back(' ');

      // don't print the group space the first time, since it was already printed, but after that
      // print the group space
      if((i % bytesPerGroup) == 0 && i > lastLineLength)
        out.push_back(' ');
    }

    // add ascii and final newline
    out += StringFormat::Fmt("   %s\n", ascii.c_str());
  }
}

static void HexDecode(const char *str, const char *end, std::vector<byte> &out)
{
  out.reserve((end - str) / 2);

  if(str[0] == '\n')
    str++;

  while(str + 1 < end)
  {
    if(IsHex(str[0]) && IsHex(str[1]))
    {
      out.push_back((FromHex(str[0]) << 4) | FromHex(str[1]));

      str += 2;

      // allow a space after hex, as a byte group
      if(str[0] == ' ')
        str++;

      // if we encounter more spaces though, it indicates the end of a line.
    }
    else
    {
      // on the first non-hex char we encounter, skip to the next newline. This might do nothing if
      // the char itself was a newline.
      while(str[0] != '\n' && str < end)
        str++;

      // the loop above terminates in two ways - when it encounters a newline or when it reaches the
      // end of the string. We can just assume we encountered a newline and increment past it - if
      // we're already past the end, going further past will still make us fail the loop condition
      // and terminate.
      str++;
    }
  }
}

static void Obj2XML(pugi::xml_node &parent, SDObject &child)
{
  pugi::xml_node obj = parent.append_child(typeNames[(uint32_t)child.type.basetype]);

  obj.append_attribute("name") = child.name.c_str();

  if(!child.type.name.empty())
    obj.append_attribute("typename") = child.type.name.c_str();

  if(child.type.basetype == SDBasic::UnsignedInteger ||
     child.type.basetype == SDBasic::SignedInteger || child.type.basetype == SDBasic::Float ||
     child.type.basetype == SDBasic::Resource)
  {
    obj.append_attribute("width") = child.type.byteSize;
  }

  if(child.type.flags & SDTypeFlags::Hidden)
    obj.append_attribute("hidden") = true;

  if(child.type.flags & SDTypeFlags::Nullable)
    obj.append_attribute("nullable") = true;

  if(child.type.flags & SDTypeFlags::NullString)
    obj.append_attribute("nullstring") = true;

  if(child.type.flags & SDTypeFlags::FixedArray)
    obj.append_attribute("fixedarray") = true;

  if(child.type.flags & SDTypeFlags::Union)
    obj.append_attribute("union") = true;

  if(child.type.basetype == SDBasic::Chunk)
  {
    RDCFATAL("Nested chunks!");
  }
  else if(child.type.basetype == SDBasic::Null)
  {
    // redundant
    obj.remove_attribute("nullable");
  }
  else if(child.type.basetype == SDBasic::Struct || child.type.basetype == SDBasic::Array)
  {
    if(child.type.basetype == SDBasic::Array && !child.data.children.empty())
      obj.remove_attribute("typename");

    for(size_t o = 0; o < child.data.children.size(); o++)
    {
      Obj2XML(obj, *child.data.children[o]);

      if(child.type.basetype == SDBasic::Array)
        obj.last_child().remove_attribute("name");
    }
  }
  else if(child.type.basetype == SDBasic::Buffer)
  {
    obj.append_attribute("byteLength") = child.type.byteSize;
    obj.text() = child.data.basic.u;
  }
  else
  {
    if(child.type.flags & SDTypeFlags::HasCustomString)
    {
      obj.append_attribute("string") = child.data.str.c_str();
    }

    switch(child.type.basetype)
    {
      case SDBasic::Resource:
      case SDBasic::Enum:
      case SDBasic::UnsignedInteger: obj.text() = child.data.basic.u; break;
      case SDBasic::SignedInteger: obj.text() = child.data.basic.i; break;
      case SDBasic::String: obj.text() = child.data.str.c_str(); break;
      case SDBasic::Float: obj.text() = child.data.basic.d; break;
      case SDBasic::Boolean: obj.text() = child.data.basic.b; break;
      case SDBasic::Character:
      {
        char str[2] = {child.data.basic.c, '\0'};
        obj.text().set(str);
        break;
      }
      default: RDCERR("Unexpected case");
    }
  }
}

static ReplayStatus Structured2XML(const char *filename, const RDCFile &file, uint64_t version,
                                   const StructuredChunkList &chunks,
                                   RENDERDOC_ProgressCallback progress)
{
  pugi::xml_document doc;

  pugi::xml_node xRoot = doc.append_child("rdc");

  {
    pugi::xml_node xHeader = xRoot.append_child("header");

    pugi::xml_node xDriver = xHeader.append_child("driver");
    xDriver.append_attribute("id") = (uint32_t)file.GetDriver();
    xDriver.text() = file.GetDriverName().c_str();

    pugi::xml_node xIdent = xHeader.append_child("machineIdent");

    xIdent.text().set(file.GetMachineIdent());

    pugi::xml_node xThumbnail = xHeader.append_child("thumbnail");

    const RDCThumb &th = file.GetThumbnail();
    if(th.pixels && th.len > 0 && th.width > 0 && th.height > 0)
    {
      xThumbnail.append_attribute("width") = th.width;
      xThumbnail.append_attribute("height") = th.height;

      if(th.format == FileType::JPG)
        xThumbnail.text() = "thumb.jpg";
      else if(th.format == FileType::PNG)
        xThumbnail.text() = "thumb.png";
      else if(th.format == FileType::Raw)
        xThumbnail.text() = "thumb.raw";
      else
        RDCERR("Unexpected thumbnail format %s", ToStr(th.format).c_str());
    }
  }

  if(progress)
    progress(StructuredProgress(0.1f));

  // write all other sections
  for(int i = 0; i < file.NumSections(); i++)
  {
    const SectionProperties &props = file.GetSectionProperties(i);

    if(props.type == SectionType::FrameCapture)
      continue;

    StreamReader *reader = file.ReadSection(i);

    if(props.type == SectionType::ExtendedThumbnail)
    {
      ExtThumbnailHeader thumbHeader = {};
      if(reader->Read(thumbHeader))
      {
        // don't need to read the data, that's handled in Buffers2ZIP
        bool succeeded = reader->SkipBytes(thumbHeader.len) && !reader->IsErrored();
        if(succeeded && (uint32_t)thumbHeader.format < (uint32_t)FileType::Count)
        {
          pugi::xml_node xExtThumbnail = xRoot.append_child("extended_thumbnail");

          xExtThumbnail.append_attribute("width") = thumbHeader.width;
          xExtThumbnail.append_attribute("height") = thumbHeader.height;
          xExtThumbnail.append_attribute("length") = thumbHeader.len;

          if(thumbHeader.format == FileType::JPG)
            xExtThumbnail.text() = "ext_thumb.jpg";
          else if(thumbHeader.format == FileType::PNG)
            xExtThumbnail.text() = "ext_thumb.png";
          else if(thumbHeader.format == FileType::Raw)
            xExtThumbnail.text() = "ext_thumb.raw";
          else
            RDCERR("Unexpected extended thumbnail format %s", ToStr(thumbHeader.format).c_str());
        }
      }

      delete reader;
      continue;
    }

    pugi::xml_node xSection = xRoot.append_child("section");

    if(props.flags & SectionFlags::ASCIIStored)
      xSection.append_attribute("ascii");
    if(props.flags & SectionFlags::LZ4Compressed)
      xSection.append_attribute("lz4");
    if(props.flags & SectionFlags::ZstdCompressed)
      xSection.append_attribute("zstd");

    pugi::xml_node name = xSection.append_child("name");
    name.text() = props.name.c_str();

    pugi::xml_node secVer = xSection.append_child("version");
    secVer.text() = props.version;

    pugi::xml_node type = xSection.append_child("type");
    type.text() = (uint32_t)props.type;

    std::vector<byte> contents;
    contents.resize((size_t)reader->GetSize());
    reader->Read(contents.data(), reader->GetSize());

    pugi::xml_node data = xSection.append_child("data");

    if(props.flags & SectionFlags::ASCIIStored)
    {
      // insert the contents literally
      data.text().set((char *)contents.data());
    }
    else
    {
      // encode to simple hex. Not efficient, but easy.
      std::string hexdata;
      hexdata.reserve(contents.size() * 2);
      HexEncode(contents, hexdata);
      data.text().set(hexdata.c_str());
    }

    delete reader;
  }

  if(progress)
    progress(StructuredProgress(0.2f));

  pugi::xml_node xChunks = xRoot.append_child("chunks");

  xChunks.append_attribute("version") = version;

  for(size_t c = 0; c < chunks.size(); c++)
  {
    pugi::xml_node xChunk = xChunks.append_child("chunk");
    SDChunk *chunk = chunks[c];

    xChunk.append_attribute("id") = chunk->metadata.chunkID;
    xChunk.append_attribute("name") = chunk->name.c_str();
    xChunk.append_attribute("length") = chunk->metadata.length;
    if(chunk->metadata.threadID)
      xChunk.append_attribute("threadID") = chunk->metadata.threadID;
    if(chunk->metadata.timestampMicro)
      xChunk.append_attribute("timestamp") = chunk->metadata.timestampMicro;
    if(chunk->metadata.durationMicro >= 0)
      xChunk.append_attribute("duration") = chunk->metadata.durationMicro;
    if(chunk->metadata.flags & SDChunkFlags::HasCallstack)
    {
      pugi::xml_node stack = xChunk.append_child("callstack");

      for(size_t i = 0; i < chunk->metadata.callstack.size(); i++)
      {
        stack.append_child("address").text() = chunk->metadata.callstack[i];
      }
    }

    if(chunk->metadata.flags & SDChunkFlags::OpaqueChunk)
    {
      xChunk.append_attribute("opaque") = true;

      RDCASSERT(!chunk->data.children.empty());
      pugi::xml_node opaque = xChunk.append_child("buffer");
      opaque.append_attribute("byteLength") = chunk->data.children[0]->type.byteSize;
      opaque.text() = chunk->data.children[0]->data.basic.u;
    }
    else
    {
      for(size_t o = 0; o < chunk->data.children.size(); o++)
        Obj2XML(xChunk, *chunk->data.children[o]);
    }

    if(progress)
      progress(StructuredProgress(0.2f + 0.8f * (float(c) / float(chunks.size()))));
  }

  xml_file_writer writer(filename);
  doc.save(writer);

  return writer.stream.IsErrored() ? ReplayStatus::FileIOFailed : ReplayStatus::Succeeded;
}

static SDObject *XML2Obj(pugi::xml_node &obj)
{
  SDObject *ret =
      new SDObject(obj.attribute("name").as_string(), obj.attribute("typename").as_string());

  std::string name = obj.name();

  for(size_t i = 0; i < ARRAY_COUNT(typeNames); i++)
  {
    if(name == typeNames[i])
    {
      ret->type.basetype = (SDBasic)i;
      break;
    }
  }

  if(ret->type.basetype == SDBasic::UnsignedInteger || ret->type.basetype == SDBasic::SignedInteger ||
     ret->type.basetype == SDBasic::Float || ret->type.basetype == SDBasic::Resource)
  {
    ret->type.byteSize = obj.attribute("width").as_uint();
  }

  if(obj.attribute("hidden"))
    ret->type.flags |= SDTypeFlags::Hidden;

  if(obj.attribute("nullable"))
    ret->type.flags |= SDTypeFlags::Nullable;

  if(obj.attribute("fixedarray"))
    ret->type.flags |= SDTypeFlags::FixedArray;

  if(obj.attribute("union"))
    ret->type.flags |= SDTypeFlags::Union;

  if(obj.attribute("typename"))
    ret->type.name = obj.attribute("typename").as_string();

  ret->name = obj.attribute("name").as_string();

  if(ret->type.basetype == SDBasic::Chunk)
  {
    RDCFATAL("Nested chunks!");
  }
  else if(ret->type.basetype == SDBasic::Null)
  {
    ret->type.flags |= SDTypeFlags::Nullable;
  }
  else if(ret->type.basetype == SDBasic::Struct || ret->type.basetype == SDBasic::Array)
  {
    for(pugi::xml_node child = obj.first_child(); child; child = child.next_sibling())
    {
      ret->data.children.push_back(XML2Obj(child));

      if(ret->type.basetype == SDBasic::Array)
        ret->data.children.back()->name = "$el";
    }

    if(ret->type.basetype == SDBasic::Array && !ret->data.children.empty())
      ret->type.name = ret->data.children.back()->type.name;
  }
  else if(ret->type.basetype == SDBasic::Buffer)
  {
    ret->type.byteSize = obj.attribute("byteLength").as_ullong();
    ret->data.basic.u = obj.text().as_uint();
  }
  else
  {
    if(obj.attribute("string"))
    {
      ret->type.flags |= SDTypeFlags::HasCustomString;
      ret->data.str = obj.attribute("string").as_string();
    }

    if(obj.attribute("nullstring"))
      ret->type.flags |= SDTypeFlags::NullString;

    switch(ret->type.basetype)
    {
      case SDBasic::Resource:
      case SDBasic::Enum:
      case SDBasic::UnsignedInteger: ret->data.basic.u = obj.text().as_ullong(); break;
      case SDBasic::SignedInteger: ret->data.basic.i = obj.text().as_llong(); break;
      case SDBasic::String: ret->data.str = obj.text().as_string(); break;
      case SDBasic::Float: ret->data.basic.d = obj.text().as_double(); break;
      case SDBasic::Boolean: ret->data.basic.b = obj.text().as_bool(); break;
      case SDBasic::Character: ret->data.basic.c = obj.text().as_string()[0]; break;
      default: RDCERR("Unexpected case");
    }
  }

  return ret;
}

static ReplayStatus XML2Structured(const char *xml, const ThumbTypeAndData &thumb,
                                   const ThumbTypeAndData &extThumb,
                                   const StructuredBufferList &buffers, RDCFile *rdc,
                                   uint64_t &version, StructuredChunkList &chunks,
                                   RENDERDOC_ProgressCallback progress)
{
  pugi::xml_document doc;
  doc.load_string(xml);

  pugi::xml_node root = doc.child("rdc");

  if(!root)
  {
    RDCERR("Malformed document, expected rdc node");
    return ReplayStatus::FileCorrupted;
  }

  pugi::xml_node xHeader = root.first_child();

  if(strcmp(xHeader.name(), "header"))
  {
    RDCERR("Malformed document, expected header node");
    return ReplayStatus::FileCorrupted;
  }

  // process the header and push meta-data into RDC
  {
    pugi::xml_node xDriver = xHeader.first_child();

    if(strcmp(xDriver.name(), "driver"))
    {
      RDCERR("Malformed document, expected driver node");
      return ReplayStatus::FileCorrupted;
    }

    RDCDriver driver = (RDCDriver)xDriver.attribute("id").as_uint();
    std::string driverName = xDriver.text().as_string();

    pugi::xml_node xIdent = xDriver.next_sibling();

    uint64_t machineIdent = xIdent.text().as_ullong();

    pugi::xml_node xThumbnail = xIdent.next_sibling();

    if(strcmp(xThumbnail.name(), "thumbnail"))
    {
      RDCERR("Malformed document, expected driver node");
      return ReplayStatus::FileCorrupted;
    }

    RDCThumb th;
    th.format = thumb.format;
    th.width = (uint16_t)xThumbnail.attribute("width").as_uint();
    th.height = (uint16_t)xThumbnail.attribute("height").as_uint();

    RDCThumb *rdcthumb = NULL;

    if(th.width > 0 && th.height > 0 && !thumb.data.empty())
    {
      th.pixels = thumb.data.data();
      th.len = (uint32_t)thumb.data.size();
      rdcthumb = &th;
    }

    rdc->SetData(driver, driverName.c_str(), machineIdent, rdcthumb);
  }

  if(progress)
    progress(StructuredProgress(0.1f));

  // push in other sections
  pugi::xml_node xSection = xHeader.next_sibling();

  while(!strcmp(xSection.name(), "section") || !strcmp(xSection.name(), "extended_thumbnail"))
  {
    if(!strcmp(xSection.name(), "extended_thumbnail"))
    {
      SectionProperties props = {};
      props.type = SectionType::ExtendedThumbnail;
      props.version = 1;
      StreamWriter *w = rdc->WriteSection(props);

      ExtThumbnailHeader header;
      header.width = (uint16_t)xSection.attribute("width").as_uint();
      header.height = (uint16_t)xSection.attribute("height").as_uint();
      header.len = (uint32_t)extThumb.data.size();
      header.format = extThumb.format;
      w->Write(header);
      w->Write(extThumb.data.data(), extThumb.data.size());

      w->Finish();

      delete w;

      xSection = xSection.next_sibling();
      continue;
    }

    SectionProperties props;

    if(xSection.attribute("ascii"))
      props.flags |= SectionFlags::ASCIIStored;
    if(xSection.attribute("lz4"))
      props.flags |= SectionFlags::LZ4Compressed;
    if(xSection.attribute("zstd"))
      props.flags |= SectionFlags::ZstdCompressed;

    pugi::xml_node name = xSection.child("name");
    if(!name)
    {
      RDCERR("Malformed section, expected name node");
      xSection = xSection.next_sibling();
      continue;
    }
    props.name = name.text().as_string();

    pugi::xml_node secVer = xSection.child("version");
    if(!secVer)
    {
      RDCERR("Malformed section, expected version node");
      xSection = xSection.next_sibling();
      continue;
    }
    props.version = secVer.text().as_ullong();

    pugi::xml_node type = xSection.child("type");
    if(!type)
    {
      RDCERR("Malformed section, expected type node");
      xSection = xSection.next_sibling();
      continue;
    }
    props.type = (SectionType)type.text().as_uint();

    pugi::xml_node data = xSection.child("data");
    if(!data)
    {
      RDCERR("Malformed section, expected data node");
      xSection = xSection.next_sibling();
      continue;
    }

    const char *str = (const char *)data.text().get();
    size_t len = strlen(str);

    StreamWriter *writer = rdc->WriteSection(props);

    if(props.flags & SectionFlags::ASCIIStored)
    {
      writer->Write(str, len);
    }
    else
    {
      std::vector<byte> decoded;
      HexDecode(str, str + len, decoded);
      writer->Write(decoded.data(), decoded.size());
    }

    writer->Finish();
    delete writer;

    xSection = xSection.next_sibling();
  }

  if(progress)
    progress(StructuredProgress(0.2f));

  pugi::xml_node xChunks = xSection;

  if(strcmp(xSection.name(), "chunks"))
  {
    RDCERR("Malformed document, expected chunks node");
    return ReplayStatus::FileCorrupted;
  }

  if(!xChunks.attribute("version"))
  {
    RDCERR("Malformed document, expected version attribute");
    return ReplayStatus::FileCorrupted;
  }

  version = xChunks.attribute("version").as_ullong();

  size_t chunkIdx = 0;
  size_t numChunks = std::distance(xChunks.begin(), xChunks.end());

  for(pugi::xml_node xChunk = xChunks.first_child(); xChunk; xChunk = xChunk.next_sibling())
  {
    if(strcmp(xChunk.name(), "chunk"))
      return ReplayStatus::FileCorrupted;

    SDChunk *chunk = new SDChunk(xChunk.attribute("name").as_string());

    chunk->metadata.chunkID = xChunk.attribute("id").as_uint();
    chunk->metadata.length = xChunk.attribute("length").as_uint();
    if(xChunk.attribute("threadID"))
      chunk->metadata.threadID = xChunk.attribute("threadID").as_ullong();
    if(xChunk.attribute("timestamp"))
      chunk->metadata.timestampMicro = xChunk.attribute("timestamp").as_ullong();
    if(xChunk.attribute("duration"))
      chunk->metadata.durationMicro = xChunk.attribute("duration").as_ullong();

    pugi::xml_node callstack = xChunk.child("callstack");
    if(callstack)
    {
      chunk->metadata.flags |= SDChunkFlags::HasCallstack;

      size_t i = 0;
      for(pugi::xml_node address = callstack.first_child(); address; address = address.next_sibling())
      {
        chunk->metadata.callstack.push_back(address.text().as_ullong());
        i++;
      }
    }

    if(xChunk.attribute("opaque"))
    {
      pugi::xml_node opaque = xChunk.child("buffer");

      chunk->metadata.flags |= SDChunkFlags::OpaqueChunk;

      chunk->data.children.push_back(new SDObject("Opaque chunk"_lit, "Byte Buffer"_lit));
      chunk->data.children[0]->type.basetype = SDBasic::Buffer;
      chunk->data.children[0]->type.byteSize = opaque.attribute("byteLength").as_ullong();
      chunk->data.children[0]->data.basic.u = opaque.text().as_ullong();
    }
    else
    {
      for(pugi::xml_node child = xChunk.first_child(); child; child = child.next_sibling())
        chunk->data.children.push_back(XML2Obj(child));
    }

    chunks.push_back(chunk);

    if(progress)
      progress(StructuredProgress(0.2f + 0.8f * (float(chunkIdx) / float(numChunks))));

    chunkIdx++;
  }

  return ReplayStatus::Succeeded;
}

static ReplayStatus Buffers2ZIP(const std::string &filename, const RDCFile &file,
                                const StructuredBufferList &buffers,
                                RENDERDOC_ProgressCallback progress)
{
  std::string zipFile = filename;
  zipFile.erase(zipFile.size() - 4);    // remove the .xml, leave only the .zip

  mz_zip_archive zip;
  memset(&zip, 0, sizeof(zip));

  mz_bool b = mz_zip_writer_init_file(&zip, zipFile.c_str(), 0);

  if(!b)
  {
    RDCERR("Failed to open .zip file '%s'", zipFile.c_str());
    return ReplayStatus::FileIOFailed;
  }

  for(size_t i = 0; i < buffers.size(); i++)
  {
    mz_zip_writer_add_mem(&zip, GetBufferName(i).c_str(), buffers[i]->data(), buffers[i]->size(), 2);

    if(progress)
      progress(BufferProgress(float(i) / float(buffers.size())));
  }

  const RDCThumb &th = file.GetThumbnail();
  if(th.pixels && th.len > 0 && th.width > 0 && th.height > 0)
  {
    if(th.format == FileType::JPG)
      mz_zip_writer_add_mem(&zip, "thumb.jpg", th.pixels, th.len, MZ_BEST_COMPRESSION);
    else if(th.format == FileType::PNG)
      mz_zip_writer_add_mem(&zip, "thumb.png", th.pixels, th.len, MZ_BEST_COMPRESSION);
    else if(th.format == FileType::Raw)
      mz_zip_writer_add_mem(&zip, "thumb.raw", th.pixels, th.len, MZ_BEST_COMPRESSION);
    else
      RDCERR("Unexpected thumbnail format %s", ToStr(th.format).c_str());
  }

  for(int i = 0; i < file.NumSections(); i++)
  {
    const SectionProperties &props = file.GetSectionProperties(i);

    if(props.type == SectionType::ExtendedThumbnail)
    {
      StreamReader *reader = file.ReadSection(i);

      ExtThumbnailHeader thumbHeader = {};
      if(reader->Read(thumbHeader))
      {
        byte *thumb_bytes = new byte[thumbHeader.len];

        bool succeeded = reader->Read(thumb_bytes, thumbHeader.len) && !reader->IsErrored();
        if(succeeded && (uint32_t)thumbHeader.format < (uint32_t)FileType::Count)
        {
          if(thumbHeader.format == FileType::JPG)
            mz_zip_writer_add_mem(&zip, "ext_thumb.jpg", thumb_bytes, thumbHeader.len,
                                  MZ_BEST_COMPRESSION);
          else if(thumbHeader.format == FileType::PNG)
            mz_zip_writer_add_mem(&zip, "ext_thumb.png", thumb_bytes, thumbHeader.len,
                                  MZ_BEST_COMPRESSION);
          else if(thumbHeader.format == FileType::Raw)
            mz_zip_writer_add_mem(&zip, "ext_thumb.raw", thumb_bytes, thumbHeader.len,
                                  MZ_BEST_COMPRESSION);
          else
            RDCERR("Unexpected extended thumbnail format %s", ToStr(thumbHeader.format).c_str());
        }

        delete[] thumb_bytes;
      }

      delete reader;
      break;
    }
  }

  mz_zip_writer_finalize_archive(&zip);
  mz_zip_writer_end(&zip);

  return ReplayStatus::Succeeded;
}

static bool ZIP2Buffers(const std::string &filename, ThumbTypeAndData &thumb,
                        ThumbTypeAndData &extThumb, StructuredBufferList &buffers,
                        RENDERDOC_ProgressCallback progress)
{
  std::string zipFile = filename;
  zipFile.erase(zipFile.size() - 4);    // remove the .xml, leave only the .zip

  if(!FileIO::exists(zipFile.c_str()))
  {
    RDCERR("Expected to file zip for %s at %s", filename.c_str(), zipFile.c_str());
    return false;
  }

  mz_zip_archive zip;
  memset(&zip, 0, sizeof(zip));

  mz_bool b = mz_zip_reader_init_file(&zip, zipFile.c_str(), 0);

  if(b)
  {
    mz_uint numfiles = mz_zip_reader_get_num_files(&zip);

    buffers.resize(numfiles);

    for(mz_uint i = 0; i < numfiles; i++)
    {
      mz_zip_archive_file_stat zstat;
      mz_zip_reader_file_stat(&zip, i, &zstat);

      size_t sz = 0;

      byte *buf = (byte *)mz_zip_reader_extract_to_heap(&zip, i, &sz, 0);

      // thumbnails are stored separately
      if(strstr(zstat.m_filename, "thumb"))
      {
        FileType type = FileType::JPG;
        if(strstr(zstat.m_filename, ".png"))
          type = FileType::PNG;
        else if(strstr(zstat.m_filename, ".raw"))
          type = FileType::Raw;

        if(strstr(zstat.m_filename, "ext_thumb"))
        {
          extThumb.format = type;
          extThumb.data.assign(buf, sz);
        }
        else
        {
          thumb.format = type;
          thumb.data.assign(buf, sz);
        }
      }
      else
      {
        int bufname = atoi(zstat.m_filename);

        if(bufname < (int)buffers.size())
        {
          buffers[bufname] = new bytebuf;
          buffers[bufname]->assign(buf, sz);
        }
      }

      if(progress)
        progress(BufferProgress(float(i) / float(numfiles)));
    }
  }

  mz_zip_reader_end(&zip);

  return true;
}

ReplayStatus importXMLZ(const char *filename, StreamReader &reader, RDCFile *rdc,
                        SDFile &structData, RENDERDOC_ProgressCallback progress)
{
  ThumbTypeAndData thumb, extThumb;
  if(filename)
  {
    bool success = ZIP2Buffers(filename, thumb, extThumb, structData.buffers, progress);
    if(!success)
    {
      RDCERR("Couldn't load zip to go with %s", filename);
      return ReplayStatus::FileCorrupted;
    }
  }

  uint64_t len = reader.GetSize();
  char *buf = new char[(size_t)len + 1];
  reader.Read(buf, (size_t)len);
  buf[len] = 0;

  return XML2Structured(buf, thumb, extThumb, structData.buffers, rdc, structData.version,
                        structData.chunks, progress);
}

ReplayStatus exportXMLZ(const char *filename, const RDCFile &rdc, const SDFile &structData,
                        RENDERDOC_ProgressCallback progress)
{
  ReplayStatus ret = Buffers2ZIP(filename, rdc, structData.buffers, progress);

  if(ret != ReplayStatus::Succeeded)
    return ret;

  return Structured2XML(filename, rdc, structData.version, structData.chunks, progress);
}

ReplayStatus exportXMLOnly(const char *filename, const RDCFile &rdc, const SDFile &structData,
                           RENDERDOC_ProgressCallback progress)
{
  return Structured2XML(filename, rdc, structData.version, structData.chunks, progress);
}

static ConversionRegistration XMLZIPConversionRegistration(
    &importXMLZ, &exportXMLZ,
    {
        "zip.xml", "XML+ZIP capture",
        R"(Stores the structured data in an xml tree, with large buffer data stored in indexed blobs in
similarly named zip file.)",
        true,
    });

static ConversionRegistration XMLOnlyConversionRegistration(
    &exportXMLOnly,
    {
        "xml", "XML capture",
        R"(Stores the structured data in an xml tree, with large buffer data omitted - that makes it
easier to work with but it cannot then be imported.)",
        false,
    });