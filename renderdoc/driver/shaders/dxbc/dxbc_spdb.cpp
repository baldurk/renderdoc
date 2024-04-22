/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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
#include "common/formatting.h"
#include "dxbc_container.h"

#include "official/cvinfo.h"
#include "os/os_specific.h"
#include "dxbc_spdb.h"

// uncomment the following to print (very verbose) debugging prints for SPDB processing
// #define SPDBLOG(...) RDCDEBUG(__VA_ARGS__)

#ifndef SPDBLOG
#define SPDBLOG(...) (void)(__VA_ARGS__)
#endif

namespace DXBC
{
bool IsPDBFile(void *data, size_t length)
{
  FileHeaderPage *header = (FileHeaderPage *)data;

  if(length < sizeof(FileHeaderPage))
    return false;

  if(memcmp(header->identifier, "Microsoft C/C++ MSF 7.00\r\n\032DS\0\0",
            sizeof(header->identifier)) != 0)
    return false;

  return true;
}

SPDBChunk::SPDBChunk(byte *data, uint32_t spdblength)
{
  m_HasDebugInfo = false;

  m_ShaderFlags = 0;

  FileHeaderPage *header = (FileHeaderPage *)data;

  if(!IsPDBFile(data, spdblength))
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

  rdcarray<PDBStream> streams;

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

  std::map<rdcstr, uint32_t> StreamNames;

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
        filename = "unnamed_shader";

      Files.push_back({filename, rdcstr((const char *)fileContents.Data(), s.byteLength)});
    }
  }

  struct TypeMember
  {
    rdcstr name;
    uint16_t byteOffset;
    uint32_t typeIndex;
  };

  struct TypeDesc
  {
    rdcstr name;
    VarType baseType;
    uint32_t byteSize;
    uint16_t vecSize;
    uint16_t matArrayStride : 15;
    uint16_t colMajorMatrix : 1;
    LEAF_ENUM_e leafType;
    rdcarray<TypeMember> members;
  };

  std::map<uint32_t, TypeDesc> typeInfo;

  // prepopulate with basic types
  // for now we stick to full-precision 32-bit VarTypes. It's not clear if HLSL even emits the other
  // types
  typeInfo[T_INT4] = {"int32_t", VarType::SInt, 4, 1, 0, 0, LF_NUMERIC, {}};
  typeInfo[T_INT2] = {"int16_t", VarType::SInt, 2, 1, 0, 0, LF_NUMERIC, {}};
  typeInfo[T_INT1] = {"int8_t", VarType::SInt, 1, 1, 0, 0, LF_NUMERIC, {}};
  typeInfo[T_LONG] = {"int32_t", VarType::SInt, 4, 1, 0, 0, LF_NUMERIC, {}};
  typeInfo[T_SHORT] = {"int16_t", VarType::SInt, 2, 1, 0, 0, LF_NUMERIC, {}};
  typeInfo[T_CHAR] = {"char", VarType::SInt, 1, 1, 0, 0, LF_NUMERIC, {}};
  typeInfo[T_BOOL32FF] = {"bool", VarType::Bool, 4, 1, 0, 0, LF_NUMERIC, {}};
  typeInfo[T_UINT4] = {"uint32_t", VarType::UInt, 4, 1, 0, 0, LF_NUMERIC, {}};
  typeInfo[T_UINT2] = {"uint16_t", VarType::UInt, 2, 1, 0, 0, LF_NUMERIC, {}};
  typeInfo[T_UINT1] = {"uint8_t", VarType::UInt, 1, 1, 0, 0, LF_NUMERIC, {}};
  typeInfo[T_ULONG] = {"uint32_t", VarType::UInt, 4, 1, 0, 0, LF_NUMERIC, {}};
  typeInfo[T_USHORT] = {"uint16_t", VarType::UInt, 2, 1, 0, 0, LF_NUMERIC, {}};
  typeInfo[T_UCHAR] = {"unsigned char", VarType::UInt, 1, 1, 0, 0, LF_NUMERIC, {}};
  typeInfo[T_REAL16] = {"half", VarType::Float, 2, 1, 0, 0, LF_NUMERIC, {}};
  typeInfo[T_REAL32] = {"float", VarType::Float, 4, 1, 0, 0, LF_NUMERIC, {}};
  // modern HLSL fake half
  typeInfo[T_REAL32PP] = {"half", VarType::Float, 4, 1, 0, 0, LF_NUMERIC, {}};
  typeInfo[T_REAL64] = {"double", VarType::Double, 8, 1, 0, 0, LF_NUMERIC, {}};

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
              0,           0,
              type,        {},
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

          uint16_t vecSize = 0, matStride = 0;

          if(matrix->matattr.row_major)
          {
            vecSize = uint16_t(matrix->cols);
            matStride = uint16_t(*bytelength / matrix->rows);
          }
          else
          {
            vecSize = uint16_t(matrix->rows);
            matStride = uint16_t(*bytelength / matrix->cols);
          }

          typeInfo[id] = {
              name,      typeInfo[matrix->elemtype].baseType, *bytelength, vecSize,
              matStride, matrix->matattr.row_major == 0,      type,        {},
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

          rdcarray<TypeMember> &members = typeInfo[id].members;

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
              case LF_METHOD:
              {
                lfMethod *method = (lfMethod *)iter;

                SPDBLOG("  [%u]: Method %s used %u times in method list %u", idx, method->Name,
                        method->count, method->mList);

                idx++;
                iter = bytes;
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
              name, VarType::Float, *bytelength, 1, 0, 0, type, typeInfo[structure->field].members,
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
        case LF_METHODLIST:
        {
          lfMethodList *mlist = (lfMethodList *)leaf;
          (void)mlist;
          SPDBLOG("Type %x is a method list", id);
          break;
        }
        case LF_STRIDED_ARRAY:
        {
          lfStridedArray *stridedArray = (lfStridedArray *)leaf;
          // documentation isn't clear, but seems like byte size is always a uint16_t
          uint16_t bytelength = *(uint16_t *)stridedArray->data;

          uint16_t stride = uint16_t(stridedArray->stride);

          if(bytelength == 0)
          {
            // busted debug info - don't trust the stride
            stride = typeInfo[stridedArray->elemtype].byteSize & 0xffff;
          }

          typeInfo[id] = {
              "", typeInfo[stridedArray->elemtype].baseType, bytelength, 1, stride, 0, type, {},
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

  std::map<uint32_t, rdcstr> Names;

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

  rdcarray<DBIModule> modules;

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
      cur += sizeof(DBIModule) - sizeof(rdcstr) * 2;

      char *moduleName = (char *)cur;
      cur += strlen(moduleName) + 1;

      char *objectName = (char *)cur;
      cur += strlen(objectName) + 1;

      // align up to DWORD boundary
      while((uintptr_t)cur & 0x3)
        cur++;

      DBIModule m;
      memcpy(&m, mod, sizeof(DBIModule) - sizeof(rdcstr) * 2);
      m.moduleName = moduleName;
      m.objectName = objectName;

      SPDBLOG("Got module named %s from object %s", moduleName, objectName);

      modules.push_back(m);
    }
    RDCASSERT(cur == end);
  }

  rdcarray<Inlinee> inlines;

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

    rdcstr localName;
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
            rdcstr cmdlineDefines = "// Command line defines:\n\n";

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
                  cmdlineDefines += rdcstr(defstart, defend - defstart) + " " +
                                    rdcstr(valstart, valend - valstart);
                  cmdlineDefines += "\n";
                }
                else
                {
                  cmdlineDefines += "#define ";
                  cmdlineDefines += rdcstr(defstart, defend - defstart);
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

        // CV_HLSLREG_e == OperandType

        mapping.regType = (DXBCBytecode::OperandType)defrange->regType;

        // this is a virtual register, not stored
        if((DXBCBytecode::OperandType)defrange->regType == DXBCBytecode::TYPE_STREAM)
          continue;

        const char *spaces[16] = {"data", "sampler", "resource", "rwresource"};

        SPDBLOG("S_DEFRANGE_HLSL: %u->%u bytes in parent: %s %s (dim %d) %s",
                defrange->offsetParent, defrange->offsetParent + defrange->sizeInParent,
                ToStr(mapping.regType).c_str(), spaces[defrange->memorySpace], defrange->regIndices,
                defrange->spilledUdtMember ? "spilled" : "");

        if(defrange->regIndices > 1)
        {
          RDCWARN("More than one register index in mapping");
          // this is used for geometry shader inputs for example
        }

        uint32_t regoffset = *CV_DEFRANGESYMHLSL_OFFSET_CONST_PTR(defrange);

        char regcomps[] = "xyzw";

        const bool indexable = (mapping.regType == DXBCBytecode::TYPE_INDEXABLE_TEMP);

        mapping.regIndex = indexable ? regoffset : regoffset / 16;
        mapping.regFirstComp = indexable ? 0 : (regoffset % 16) / 4;
        mapping.numComps = indexable ? 4 : defrange->sizeInParent / 4;

        char *regswizzle = regcomps;
        regswizzle += mapping.regFirstComp;
        regswizzle[mapping.numComps] = 0;

        SPDBLOG("Stored in %s %u.%s", ToStr(mapping.regType).c_str(), mapping.regIndex, regswizzle);

        mapping.var.name = localName;

        uint32_t varOffset = defrange->offsetParent;
        uint32_t varLen = defrange->sizeInParent;

        mapping.varOffset = varOffset;

        const TypeDesc *vartype = &typeInfo[localType];

        RDCASSERT((varOffset + varLen <= vartype->byteSize) ||
                      (vartype->byteSize == 0 && vartype->leafType == LF_STRIDED_ARRAY),
                  varOffset, varLen, vartype->byteSize, (uint32_t)vartype->leafType);

        uint32_t varTypeByteSize = vartype->byteSize;

        // step through struct members
        while(!vartype->members.empty())
        {
          bool found = false;

          // find the child member this register corresponds to. We don't handle overlaps between
          // members
          for(size_t memIndex = 0; memIndex < vartype->members.size(); memIndex++)
          {
            const TypeMember &mem = vartype->members[memIndex];

            TypeDesc &childType = typeInfo[mem.typeIndex];

            uint32_t memberOffset = mem.byteOffset;
            uint32_t memberLen = childType.byteSize;

            if(memberLen == 0)
            {
              // if the member length is 0 this is busted debug info from fxc, so assume the member
              // runs up to the end of the struct or the next member
              if(memIndex == vartype->members.size() - 1)
                memberLen = vartype->byteSize - memberOffset;
              else
                memberLen = vartype->members[memIndex + 1].byteOffset - memberOffset;
            }

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

            mapping.var.name += "." + mem.name;

            // subtract off the offset of this member so we're now relative to it - since it might
            // be a struct itself and we need to recurse.
            varOffset -= memberOffset;

            vartype = &childType;
            varTypeByteSize = memberLen;

            found = true;

            break;
          }

          if(!found)
          {
            RDCERR("No member of %s corresponds to variable range [%u,%u]", vartype->name.c_str(),
                   varOffset, varOffset + varLen);
            mapping.var.name += ".__unknown__";
            break;
          }
        }

        mapping.var.baseType = vartype->baseType;
        mapping.var.rows = 1;
        mapping.var.columns = uint8_t(vartype->vecSize);
        mapping.var.elements = 1;

        // if it's an array or matrix, figure out the index
        if(vartype->matArrayStride)
        {
          // number of rows is the number of vectors in the matrix's total byte size (each vector is
          // a row)
          mapping.var.rows =
              uint8_t((varTypeByteSize + vartype->matArrayStride - 1) / vartype->matArrayStride);

          // unless this is a column major matrix, in which case each vector is a column so swap the
          // rows/columns (the number of ROWS is the vector size, when each vector is a column)
          if(vartype->colMajorMatrix)
            std::swap(mapping.var.rows, mapping.var.columns);

          // calculate which vector we're on, and which component
          uint32_t idx = varOffset / vartype->matArrayStride;
          varOffset -= vartype->matArrayStride * idx;
          uint32_t comp = (varOffset % 16) / 4;

          // should now be down to a vector, so the remaining offset is the component. Unless we had
          // multiple indices in which case it's a multi-dimensional array, or this is a subrange of
          // an array mapped to a single register.
          RDCASSERT(varOffset < 16 || defrange->regIndices > 1 || varLen < varTypeByteSize);

          if(vartype->leafType == LF_MATRIX)
          {
            // if this is a matrix, start with the index as row, and component as column
            uint32_t row = idx;
            uint32_t col = comp;

            // flip them if this is column major
            if(vartype->colMajorMatrix)
              std::swap(row, col);

            // add the row to the name since we want our mapping row-major for better display
            mapping.var.name += StringFormat::Fmt(".row%u", row);

            // and set the component after flipping
            comp = col;
          }
          else
          {
            // the number of rows is actually the number of elements (and number of rows is 1)
            mapping.var.elements = mapping.var.rows;
            mapping.var.rows = 1;

            // if this is an array, the index is just the array index. However if we're mapping the
            // whole array, don't add the index as the mapping will do that for us
            if(varLen < varTypeByteSize)
            {
              mapping.var.name += StringFormat::Fmt("[%u]", idx);

              // we've selected one element in the array, so it's not longer an array
              mapping.var.elements = 1;
            }
          }

          // set the offset explicitly to the component within the final vector we chose (whatever
          // it is)
          varOffset = comp * 4;
        }

        RDCASSERT(mapping.var.rows <= 4 && mapping.var.columns <= 4);

        mapping.varFirstComp = (varOffset % 16) / 4;

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

          rdcstr name;

          if(Names.find(checksum->nameIndex) != Names.end())
          {
            name = Names[checksum->nameIndex];
            if(name.empty())
              name = Names[checksum->nameIndex] = "unnamed_shader";
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
              if(!_stricmp(Files[i].filename.c_str(), name.c_str()))
              {
                fileIdx = (int32_t)i;
                break;
              }
            }

            if(fileIdx == -1)
            {
              // if file index is still -1, try again but with normalised names
              for(char &c : name)
                if(c == '\\')
                  c = '/';

              for(size_t i = 0; i < Files.size(); i++)
              {
                rdcstr normalised = Files[i].filename;
                for(char &c : normalised)
                  if(c == '\\')
                    c = '/';

                if(!_stricmp(normalised.c_str(), name.c_str()))
                {
                  fileIdx = (int32_t)i;
                  break;
                }
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
              int32_t fileIdx = -1;
              if(name == "unnamed_shader")
              {
                for(int32_t i = 0; i < Files.count(); i++)
                {
                  if(!_stricmp(Files[i].filename.c_str(), name.c_str()))
                  {
                    fileIdx = i;
                    break;
                  }
                }
              }
              if(fileIdx == -1)
              {
                Files.push_back({name, ""});
                fileIdx = (int32_t)Files.size() - 1;
              }

              FileMapping[chunkOffs] = fileIdx;
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

            LineColumnInfo &lineInfo = m_InstructionInfo[line.offset].lineInfo;
            lineInfo.fileIndex = fileIdx;
            lineInfo.lineStart = line.linenumStart;
            lineInfo.lineEnd = line.linenumStart + line.deltaLineEnd;

            if(hasColumns)
            {
              CV_Column_t &col = columns[l];
              lineInfo.colStart = col.offColumnStart;
              lineInfo.colEnd = col.offColumnEnd;
            }
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
            for(size_t in = 0; in < inlines.size(); in++)
            {
              if(inlinee->inlinee == inlines[in].id)
              {
                inlines[in].fileOffs = inlinee->fileId;
                inlines[in].baseLineNum = inlinee->sourceLineNum;
              }
            }
          }
        }
        else if(sourceLineType == CV_INLINEE_SOURCE_LINE_SIGNATURE_EX)
        {
          while(iter < subend)
          {
            CodeViewInfo::InlineeSourceLineEx *inlinee = (CodeViewInfo::InlineeSourceLineEx *)iter;

            iter += sizeof(CodeViewInfo::InlineeSourceLineEx) +
                    sizeof(CV_off32_t) * inlinee->countOfExtraFiles;

            for(size_t in = 0; in < inlines.size(); in++)
            {
              if(inlinee->inlinee == inlines[in].id)
              {
                inlines[in].fileOffs = inlinee->fileId;
                inlines[in].baseLineNum = inlinee->sourceLineNum;
              }
            }
          }
        }
      }
      else
      {
        break;
      }
    }
  }

  for(auto it = m_InstructionInfo.begin(); it != m_InstructionInfo.end(); ++it)
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

      auto it = m_InstructionInfo.lower_bound(loc.offsetStart);

      for(; it != m_InstructionInfo.end() && it->first <= loc.offsetEnd; ++it)
      {
        if((it->first >= loc.offsetStart && it->first < loc.offsetEnd) ||
           (it->first == loc.offsetStart && it->first == loc.offsetEnd))
        {
          LineColumnInfo &lineInfo = it->second.lineInfo;

          SPDBLOG("Patching %x between [%x,%x] from (%d %u:%u -> %u:%u) into (%d %u:%u -> %u:%u)",
                  it->first, loc.offsetStart, loc.offsetEnd, lineInfo.fileIndex, lineInfo.lineStart,
                  lineInfo.colStart, lineInfo.lineEnd, lineInfo.colEnd, fileIdx,
                  loc.lineStart + inlines[i].baseLineNum, loc.colStart,
                  loc.lineEnd + inlines[i].baseLineNum, loc.colEnd);

          lineInfo.fileIndex = fileIdx;
          lineInfo.lineStart = loc.lineStart + inlines[i].baseLineNum;
          lineInfo.lineEnd = loc.lineEnd + inlines[i].baseLineNum;
          lineInfo.colStart = loc.colStart;
          lineInfo.colEnd = loc.colEnd;
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
  rdcarray<rdcstr> filenames;
  filenames.reserve(Files.size());
  for(size_t i = 0; i < Files.size(); i++)
    filenames.push_back(Files[i].filename);

  // Sort files according to the order they come in the Names array, this seems to be more reliable
  // about placing the main file first.
  std::sort(Files.begin(), Files.end(),
            [&Names](const ShaderSourceFile &a, const ShaderSourceFile &b) {
              // any entries that aren't found in Names at all (like @cmdline that we add) will be
              // sorted to
              // the end.
              size_t aIdx = ~0U, bIdx = ~0U;

              size_t i = 0;
              for(auto it = Names.begin(); it != Names.end(); ++it)
              {
                if(it->second == a.filename)
                  aIdx = i;
                if(it->second == b.filename)
                  bIdx = i;

                i++;
              }

              // if neither were found, sort by filename
              if(aIdx == bIdx)
                return a.filename < b.filename;

              return aIdx < bIdx;
            });

  // create a map from filename -> index
  std::map<rdcstr, int32_t> remapping;
  for(size_t i = 0; i < Files.size(); i++)
    remapping[Files[i].filename] = (int32_t)i;

  // remap the line info by looking up the original intended filename, then looking up the new index
  for(auto it = m_InstructionInfo.begin(); it != m_InstructionInfo.end(); ++it)
  {
    if(it->second.lineInfo.fileIndex == -1)
      continue;
    it->second.lineInfo.fileIndex = remapping[filenames[it->second.lineInfo.fileIndex]];
  }

  std::sort(m_Locals.begin(), m_Locals.end());

  m_HasDebugInfo = true;
}

void SPDBChunk::GetLineInfo(size_t, uintptr_t offset, LineColumnInfo &lineInfo) const
{
  if(offset == ~0U && !m_InstructionInfo.empty())
  {
    lineInfo = m_InstructionInfo.begin()->second.lineInfo;
    return;
  }

  auto it = m_InstructionInfo.lower_bound((uint32_t)offset);

  if(it != m_InstructionInfo.end() && (uintptr_t)it->first <= offset)
    lineInfo = it->second.lineInfo;
}

void SPDBChunk::GetCallstack(size_t, uintptr_t offset, rdcarray<rdcstr> &callstack) const
{
  auto it = m_InstructionInfo.lower_bound((uint32_t)offset);

  if(it != m_InstructionInfo.end() && (uintptr_t)it->first <= offset)
    callstack = it->second.callstack;
}

bool SPDBChunk::HasSourceMapping() const
{
  return true;
}

void SPDBChunk::GetLocals(const DXBC::DXBCContainer *dxbc, size_t, uintptr_t offset,
                          rdcarray<SourceVariableMapping> &locals) const
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

    // set up the register range for this mapping
    DebugVariableReference range;

    // it's possible to declare coverage input but then not use it. We then get a local that
    // doesn't map to any register because the register declaration is stripped.
    // this doesn't happen on output because outputs don't get stripped in the same way.
    if(it->regType == DXBCBytecode::TYPE_INPUT_COVERAGE_MASK &&
       !dxbc->GetDXBCByteCode()->HasCoverageInput())
      continue;

    range.name = dxbc->GetDXBCByteCode()->GetRegisterName(it->regType, it->regIndex);
    range.component = it->regFirstComp;

    if(IsInput(it->regType))
      range.type = DebugVariableType::Input;
    else if(it->regType == DXBCBytecode::TYPE_CONSTANT_BUFFER)
      range.type = DebugVariableType::Constant;
    else
      range.type = DebugVariableType::Variable;

    // we don't handle more than float4 at a time (unless in an array)
    RDCASSERT(it->numComps <= 4);

    // we apply each matching local over the top. Where there is an overlap (e.g. two variables with
    // the same name) we take the last mapping as authoratitive. This is a good solution for the
    // case where one function with a parameter/variable name calls an inner function with the same
    // parameter name and there's shadowing. The later mapping will be for the inner function so we
    // use it in preference.

    // check if we already have a mapping for this variable
    for(SourceVariableMapping &a : locals)
    {
      const ShaderConstantType &b = it->var;

      if(a.name == b.name)
      {
        a.variables.resize(RDCMAX(a.variables.size(), (size_t)it->varFirstComp + it->numComps));
        for(uint32_t i = 0; i < it->numComps; i++)
        {
          a.variables[it->varFirstComp + i] = range;
          a.variables[it->varFirstComp + i].component += (uint8_t)i;
        }

        RDCASSERT(it->var.elements == 1);

        // we've processed this, no need to add a new entry
        added = true;
        break;
      }
    }

    if(!added)
    {
      SourceVariableMapping a;

      a.name = it->var.name;
      a.type = it->var.baseType;
      a.rows = it->var.rows;
      a.columns = it->var.columns;
      a.offset = it->varOffset;

      a.variables.resize(it->varFirstComp + it->numComps);

      for(uint32_t i = 0; i < it->numComps; i++)
      {
        a.variables[it->varFirstComp + i] = range;
        a.variables[it->varFirstComp + i].component += (uint8_t)i;
      }

      for(uint32_t e = 0; e < it->var.elements; e++)
      {
        if(it->var.elements > 1)
        {
          a.name = StringFormat::Fmt("%s[%u]", it->var.name.c_str(), e);
          for(uint32_t i = 0; i < it->numComps; i++)
          {
            a.variables[it->varFirstComp + i].name =
                StringFormat::Fmt("%s[%u]", range.name.c_str(), e);
          }
        }

        locals.push_back(a);

        a.offset += VarTypeByteSize(a.type) * RDCMAX(1U, a.columns) * RDCMAX(1U, a.rows);
      }
    }
  }
}

IDebugInfo *ProcessSPDBChunk(void *chunk)
{
  uint32_t *raw = (uint32_t *)chunk;

  if(raw[0] != FOURCC_SPDB)
    return NULL;

  uint32_t spdblength = raw[1];

  return new SPDBChunk((byte *)&raw[2], spdblength);
}

IDebugInfo *ProcessPDB(byte *data, uint32_t length)
{
  return new SPDBChunk(data, length);
}

void UnwrapEmbeddedPDBData(bytebuf &bytes)
{
  if(!IsPDBFile(bytes.data(), bytes.size()))
    return;

  FileHeaderPage *header = (FileHeaderPage *)bytes.data();

  uint32_t pageCount = header->PageCount;

  if(pageCount * header->PageSize != bytes.size())
  {
    RDCWARN("Corrupt header/pdb. %u pages of %u size doesn't match %zu file size", pageCount,
            header->PageSize, bytes.size());

    // some DXC versions write the wrong page count, just count ourselves from the file size.
    if((bytes.size() % header->PageSize) == 0)
    {
      header->PageCount = (uint32_t)bytes.size() / header->PageSize;
      RDCWARN("Correcting page count to %u by dividing file size %zu by page size %u.",
              header->PageCount, bytes.size(), header->PageSize);
    }
  }

  const byte **pages = new const byte *[header->PageCount];
  for(uint32_t i = 0; i < header->PageCount; i++)
    pages[i] = &bytes[i * header->PageSize];

  uint32_t rootdirCount = header->PagesForByteSize(header->RootDirSize);
  uint32_t rootDirIndicesCount = header->PagesForByteSize(rootdirCount * sizeof(uint32_t));

  PageMapping rootdirIndicesMapping(pages, header->PageSize, header->RootDirectory,
                                    rootDirIndicesCount);
  const byte *rootdirIndices = rootdirIndicesMapping.Data();

  PageMapping directoryMapping(pages, header->PageSize, (uint32_t *)rootdirIndices, rootdirCount);
  const uint32_t *dirContents = (const uint32_t *)directoryMapping.Data();

  rdcarray<PDBStream> streams;

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

  if(streams.size() > 5)
  {
    // stream 5 is expected to contain the embedded data if this is a turducken PDB
    PageMapping embeddedData(pages, header->PageSize, &streams[5].pageIndices[0],
                             (uint32_t)streams[5].pageIndices.size());

    if(streams[5].pageIndices.size() > 0)
    {
      const byte *data = embeddedData.Data();

      // if we have a DXBC file in this stream, then it's what we want
      if(!memcmp(data, &FOURCC_DXBC, 4))
        bytes.assign(data, streams[5].byteLength);
    }
  }

  delete[] pages;
}

};    // namespace DXBC
