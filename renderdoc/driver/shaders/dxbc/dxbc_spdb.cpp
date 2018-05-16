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

using std::make_pair;

namespace DXBC
{
static const uint32_t FOURCC_SPDB = MAKE_FOURCC('S', 'P', 'D', 'B');

SPDBChunk::SPDBChunk(void *chunk)
{
  m_HasDebugInfo = false;

  uint32_t firstInstructionOffset = 0;

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

  for(size_t i = 0; i < streams.size(); i++)
  {
    streams[i].byteLength = *dirContents;
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

      if(filename[0] == 0)
        filename = "shader";

      Files.push_back(make_pair(filename, (char *)fileContents.Data()));
    }
  }

  vector<Function> functions;

  if(streams.size() >= 5)
  {
    PDBStream &s = streams[4];
    PageMapping fileContents(pages, header->PageSize, &s.pageIndices[0],
                             (uint32_t)s.pageIndices.size());

    byte *bytes = (byte *)fileContents.Data();
    byte *end = bytes + s.byteLength;

    // seems to be accurate, but we'll just iterate to end
    // uint32_t *u32 = (uint32_t *)bytes;
    // uint32_t numFuncs = u32[6];

    // skip header
    bytes += 57;

    while(bytes < end)
    {
      Function f;
      memcpy(&f, bytes, 11);
      bytes += 11;
      f.funcName = (const char *)bytes;
      bytes += 1 + f.funcName.length();

      while(*bytes)
      {
        f.things.push_back(*(int8_t *)bytes);
        bytes++;
      }

      functions.push_back(f);
    }
  }

  {
    Function mainFunc;
    mainFunc.funcName = "entrypoint";

    functions.push_back(mainFunc);
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
        Names[idx] = Strings + idx;
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

      modules.push_back(m);
    }
    RDCASSERT(cur == end);
  }

  vector<FuncCallLineNumbers> funcCalls;

  map<uint32_t, int32_t> FileMapping;    // mapping from hash chunk to index in Files[], or -1

  for(size_t m = 0; m < modules.size(); m++)
  {
    if(modules[m].stream == -1)
      continue;

    PDBStream &s = streams[modules[m].stream];
    PageMapping modMapping(pages, header->PageSize, &s.pageIndices[0],
                           (uint32_t)s.pageIndices.size());
    uint32_t *moduledata = (uint32_t *)modMapping.Data();

    RDCASSERT(moduledata[0] == 4);

    byte *cur = (byte *)&moduledata[1];
    byte *end = (byte *)moduledata + modules[m].cbSyms;
    while(cur < end)
    {
      uint16_t *sym = (uint16_t *)cur;

      uint16_t len = sym[0];
      uint16_t type = sym[1];
      len -= sizeof(uint16_t);    // len includes type uint16, subtract for ease of use

      cur += sizeof(uint16_t) * 2;

      byte *contents = cur;

      if(type == 0x1110)
      {
        ProcHeader *proc = (ProcHeader *)contents;
        // char *name = (char *)(proc + 1);

        firstInstructionOffset = proc->Offset;

        // RDCDEBUG("Got global procedure start %s %x -> %x", name, proc->Offset,
        // proc->Offset+proc->Length);
      }
      else if(type == 0x113c)
      {
        CompilandDetails *details = (CompilandDetails *)contents;
        char *compilerString = (char *)&details->CompilerSig;

        memcpy(&m_CompilandDetails, details, sizeof(CompilandDetails) - sizeof(string));
        m_CompilandDetails.CompilerSig = compilerString;

        /*
        RDCDEBUG("CompilandDetails: %s (%d.%d.%d.%d)", compilerString,
            details->FrontendVersion.Major, details->FrontendVersion.Minor,
            details->FrontendVersion.Build, details->FrontendVersion.QFE);*/

        // for hlsl/fxc
        // RDCASSERT(details->Language == 16 && details->Platform == 256);
      }
      else if(type == 0x113d)
      {
        // for hlsl/fxc?
        // RDCASSERT(contents[0] == 0x1);
        char *key = (char *)contents + 1;
        while(key[0])
        {
          char *value = key + strlen(key) + 1;

          // RDCDEBUG("CompilandEnv: %s = \"%s\"", key, value);

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
                char *defend = strchr(c, '=');

                if(defend == 0)
                  break;

                c = defend + 1;

                char *valstart = c;

                // skip to end or next whitespace
                while(*c && *c != ' ' && *c != '\t' && *c != '\n')
                  c++;

                char *valend = c;

                cmdlineDefines += "#define ";
                cmdlineDefines += string(defstart, defend) + " " + string(valstart, valend);
                cmdlineDefines += "\n";
              }
            }

            Files.push_back(make_pair("@cmdline", cmdlineDefines));
          }

          key = value + strlen(value) + 1;
        }
      }
      else if(type == 0x114D)
      {
        // RDCDEBUG("0x%04x, %d bytes", uint32_t(type), uint32_t(len));

        FuncCallLineNumbers func;
        func.fileOffs = 0;
        func.baseLineNum = 0;

        byte *iterator = (byte *)contents;
        byte *callend = contents + len;

        // uint32_t *adsf = (uint32_t *)iterator;

        // RDCDEBUG("funcdef for %s (%x) flags??=0x%x offset/length??=0x%x",
        // functions[adsf[2]&0xfff].funcName.c_str(), adsf[2], adsf[0], adsf[1]);
        iterator += 3 * sizeof(uint32_t);

        bool working = true;
        uint32_t currentBytes = firstInstructionOffset;
        uint32_t currentLine = 0;
        uint32_t currentColStart = 1;
        uint32_t currentColEnd = 100000;

        while(iterator < callend)
        {
          FuncCallBytestreamOpcodes opcode = (FuncCallBytestreamOpcodes)*iterator;
          iterator++;

          if(opcode == PrologueEnd || opcode == EpilogueBegin)
          {
            // uint32_t value = ReadVarLenUInt(iterator);
            // RDCDEBUG("type %02x: unk=%02x", opcode, value);

            if(opcode == EpilogueBegin)
            {
              // RDCDEBUG("                          (endloc: %04x - %u [%u,%u] )", currentBytes,
              // currentLine, currentColStart, currentColEnd);
              working = false;
            }
          }
          else if(opcode == FunctionEndNoAdvance)
          {
            uint32_t value = CodeViewInfo::CVUncompressData(iterator);

            // RDCDEBUG("                      type %02x: %02x: adjust line by 4(?!) & bytes by %x",
            // opcode, value, value);

            if(working)
            {
              InstructionLocation loc;
              loc.offset = currentBytes;
              loc.line = currentLine;
              loc.colStart = currentColStart;
              loc.colEnd = currentColEnd;
              func.locations.push_back(loc);

              loc.offset = currentBytes + value;
              loc.funcEnd = true;
              func.locations.push_back(loc);

              // RDCDEBUG("                          (loc: %04x - %u [%u,%u] )", currentBytes,
              // currentLine, currentColStart, currentColEnd);
            }

            currentBytes += value;
          }
          else if(opcode == AdvanceBytesAndLines)
          {
            uint32_t value = (uint32_t)*iterator;
            iterator++;

            uint32_t byteMod = (value & 0xf);
            uint32_t lineMod = (value >> 4);

            currentBytes += byteMod;
            currentLine += lineMod / 2;

            // RDCDEBUG("                      type %02x: %02x: adjust line by %u & bytes by %x",
            // type, value, lineMod/2, byteMod);
          }
          else if(opcode == EndOfFunction)
          {
            // RDCDEBUG("type %02x:", opcode);

            uint32_t retlen = CodeViewInfo::CVUncompressData(iterator);
            uint32_t byteAdv = CodeViewInfo::CVUncompressData(iterator);

            // RDCDEBUG("           retlen=%x, byteAdv=%x", retlen, byteAdv);

            currentBytes += byteAdv;

            if(working)
            {
              InstructionLocation loc;
              loc.offset = currentBytes;
              loc.line = currentLine;
              loc.colStart = currentColStart;
              loc.colEnd = currentColEnd;
              func.locations.push_back(loc);

              loc.offset = currentBytes + retlen;
              loc.funcEnd = true;
              func.locations.push_back(loc);
            }

            currentBytes += retlen;
          }
          else if(opcode == SetByteOffset)
          {
            currentBytes = CodeViewInfo::CVUncompressData(iterator);
            // RDCDEBUG("                      type %02x: start at byte offset %x", opcode,
            // currentBytes);
          }
          else if(opcode == AdvanceBytes)
          {
            uint32_t offs = CodeViewInfo::CVUncompressData(iterator);

            currentBytes += offs;

            // RDCDEBUG("                      type %02x: advance %x bytes", opcode, offs);

            if(working)
            {
              InstructionLocation loc;
              loc.offset = currentBytes;
              loc.line = currentLine;
              loc.colStart = currentColStart;
              loc.colEnd = currentColEnd;
              func.locations.push_back(loc);

              // RDCDEBUG("                          (loc: %04x - %u [%u,%u] )", currentBytes,
              // currentLine, currentColStart, currentColEnd);
            }
          }
          else if(opcode == AdvanceLines)
          {
            uint32_t linesAdv = CodeViewInfo::CVUncompressData(iterator);

            if(linesAdv & 0x1)
              currentLine -= (linesAdv / 2);
            else
              currentLine += (linesAdv / 2);

            // RDCDEBUG("                      type %02x: advance %u (%u) lines", opcode,
            // linesAdv/2, linesAdv);
          }
          else if(opcode == ColumnStart)
          {
            currentColStart = CodeViewInfo::CVUncompressData(iterator);
            // RDCDEBUG("                      type %02x: col < %u", opcode, currentColStart);
          }
          else if(opcode == ColumnEnd)
          {
            currentColEnd = CodeViewInfo::CVUncompressData(iterator);
            // RDCDEBUG("                      type %02x: col > %u", opcode, currentColEnd);
          }
          else if(opcode == EndStream)
          {
            while(*iterator == 0 && iterator < callend)
              iterator++;
            // RDCASSERT(iterator == callend); // seems like this isn't always true
          }
          else
          {
            RDCDEBUG("Unrecognised: %02x", opcode);
            break;
          }
        }

        if(func.locations.size() == 1)
        {
          // not sure what this means, but it seems to just be intended to match
          // one instruction offset and we don't have an 0xc to 'cap' things off.
          // just insert a dummy location at the same file line but with a slightly
          // higher offset so we have a valid range to match against
          auto loc = func.locations[0];
          loc.offset++;
          func.locations.push_back(loc);
        }

        RDCASSERT(func.locations.size() > 1);

        funcCalls.push_back(func);

        // RDCDEBUG("Lost %d bytes after we stopped processing", callend - iterator);
      }
      else if(type == 0x113E)
      {
        // not currently used
        /*
          //RDCDEBUG("0x%04x, %d bytes", uint32_t(type), uint32_t(len));

          RegisterVariableAssign *var = (RegisterVariableAssign *)contents;

          string funcName = "undefined";

          if((size_t)(var->func&0xfff) < functions.size())
            funcName = functions[var->func&0xfff].funcName;

          //RDCDEBUG("     in %s (%x) flags??=%04x, %s:", funcName.c_str(), var->func,
          var->unkflags, var->name);

          byte *afterName = (byte *)var->name + (strlen(var->name) + 1);

          byte *end = contents + len;

          // seems to always be 0s
          while(afterName < end)
          {
            RDCASSERT(*afterName == 0);
            afterName++;
          }
          */
      }
      else if(type == 0x1150)
      {
        // not currently used
        /*
          RDCASSERT(len %4 == 0);
          //RDCDEBUG("0x%04x, %d bytes", uint32_t(type), uint32_t(len));

          RegisterVariableAssignComponent *comp = (RegisterVariableAssignComponent *)contents;

          OperandType t = comp->Type();
          const char *type = "";
          switch(t)
          {
            case TYPE_TEMP: type = "r"; break;
            case TYPE_INPUT: type = "v"; break;
            case TYPE_OUTPUT: type = "o"; break;
            case TYPE_INDEXABLE_TEMP: type = "x"; break;
            case TYPE_INPUT_THREAD_ID: type = "globalIdx"; break;
            case TYPE_INPUT_THREAD_ID_IN_GROUP: type = "localIdx"; break;
            case TYPE_INPUT_THREAD_GROUP_ID: type = "groupIdx"; break;
            default: break;
          }

          uint16_t destComp = comp->destComp;
          if(len == 24)
            destComp = comp->unkE;
          if(len > 24)
          {
            uint16_t *end = (uint16_t *)(contents + len);

            destComp = end[-1];
          }

          char comps[] = "xyzw";

          //RDCDEBUG("%s%d.%c (%x, %x) <- <above>.%c @ 0x%x", type, (destComp)>>4,
          comps[(destComp&0xf)>>2], comp->destComp, comp->unkE, comps[(comp->srcComp&0xf)>>2],
          comp->instrOffset);

          //RDCDEBUG("     A:%04x B:%04x C:%04x D:%04x", comp->unkA, comp->unkB, comp->unkC,
          comp->unkD);
          //RDCDEBUG("     E(d):%04x", comp->unkE);

          uint32_t *extra = (uint32_t *)(comp+1);

          for(uint16_t l=20; l < len; l+=4)
          {
            //RDCDEBUG("     %08x", extra[0]);
            extra++;
          }
          */
      }
      else if(type == 0x114E)
      {
        RDCASSERT(len == 0);
        // RDCDEBUG("0x%04x, %d bytes", uint32_t(type), uint32_t(len));
      }
      else if(type == 0x0006)
      {
        // RDCDEBUG("end");
      }
      else
      {
        // RDCDEBUG("(unexpected?) 0x%04x", uint32_t(type));
      }

      cur += len;
    }
    RDCASSERT(cur == end);

    end = cur + modules[m].cbLines;

    while(cur < end)
    {
      uint16_t *type = (uint16_t *)cur;

      if(*type == 0xF4)    // hash
      {
        uint32_t *len = (uint32_t *)(type + 2);

        cur = (byte *)(len + 1);

        byte *start = cur;
        while(cur < start + *len)
        {
          uint32_t *hashData = (uint32_t *)cur;
          cur += sizeof(uint32_t);
          uint16_t *unknown = (uint16_t *)cur;
          cur += sizeof(uint16_t);

          uint32_t chunkOffs = uint32_t((byte *)hashData - start);

          uint32_t nameoffset = hashData[0];

          // if this is 0, we don't have a hash
          if(*unknown)
          {
            byte hash[16];
            memcpy(hash, cur, sizeof(hash));
            cur += sizeof(hash);

            int32_t fileIdx = -1;

            for(size_t i = 0; i < Files.size(); i++)
            {
              if(!_stricmp(Files[i].first.c_str(), Names[nameoffset].c_str()))
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
            if(Names.find(nameoffset) != Names.end())
            {
              string name = Names[nameoffset];
              Files.push_back(make_pair(name, ""));

              FileMapping[chunkOffs] = (int32_t)Files.size() - 1;
            }
            else
            {
              RDCERR(
                  "Processing SPDB chunk, encountered nameoffset %d that doesn't correspond to any "
                  "name.",
                  nameoffset);

              FileMapping[chunkOffs] = -1;
            }
          }

          unknown = (uint16_t *)cur;
          cur += sizeof(uint16_t);
          // another unknown
        }
        RDCASSERT(cur == start + *len);
      }
      else if(*type == 0xF2)
      {
        uint32_t *len = (uint32_t *)(type + 2);

        cur = (byte *)(len + 1);

        byte *start = cur;

        LineNumbersHeader *hdr = (LineNumbersHeader *)cur;

        cur = (byte *)(hdr + 1);

        bool hasExtra = (hdr->flags & 0x1);

        while(cur < start + *len)
        {
          FileLineNumbers *file = (FileLineNumbers *)cur;
          cur = (byte *)(file + 1);

          uint32_t *linedata = (uint32_t *)cur;

          cur += (sizeof(uint32_t) + sizeof(uint32_t)) * file->numLines;
          if(hasExtra)
            cur += (sizeof(uint16_t) + sizeof(uint16_t)) * file->numLines;

          int32_t fileIdx = -1;

          if(FileMapping.find(file->fileIdx) == FileMapping.end())
          {
            RDCERR(
                "SPDB chunk - line numbers file references index %u not encountered in file "
                "mapping",
                file->fileIdx);
          }
          else
          {
            fileIdx = FileMapping[file->fileIdx];
          }

          for(uint32_t l = 0; l < file->numLines; l++)
          {
            uint32_t offs = linedata[0];
            uint32_t lineNum = linedata[1] & 0x00fffff;
            // uint32_t unknown = linedata[1]>>24;

            linedata += 2;

            m_LineNumbers[offs] = make_pair(fileIdx, lineNum);

            // RDCDEBUG("Offset %x is line %d", offs, lineNum);
          }

          uint16_t *extraData = (uint16_t *)linedata;

          for(uint32_t l = 0; l < file->numLines; l++)
          {
            // uint16_t unkA = extraData[0];
            // uint16_t unkB = extraData[1];

            extraData += 2;
          }

          RDCASSERT((byte *)extraData == cur);
        }
        RDCASSERT(cur == start + *len);
      }
      else if(*type == 0xF6)
      {
        uint32_t *len = (uint32_t *)(type + 2);

        cur = (byte *)(len + 1);

        uint32_t *calls = (uint32_t *)cur;
        uint32_t *callsend = (uint32_t *)(cur + *len);

        // 0 seems to indicate no files, 1 indicates files but we don't need
        // to care as we can just handle this below.
        // RDCDEBUG("start: %x", calls[0]);
        calls++;

        int idx = 0;
        while(calls < callsend)
        {
          // some kind of control bytes? they have n file mappings following but I'm not sure what
          // they mean
          if(calls[0] <= 0xfff)
          {
            calls += 1 + calls[0];
          }
          else
          {
            // function call - 3 uint32s: (function idx | 0x1000, FileMapping idx, line # of start
            // of function)

            // RDCDEBUG("Call to %s(%x) - file %x, line %d",
            // functions[calls[0]&0xfff].funcName.c_str(), calls[0], calls[1], calls[2]);

            funcCalls[idx].fileOffs = calls[1];
            funcCalls[idx].baseLineNum = calls[2];

            idx++;

            calls += 3;
          }
        }

        cur += *len;
      }
      else
      {
        break;
      }
    }
  }

  for(size_t i = 0; i < funcCalls.size(); i++)
  {
    RDCASSERT(funcCalls[i].locations.size() > 1);

    if(funcCalls[i].locations.empty() || funcCalls[i].locations.size() == 1)
    {
      RDCWARN("Skipping patching function call with %d locations", funcCalls[i].locations.size());
      continue;
    }

    RDCASSERT(FileMapping.find(funcCalls[i].fileOffs) != FileMapping.end());

    if(FileMapping.find(funcCalls[i].fileOffs) == FileMapping.end())
    {
      RDCWARN("Got function call patch with fileoffs %x - skipping", funcCalls[i].fileOffs);
      continue;
    }

    // RDCDEBUG("Function call %d", i);

    for(size_t j = 0; j < funcCalls[i].locations.size() - 1; j++)
    {
      auto &loc = funcCalls[i].locations[j];
      auto &locNext = funcCalls[i].locations[j + 1];

      // don't apply between function end and next section (if there is one)
      if(loc.funcEnd)
        continue;

      int nPatched = 0;

      for(auto it = m_LineNumbers.begin(); it != m_LineNumbers.end(); ++it)
      {
        if(it->first >= loc.offset && it->first < locNext.offset)
        {
          int32_t fileIdx = FileMapping[funcCalls[i].fileOffs];

          /*
          RDCDEBUG("Patching offset %x between [%x,%x] from (%d,%u) to (%d,%u [%u->%u])",
            it->first, loc.offset, locNext.offset,
            it->second.first, it->second.second,
            fileIdx, loc.line+funcCalls[i].baseLineNum,
            loc.colStart, loc.colEnd);
            */

          it->second.first = fileIdx;
          it->second.second = loc.line + funcCalls[i].baseLineNum;
          nPatched++;
        }
      }

      /*
      if(nPatched == 0)
        RDCDEBUG("Can't find anything between offsets %x,%x as desired", loc.offset,
      locNext.offset);*/
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
  for(auto it = m_LineNumbers.begin(); it != m_LineNumbers.end(); ++it)
  {
    if(it->second.fileIndex == -1)
      continue;
    it->second.first = remapping[filenames[it->second.first]];
  }

  m_HasDebugInfo = true;
}

void SPDBChunk::GetFileLine(size_t instruction, uintptr_t offset, int32_t &fileIdx,
                            int32_t &lineNum) const
{
  for(auto it = m_LineNumbers.begin(); it != m_LineNumbers.end(); ++it)
  {
    if((uintptr_t)it->first <= offset)
    {
      fileIdx = it->second.first;
      lineNum = it->second.second - 1;    // 0-indexed
    }
    else
    {
      return;
    }
  }
}

};    // namespace DXBC