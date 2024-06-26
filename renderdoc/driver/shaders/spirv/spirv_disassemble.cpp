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

#include "common/formatting.h"
#include "core/settings.h"
#include "maths/half_convert.h"
#include "spirv_op_helpers.h"
#include "spirv_reflect.h"

struct StructuredCFG
{
  enum
  {
    Loop,
    If,
    Switch,
  } type;

  // the Id of the header block
  rdcspv::Id headerBlock;

  // the merge target is basically the block after the construct.
  // branching to it exits the construct and all branches must go via the merge target
  rdcspv::Id mergeTarget;

  // only valid for ifs. The target of the else - we reverse the condition when printing if
  // necessary so that the false condition is second
  rdcspv::Id elseTarget;

  // only valid for loops
  rdcspv::Id continueTarget;

  // only valid for switches
  rdcarray<rdcspv::SwitchPairU64LiteralId> caseTargets;
  rdcspv::Id defaultTarget;
};

static rdcstr StringiseBinaryOperation(const std::function<rdcstr(rdcspv::Id)> &idName,
                                       rdcspv::Op op, rdcspv::Id operand1, rdcspv::Id operand2)
{
  rdcstr ret;

  ret += idName(operand1);
  ret += " ";
  switch(op)
  {
    case rdcspv::Op::IAdd:
    case rdcspv::Op::FAdd: ret += "+"; break;
    case rdcspv::Op::ISub:
    case rdcspv::Op::FSub: ret += "-"; break;
    case rdcspv::Op::IMul:
    case rdcspv::Op::FMul:
    case rdcspv::Op::VectorTimesMatrix:
    case rdcspv::Op::VectorTimesScalar:
    case rdcspv::Op::MatrixTimesMatrix:
    case rdcspv::Op::MatrixTimesVector:
    case rdcspv::Op::MatrixTimesScalar: ret += "*"; break;
    case rdcspv::Op::UDiv:
    case rdcspv::Op::SDiv:
    case rdcspv::Op::FDiv: ret += "/"; break;
    case rdcspv::Op::ShiftLeftLogical: ret += "<<"; break;
    case rdcspv::Op::BitwiseAnd: ret += "&"; break;
    case rdcspv::Op::BitwiseOr: ret += "|"; break;
    case rdcspv::Op::BitwiseXor: ret += "^"; break;
    case rdcspv::Op::LogicalEqual:
    case rdcspv::Op::IEqual:
    case rdcspv::Op::FOrdEqual:
    case rdcspv::Op::FUnordEqual: ret += "=="; break;
    case rdcspv::Op::LogicalNotEqual:
    case rdcspv::Op::INotEqual:
    case rdcspv::Op::FOrdNotEqual:
    case rdcspv::Op::FUnordNotEqual: ret += "!="; break;
    case rdcspv::Op::LogicalOr: ret += "||"; break;
    case rdcspv::Op::LogicalAnd: ret += "&&"; break;
    case rdcspv::Op::UGreaterThan:
    case rdcspv::Op::SGreaterThan:
    case rdcspv::Op::FOrdGreaterThan:
    case rdcspv::Op::FUnordGreaterThan: ret += ">"; break;
    case rdcspv::Op::UGreaterThanEqual:
    case rdcspv::Op::SGreaterThanEqual:
    case rdcspv::Op::FOrdGreaterThanEqual:
    case rdcspv::Op::FUnordGreaterThanEqual: ret += ">="; break;
    case rdcspv::Op::ULessThan:
    case rdcspv::Op::SLessThan:
    case rdcspv::Op::FOrdLessThan:
    case rdcspv::Op::FUnordLessThan: ret += "<"; break;
    case rdcspv::Op::ULessThanEqual:
    case rdcspv::Op::SLessThanEqual:
    case rdcspv::Op::FOrdLessThanEqual:
    case rdcspv::Op::FUnordLessThanEqual: ret += "<="; break;
    default: break;
  }

  ret += " ";
  ret += idName(operand2);

  return ret;
}

namespace rdcspv
{
rdcstr Reflector::Disassemble(const rdcstr &entryPoint,
                              std::map<size_t, uint32_t> &instructionLines) const
{
  std::set<rdcstr> usedNames;
  std::map<Id, rdcstr> dynamicNames;

  auto idName = [this, &dynamicNames](Id id) -> rdcstr {
    // see if we have a dynamic name assigned (to disambiguate), if so use that
    auto it = dynamicNames.find(id);
    if(it != dynamicNames.end())
      return it->second;

    // otherwise try the string
    rdcstr ret = strings[id];
    if(!ret.empty())
    {
      // escape and truncate any multiline strings
      if(ret.indexOf('\n') >= 0 || ret.indexOf('\r') >= 0)
      {
        rdcstr escaped = "\"";

        for(size_t i = 0; i < ret.size(); i++)
        {
          if(i == 100 && i + 1 < ret.size())
          {
            escaped += "...";
            break;
          }

          if(ret[i] == '\r')
            escaped += "\\r";
          else if(ret[i] == '\t')
            escaped += "\\t";
          else if(ret[i] == '\n')
            escaped += "\\n";
          else
            escaped += ret[i];
        }

        escaped.push_back('"');
        return escaped;
      }
      return ret;
    }

    // for non specialised constants, see if we can stringise them directly if they're unnamed
    ret = StringiseConstant(id);
    if(!ret.empty())
      return ret;

    // if we *still* have nothing, just stringise the id itself
    return StringFormat::Fmt("_%u", id.value());
  };
  auto constIntVal = [this](Id id) { return EvaluateConstant(id, {}).value.u32v[0]; };
  auto declName = [this, &idName, &usedNames, &dynamicNames](Id typeId, Id id) -> rdcstr {
    if(typeId == Id())
      return idName(id);

    rdcstr ret = dataTypes[typeId].name;
    if(ret.empty())
      ret = StringFormat::Fmt("type%u", typeId.value());

    if(id == Id())
      return ret;

    rdcstr basename = strings[id];
    if(basename.empty())
    {
      return ret + " " + StringFormat::Fmt("_%u", id.value());
    }

    rdcstr name = basename;

    int alias = 2;
    while(usedNames.find(name) != usedNames.end())
    {
      name = basename + "@" + ToStr(alias);
      alias++;
    }

    usedNames.insert(name);
    dynamicNames[id] = name;

    return ret + " " + name;
  };
  auto getDecorationString = [&idName](const Decorations &dec) -> rdcstr {
    if(!dec.HasDecorations())
      return rdcstr();

    rdcstr ret;

    ret += " : [[";

    if(dec.flags & Decorations::Block)
      ret += "Block, ";
    if(dec.flags & Decorations::BufferBlock)
      ret += "BufferBlock, ";
    if(dec.flags & Decorations::RowMajor)
      ret += "RowMajor, ";
    if(dec.flags & Decorations::ColMajor)
      ret += "ColMajor, ";

    if(dec.flags & Decorations::HasDescriptorSet)
      ret += StringFormat::Fmt("DescriptorSet(%u), ", dec.set);
    if(dec.flags & Decorations::HasBinding)
      ret += StringFormat::Fmt("Binding(%u), ", dec.binding);
    if(dec.flags & Decorations::HasBuiltIn)
      ret += StringFormat::Fmt("BuiltIn(%s), ", ToStr(dec.builtIn).c_str());
    if(dec.flags & Decorations::HasLocation)
      ret += StringFormat::Fmt("Location(%u), ", dec.location);
    if(dec.flags & Decorations::HasArrayStride)
      ret += StringFormat::Fmt("ArrayStride(%u), ", dec.arrayStride);
    if(dec.flags & Decorations::HasMatrixStride)
      ret += StringFormat::Fmt("MatrixStride(%u), ", dec.matrixStride);
    if(dec.flags & Decorations::HasOffset)
      ret += StringFormat::Fmt("Offset(%u), ", dec.offset);
    if(dec.flags & Decorations::HasSpecId)
      ret += StringFormat::Fmt("SpecId(%u), ", dec.specID);

    for(const DecorationAndParamData &d : dec.others)
      ret += ParamToStr(idName, d) + ", ";

    ret.pop_back();
    ret.pop_back();

    ret += "]]";

    return ret;
  };
  auto accessor = [](const DataType *baseType, uint32_t idx, rdcstr idxname) -> rdcstr {
    if(baseType->type == DataType::ArrayType || baseType->type == DataType::MatrixType ||
       (baseType->type == DataType::VectorType && !idxname.empty()))
    {
      if(idxname.empty())
        idxname = ToStr(idx);
      return StringFormat::Fmt("[%s]", idxname.c_str());
    }

    if(baseType->type == DataType::VectorType)
    {
      if(idx >= 4)
      {
        if(idxname.empty())
          idxname = ToStr(idx);
        return StringFormat::Fmt("[%s]", idxname.c_str());
      }

      rdcstr ret = ".";
      const char comps[5] = "xyzw";
      ret.push_back(comps[idx]);
      return ret;
    }

    RDCASSERT(baseType->type == DataType::StructType && idx < baseType->children.size());
    if(!baseType->children[idx].name.empty())
      return "." + baseType->children[idx].name;

    return StringFormat::Fmt("._child%u", idx);
  };

  rdcstr ret;
  rdcstr indent;

  // stack of structured CFG constructs
  rdcarray<StructuredCFG> cfgStack;

  // set of labels that must be printed because we have gotos for them
  std::set<Id> printLabels;

  Id currentBlock;

  ret = StringFormat::Fmt(
      "SPIR-V %u.%u module, <id> bound of %u\n"
      "\n"
      "Generator: %s\n"
      "Generator Version: %u\n"
      "\n",
      m_MajorVersion, m_MinorVersion, m_SPIRV[3], ToStr(m_Generator).c_str(), m_GeneratorVersion);

  uint32_t lineNum = 6;

  for(size_t sec = 0; sec < Section::Count; sec++)
  {
    ConstIter it(m_SPIRV, m_Sections[sec].startOffset);
    ConstIter end(m_SPIRV, m_Sections[sec].endOffset);

    for(; it < end; it++)
    {
      instructionLines[it.offs()] = lineNum;

      // special case some opcodes for more readable disassembly, but generally pass to the
      // auto-generated disassembler
      switch(it.opcode())
      {
        ////////////////////////////////////////////////////////////////////////////////////////
        // global instructions
        ////////////////////////////////////////////////////////////////////////////////////////

        // don't print the full source
        case Op::Source:
        {
          OpSource decoded(it);
          ret += StringFormat::Fmt("Source(%s, %u", ToStr(decoded.sourceLanguage).c_str(),
                                   decoded.version);
          if(decoded.file != Id())
            ret += ", file: " + idName(decoded.file);
          ret += ")";
          break;
        }

        // split the interface list onto separate lines to avoid hugely long lines on some shaders
        case Op::EntryPoint:
        {
          OpEntryPoint decoded(it);
          ret += rdcstr("EntryPoint("_lit) + ParamToStr(idName, decoded.executionModel) + ", " +
                 ParamToStr(idName, decoded.entryPoint) + ", " + ParamToStr(idName, decoded.name) +
                 ", {\n";
          lineNum++;
          for(size_t i = 0; i < decoded.iface.size(); i++)
          {
            ret += "    " + ParamToStr(idName, decoded.iface[i]);
            if(i + 1 < decoded.iface.size())
              ret += ", ";
            ret += "\n";
            lineNum++;
          }
          ret += "  })";
          break;
        }

        // ignore these operations entirely
        case Op::SourceContinued:
        case Op::Line:
        case Op::String:
        case Op::Name:
        case Op::MemberName:
        case Op::ExtInstImport: continue;

        // ignore decorations too, we already have these cached
        case Op::Decorate:
        case Op::DecorateId:
        case Op::DecorateString:
        case Op::MemberDecorate:
        case Op::MemberDecorateString:
        case Op::DecorationGroup:
        case Op::GroupDecorate:
        case Op::GroupMemberDecorate: continue;

        // suppress almost all types
        case Op::TypeVoid:
        case Op::TypeBool:
        case Op::TypeInt:
        case Op::TypeFloat:
        case Op::TypeVector:
        case Op::TypeMatrix:
        case Op::TypePointer:
        case Op::TypeArray:
        case Op::TypeImage:
        case Op::TypeSampler:
        case Op::TypeSampledImage:
        case Op::TypeFunction:
        case Op::TypeRuntimeArray:
        case Op::TypeRayQueryKHR: continue;

        case Op::TypeForwardPointer:
        {
          OpTypeForwardPointer decoded(it);

          ret += ToStr(decoded.storageClass) + " " + declName(decoded.pointerType, Id()) + ";\n";
          lineNum++;

          continue;
        }

        // structs we print out
        case Op::TypeStruct:
        {
          OpTypeStruct decoded(it);

          const DataType &type = dataTypes[decoded.result];

          ret += "struct " + idName(decoded.result) +
                 getDecorationString(decorations[decoded.result]) + " {\n";
          lineNum++;
          for(size_t i = 0; i < type.children.size(); i++)
          {
            ret += "  " + declName(type.children[i].type, Id());
            ret += " ";
            if(!type.children[i].name.empty())
              ret += type.children[i].name;
            else
              ret += StringFormat::Fmt("_child%zu", i);
            ret += getDecorationString(type.children[i].decorations);
            ret += getDecorationString(decorations[type.children[i].type]);
            ret += ";\n";
            lineNum++;
          }
          ret += "}\n";
          lineNum++;
          continue;
        }

        // scalar and vector constants are inlined when they're unnamed and not specialised, so only
        // declare others.
        case Op::ConstantTrue:
        case Op::ConstantFalse:
        case Op::ConstantNull:
        case Op::Undef: continue;

        case Op::SpecConstant:
        case Op::Constant:
        {
          OpDecoder decoded(it);

          const DataType &type = dataTypes[decoded.resultType];

          RDCASSERT(type.type == DataType::ScalarType);

          // if it's not specialised, not decorated, and not named, don't declare it at all.
          if(it.opcode() == Op::Constant &&
             specConstants.find(decoded.result) == specConstants.end() &&
             strings[decoded.result].empty() && !decorations[decoded.result].HasDecorations())
          {
            continue;
          }

          ret += indent;
          ret +=
              StringFormat::Fmt("const %s = ", declName(decoded.resultType, decoded.result).c_str());

          // evalute the value with no specialisation
          ShaderValue value = EvaluateConstant(decoded.result, {}).value;

          switch(type.scalar().Type())
          {
            case VarType::Half: ret += ToStr(value.f16v[0]); break;
            case VarType::Float: ret += ToStr(value.f32v[0]); break;
            case VarType::Double: ret += ToStr(value.f64v[0]); break;
            case VarType::SInt: ret += ToStr(value.s32v[0]); break;
            case VarType::SShort: ret += ToStr(value.s16v[0]); break;
            case VarType::SByte: ret += ToStr(value.s8v[0]); break;
            case VarType::Bool: ret += value.u32v[0] ? "true" : "false"; break;
            case VarType::UInt: ret += ToStr(value.u32v[0]); break;
            case VarType::UShort: ret += ToStr(value.u16v[0]); break;
            case VarType::UByte: ret += ToStr(value.u8v[0]); break;
            case VarType::SLong: ret += ToStr(value.s64v[0]); break;
            case VarType::ULong: ret += ToStr(value.u64v[0]); break;
            // none of these types are expected, either because they're opaque or (for struct)
            // because ConstantComposite should have been used
            case VarType::Enum:
            case VarType::Struct:
            case VarType::Unknown:
            case VarType::GPUPointer:
            case VarType::ConstantBlock:
            case VarType::ReadOnlyResource:
            case VarType::ReadWriteResource:
            case VarType::Sampler: ret += "???"; break;
          }

          ret += getDecorationString(decorations[decoded.result]);

          break;
        }
        case Op::SpecConstantComposite:
        case Op::ConstantComposite:
        {
          OpConstantComposite decoded(it);

          const DataType &type = dataTypes[decoded.resultType];

          // if it's a vector, not specialised, not decorated, and not named, don't declare it at
          // all.
          if(it.opcode() == Op::ConstantComposite && type.type == DataType::VectorType &&
             specConstants.find(decoded.result) == specConstants.end() &&
             strings[decoded.result].empty() && !decorations[decoded.result].HasDecorations())
          {
            continue;
          }

          ret += indent;
          ret += StringFormat::Fmt("const %s = {",
                                   declName(decoded.resultType, decoded.result).c_str());

          for(size_t i = 0; i < decoded.constituents.size(); i++)
          {
            ret += idName(decoded.constituents[i]);
            if(i + 1 < decoded.constituents.size())
              ret += ", ";
          }
          ret += "}";

          ret += getDecorationString(decorations[decoded.result]);

          break;
        }
        case Op::SpecConstantOp:
        {
          OpDecoder decoded(it);

          Op op = (Op)it.word(3);

          ret += indent;
          ret +=
              StringFormat::Fmt("const %s = ", declName(decoded.resultType, decoded.result).c_str());

          bool binary = false;

          switch(op)
          {
            case Op::IAdd:
            case Op::ISub:
            case Op::IMul:
            case Op::UDiv:
            case Op::SDiv:
            case Op::UMod:
            case Op::SRem:
            case Op::SMod:
            case Op::ShiftRightLogical:
            case Op::ShiftRightArithmetic:
            case Op::ShiftLeftLogical:
            case Op::BitwiseOr:
            case Op::BitwiseXor:
            case Op::BitwiseAnd:
            case Op::LogicalOr:
            case Op::LogicalAnd:
            case Op::LogicalEqual:
            case Op::LogicalNotEqual:
            case Op::IEqual:
            case Op::INotEqual:
            case Op::ULessThan:
            case Op::SLessThan:
            case Op::UGreaterThan:
            case Op::SGreaterThan:
            case Op::ULessThanEqual:
            case Op::SLessThanEqual:
            case Op::UGreaterThanEqual:
            case Op::SGreaterThanEqual:
              RDCASSERT(it.size() == 6);
              binary = true;
              ret += StringiseBinaryOperation(idName, op, Id::fromWord(it.word(4)),
                                              Id::fromWord(it.word(5)));
              break;
            default: break;
          }

          if(!binary)
          {
            ret += StringFormat::Fmt("%s(", declName(decoded.resultType, decoded.result).c_str(),
                                     ToStr(op).c_str());

            // interpret params as Ids, except for CompositeExtract and CompositeInsert
            size_t limit = it.size();
            if(op == Op::CompositeExtract)
              limit = 5;
            else if(op == Op::CompositeInsert)
              limit = 6;

            for(size_t w = 4; w < limit; w++)
            {
              ret += idName(Id::fromWord(it.word(w)));

              if(w + 1 < limit)
                ret += ", ";
            }

            // if we have trailing literals, print them
            if(limit < it.size())
            {
              for(size_t w = limit; w < it.size(); w++)
              {
                ret += ToStr(it.word(w));

                if(w + 1 < it.size())
                  ret += ", ";
              }
            }

            ret += ")";
          }

          break;
        }

        // declare variables by hand with optional initialiser
        case Op::Variable:
        {
          OpVariable decoded(it);
          ret += indent;
          if(decoded.storageClass != StorageClass::Function)
            ret += ToStr(decoded.storageClass) + " ";
          ret += declName(decoded.resultType, decoded.result);
          if(decoded.HasInitializer())
            ret += " = " + idName(decoded.initializer);
          ret += getDecorationString(decorations[decoded.result]);
          break;
        }

        ////////////////////////////////////////////////////////////////////////////////////////
        // control flow and scope indentation
        ////////////////////////////////////////////////////////////////////////////////////////

        // indent around functions
        case Op::Function:
        {
          OpFunction decoded(it);
          rdcstr name = declName(decoded.resultType, decoded.result);

          // glslang outputs encoded type information in the OpName of functions, strip it
          {
            int32_t offs = name.indexOf('(');
            if(offs > 0)
              name.erase(offs, name.size() - offs);
          }

          ret += StringFormat::Fmt("%s(", name.c_str());

          // peek ahead and consume any function parameters
          {
            it++;
            while(it.opcode() == Op::Line || it.opcode() == Op::NoLine)
            {
              it++;
              instructionLines[it.offs()] = lineNum;
            }

            const bool added_params = (it.opcode() == Op::FunctionParameter);

            while(it.opcode() == Op::FunctionParameter)
            {
              OpFunctionParameter param(it);
              ret += declName(param.resultType, param.result) + ", ";
              it++;
              instructionLines[it.offs()] = lineNum;
              while(it.opcode() == Op::Line || it.opcode() == Op::NoLine)
              {
                it++;
                instructionLines[it.offs()] = lineNum;
              }
            }

            // remove trailing ", "
            if(added_params)
            {
              ret.pop_back();
              ret.pop_back();
            }
          }

          instructionLines[it.offs()] = lineNum;

          ret += ")";

          if(decoded.functionControl != FunctionControl::None)
            ret += " [[" + ToStr(decoded.functionControl) + "]]";

          ret += getDecorationString(decorations[decoded.result]);

          // if we reached the end, it's a declaration not a definition
          if(it.opcode() == Op::FunctionEnd)
            ret += ";";
          else
            ret += " {";

          // it points to the FunctionEnd (for declarations) or first Label (for definitions).
          // the next it++ will skip that but it's fine, because we have processed them
          ret += "\n";
          lineNum++;

          indent += "  ";
          continue;
        }
        case Op::FunctionEnd:
        {
          ret += "}\n\n";
          lineNum += 2;
          indent.resize(indent.size() - 2);
          continue;
        }
        // indent around control flow
        case Op::SelectionMerge:
        {
          OpSelectionMerge decoded(it);
          ret += indent;
          if(decoded.selectionControl != SelectionControl::None)
            ret += StringFormat::Fmt("[[%s]]", ToStr(decoded.selectionControl).c_str());

          // increment any previous instructions that were pointing at this line, to point at the
          // next one.
          for(auto lineIt = instructionLines.end(); lineIt != instructionLines.begin();)
          {
            --lineIt;
            if(lineIt->second == lineNum)
            {
              lineIt->second++;
              continue;
            }
            break;
          }

          ret += "\n";
          lineNum++;

          StructuredCFG cfg;
          cfg.headerBlock = currentBlock;
          cfg.mergeTarget = decoded.mergeBlock;

          it++;
          instructionLines[it.offs()] = lineNum;

          // the Switch or BranchConditional operation declares the structured CFG
          if(it.opcode() == Op::Switch)
          {
            cfg.type = StructuredCFG::Switch;

            OpSwitch32 switch32(it);
            // selector and default are common beteen 32-bit and 64-bit versions of OpSwitch
            Id selector = switch32.selector;
            cfg.defaultTarget = switch32.def;

            const DataType &type = dataTypes[idTypes[selector]];
            RDCASSERT(type.type == DataType::ScalarType);
            const uint32_t selectorWidth = type.scalar().width;

            const bool longLiterals = (selectorWidth == 64);
            if(!longLiterals)
            {
              for(size_t i = 0; i < switch32.targets.size(); ++i)
              {
                SwitchPairU32LiteralId target = switch32.targets[i];
                cfg.caseTargets.push_back({target.literal, target.target});
              }
            }
            else
            {
              OpSwitch64 switch64(it);
              for(size_t i = 0; i < switch64.targets.size(); ++i)
              {
                SwitchPairU64LiteralId target = switch64.targets[i];
                cfg.caseTargets.push_back({target.literal, target.target});
              }
            }

            ret += indent;
            ret += StringFormat::Fmt("switch(%s) {\n", idName(selector).c_str());
            lineNum++;

            // add another level - each case label will be un-intended.
            indent += "  ";
          }
          else
          {
            cfg.type = StructuredCFG::If;

            OpBranchConditional decodedbranch(it);

            ConstIter nextit = it;
            nextit++;
            while(nextit.opcode() == Op::Line || nextit.opcode() == Op::NoLine)
              nextit++;

            // if we got here we're a simple if() without an else - SelectionMerge/LoopMerge
            // consumes any branch

            // next opcode *must* be a label because this is the end of a block
            RDCASSERTEQUAL(nextit.opcode(), Op::Label);
            OpLabel decodedlabel(nextit);

            if(decodedbranch.trueLabel == decodedlabel.result ||
               decodedbranch.falseLabel == decodedlabel.result)
            {
              const char *negate = (decodedbranch.falseLabel == decodedlabel.result) ? "!" : "";
              ret += indent;
              ret += StringFormat::Fmt("if(%s%s) {", negate, idName(decodedbranch.condition).c_str());

              if(decodedbranch.branchweights.size() == 2)
                ret += StringFormat::Fmt(" [[true: %u, false: %u]]", decodedbranch.branchweights[0],
                                         decodedbranch.branchweights[1]);

              ret += "\n";
              lineNum++;

              cfg.elseTarget = (decodedbranch.trueLabel == decodedlabel.result)
                                   ? decodedbranch.falseLabel
                                   : decodedbranch.trueLabel;
            }
            else
            {
              RDCWARN(
                  "Unexpected SPIR-V formulation - OpBranchConditional not followed by either true "
                  "or false block");

              // this is legal. We just have to emit an if with gotos
              ret += indent;
              ret += StringFormat::Fmt(
                  "if(%s) { goto %s; } else { goto %s; }\n", idName(decodedbranch.condition).c_str(),
                  idName(decodedbranch.trueLabel).c_str(), idName(decodedbranch.falseLabel).c_str());
              lineNum++;

              // need to print these labels now
              printLabels.insert(decodedbranch.trueLabel);
              printLabels.insert(decodedbranch.falseLabel);

              continue;
            }
          }

          cfgStack.push_back(cfg);
          indent += "  ";

          continue;
        }
        case Op::LoopMerge:
        {
          OpLoopMerge decoded(it);
          if(decoded.loopControl != LoopControl::None)
          {
            ret += indent;
            ret += StringFormat::Fmt("[[%s]]\n", ParamToStr(idName, decoded.loopControl).c_str());
            lineNum++;
          }

          it++;
          instructionLines[it.offs()] = lineNum;
          if(it.opcode() == Op::Branch)
          {
            OpBranch decodedbranch(it);

            // if the first branch after the loopmerge is to the continue target, this is an empty
            // infinite loop. Don't try and detect the branch, just make an empty loop and exit.
            if(decodedbranch.targetLabel != decoded.continueTarget)
            {
              ConstIter nextit = it;
              nextit++;
              while(nextit.opcode() == Op::Line || nextit.opcode() == Op::NoLine)
                nextit++;

              // we can now ignore everything between us and the label of this branch, which is
              // almost always going to be the very next label.
              //
              // The reasoning is this:
              // - assume the next block is not the one we are jumping to
              // - all blocks inside the loop must only ever branch backwards to the header block
              //   (that's this one) so the block can't be jumped to from within the loop
              // - it's also illegal to jump into a structured control flow construct from outside,
              //   so it can't be jumped to from outside the loop
              // - that means it is completely inaccessible from everywhere, so we can skip it

              while(nextit.opcode() != Op::Label ||
                    OpLabel(nextit).result != decodedbranch.targetLabel)
              {
                nextit++;
                it++;
                instructionLines[it.offs()] = lineNum;
              }
            }
          }
          else
          {
            RDCASSERTEQUAL(it.opcode(), Op::BranchConditional);
            OpBranchConditional decodedbranch(it);

            // we assume one of the targets of this is the merge block. Then we can express this as
            // a while(condition) loop, potentially by negating the condition.
            // If it isn't, then we have to do an infinite loop with a branchy if at the top

            if(decodedbranch.trueLabel == decoded.mergeBlock ||
               decodedbranch.falseLabel == decoded.mergeBlock)
            {
              const char *negate = (decodedbranch.trueLabel == decoded.mergeBlock) ? "" : "!";
              ret += indent;
              ret += StringFormat::Fmt("if(%s%s) break;", negate,
                                       idName(decodedbranch.condition).c_str());

              if(decodedbranch.branchweights.size() == 2)
                ret += StringFormat::Fmt(" [[true: %u, false: %u]]", decodedbranch.branchweights[0],
                                         decodedbranch.branchweights[1]);

              ret += "\n";
              lineNum++;
            }
            else
            {
              RDCWARN(
                  "Unexpected SPIR-V construct - loop with conditional branch both pointing inside "
                  "the loop");
              ret += indent;
              ret += "while(true) {\n";
              lineNum++;

              ret += indent;
              ret += StringFormat::Fmt("  if(%s) { goto %s; } else { goto %s; }\n",
                                       idName(decodedbranch.condition).c_str(),
                                       idName(decodedbranch.trueLabel).c_str(),
                                       idName(decodedbranch.falseLabel).c_str());
              lineNum++;

              // need to print these labels now
              printLabels.insert(decodedbranch.trueLabel);
              printLabels.insert(decodedbranch.falseLabel);
            }
          }

          StructuredCFG cfg = {StructuredCFG::Loop};
          cfg.headerBlock = currentBlock;
          cfg.mergeTarget = decoded.mergeBlock;
          cfg.continueTarget = decoded.continueTarget;
          cfgStack.push_back(cfg);

          continue;
        }
        case Op::Label:
        {
          OpLabel decoded(it);

          currentBlock = decoded.result;

          if(!cfgStack.empty() && decoded.result == cfgStack.back().mergeTarget)
          {
            // if this is the latest merge block print a closing brace and reduce the indent
            indent.resize(indent.size() - 2);

            // if this is a switch, remove another level
            if(cfgStack.back().type == StructuredCFG::Switch)
              indent.resize(indent.size() - 2);

            ret += indent;
            ret += "}\n\n";
            lineNum += 2;

            cfgStack.pop_back();
          }
          else if(!cfgStack.empty() && cfgStack.back().type == StructuredCFG::If &&
                  decoded.result == cfgStack.back().elseTarget)
          {
            // if this is the current if's else{} then print it
            ret += indent.substr(0, indent.size() - 2);
            ret += "} else {\n";
            lineNum++;
          }
          else if(!cfgStack.empty() && cfgStack.back().type == StructuredCFG::Switch &&
                  decoded.result == cfgStack.back().defaultTarget)
          {
            // if this is the current switch's default: then print it
            ret += indent.substr(0, indent.size() - 2);
            ret += "default:\n";
            lineNum++;
          }

          if(!cfgStack.empty() && cfgStack.back().type == StructuredCFG::Switch)
          {
            for(const SwitchPairU64LiteralId &caseTarget : cfgStack.back().caseTargets)
            {
              if(caseTarget.target == decoded.result)
              {
                // if this is the current switch's default: then print it
                ret += indent.substr(0, indent.size() - 2);
                ret += StringFormat::Fmt("case %llu:\n", caseTarget.literal);
                lineNum++;
                break;
              }
            }
          }

          // print the label if we decided it was needed
          if(printLabels.find(decoded.result) != printLabels.end())
          {
            ret += idName(decoded.result) + ":\n";
            lineNum++;
          }

          // if this is a loop header, begin the loop here
          if(loopBlocks.find(decoded.result) != loopBlocks.end())
          {
            // increment any previous instructions that were pointing at this line, to point at the
            // next one.
            for(auto lineIt = instructionLines.end(); lineIt != instructionLines.begin();)
            {
              --lineIt;
              if(lineIt->second == lineNum)
              {
                lineIt->second++;
                continue;
              }
              break;
            }

            ret += "\n";
            lineNum++;

            ret += indent + "while(true) {\n";
            lineNum++;
            indent += "  ";
          }

          continue;
        }
        case Op::Branch:
        {
          OpBranch decoded(it);

          ConstIter nextit = it;
          nextit++;
          while(nextit.opcode() == Op::Line || nextit.opcode() == Op::NoLine)
            nextit++;

          // next opcode *must* be a label because this is the end of a block
          RDCASSERTEQUAL(nextit.opcode(), Op::Label);
          OpLabel decodedlabel(nextit);

          // always skip redundant gotos
          if(decodedlabel.result == decoded.targetLabel)
          {
            // however if we're in a switch we might want to print a clarifying fallthrough comment
            // or end-of-case break

            if(!cfgStack.empty() && cfgStack.back().type == StructuredCFG::Switch)
            {
              // add a break even for the final branch to the merge block
              if(cfgStack.back().mergeTarget == decoded.targetLabel)
              {
                ret += indent + "break;\n";
                lineNum++;
                continue;
              }

              // if we're falling through to the next case, print a comment
              for(const SwitchPairU64LiteralId &caseTarget : cfgStack.back().caseTargets)
              {
                if(caseTarget.target == decoded.targetLabel)
                {
                  ret += indent;
                  ret += StringFormat::Fmt("// deliberate fallthrough to case %llu\n",
                                           caseTarget.literal);
                  lineNum++;
                  break;
                }
              }
            }

            continue;
          }

          // if we're in a loop, skip branches from the continue block to the loop header
          if(!cfgStack.empty() && cfgStack.back().type == StructuredCFG::Loop &&
             cfgStack.back().continueTarget == currentBlock &&
             cfgStack.back().headerBlock == decoded.targetLabel)
            continue;

          // if we're in an if, skip branches to the merge block if the next block is the 'else'
          if(!cfgStack.empty() && cfgStack.back().type == StructuredCFG::If &&
             cfgStack.back().elseTarget == decodedlabel.result &&
             cfgStack.back().mergeTarget == decoded.targetLabel)
            continue;

          const StructuredCFG *lastLoopSwitch = NULL;

          // walk stack to get the last switch/loop that we're in
          for(size_t s = 0; s < cfgStack.size(); s++)
          {
            const StructuredCFG &cfg = cfgStack[cfgStack.size() - 1 - s];
            if(cfg.type == StructuredCFG::Switch || cfg.type == StructuredCFG::Loop)
            {
              lastLoopSwitch = &cfg;
              break;
            }
          }

          // if we're in a switch/loop, branches to the merge block are printed as 'break'
          if(lastLoopSwitch && lastLoopSwitch->mergeTarget == decoded.targetLabel)
          {
            ret += indent + "break;\n";
            lineNum++;
            continue;
          }

          // if we're in a loop, branches to the continue target are printed as 'continue'
          if(lastLoopSwitch && lastLoopSwitch->type == StructuredCFG::Loop &&
             lastLoopSwitch->continueTarget == decoded.targetLabel)
          {
            ret += indent + "continue;\n";
            lineNum++;
            continue;
          }

          // if we're in a switch and we're about to print a goto, see if it's a case label and
          // print a 'nicer' goto. Fallthrough to the next case would be handled above as a
          // redundant goto
          if(lastLoopSwitch && lastLoopSwitch->type == StructuredCFG::Switch)
          {
            bool printed = false;

            for(const SwitchPairU64LiteralId &caseTarget : lastLoopSwitch->caseTargets)
            {
              if(caseTarget.target == decoded.targetLabel)
              {
                ret += StringFormat::Fmt("goto case %llu;\n", caseTarget.literal);
                lineNum++;
                printed = true;
                break;
              }
            }

            if(printed)
              continue;
          }

          ret += indent;
          ret += "goto " + idName(decoded.targetLabel) + ";\n";
          lineNum++;

          // we must print this label because we've created a goto for it
          printLabels.insert(decoded.targetLabel);

          continue;
        }
        case Op::BranchConditional:
        {
          OpBranchConditional decoded(it);

          ConstIter nextit = it;
          nextit++;

          // if we got here we're a simple if() without an else - SelectionMerge/LoopMerge
          // consumes any branch.
          //
          // we must be careful because this kind of branch can conditionally branch out of a loop,
          // so we need to look to see if either 'other' branch is to the continue or merge blocks
          // of the current loop

          // next opcode *must* be a label because this is the end of a block
          RDCASSERTEQUAL(nextit.opcode(), Op::Label);
          OpLabel decodedlabel(nextit);

          if(decoded.trueLabel == decodedlabel.result || decoded.falseLabel == decodedlabel.result)
          {
            Id otherLabel =
                (decoded.trueLabel == decodedlabel.result) ? decoded.falseLabel : decoded.trueLabel;

            if(!cfgStack.empty() && cfgStack.back().type == StructuredCFG::Loop &&
               (otherLabel == cfgStack.back().mergeTarget ||
                otherLabel == cfgStack.back().continueTarget))
            {
              const char *negate = (decoded.falseLabel == cfgStack.back().mergeTarget) ? "!" : "";

              ret += indent;

              if(otherLabel == cfgStack.back().mergeTarget)
              {
                ret +=
                    StringFormat::Fmt("if(%s%s) break;", negate, idName(decoded.condition).c_str());
              }
              else
              {
                if(strings[otherLabel].empty())
                  dynamicNames[otherLabel] = StringFormat::Fmt("_continue%u", otherLabel.value());
                ret += StringFormat::Fmt("if(%s%s) goto %s;", negate,
                                         idName(decoded.condition).c_str(),
                                         idName(otherLabel).c_str());

                printLabels.insert(otherLabel);
              }

              if(decoded.branchweights.size() == 2)
                ret += StringFormat::Fmt(" [[true: %u, false: %u]]", decoded.branchweights[0],
                                         decoded.branchweights[1]);

              ret += "\n";
              lineNum++;
            }
            else
            {
              const char *negate = (decoded.falseLabel == decodedlabel.result) ? "!" : "";

              ret += indent;
              ret += StringFormat::Fmt("if(%s%s) {", negate, idName(decoded.condition).c_str());

              if(decoded.branchweights.size() == 2)
                ret += StringFormat::Fmt(" [[true: %u, false: %u]]", decoded.branchweights[0],
                                         decoded.branchweights[1]);

              ret += "\n";
              lineNum++;

              // this isn't technically structured but it's easier to pretend that it is
              // no else, the merge target is where we exit the 'if'
              StructuredCFG cfg = {StructuredCFG::If};
              cfg.headerBlock = currentBlock;
              cfg.mergeTarget = otherLabel;
              cfg.elseTarget = rdcspv::Id();
              cfgStack.push_back(cfg);

              indent += "  ";
            }
          }
          else
          {
            RDCWARN(
                "Unexpected SPIR-V formulation - OpBranchConditional not followed by either true "
                "or false block");

            // this is legal. We just have to emit an if with gotos
            ret += indent;
            ret += StringFormat::Fmt(
                "if(%s) { goto %s; } else { goto %s; }\n", idName(decoded.condition).c_str(),
                idName(decoded.trueLabel).c_str(), idName(decoded.falseLabel).c_str());
            lineNum++;

            // need to print these labels now
            printLabels.insert(decoded.trueLabel);
            printLabels.insert(decoded.falseLabel);
          }

          continue;
        }

        // since we're eliding a lot of labels, simplify display of OpPhi
        case Op::Phi:
        {
          OpPhi decoded(it);
          ret += indent;
          ret += declName(decoded.resultType, decoded.result);
          ret += " = Phi(";
          for(size_t i = 0; i < decoded.parents.size(); i++)
          {
            ret += idName(decoded.parents[i].first);
            if(i + 1 < decoded.parents.size())
              ret += ", ";
          }
          ret += ")";
          break;
        }

          ////////////////////////////////////////////////////////////////////////////////////////
          // pretty printing unary instructions
          ////////////////////////////////////////////////////////////////////////////////////////

        case Op::SNegate:
        case Op::FNegate:
        case Op::Not:
        case Op::LogicalNot:
        {
          rdcstr opstr;
          switch(it.opcode())
          {
            case Op::SNegate:
            case Op::FNegate: opstr = "-"; break;
            case Op::Not: opstr = "~"; break;
            case Op::LogicalNot: opstr = "!"; break;
            default: break;
          }

          // all of these operations share the same encoding
          OpNot decoded(it);

          ret += indent;
          ret += declName(decoded.resultType, decoded.result);
          ret += " = ";
          ret += opstr;
          ret += idName(decoded.operand);
          break;
        }

          ////////////////////////////////////////////////////////////////////////////////////////
          // pretty printing binary instructions
          ////////////////////////////////////////////////////////////////////////////////////////

        case Op::IAdd:
        case Op::FAdd:
        case Op::ISub:
        case Op::FSub:
        case Op::IMul:
        case Op::FMul:
        case Op::UDiv:
        case Op::SDiv:
        case Op::FDiv:
        case Op::VectorTimesMatrix:
        case Op::VectorTimesScalar:
        case Op::MatrixTimesMatrix:
        case Op::MatrixTimesVector:
        case Op::MatrixTimesScalar:
        case Op::ShiftLeftLogical:
        case Op::BitwiseAnd:
        case Op::BitwiseOr:
        case Op::BitwiseXor:
        case Op::LogicalEqual:
        case Op::LogicalNotEqual:
        case Op::LogicalOr:
        case Op::LogicalAnd:
        case Op::IEqual:
        case Op::INotEqual:
        case Op::UGreaterThan:
        case Op::SGreaterThan:
        case Op::UGreaterThanEqual:
        case Op::SGreaterThanEqual:
        case Op::ULessThan:
        case Op::SLessThan:
        case Op::ULessThanEqual:
        case Op::SLessThanEqual:
        case Op::FOrdEqual:
        case Op::FUnordEqual:
        case Op::FOrdNotEqual:
        case Op::FUnordNotEqual:
        case Op::FOrdGreaterThan:
        case Op::FUnordGreaterThan:
        case Op::FOrdGreaterThanEqual:
        case Op::FUnordGreaterThanEqual:
        case Op::FOrdLessThan:
        case Op::FUnordLessThan:
        case Op::FOrdLessThanEqual:
        case Op::FUnordLessThanEqual:
        {
          // all of these operations share the same encoding
          OpIMul decoded(it);

          ret += indent;
          ret += declName(decoded.resultType, decoded.result);
          ret += " = ";
          ret += StringiseBinaryOperation(idName, it.opcode(), decoded.operand1, decoded.operand2);
          break;
        }

        // right shifts must be done as a particular type
        case Op::ShiftRightLogical:
        case Op::ShiftRightArithmetic:
        {
          // these operations share the same encoding
          OpShiftRightLogical decoded(it);

          bool signedOp = (it.opcode() == Op::ShiftRightArithmetic);

          ret += indent;
          ret += declName(decoded.resultType, decoded.result);
          ret += " = ";

          if(signedOp != dataTypes[idTypes[decoded.base]].scalar().signedness)
          {
            ret += signedOp ? "signed(" : "unsigned(";
            ret += idName(decoded.base);
            ret += ")";
          }
          else
          {
            ret += idName(decoded.base);
          }

          ret += " >> ";

          if(signedOp != dataTypes[idTypes[decoded.shift]].scalar().signedness)
          {
            ret += signedOp ? "signed(" : "unsigned(";
            ret += idName(decoded.shift);
            ret += ")";
          }
          else
          {
            ret += idName(decoded.shift);
          }

          break;
        }

        ////////////////////////////////////////////////////////////////////////////////////////
        // pretty printing misc instructions
        ////////////////////////////////////////////////////////////////////////////////////////

        // write loads and stores as assignment via pointer
        case Op::Load:
        {
          OpLoad decoded(it);
          ret += indent;
          ret += declName(decoded.resultType, decoded.result);
          ret += " = *" + idName(decoded.pointer);
          if(decoded.memoryAccess != MemoryAccess::None)
            ret += " [" + ParamToStr(idName, decoded.memoryAccess) + "]";
          ret += getDecorationString(decorations[decoded.result]);
          break;
        }
        case Op::Store:
        {
          OpStore decoded(it);
          ret += indent;
          ret += StringFormat::Fmt("*%s = %s", idName(decoded.pointer).c_str(),
                                   idName(decoded.object).c_str());
          if(decoded.memoryAccess != MemoryAccess::None)
            ret += " [" + ParamToStr(idName, decoded.memoryAccess) + "]";
          break;
        }

        // returns as a conventional return
        case Op::Return:
        {
          ret += indent + "return";
          break;
        }
        case Op::ReturnValue:
        {
          OpReturnValue decoded(it);
          ret += indent + StringFormat::Fmt("return %s", idName(decoded.value).c_str());
          break;
        }

        case Op::FunctionCall:
        {
          OpFunctionCall decoded(it);
          ret += indent;

          if(dataTypes[decoded.resultType].scalar().type != Op::TypeVoid)
          {
            ret += declName(decoded.resultType, decoded.result);
            ret += " = ";
          }

          rdcstr name = idName(decoded.function);

          // glslang outputs encoded type information in the OpName of functions, strip it
          {
            int32_t offs = name.indexOf('(');
            if(offs > 0)
              name.erase(offs, name.size() - offs);
          }

          ret += name + "(";
          for(size_t i = 0; i < decoded.arguments.size(); i++)
          {
            ret += idName(decoded.arguments[i]);
            if(i + 1 < decoded.arguments.size())
              ret += ", ";
          }
          ret += ")";

          break;
        }

        // decode OpCompositeExtract and OpAccesschain as best as possible
        case Op::CompositeExtract:
        {
          OpCompositeExtract decoded(it);
          ret += indent;
          ret += declName(decoded.resultType, decoded.result);
          ret += " = " + idName(decoded.composite);

          const DataType *type = &dataTypes[idTypes[decoded.composite]];
          for(size_t i = 0; i < decoded.indexes.size(); i++)
          {
            uint32_t idx = decoded.indexes[i];
            ret += accessor(type, idx, rdcstr());

            if(type->type == DataType::ArrayType || type->type == DataType::MatrixType ||
               type->type == DataType::VectorType)
            {
              type = &dataTypes[type->InnerType()];
            }
            else if(type->type == DataType::StructType)
            {
              RDCASSERT(idx < type->children.size());
              type = &dataTypes[type->children[idx].type];
            }
          }

          ret += getDecorationString(decorations[decoded.result]);
          break;
        }
        case Op::AccessChain:
        case Op::InBoundsAccessChain:
        {
          OpAccessChain decoded(it);
          ret += indent;
          ret += declName(decoded.resultType, decoded.result);
          ret += " = &" + idName(decoded.base);

          const DataType *type = &dataTypes[idTypes[decoded.base]];

          // base should be a pointer
          RDCASSERT(type->type == DataType::PointerType);
          type = &dataTypes[type->InnerType()];

          size_t i = 0;
          for(; i < decoded.indexes.size(); i++)
          {
            int32_t idx = -1;

            // if it's a non-specialised constant, get its value
            if(constants.find(decoded.indexes[i]) != constants.end() &&
               specConstants.find(decoded.indexes[i]) == specConstants.end())
              idx = EvaluateConstant(decoded.indexes[i], {}).value.s32v[0];

            // if it's a struct we must have an OpConstant to use, if it's a vector and we did get a
            // constant then do better than a basic array index syntax.
            if(type->type == DataType::StructType || (type->type == DataType::VectorType && idx >= 0))
            {
              // index must be an OpConstant when indexing into a structure
              RDCASSERT(idx >= 0);
              ret += accessor(type, idx, rdcstr());
            }
            else
            {
              RDCASSERT(type->type == DataType::ArrayType || type->type == DataType::MatrixType ||
                            type->type == DataType::VectorType,
                        (uint32_t)type->type);
              ret += StringFormat::Fmt("[%s]", idName(decoded.indexes[i]).c_str());
            }

            if(type->type == DataType::ArrayType || type->type == DataType::MatrixType ||
               type->type == DataType::VectorType)
            {
              type = &dataTypes[type->InnerType()];
            }
            else if(type->type == DataType::StructType)
            {
              RDCASSERT((size_t)idx < type->children.size());
              type = &dataTypes[type->children[idx].type];
            }
          }

          ret += getDecorationString(decorations[decoded.result]);
          break;
        }

        // handle vector shuffle
        case Op::VectorShuffle:
        {
          OpVectorShuffle decoded(it);
          ret += indent;
          ret += StringFormat::Fmt("%s = ", declName(decoded.resultType, decoded.result).c_str());

          // it's common to only swizzle from the first vector, detect that case
          bool allFirst = true;

          uint32_t vec1Cols = dataTypes[idTypes[decoded.vector1]].vector().count;

          for(uint32_t c : decoded.components)
            if(c >= vec1Cols)
              allFirst = false;

          const char comps[] = "xyzw";

          if(allFirst)
          {
            ret += idName(decoded.vector1) + ".";

            for(uint32_t c : decoded.components)
              ret.push_back(comps[c]);
          }
          else
          {
            ret += declName(decoded.resultType, Id()) + "(";
            for(size_t i = 0; i < decoded.components.size(); i++)
            {
              uint32_t c = decoded.components[i];

              ret += idName(c < vec1Cols ? decoded.vector1 : decoded.vector2) + ".";
              ret.push_back(comps[c % vec1Cols]);

              if(i + 1 < decoded.components.size())
                ret += ", ";
            }
            ret += ")";
          }

          break;
        }

        case Op::ExtInst:
        case Op::ExtInstWithForwardRefsKHR:
        {
          OpExtInst decoded(it);

          rdcstr setname = extSets.find(decoded.set)->second;
          uint32_t inst = it.word(4);

          const bool IsGLSL450 = knownExtSet[ExtSet_GLSL450] == decoded.set;
          const bool IsDebugPrintf = knownExtSet[ExtSet_Printf] == decoded.set;
          const bool IsShaderDbg = knownExtSet[ExtSet_ShaderDbg] == decoded.set;
          // GLSL.std.450 all parameters are Ids
          const bool idParams = IsGLSL450 || setname.beginsWith("NonSemantic.");

          // most vulkan debug info instructions don't get printed explicitly, and those that do
          // have no return value that we print
          if(IsShaderDbg)
          {
            OpShaderDbg dbg(it);

            if(dbg.inst == ShaderDbg::Source)
            {
              dynamicNames[dbg.result] = idName(dbg.arg<Id>(0));
              continue;
            }
            else if(dbg.inst == ShaderDbg::CompilationUnit)
            {
              uint32_t lang = EvaluateConstant(dbg.arg<Id>(3), {}).value.u32v[0];
              ret += indent;
              ret += "DebugCompilationUnit(";
              ret += idName(dbg.arg<Id>(2));
              ret += ", ";
              ret += ToStr(rdcspv::SourceLanguage(lang));
              ret += ")";
            }
            else if(dbg.inst == ShaderDbg::EntryPoint)
            {
              ret += indent;
              ret += "DebugEntryPoint(";
              OpShaderDbg debugFunc(GetID(dbg.arg<Id>(0)));
              ret += idName(debugFunc.arg<Id>(0));
              ret += ", ";
              rdcstr gen = ToStr(m_Generator);
              int i = gen.indexOf('-');
              if(i > 0)
                gen.erase(i - 1, ~0u);
              ret += gen;
              ret += " ";
              ret += idName(dbg.arg<Id>(2));
              rdcstr args = idName(dbg.arg<Id>(3));
              if(!args.empty())
              {
                ret += ",\n";
                lineNum++;
                ret += indent + "    command line: ";
                ret += args;
              }

              ret += ")";
            }
            else if(dbg.inst == ShaderDbg::Value)
            {
              OpShaderDbg localVar(GetID(dbg.arg<Id>(0)));
              ret += indent;
              ret += "// DebugValue(";
              ret += idName(dbg.arg<Id>(1));
              ret += " is ";
              ret += idName(localVar.arg<Id>(0));
              ret += " @ ";
              ret += idName(localVar.arg<Id>(2));
              ret += ":";
              ret += idName(localVar.arg<Id>(3));

              if(dbg.params.size() > 3)
                ret += " (subset)";
              ret += ")";
            }
            else
            {
              continue;
            }
          }
          else
          {
            ret += indent;
            ret += StringFormat::Fmt("%s = ", declName(decoded.resultType, decoded.result).c_str());

            if(IsGLSL450)
              ret += StringFormat::Fmt("%s::%s(", setname.c_str(), ToStr(GLSLstd450(inst)).c_str());
            else if(IsDebugPrintf)
              ret += "DebugPrintf(";
            else
              ret += StringFormat::Fmt("%s::[%u](", setname.c_str(), inst);

            for(uint32_t i = 0; i < decoded.params.size(); i++)
            {
              if(i == 0 && IsDebugPrintf)
                ret += "\"";

              // TODO could generate this from the instruction set grammar.
              ret += idParams ? idName(decoded.arg<Id>(i)) : ToStr(decoded.arg<uint32_t>(i));

              if(i == 0 && IsDebugPrintf)
                ret += "\"";

              if(i + 1 < decoded.params.size())
                ret += ", ";
            }

            ret += ")";
          }
          break;
        }

        default:
        {
          ret += indent;
          ret += OpDecoder::Disassemble(it, declName, idName, constIntVal);

          OpDecoder decoded(it);

          if(decoded.result != Id())
            ret += getDecorationString(decorations[decoded.result]);
        }
      }

      ret += ";\n";
      lineNum++;
    }

    ret += "\n";
    lineNum++;
  }

  return ret;
}

rdcstr Reflector::StringiseConstant(rdcspv::Id id) const
{
  // only stringise constants
  auto cit = constants.find(id);
  if(cit == constants.end())
    return rdcstr();

  // don't stringise spec constants either
  if(specConstants.find(id) != specConstants.end())
    return rdcstr();

  // print NULL or Undef values specially
  if(cit->second.op == Op::ConstantNull || cit->second.op == Op::Undef)
  {
    rdcstr ret = dataTypes[cit->second.type].name;
    if(ret.empty())
      ret = StringFormat::Fmt("type%u", cit->second.type.value());

    if(cit->second.op == Op::ConstantNull)
      ret += "(Null)";
    else if(cit->second.op == Op::Undef)
      ret += "(Undef)";
    return ret;
  }

  const DataType &type = dataTypes[cit->second.type];
  const ShaderVariable &value = cit->second.value;

  if(type.type == DataType::ScalarType)
  {
    if(type.scalar().type == Op::TypeBool)
      return value.value.u32v[0] ? "true" : "false";

    switch(value.type)
    {
      case VarType::Half: return ToStr(value.value.f16v[0]);
      case VarType::Float: return ToStr(value.value.f32v[0]);
      case VarType::Double: return ToStr(value.value.f64v[0]);
      case VarType::SInt: return ToStr(value.value.s32v[0]);
      case VarType::SShort: return ToStr(value.value.s16v[0]);
      case VarType::SByte: return ToStr(value.value.s8v[0]);
      case VarType::Bool: return value.value.u32v[0] ? "true" : "false";
      case VarType::UInt: return ToStr(value.value.u32v[0]);
      case VarType::UShort: return ToStr(value.value.u16v[0]);
      case VarType::UByte: return ToStr(value.value.u8v[0]);
      case VarType::SLong: return ToStr(value.value.s64v[0]);
      case VarType::ULong: return ToStr(value.value.u64v[0]);
      case VarType::Enum:
      case VarType::Struct:
      case VarType::Unknown:
      case VarType::GPUPointer:
      case VarType::ConstantBlock:
      case VarType::ReadOnlyResource:
      case VarType::ReadWriteResource:
      case VarType::Sampler: return "???";
    }
  }
  else if(type.type == DataType::VectorType)
  {
    rdcstr ret = "{";
    for(size_t i = 0; i < value.columns; i++)
    {
      switch(value.type)
      {
        case VarType::Half: ret += ToStr(value.value.f16v[i]); break;
        case VarType::Float: ret += ToStr(value.value.f32v[i]); break;
        case VarType::Double: ret += ToStr(value.value.f64v[i]); break;
        case VarType::SInt: ret += ToStr(value.value.s32v[i]); break;
        case VarType::SShort: ret += ToStr(value.value.s16v[i]); break;
        case VarType::SByte: ret += ToStr(value.value.s8v[i]); break;
        case VarType::Bool: ret += value.value.u32v[i] ? "true" : "false"; break;
        case VarType::UInt: ret += ToStr(value.value.u32v[i]); break;
        case VarType::UShort: ret += ToStr(value.value.u16v[i]); break;
        case VarType::UByte: ret += ToStr(value.value.u8v[i]); break;
        case VarType::SLong: ret += ToStr(value.value.s64v[i]); break;
        case VarType::ULong: ret += ToStr(value.value.u64v[i]); break;
        case VarType::Enum:
        case VarType::Struct:
        case VarType::Unknown:
        case VarType::GPUPointer:
        case VarType::ConstantBlock:
        case VarType::ReadOnlyResource:
        case VarType::ReadWriteResource:
        case VarType::Sampler: ret += "???"; break;
      }
      if(i + 1 < value.columns)
        ret += ", ";
    }
    ret += "}";
    return ret;
  }

  return rdcstr();
}

};    // namespace rdcspv
