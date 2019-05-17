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

#include <map>
#include <string>
#include <utility>
#include <vector>

#pragma once

#include "dxbc_disassemble.h"

namespace DXBC
{
////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Partial (and by that I mean very partial) spec of the SDBG debug information chunk in shader
// bytecode.
//
// Very much work in progress, feel free to contribute if you figure out what some of the fields are
// or have
// a correction.
//
// I've documented assumptions/guesses/suppositions where relevant. There are plenty of them.
//
// Current completely understood structures:
//  * SDBGHeader
//  * SDBGFileHeader
//  * SDBGSymbol
//  * SDBGType
//  * SDBGScope
//
// Structures that are understood but with unknown elements:
//  * SDBGAsmInstruction
//
// Structures that are partly understood, but their place/purpose is still vague:
//  * SDBGVariable
//  * SDBGInputRegister
////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct CountOffset
{
  int32_t count;
  int32_t offset;
};

// Completely understood
struct SDBGHeader
{
  int32_t
      version;    // Always 0x00000054 it seems. Probably a version number, might be some other ID

  int32_t compilerSigOffset;    // offset from asciiOffset at the end of this structure.
  int32_t entryFuncOffset;      // offset from asciiOffset at the end of this structure.
  int32_t profileOffset;        // offset from asciiOffset at the end of this structure.

  uint32_t shaderFlags;    // Shader flags - same as from reflection.

  // All offsets are after this header.
  CountOffset files;             // total unique files opened and used via #include
  CountOffset instructions;      // assembly instructions
  CountOffset variables;         // Looks to be the variables (one per component) used in the shader
  CountOffset inputRegisters;    // This lists which bits of which inputs are used - e.g. the
                                 // components in input
                                 // signature elements and cbuffers.
  CountOffset
      symbolTable;    // This is a symbol table definitely, also includes 'virtual' symbols to match
                      // up ASM instructions to lines.
  CountOffset scopes;    // These are scopes - like for structures/functions. Also Globals/Locals
                         // lists of variables
                         // in scope for reference in ASM instructions
  CountOffset types;     // Type specifications

  int32_t int32DBOffset;    // offset after this header. Same principle as ASCII db, but for int32s

  int32_t asciiDBOffset;    // offset after this header to the ASCII data. This is a general "ascii
                            // database section"
  // or similar because it has file sources, generated symbol names, etc. Hefty deduping goes
  // on here, so if the hlsl source is included then offsets for symbols etc in that source
  // point inside that source - only generated names like "structure::member" that don't exist
  // in the source are duplicated after. Same goes for hlsl include file names, they're always
  // obviously in the source somewhere.
};

// Completely understood
// one per included file (unique). First always exists and is the hlsl file passed to the compiler
struct SDBGFileHeader
{
  int32_t filenameOffset;    // offset into the ascii Database where the filename sits.
  int32_t filenameLen;       // filename path. Absolute for root file, relative for other headers

  int32_t sourceOffset;    // offset into the ascii Database where this file's source lives
  int32_t sourceLen;       // bytes in source file. Valid for all file headers
};

// Partly understood, many unknown/guessed elements. Completely understood how this fits in in the
// overall structure
// Details of each assembly instruction
struct SDBGAsmInstruction
{
  int32_t instructionNum;

  OpcodeType opCode;

  int32_t unknown_a[2];
  int32_t destRegister;
  int32_t unknown_b;

  int32_t destXMask;    // 00 if writing to this component in dest register, -1 if not writing
  int32_t destYMask;    // 01		"				"				"				"
  int32_t destZMask;    // 02		"				"				"				"
  int32_t destWMask;    // 03		"				"				"				"

  struct Component
  {
    int32_t varID;          // matches SDBGVariable below
    float lowBounds[2];     // what's this? defaults  0.0 to -QNAN. Some kind of bound.
    float highBounds[2];    // what's this?			 -0.0 to  QNAN. Some kind of bound.
    float minBound;         // min value this components's dest can be
    float maxBound;         // max value	"			"			"
    int32_t unknown_a[2];
  } component[4];

  int32_t unknown_c[9];

  // I don't know what this is, but it's 9 int32s and 4 of them, so sounds like
  // something that's per-component
  struct Something
  {
    int32_t unknown[9];
  } somethings[4];

  int32_t unknown_d[2];

  int32_t symbol;    // symbol, usually virtual I think, that links this instruction
                     // to somewhere in hlsl - e.g. a line number and such

  int32_t callstackDepth;    // 0-indexed current level of the callstack. ie. 0 is in the main
                             // function, 1 is in a sub-function, etc etc.

  CountOffset scopes;    // The scopeIDs that show the call trace in each instruction (or rather,
                         // where this instruction takes place).
  // it has several elements: N Locals entries, with different locals for different scopes or
  // branches
  // (this doesn't quite make sense yet. Some Locals lists can contain variables from if AND else
  // branches,
  // or include variables that have gone out of scope). Then it contains a single element pointing
  // to the current
  // function, then a globals list showing all variables and return-value functions in global scope
  // at this point.
  CountOffset varTypes;    // The Type IDs of variables involved in this instruction. Possibly in
                           // source,source,dest order but
                           // maybe not.
};

// Mostly understood, a couple of unknown elements and/or not sure how it fits together in the grand
// scheme
struct SDBGVariable
{
  int32_t symbolID;    // Symbol this assignment depends on
  VariableType type;
  int32_t unknown[2];
  int32_t typeID;    // refers to SDBGType. -1 if a constant
  union
  {
    int32_t component;    // x=0,y=1,z=2,w=3
    float value;          // const value
  };
};

// Mostly understood, a couple of unknown elements and/or not sure how it fits together in the grand
// scheme
struct SDBGInputRegister
{
  int32_t varID;
  int32_t type;    // 2 = from cbuffer, 0 = from input signature, 6 = from texture, 7 = from sampler
  int32_t cbuffer_register;      // -1 if input signature
  int32_t cbuffer_packoffset;    // index of input signature
  int32_t component;             // x=0,y=1,z=2,w=3
  int32_t initValue;             // I think this is a value? -1 or some value. Or maybe an index.
};

// Completely understood
struct SDBGSymbol
{
  int32_t fileID;    // index into SDBGFileHeader array
  int32_t lineNum;
  int32_t characterNum;    // not column, so after a tab would just be 1.
  CountOffset symbol;      // offset can be 0 for 'virtual' symbols
};

// Almost entirely understood, there is sometimes redundancy in that the same scope appears with
// different tree entries that overlap and are supersets. Seems like MAYBE each new instruction it
// shows all the variables in scope up to that point, but the scope tree is inconsistent e.g. in
// what
// ends up in Globals. Still useful for resolving types though
struct SDBGScope
{
  int32_t type;    // what kind of type I have no idea. 0 = Globals, 1 = Locals, 3 = Structure, 4 =
                   // Function
  int32_t symbolNameOffset;    // offset from start of ascii Database
  int32_t symbolNameLength;
  CountOffset scopeTree;
};

// Completely understood
struct SDBGType
{
  int32_t symbolID;
  int32_t isFunction;     // 0 / 1
  int32_t type;           // 0 == scalar, 1 == vector, 3 == matrix, 4 == texture/sampler
  int32_t typeNumRows;    // number of floats in the height of the base type (mostly for matrices)
  int32_t typeNumColumns;    // number of floats in the width of the base type. 0 for functions or
                             // structure types
  int32_t scopeID;    // if type is a complex type (including function return type), the scope of
                      // this type.
  int32_t arrayDimension;    // 0, 1, 2, ...
  int32_t arrayLenOffset;    // offset into the int32 database. Contains an array length for each
                             // dimension
  int32_t stridesOffset;     // offset into the int32 database. Contains the stride for that level,
                             // for each dimension.
  // so with array[a][b][c] it has b*c*baseSize, then c*baseSize then baseSize
  int32_t numFloats;    // number of floats in this type (or maybe 32bit words, not sure).
  int32_t varID;        // Variable ID, or -1 if this variable isn't used.
};

// SDBG chunk gets its own class since it's so complex. Deliberately fairly leaky too
// since the data + use is a bit unclear still
class SDBGChunk : public DXBCDebugChunk
{
public:
  SDBGChunk(void *data);

  std::string GetCompilerSig() const { return m_CompilerSig; }
  std::string GetEntryFunction() const { return m_Entry; }
  std::string GetShaderProfile() const { return m_Profile; }
  uint32_t GetShaderCompileFlags() const { return m_ShaderFlags; }
  void GetLineInfo(size_t instruction, uintptr_t offset, LineColumnInfo &lineInfo) const;

  bool HasLocals() const;
  void GetLocals(size_t instruction, uintptr_t offset, rdcarray<LocalVariableMapping> &locals) const;

private:
  SDBGChunk();
  SDBGChunk(const SDBGChunk &);
  SDBGChunk &operator=(const SDBGChunk &o);

  bool m_HasDebugInfo;

  std::string GetSymbolName(int symbolID);
  std::string GetSymbolName(int32_t symbolOffset, int32_t symbolLength);

  std::vector<SDBGAsmInstruction> m_Instructions;
  std::vector<SDBGVariable> m_Variables;
  std::vector<SDBGInputRegister> m_Inputs;
  std::vector<SDBGSymbol> m_SymbolTable;
  std::vector<SDBGScope> m_Scopes;
  std::vector<SDBGType> m_Types;
  std::vector<int32_t> m_Int32Database;

  uint32_t m_ShaderFlags;

  std::string m_CompilerSig;
  std::string m_Entry;
  std::string m_Profile;

  // these don't need to be exposed, a more processed and friendly
  // version is exposed
  SDBGHeader m_Header;
  std::vector<SDBGFileHeader> m_FileHeaders;

  std::vector<char> m_RawData;
};
};
