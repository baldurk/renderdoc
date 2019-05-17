/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

#include <algorithm>
#include "dxbc_inspect.h"

#include "official/cvinfo.h"
#include "dxbc_spdb.h"

// uncomment the following to print (very verbose) debugging prints for SPDB processing
//#define SPDBLOG(...) RDCDEBUG(__VA_ARGS__)

#ifndef SPDBLOG
#define SPDBLOG(...) (void)(__VA_ARGS__)
#endif

namespace DXBC
{
static const uint32_t FOURCC_SPDB = MAKE_FOURCC('S', 'P', 'D', 'B');

SPDBChunk::SPDBChunk(DXBCFile *dxbc, void *chunk)
{
  m_HasDebugInfo = false;

  byte *data = NULL;

  m_ShaderFlags = 0;

  uint32_t spdblength;
  {
    uint32_t *raw = (uint32_t *)chunk;

    if(raw[0] != FOURCC_SPDB)
      return;

    spdblength = raw[1];

    data = (byte *)&raw[2];
  }

  FileHeaderPage *header = (FileHeaderPage *)data;

  if(memcmp(header->identifier, "Microsoft C/C++ MSF 7.00\r\n\032DS\0\0", sizeof(header->identifier)))
  {
    RDCWARN("Unexpected SPDB type");
    return;
  }

  RDCASSERT(header->PageCount * header->PageSize == spdblength);

  const byte **pages = new const byte *[header->PageCount];
  for(uint32_t i = 0; i < header->PageCount; i++)
    pages[i] = &data[i * header->PageSize];

  uint32_t rootdirCount = header->PagesForByteSize(header->RootDirSize);
  uint32_t rootDirIndicesCount = header->PagesForByteSize(rootdirCount * sizeof(uint32_t));

  PageMapping rootdirIndicesMapping(pages, header->PageSize, header->RootDirectory,
                                    rootDirIndicesCount);
  const byte *rootdirIndices = rootdirIndicesMapping.Data();

  PageMapping directoryMapping(pages, header->PageSize, (uint32_t *)rootdirIndices, rootdirCount);
  const uint32_t *dirContents = (const uint32_t *)directoryMapping.Data();

  std::vector<PDBStream> streams;

  streams.resize(*dirContents);
  dirContents++;

  SPDBLOG("SPDB contains %zu streams", streams.size());

  for(size_t i = 0; i < streams.size(); i++)
  {
    streams[i].byteLength = *dirContents;
    SPDBLOG("Stream[%zu] is %u bytes", i, streams[i].byteLength);
    dirContents++;
  }

  for(size_t i = 0; i < streams.size(); i++)
  {
    if(streams[i].byteLength == 0)
      continue;

    for(uint32_t p = 0; p < header->PagesForByteSize(streams[i].byteLength); p++)
    {
      streams[i].pageIndices.push_back(*dirContents);
      dirContents++;
    }
  }

  RDCASSERT(streams.size() > 1);

  // stream 1: GUID + stream names
  PageMapping guidMapping(pages, header->PageSize, &streams[1].pageIndices[0],
                          (uint32_t)streams[1].pageIndices.size());
  GuidPageHeader *guid = (GuidPageHeader *)guidMapping.Data();

  uint32_t *hashtable = (uint32_t *)(guid->Strings + guid->StringBytes);

  uint32_t numSetBits = hashtable[0];
  hashtable++;
  uint32_t maxBit = hashtable[0];
  hashtable++;
  uint32_t setBitsetWords = hashtable[0];
  hashtable++;
  uint32_t *setBitset = hashtable;
  hashtable += setBitsetWords;
  RDCASSERT(hashtable[0] == 0);
  hashtable++;

  std::map<std::string, uint32_t> StreamNames;

  uint32_t numset = 0;
  for(uint32_t i = 0; i < maxBit; i++)
  {
    if((setBitset[(i / 32)] & (1 << (i % 32))) != 0)
    {
      uint32_t strOffs = hashtable[0];
      hashtable++;
      uint32_t stream = hashtable[0];
      hashtable++;

      char *streamName = guid->Strings + strOffs;

      StreamNames[streamName] = stream;

      SPDBLOG("Stream %u is %s", stream, streamName);

      numset++;
    }
  }
  RDCASSERT(numset == numSetBits);

  for(auto it = StreamNames.begin(); it != StreamNames.end(); ++it)
  {
    if(!strncmp(it->first.c_str(), "/src/files/", 11))
    {
      PDBStream &s = streams[it->second];
      PageMapping fileContents(pages, header->PageSize, &s.pageIndices[0],
                               (uint32_t)s.pageIndices.size());

      const char *filename = (const char *)it->first.c_str();
      filename += sizeof("/src/files/") - 1;

      SPDBLOG("Found file '%s' from stream %u", filename, it->second);

      if(filename[0] == 0)
        filename = "shader";

      Files.push_back({filename, (char *)fileContents.Data()});
    }
  }

  struct TypeMember
  {
    std::string name;
    uint16_t byteOffset;
    uint32_t typeIndex;
  };

  struct TypeDesc
  {
    std::string name;
    VarType baseType;
    uint32_t byteSize;
    uint16_t vecSize;
    uint16_t matArrayStride;
    LEAF_ENUM_e leafType;
    std::vector<TypeMember> members;
  };

  std::map<uint32_t, TypeDesc> typeInfo;

  // prepopulate with basic types
  // for now we stick to full-precision 32-bit VarTypes. It's not clear if HLSL even emits the other
  // types
  typeInfo[T_INT4] = {"int32_t", VarType::SInt, 4, 1, 0, LF_NUMERIC, {}};
  typeInfo[T_INT2] = {"int16_t", VarType::SInt, 2, 1, 0, LF_NUMERIC, {}};
  typeInfo[T_INT1] = {"int8_t", VarType::SInt, 1, 1, 0, LF_NUMERIC, {}};
  typeInfo[T_LONG] = {"int32_t", VarType::SInt, 4, 1, 0, LF_NUMERIC, {}};
  typeInfo[T_SHORT] = {"int16_t", VarType::SInt, 2, 1, 0, LF_NUMERIC, {}};
  typeInfo[T_CHAR] = {"char", VarType::SInt, 1, 1, 0, LF_NUMERIC, {}};
  typeInfo[T_BOOL32FF] = {"bool", VarType::UInt, 4, 1, 0, LF_NUMERIC, {}};
  typeInfo[T_UINT4] = {"uint32_t", VarType::UInt, 4, 1, 0, LF_NUMERIC, {}};
  typeInfo[T_UINT2] = {"uint16_t", VarType::UInt, 2, 1, 0, LF_NUMERIC, {}};
  typeInfo[T_UINT1] = {"uint8_t", VarType::UInt, 1, 1, 0, LF_NUMERIC, {}};
  typeInfo[T_ULONG] = {"uint32_t", VarType::UInt, 4, 1, 0, LF_NUMERIC, {}};
  typeInfo[T_USHORT] = {"uint16_t", VarType::UInt, 2, 1, 0, LF_NUMERIC, {}};
  typeInfo[T_UCHAR] = {"unsigned char", VarType::UInt, 1, 1, 0, LF_NUMERIC, {}};
  typeInfo[T_REAL16] = {"half", VarType::Float, 2, 1, 0, LF_NUMERIC, {}};
  typeInfo[T_REAL32] = {"float", VarType::Float, 4, 1, 0, LF_NUMERIC, {}};
  // modern HLSL fake half
  typeInfo[T_REAL32PP] = {"half", VarType::Float, 4, 1, 0, LF_NUMERIC, {}};
  typeInfo[T_REAL64] = {"double", VarType::Double, 8, 1, 0, LF_NUMERIC, {}};

  if(streams.size() >= 3)
  {
    SPDBLOG("Got types stream");

    PDBStream &s = streams[2];
    PageMapping fileContents(pages, header->PageSize, &s.pageIndices[0],
                             (uint32_t)s.pageIndices.size());

    byte *bytes = (byte *)fileContents.Data();
    byte *end = bytes + s.byteLength;

    TPIHeader *tpi = (TPIHeader *)bytes;

    // skip header
    bytes += tpi->headerSize;

    RDCASSERT(bytes + tpi->dataSize == end);

// this isn't needed, but this is the hash stream
#if 0
    PageMapping hashContents;

    if(tpi->hash.streamNumber < streams.size())
    {
      PDBStream &hashstrm = streams[tpi->hash.streamNumber];
      hashContents = PageMapping(pages, header->PageSize, &hashstrm.pageIndices[0],
                                 (uint32_t)hashstrm.pageIndices.size());
    }
#endif

    uint32_t id = tpi->typeMin;

    while(bytes < end)
    {
      uint16_t *leafheader = (uint16_t *)bytes;

      uint16_t length = leafheader[0];
      LEAF_ENUM_e type = (LEAF_ENUM_e)leafheader[1];

      byte *leaf = (byte *)&leafheader[1];

      bytes += 2 + length;

      switch(type)
      {
        case LF_VECTOR:
        {
          lfVector *vector = (lfVector *)leaf;
          // documentation isn't clear, but seems like byte size is always a uint16_t
          uint16_t *bytelength = (uint16_t *)vector->data;
          char *name = (char *)(bytelength + 1);
          SPDBLOG("Type %x is '%s': a vector of %x with %u elements over %u bytes", id, name,
                  vector->elemtype, vector->count, *bytelength);

          typeInfo[id] = {
              name,        typeInfo[vector->elemtype].baseType,
              *bytelength, (uint16_t)vector->count,
              0,           type,
              {},
          };

          break;
        }
        case LF_MATRIX:
        {
          lfMatrix *matrix = (lfMatrix *)leaf;
          // documentation isn't clear, but seems like byte size is always a uint16_t
          uint16_t *bytelength = (uint16_t *)matrix->data;
          char *name = (char *)(bytelength + 1);
          SPDBLOG(
              "Type %x is '%s': a matrix of %x with %u rows, %u columns over %u bytes with %u "
              "byte %s major stride",
              id, name, matrix->elemtype, matrix->rows, matrix->cols, *bytelength,
              matrix->majorStride, matrix->matattr.row_major ? "row" : "column");

          typeInfo[id] = {
              name,
              typeInfo[matrix->elemtype].baseType,
              *bytelength,
              uint16_t(matrix->rows),
              uint16_t(*bytelength / matrix->cols),
              type,
              {},
          };

          break;
        }
        case LF_HLSL:
        {
          lfHLSL *hlsl = (lfHLSL *)leaf;
          // documentation mentions "numeric properties followed by byte size" but we don't need
          // that

          const char *hlslTypeName = "";
          switch((CV_builtin_e)hlsl->kind)
          {
            case CV_BI_HLSL_INTERFACE_POINTER: hlslTypeName = "INTERFACE_POINTER"; break;
            case CV_BI_HLSL_TEXTURE1D: hlslTypeName = "TEXTURE1D"; break;
            case CV_BI_HLSL_TEXTURE1D_ARRAY: hlslTypeName = "TEXTURE1D_ARRAY"; break;
            case CV_BI_HLSL_TEXTURE2D: hlslTypeName = "TEXTURE2D"; break;
            case CV_BI_HLSL_TEXTURE2D_ARRAY: hlslTypeName = "TEXTURE2D_ARRAY"; break;
            case CV_BI_HLSL_TEXTURE3D: hlslTypeName = "TEXTURE3D"; break;
            case CV_BI_HLSL_TEXTURECUBE: hlslTypeName = "TEXTURECUBE"; break;
            case CV_BI_HLSL_TEXTURECUBE_ARRAY: hlslTypeName = "TEXTURECUBE_ARRAY"; break;
            case CV_BI_HLSL_TEXTURE2DMS: hlslTypeName = "TEXTURE2DMS"; break;
            case CV_BI_HLSL_TEXTURE2DMS_ARRAY: hlslTypeName = "TEXTURE2DMS_ARRAY"; break;
            case CV_BI_HLSL_SAMPLER: hlslTypeName = "SAMPLER"; break;
            case CV_BI_HLSL_SAMPLERCOMPARISON: hlslTypeName = "SAMPLERCOMPARISON"; break;
            case CV_BI_HLSL_BUFFER: hlslTypeName = "BUFFER"; break;
            case CV_BI_HLSL_POINTSTREAM: hlslTypeName = "POINTSTREAM"; break;
            case CV_BI_HLSL_LINESTREAM: hlslTypeName = "LINESTREAM"; break;
            case CV_BI_HLSL_TRIANGLESTREAM: hlslTypeName = "TRIANGLESTREAM"; break;
            case CV_BI_HLSL_INPUTPATCH: hlslTypeName = "INPUTPATCH"; break;
            case CV_BI_HLSL_OUTPUTPATCH: hlslTypeName = "OUTPUTPATCH"; break;
            case CV_BI_HLSL_RWTEXTURE1D: hlslTypeName = "RWTEXTURE1D"; break;
            case CV_BI_HLSL_RWTEXTURE1D_ARRAY: hlslTypeName = "RWTEXTURE1D_ARRAY"; break;
            case CV_BI_HLSL_RWTEXTURE2D: hlslTypeName = "RWTEXTURE2D"; break;
            case CV_BI_HLSL_RWTEXTURE2D_ARRAY: hlslTypeName = "RWTEXTURE2D_ARRAY"; break;
            case CV_BI_HLSL_RWTEXTURE3D: hlslTypeName = "RWTEXTURE3D"; break;
            case CV_BI_HLSL_RWBUFFER: hlslTypeName = "RWBUFFER"; break;
            case CV_BI_HLSL_BYTEADDRESS_BUFFER: hlslTypeName = "BYTEADDRESS_BUFFER"; break;
            case CV_BI_HLSL_RWBYTEADDRESS_BUFFER: hlslTypeName = "RWBYTEADDRESS_BUFFER"; break;
            case CV_BI_HLSL_STRUCTURED_BUFFER: hlslTypeName = "STRUCTURED_BUFFER"; break;
            case CV_BI_HLSL_RWSTRUCTURED_BUFFER: hlslTypeName = "RWSTRUCTURED_BUFFER"; break;
            case CV_BI_HLSL_APPEND_STRUCTURED_BUFFER:
              hlslTypeName = "APPEND_STRUCTURED_BUFFER";
              break;
            case CV_BI_HLSL_CONSUME_STRUCTURED_BUFFER:
              hlslTypeName = "CONSUME_STRUCTURED_BUFFER";
              break;
            case CV_BI_HLSL_MIN8FLOAT: hlslTypeName = "MIN8FLOAT"; break;
            case CV_BI_HLSL_MIN10FLOAT: hlslTypeName = "MIN10FLOAT"; break;
            case CV_BI_HLSL_MIN16FLOAT: hlslTypeName = "MIN16FLOAT"; break;
            case CV_BI_HLSL_MIN12INT: hlslTypeName = "MIN12INT"; break;
            case CV_BI_HLSL_MIN16INT: hlslTypeName = "MIN16INT"; break;
            case CV_BI_HLSL_MIN16UINT: hlslTypeName = "MIN16UINT"; break;
            default: hlslTypeName = "Unknown type";
          }

          SPDBLOG("Type %x is an hlsl %s[%u] (subtype %x)", id, hlslTypeName, hlsl->numprops,
                  hlsl->subtype);
          break;
        }
        case LF_ALIAS:
        {
          lfAlias *alias = (lfAlias *)leaf;
          SPDBLOG("Type %x is an alias for %x", id, alias->utype);

          typeInfo[id] = typeInfo[alias->utype];
          typeInfo[id].name = (char *)alias->Name;

          break;
        }
        case LF_MODIFIER_EX:
        {
          lfModifierEx *modifier = (lfModifierEx *)leaf;
          SPDBLOG("Type %x is %x modified with:", id, modifier->type);

          typeInfo[id] = typeInfo[modifier->type];
          typeInfo[id].name = "modif " + typeInfo[id].name;

          uint16_t *mods = (uint16_t *)modifier->mods;
          for(unsigned short i = 0; i < modifier->count; i++)
          {
            CV_modifier_e mod = (CV_modifier_e)mods[i];

            const char *modName = "";
            switch(mod)
            {
              case CV_MOD_CONST: modName = "CONST"; break;
              case CV_MOD_VOLATILE: modName = "VOLATILE"; break;
              case CV_MOD_UNALIGNED: modName = "UNALIGNED"; break;
              case CV_MOD_HLSL_UNIFORM: modName = "HLSL_UNIFORM"; break;
              case CV_MOD_HLSL_LINE: modName = "HLSL_LINE"; break;
              case CV_MOD_HLSL_TRIANGLE: modName = "HLSL_TRIANGLE"; break;
              case CV_MOD_HLSL_LINEADJ: modName = "HLSL_LINEADJ"; break;
              case CV_MOD_HLSL_TRIANGLEADJ: modName = "HLSL_TRIANGLEADJ"; break;
              case CV_MOD_HLSL_LINEAR: modName = "HLSL_LINEAR"; break;
              case CV_MOD_HLSL_CENTROID: modName = "HLSL_CENTROID"; break;
              case CV_MOD_HLSL_CONSTINTERP: modName = "HLSL_CONSTINTERP"; break;
              case CV_MOD_HLSL_NOPERSPECTIVE: modName = "HLSL_NOPERSPECTIVE"; break;
              case CV_MOD_HLSL_SAMPLE: modName = "HLSL_SAMPLE"; break;
              case CV_MOD_HLSL_CENTER: modName = "HLSL_CENTER"; break;
              case CV_MOD_HLSL_SNORM: modName = "HLSL_SNORM"; break;
              case CV_MOD_HLSL_UNORM: modName = "HLSL_UNORM"; break;
              case CV_MOD_HLSL_PRECISE: modName = "HLSL_PRECISE"; break;
              case CV_MOD_HLSL_UAV_GLOBALLY_COHERENT: modName = "HLSL_UAV_GLOBALLY_COHERENT"; break;
              default: modName = "Unknown modification";
            }

            SPDBLOG("  + %s", modName);
          }
          break;
        }
        case LF_FIELDLIST:
        {
          lfFieldList *fieldList = (lfFieldList *)leaf;
          SPDBLOG("Type %x is a field list containing:", id);

          uint32_t idx = 0;

          std::vector<TypeMember> &members = typeInfo[id].members;

          byte *iter = (byte *)fieldList->data;
          while(iter < bytes)
          {
            if(*iter >= LF_PAD0)
            {
              iter += (*iter) - LF_PAD0;
              continue;
            }

            LEAF_ENUM_e memberType = LEAF_ENUM_e(*(uint16_t *)iter);

            switch(memberType)
            {
              case LF_MEMBER:
              {
                lfMember *member = (lfMember *)iter;

                uint16_t *byteoffset = (uint16_t *)member->offset;
                char *name = (char *)(byteoffset + 1);

                char *access = "???";

                if(member->attr.access == 1)
                  access = "private";
                else if(member->attr.access == 2)
                  access = "protected";
                else if(member->attr.access == 3)
                  access = "public";

                SPDBLOG("  [%u]: %x %s (%s) (at offset %u bytes)", idx, member->index, name, access,
                        *byteoffset);

                members.push_back({name, *byteoffset, member->index});

                idx++;

                iter = (byte *)(name + strlen(name) + 1);
                break;
              }
              case LF_ONEMETHOD:
              {
                lfOneMethod *method = (lfOneMethod *)iter;

                iter = (byte *)method->vbaseoff;

                // MTintro = 0x04, MTpureintro = 0x06
                if(method->attr.mprop == 0x04 || method->attr.mprop == 0x06)
                {
                  iter += 4;
                }

                char *name = (char *)iter;
                iter += strlen(name) + 1;

                char *access = "???";

                if(method->attr.access == 1)
                  access = "private";
                else if(method->attr.access == 2)
                  access = "protected";
                else if(method->attr.access == 3)
                  access = "public";

                SPDBLOG("  [%u]: Method %s (%s) (at offset %u bytes)", idx, name, access);

                idx++;
                break;
              }
              case LF_BCLASS:
              case LF_BINTERFACE:
              {
                lfBClass *binterface = (lfBClass *)iter;

                char *access = "???";

                if(binterface->attr.access == 1)
                  access = "private";
                else if(binterface->attr.access == 2)
                  access = "protected";
                else if(binterface->attr.access == 3)
                  access = "public";

                SPDBLOG("  [%u]: %x Base Class/Interface (%s)", idx, binterface->index, access);

                iter = (byte *)binterface->offset + 2;

                idx++;
                break;
              }
              default:
              {
                RDCERR("Unexpected member type %x", memberType);
                // skip the remaining data as we don't know how to safely advance - no length fields
                // to use
                iter = bytes;
                break;
              }
            }
          }
          break;
        }
        case LF_ARGLIST:
        {
          lfArgList *argList = (lfArgList *)leaf;
          SPDBLOG("Type %x is a field list containing:", id);

          for(unsigned long i = 0; i < argList->count; i++)
            SPDBLOG("  %x", argList->arg[i]);
          break;
        }
        case LF_INTERFACE:
        case LF_CLASS:
        case LF_STRUCTURE:
        {
          lfStructure *structure = (lfStructure *)leaf;
          // documentation isn't clear, but seems like byte size is always a uint16_t
          uint16_t *bytelength = (uint16_t *)structure->data;
          char *name = (char *)(bytelength + 1);

          const char *structType = "struct";
          if(type == LF_INTERFACE)
            structType = "interface";
          else if(type == LF_CLASS)
            structType = "class";

          typeInfo[id] = {
              name, VarType::Float, *bytelength, 1, 0, type, typeInfo[structure->field].members,
          };

          SPDBLOG(
              "Type %x is '%s': a %s with %u fields %x derived from %x and vshape %x over %u "
              "bytes",
              id, name, structType, structure->count, structure->field, structure->derived,
              structure->vshape, *bytelength);
          break;
        }
        case LF_PROCEDURE:
        {
          lfProc *procedure = (lfProc *)leaf;
          SPDBLOG("Type %x is a procedure returning %x with %u args: %x", id, procedure->rvtype,
                  procedure->parmcount, procedure->arglist);
          break;
        }
        case LF_MFUNCTION:
        {
          lfMFunc *mfunction = (lfMFunc *)leaf;
          SPDBLOG("Type %x is a member function of class %x returning %x with %u args: %x", id,
                  mfunction->classtype, mfunction->rvtype, mfunction->parmcount, mfunction->arglist);
          break;
        }
        case LF_STRIDED_ARRAY:
        {
          lfStridedArray *stridedArray = (lfStridedArray *)leaf;
          // documentation isn't clear, but seems like byte size is always a uint16_t
          uint16_t *bytelength = (uint16_t *)stridedArray->data;

          typeInfo[id] = {
              "",
              typeInfo[stridedArray->elemtype].baseType,
              *bytelength,
              1,
              uint16_t(stridedArray->stride),
              type,
              {},
          };

          break;
        }
        default:
        {
          RDCWARN("Encountered unknown type leaf %x", type);
          break;
        }
      }
      id++;
    }

    RDCASSERT(id == tpi->typeMax);
  }

  if(streams.size() >= 5)
  {
    SPDBLOG("Got function calls stream");

    PDBStream &s = streams[4];
    PageMapping fileContents(pages, header->PageSize, &s.pageIndices[0],
                             (uint32_t)s.pageIndices.size());

    byte *bytes = (byte *)fileContents.Data();
    byte *end = bytes + s.byteLength;

    TPIHeader *tpi = (TPIHeader *)bytes;

    // skip header
    bytes += tpi->headerSize;

    RDCASSERT(bytes + tpi->dataSize == end);

// this isn't needed, but this is the hash stream
#if 0
    PageMapping hashContents;

    if(tpi->hash.streamNumber < streams.size())
    {
      PDBStream &hashstrm = streams[tpi->hash.streamNumber];
      hashContents = PageMapping(pages, header->PageSize, &hashstrm.pageIndices[0],
                                 (uint32_t)hashstrm.pageIndices.size());
    }
#endif

    uint32_t id = tpi->typeMin;

    while(bytes < end)
    {
      uint16_t length = *(uint16_t *)bytes;
      bytes += 2;

      lfFuncId *func = (lfFuncId *)bytes;
      lfMFuncId *mfunc = (lfMFuncId *)bytes;
      bytes += length;

      if(func->leaf != LF_FUNC_ID && func->leaf != LF_MFUNC_ID)
      {
        SPDBLOG("Encountered leaf type %x, skipping as not function", func->leaf);
        id++;
        continue;
      }

      if(func->leaf == LF_FUNC_ID && func->scopeId != 0)
        SPDBLOG("Unexpected scope %u", func->scopeId);

      Function f;
      f.type = func->type;
      f.name = (char *)func->name;

      SPDBLOG("Function %x (%s) is type %x", id, f.name.c_str(), f.type);

      if(func->leaf == LF_MFUNC_ID)
        SPDBLOG("Member of %x", mfunc->parentType);

      m_Functions[id] = f;

      id++;
    }

    RDCASSERT(id == tpi->typeMax);
  }

  {
    Function mainFunc;
    mainFunc.name = "entrypoint";

    m_Functions[0] = mainFunc;
  }

  std::map<uint32_t, std::string> Names;

  if(StreamNames.find("/names") != StreamNames.end())
  {
    PDBStream &s = streams[StreamNames["/names"]];
    PageMapping namesMapping(pages, header->PageSize, &s.pageIndices[0],
                             (uint32_t)s.pageIndices.size());
    const uint32_t *contents = (const uint32_t *)namesMapping.Data();

    RDCASSERT(contents[0] == 0xeffeeffe && contents[1] == 1);

    uint32_t StringBytes = contents[2];
    char *Strings = (char *)&contents[3];

    contents += 3;

    contents = (uint32_t *)((byte *)contents + StringBytes);

    uint32_t numHashes = contents[0];
    contents++;

    for(uint32_t i = 0; i < numHashes; i++)
    {
      uint32_t idx = contents[0];
      contents++;

      if(idx != 0)
      {
        Names[idx] = Strings + idx;

        if(Names[idx].size() > 100)
          SPDBLOG("Got Name %u: '%s...'", idx, Names[idx].substr(0, 100).c_str());
        else
          SPDBLOG("Got Name %u: '%s'", idx, Names[idx].c_str());
      }
    }
  }

  std::vector<DBIModule> modules;

  {
    PageMapping dbiMapping(pages, header->PageSize, &streams[3].pageIndices[0],
                           (uint32_t)streams[3].pageIndices.size());
    DBIHeader *dbi = (DBIHeader *)dbiMapping.Data();

    RDCASSERT(dbi->sig == 0xffffffff);
    RDCASSERT(dbi->ver == 19990903);

    byte *cur = (byte *)(dbi + 1);
    byte *end = cur + dbi->gpmodiSize;
    while(cur < end)
    {
      DBIModule *mod = (DBIModule *)cur;
      cur += sizeof(DBIModule) - sizeof(std::string) * 2;

      char *moduleName = (char *)cur;
      cur += strlen(moduleName) + 1;

      char *objectName = (char *)cur;
      cur += strlen(objectName) + 1;

      // align up to DWORD boundary
      while((uintptr_t)cur & 0x3)
        cur++;

      DBIModule m;
      memcpy(&m, mod, sizeof(DBIModule) - sizeof(std::string) * 2);
      m.moduleName = moduleName;
      m.objectName = objectName;

      SPDBLOG("Got module named %s from object %s", moduleName, objectName);

      modules.push_back(m);
    }
    RDCASSERT(cur == end);
  }

  std::vector<Inlinee> inlines;

  PROCSYM32 main = {};

  std::map<uint32_t, int32_t> FileMapping;    // mapping from hash chunk to index in Files[], or -1

  for(size_t m = 0; m < modules.size(); m++)
  {
    if(modules[m].stream == -1)
      continue;

    PDBStream &s = streams[modules[m].stream];
    PageMapping modMapping(pages, header->PageSize, &s.pageIndices[0],
                           (uint32_t)s.pageIndices.size());
    uint32_t *moduledata = (uint32_t *)modMapping.Data();

    SPDBLOG("Examining module %s with %u symbols", modules[m].moduleName.c_str(), modules[m].cbSyms);

    RDCASSERT(moduledata[0] == CV_SIGNATURE_C13);

    std::string localName;
    CV_typ_t localType = 0;

    byte *basePtr = (byte *)&moduledata[1];

    byte *cur = basePtr;
    byte *end = (byte *)moduledata + modules[m].cbSyms;
    while(cur < end)
    {
      uint16_t *sym = (uint16_t *)cur;

      ptrdiff_t ptr = (byte *)(sym + 2) - basePtr;

      uint16_t len = sym[0];
      SYM_ENUM_e type = (SYM_ENUM_e)sym[1];
      cur += len + sizeof(uint16_t);    // len does not include itself

      if(type == S_GPROC32)
      {
        PROCSYM32 *gproc32 = (PROCSYM32 *)sym;
        main = *gproc32;

        m_Functions[0].name = (char *)gproc32->name;

        SPDBLOG("S_GPROC32 @ %llx: '%s' of type %x covering bytes %x -> %x", ptr, gproc32->name,
                gproc32->typind, gproc32->off, gproc32->off + gproc32->len);

        RDCASSERT(gproc32->DbgStart == 0);
        RDCASSERT(gproc32->DbgEnd == gproc32->len);
      }
      else if(type == S_COMPILE3)
      {
        COMPILESYM3 *compile3 = (COMPILESYM3 *)sym;

        m_CompilerSig = compile3->verSz;

        SPDBLOG("S_COMPILE3: %s (%d.%d.%d.%d)", compile3->verSz, compile3->verFEMajor,
                compile3->verFEMinor, compile3->verFEBuild, compile3->verFEQFE);

        // for hlsl/fxc
        RDCASSERT(compile3->flags.iLanguage == CV_CFL_HLSL &&
                  compile3->machine == CV_CFL_D3D11_SHADER);
      }
      else if(type == S_ENVBLOCK)
      {
        ENVBLOCKSYM *envblock = (ENVBLOCKSYM *)sym;

        RDCASSERT(envblock->flags.rev == 1);    // this is another edit & continue flag

        SPDBLOG("S_ENVBLOCK:");

        char *key = (char *)&envblock->rgsz[0];
        while(key[0])
        {
          char *value = key + strlen(key) + 1;

          SPDBLOG("  %s = \"%s\"", key, value);

          if(!strcmp(key, "hlslEntry"))
          {
            m_Entry = value;
          }
          else if(!strcmp(key, "hlslTarget"))
          {
            m_Profile = value;
          }
          else if(!strcmp(key, "hlslFlags"))
          {
            if(value[0] == '0' && value[1] == 'x')
            {
              int i = 2;

              m_ShaderFlags = 0;

              while(value[i] != 0)
              {
                uint32_t digit = 0;
                if(value[i] >= '0' && value[i] <= '9')
                  digit = (uint32_t)(value[i] - '0');
                if(value[i] >= 'a' && value[i] <= 'f')
                  digit = 0xa + (uint32_t)(value[i] - 'a');
                if(value[i] >= 'A' && value[i] <= 'F')
                  digit = 0xa + (uint32_t)(value[i] - 'A');

                m_ShaderFlags <<= 4;
                m_ShaderFlags |= digit;

                i++;
              }
            }
          }
          else if(!strcmp(key, "hlslDefines"))
          {
            std::string cmdlineDefines = "// Command line defines:\n\n";

            char *c = value;

            while(*c)
            {
              // skip whitespace
              while(*c && (*c == ' ' || *c == '\t' || *c == '\n'))
                c++;

              if(*c == 0)
                break;

              // start of a definition
              if(c[0] == '/' && c[1] == 'D')
              {
                c += 2;
                // skip whitespace
                while(*c && (*c == ' ' || *c == '\t' || *c == '\n'))
                  c++;

                if(*c == 0)
                  break;

                char *defstart = c;
                // the definition ends either at the next = or at the next whitespace, whichever
                // comes first
                char *defend = strpbrk(c, "= \t\n");

                if(defend == 0)
                  break;

                bool hasValue = defend[0] == '=';

                c = defend + 1;

                if(hasValue)
                {
                  char *valstart = c;

                  // skip to end or next whitespace
                  while(*c && *c != ' ' && *c != '\t' && *c != '\n')
                    c++;

                  char *valend = c;

                  cmdlineDefines += "#define ";
                  cmdlineDefines +=
                      std::string(defstart, defend) + " " + std::string(valstart, valend);
                  cmdlineDefines += "\n";
                }
                else
                {
                  cmdlineDefines += "#define ";
                  cmdlineDefines += std::string(defstart, defend);
                  cmdlineDefines += "\n";
                }
              }
              else
              {
                c++;
              }
            }

            Files.push_back({"@cmdline", cmdlineDefines});
          }

          key = value + strlen(value) + 1;
        }
      }
      else if(type == S_INLINESITE)
      {
        INLINESITESYM *inlinesite = (INLINESITESYM *)sym;

        SPDBLOG("S_INLINESITE @ %llx: function '%s' inlined into %x", ptr,
                m_Functions[inlinesite->inlinee].name.c_str(), inlinesite->pParent);

        uint32_t codeOffsetBase = 0;
        uint32_t codeOffset = main.off;
        uint32_t codeLength = 0;
        uint32_t currentLine = 0;
        uint32_t currentLineLength = 1;
        uint32_t currentColStart = 1;
        uint32_t currentColEnd = 100000;
        bool statement = true;

        Inlinee inlinee;

        inlinee.ptr = ptr;
        inlinee.parentPtr = inlinesite->pParent;
        inlinee.id = inlinesite->inlinee;

        byte *iter = inlinesite->binaryAnnotations;
        while(iter < cur)
        {
          CodeViewInfo::BinaryAnnotationOpcode op = (CodeViewInfo::BinaryAnnotationOpcode)*iter;

          // stop when reaching this, there may be padding ahead
          if(op == CodeViewInfo::BA_OP_Invalid)
            break;

          iter++;

          uint32_t parameter = CodeViewInfo::CVUncompressData(iter);

          uint32_t parameter2 = 0;
          if(CodeViewInfo::BinaryAnnotationInstructionOperandCount(op) == 2)
            parameter2 = CodeViewInfo::CVUncompressData(iter);

          bool apply = false;

          // apply op to current state
          switch(op)
          {
            case CodeViewInfo::BA_OP_Invalid: break;
            case CodeViewInfo::BA_OP_CodeOffset: codeOffset = parameter; break;
            case CodeViewInfo::BA_OP_ChangeCodeOffsetBase: codeOffsetBase = parameter; break;
            case CodeViewInfo::BA_OP_ChangeCodeOffset:
              codeOffset += parameter;
              apply = true;
              break;
            case CodeViewInfo::BA_OP_ChangeCodeLength:
            {
              // this applies to the previous/last instruction.
              if(!inlinee.locations.empty() &&
                 inlinee.locations.back().offsetEnd == inlinee.locations.back().offsetStart &&
                 inlinee.locations.back().statement == statement)
              {
                inlinee.locations.back().offsetEnd += parameter;
              }
              else
              {
                RDCERR("No valid previous instruction to apply codeLength to");
              }

              codeOffset += parameter;
              break;
            }
            case CodeViewInfo::BA_OP_ChangeFile:
              RDCERR("Unsupported change of file within inline site!");
              break;
            case CodeViewInfo::BA_OP_ChangeLineOffset:
            {
              currentLine += CodeViewInfo::DecodeSignedInt32(parameter);
              break;
            }
            case CodeViewInfo::BA_OP_ChangeLineEndDelta: currentLineLength = parameter; break;
            case CodeViewInfo::BA_OP_ChangeRangeKind:
            {
              statement = (parameter == 1);
              break;
            }
            case CodeViewInfo::BA_OP_ChangeColumnStart: currentColStart = parameter; break;
            case CodeViewInfo::BA_OP_ChangeColumnEndDelta:
            {
              currentColEnd += CodeViewInfo::DecodeSignedInt32(parameter);
              break;
            }
            case CodeViewInfo::BA_OP_ChangeCodeOffsetAndLineOffset:
            {
              uint32_t CodeDelta = parameter & 0xf;
              uint32_t sourceDelta = parameter >> 4;
              codeOffset += CodeDelta;
              currentLine += CodeViewInfo::DecodeSignedInt32(sourceDelta);
              apply = true;
              break;
            }
            case CodeViewInfo::BA_OP_ChangeCodeLengthAndCodeOffset:
            {
              codeLength = parameter;
              codeOffset += parameter2;
              apply = true;
              break;
            }
            case CodeViewInfo::BA_OP_ChangeColumnEnd: currentColEnd = parameter; break;
          }

          if(apply)
          {
            InstructionLocation loc;

            loc.statement = statement;
            loc.offsetStart = codeOffsetBase + codeOffset;
            loc.offsetEnd = loc.offsetStart + codeLength;
            loc.colStart = currentColStart;
            loc.colEnd = currentColEnd;
            loc.lineStart = currentLine;
            loc.lineEnd = currentLine + currentLineLength;

            // the behaviour seems to be that if codeLength is ephemeral, not sticky like the rest
            codeLength = 0;

            // if we have a previous location with implicit length, fix it up now
            if(!inlinee.locations.empty() &&
               inlinee.locations.back().offsetEnd == inlinee.locations.back().offsetStart &&
               inlinee.locations.back().statement == loc.statement)
            {
              inlinee.locations.back().offsetEnd = loc.offsetStart;
            }

            inlinee.locations.push_back(loc);

            SPDBLOG("inline annotation of %s, from %x (length %x), from %u:%u to %u:%u",
                    statement ? "statement" : "expression", loc.offsetStart,
                    loc.offsetEnd - loc.offsetStart, currentLine, currentColStart,
                    currentLine + currentLineLength, currentColEnd);
          }
        }

        inlines.push_back(inlinee);
      }
      else if(type == S_LOCAL)
      {
        LOCALSYM *local = (LOCALSYM *)sym;

        SPDBLOG("S_LOCAL: '%s' of type %x", local->name, local->typind);

        localName = (char *)local->name;
        localType = local->typind;

        if(local->flags.fIsParam)
          SPDBLOG("  fIsParam: variable is a parameter");
        if(local->flags.fAddrTaken)
          SPDBLOG("  fAddrTaken: address is taken");
        if(local->flags.fCompGenx)
          SPDBLOG("  fCompGenx: variable is compiler generated");
        if(local->flags.fIsAggregate)
          SPDBLOG(
              "  fIsAggregate: the symbol is splitted in temporaries, "
              "which are treated by compiler as independent entities");
        if(local->flags.fIsAggregated)
          SPDBLOG("  fIsAggregated: variable is a part of a fIsAggregate symbol");
        if(local->flags.fIsAliased)
          SPDBLOG("  fIsAliased: variable has multiple simultaneous lifetimes");
        if(local->flags.fIsAlias)
          SPDBLOG("  fIsAlias: variable represents one of the multiple simultaneous lifetimes");
        if(local->flags.fIsRetValue)
          SPDBLOG("  fIsRetValue: variable represents a function return value");
        if(local->flags.fIsOptimizedOut)
          SPDBLOG("  fIsOptimizedOut: variable variable has no lifetimes");
        if(local->flags.fIsEnregGlob)
          SPDBLOG("  fIsEnregGlob: variable is an enregistered global");
        if(local->flags.fIsEnregStat)
          SPDBLOG("  fIsEnregStat: variable is an enregistered static");
      }
      else if(type == S_DEFRANGE_HLSL)
      {
        DEFRANGESYMHLSL *defrange = (DEFRANGESYMHLSL *)sym;

        LocalMapping mapping;
        RegisterRange &range = mapping.var.registers[0];

        bool indexable = false;
        const char *regtype = "";
        const char *regprefix = "?";

        // CV_HLSLREG_e == OperandType

        switch((OperandType)defrange->regType)
        {
          case TYPE_TEMP:
            range.type = RegisterType::Temporary;
            regtype = "temp";
            regprefix = "r";
            break;
          case TYPE_INPUT:
          case TYPE_INPUT_PRIMITIVEID:
          case TYPE_INPUT_FORK_INSTANCE_ID:
          case TYPE_INPUT_JOIN_INSTANCE_ID:
          case TYPE_INPUT_CONTROL_POINT:
          case TYPE_INPUT_PATCH_CONSTANT:
          case TYPE_INPUT_DOMAIN_POINT:
          case TYPE_INPUT_THREAD_ID:
          case TYPE_INPUT_THREAD_GROUP_ID:
          case TYPE_INPUT_THREAD_ID_IN_GROUP:
          case TYPE_INPUT_COVERAGE_MASK:
          case TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED:
          case TYPE_INPUT_GS_INSTANCE_ID:
            range.type = RegisterType::Input;
            regtype = "input";
            regprefix = "v";
            break;
          case TYPE_OUTPUT:
          case TYPE_OUTPUT_DEPTH:
          case TYPE_OUTPUT_DEPTH_LESS_EQUAL:
          case TYPE_OUTPUT_DEPTH_GREATER_EQUAL:
          case TYPE_OUTPUT_STENCIL_REF:
          case TYPE_OUTPUT_COVERAGE_MASK:
            range.type = RegisterType::Output;
            regtype = "output";
            regprefix = "o";
            break;
          case TYPE_INDEXABLE_TEMP:
            range.type = RegisterType::IndexedTemporary;
            regtype = "indexable";
            regprefix = "x";
            indexable = true;
            break;
          default: break;
        }

        // this is a virtual register, not stored
        if((OperandType)defrange->regType == TYPE_STREAM)
          continue;

        const char *space = "";
        switch((CV_HLSLMemorySpace_e)defrange->memorySpace)
        {
          case CV_HLSL_MEMSPACE_DATA: space = "data"; break;
          case CV_HLSL_MEMSPACE_SAMPLER: space = "sampler"; break;
          case CV_HLSL_MEMSPACE_RESOURCE: space = "resource"; break;
          case CV_HLSL_MEMSPACE_RWRESOURCE: space = "rwresource"; break;
          default: break;
        }

        SPDBLOG("S_DEFRANGE_HLSL: %u->%u bytes in parent: %s %s (dim %d) %s",
                defrange->offsetParent, defrange->offsetParent + defrange->sizeInParent, regtype,
                space, defrange->regIndices, defrange->spilledUdtMember ? "spilled" : "");

        if(defrange->regIndices > 1)
        {
          RDCWARN("More than one register index in mapping");
          // this is used for geometry shader inputs for example
        }

        uint32_t regoffset = *CV_DEFRANGESYMHLSL_OFFSET_CONST_PTR(defrange);

        char regcomps[] = "xyzw";

        uint32_t regindex = indexable ? regoffset : regoffset / 16;
        uint32_t regfirstcomp = indexable ? 0 : (regoffset % 16) / 4;
        uint32_t regnumcomps = indexable ? 4 : defrange->sizeInParent / 4;

        bool builtinoutput = false;
        mapping.var.builtin = ShaderBuiltin::Undefined;
        switch((OperandType)defrange->regType)
        {
          case TYPE_OUTPUT_DEPTH:
            builtinoutput = true;
            mapping.var.builtin = ShaderBuiltin::DepthOutput;
            break;
          case TYPE_OUTPUT_DEPTH_LESS_EQUAL:
            builtinoutput = true;
            mapping.var.builtin = ShaderBuiltin::DepthOutputLessEqual;
            break;
          case TYPE_OUTPUT_DEPTH_GREATER_EQUAL:
            builtinoutput = true;
            mapping.var.builtin = ShaderBuiltin::DepthOutputGreaterEqual;
            break;
          case TYPE_OUTPUT_STENCIL_REF:
            builtinoutput = true;
            mapping.var.builtin = ShaderBuiltin::StencilReference;
            break;
          case TYPE_OUTPUT_COVERAGE_MASK:
            builtinoutput = true;
            mapping.var.builtin = ShaderBuiltin::MSAACoverage;
            break;
          case TYPE_INPUT_PRIMITIVEID: mapping.var.builtin = ShaderBuiltin::PrimitiveIndex; break;
          case TYPE_INPUT_COVERAGE_MASK: mapping.var.builtin = ShaderBuiltin::MSAACoverage; break;
          case TYPE_INPUT_THREAD_ID:
            mapping.var.builtin = ShaderBuiltin::DispatchThreadIndex;
            break;
          case TYPE_INPUT_THREAD_GROUP_ID: mapping.var.builtin = ShaderBuiltin::GroupIndex; break;
          case TYPE_INPUT_THREAD_ID_IN_GROUP:
            mapping.var.builtin = ShaderBuiltin::GroupThreadIndex;
            break;
          case TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED:
            mapping.var.builtin = ShaderBuiltin::GroupFlatIndex;
            break;
          case TYPE_INPUT_GS_INSTANCE_ID:
            mapping.var.builtin = ShaderBuiltin::GSInstanceIndex;
            break;
          default: break;
        }

        if(mapping.var.builtin != ShaderBuiltin::Undefined)
        {
          bool found = false;

          if(builtinoutput)
          {
            for(size_t i = 0; i < dxbc->m_OutputSig.size(); i++)
            {
              if(dxbc->m_OutputSig[i].systemValue == mapping.var.builtin)
              {
                regindex = (uint32_t)i;
                regfirstcomp = 0;
                found = true;
                break;
              }
            }
          }
          else
          {
            for(size_t i = 0; i < dxbc->m_InputSig.size(); i++)
            {
              if(dxbc->m_InputSig[i].systemValue == mapping.var.builtin)
              {
                regindex = (uint32_t)i;
                regfirstcomp = 0;
                found = true;
                break;
              }
            }
          }

          // if not found in the signatures, then it's a fixed-function input like threadid - it
          // will be matched by builtin
          if(!found)
            regindex = ~0U;
        }

        char *regswizzle = regcomps;
        regswizzle += regfirstcomp;
        regswizzle[regnumcomps] = 0;

        SPDBLOG("Stored in %s%u.%s", regprefix, regindex, regswizzle);

        mapping.var.localName = localName;

        uint32_t varOffset = defrange->offsetParent;
        uint32_t varLen = defrange->sizeInParent;

        const TypeDesc *vartype = &typeInfo[localType];

        RDCASSERT(varOffset + varLen <= vartype->byteSize);

        // step through struct members
        while(!vartype->members.empty())
        {
          bool found = false;

          // find the child member this register corresponds to. We don't handle overlaps between
          // members
          for(const TypeMember &mem : vartype->members)
          {
            TypeDesc &childType = typeInfo[mem.typeIndex];

            uint32_t memberOffset = mem.byteOffset;
            uint32_t memberLen = childType.byteSize;

            // if this member is before our variable, continue
            if(memberOffset + memberLen <= varOffset)
              continue;

            // if this member is after our variable, continue (though if members are sorted, this
            // means we won't find a candidate member)
            if(varOffset + varLen <= memberOffset)
              continue;

            if(memberOffset > varOffset || memberOffset + memberLen < varOffset + varLen)
            {
              RDCERR("member %s of %s doesn't fully enclose variable [%u,%u] vs [%u,%u]",
                     mem.name.c_str(), vartype->name.c_str(), memberOffset,
                     memberOffset + memberLen, varOffset, varOffset + varLen);
            }

            mapping.var.localName = std::string(mapping.var.localName) + "." + mem.name;

            // subtract off the offset of this member so we're now relative to it - since it might
            // be a struct itself and we need to recurse.
            varOffset -= memberOffset;

            vartype = &childType;

            found = true;

            break;
          }

          if(!found)
          {
            RDCERR("No member of %s corresponds to variable range [%u,%u]", vartype->name.c_str(),
                   varOffset, varOffset + varLen);
            mapping.var.localName = std::string(mapping.var.localName) + ".__unknown__";
            break;
          }
        }

        mapping.var.type = vartype->baseType;
        mapping.var.rows = 1;
        mapping.var.columns = vartype->vecSize;

        // if it's an array or matrix, figure out the index
        if(vartype->matArrayStride)
        {
          uint32_t idx = varOffset / vartype->matArrayStride;

          mapping.var.localName = StringFormat::Fmt("%s[%u]", mapping.var.localName.c_str(), idx);
          mapping.var.rows = RDCMAX(
              1U, (vartype->byteSize + (vartype->matArrayStride - 1)) / vartype->matArrayStride);

          varOffset -= vartype->matArrayStride * idx;
        }

        if(vartype->leafType != LF_MATRIX)
        {
          mapping.var.elements = mapping.var.rows;
          mapping.var.rows = 1;
        }

        RDCASSERT(mapping.var.rows <= 4 && mapping.var.columns <= 4);

        range.index = uint16_t(regindex & 0xffff);
        mapping.regFirstComp = regfirstcomp;
        mapping.varFirstComp = (varOffset % 16) / 4;
        mapping.numComps = regnumcomps;

        SPDBLOG("Valid from %x to %x", defrange->range.offStart,
                defrange->range.offStart + defrange->range.cbRange);

        mapping.range.startRange = defrange->range.offStart;
        mapping.range.endRange = defrange->range.offStart + defrange->range.cbRange;

        const CV_LVAR_ADDR_GAP *gaps = CV_DEFRANGESYMHLSL_GAPS_CONST_PTR(defrange);
        size_t gapcount = CV_DEFRANGESYMHLSL_GAPS_COUNT(defrange);
        if(gapcount > 0)
          SPDBLOG("Except for in:");
        for(size_t i = 0; i < gapcount; i++)
        {
          SPDBLOG("  Gap %zu: %x -> %x", i, defrange->range.offStart + gaps[i].gapStartOffset,
                  defrange->range.offStart + gaps[i].gapStartOffset + gaps[i].cbRange);

          LocalRange r = {defrange->range.offStart + gaps[i].gapStartOffset,
                          defrange->range.offStart + gaps[i].gapStartOffset + gaps[i].cbRange};

          mapping.gaps.push_back(r);
        }

        m_Locals.push_back(mapping);
      }
      else if(type == S_INLINESITE_END)
      {
        SPDBLOG("S_INLINESITE_END");
      }
      else if(type == S_END)
      {
        SPDBLOG("S_END");
      }
      else
      {
        SPDBLOG("Unhandled type %04x", type);
      }
    }
    RDCASSERT(cur == end);

    end = cur + modules[m].cbLines;

    while(cur < end)
    {
      CV_DebugSSubsectionHeader_t *subsection = (CV_DebugSSubsectionHeader_t *)cur;

      byte *substart = (byte *)(subsection + 1);

      cur += sizeof(CV_DebugSSubsectionHeader_t) + subsection->cbLen;

      byte *subend = cur;

      if(subsection->type == DEBUG_S_FILECHKSMS)    // hash
      {
        byte *iter = substart;
        while(iter < subend)
        {
          FileChecksum *checksum = (FileChecksum *)iter;

          uint32_t chunkOffs = uint32_t(iter - substart);

          iter += offsetof(FileChecksum, hashData);

          std::string name;

          if(Names.find(checksum->nameIndex) != Names.end())
          {
            name = Names[checksum->nameIndex];
            if(name.empty())
              name = Names[checksum->nameIndex] = "shader";
          }
          else
          {
            RDCERR("Encountered nameoffset %u that doesn't match any name.", checksum->nameIndex);
          }

          CV_SourceChksum_t hashType = (CV_SourceChksum_t)checksum->hashType;

          if(hashType != CHKSUM_TYPE_NONE)
          {
            byte hash[256];
            memcpy(hash, checksum->hashData, checksum->hashLength);

            char hashstr[16 * 2 + 1] = {0};
            char hex[] = "0123456789abcdef";

            for(uint8_t i = 0; i < RDCMIN(uint8_t(16), checksum->hashLength); i++)
            {
              hashstr[i * 2 + 0] = hex[(hash[i] & 0xf0) >> 4];
              hashstr[i * 2 + 1] = hex[(hash[i] & 0x0f) >> 0];
            }

            SPDBLOG("File %s has checksum %s%s", name.c_str(), hashstr,
                    checksum->hashLength > 16 ? "..." : "");

            int32_t fileIdx = -1;

            for(size_t i = 0; i < Files.size(); i++)
            {
              if(!_stricmp(Files[i].first.c_str(), name.c_str()))
              {
                fileIdx = (int32_t)i;
                break;
              }
            }

            FileMapping[chunkOffs] = fileIdx;
          }
          else
          {
            // this is a 'virtual' file. Create a source file that we can map lines to just for
            // something,
            // as we won't be able to reliably get the real source lines back. The PDB lies
            // convincingly about the
            // source according to #line
            if(!name.empty())
            {
              Files.push_back({name, ""});

              FileMapping[chunkOffs] = (int32_t)Files.size() - 1;
            }
            else
            {
              FileMapping[chunkOffs] = -1;
            }
          }

          iter = AlignUpPtr(iter + checksum->hashLength, 4);
        }
        RDCASSERT(iter == subend);
      }
      else if(subsection->type == DEBUG_S_LINES)
      {
        CV_DebugSLinesHeader_t *hdr = (CV_DebugSLinesHeader_t *)substart;

        bool hasColumns = (hdr->flags & CV_LINES_HAVE_COLUMNS);

        byte *iter = (byte *)(hdr + 1);
        while(iter < subend)
        {
          CV_DebugSLinesFileBlockHeader_t *file = (CV_DebugSLinesFileBlockHeader_t *)iter;
          CV_Line_t *lines = (CV_Line_t *)(file + 1);
          CV_Column_t *columns = (CV_Column_t *)(lines + file->nLines);

          iter = (byte *)file + file->cbBlock;

          int32_t fileIdx = -1;

          if(FileMapping.find(file->offFile) == FileMapping.end())
          {
            RDCERR(
                "SPDB chunk - line numbers file references index %u not encountered in file "
                "mapping",
                file->offFile);
          }
          else
          {
            fileIdx = FileMapping[file->offFile];
          }

          for(CV_off32_t l = 0; l < file->nLines; l++)
          {
            CV_Line_t &line = lines[l];

            LineColumnInfo lineCol;
            lineCol.fileIndex = fileIdx;
            lineCol.lineStart = line.linenumStart;
            lineCol.lineEnd = line.linenumStart + line.deltaLineEnd;

            if(hasColumns)
            {
              CV_Column_t &col = columns[l];
              lineCol.colStart = col.offColumnStart;
              lineCol.colEnd = col.offColumnEnd;
            }

            m_Lines[line.offset] = lineCol;
          }
        }
        RDCASSERT(iter == subend);
      }
      else if(subsection->type == DEBUG_S_INLINEELINES)
      {
        byte *iter = substart;
        uint32_t sourceLineType = *(uint32_t *)iter;
        iter += sizeof(uint32_t);

        if(sourceLineType == CV_INLINEE_SOURCE_LINE_SIGNATURE)
        {
          CodeViewInfo::InlineeSourceLine *inlinee = (CodeViewInfo::InlineeSourceLine *)iter;
          size_t count = (subend - iter) / sizeof(CodeViewInfo::InlineeSourceLine);
          for(size_t i = 0; i < count; i++, inlinee++)
          {
            inlines[i].id = inlinee->inlinee;
            inlines[i].fileOffs = inlinee->fileId;
            inlines[i].baseLineNum = inlinee->sourceLineNum;
          }
        }
        else if(sourceLineType == CV_INLINEE_SOURCE_LINE_SIGNATURE_EX)
        {
          size_t idx = 0;
          while(iter < subend)
          {
            CodeViewInfo::InlineeSourceLineEx *inlinee = (CodeViewInfo::InlineeSourceLineEx *)iter;

            iter += sizeof(CodeViewInfo::InlineeSourceLineEx) +
                    sizeof(CV_off32_t) * inlinee->countOfExtraFiles;

            inlines[idx].id = inlinee->inlinee;
            inlines[idx].fileOffs = inlinee->fileId;
            inlines[idx].baseLineNum = inlinee->sourceLineNum;
            idx++;
          }
        }
      }
      else
      {
        break;
      }
    }
  }

  for(auto it = m_Lines.begin(); it != m_Lines.end(); ++it)
    it->second.callstack.push_back(m_Functions[0].name);

  SPDBLOG("Applying %zu inline sites", inlines.size());

  for(size_t i = 0; i < inlines.size(); i++)
  {
    RDCASSERT(inlines[i].locations.size() > 1);

    if(inlines[i].locations.empty() || inlines[i].locations.size() == 1)
    {
      RDCWARN("Skipping patching function call with %d locations", inlines[i].locations.size());
      continue;
    }

    RDCASSERT(FileMapping.find(inlines[i].fileOffs) != FileMapping.end());

    if(FileMapping.find(inlines[i].fileOffs) == FileMapping.end())
    {
      RDCWARN("Got function call patch with fileoffs %x - skipping", inlines[i].fileOffs);
      continue;
    }

    SPDBLOG("Inline site %zu", i);

    int32_t fileIdx = FileMapping[inlines[i].fileOffs];

    for(size_t j = 0; j < inlines[i].locations.size(); j++)
    {
      InstructionLocation &loc = inlines[i].locations[j];

      int nPatched = 0;

      auto it = m_Lines.lower_bound(loc.offsetStart);

      for(; it != m_Lines.end() && it->first <= loc.offsetEnd; ++it)
      {
        if((it->first >= loc.offsetStart && it->first < loc.offsetEnd) ||
           (it->first == loc.offsetStart && it->first == loc.offsetEnd))
        {
          SPDBLOG("Patching %x between [%x,%x] from (%d %u:%u -> %u:%u) into (%d %u:%u -> %u:%u)",
                  it->first, loc.offsetStart, loc.offsetEnd, it->second.fileIndex,
                  it->second.lineStart, it->second.colStart, it->second.lineEnd, it->second.colEnd,
                  fileIdx, loc.lineStart + inlines[i].baseLineNum, loc.colStart,
                  loc.lineEnd + inlines[i].baseLineNum, loc.colEnd);

          it->second.fileIndex = fileIdx;
          it->second.lineStart = loc.lineStart + inlines[i].baseLineNum;
          it->second.lineEnd = loc.lineEnd + inlines[i].baseLineNum;
          it->second.colStart = loc.colStart;
          it->second.colEnd = loc.colEnd;
          if(loc.statement)
            it->second.callstack.push_back(m_Functions[inlines[i].id].name);
          nPatched++;
        }
      }

      if(nPatched == 0)
        RDCWARN("Can't find anything between offsets %x,%x as desired", loc.offsetStart,
                loc.offsetEnd);
    }
  }

  delete[] pages;

  // save the filenames in their original order
  std::vector<std::string> filenames;
  filenames.reserve(Files.size());
  for(size_t i = 0; i < Files.size(); i++)
    filenames.push_back(Files[i].first);

  // Sort files according to the order they come in the Names array, this seems to be more reliable
  // about placing the main file first.
  std::sort(Files.begin(), Files.end(), [&Names](const rdcpair<std::string, std::string> &a,
                                                 const rdcpair<std::string, std::string> &b) {
    // any entries that aren't found in Names at all (like @cmdline that we add) will be sorted to
    // the end.
    size_t aIdx = ~0U, bIdx = ~0U;

    size_t i = 0;
    for(auto it = Names.begin(); it != Names.end(); ++it)
    {
      if(it->second == a.first)
        aIdx = i;
      if(it->second == b.first)
        bIdx = i;

      i++;
    }

    // if neither were found, sort by filename
    if(aIdx == bIdx)
      return a.first < b.first;

    return aIdx < bIdx;
  });

  // create a map from filename -> index
  std::map<std::string, int32_t> remapping;
  for(size_t i = 0; i < Files.size(); i++)
    remapping[Files[i].first] = (int32_t)i;

  // remap the line info by looking up the original intended filename, then looking up the new index
  for(auto it = m_Lines.begin(); it != m_Lines.end(); ++it)
  {
    if(it->second.fileIndex == -1)
      continue;
    it->second.fileIndex = remapping[filenames[it->second.fileIndex]];
  }

  std::sort(m_Locals.begin(), m_Locals.end());

  m_HasDebugInfo = true;
}

void SPDBChunk::GetLineInfo(size_t instruction, uintptr_t offset, LineColumnInfo &lineInfo) const
{
  auto it = m_Lines.lower_bound((uint32_t)offset);

  if(it != m_Lines.end() && (uintptr_t)it->first <= offset)
    lineInfo = it->second;
}

bool SPDBChunk::HasLocals() const
{
  return true;
}

void SPDBChunk::GetLocals(size_t instruction, uintptr_t offset,
                          rdcarray<LocalVariableMapping> &locals) const
{
  locals.clear();

  for(auto it = m_Locals.begin(); it != m_Locals.end(); ++it)
  {
    if(it->range.startRange > offset)
      break;

    if(it->range.endRange <= offset)
      continue;

    bool ingap = false;

    for(auto gapit = it->gaps.begin(); gapit != it->gaps.end(); gapit++)
    {
      if(gapit->startRange >= offset && gapit->endRange < offset)
      {
        ingap = true;
        break;
      }
    }

    if(ingap)
      continue;

    bool added = false;

    // we apply each matching local over the top. Where there is an overlap (e.g. two variables with
    // the same name) we take the last mapping as authoratitive. This is a good solution for the
    // case where one function with a parameter/variable name calls an inner function with the same
    // parameter name and there's shadowing. The later mapping will be for the inner function so we
    // use it in preference.

    // check if we already have a mapping for this variable
    for(LocalVariableMapping &a : locals)
    {
      const LocalVariableMapping &b = it->var;

      if(a.localName == b.localName)
      {
        RegisterRange range = b.registers[0];

        for(uint32_t i = 0; i < it->numComps; i++)
        {
          a.registers[it->varFirstComp + i].type = b.registers[0].type;
          a.registers[it->varFirstComp + i].index = b.registers[0].index;
          a.registers[it->varFirstComp + i].component = uint16_t(it->regFirstComp + i);
        }

        a.regCount = RDCMAX(a.regCount, it->varFirstComp + it->numComps);

        // we've processed this, no need to add a new entry
        added = true;
        break;
      }
    }

    if(!added)
    {
      locals.push_back(it->var);
      LocalVariableMapping &a = locals.back();

      // the register range is stored in [0] but we don't want to actually push that, so make it
      // undefined and grab it locally
      RegisterRange range;
      std::swap(a.registers[0], range);

      for(uint32_t i = 0; i < it->numComps; i++)
      {
        a.registers[it->varFirstComp + i].type = range.type;
        a.registers[it->varFirstComp + i].index = range.index;
        a.registers[it->varFirstComp + i].component = uint16_t(it->regFirstComp + i);
      }

      a.regCount = RDCMAX(it->var.columns, it->varFirstComp + it->numComps);
    }
  }
}

};    // namespace DXBC