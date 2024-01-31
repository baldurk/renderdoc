/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2021-2024 Baldur Karlsson
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

#include "dxbc_bytecode.h"
#include "core/settings.h"
#include "dxbc_bytecode_ops.h"

#include "driver/ihv/nv/nvapi_wrapper.h"

RDOC_EXTERN_CONFIG(bool, DXBC_Disassembly_FriendlyNaming);

namespace DXBCBytecode
{
namespace AMDInstruction
{
// ha ha these are different :(
enum class DX11Op
{
  Readfirstlane = 0x01,
  Readlane = 0x02,
  LaneId = 0x03,
  Swizzle = 0x04,
  Ballot = 0x05,
  MBCnt = 0x06,
  Min3U = 0x08,
  Min3F = 0x09,
  Med3U = 0x0a,
  Med3F = 0x0b,
  Max3U = 0x0c,
  Max3F = 0x0d,
  BaryCoord = 0x0e,
  VtxParam = 0x0f,
  ViewportIndex = 0x10,
  RtArraySlice = 0x11,
  WaveReduce = 0x12,
  WaveScan = 0x13,
  DrawIndex = 0x17,
  AtomicU64 = 0x18,
  GetWaveSize = 0x19,
  BaseInstance = 0x1a,
  BaseVertex = 0x1b,
};

enum class DX12Op
{
  Readfirstlane = 0x01,
  Readlane = 0x02,
  LaneId = 0x03,
  Swizzle = 0x04,
  Ballot = 0x05,
  MBCnt = 0x06,
  Min3U = 0x07,
  Min3F = 0x08,
  Med3U = 0x09,
  Med3F = 0x0a,
  Max3U = 0x0b,
  Max3F = 0x0c,
  BaryCoord = 0x0d,
  VtxParam = 0x0e,
  ViewportIndex = 0x10,    // DX11 only
  RtArraySlice = 0x11,     // DX11 only
  WaveReduce = 0x12,
  WaveScan = 0x13,
  LoadDwAtAddr = 0x14,
  DrawIndex = 0x17,
  AtomicU64 = 0x18,
  GetWaveSize = 0x19,
  BaseInstance = 0x1a,
  BaseVertex = 0x1b,
};

DX12Op convert(DX11Op op)
{
  switch(op)
  {
    // convert opcodes that don't match up
    case DX11Op::Min3U: return DX12Op::Min3U;
    case DX11Op::Min3F: return DX12Op::Min3F;
    case DX11Op::Med3U: return DX12Op::Med3U;
    case DX11Op::Med3F: return DX12Op::Med3F;
    case DX11Op::Max3U: return DX12Op::Max3U;
    case DX11Op::Max3F: return DX12Op::Max3F;
    case DX11Op::BaryCoord: return DX12Op::BaryCoord;
    case DX11Op::VtxParam: return DX12Op::VtxParam;
    // others match up exactly
    default: return DX12Op(op);
  }
}

enum BaryInterpMode
{
  LinearCenter = 1,
  LinearCentroid = 2,
  LinearSample = 3,
  PerspCenter = 4,
  PerspCentroid = 5,
  PerspSample = 6,
  PerspPullModel = 7,
};

enum SwizzleMask
{
  SwapX1 = 0x041f,
  SwapX2 = 0x081f,
  SwapX4 = 0x101f,
  SwapX8 = 0x201f,
  SwapX16 = 0x401f,
  ReverseX4 = 0x0c1f,
  ReverseX8 = 0x1c1f,
  ReverseX16 = 0x3c1f,
  ReverseX32 = 0x7c1f,
  BCastX2 = 0x003e,
  BCastX4 = 0x003c,
  BCastX8 = 0x0038,
  BCastX16 = 0x0030,
  BCastX32 = 0x0020,
};

enum AMDAtomic
{
  Min = 0x01,
  Max = 0x02,
  And = 0x03,
  Or = 0x04,
  Xor = 0x05,
  Add = 0x06,
  Xchg = 0x07,
  CmpXchg = 0x08,
};

VendorAtomicOp convert(AMDAtomic op)
{
  switch(op)
  {
    case Min: return ATOMIC_OP_MIN;
    case Max: return ATOMIC_OP_MAX;
    case And: return ATOMIC_OP_AND;
    case Or: return ATOMIC_OP_OR;
    case Xor: return ATOMIC_OP_XOR;
    case Add: return ATOMIC_OP_ADD;
    case Xchg: return ATOMIC_OP_SWAP;
    case CmpXchg: return ATOMIC_OP_CAS;
    default: return ATOMIC_OP_NONE;
  }
}

static MaskedElement<uint32_t, 0xF0000000> Magic;
static MaskedElement<uint32_t, 0x03000000> Phase;
static MaskedElement<uint32_t, 0x00FFFF00> Data;
static MaskedElement<BaryInterpMode, 0x00FFFF00> BaryInterp;
static MaskedElement<SwizzleMask, 0x00FFFF00> SwizzleOp;
static MaskedElement<DX11Op, 0x000000FF> Opcode11;
static MaskedElement<DX12Op, 0x000000FF> Opcode12;

static MaskedElement<uint8_t, 0x00018000> VtxParamComponent;
static MaskedElement<uint32_t, 0x00001F00> VtxParamParameter;
static MaskedElement<uint32_t, 0x00006000> VtxParamVertex;

static MaskedElement<uint8_t, 0x0000FF00> WaveOp;
static MaskedElement<uint32_t, 0x00FF0000> WaveOpFlags;

static MaskedElement<AMDAtomic, 0x0000FF00> AtomicOp;
};

void Program::PostprocessVendorExtensions()
{
  const bool friendly = DXBC_Disassembly_FriendlyNaming();

  uint32_t magicID = ~0U;

  for(size_t i = 0; i < m_Declarations.size(); i++)
  {
    const Declaration &decl = m_Declarations[i];
    if((decl.operand.indices.size() == 1 && decl.operand.indices[0].index == m_ShaderExt.second) ||
       (decl.operand.indices.size() == 3 && decl.operand.indices[1].index == m_ShaderExt.second &&
        decl.space == m_ShaderExt.first))
    {
      magicID = (uint32_t)decl.operand.indices[0].index;
      m_Declarations.erase(i);
      break;
    }
  }

  // now we know the UAV, iterate the instructions looking for patterns to replace.
  //
  // AMD is nice and easy. Every instruction works on a scalar (vector versions repeat for each
  // component) and is encoded into a single InterlockedCompareExchange on the UAV.
  // So we can simply replace them in-place by decoding.
  //
  // NV's are not as nice. They are demarcated by IncrementCounter on the UAV so we know we'll see
  // a linear stream without re-ordering, but they *can* be intermixed with other non-intrinsic
  // instructions. Parameters and data are set by writing to specific offsets within the structure
  //
  // There are two types:
  //
  // Simpler, instructions that work purely on vars and not on resources. Shuffle/ballot/etc
  //
  // These come in the form:
  // index = magicUAV.IncrementCounter()
  // set params and opcode by writing to magicUAV[index].member...
  // retval = magicUAV.IncrementCounter()
  // [optional (see below): retval2 = magicUAV.IncrementCounter()]
  //
  // This type of operand returns the result with the closing IncrementCounter(). There could be
  // multiple results, so numOutputs is set before any, and then that many IncrementCounter() are
  // emitted with each result.
  //
  // More complex, instructions that use UAVs. Mostly atomics
  //
  // index1 = magicUAV.IncrementCounter()
  // magicUAV[index1].markUAV = 1;
  // userUAV[index1] = 0; // or some variation of such
  // index2 = magicUAV.IncrementCounter()
  // set params and opcode as above in magicUAV[index2].member...
  // retval = magicUAV[index2].dst
  //
  // Also note that if the shader doesn't use the return result of an atomic, the dst may never be
  // read!
  //
  // The difficulty then is distinguishing between the two and knowing where the boundaries are.
  // We do this with a simple state machine tracking where we are in an opcode:
  //
  //         +----------> Nothing
  //         |              v
  //         |              |
  //         |      IncrementCounter()
  // Emit instruction       |
  //         |              v
  //         |         Instruction >--write markUAV---> UAV instruction header
  //         |              v                           (wait for other UAV write)
  //         |              |                             v
  //         |              |                             |
  //         |         write opcode    ]                  |
  //         |              |          ]                  |
  //         |              v          ] simple           |
  //         |        Instruction Body ] case             |
  //         |              v          ]                  |
  //         |              |          ]                  |
  //         |      IncrementCounter() ]                  |
  //         |              |                             |
  //         +----<---------+                             |
  //         |                                    IncrementCounter()
  //         |                                            |
  //         |    UAV instruction body <------------------+
  //         |              v
  //         |              |
  //         |         write opcode
  //         |              |
  //         +--------------+
  //
  // so most state transitions are marked by an IncrementCounter(). The exceptions being
  // Instruction where we wait for a write to either markUAV or opcode to move to either simple
  // instruction body or to the UAV instruction header, and UAV instruction body which leaves
  // when we see an opcode write.
  //
  // We assume that markUAV will be written BEFORE the fake UAV write. It's not entirely clear if
  // this is guaranteed to not be re-ordered but it seems to be true and it's implied that NV's
  // driver relies on this. This simplifies tracking since we can use it as a state transition.
  //
  // We also assume that multiple accesses to the UAV don't overlap. This should be guaranteed by
  // the use of the index from the counter being used for access. However we don't actually check
  // the index itself.
  //
  // all src/dst are uint4, others are all uint

  enum class InstructionState
  {
    // if something goes wrong we enter this state and stop patching
    Broken,

    Nothing,

    // this is a state only used for AMD's UAV atomic op, which takes more parameters and uses the
    // operation phases.
    AMDUAVAtomic,

    // this is the state when we're not sure what type we are. Either markUAV is written, in which
    // case we move to UAVInstructionHeader1, or opcode is written, in which case we move to
    // Instruction1Out. We should see one or the other.
    //
    // FP16 UAV instructions (NV_EXTN_OP_FP16_ATOMIC) that operate on float4 resources have two
    // return values. Unfortunately we can't reliably detect this from the bytecode, so what
    // happens is that when we see opode get written if it's NV_EXTN_OP_FP16_ATOMIC then we jump
    // straight to UAVInstructionBody and re-use the UAV instruction header from last time. We
    // know this MUST be a continuation because otherwise NV_EXTN_OP_FP16_ATOMIC is always
    // preceeded by a UAV instruction header (via markUAV).
    InstructionHeader,
    InstructionBody,
    // we move from Instruction1Out to this state when markUAV is written. The next UAV write is
    // used to determine the 'target' UAV.
    // We then move to header2 so we don't consume any other UAV writes.
    UAVInstructionHeader1,
    // here we do nothing but sit and wait for the IncrementCounter() so we can move to the UAV
    // body state
    UAVInstructionHeader2,
    // in this state we aren't sure exactly when to leave it. We wait *at least* until opcode is
    // written, but there may be more instructions after that to read from dst :(
    UAVInstructionBody,
  };

  enum class NvUAVParam
  {
    opcode = 0,
    src0 = 76,
    src1 = 92,
    src2 = 108,
    src3 = 28,
    src4 = 44,
    src5 = 60,
    dst = 124,
    markUAV = 140,
    numOutputs = 144,
  };

  InstructionState state = InstructionState::Nothing;

  NvShaderOpcode nvopcode = NvShaderOpcode::Unknown;
  Operand srcParam[8];
  Operand dstParam[4];
  Operand uavParam;
  int numOutputs = 0, outputsNeeded = 0;

  ToString flags = friendly ? ToString::FriendlyNameRegisters : ToString::None;

  for(size_t i = 0; i < m_Instructions.size(); i++)
  {
    // reserve space for an added instruction so that curOp can stay valid even if we insert a new
    // op. This only actually does work the first time (or after we've inserted a new
    // instruction).
    m_Instructions.reserve(m_Instructions.size() + 1);

    Operation &curOp = m_Instructions[i];

    if(state == InstructionState::Broken)
      break;

    if((curOp.operation == OPCODE_IMM_ATOMIC_CMP_EXCH &&
        curOp.operands[1].indices[0].index == magicID) ||
       (curOp.operation == OPCODE_ATOMIC_CMP_STORE && curOp.operands[0].indices[0].index == magicID))
    {
      // AMD operations where the return value isn't used becomes an atomic_cmp_store instead of
      // imm_atomic_cmp_exch

      const int32_t instructionIndex = curOp.operation == OPCODE_ATOMIC_CMP_STORE ? 1 : 2;
      const int32_t param0Index = instructionIndex + 1;
      const int32_t param1Index = param0Index + 1;

      Operand dstOperand = curOp.operands[0];
      // if we have a store there's no destination, so set it to null
      if(curOp.operation == OPCODE_ATOMIC_CMP_STORE)
      {
        dstOperand = Operand();
        dstOperand.type = TYPE_NULL;
        dstOperand.setComps(0xff, 0xff, 0xff, 0xff);
      }

      // AMD operation
      if(curOp.operands[instructionIndex].type != TYPE_IMMEDIATE32)
      {
        RDCERR(
            "Expected literal value for AMD extension instruction. Was the shader compiled with "
            "optimisations disabled?");
        state = InstructionState::Broken;
        break;
      }

      uint32_t instruction = curOp.operands[instructionIndex].values[0];

      if(AMDInstruction::Magic.Get(instruction) == 5)
      {
        AMDInstruction::DX12Op amdop;

        if(m_API == GraphicsAPI::D3D11)
          amdop = AMDInstruction::convert(AMDInstruction::Opcode11.Get(instruction));
        else
          amdop = AMDInstruction::Opcode12.Get(instruction);

        uint32_t phase = AMDInstruction::Phase.Get(instruction);
        if(phase == 0)
        {
          srcParam[0] = curOp.operands[param0Index];
          srcParam[1] = curOp.operands[param1Index];
        }
        else if(phase == 1)
        {
          srcParam[2] = curOp.operands[param0Index];
          srcParam[3] = curOp.operands[param1Index];
        }
        else if(phase == 2)
        {
          srcParam[4] = curOp.operands[param0Index];
          srcParam[5] = curOp.operands[param1Index];
        }
        else if(phase == 3)
        {
          srcParam[6] = curOp.operands[param0Index];
          srcParam[7] = curOp.operands[param1Index];
        }

        Operation op;

        switch(amdop)
        {
          case AMDInstruction::DX12Op::Readfirstlane:
          {
            op.operation = OPCODE_AMD_READFIRSTLANE;
            op.operands.resize(2);
            op.operands[0] = dstOperand;
            op.operands[1].name = "src"_lit;
            op.operands[1] = srcParam[0];
            break;
          }
          case AMDInstruction::DX12Op::Readlane:
          {
            op.operation = OPCODE_AMD_READLANE;
            op.operands.resize(3);
            op.operands[0] = dstOperand;
            op.operands[1].name = "src"_lit;
            op.operands[1] = srcParam[0];
            // lane is encoded in instruction data
            op.operands[2].name = "lane"_lit;
            op.operands[2].type = TYPE_IMMEDIATE32;
            op.operands[2].numComponents = NUMCOMPS_1;
            op.operands[2].values[0] = AMDInstruction::Data.Get(instruction);
            break;
          }
          case AMDInstruction::DX12Op::LaneId:
          {
            op.operation = OPCODE_AMD_LANEID;
            op.operands = {dstOperand};
            break;
          }
          case AMDInstruction::DX12Op::Swizzle:
          {
            op.operation = OPCODE_AMD_SWIZZLE;
            op.operands.resize(2);
            op.operands[0] = dstOperand;
            op.operands[1].name = "src"_lit;
            op.operands[1] = srcParam[0];
            break;
          }
          case AMDInstruction::DX12Op::Ballot:
          {
            if(phase == 0)
            {
              // srcParams already stored, store the dst for phase 0
              dstParam[0] = dstOperand;
            }
            else if(phase == 1)
            {
              op.operation = OPCODE_AMD_BALLOT;
              op.operands.resize(3);
              op.operands[0] = dstParam[0];
              op.operands[1] = dstOperand;
              op.operands[2] = srcParam[0];
              op.operands[2].name = "predicate"_lit;
            }
            break;
          }
          case AMDInstruction::DX12Op::MBCnt:
          {
            op.operation = OPCODE_AMD_MBCNT;
            op.operands.resize(3);
            op.operands[0] = dstOperand;
            op.operands[1] = srcParam[0];
            op.operands[2] = srcParam[1];
            break;
          }
          case AMDInstruction::DX12Op::Min3U:
          case AMDInstruction::DX12Op::Min3F:
          case AMDInstruction::DX12Op::Med3U:
          case AMDInstruction::DX12Op::Med3F:
          case AMDInstruction::DX12Op::Max3U:
          case AMDInstruction::DX12Op::Max3F:
          {
            if(phase == 0)
            {
              // don't need the output at all, it's just used to chain the instructions
            }
            else if(phase == 1)
            {
              switch(amdop)
              {
                case AMDInstruction::DX12Op::Min3U: op.operation = OPCODE_AMD_MIN3U; break;
                case AMDInstruction::DX12Op::Min3F: op.operation = OPCODE_AMD_MIN3F; break;
                case AMDInstruction::DX12Op::Med3U: op.operation = OPCODE_AMD_MED3U; break;
                case AMDInstruction::DX12Op::Med3F: op.operation = OPCODE_AMD_MED3F; break;
                case AMDInstruction::DX12Op::Max3U: op.operation = OPCODE_AMD_MAX3U; break;
                case AMDInstruction::DX12Op::Max3F: op.operation = OPCODE_AMD_MAX3F; break;
                default: break;
              }
              op.operands.resize(4);
              op.operands[0] = dstOperand;
              op.operands[1] = srcParam[0];
              op.operands[2] = srcParam[1];
              op.operands[3] = srcParam[2];
            }
            break;
          }
          case AMDInstruction::DX12Op::BaryCoord:
          {
            if(phase == 0)
            {
              // srcParams already stored, store the dst for phase 0
              dstParam[0] = dstOperand;
            }
            else if(phase == 1)
            {
              if(AMDInstruction::BaryInterp.Get(instruction) != AMDInstruction::PerspPullModel)
              {
                // all modes except pull model have two outputs
                op.operation = OPCODE_AMD_BARYCOORD;
                op.operands.resize(2);
                op.operands[0].name = "i"_lit;
                op.operands[0] = dstParam[0];
                op.operands[0].name = "j"_lit;
                op.operands[1] = dstOperand;
              }
              else
              {
                dstParam[1] = dstOperand;
              }
            }
            else if(phase == 2)
            {
              // all modes except pull model have two outputs
              op.operation = OPCODE_AMD_BARYCOORD;
              op.operands.resize(3);
              op.operands[0].name = "invW"_lit;
              op.operands[0] = dstParam[0];
              op.operands[1].name = "invI"_lit;
              op.operands[1] = dstParam[1];
              op.operands[2].name = "invJ"_lit;
              op.operands[2] = dstOperand;
            }
            break;
          }
          case AMDInstruction::DX12Op::VtxParam:
          {
            op.operation = OPCODE_AMD_VTXPARAM;
            op.operands.resize(3);
            op.operands[0] = dstOperand;
            // vertexIndex is encoded in instruction data
            op.operands[1].name = "vertexIndex"_lit;
            op.operands[1].type = TYPE_IMMEDIATE32;
            op.operands[1].numComponents = NUMCOMPS_1;
            op.operands[1].values[0] = AMDInstruction::VtxParamVertex.Get(instruction);

            // decode and pretty-ify the parameter index and component
            op.operands[2].name = "parameter"_lit;
            op.operands[2].type = TYPE_INPUT;
            op.operands[2].numComponents = NUMCOMPS_1;
            op.operands[2].indices.resize(1);
            op.operands[2].indices[0].absolute = true;
            op.operands[2].indices[0].index = AMDInstruction::VtxParamParameter.Get(instruction);
            op.operands[2].setComps(AMDInstruction::VtxParamComponent.Get(instruction), 0xff, 0xff,
                                    0xff);

            break;
          }
          case AMDInstruction::DX12Op::ViewportIndex:
          {
            op.operation = OPCODE_AMD_GET_VIEWPORTINDEX;
            op.operands = {dstOperand};
            break;
          }
          case AMDInstruction::DX12Op::RtArraySlice:
          {
            op.operation = OPCODE_AMD_GET_RTARRAYSLICE;
            op.operands = {dstOperand};
            break;
          }
          case AMDInstruction::DX12Op::WaveReduce:
          case AMDInstruction::DX12Op::WaveScan:
          {
            if(amdop == AMDInstruction::DX12Op::WaveReduce)
              op.operation = OPCODE_AMD_WAVE_REDUCE;
            else
              op.operation = OPCODE_AMD_WAVE_SCAN;

            op.preciseValues = AMDInstruction::WaveOp.Get(instruction);
            break;
          }
          case AMDInstruction::DX12Op::LoadDwAtAddr:
          {
            if(phase == 0)
            {
              // don't need the output at all, it's just used to chain the instructions
            }
            else if(phase == 1)
            {
              op.operation = OPCODE_AMD_LOADDWATADDR;
              op.operands.resize(4);
              op.operands[0] = dstOperand;
              op.operands[1] = srcParam[0];
              op.operands[1].name = "gpuVaLoBits"_lit;
              op.operands[2] = srcParam[1];
              op.operands[2].name = "gpuVaHiBits"_lit;
              op.operands[3] = srcParam[2];
              op.operands[3].name = "offset"_lit;
            }
            break;
          }
          case AMDInstruction::DX12Op::DrawIndex:
          {
            op.operation = OPCODE_AMD_GET_DRAWINDEX;
            op.operands = {dstOperand};
            break;
          }
          case AMDInstruction::DX12Op::GetWaveSize:
          {
            op.operation = OPCODE_AMD_GET_WAVESIZE;
            op.operands = {dstOperand};
            break;
          }
          case AMDInstruction::DX12Op::BaseInstance:
          {
            op.operation = OPCODE_AMD_GET_BASEINSTANCE;
            op.operands = {dstOperand};
            break;
          }
          case AMDInstruction::DX12Op::BaseVertex:
          {
            op.operation = OPCODE_AMD_GET_BASEVERTEX;
            op.operands = {dstOperand};
            break;
          }
          case AMDInstruction::DX12Op::AtomicU64:
          {
            // if we're in the nothing state, move to the AMD UAV state so we watch for a UAV access
            // and nop it out
            if(state == InstructionState::Nothing)
              state = InstructionState::AMDUAVAtomic;

            VendorAtomicOp atomicop = convert(AMDInstruction::AtomicOp.Get(instruction));
            op.preciseValues = atomicop;

            bool isCAS = (atomicop == ATOMIC_OP_CAS);

            // for CAS we have four phases, only exit the state when we're in phase 3. For all other
            // instructions we have three phases so exit in phase 2.
            if(phase == 3 || (phase == 2 && !isCAS))
            {
              op.operation = OPCODE_AMD_U64_ATOMIC;
              state = InstructionState::Nothing;

              // output values first
              op.operands.push_back(dstParam[0]);
              op.operands.push_back(dstOperand);

              // then the saved UAV
              op.operands.push_back(uavParam);

              // then the address. This is in params [0], [1], [2]. If they all come from the same
              // register we can compact this
              if(srcParam[0].indices == srcParam[1].indices &&
                 srcParam[1].indices == srcParam[2].indices)
              {
                op.operands.push_back(srcParam[0]);
                op.operands.back().setComps(srcParam[0].comps[0], srcParam[1].comps[0],
                                            srcParam[2].comps[0], 0xff);
                op.operands.back().name = "address"_lit;

                // store in texelOffset whether the parameter is combined (1) or split (2)
                op.texelOffset[0] = 1;
              }
              else
              {
                op.operands.push_back(srcParam[0]);
                op.operands.back().name = "address.x"_lit;
                op.operands.back().setComps(srcParam[0].comps[0], 0xff, 0xff, 0xff);
                op.operands.push_back(srcParam[1]);
                op.operands.back().name = "address.y"_lit;
                op.operands.back().setComps(srcParam[1].comps[0], 0xff, 0xff, 0xff);
                op.operands.push_back(srcParam[2]);
                op.operands.back().name = "address.z"_lit;
                op.operands.back().setComps(srcParam[2].comps[0], 0xff, 0xff, 0xff);

                // store in texelOffset whether the parameter is combined (1) or split (2)
                op.texelOffset[0] = 2;
              }

              // for CAS, the compare value next
              if(isCAS)
              {
                if(srcParam[5].indices == srcParam[6].indices)
                {
                  op.operands.push_back(srcParam[5]);
                  op.operands.back().setComps(srcParam[5].comps[0], srcParam[6].comps[0], 0xff, 0xff);
                  op.operands.back().values[1] = srcParam[6].values[0];
                  op.operands.back().name = "compare_value"_lit;

                  // store in texelOffset whether the parameter is combined (1) or split (2)
                  op.texelOffset[1] = 1;
                }
                else
                {
                  op.operands.push_back(srcParam[5].reswizzle(0));
                  op.operands.back().name = "compare_value.x"_lit;
                  op.operands.back().setComps(srcParam[5].comps[0], 0xff, 0xff, 0xff);
                  op.operands.push_back(srcParam[6].reswizzle(0));
                  op.operands.back().name = "compare_value.y"_lit;
                  op.operands.back().setComps(srcParam[6].comps[0], 0xff, 0xff, 0xff);

                  // store in texelOffset whether the parameter is combined (1) or split (2)
                  op.texelOffset[1] = 2;
                }
              }

              // then the value
              if(srcParam[3].indices == srcParam[4].indices)
              {
                op.operands.push_back(srcParam[3]);
                op.operands.back().setComps(srcParam[3].comps[0], srcParam[4].comps[0], 0xff, 0xff);
                op.operands.back().values[1] = srcParam[4].values[0];
                op.operands.back().name = "value"_lit;

                // store in texelOffset whether the parameter is combined (1) or split (2)
                op.texelOffset[2] = 1;
              }
              else
              {
                op.operands.push_back(srcParam[3].reswizzle(0));
                op.operands.back().name = "value.x"_lit;
                op.operands.back().setComps(srcParam[3].comps[0], 0xff, 0xff, 0xff);
                op.operands.push_back(srcParam[4].reswizzle(0));
                op.operands.back().name = "value.y"_lit;
                op.operands.back().setComps(srcParam[4].comps[0], 0xff, 0xff, 0xff);

                // store in texelOffset whether the parameter is combined (1) or split (2)
                op.texelOffset[2] = 2;
              }
            }

            // phase 0's destination is the first destination
            if(phase == 0)
              dstParam[0] = dstOperand;

            break;
          }
        }

        // if the operation wasn't set we're on an intermediate phase. operands were saved,
        // wait until we have the full operation
        if(op.operation != NUM_REAL_OPCODES)
        {
          op.offset = curOp.offset;
          op.str = ToStr(op.operation);

          if(op.operation == OPCODE_AMD_BARYCOORD)
          {
            switch(AMDInstruction::BaryInterp.Get(instruction))
            {
              case AMDInstruction::LinearCenter: op.str += "_linear_center"; break;
              case AMDInstruction::LinearCentroid: op.str += "_linear_centroid"; break;
              case AMDInstruction::LinearSample: op.str += "_linear_sample"; break;
              case AMDInstruction::PerspCenter: op.str += "_persp_center"; break;
              case AMDInstruction::PerspCentroid: op.str += "_persp_centroid"; break;
              case AMDInstruction::PerspSample: op.str += "_persp_sample"; break;
              case AMDInstruction::PerspPullModel: op.str += "_persp_pullmodel"; break;
              default: op.str += "_unknown"; break;
            }
          }
          else if(op.operation == OPCODE_AMD_SWIZZLE)
          {
            switch(AMDInstruction::SwizzleOp.Get(instruction))
            {
              case AMDInstruction::SwapX1: op.str += "_swap1"; break;
              case AMDInstruction::SwapX2: op.str += "_swap2"; break;
              case AMDInstruction::SwapX4: op.str += "_swap4"; break;
              case AMDInstruction::SwapX8: op.str += "_swap8"; break;
              case AMDInstruction::SwapX16: op.str += "_swap16"; break;
              case AMDInstruction::ReverseX4: op.str += "_reverse4"; break;
              case AMDInstruction::ReverseX8: op.str += "_reverse8"; break;
              case AMDInstruction::ReverseX16: op.str += "_reverse16:"; break;
              case AMDInstruction::ReverseX32: op.str += "_reverse32:"; break;
              case AMDInstruction::BCastX2: op.str += "_bcast2"; break;
              case AMDInstruction::BCastX4: op.str += "_bcast4"; break;
              case AMDInstruction::BCastX8: op.str += "_bcast8"; break;
              case AMDInstruction::BCastX16: op.str += "_bcast16"; break;
              case AMDInstruction::BCastX32: op.str += "_bcast32"; break;
            }
          }
          else if(op.operation == OPCODE_AMD_WAVE_REDUCE || op.operation == OPCODE_AMD_WAVE_SCAN)
          {
            switch((VendorWaveOp)op.preciseValues)
            {
              default: break;
              case WAVE_OP_ADD_FLOAT: op.str += "_addf"; break;
              case WAVE_OP_ADD_SINT: op.str += "_addi"; break;
              case WAVE_OP_ADD_UINT: op.str += "_addu"; break;
              case WAVE_OP_MUL_FLOAT: op.str += "_mulf"; break;
              case WAVE_OP_MUL_SINT: op.str += "_muli"; break;
              case WAVE_OP_MUL_UINT: op.str += "_mulu"; break;
              case WAVE_OP_MIN_FLOAT: op.str += "_minf"; break;
              case WAVE_OP_MIN_SINT: op.str += "_mini"; break;
              case WAVE_OP_MIN_UINT: op.str += "_minu"; break;
              case WAVE_OP_MAX_FLOAT: op.str += "_maxf"; break;
              case WAVE_OP_MAX_SINT: op.str += "_maxi"; break;
              case WAVE_OP_MAX_UINT: op.str += "_maxu"; break;
              case WAVE_OP_AND: op.str += "_and"; break;
              case WAVE_OP_OR: op.str += "_or"; break;
              case WAVE_OP_XOR: op.str += "_xor"; break;
            }

            if(op.operation == OPCODE_AMD_WAVE_SCAN)
            {
              if(AMDInstruction::WaveOpFlags.Get(instruction) & 0x1)
                op.str += "_incl";
              if(AMDInstruction::WaveOpFlags.Get(instruction) & 0x2)
                op.str += "_excl";
            }
          }

          for(size_t a = 0; a < op.operands.size(); a++)
          {
            if(a == 0)
              op.str += " ";
            else
              op.str += ", ";
            op.str += op.operands[a].toString(m_Reflection, flags | ToString::ShowSwizzle);
          }

          m_Instructions.insert(i + 1, op);
        }
      }
      else
      {
        RDCERR("Expected magic value of 5 in encoded AMD instruction %x", instruction);
        state = InstructionState::Broken;
        break;
      }

      if(state == InstructionState::Broken)
        continue;

      // remove this operation, but keep the old operation so we can undo this if things go
      // wrong
      curOp.stride = curOp.operation;
      curOp.operation = OPCODE_VENDOR_REMOVED;
      RDCCOMPILE_ASSERT(sizeof(curOp.stride) >= sizeof(curOp.operation),
                        "Hackily assuming stride is big enough to hold an operation");
    }
    else if(curOp.operation == OPCODE_IMM_ATOMIC_ALLOC &&
            curOp.operands[1].indices[0].index == magicID)
    {
      // NV IncrementCounter()
      switch(state)
      {
        case InstructionState::Broken:
        case InstructionState::AMDUAVAtomic: break;
        // in Nothing an increment marks the beginning of an instruction of some type
        case InstructionState::Nothing:
        {
          state = InstructionState::InstructionHeader;
          break;
        }
        case InstructionState::InstructionHeader:
        {
          // the transition from instruction to any other state should happen via a markUAV or
          // opcode write, not with a counter increment
          RDCERR(
              "Expected either markUAV or opcode write before counter increment in unknown "
              "instruction header!");
          state = InstructionState::Broken;
          break;
        }
        case InstructionState::InstructionBody:
        {
          outputsNeeded--;
          if(outputsNeeded <= 0)
          {
            // once we've emitted all outputs, move to Nothing state
            state = InstructionState::Nothing;

            // and emit vendor instruction
            Operation op;

            switch(nvopcode)
            {
              case NvShaderOpcode::Shuffle:
              case NvShaderOpcode::ShuffleUp:
              case NvShaderOpcode::ShuffleDown:
              case NvShaderOpcode::ShuffleXor:
              {
                if(nvopcode == NvShaderOpcode::Shuffle)
                  op.operation = OPCODE_NV_SHUFFLE;
                else if(nvopcode == NvShaderOpcode::ShuffleUp)
                  op.operation = OPCODE_NV_SHUFFLE_UP;
                else if(nvopcode == NvShaderOpcode::ShuffleDown)
                  op.operation = OPCODE_NV_SHUFFLE_DOWN;
                else if(nvopcode == NvShaderOpcode::ShuffleXor)
                  op.operation = OPCODE_NV_SHUFFLE_XOR;

                op.operands.resize(4);
                op.operands[0] = curOp.operands[0];

                op.operands[1].name = "value"_lit;
                op.operands[1] = srcParam[0].reswizzle(0);
                if(nvopcode == NvShaderOpcode::Shuffle)
                  op.operands[2].name = "srcLane"_lit;
                else if(nvopcode == NvShaderOpcode::ShuffleXor)
                  op.operands[2].name = "laneMask"_lit;
                else
                  op.operands[2].name = "delta"_lit;
                op.operands[2] = srcParam[0].reswizzle(1);
                op.operands[3].name = "width"_lit;
                op.operands[3] = srcParam[0].reswizzle(3);
                break;
              }
              case NvShaderOpcode::VoteAll:
              case NvShaderOpcode::VoteAny:
              case NvShaderOpcode::VoteBallot:
              {
                if(nvopcode == NvShaderOpcode::VoteAll)
                  op.operation = OPCODE_NV_VOTE_ALL;
                else if(nvopcode == NvShaderOpcode::VoteAny)
                  op.operation = OPCODE_NV_VOTE_ANY;
                else if(nvopcode == NvShaderOpcode::VoteBallot)
                  op.operation = OPCODE_NV_VOTE_BALLOT;

                op.operands.resize(2);
                op.operands[0] = curOp.operands[0];
                op.operands[1] = srcParam[0];
                op.operands[1].name = "predicate"_lit;
                break;
              }
              case NvShaderOpcode::GetLaneId:
              {
                op.operation = OPCODE_NV_GET_LANEID;
                op.operands = {curOp.operands[0]};
                break;
              }
              case NvShaderOpcode::GetSpecial:
              {
                if(srcParam[0].type != TYPE_IMMEDIATE32)
                {
                  RDCERR("Expected literal value for special subopcode");
                  state = InstructionState::Broken;
                  break;
                }

                NvShaderSpecial special = (NvShaderSpecial)srcParam[0].values[0];

                if(special == NvShaderSpecial::ThreadLtMask)
                {
                  op.operation = OPCODE_NV_GET_THREADLTMASK;
                }
                else if(special == NvShaderSpecial::FootprintSingleLOD)
                {
                  op.operation = OPCODE_NV_GET_FOOTPRINT_SINGLELOD;
                }
                else
                {
                  RDCERR("Unexpected special subopcode");
                  state = InstructionState::Broken;
                  break;
                }
                op.operands = {curOp.operands[0]};
                break;
              }
              case NvShaderOpcode::MatchAny:
              {
                op.operation = OPCODE_NV_MATCH_ANY;
                op.operands.resize(2);
                op.operands[0] = curOp.operands[0];
                op.operands[1] = srcParam[0];
                // we don't need src1, it only indicates the number of components in the value,
                // which we already have
                break;
              }
              case NvShaderOpcode::GetShadingRate:
              {
                op.operation = OPCODE_NV_GET_SHADING_RATE;

                if(dstParam[0].indices == curOp.operands[0].indices &&
                   dstParam[1].indices == curOp.operands[0].indices)
                {
                  op.operands.push_back(curOp.operands[0]);
                  op.operands.back().name = "result"_lit;

                  // fixup the comps according to the shuffle
                  op.operands.back().setComps(
                      // x
                      dstParam[1].comps[0],
                      // y
                      dstParam[0].comps[0],
                      // z
                      curOp.operands[0].comps[0], 0xff);
                }
                else
                {
                  // these are in reverse order because we read them as numOutputs was decrementing
                  op.operands.push_back(dstParam[1]);
                  op.operands.back().name = "result.x"_lit;
                  op.operands.push_back(dstParam[0]);
                  op.operands.back().name = "result.y"_lit;
                  // z is last
                  op.operands.push_back(curOp.operands[0]);
                  op.operands.back().name = "result.z"_lit;
                }

                break;
              }
              // all footprint ops are very similar
              case NvShaderOpcode::Footprint:
              case NvShaderOpcode::FootprintBias:
              case NvShaderOpcode::FootprintLevel:
              case NvShaderOpcode::FootprintGrad:
              {
                if(nvopcode == NvShaderOpcode::Footprint)
                  op.operation = OPCODE_NV_FOOTPRINT;
                else if(nvopcode == NvShaderOpcode::FootprintBias)
                  op.operation = OPCODE_NV_FOOTPRINT_BIAS;
                else if(nvopcode == NvShaderOpcode::FootprintLevel)
                  op.operation = OPCODE_NV_FOOTPRINT_LEVEL;
                else if(nvopcode == NvShaderOpcode::FootprintGrad)
                  op.operation = OPCODE_NV_FOOTPRINT_GRAD;

                // four output values, could be assigned to different registers depending on packing
                // because they come back as scalars from increment counter. In general we have to
                // have them separately, but see if they all neatly line up into one output first.

                if(dstParam[0].indices == curOp.operands[0].indices &&
                   dstParam[1].indices == curOp.operands[0].indices &&
                   dstParam[2].indices == curOp.operands[0].indices)
                {
                  op.operands.push_back(curOp.operands[0]);
                  op.operands.back().name = "result"_lit;

                  // fixup the comps according to the shuffle
                  op.operands.back().setComps(
                      // x
                      dstParam[2].comps[0],
                      // y
                      dstParam[1].comps[0],
                      // z
                      dstParam[0].comps[0],
                      // w
                      curOp.operands[0].comps[0]);
                }
                else
                {
                  // these are in reverse order because we read them as numOutputs was decrementing
                  op.operands.push_back(dstParam[2]);
                  op.operands.back().name = "result.x"_lit;
                  op.operands.push_back(dstParam[1]);
                  op.operands.back().name = "result.y"_lit;
                  op.operands.push_back(dstParam[0]);
                  op.operands.back().name = "result.z"_lit;
                  // w is last
                  op.operands.push_back(curOp.operands[0]);
                  op.operands.back().name = "result.w"_lit;
                }

                // peel out the source parameters
                op.operands.push_back(srcParam[3].reswizzle(0));
                op.operands.back().name = "texSpace"_lit;
                op.operands.push_back(srcParam[0].reswizzle(0));
                op.operands.back().name = "texIndex"_lit;
                op.operands.push_back(srcParam[3].reswizzle(1));
                op.operands.back().name = "smpSpace"_lit;
                op.operands.push_back(srcParam[0].reswizzle(1));
                op.operands.back().name = "smpIndex"_lit;
                op.operands.push_back(srcParam[3].reswizzle(2));
                op.operands.back().name = "texType"_lit;
                op.operands.push_back(srcParam[1]);
                op.operands.back().comps[3] = 0xff;    // location is a float3
                op.operands.back().values[3] = 0;
                op.operands.back().name = "location"_lit;
                op.operands.push_back(srcParam[3].reswizzle(3));
                op.operands.back().name = "coarse"_lit;
                op.operands.push_back(srcParam[1].reswizzle(3));
                op.operands.back().name = "gran"_lit;

                if(nvopcode == NvShaderOpcode::FootprintBias)
                {
                  op.operands.push_back(srcParam[2].reswizzle(0));
                  op.operands.back().name = "bias"_lit;
                }
                else if(nvopcode == NvShaderOpcode::FootprintLevel)
                {
                  op.operands.push_back(srcParam[2].reswizzle(0));
                  op.operands.back().name = "lodLevel"_lit;
                }
                else if(nvopcode == NvShaderOpcode::FootprintGrad)
                {
                  op.operands.push_back(srcParam[2]);
                  op.operands.back().name = "ddx"_lit;
                  op.operands.push_back(srcParam[5]);
                  op.operands.back().name = "ddy"_lit;
                }

                op.operands.push_back(srcParam[4]);
                op.operands.back().name = "offset"_lit;

                break;
              }
              case NvShaderOpcode::ShuffleGeneric:
              {
                op.operation = OPCODE_NV_SHUFFLE_GENERIC;
                op.operands.resize(5);
                // first output is the actual result
                op.operands[0] = curOp.operands[0];
                // second output is the laneValid we stored previously
                op.operands[1] = dstParam[0];
                op.operands[1].name = "out laneValid"_lit;

                // we expect the params are packed into srcParam[0]

                op.operands[2] = srcParam[0].reswizzle(0);
                op.operands[2].name = "value"_lit;
                op.operands[3] = srcParam[0].reswizzle(1);
                op.operands[3].name = "srcLane"_lit;
                op.operands[4] = srcParam[0].reswizzle(2);
                op.operands[4].name = "width"_lit;
                break;
              }
              case NvShaderOpcode::VPRSEvalAttribAtSample:
              case NvShaderOpcode::VPRSEvalAttribSnapped:
              {
                if(nvopcode == NvShaderOpcode::VPRSEvalAttribAtSample)
                  op.operation = OPCODE_NV_VPRS_EVAL_ATTRIB_SAMPLE;
                else if(nvopcode == NvShaderOpcode::VPRSEvalAttribSnapped)
                  op.operation = OPCODE_NV_VPRS_EVAL_ATTRIB_SNAPPED;

                // up to four output values, could be assigned to different registers depending on
                // packing because they come back as scalars from increment counter. In general we
                // have to have them separately, but see if they all neatly line up into one output
                // first.

                bool allSameReg = true;

                for(int o = 0; o < numOutputs - 1; o++)
                {
                  if(!(dstParam[o].indices == curOp.operands[0].indices))
                  {
                    allSameReg = false;
                    break;
                  }
                }

                if(allSameReg)
                {
                  op.operands.push_back(curOp.operands[0]);
                  op.operands.back().name = "result"_lit;

                  for(int o = 0; o < 4; o++)
                  {
                    if(o >= numOutputs)
                      op.operands.back().comps[o] = 0xff;
                    else if(o + 1 == numOutputs)
                      op.operands.back().comps[o] = curOp.operands[0].comps[0];
                    else
                      op.operands.back().comps[o] = dstParam[numOutputs - 2 - o].comps[0];
                  }
                }
                else
                {
                  const char swz[] = "xyzw";
                  for(int o = 0; o < numOutputs - 1; o++)
                  {
                    // these are in reverse order because we read them as numOutputs was
                    // decrementing
                    op.operands.push_back(dstParam[numOutputs - 2 - o]);
                    op.operands.back().name = rdcstr("result.") + swz[o];
                  }
                  op.operands.push_back(curOp.operands[0]);
                  op.operands.back().name = rdcstr("result.") + swz[numOutputs - 1];
                }

                op.operands.push_back(srcParam[0]);
                op.operands.back().name = "attrib"_lit;

                if(nvopcode == NvShaderOpcode::VPRSEvalAttribAtSample)
                {
                  op.operands.push_back(srcParam[1]);
                  op.operands.back().name = "sampleIndex"_lit;
                  op.operands.push_back(srcParam[2]);
                  op.operands.back().name = "pixelOffset"_lit;
                }
                else if(nvopcode == NvShaderOpcode::VPRSEvalAttribSnapped)
                {
                  op.operands.push_back(srcParam[1]);
                  op.operands.back().name = "offset"_lit;
                }

                break;
              }
              default:
                RDCERR("Unexpected non-UAV opcode %d.", nvopcode);
                state = InstructionState::Broken;
                break;
            }

            if(state == InstructionState::Broken)
              break;

            op.offset = curOp.offset;
            op.str = ToStr(op.operation);

            for(size_t a = 0; a < op.operands.size(); a++)
            {
              if(a == 0)
                op.str += " ";
              else
                op.str += ", ";
              op.str += op.operands[a].toString(m_Reflection, flags | ToString::ShowSwizzle);
            }

            m_Instructions.insert(i + 1, op);
          }
          else
          {
            dstParam[outputsNeeded - 1] = curOp.operands[0];
          }
          break;
        }
        case InstructionState::UAVInstructionHeader1:
        {
          RDCERR("Expected other UAV write before counter increment in UAV instruction header!");
          state = InstructionState::Broken;
          break;
        }
        case InstructionState::UAVInstructionHeader2:
        {
          // now that we've gotten the UAV, we can go to the body
          state = InstructionState::UAVInstructionBody;
          break;
        }
        case InstructionState::UAVInstructionBody:
        {
          RDCERR(
              "Unexpected counter increment while processing UAV instruction body. Expected "
              "opcode!");
          state = InstructionState::Broken;
          break;
        }
      }

      if(state == InstructionState::Broken)
        continue;

      // remove this operation, but keep the old operation so we can undo this if things go
      // wrong
      curOp.stride = curOp.operation;
      curOp.operation = OPCODE_VENDOR_REMOVED;
    }
    else if(curOp.operation == OPCODE_STORE_STRUCTURED &&
            curOp.operands[0].indices[0].index == magicID)
    {
      if(curOp.operands[2].type != TYPE_IMMEDIATE32)
      {
        RDCERR("Expected literal value for UAV write offset");
        state = InstructionState::Broken;
        break;
      }

      // NV magic UAV write
      NvUAVParam param = (NvUAVParam)curOp.operands[2].values[0];

      switch(param)
      {
        case NvUAVParam::opcode:
        {
          if(curOp.operands[3].type != TYPE_IMMEDIATE32)
          {
            RDCERR(
                "Expected literal value being written as opcode. Was the shader compiled with "
                "optimisations disabled?");
            state = InstructionState::Broken;
            break;
          }

          nvopcode = (NvShaderOpcode)curOp.operands[3].values[0];

          // if this is NV_EXTN_OP_FP16_ATOMIC we should have come here in UAVInstructionBody.
          // That we're here now means this is the continuation of an earlier instruction.
          if(state == InstructionState::InstructionHeader && nvopcode == NvShaderOpcode::FP16Atomic)
            state = InstructionState::UAVInstructionBody;

          // if we're in instruction, this is the simple case so move to the output
          if(state == InstructionState::InstructionHeader)
          {
            // if we haven't gotten a number of outputs at all, set it to 1
            if(outputsNeeded <= 0)
              numOutputs = outputsNeeded = 1;
            state = InstructionState::InstructionBody;
          }
          else if(state == InstructionState::UAVInstructionBody)
          {
            // emit the instruction now, writing to the index register (which we know is
            // 'unused'). There might be nothing to read the result value. We'll look out for
            // loads and post-patch it.
            // once we've emitted all outputs, move to Nothing state
            state = InstructionState::Nothing;

            // and emit vendor instruction
            Operation op;
            // write to the index register at first. If there's a subsequent read of dst we'll patch
            // this instruction with the destination for that.
            op.operands.push_back(curOp.operands[1]);
            // also include the UAV we noted elsewhere
            op.operands.push_back(uavParam);

            NvShaderAtomic atomicop = NvShaderAtomic::Unknown;

            switch(nvopcode)
            {
              case NvShaderOpcode::FP16Atomic:
              {
                op.operation = OPCODE_NV_FP16_ATOMIC;

                if(srcParam[2].type != TYPE_IMMEDIATE32)
                {
                  RDCERR(
                      "Expected literal value as atomic opcode. Was the shader compiled with "
                      "optimisations disabled?");
                  state = InstructionState::Broken;
                  break;
                }

                atomicop = (NvShaderAtomic)srcParam[2].values[0];

                op.operands.push_back(srcParam[0]);
                op.operands.back().name = "address"_lit;
                op.operands.push_back(srcParam[1]);
                op.operands.back().name = "value"_lit;

                break;
              }
              case NvShaderOpcode::FP32Atomic:
              {
                op.operation = OPCODE_NV_FP32_ATOMIC;

                if(srcParam[2].type != TYPE_IMMEDIATE32)
                {
                  RDCERR(
                      "Expected literal value as atomic opcode. Was the shader compiled with "
                      "optimisations disabled?");
                  state = InstructionState::Broken;
                  break;
                }

                atomicop = (NvShaderAtomic)srcParam[2].values[0];

                op.operands.push_back(srcParam[0].reswizzle(0));
                op.operands.back().name = "byteAddress"_lit;
                op.operands.push_back(srcParam[1].reswizzle(0));
                op.operands.back().name = "value"_lit;

                break;
              }
              case NvShaderOpcode::U64Atomic:
              {
                op.operation = OPCODE_NV_U64_ATOMIC;

                if(srcParam[2].type != TYPE_IMMEDIATE32)
                {
                  RDCERR(
                      "Expected literal value as atomic opcode. Was the shader compiled with "
                      "optimisations disabled?");
                  state = InstructionState::Broken;
                  break;
                }

                // insert second dummy return value for high bits
                op.operands.insert(0, curOp.operands[1]);

                // make both of them NULL
                op.operands[0].type = TYPE_NULL;
                op.operands[0].setComps(0xff, 0xff, 0xff, 0xff);
                op.operands[1].type = TYPE_NULL;
                op.operands[1].setComps(0xff, 0xff, 0xff, 0xff);

                atomicop = (NvShaderAtomic)srcParam[2].values[0];

                op.operands.push_back(srcParam[0]);
                op.operands.back().numComponents = NUMCOMPS_1;
                op.operands.back().name = "address"_lit;

                // store in texelOffset whether the parameter is combined (1) or split (2).
                // on nv we assume the parameters are always combined
                op.texelOffset[0] = 1;
                op.texelOffset[1] = 1;
                op.texelOffset[2] = 1;

                if(atomicop == NvShaderAtomic::CompareAndSwap)
                {
                  op.operands.push_back(srcParam[1]);
                  op.operands.back().numComponents = NUMCOMPS_4;
                  op.operands.back().setComps(srcParam[1].comps[0], srcParam[1].comps[1], 0xff, 0xff);
                  op.operands.back().values[1] = srcParam[1].values[1];
                  op.operands.back().name = "compareValue"_lit;
                  op.operands.push_back(srcParam[1]);
                  op.operands.back().numComponents = NUMCOMPS_4;
                  op.operands.back().setComps(srcParam[1].comps[2], srcParam[1].comps[3], 0xff, 0xff);
                  op.operands.back().values[1] = srcParam[1].values[3];
                  op.operands.back().name = "value"_lit;
                }
                else
                {
                  op.operands.push_back(srcParam[1]);
                  op.operands.back().numComponents = NUMCOMPS_4;
                  op.operands.back().setComps(srcParam[1].comps[0], srcParam[1].comps[1], 0xff, 0xff);
                  op.operands.back().values[1] = srcParam[1].values[1];
                  op.operands.back().name = "value"_lit;
                }

                break;
              }
              default:
                RDCERR("Unexpected UAV opcode %d.", nvopcode);
                state = InstructionState::Broken;
                break;
            }

            if(state == InstructionState::Broken)
              break;

            if(atomicop == NvShaderAtomic::Unknown)
            {
              RDCERR("Couldn't determine atomic op");
              state = InstructionState::Broken;
              break;
            }

            op.offset = curOp.offset;
            op.preciseValues = (uint8_t)atomicop;
            op.str = ToStr(op.operation);

            switch(atomicop)
            {
              case NvShaderAtomic::Unknown: break;
              case NvShaderAtomic::And:
                op.str += "_and";
                op.preciseValues = ATOMIC_OP_AND;
                break;
              case NvShaderAtomic::Or:
                op.str += "_or";
                op.preciseValues = ATOMIC_OP_OR;
                break;
              case NvShaderAtomic::Xor:
                op.str += "_xor";
                op.preciseValues = ATOMIC_OP_XOR;
                break;
              case NvShaderAtomic::Add:
                op.str += "_add";
                op.preciseValues = ATOMIC_OP_ADD;
                break;
              case NvShaderAtomic::Max:
                op.str += "_max";
                op.preciseValues = ATOMIC_OP_MAX;
                break;
              case NvShaderAtomic::Min:
                op.str += "_min";
                op.preciseValues = ATOMIC_OP_MIN;
                break;
              case NvShaderAtomic::Swap:
                op.str += "_swap";
                op.preciseValues = ATOMIC_OP_SWAP;
                break;
              case NvShaderAtomic::CompareAndSwap:
                op.str += "_comp_swap";
                op.preciseValues = ATOMIC_OP_CAS;
                break;
            }

            for(size_t a = 0; a < op.operands.size(); a++)
            {
              if(a == 0)
                op.str += " ";
              else
                op.str += ", ";
              op.str += op.operands[a].toString(m_Reflection, flags | ToString::ShowSwizzle);
            }

            m_Instructions.insert(i + 1, op);

            // move into nothing state
            state = InstructionState::Nothing;
          }
          else
          {
            // no other state should be writing an opcode.
            RDCERR("Writing opcode in unexpected state %d.", state);
            state = InstructionState::Broken;
          }
          break;
        }
        case NvUAVParam::markUAV:
        {
          if(curOp.operands[3].type != TYPE_IMMEDIATE32 || curOp.operands[3].values[0] != 1)
          {
            RDCERR(
                "Expected literal 1 being written to markUAV. Was the shader compiled with "
                "optimisations disabled?");
            state = InstructionState::Broken;
            break;
          }

          if(state == InstructionState::InstructionHeader)
          {
            // start waiting for the user's UAV write
            state = InstructionState::UAVInstructionHeader1;
          }
          else
          {
            // no other state should be writing an opcode.
            RDCERR("Writing markUAV in unexpected state %d.", state);
            state = InstructionState::Broken;
          }
          break;
        }
        // store the src params unconditionally, don't care about the state.
        case NvUAVParam::src0:
        {
          srcParam[0] = curOp.operands[3];
          break;
        }
        case NvUAVParam::src1:
        {
          srcParam[1] = curOp.operands[3];
          break;
        }
        case NvUAVParam::src2:
        {
          srcParam[2] = curOp.operands[3];
          break;
        }
        case NvUAVParam::src3:
        {
          srcParam[3] = curOp.operands[3];
          break;
        }
        case NvUAVParam::src4:
        {
          srcParam[4] = curOp.operands[3];
          break;
        }
        case NvUAVParam::src5:
        {
          srcParam[5] = curOp.operands[3];
          break;
        }
        case NvUAVParam::dst:
        {
          RDCERR("Unexpected store to dst");
          state = InstructionState::Broken;
          break;
        }
        case NvUAVParam::numOutputs:
        {
          if(curOp.operands[3].type != TYPE_IMMEDIATE32)
          {
            RDCERR(
                "Expected literal value being written as numOutputs. Was the shader compiled "
                "with optimisations disabled?");
            state = InstructionState::Broken;
            break;
          }

          if(state == InstructionState::InstructionHeader ||
             state == InstructionState::InstructionBody)
          {
            // allow writing number of outputs in either header or body (before or after
            // simple
            // opcode)
            numOutputs = outputsNeeded = (int)curOp.operands[3].values[0];
          }
          else
          {
            // no other state should be writing an opcode.
            RDCERR("Writing numOutputs in unexpected state %d.", state);
            state = InstructionState::Broken;
          }
          break;
        }
        default:
        {
          RDCERR("Unexpected offset %u in nvidia magic UAV write.", param);
          state = InstructionState::Broken;
          break;
        }
      }

      if(state == InstructionState::Broken)
        continue;

      // remove this operation, but keep the old operation so we can undo this if things go
      // wrong
      curOp.stride = curOp.operation;
      curOp.operation = OPCODE_VENDOR_REMOVED;
    }
    else if(curOp.operation == OPCODE_LD_STRUCTURED && curOp.operands[3].indices[0].index == magicID)
    {
      // NV magic UAV load. This should only be of dst and only in the Nothing state after
      // we've
      // emitted a UAV instruction.
      if(state == InstructionState::Nothing)
      {
        if(curOp.operands[2].type == TYPE_IMMEDIATE32)
        {
          // NV magic UAV read
          NvUAVParam param = (NvUAVParam)curOp.operands[2].values[0];

          if(param == NvUAVParam::dst)
          {
            // search backwards for the last vendor operation. That's the one we're reading
            // from
            for(size_t j = i; j > 0; j--)
            {
              if(m_Instructions[j].operation >= OPCODE_VENDOR_FIRST)
              {
                // re-emit the instruction writing to the actual output now
                Operation op = m_Instructions[j];
                op.offset = curOp.offset;
                op.operands[0] = curOp.operands[0];
                op.str = ToStr(op.operation);

                // if this is an atomic64, the low/high bits are separate operands
                if(op.operation == OPCODE_NV_U64_ATOMIC)
                {
                  op.operands[1] = curOp.operands[0];
                  op.operands[0].setComps(curOp.operands[0].comps[0], 0xff, 0xff, 0xff);
                  op.operands[1].setComps(curOp.operands[0].comps[1], 0xff, 0xff, 0xff);
                }

                switch((VendorAtomicOp)op.preciseValues)
                {
                  case ATOMIC_OP_NONE: break;
                  case ATOMIC_OP_AND: op.str += "_and"; break;
                  case ATOMIC_OP_OR: op.str += "_or"; break;
                  case ATOMIC_OP_XOR: op.str += "_xor"; break;
                  case ATOMIC_OP_ADD: op.str += "_add"; break;
                  case ATOMIC_OP_MAX: op.str += "_max"; break;
                  case ATOMIC_OP_MIN: op.str += "_min"; break;
                  case ATOMIC_OP_SWAP: op.str += "_swap"; break;
                  case ATOMIC_OP_CAS: op.str += "_comp_swap"; break;
                }

                for(size_t a = 0; a < op.operands.size(); a++)
                {
                  if(a == 0)
                    op.str += " ";
                  else
                    op.str += ", ";
                  op.str += op.operands[a].toString(m_Reflection, flags | ToString::ShowSwizzle);
                }

                m_Instructions.insert(i + 1, op);

                // remove the old one, we've replaced it
                m_Instructions[j].operation = OPCODE_VENDOR_REMOVED;
                // if we break and try to revert this one, keep it removed
                m_Instructions[j].stride = OPCODE_VENDOR_REMOVED;
                // also remove the current one! but back up the original in case something
                // goes
                // wrong
                curOp.stride = curOp.operation;
                curOp.operation = OPCODE_VENDOR_REMOVED;
                break;
              }
            }
          }
          else
          {
            RDCERR("Unexpected read of UAV at offset %d instead of dst (%d)", param, NvUAVParam::dst);
            state = InstructionState::Broken;
          }
        }
        else
        {
          RDCERR("Expected literal value for UAV read offset");
          state = InstructionState::Broken;
        }
      }
      else
      {
        RDCERR("Unexpected UAV read in state %d.", state);
        state = InstructionState::Broken;
      }
    }
    else if(state == InstructionState::UAVInstructionHeader1)
    {
      // while we're here the next UAV write is snooped
      if(curOp.operation == OPCODE_STORE_RAW || curOp.operation == OPCODE_STORE_UAV_TYPED)
      {
        uavParam = curOp.operands[0];
        state = InstructionState::UAVInstructionHeader2;

        // remove this operation, but keep the old operation so we can undo this if things go
        // wrong
        curOp.stride = curOp.operation;
        curOp.operation = OPCODE_VENDOR_REMOVED;
      }
    }
    else if(state == InstructionState::AMDUAVAtomic)
    {
      // similarly for AMD we store the UAV referenced, but we don't change state - that happens
      // when we see the appropriate phase instruction.
      if(curOp.operation == OPCODE_STORE_RAW || curOp.operation == OPCODE_STORE_UAV_TYPED)
      {
        uavParam = curOp.operands[0];
        state = InstructionState::UAVInstructionHeader2;

        // remove this operation, but keep the old operation so we can undo this if things go
        // wrong
        curOp.stride = curOp.operation;
        curOp.operation = OPCODE_VENDOR_REMOVED;
      }
    }

    // any other operation we completely ignore
  }

  if(state == InstructionState::Broken)
  {
    // if we broke, restore the operations and remove any added vendor operations
    for(size_t i = 0; i < m_Instructions.size(); i++)
    {
      if(m_Instructions[i].operation == OPCODE_VENDOR_REMOVED)
        m_Instructions[i].operation = (OpcodeType)m_Instructions[i].stride;
      else if(m_Instructions[i].operation >= OPCODE_VENDOR_FIRST)
        m_Instructions[i].operation = OPCODE_VENDOR_REMOVED;
    }
  }

  // erase any OPCODE_VENDOR_REMOVED instructions now
  for(int32_t i = m_Instructions.count() - 1; i >= 0; i--)
  {
    if(m_Instructions[i].operation == OPCODE_VENDOR_REMOVED)
      m_Instructions.erase(i);
  }
}

};    // namespace DXBCBytecode
