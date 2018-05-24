/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2018 Baldur Karlsson
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

using std::make_pair;

namespace DXBC
{
static const uint32_t FOURCC_SPDB = MAKE_FOURCC('S', 'P', 'D', 'B');

SPDBChunk::SPDBChunk(void *chunk)
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

  vector<PDBStream> streams;

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

  map<string, uint32_t> StreamNames;

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

      Files.push_back(make_pair(filename, (char *)fileContents.Data()));
    }
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

  map<uint32_t, string> Names;

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

  vector<DBIModule> modules;

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
      cur += sizeof(DBIModule) - sizeof(string) * 2;

      char *moduleName = (char *)cur;
      cur += strlen(moduleName) + 1;

      char *objectName = (char *)cur;
      cur += strlen(objectName) + 1;

      // align up to DWORD boundary
      while((uintptr_t)cur & 0x3)
        cur++;

      DBIModule m;
      memcpy(&m, mod, sizeof(DBIModule) - sizeof(string) * 2);
      m.moduleName = moduleName;
      m.objectName = objectName;

      SPDBLOG("Got module named %s from object %s", moduleName, objectName);

      modules.push_back(m);
    }
    RDCASSERT(cur == end);
  }

  std::vector<Inlinee> inlines;

  PROCSYM32 main = {};

  map<uint32_t, int32_t> FileMapping;    // mapping from hash chunk to index in Files[], or -1

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
            string cmdlineDefines = "// Command line defines:\n\n";

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
                  cmdlineDefines += string(defstart, defend) + " " + string(valstart, valend);
                  cmdlineDefines += "\n";
                }
                else
                {
                  cmdlineDefines += "#define ";
                  cmdlineDefines += string(defstart, defend);
                  cmdlineDefines += "\n";
                }
              }
            }

            Files.push_back(make_pair("@cmdline", cmdlineDefines));
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
            case CodeViewInfo::BA_OP_ChangeCodeLength: codeLength = parameter; break;
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
              // unclear where this should be reset
              codeLength = 0;
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
              // not sure if this is a bug in the HLSL compiler or what, but the sourceDelta seems
              // to come out double what it should be - so add an extra shift
              sourceDelta >>= 1;
              codeOffset += CodeDelta;
              currentLine += sourceDelta;
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

            // if we have a previous location with implicit length, fix it up now
            if(!inlinee.locations.empty() &&
               inlinee.locations.back().offsetEnd == inlinee.locations.back().offsetStart)
            {
              inlinee.locations.back().offsetEnd = loc.offsetStart;
            }

            inlinee.locations.push_back(loc);

            SPDBLOG("inline annotation of %s, from %x (length %x), from %u:%u to %u:%u",
                    statement ? "statement" : "expression", codeOffsetBase + codeOffset, codeLength,
                    currentLine, currentColStart, currentLine + currentLineLength, currentColEnd);
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

        const char *regtype = "";
        const char *regprefix = "?";
        switch((CV_HLSLREG_e)defrange->regType)
        {
          case CV_HLSLREG_TEMP:
            regtype = "temp";
            regprefix = "r";
            break;
          case CV_HLSLREG_INPUT:
            regtype = "input";
            regprefix = "v";
            break;
          case CV_HLSLREG_OUTPUT:
            regtype = "output";
            regprefix = "o";
            break;
          case CV_HLSLREG_INDEXABLE_TEMP:
            regtype = "indexable";
            regprefix = "x";
            break;
          default: break;
        }

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

        uint32_t regoffset = *CV_DEFRANGESYMHLSL_OFFSET_CONST_PTR(defrange);

        char regcomps[] = "xyzw";

        uint32_t regindex = regoffset / 16;
        uint32_t regfirstcomp = (regoffset % 16) / 4;
        uint32_t regnumcomps = defrange->sizeInParent / 4;

        char *regswizzle = regcomps;
        regswizzle += regfirstcomp;
        regswizzle[regnumcomps] = 0;

        SPDBLOG("Stored in %s%u.%s", regprefix, regindex, regswizzle);

        SPDBLOG("Valid from %x to %x", defrange->range.offStart,
                defrange->range.offStart + defrange->range.cbRange);

        const CV_LVAR_ADDR_GAP *gaps = CV_DEFRANGESYMHLSL_GAPS_CONST_PTR(defrange);
        size_t gapcount = CV_DEFRANGESYMHLSL_GAPS_COUNT(defrange);
        if(gapcount > 0)
          SPDBLOG("Except for in:");
        for(size_t i = 0; i < gapcount; i++)
        {
          SPDBLOG("  Gap %zu: %x -> %x", i, defrange->range.offStart + gaps[i].gapStartOffset,
                  defrange->range.offStart + gaps[i].gapStartOffset + gaps[i].cbRange);
        }
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
              Files.push_back(make_pair(name, ""));

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
            lineCol.statement = line.fStatement;

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
    it->second.stack.push_back(m_Functions[0].name);

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

      // don't apply expressions
      if(!loc.statement)
        continue;

      int nPatched = 0;

      for(auto it = m_Lines.begin(); it != m_Lines.end(); ++it)
      {
        if(it->first >= loc.offsetStart && it->first < loc.offsetEnd)
        {
          SPDBLOG("Patching %x between [%x,%x) from (%d %u:%u -> %u:%u) into (%d %u:%u -> %u:%u)",
                  it->first, loc.offsetStart, loc.offsetEnd, it->second.fileIndex,
                  it->second.lineStart, it->second.colStart, it->second.lineEnd, it->second.colEnd,
                  fileIdx, loc.lineStart + inlines[i].baseLineNum, loc.colStart,
                  loc.lineEnd + inlines[i].baseLineNum, loc.colEnd);

          it->second.fileIndex = fileIdx;
          it->second.funcIndex = inlines[i].id;
          it->second.lineStart = loc.lineStart + inlines[i].baseLineNum;
          it->second.lineEnd = loc.lineEnd + inlines[i].baseLineNum;
          it->second.colStart = loc.colStart;
          it->second.colEnd = loc.colEnd;
          it->second.stack.push_back(m_Functions[inlines[i].id].name);
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
  std::sort(Files.begin(), Files.end(), [&Names](const std::pair<std::string, std::string> &a,
                                                 const std::pair<std::string, std::string> &b) {
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

  m_HasDebugInfo = true;
}

void SPDBChunk::GetFileLine(size_t instruction, uintptr_t offset, int32_t &fileIdx,
                            int32_t &lineNum) const
{
  for(auto it = m_Lines.begin(); it != m_Lines.end(); ++it)
  {
    if((uintptr_t)it->first <= offset)
    {
      fileIdx = it->second.fileIndex;
      lineNum = it->second.lineStart - 1;    // 0-indexed
    }
    else
    {
      return;
    }
  }
}

};    // namespace DXBC