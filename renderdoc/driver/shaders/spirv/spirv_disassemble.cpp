/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015 Baldur Karlsson
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

#include "common/common.h"
#include "maths/formatpacking.h"

#include "serialise/serialiser.h"

#include "spirv_common.h"

#include <utility>
#include <algorithm>
using std::pair;
using std::make_pair;

#undef min
#undef max

#include "3rdparty/glslang/SPIRV/spirv.hpp"
#include "3rdparty/glslang/SPIRV/GLSL.std.450.h"
#include "3rdparty/glslang/glslang/Public/ShaderLang.h"

const char *GLSL_STD_450_names[GLSLstd450Count] = {0};

// list of known generators, just for kicks
struct { uint32_t magic; const char *name; } KnownGenerators[] = {
	{ 0x051a00bb, "glslang" },
};

template<typename EnumType>
static string OptionalFlagString(EnumType e)
{
	return (int)e ? " [" + ToStr::Get(e) + "]" : "";
}

static string DefaultIDName(uint32_t ID)
{
	return StringFormat::Fmt("{%u}", ID);
}

template<typename T>
static bool erase_item(std::vector<T> &vec, const T& elem)
{
	auto it = std::find(vec.begin(), vec.end(), elem);
	if(it != vec.end())
	{
		vec.erase(it);
		return true;
	}

	return false;
}

struct SPVInstruction;

struct SPVDecoration
{
	SPVDecoration() : decoration(spv::DecorationRelaxedPrecision), val(0) {}

	spv::Decoration decoration;

	uint32_t val;
};

struct SPVExtInstSet
{
	SPVExtInstSet() : instructions(NULL) {}

	string setname;
	const char **instructions;
};

struct SPVEntryPoint
{
	SPVEntryPoint() : func(0), model(spv::ExecutionModelVertex) {}

	// entry point will come before declaring instruction,
	// so we reference the function by ID
	uint32_t func;
	spv::ExecutionModel model;
};

struct SPVTypeData
{
	SPVTypeData() :
		baseType(NULL), storage(spv::StorageClassUniformConstant),
		bitCount(32), vectorSize(1), matrixSize(1), arraySize(1) {}

	enum
	{
		eVoid,
		eBool,
		eFloat,
		eSInt,
		eUInt,
		eBasicCount,

		eVector,
		eMatrix,
		eArray,
		ePointer,
		eCompositeCount,

		eFunction,

		eStruct,
		eSampler,
		eFilter,

		eTypeCount,
	} type;

	SPVTypeData *baseType;

	string name;

	bool IsBasicInt() const
	{
		return type == eUInt || type == eSInt;
	}

	string DeclareVariable(const string &varName)
	{
		if(type == eArray)
			return StringFormat::Fmt("%s %s[%u]", baseType->GetName().c_str(), varName.c_str(), arraySize);

		return StringFormat::Fmt("%s %s", GetName().c_str(), varName.c_str());
	}

	const string &GetName()
	{
		if(name.empty())
		{
			if(type == eVoid)
			{
				name = "void";
			}
			else if(type == eBool)
			{
				name = "bool";
			}
			else if(type == eFloat)
			{
				RDCASSERT(bitCount == 64 || bitCount == 32 || bitCount == 16);
				name =    bitCount == 64 ? "double"
					: bitCount == 32 ? "float"
					: "half";
			}
			else if(type == eSInt)
			{
				RDCASSERT(bitCount == 64 || bitCount == 32 || bitCount == 16 || bitCount == 8);
				name =    bitCount == 64 ? "long"
					: bitCount == 32 ? "int"
					: bitCount == 16 ? "short"
					: "byte";
			}
			else if(type == eUInt)
			{
				RDCASSERT(bitCount == 64 || bitCount == 32 || bitCount == 16 || bitCount == 8);
				name =    bitCount == 64 ? "ulong"
					: bitCount == 32 ? "uint"
					: bitCount == 16 ? "ushort"
					: "ubyte";
			}
			else if(type == eVector)
			{
				name = StringFormat::Fmt("%s%u", baseType->GetName().c_str(), vectorSize);
			}
			else if(type == eMatrix)
			{
				name = StringFormat::Fmt("%s%ux%u", baseType->GetName().c_str(), vectorSize, matrixSize);
			}
			else if(type == ePointer)
			{
				name = StringFormat::Fmt("%s*", baseType->GetName().c_str());
			}
		}

		return name;
	}

	vector<SPVDecoration> decorations;

	// struct/function
	vector< pair<SPVTypeData *, string> > children;

	// pointer
	spv::StorageClass storage;

	// ints and floats
	uint32_t bitCount;

	uint32_t vectorSize;
	uint32_t matrixSize;
	uint32_t arraySize;
};

struct SPVOperation
{
	SPVOperation() : type(NULL), complexity(0), inlineArgs(0) {}

	SPVTypeData *type;

	// OpLoad/OpStore/OpCopyMemory
	spv::MemoryAccessMask access;

	// OpExtInst
	vector<uint32_t> literals;

	// OpFunctionCall
	uint32_t funcCall;

	// this is modified on the fly, it's used as a measure of whether we
	// can combine multiple statements into one line when displaying the
	// disassembly.
	int complexity;

	// bitfield indicating which arguments should be inlined
	uint32_t inlineArgs;

	// arguments always reference IDs that already exist (branch/flow
	// control type statements aren't SPVOperations)
	vector<SPVInstruction*> arguments;

	void GetArg(const vector<SPVInstruction *> &ids, size_t idx, string &arg);
};

struct SPVConstant
{
	SPVConstant() : type(NULL), u64(0) {}

	SPVTypeData *type;
	union
	{
		uint64_t u64;
		uint32_t u32;
		uint16_t u16;
		uint8_t u8;
		int64_t i64;
		int32_t i32;
		int16_t i16;
		int8_t i8;
		float f;
		double d;
	};

	vector<SPVConstant *> children;

	string GetValString()
	{
		RDCASSERT(children.empty());

		if(type->type == SPVTypeData::eFloat)
		{
			if(type->bitCount == 64)
				return StringFormat::Fmt("%lf", d);
			if(type->bitCount == 32)
				return StringFormat::Fmt("%f", f);
			if(type->bitCount == 16)
				return StringFormat::Fmt("%f", ConvertFromHalf(u16));
		}
		else if(type->type == SPVTypeData::eSInt)
		{
			if(type->bitCount == 64)
				return StringFormat::Fmt("%lli", i64);
			if(type->bitCount == 32)
				return StringFormat::Fmt("%i", i32);
			if(type->bitCount == 16)
				return StringFormat::Fmt("%hi", i16);
			if(type->bitCount == 8)
				return StringFormat::Fmt("%hhi", i8);
		}
		else if(type->type == SPVTypeData::eSInt)
		{
			if(type->bitCount == 64)
				return StringFormat::Fmt("%llu", u64);
			if(type->bitCount == 32)
				return StringFormat::Fmt("%u", u32);
			if(type->bitCount == 16)
				return StringFormat::Fmt("%hu", u16);
			if(type->bitCount == 8)
				return StringFormat::Fmt("%hhu", u8);
		}
		else if(type->type == SPVTypeData::eBool)
			return u32 ? "true" : "false";

		return StringFormat::Fmt("!%u!", u32);
	}

	string GetIDName()
	{
		string ret = type->GetName();
		ret += "(";
		if(children.empty())
			ret += GetValString();
		for(size_t i=0; i < children.size(); i++)
		{
			ret += children[i]->GetValString();
			if(i+1 < children.size())
				ret += ", ";
		}
		ret += ")";

		return ret;
	}
};

struct SPVVariable
{
	SPVVariable() : type(NULL), storage(spv::StorageClassUniformConstant), initialiser(NULL) {}

	SPVTypeData *type;
	spv::StorageClass storage;
	SPVConstant *initialiser;
};

struct SPVFlowControl
{
	SPVFlowControl() : selControl(spv::SelectionControlMaskNone), condition(NULL) {}

	union
	{
		spv::SelectionControlMask selControl;
		spv::LoopControlMask loopControl;
	};
	
	SPVInstruction *condition;

	// branch weights or switch cases
	vector<uint32_t> literals;

	// flow control can reference future IDs, so we index
	vector<uint32_t> targets;
};

struct SPVBlock
{
	SPVBlock() : mergeFlow(NULL), exitFlow(NULL) {}
	
	vector<SPVInstruction *> instructions;

	SPVInstruction *mergeFlow;
	SPVInstruction *exitFlow;
};

struct SPVFunction
{
	SPVFunction() : retType(NULL), funcType(NULL), control(spv::FunctionControlMaskNone) {}

	SPVTypeData *retType;
	SPVTypeData *funcType;
	vector<SPVInstruction *> arguments;

	spv::FunctionControlMask control;

	vector<SPVInstruction *> blocks;
	vector<SPVInstruction *> variables;
};

struct SPVInstruction
{
	SPVInstruction()
	{
		opcode = spv::OpNop;
		id = 0;

		ext = NULL;
		entry = NULL;
		op = NULL;
		flow = NULL;
		type = NULL;
		func = NULL;
		block = NULL;
		constant = NULL;
		var = NULL;

		line = -1;

		source.col = source.line = 0;
	}

	~SPVInstruction()
	{
		SAFE_DELETE(ext);
		SAFE_DELETE(entry);
		SAFE_DELETE(op);
		SAFE_DELETE(flow);
		SAFE_DELETE(type);
		SAFE_DELETE(func);
		SAFE_DELETE(block);
		SAFE_DELETE(constant);
		SAFE_DELETE(var);
	}

	spv::Op opcode;
	uint32_t id;

	// line number in disassembly (used for stepping when debugging)
	int line;

	struct { string filename; uint32_t line; uint32_t col; } source;

	string str;

	const string &GetIDName()
	{
		if(str.empty())
		{
			if(constant)
				str = constant->GetIDName();
			else
				str = DefaultIDName(id);
		}

		return str;
	}

	string Disassemble(const vector<SPVInstruction *> &ids, bool inlineOp)
	{
		switch(opcode)
		{
			case spv::OpConstant:
			case spv::OpConstantComposite:
			case spv::OpVariable:
			case spv::OpFunctionParameter:
			{
				return GetIDName();
			}
			case spv::OpLabel:
			{
				RDCASSERT(!inlineOp);
				return StringFormat::Fmt("Label%u:", id);
			}
			case spv::OpReturn:
			{
				RDCASSERT(!inlineOp);
				return "Return";
			}
			case spv::OpReturnValue:
			{
				RDCASSERT(!inlineOp);

				string arg = ids[flow->targets[0]]->Disassemble(ids, true);

				return StringFormat::Fmt("Return %s", arg.c_str());
			}
			case spv::OpBranch:
			{
				RDCASSERT(!inlineOp);
				return StringFormat::Fmt("goto Label%u", flow->targets[0]);
			}
			case spv::OpBranchConditional:
			{
				RDCASSERT(!inlineOp);

				// we don't output the targets since that is handled specially

				if(flow->literals.empty())
					return flow->condition->Disassemble(ids, true);

				uint32_t weightA = flow->literals[0];
				uint32_t weightB = flow->literals[1];

				float a = float(weightA)/float(weightA+weightB);
				float b = float(weightB)/float(weightA+weightB);

				a *= 100.0f;
				b *= 100.0f;

				return StringFormat::Fmt("%s [true: %.2f%%, false: %.2f%%]", flow->condition->Disassemble(ids, true).c_str(), a, b);
			}
			case spv::OpSelectionMerge:
			{
				RDCASSERT(!inlineOp);
				return StringFormat::Fmt("SelectionMerge Label%u%s", flow->targets[0], OptionalFlagString(flow->selControl).c_str());
			}
			case spv::OpLoopMerge:
			{
				RDCASSERT(!inlineOp);
				return StringFormat::Fmt("LoopMerge Label%u%s", flow->targets[0], OptionalFlagString(flow->loopControl).c_str());
			}
			case spv::OpStore:
			{
				RDCASSERT(op);

				string arg;
				op->GetArg(ids, 1, arg);

				// inlined only in function parameters, just return argument
				if(inlineOp)
					return arg;

				return StringFormat::Fmt("Store(%s%s) = %s", op->arguments[0]->GetIDName().c_str(), OptionalFlagString(op->access).c_str(), arg.c_str());
			}
			case spv::OpCopyMemory:
			{
				RDCASSERT(!inlineOp && op);

				string arg;
				op->GetArg(ids, 1, arg);

				return StringFormat::Fmt("Copy(%s%s) = Load(%s%s)", op->arguments[0]->GetIDName().c_str(), OptionalFlagString(op->access).c_str(), arg.c_str(), OptionalFlagString(op->access).c_str());
			}
			case spv::OpLoad:
			{
				RDCASSERT(op);

				string arg;
				op->GetArg(ids, 0, arg);

				if(inlineOp)
					return StringFormat::Fmt("Load(%s%s)", arg.c_str(), OptionalFlagString(op->access).c_str());

				return StringFormat::Fmt("%s %s = Load(%s%s)", op->type->GetName().c_str(), GetIDName().c_str(), arg.c_str(), OptionalFlagString(op->access).c_str());
			}
			case spv::OpAccessChain:
			{
				RDCASSERT(op);

				string ret = "";

				if(!inlineOp)
					ret = StringFormat::Fmt("%s %s = ", op->type->GetName().c_str(), GetIDName().c_str());

				ret += op->arguments[0]->GetIDName();

				SPVTypeData *type = op->arguments[0]->var->type;
				if(type->type == SPVTypeData::ePointer)
					type = type->baseType;

				for(size_t i=1; i < op->arguments.size(); i++)
				{
					int32_t idx = -1;
					if(op->arguments[i]->constant)
					{
						RDCASSERT(op->arguments[i]->constant && op->arguments[i]->constant->type->IsBasicInt());
						idx = op->arguments[i]->constant->i32;
					}
					RDCASSERT(type);
					if(!type)
						break;

					if(type->type == SPVTypeData::eStruct)
					{
						// Assuming you can't dynamically index into a structure
						RDCASSERT(op->arguments[i]->constant);
						const pair<SPVTypeData*,string> &child = type->children[idx];
						if(child.second.empty())
							ret += StringFormat::Fmt("._member%u", idx);
						else
							ret += StringFormat::Fmt(".%s", child.second.c_str());
						type = child.first;
						continue;
					}
					else if(type->type == SPVTypeData::eArray)
					{
						if(op->arguments[i]->constant)
						{
							ret += StringFormat::Fmt("[%u]", idx);
						}
						else
						{
							// dynamic indexing into this array
							string arg;
							op->GetArg(ids, i, arg);
							ret += StringFormat::Fmt("[%s]", arg.c_str());
						}
						type = type->baseType;
						continue;
					}
					else if(type->type == SPVTypeData::eMatrix)
					{
						if(op->arguments[i]->constant)
						{
							ret += StringFormat::Fmt("[%u]", idx);
						}
						else
						{
							// dynamic indexing into this array
							string arg;
							op->GetArg(ids, i, arg);
							ret += StringFormat::Fmt("[%s]", arg.c_str());
						}

						// fall through to vector if we have another index
						if(i == op->arguments.size()-1)
							break;

						i++;
						// assuming can't dynamically index into a vector (would be a OpVectorShuffle)
						RDCASSERT(op->arguments[i]->constant && op->arguments[i]->constant->type->IsBasicInt());
						idx = op->arguments[i]->constant->i32;
					}

					// vector (or matrix + extra)
					{
						char swizzle[] = "xyzw";
						if(idx < 4)
							ret += StringFormat::Fmt(".%c", swizzle[idx]);
						else
							ret += StringFormat::Fmt("._%u", idx);

						// must be the last index, we're down to scalar granularity
						type = NULL;
						RDCASSERT(i == op->arguments.size()-1);
					}
				}

				return ret;
			}
			case spv::OpExtInst:
			{
				RDCASSERT(op);

				string ret = "";

				if(!inlineOp)
					ret = StringFormat::Fmt("%s %s = ", op->type->GetName().c_str(), GetIDName().c_str());

				ret += op->arguments[0]->ext->setname + "::";
				ret += op->arguments[0]->ext->instructions[op->literals[0]];
				ret += "(";

				for(size_t i=1; i < op->arguments.size(); i++)
				{
					string arg;
					op->GetArg(ids, i, arg);

					ret += arg;
					if(i+1 < op->arguments.size())
						ret += ", ";
				}

				ret += ")";

				return ret;
			}
			case spv::OpFunctionCall:
			{
				RDCASSERT(op);

				string ret = "";

				if(!inlineOp && op->type->type != SPVTypeData::eVoid)
					ret = StringFormat::Fmt("%s %s = ", op->type->GetName().c_str(), GetIDName().c_str());

				ret += ids[op->funcCall]->GetIDName() + "(";

				for(size_t i=0; i < op->arguments.size(); i++)
				{
					string arg;
					op->GetArg(ids, i, arg);

					ret += arg;
					if(i+1 < op->arguments.size())
						ret += ", ";
				}

				ret += ")";

				return ret;
			}
			case spv::OpVectorShuffle:
			{
				RDCASSERT(op);

				string ret = "";

				if(!inlineOp)
					ret = StringFormat::Fmt("%s %s = ", op->type->GetName().c_str(), GetIDName().c_str());

				SPVTypeData *vec1type = op->arguments[0]->op->type;
				SPVTypeData *vec2type = op->arguments[1]->op->type;

				RDCASSERT(vec1type->type == SPVTypeData::eVector && vec2type->type == SPVTypeData::eVector);

				uint32_t maxShuffle = 0;
				for(size_t i=0; i < op->literals.size(); i++)
				{
					uint32_t s = op->literals[i];
					if(s > vec1type->vectorSize)
						s -= vec1type->vectorSize;
					maxShuffle = RDCMAX(maxShuffle, op->literals[i]);
				}

				ret += op->type->GetName() + "(";

				// sane path for 4-vectors or less
				if(maxShuffle < 4)
				{
					char swizzle[] = "xyzw";

					int lastvec = -1;
					for(size_t i=0; i < op->literals.size(); i++)
					{
						int vec = 0;
						uint32_t s = op->literals[i];
						if(s > vec1type->vectorSize)
						{
							s -= vec1type->vectorSize;
							vec = 1;
						}

						if(vec != lastvec)
						{
							lastvec = vec;
							if(i > 0)
								ret += ", ";
							string arg;
							op->GetArg(ids, 0, arg);
							ret += arg;
							ret += ".";
						}

						ret += swizzle[s];
					}
				}

				ret += ")";

				return ret;
			}
			case spv::OpIAdd:
			case spv::OpIMul:
			case spv::OpFAdd:
			case spv::OpFMul:
			case spv::OpVectorTimesScalar:
			case spv::OpSLessThan:
			{
				RDCASSERT(op);

				char c = '?';
				switch(opcode)
				{
				case spv::OpIAdd:
				case spv::OpFAdd:
					c = '+';
					break;
				case spv::OpIMul:
				case spv::OpFMul:
				case spv::OpVectorTimesScalar:
					c = '*';
					break;
				case spv::OpSLessThan:
					c = '<';
					break;
				default:
					break;
				}

				string a, b;
				op->GetArg(ids, 0, a);
				op->GetArg(ids, 1, b);

				if(inlineOp)
					return StringFormat::Fmt("%s %c %s", a.c_str(), c, b.c_str());

				return StringFormat::Fmt("%s %s = %s %c %s", op->type->GetName().c_str(), GetIDName().c_str(), a.c_str(), c, b.c_str());
			}
			default:
				break;
		}

		string ret;

		ret = GetIDName() + " <=> " + ToStr::Get(opcode) + " ";
		for(size_t a=0; op && a < op->arguments.size(); a++)
		{
			ret += op->arguments[a]->GetIDName();
			if(a+1 < op->arguments.size())
				ret += " ";
		}

		return ret;
	}

	vector<SPVDecoration> decorations;

	// zero or one of these pointers might be set
	SPVExtInstSet *ext; // this ID is an extended instruction set
	SPVEntryPoint *entry; // this ID is an entry point
	SPVOperation *op; // this ID is the result of an operation
	SPVFlowControl *flow; // this is a flow control operation (no ID)
	SPVTypeData *type; // this ID names a type
	SPVFunction *func; // this ID names a function
	SPVBlock *block; // this is the ID of a label
	SPVConstant *constant; // this ID is a constant value
	SPVVariable *var; // this ID is a variable
};

void SPVOperation::GetArg(const vector<SPVInstruction *> &ids, size_t idx, string &arg)
{
	if(inlineArgs & (1<<idx))
		arg = arguments[idx]->Disassemble(ids, true);
	else
		arg = arguments[idx]->GetIDName();
}

struct SPVModule
{
	SPVModule()
	{
		shadType = eSPIRVInvalid;
		moduleVersion = 0;
		generator = 0;
		source.lang = spv::SourceLanguageUnknown; source.ver = 0;
		model.addr = spv::AddressingModelLogical; model.mem = spv::MemoryModelSimple;
	}

	~SPVModule()
	{
		for(size_t i=0; i < operations.size(); i++)
			delete operations[i];
		operations.clear();
	}

	SPIRVShaderStage shadType;
	uint32_t moduleVersion;
	uint32_t generator;
	struct { spv::SourceLanguage lang; uint32_t ver; } source;
	struct { spv::AddressingModel addr; spv::MemoryModel mem; } model;

	vector<SPVInstruction*> operations; // all operations (including those that don't generate an ID)

	vector<SPVInstruction*> ids; // pointers indexed by ID

	vector<SPVInstruction*> entries; // entry points
	vector<SPVInstruction*> globals; // global variables
	vector<SPVInstruction*> funcs; // functions
	vector<SPVInstruction*> structs; // struct types

	string Disassemble()
	{
		string disasm;

		// temporary function until we build our own structure from the SPIR-V
		const char *header[] = {
			"Vertex Shader",
			"Tessellation Control Shader",
			"Tessellation Evaluation Shader",
			"Geometry Shader",
			"Fragment Shader",
			"Compute Shader",
		};

		disasm = header[(int)shadType];
		disasm += " SPIR-V:\n\n";

		const char *gen = "Unrecognised";

		for(size_t i=0; i < ARRAY_COUNT(KnownGenerators); i++) if(KnownGenerators[i].magic == generator)	gen = KnownGenerators[i].name;

		disasm += StringFormat::Fmt("Version %u, Generator %08x (%s)\n", moduleVersion, generator, gen);
		disasm += StringFormat::Fmt("IDs up to {%u}\n", (uint32_t)ids.size());

		disasm += "\n";

		for(size_t i=0; i < entries.size(); i++)
		{
			RDCASSERT(entries[i]->entry);
			uint32_t func = entries[i]->entry->func;
			RDCASSERT(ids[func]);
			disasm += StringFormat::Fmt("Entry point '%s' (%s)\n", ids[func]->str.c_str(), ToStr::Get(entries[i]->entry->model).c_str());
		}

		disasm += "\n";

		for(size_t i=0; i < structs.size(); i++)
		{
			disasm += StringFormat::Fmt("struct %s {\n", structs[i]->type->GetName().c_str());
			for(size_t c=0; c < structs[i]->type->children.size(); c++)
			{
				auto member = structs[i]->type->children[c];

				string varName = member.second;

				if(varName.empty())
					varName = StringFormat::Fmt("member%u", c);

				disasm += StringFormat::Fmt("  %s;\n", member.first->DeclareVariable(varName).c_str());
			}
			disasm += StringFormat::Fmt("}; // struct %s\n", structs[i]->type->GetName().c_str());
		}

		disasm += "\n";

		for(size_t i=0; i < globals.size(); i++)
		{
			RDCASSERT(globals[i]->var && globals[i]->var->type);
			disasm += StringFormat::Fmt("%s %s %s;\n", ToStr::Get(globals[i]->var->storage).c_str(), globals[i]->var->type->GetName().c_str(), globals[i]->str.c_str());
		}

		disasm += "\n";

		for(size_t f=0; f < funcs.size(); f++)
		{
			SPVFunction *func = funcs[f]->func;
			RDCASSERT(func && func->retType && func->funcType);

			string args = "";

			for(size_t a=0; a < func->funcType->children.size(); a++)
			{
				const pair<SPVTypeData *,string> &arg = func->funcType->children[a];
				RDCASSERT(a < func->arguments.size());
				const SPVInstruction *argname = func->arguments[a];

				if(argname->str.empty())
					args += arg.first->GetName();
				else
					args += StringFormat::Fmt("%s %s", arg.first->GetName().c_str(), argname->str.c_str());

				if(a+1 < func->funcType->children.size())
					args += ", ";
			}

			disasm += StringFormat::Fmt("%s %s(%s)%s {\n", func->retType->GetName().c_str(), funcs[f]->str.c_str(), args.c_str(), OptionalFlagString(func->control).c_str());

			// local copy of variables vector
			vector<SPVInstruction *> vars = func->variables;
			vector<SPVInstruction *> funcops;

			for(size_t b=0; b < func->blocks.size(); b++)
			{
				SPVInstruction *block = func->blocks[b];

				// don't push first label in a function
				if(b > 0)
					funcops.push_back(block); // OpLabel

				set<SPVInstruction *> ignore_items;

				for(size_t i=0; i < block->block->instructions.size(); i++)
				{
					SPVInstruction *instr = block->block->instructions[i];

					if(ignore_items.find(instr) == ignore_items.end())
						funcops.push_back(instr);

					if(instr->op)
					{
						for(size_t a=0; a < instr->op->arguments.size(); a++)
						{
							SPVInstruction *arg = instr->op->arguments[a];

							// don't fold up too complex an operation
							if(arg->op)
							{
								// allow access chains to have multiple arguments
								if(arg->op->complexity > 1 || (arg->op->arguments.size() > 2 && arg->opcode != spv::OpAccessChain))
									continue;
							}

							erase_item(funcops, arg);

							instr->op->inlineArgs |= (1<<a);
						}

						instr->op->complexity++;

						// special handling for function call to inline temporary pointer variables
						// created for passing parameters
						if(instr->opcode == spv::OpFunctionCall)
						{
							for(size_t a=0; a < instr->op->arguments.size(); a++)
							{
								SPVInstruction *arg = instr->op->arguments[a];

								// if this argument has
								//  - only one usage as a store target before the function call
								//  = then it's an in parameter, and we can fold it in.
								//
								//  - only one usage as a load target after the function call
								//  = then it's an out parameter, we can fold it in as long as
								//    the usage after is in a Store(a) = Load(param) case
								//
								//  - exactly one usage as store before, and load after, such that
								//    it is Store(param) = Load(a) .... Store(a) = Load(param)
								//  = then it's an inout parameter, and we can fold it in

								bool canReplace = true;
								SPVInstruction *storeBefore = NULL;
								SPVInstruction *loadAfter = NULL;
								size_t storeIdx = block->block->instructions.size();
								size_t loadIdx = block->block->instructions.size();

								for(size_t j=0; j < i; j++)
								{
									SPVInstruction *searchInst = block->block->instructions[j];
									for(size_t aa=0; searchInst->op && aa < searchInst->op->arguments.size(); aa++)
									{
										if(searchInst->op->arguments[aa]->id == arg->id)
										{
											if(searchInst->opcode == spv::OpStore)
											{
												// if it's used in multiple stores, it can't be folded
												if(storeBefore)
												{
													canReplace = false;
													break;
												}
												storeBefore = searchInst;
												storeIdx = j;
											}
											else
											{
												// if it's used in anything but a store, it can't be folded
												canReplace = false;
												break;
											}
										}
									}

									// if it's used in a condition, it can't be folded
									if(searchInst->flow && searchInst->flow->condition && searchInst->flow->condition->id == arg->id)
										canReplace = false;

									if(!canReplace)
										break;
								}

								for(size_t j=i+1; j < block->block->instructions.size(); j++)
								{
									SPVInstruction *searchInst = block->block->instructions[j];
									for(size_t aa=0; searchInst->op && aa < searchInst->op->arguments.size(); aa++)
									{
										if(searchInst->op->arguments[aa]->id == arg->id)
										{
											if(searchInst->opcode == spv::OpLoad)
											{
												// if it's used in multiple load, it can't be folded
												if(loadAfter)
												{
													canReplace = false;
													break;
												}
												loadAfter = searchInst;
												loadIdx = j;
											}
											else
											{
												// if it's used in anything but a load, it can't be folded
												canReplace = false;
												break;
											}
										}
									}

									// if it's used in a condition, it can't be folded
									if(searchInst->flow && searchInst->flow->condition && searchInst->flow->condition->id == arg->id)
										canReplace = false;

									if(!canReplace)
										break;
								}

								if(canReplace)
								{
									// in parameter
									if(storeBefore && !loadAfter)
									{
										erase_item(funcops, storeBefore);

										erase_item(vars, instr->op->arguments[a]);

										// pass function parameter directly from where the store was coming from
										instr->op->arguments[a] = storeBefore->op->arguments[1];
									}

									// out or inout parameter
									if(loadAfter)
									{
										// need to check the load afterwards is only ever used in a store operation

										SPVInstruction *storeUse = NULL;
										
										for(size_t j=loadIdx+1; j < block->block->instructions.size(); j++)
										{
											SPVInstruction *searchInst = block->block->instructions[j];

											for(size_t aa=0; searchInst->op && aa < searchInst->op->arguments.size(); aa++)
											{
												if(searchInst->op->arguments[aa] == loadAfter)
												{
													if(searchInst->opcode == spv::OpStore)
													{
														// if it's used in multiple stores, it can't be folded
														if(storeUse)
														{
															canReplace = false;
															break;
														}
														storeUse = searchInst;
													}
													else
													{
														// if it's used in anything but a store, it can't be folded
														canReplace = false;
														break;
													}
												}
											}
											
											// if it's used in a condition, it can't be folded
											if(searchInst->flow && searchInst->flow->condition == loadAfter)
												canReplace = false;

											if(!canReplace)
												break;
										}
										
										if(canReplace && storeBefore != NULL)
										{
											// for the inout parameter case, we also need to verify that
											// the Store() before the function call comes from a Load(),
											// and that the variable being Load()'d is identical to the
											// variable in the Store() in storeUse that we've found

											if(storeBefore->op->arguments[1]->opcode == spv::OpLoad &&
												storeBefore->op->arguments[1]->op->arguments[0]->id ==
												storeUse->op->arguments[0]->id)
											{
												erase_item(funcops, storeBefore);
											}
											else
											{
												canReplace = false;
											}
										}

										if(canReplace)
										{
											// we haven't reached this store instruction yet, so need to mark that
											// it has been folded and should be skipped
											ignore_items.insert(storeUse);

											erase_item(vars, instr->op->arguments[a]);

											// pass argument directly
											instr->op->arguments[a] = storeUse->op->arguments[0];
										}
									}
								}
							}
						}
					}
				}

				if(block->block->mergeFlow)
					funcops.push_back(block->block->mergeFlow);
				if(block->block->exitFlow)
				{
					// branch conditions are inlined
					if(block->block->exitFlow->flow->condition)
							erase_item(funcops, block->block->exitFlow->flow->condition);

					// return values are inlined
					if(block->block->exitFlow->opcode == spv::OpReturnValue)
					{
						SPVInstruction *arg = ids[block->block->exitFlow->flow->targets[0]];

						erase_item(funcops, arg);
					}

					funcops.push_back(block->block->exitFlow);
				}
			}

			// find redundant branch/label pairs
			for(size_t l=0; l < funcops.size()-1;)
			{
				if(funcops[l]->opcode == spv::OpBranch)
				{
					if(funcops[l+1]->opcode == spv::OpLabel && funcops[l]->flow->targets[0] == funcops[l+1]->id)
					{
						uint32_t label = funcops[l+1]->id;

						bool refd = false;

						// see if this label is a target anywhere else
						for(size_t b=0; b < funcops.size(); b++)
						{
							if(l == b) continue;

							if(funcops[b]->flow)
							{
								for(size_t t=0; t < funcops[b]->flow->targets.size(); t++)
								{
									if(funcops[b]->flow->targets[t] == label)
									{
										refd = true;
										break;
									}
								}

								if(refd)
									break;
							}
						}

						if(!refd)
						{
							funcops.erase(funcops.begin()+l);
							funcops.erase(funcops.begin()+l);
							continue;
						}
						else
						{
							// if it is refd, we can at least remove the goto
							funcops.erase(funcops.begin()+l);
							continue;
						}
					}
				}

				l++;
			}

			size_t tabSize = 2;
			size_t indent = tabSize;

			for(size_t v=0; v < vars.size(); v++)
			{
				RDCASSERT(vars[v]->var && vars[v]->var->type);
				disasm += StringFormat::Fmt("%s%s %s;\n", string(indent, ' ').c_str(), vars[v]->var->type->GetName().c_str(), vars[v]->str.c_str());
			}

			if(!vars.empty())
				disasm += "\n";

			vector<uint32_t> selectionstack;
			vector<uint32_t> elsestack;

			vector<uint32_t> loopheadstack;
			vector<uint32_t> loopstartstack;
			vector<uint32_t> loopmergestack;

			for(size_t o=0; o < funcops.size(); o++)
			{
				if(funcops[o]->opcode == spv::OpLabel)
				{
					if(!elsestack.empty() && elsestack.back() == funcops[o]->id)
					{
						// handle meeting an else block
						disasm += string(indent - tabSize, ' ');
						disasm += "} else {\n";
						elsestack.pop_back();
					}
					else if(!selectionstack.empty() && selectionstack.back() == funcops[o]->id)
					{
						// handle meeting a selection merge block
						indent -= tabSize;

						disasm += string(indent, ' ');
						disasm += "}\n";
						selectionstack.pop_back();
					}
					else if(!loopmergestack.empty() && loopmergestack.back() == funcops[o]->id)
					{
						// handle meeting a loop merge block
						indent -= tabSize;

						disasm += string(indent, ' ');
						disasm += "}\n";
						loopmergestack.pop_back();
					}
					else if(!loopstartstack.empty() && loopstartstack.back() == funcops[o]->id)
					{
						// completely skip a label at the start of the loop. It's implicit from braces
					}
					else if(funcops[o]->block->mergeFlow && funcops[o]->block->mergeFlow->opcode == spv::OpLoopMerge)
					{
						// this block is a loop header
						// TODO handle if the loop header condition expression isn't sufficiently in-lined.
						// We need to force inline it.
						disasm += string(indent, ' ');
						disasm += "while(" + funcops[o]->block->exitFlow->flow->condition->Disassemble(ids, true) + ") {\n";

						loopheadstack.push_back(funcops[o]->id);
						loopstartstack.push_back(funcops[o]->block->exitFlow->flow->targets[0]);
						loopmergestack.push_back(funcops[o]->block->mergeFlow->flow->targets[0]);

						// false from the condition should jump straight to merge block
						RDCASSERT(funcops[o]->block->exitFlow->flow->targets[1] == funcops[o]->block->mergeFlow->flow->targets[0]);

						indent += tabSize;
					}
					else
					{
						disasm += funcops[o]->Disassemble(ids, false) + "\n";
					}
				}
				else if(funcops[o]->opcode == spv::OpBranch)
				{
					if(!selectionstack.empty() && funcops[o]->flow->targets[0] == selectionstack.back())
					{
						// if we're at the end of a true if path there will be a goto to
						// the merge block before the false path label. Don't output it
					}
					else if(!loopheadstack.empty() && funcops[o]->flow->targets[0] == loopheadstack.back())
					{
						if(o+1 < funcops.size() && funcops[o+1]->opcode == spv::OpLabel &&
							funcops[o+1]->id == loopmergestack.back())
						{
							// skip any gotos at the end of a loop jumping back to the header
							// block to do another loop
						}
						else
						{
							// if we're skipping to the header of the loop before the end, this is a continue
							disasm += string(indent, ' ');
							disasm += "continue;\n";
						}
					}
					else if(!loopmergestack.empty() && funcops[o]->flow->targets[0] == loopmergestack.back())
					{
						// if we're skipping to the merge of the loop without going through the
						// branch conditional, this is a break
						disasm += string(indent, ' ');
						disasm += "break;\n";
					}
					else
					{
						disasm += string(indent, ' ');
						disasm += funcops[o]->Disassemble(ids, false) + ";\n";
					}
				}
				else if(funcops[o]->opcode == spv::OpLoopMerge)
				{
					// handled above when this block started
					o++; // skip the branch conditional op
				}
				else if(funcops[o]->opcode == spv::OpSelectionMerge)
				{
					selectionstack.push_back(funcops[o]->flow->targets[0]);

					RDCASSERT(o+1 < funcops.size() && funcops[o+1]->opcode == spv::OpBranchConditional);
					o++;

					disasm += string(indent, ' ');
					disasm += "if(" + funcops[o]->Disassemble(ids, false) + ") {\n";

					indent += tabSize;

					// does the branch have an else case
					if(funcops[o]->flow->targets[1] != selectionstack.back())
						elsestack.push_back(funcops[o]->flow->targets[1]);

					RDCASSERT(o+1 < funcops.size() && funcops[o+1]->opcode == spv::OpLabel &&
						  funcops[o+1]->id == funcops[o]->flow->targets[0]);
					o++; // skip outputting this label, it becomes our { essentially
				}
				else
				{
					disasm += string(indent, ' ');
					disasm += funcops[o]->Disassemble(ids, false) + ";\n";
				}

				funcops[o]->line = (int)o;
			}

			disasm += StringFormat::Fmt("} // %s\n\n", funcs[f]->str.c_str());
		}

		// for each func:
		//   declare instructino list
		//   push variable declares
		//
		//   for each block:
		//     for each instruction:
		//       add to list
		//       if instruction takes params:
		//         if params instructions complexity are low enough
		//           take param instructions out of list
		//           incr. this instruction's complexity
		//           mark params to be melded
		//           aggressively meld function call parameters, remove variable declares
		//
		//   do magic secret sauce to analyse ifs and loops
		//
		//   for instructions in list:
		//     mark line num to all 'child' instructions, for stepping
		//     output combined line
		//     if instruction pair is goto then label, skip
		//
		//

		return disasm;
	}
};

void DisassembleSPIRV(SPIRVShaderStage shadType, const vector<uint32_t> &spirv, string &disasm)
{
#if 1
	return;
#else
	if(shadType >= eSPIRVInvalid)
		return;

	SPVModule module;

	module.shadType = shadType;

	if(spirv[0] != (uint32_t)spv::MagicNumber)
	{
		disasm = StringFormat::Fmt("Unrecognised magic number %08x", spirv[0]);
		return;
	}
	
	module.moduleVersion = spirv[1];
	module.generator = spirv[2];
	module.ids.resize(spirv[3]);

	uint32_t idbound = spirv[3];

	RDCASSERT(spirv[4] == 0);

	SPVFunction *curFunc = NULL;
	SPVBlock *curBlock = NULL;

	size_t it = 5;
	while(it < spirv.size())
	{
		uint16_t WordCount = spirv[it]>>16;

		module.operations.push_back(new SPVInstruction());
		SPVInstruction &op = *module.operations.back();

		op.opcode = spv::Op(spirv[it]&0xffff);

		switch(op.opcode)
		{
			//////////////////////////////////////////////////////////////////////
			// 'Global' opcodes
			case spv::OpSource:
			{
				module.source.lang = spv::SourceLanguage(spirv[it+1]);
				module.source.ver = spirv[it+2];
				break;
			}
			case spv::OpMemoryModel:
			{
				module.model.addr = spv::AddressingModel(spirv[it+1]);
				module.model.mem = spv::MemoryModel(spirv[it+2]);
				break;
			}
			case spv::OpEntryPoint:
			{
				op.entry = new SPVEntryPoint();
				op.entry->func = spirv[it+2];
				op.entry->model = spv::ExecutionModel(spirv[it+1]);
				module.entries.push_back(&op);
				break;
			}
			case spv::OpExtInstImport:
			{
				op.ext = new SPVExtInstSet();
				op.ext->setname = (const char *)&spirv[it+2];
				op.ext->instructions = NULL;

				if(op.ext->setname == "GLSL.std.450")
				{
					op.ext->instructions = GLSL_STD_450_names;

					if(GLSL_STD_450_names[0] == NULL)
						GLSL_STD_450::GetDebugNames(GLSL_STD_450_names);
				}

				op.id = spirv[it+1];
				module.ids[spirv[it+1]] = &op;
				break;
			}
			case spv::OpString:
			{
				op.str = (const char *)&spirv[it+2];

				op.id = spirv[it+1];
				module.ids[spirv[it+1]] = &op;
				break;
			}
			//////////////////////////////////////////////////////////////////////
			// Type opcodes
			case spv::OpTypeVoid:
			{
				op.type = new SPVTypeData();
				op.type->type = SPVTypeData::eVoid;
				
				op.id = spirv[it+1];
				module.ids[spirv[it+1]] = &op;
				break;
			}
			case spv::OpTypeBool:
			{
				op.type = new SPVTypeData();
				op.type->type = SPVTypeData::eBool;
				
				op.id = spirv[it+1];
				module.ids[spirv[it+1]] = &op;
				break;
			}
			case spv::OpTypeInt:
			{
				op.type = new SPVTypeData();
				op.type->type = spirv[it+3] ? SPVTypeData::eSInt : SPVTypeData::eUInt;
				op.type->bitCount = spirv[it+2];
				
				op.id = spirv[it+1];
				module.ids[spirv[it+1]] = &op;
				break;
			}
			case spv::OpTypeFloat:
			{
				op.type = new SPVTypeData();
				op.type->type = SPVTypeData::eFloat;
				op.type->bitCount = spirv[it+2];
				
				op.id = spirv[it+1];
				module.ids[spirv[it+1]] = &op;
				break;
			}
			case spv::OpTypeVector:
			{
				op.type = new SPVTypeData();
				op.type->type = SPVTypeData::eVector;

				SPVInstruction *baseTypeInst = module.ids[spirv[it+2]];
				RDCASSERT(baseTypeInst && baseTypeInst->type);

				op.type->baseType = baseTypeInst->type;
				op.type->vectorSize = spirv[it+3];
				
				op.id = spirv[it+1];
				module.ids[spirv[it+1]] = &op;
				break;
			}
			case spv::OpTypeArray:
			{
				op.type = new SPVTypeData();
				op.type->type = SPVTypeData::eArray;

				SPVInstruction *baseTypeInst = module.ids[spirv[it+2]];
				RDCASSERT(baseTypeInst && baseTypeInst->type);

				op.type->baseType = baseTypeInst->type;

				SPVInstruction *sizeInst = module.ids[spirv[it+3]];
				RDCASSERT(sizeInst && sizeInst->constant && sizeInst->constant->type->IsBasicInt());

				op.type->arraySize = sizeInst->constant->u32;
				
				op.id = spirv[it+1];
				module.ids[spirv[it+1]] = &op;
				break;
			}
			case spv::OpTypeStruct:
			{
				op.type = new SPVTypeData();
				op.type->type = SPVTypeData::eStruct;

				for(int i=2; i < WordCount; i++)
				{
					SPVInstruction *memberInst = module.ids[spirv[it+i]];
					RDCASSERT(memberInst && memberInst->type);

					// names might come later from OpMemberName instructions
					op.type->children.push_back(make_pair(memberInst->type, ""));
				}

				module.structs.push_back(&op);
				
				op.id = spirv[it+1];
				module.ids[spirv[it+1]] = &op;
				break;
			}
			case spv::OpTypePointer:
			{
				op.type = new SPVTypeData();
				op.type->type = SPVTypeData::ePointer;

				SPVInstruction *baseTypeInst = module.ids[spirv[it+3]];
				RDCASSERT(baseTypeInst && baseTypeInst->type);

				op.type->baseType = baseTypeInst->type;
				op.type->storage = spv::StorageClass(spirv[it+2]);
				
				op.id = spirv[it+1];
				module.ids[spirv[it+1]] = &op;
				break;
			}
			case spv::OpTypeFunction:
			{
				op.type = new SPVTypeData();
				op.type->type = SPVTypeData::eFunction;

				for(int i=3; i < WordCount; i++)
				{
					SPVInstruction *argInst = module.ids[spirv[it+i]];
					RDCASSERT(argInst && argInst->type);

					// function parameters have no name
					op.type->children.push_back(make_pair(argInst->type, ""));
				}

				SPVInstruction *baseTypeInst = module.ids[spirv[it+2]];
				RDCASSERT(baseTypeInst && baseTypeInst->type);

				// return type
				op.type->baseType = baseTypeInst->type;
				
				op.id = spirv[it+1];
				module.ids[spirv[it+1]] = &op;
				break;
			}
			//////////////////////////////////////////////////////////////////////
			// Constants
			case spv::OpConstantTrue:
			case spv::OpConstantFalse:
			{
				SPVInstruction *typeInst = module.ids[spirv[it+1]];
				RDCASSERT(typeInst && typeInst->type);

				op.constant = new SPVConstant();
				op.constant->type = typeInst->type;

				op.constant->u32 = op.opcode == spv::OpConstantTrue ? 1 : 0;

				op.id = spirv[it+2];
				module.ids[spirv[it+2]] = &op;
				break;
			}
			case spv::OpConstant:
			{
				SPVInstruction *typeInst = module.ids[spirv[it+1]];
				RDCASSERT(typeInst && typeInst->type);

				op.constant = new SPVConstant();
				op.constant->type = typeInst->type;

				op.constant->u32 = spirv[it+3];

				if(WordCount > 3)
				{
					// only handle 32-bit or 64-bit constants
					RDCASSERT(WordCount <= 4);

					uint64_t lo = spirv[it+3];
					uint64_t hi = spirv[it+4];

					op.constant->u64 = lo | (hi<<32);
				}

				op.id = spirv[it+2];
				module.ids[spirv[it+2]] = &op;
				break;
			}
			case spv::OpConstantComposite:
			{
				SPVInstruction *typeInst = module.ids[spirv[it+1]];
				RDCASSERT(typeInst && typeInst->type);

				op.constant = new SPVConstant();
				op.constant->type = typeInst->type;

				for(int i=3; i < WordCount; i++)
				{
					SPVInstruction *constInst = module.ids[spirv[it+i]];
					RDCASSERT(constInst && constInst->constant);

					op.constant->children.push_back(constInst->constant);
				}

				op.id = spirv[it+2];
				module.ids[spirv[it+2]] = &op;

				break;
			}
			//////////////////////////////////////////////////////////////////////
			// Functions
			case spv::OpFunction:
			{
				SPVInstruction *retTypeInst = module.ids[spirv[it+1]];
				RDCASSERT(retTypeInst && retTypeInst->type);

				SPVInstruction *typeInst = module.ids[spirv[it+4]];
				RDCASSERT(typeInst && typeInst->type);

				op.func = new SPVFunction();
				op.func->retType = retTypeInst->type;
				op.func->funcType = typeInst->type;
				op.func->control = spv::FunctionControlMask(spirv[it+3]);

				module.funcs.push_back(&op);

				op.id = spirv[it+2];
				module.ids[spirv[it+2]] = &op;

				curFunc = op.func;

				break;
			}
			case spv::OpFunctionEnd:
			{
				curFunc = NULL;
				break;
			}
			//////////////////////////////////////////////////////////////////////
			// Variables
			case spv::OpVariable:
			{
				SPVInstruction *typeInst = module.ids[spirv[it+1]];
				RDCASSERT(typeInst && typeInst->type);

				op.var = new SPVVariable();
				op.var->type = typeInst->type;
				op.var->storage = spv::StorageClass(spirv[it+3]);

				if(WordCount > 4)
				{
					SPVInstruction *initInst = module.ids[spirv[it+4]];
					RDCASSERT(initInst && initInst->constant);
					op.var->initialiser = initInst->constant;
				}

				if(curFunc) curFunc->variables.push_back(&op);
				else module.globals.push_back(&op);

				op.id = spirv[it+2];
				module.ids[spirv[it+2]] = &op;
				break;
			}
			case spv::OpFunctionParameter:
			{
				SPVInstruction *typeInst = module.ids[spirv[it+1]];
				RDCASSERT(typeInst && typeInst->type);

				op.var = new SPVVariable();
				op.var->type = typeInst->type;
				op.var->storage = spv::StorageClassFunction;

				RDCASSERT(curFunc);
				curFunc->arguments.push_back(&op);

				op.id = spirv[it+2];
				module.ids[spirv[it+2]] = &op;
				break;
			}
			//////////////////////////////////////////////////////////////////////
			// Branching/flow control
			case spv::OpLabel:
			{
				op.block = new SPVBlock();

				RDCASSERT(curFunc);

				curFunc->blocks.push_back(&op);
				curBlock = op.block;

				op.id = spirv[it+1];
				module.ids[spirv[it+1]] = &op;
				break;
			}
			case spv::OpKill:
			case spv::OpUnreachable:
			case spv::OpReturn:
			{
				op.flow = new SPVFlowControl();

				curBlock->exitFlow = &op;
				curBlock = NULL;
				break;
			}
			case spv::OpReturnValue:
			{
				op.flow = new SPVFlowControl();

				op.flow->targets.push_back(spirv[it+1]);

				curBlock->exitFlow = &op;
				curBlock = NULL;
				break;
			}
			case spv::OpBranch:
			{
				op.flow = new SPVFlowControl();

				op.flow->targets.push_back(spirv[it+1]);

				curBlock->exitFlow = &op;
				curBlock = NULL;
				break;
			}
			case spv::OpBranchConditional:
			{
				op.flow = new SPVFlowControl();

				SPVInstruction *condInst = module.ids[spirv[it+1]];
				RDCASSERT(condInst);

				op.flow->condition = condInst;
				op.flow->targets.push_back(spirv[it+2]);
				op.flow->targets.push_back(spirv[it+3]);

				if(WordCount == 6)
				{
					op.flow->literals.push_back(spirv[it+4]);
					op.flow->literals.push_back(spirv[it+5]);
				}

				curBlock->exitFlow = &op;
				curBlock = NULL;
				break;
			}
			case spv::OpSelectionMerge:
			{
				op.flow = new SPVFlowControl();

				op.flow->targets.push_back(spirv[it+1]);
				op.flow->selControl = spv::SelectionControlMask(spirv[it+2]);

				curBlock->mergeFlow = &op;
				break;
			}
			case spv::OpLoopMerge:
			{
				op.flow = new SPVFlowControl();

				op.flow->targets.push_back(spirv[it+1]);
				op.flow->loopControl = spv::LoopControlMask(spirv[it+2]);

				curBlock->mergeFlow = &op;
				break;
			}
			//////////////////////////////////////////////////////////////////////
			// Operations with special parameters
			case spv::OpLoad:
			{
				SPVInstruction *typeInst = module.ids[spirv[it+1]];
				RDCASSERT(typeInst && typeInst->type);

				op.op = new SPVOperation();
				op.op->type = typeInst->type;
				
				SPVInstruction *ptrInst = module.ids[spirv[it+3]];
				RDCASSERT(ptrInst);

				op.op->arguments.push_back(ptrInst);

				op.op->access = spv::MemoryAccessMaskNone;

				for(int i=4; i < WordCount; i++)
				{
					if(i == WordCount-1)
					{
						op.op->access = spv::MemoryAccessMask(spirv[it+i]);
					}
					else
					{
						uint32_t lit = spirv[it+i];
						// don't understand what these literals are - seems like OpAccessChain handles
						// struct member/array access so it doesn't seem to be for array indices
						RDCBREAK(); 
					}
				}
				
				op.id = spirv[it+2];
				module.ids[spirv[it+2]] = &op;
				
				curBlock->instructions.push_back(&op);
				break;
			}
			case spv::OpStore:
			case spv::OpCopyMemory:
			{
				op.op = new SPVOperation();
				op.op->type = NULL;
				
				SPVInstruction *ptrInst = module.ids[spirv[it+1]];
				RDCASSERT(ptrInst);
				
				SPVInstruction *valInst = module.ids[spirv[it+2]];
				RDCASSERT(valInst);

				op.op->arguments.push_back(ptrInst);
				op.op->arguments.push_back(valInst);

				op.op->access = spv::MemoryAccessMaskNone;

				for(int i=3; i < WordCount; i++)
				{
					if(i == WordCount-1)
					{
						op.op->access = spv::MemoryAccessMask(spirv[it+i]);
					}
					else
					{
						uint32_t lit = spirv[it+i];
						// don't understand what these literals are - seems like OpAccessChain handles
						// struct member/array access so it doesn't seem to be for array indices
						RDCBREAK(); 
					}
				}
				
				curBlock->instructions.push_back(&op);
				break;
			}
			case spv::OpFunctionCall:
			{
				SPVInstruction *typeInst = module.ids[spirv[it+1]];
				RDCASSERT(typeInst && typeInst->type);

				op.op = new SPVOperation();
				op.op->type = typeInst->type;

				op.op->funcCall = spirv[it+3];

				for(int i=4; i < WordCount; i++)
				{
					SPVInstruction *argInst = module.ids[spirv[it+i]];
					RDCASSERT(argInst);

					op.op->arguments.push_back(argInst);
				}

				op.id = spirv[it+2];
				module.ids[spirv[it+2]] = &op;

				curBlock->instructions.push_back(&op);
				break;
			}
			case spv::OpVectorShuffle:
			{
				SPVInstruction *typeInst = module.ids[spirv[it+1]];
				RDCASSERT(typeInst && typeInst->type);

				op.op = new SPVOperation();
				op.op->type = typeInst->type;

				{
					SPVInstruction *argInst = module.ids[spirv[it+3]];
					RDCASSERT(argInst);

					op.op->arguments.push_back(argInst);
				}

				{
					SPVInstruction *argInst = module.ids[spirv[it+4]];
					RDCASSERT(argInst);

					op.op->arguments.push_back(argInst);
				}

				for(int i=5; i < WordCount; i++)
					op.op->literals.push_back(spirv[it+i]);

				op.id = spirv[it+2];
				module.ids[spirv[it+2]] = &op;

				curBlock->instructions.push_back(&op);
				break;
			}
			case spv::OpExtInst:
			{
				SPVInstruction *typeInst = module.ids[spirv[it+1]];
				RDCASSERT(typeInst && typeInst->type);

				op.op = new SPVOperation();
				op.op->type = typeInst->type;

				{
					SPVInstruction *setInst = module.ids[spirv[it+3]];
					RDCASSERT(setInst);

					op.op->arguments.push_back(setInst);
				}

				op.op->literals.push_back(spirv[it+4]);

				for(int i=5; i < WordCount; i++)
				{
					SPVInstruction *argInst = module.ids[spirv[it+i]];
					RDCASSERT(argInst);

					op.op->arguments.push_back(argInst);
				}

				op.id = spirv[it+2];
				module.ids[spirv[it+2]] = &op;

				curBlock->instructions.push_back(&op);
				break;
			}
			//////////////////////////////////////////////////////////////////////
			// Easy to handle opcodes with just some number of ID parameters
			case spv::OpAccessChain:
			case spv::OpIAdd:
			case spv::OpIMul:
			case spv::OpFAdd:
			case spv::OpFMul:
			case spv::OpVectorTimesScalar:
			case spv::OpSLessThan:
			{
				SPVInstruction *typeInst = module.ids[spirv[it+1]];
				RDCASSERT(typeInst && typeInst->type);

				op.op = new SPVOperation();
				op.op->type = typeInst->type;
				
				for(int i=3; i < WordCount; i++)
				{
					SPVInstruction *argInst = module.ids[spirv[it+i]];
					RDCASSERT(argInst);

					op.op->arguments.push_back(argInst);
				}
				
				op.id = spirv[it+2];
				module.ids[spirv[it+2]] = &op;
				
				curBlock->instructions.push_back(&op);
				break;
			}
			case spv::OpName:
			case spv::OpMemberName:
			case spv::OpLine:
			case spv::OpDecorate:
			case spv::OpMemberDecorate:
			case spv::OpGroupDecorate:
			case spv::OpGroupMemberDecorate:
			case spv::OpDecorationGroup:
				// Handled in second pass once all IDs are in place
				break;
			default:
			{
				RDCERR("Unhandled opcode %s - result ID will be missing", ToStr::Get(op.opcode).c_str());
				break;
			}
		}

		it += WordCount;
	}

	// second pass now that we have all ids set up, apply decorations/names/etc
	it = 5;
	while(it < spirv.size())
	{
		uint16_t WordCount = spirv[it]>>16;
		spv::Op op = spv::Op(spirv[it]&0xffff);

		switch(op)
		{
			case spv::OpName:
			{
				SPVInstruction *varInst = module.ids[spirv[it+1]];
				RDCASSERT(varInst);

				varInst->str = (const char *)&spirv[it+2];

				// strip any 'encoded type' information from function names
				if(varInst->opcode == spv::OpFunction)
				{
					size_t bracket = varInst->str.find('(');
					if(bracket != string::npos)
						varInst->str = varInst->str.substr(0, bracket);
				}

				if(varInst->type)
					varInst->type->name = varInst->str;
				break;
			}
			case spv::OpMemberName:
			{
				SPVInstruction *varInst = module.ids[spirv[it+1]];
				RDCASSERT(varInst && varInst->type && varInst->type->type == SPVTypeData::eStruct);
				uint32_t memIdx = spirv[it+2];
				RDCASSERT(memIdx < varInst->type->children.size());
				varInst->type->children[memIdx].second = (const char *)&spirv[it+3];
				break;
			}
			case spv::OpLine:
			{
				SPVInstruction *varInst = module.ids[spirv[it+1]];
				RDCASSERT(varInst);

				SPVInstruction *fileInst = module.ids[spirv[it+2]];
				RDCASSERT(fileInst);

				varInst->source.filename = fileInst->str;
				varInst->source.line = spirv[it+3];
				varInst->source.col = spirv[it+4];
				break;
			}
			case spv::OpDecorate:
			{
				SPVInstruction *inst = module.ids[spirv[it+1]];
				RDCASSERT(inst);

				SPVDecoration d;
				d.decoration = spv::Decoration(spirv[it+2]);
				
				// TODO this isn't enough for all decorations
				RDCASSERT(WordCount <= 3);
				if(WordCount > 2)
					d.val = spirv[it+3];

				inst->decorations.push_back(d);
				break;
			}
			case spv::OpMemberDecorate:
			case spv::OpGroupDecorate:
			case spv::OpGroupMemberDecorate:
			case spv::OpDecorationGroup:
				// TODO
				RDCBREAK();
				break;
			default:
				break;
		}

		it += WordCount;
	}

	struct SortByVarClass
	{
		bool operator () (const SPVInstruction *a, const SPVInstruction *b)
		{
			RDCASSERT(a->var && b->var);

			return a->var->storage < b->var->storage;
		}
	};

	std::sort(module.globals.begin(), module.globals.end(), SortByVarClass());

	disasm = module.Disassemble();

	return;
#endif
}

template<>
string ToStrHelper<false, spv::Op>::Get(const spv::Op &el)
{
#if 0
	switch(el)
	{
		case spv::OpNop:                                      return "Nop";
		case spv::OpSource:                                   return "Source";
		case spv::OpSourceExtension:                          return "SourceExtension";
		case spv::OpExtension:                                return "Extension";
		case spv::OpExtInstImport:                            return "ExtInstImport";
		case spv::OpMemoryModel:                              return "MemoryModel";
		case spv::OpEntryPoint:                               return "EntryPoint";
		case spv::OpExecutionMode:                            return "ExecutionMode";
		case spv::OpTypeVoid:                                 return "TypeVoid";
		case spv::OpTypeBool:                                 return "TypeBool";
		case spv::OpTypeInt:                                  return "TypeInt";
		case spv::OpTypeFloat:                                return "TypeFloat";
		case spv::OpTypeVector:                               return "TypeVector";
		case spv::OpTypeMatrix:                               return "TypeMatrix";
		case spv::OpTypeSampler:                              return "TypeSampler";
		case spv::OpTypeFilter:                               return "TypeFilter";
		case spv::OpTypeArray:                                return "TypeArray";
		case spv::OpTypeRuntimeArray:                         return "TypeRuntimeArray";
		case spv::OpTypeStruct:                               return "TypeStruct";
		case spv::OpTypeOpaque:                               return "TypeOpaque";
		case spv::OpTypePointer:                              return "TypePointer";
		case spv::OpTypeFunction:                             return "TypeFunction";
		case spv::OpTypeEvent:                                return "TypeEvent";
		case spv::OpTypeDeviceEvent:                          return "TypeDeviceEvent";
		case spv::OpTypeReserveId:                            return "TypeReserveId";
		case spv::OpTypeQueue:                                return "TypeQueue";
		case spv::OpTypePipe:                                 return "TypePipe";
		case spv::OpConstantTrue:                             return "ConstantTrue";
		case spv::OpConstantFalse:                            return "ConstantFalse";
		case spv::OpConstant:                                 return "Constant";
		case spv::OpConstantComposite:                        return "ConstantComposite";
		case spv::OpConstantSampler:                          return "ConstantSampler";
		case spv::OpConstantNullPointer:                      return "ConstantNullPointer";
		case spv::OpConstantNullObject:                       return "ConstantNullObject";
		case spv::OpSpecConstantTrue:                         return "SpecConstantTrue";
		case spv::OpSpecConstantFalse:                        return "SpecConstantFalse";
		case spv::OpSpecConstant:                             return "SpecConstant";
		case spv::OpSpecConstantComposite:                    return "SpecConstantComposite";
		case spv::OpVariable:                                 return "Variable";
		case spv::OpVariableArray:                            return "VariableArray";
		case spv::OpFunction:                                 return "Function";
		case spv::OpFunctionParameter:                        return "FunctionParameter";
		case spv::OpFunctionEnd:                              return "FunctionEnd";
		case spv::OpFunctionCall:                             return "FunctionCall";
		case spv::OpExtInst:                                  return "ExtInst";
		case spv::OpUndef:                                    return "Undef";
		case spv::OpLoad:                                     return "Load";
		case spv::OpStore:                                    return "Store";
		case spv::OpPhi:                                      return "Phi";
		case spv::OpDecorationGroup:                          return "DecorationGroup";
		case spv::OpDecorate:                                 return "Decorate";
		case spv::OpMemberDecorate:                           return "MemberDecorate";
		case spv::OpGroupDecorate:                            return "GroupDecorate";
		case spv::OpGroupMemberDecorate:                      return "GroupMemberDecorate";
		case spv::OpName:                                     return "Name";
		case spv::OpMemberName:                               return "MemberName";
		case spv::OpString:                                   return "String";
		case spv::OpLine:                                     return "Line";
		case spv::OpVectorExtractDynamic:                     return "VectorExtractDynamic";
		case spv::OpVectorInsertDynamic:                      return "VectorInsertDynamic";
		case spv::OpVectorShuffle:                            return "VectorShuffle";
		case spv::OpCompositeConstruct:                       return "CompositeConstruct";
		case spv::OpCompositeExtract:                         return "CompositeExtract";
		case spv::OpCompositeInsert:                          return "CompositeInsert";
		case spv::OpCopyObject:                               return "CopyObject";
		case spv::OpCopyMemory:                               return "CopyMemory";
		case spv::OpCopyMemorySized:                          return "CopyMemorySized";
		case spv::OpSampler:                                  return "Sampler";
		case spv::OpTextureSample:                            return "TextureSample";
		case spv::OpTextureSampleDref:                        return "TextureSampleDref";
		case spv::OpTextureSampleLod:                         return "TextureSampleLod";
		case spv::OpTextureSampleProj:                        return "TextureSampleProj";
		case spv::OpTextureSampleGrad:                        return "TextureSampleGrad";
		case spv::OpTextureSampleOffset:                      return "TextureSampleOffset";
		case spv::OpTextureSampleProjLod:                     return "TextureSampleProjLod";
		case spv::OpTextureSampleProjGrad:                    return "TextureSampleProjGrad";
		case spv::OpTextureSampleLodOffset:                   return "TextureSampleLodOffset";
		case spv::OpTextureSampleProjOffset:                  return "TextureSampleProjOffset";
		case spv::OpTextureSampleGradOffset:                  return "TextureSampleGradOffset";
		case spv::OpTextureSampleProjLodOffset:               return "TextureSampleProjLodOffset";
		case spv::OpTextureSampleProjGradOffset:              return "TextureSampleProjGradOffset";
		case spv::OpTextureFetchTexelLod:                     return "TextureFetchTexelLod";
		case spv::OpTextureFetchTexelOffset:                  return "TextureFetchTexelOffset";
		case spv::OpTextureFetchSample:                       return "TextureFetchSample";
		case spv::OpTextureFetchTexel:                        return "TextureFetchTexel";
		case spv::OpTextureGather:                            return "TextureGather";
		case spv::OpTextureGatherOffset:                      return "TextureGatherOffset";
		case spv::OpTextureGatherOffsets:                     return "TextureGatherOffsets";
		case spv::OpTextureQuerySizeLod:                      return "TextureQuerySizeLod";
		case spv::OpTextureQuerySize:                         return "TextureQuerySize";
		case spv::OpTextureQueryLod:                          return "TextureQueryLod";
		case spv::OpTextureQueryLevels:                       return "TextureQueryLevels";
		case spv::OpTextureQuerySamples:                      return "TextureQuerySamples";
		case spv::OpAccessChain:                              return "AccessChain";
		case spv::OpInBoundsAccessChain:                      return "InBoundsAccessChain";
		case spv::OpSNegate:                                  return "SNegate";
		case spv::OpFNegate:                                  return "FNegate";
		case spv::OpNot:                                      return "Not";
		case spv::OpAny:                                      return "Any";
		case spv::OpAll:                                      return "All";
		case spv::OpConvertFToU:                              return "ConvertFToU";
		case spv::OpConvertFToS:                              return "ConvertFToS";
		case spv::OpConvertSToF:                              return "ConvertSToF";
		case spv::OpConvertUToF:                              return "ConvertUToF";
		case spv::OpUConvert:                                 return "UConvert";
		case spv::OpSConvert:                                 return "SConvert";
		case spv::OpFConvert:                                 return "FConvert";
		case spv::OpConvertPtrToU:                            return "ConvertPtrToU";
		case spv::OpConvertUToPtr:                            return "ConvertUToPtr";
		case spv::OpPtrCastToGeneric:                         return "PtrCastToGeneric";
		case spv::OpGenericCastToPtr:                         return "GenericCastToPtr";
		case spv::OpBitcast:                                  return "Bitcast";
		case spv::OpTranspose:                                return "Transpose";
		case spv::OpIsNan:                                    return "IsNan";
		case spv::OpIsInf:                                    return "IsInf";
		case spv::OpIsFinite:                                 return "IsFinite";
		case spv::OpIsNormal:                                 return "IsNormal";
		case spv::OpSignBitSet:                               return "SignBitSet";
		case spv::OpLessOrGreater:                            return "LessOrGreater";
		case spv::OpOrdered:                                  return "Ordered";
		case spv::OpUnordered:                                return "Unordered";
		case spv::OpArrayLength:                              return "ArrayLength";
		case spv::OpIAdd:                                     return "IAdd";
		case spv::OpFAdd:                                     return "FAdd";
		case spv::OpISub:                                     return "ISub";
		case spv::OpFSub:                                     return "FSub";
		case spv::OpIMul:                                     return "IMul";
		case spv::OpFMul:                                     return "FMul";
		case spv::OpUDiv:                                     return "UDiv";
		case spv::OpSDiv:                                     return "SDiv";
		case spv::OpFDiv:                                     return "FDiv";
		case spv::OpUMod:                                     return "UMod";
		case spv::OpSRem:                                     return "SRem";
		case spv::OpSMod:                                     return "SMod";
		case spv::OpFRem:                                     return "FRem";
		case spv::OpFMod:                                     return "FMod";
		case spv::OpVectorTimesScalar:                        return "VectorTimesScalar";
		case spv::OpMatrixTimesScalar:                        return "MatrixTimesScalar";
		case spv::OpVectorTimesMatrix:                        return "VectorTimesMatrix";
		case spv::OpMatrixTimesVector:                        return "MatrixTimesVector";
		case spv::OpMatrixTimesMatrix:                        return "MatrixTimesMatrix";
		case spv::OpOuterProduct:                             return "OuterProduct";
		case spv::OpDot:                                      return "Dot";
		case spv::OpShiftRightLogical:                        return "ShiftRightLogical";
		case spv::OpShiftRightArithmetic:                     return "ShiftRightArithmetic";
		case spv::OpShiftLeftLogical:                         return "ShiftLeftLogical";
		case spv::OpLogicalOr:                                return "LogicalOr";
		case spv::OpLogicalXor:                               return "LogicalXor";
		case spv::OpLogicalAnd:                               return "LogicalAnd";
		case spv::OpBitwiseOr:                                return "BitwiseOr";
		case spv::OpBitwiseXor:                               return "BitwiseXor";
		case spv::OpBitwiseAnd:                               return "BitwiseAnd";
		case spv::OpSelect:                                   return "Select";
		case spv::OpIEqual:                                   return "IEqual";
		case spv::OpFOrdEqual:                                return "FOrdEqual";
		case spv::OpFUnordEqual:                              return "FUnordEqual";
		case spv::OpINotEqual:                                return "INotEqual";
		case spv::OpFOrdNotEqual:                             return "FOrdNotEqual";
		case spv::OpFUnordNotEqual:                           return "FUnordNotEqual";
		case spv::OpULessThan:                                return "ULessThan";
		case spv::OpSLessThan:                                return "SLessThan";
		case spv::OpFOrdLessThan:                             return "FOrdLessThan";
		case spv::OpFUnordLessThan:                           return "FUnordLessThan";
		case spv::OpUGreaterThan:                             return "UGreaterThan";
		case spv::OpSGreaterThan:                             return "SGreaterThan";
		case spv::OpFOrdGreaterThan:                          return "FOrdGreaterThan";
		case spv::OpFUnordGreaterThan:                        return "FUnordGreaterThan";
		case spv::OpULessThanEqual:                           return "ULessThanEqual";
		case spv::OpSLessThanEqual:                           return "SLessThanEqual";
		case spv::OpFOrdLessThanEqual:                        return "FOrdLessThanEqual";
		case spv::OpFUnordLessThanEqual:                      return "FUnordLessThanEqual";
		case spv::OpUGreaterThanEqual:                        return "UGreaterThanEqual";
		case spv::OpSGreaterThanEqual:                        return "SGreaterThanEqual";
		case spv::OpFOrdGreaterThanEqual:                     return "FOrdGreaterThanEqual";
		case spv::OpFUnordGreaterThanEqual:                   return "FUnordGreaterThanEqual";
		case spv::OpDPdx:                                     return "DPdx";
		case spv::OpDPdy:                                     return "DPdy";
		case spv::OpFwidth:                                   return "Fwidth";
		case spv::OpDPdxFine:                                 return "DPdxFine";
		case spv::OpDPdyFine:                                 return "DPdyFine";
		case spv::OpFwidthFine:                               return "FwidthFine";
		case spv::OpDPdxCoarse:                               return "DPdxCoarse";
		case spv::OpDPdyCoarse:                               return "DPdyCoarse";
		case spv::OpFwidthCoarse:                             return "FwidthCoarse";
		case spv::OpEmitVertex:                               return "EmitVertex";
		case spv::OpEndPrimitive:                             return "EndPrimitive";
		case spv::OpEmitStreamVertex:                         return "EmitStreamVertex";
		case spv::OpEndStreamPrimitive:                       return "EndStreamPrimitive";
		case spv::OpControlBarrier:                           return "ControlBarrier";
		case spv::OpMemoryBarrier:                            return "MemoryBarrier";
		case spv::OpImagePointer:                             return "ImagePointer";
		case spv::OpAtomicInit:                               return "AtomicInit";
		case spv::OpAtomicLoad:                               return "AtomicLoad";
		case spv::OpAtomicStore:                              return "AtomicStore";
		case spv::OpAtomicExchange:                           return "AtomicExchange";
		case spv::OpAtomicCompareExchange:                    return "AtomicCompareExchange";
		case spv::OpAtomicCompareExchangeWeak:                return "AtomicCompareExchangeWeak";
		case spv::OpAtomicIIncrement:                         return "AtomicIIncrement";
		case spv::OpAtomicIDecrement:                         return "AtomicIDecrement";
		case spv::OpAtomicIAdd:                               return "AtomicIAdd";
		case spv::OpAtomicISub:                               return "AtomicISub";
		case spv::OpAtomicUMin:                               return "AtomicUMin";
		case spv::OpAtomicUMax:                               return "AtomicUMax";
		case spv::OpAtomicAnd:                                return "AtomicAnd";
		case spv::OpAtomicOr:                                 return "AtomicOr";
		case spv::OpAtomicXor:                                return "AtomicXor";
		case spv::OpLoopMerge:                                return "LoopMerge";
		case spv::OpSelectionMerge:                           return "SelectionMerge";
		case spv::OpLabel:                                    return "Label";
		case spv::OpBranch:                                   return "Branch";
		case spv::OpBranchConditional:                        return "BranchConditional";
		case spv::OpSwitch:                                   return "Switch";
		case spv::OpKill:                                     return "Kill";
		case spv::OpReturn:                                   return "Return";
		case spv::OpReturnValue:                              return "ReturnValue";
		case spv::OpUnreachable:                              return "Unreachable";
		case spv::OpLifetimeStart:                            return "LifetimeStart";
		case spv::OpLifetimeStop:                             return "LifetimeStop";
		case spv::OpCompileFlag:                              return "CompileFlag";
		case spv::OpAsyncGroupCopy:                           return "AsyncGroupCopy";
		case spv::OpWaitGroupEvents:                          return "WaitGroupEvents";
		case spv::OpGroupAll:                                 return "GroupAll";
		case spv::OpGroupAny:                                 return "GroupAny";
		case spv::OpGroupBroadcast:                           return "GroupBroadcast";
		case spv::OpGroupIAdd:                                return "GroupIAdd";
		case spv::OpGroupFAdd:                                return "GroupFAdd";
		case spv::OpGroupFMin:                                return "GroupFMin";
		case spv::OpGroupUMin:                                return "GroupUMin";
		case spv::OpGroupSMin:                                return "GroupSMin";
		case spv::OpGroupFMax:                                return "GroupFMax";
		case spv::OpGroupUMax:                                return "GroupUMax";
		case spv::OpGroupSMax:                                return "GroupSMax";
		case spv::OpGenericCastToPtrExplicit:                 return "GenericCastToPtrExplicit";
		case spv::OpGenericPtrMemSemantics:                   return "GenericPtrMemSemantics";
		case spv::OpReadPipe:                                 return "ReadPipe";
		case spv::OpWritePipe:                                return "WritePipe";
		case spv::OpReservedReadPipe:                         return "ReservedReadPipe";
		case spv::OpReservedWritePipe:                        return "ReservedWritePipe";
		case spv::OpReserveReadPipePackets:                   return "ReserveReadPipePackets";
		case spv::OpReserveWritePipePackets:                  return "ReserveWritePipePackets";
		case spv::OpCommitReadPipe:                           return "CommitReadPipe";
		case spv::OpCommitWritePipe:                          return "CommitWritePipe";
		case spv::OpIsValidReserveId:                         return "IsValidReserveId";
		case spv::OpGetNumPipePackets:                        return "GetNumPipePackets";
		case spv::OpGetMaxPipePackets:                        return "GetMaxPipePackets";
		case spv::OpGroupReserveReadPipePackets:              return "GroupReserveReadPipePackets";
		case spv::OpGroupReserveWritePipePackets:             return "GroupReserveWritePipePackets";
		case spv::OpGroupCommitReadPipe:                      return "GroupCommitReadPipe";
		case spv::OpGroupCommitWritePipe:                     return "GroupCommitWritePipe";
		case spv::OpEnqueueMarker:                            return "EnqueueMarker";
		case spv::OpEnqueueKernel:                            return "EnqueueKernel";
		case spv::OpGetKernelNDrangeSubGroupCount:            return "GetKernelNDrangeSubGroupCount";
		case spv::OpGetKernelNDrangeMaxSubGroupSize:          return "GetKernelNDrangeMaxSubGroupSize";
		case spv::OpGetKernelWorkGroupSize:                   return "GetKernelWorkGroupSize";
		case spv::OpGetKernelPreferredWorkGroupSizeMultiple:  return "GetKernelPreferredWorkGroupSizeMultiple";
		case spv::OpRetainEvent:                              return "RetainEvent";
		case spv::OpReleaseEvent:                             return "ReleaseEvent";
		case spv::OpCreateUserEvent:                          return "CreateUserEvent";
		case spv::OpIsValidEvent:                             return "IsValidEvent";
		case spv::OpSetUserEventStatus:                       return "SetUserEventStatus";
		case spv::OpCaptureEventProfilingInfo:                return "CaptureEventProfilingInfo";
		case spv::OpGetDefaultQueue:                          return "GetDefaultQueue";
		case spv::OpBuildNDRange:                             return "BuildNDRange";
		case spv::OpSatConvertSToU:                           return "SatConvertSToU";
		case spv::OpSatConvertUToS:                           return "SatConvertUToS";
		case spv::OpAtomicIMin:                               return "AtomicIMin";
		case spv::OpAtomicIMax:                               return "AtomicIMax";
		default: break;
	}
#endif

	return StringFormat::Fmt("Unrecognised{%u}", (uint32_t)el);
}

template<>
string ToStrHelper<false, spv::SourceLanguage>::Get(const spv::SourceLanguage &el)
{
	switch(el)
	{
		case spv::SourceLanguageUnknown:    return "Unknown";
		case spv::SourceLanguageESSL:       return "ESSL";
		case spv::SourceLanguageGLSL:       return "GLSL";
		case spv::SourceLanguageOpenCL_C:   return "OpenCL C";
		case spv::SourceLanguageOpenCL_CPP: return "OpenCL C++";
		default: break;
	}
	
	return "Unrecognised";
}

template<>
string ToStrHelper<false, spv::AddressingModel>::Get(const spv::AddressingModel &el)
{
	switch(el)
	{
		case spv::AddressingModelLogical:    return "Logical";
		case spv::AddressingModelPhysical32: return "Physical (32-bit)";
		case spv::AddressingModelPhysical64: return "Physical (64-bit)";
		default: break;
	}
	
	return "Unrecognised";
}

template<>
string ToStrHelper<false, spv::MemoryModel>::Get(const spv::MemoryModel &el)
{
	switch(el)
	{
		case spv::MemoryModelSimple:   return "Simple";
		case spv::MemoryModelGLSL450:  return "GLSL450";
		case spv::MemoryModelOpenCL:   return "OpenCL";
		default: break;
	}
	
	return "Unrecognised";
}

template<>
string ToStrHelper<false, spv::ExecutionModel>::Get(const spv::ExecutionModel &el)
{
	switch(el)
	{
		case spv::ExecutionModelVertex:    return "Vertex Shader";
		case spv::ExecutionModelTessellationControl: return "Tess. Control Shader";
		case spv::ExecutionModelTessellationEvaluation: return "Tess. Eval Shader";
		case spv::ExecutionModelGeometry:  return "Geometry Shader";
		case spv::ExecutionModelFragment:  return "Fragment Shader";
		case spv::ExecutionModelGLCompute: return "Compute Shader";
		case spv::ExecutionModelKernel:    return "Kernel";
		default: break;
	}
	
	return "Unrecognised";
}

template<>
string ToStrHelper<false, spv::Decoration>::Get(const spv::Decoration &el)
{
#if 0
	switch(el)
	{
    case spv::DecorationPrecisionLow:         return "PrecisionLow";
    case spv::DecorationPrecisionMedium:      return "PrecisionMedium";
    case spv::DecorationPrecisionHigh:        return "PrecisionHigh";
    case spv::DecorationBlock:                return "Block";
    case spv::DecorationBufferBlock:          return "BufferBlock";
    case spv::DecorationRowMajor:             return "RowMajor";
    case spv::DecorationColMajor:             return "ColMajor";
    case spv::DecorationGLSLShared:           return "GLSLShared";
    case spv::DecorationGLSLStd140:           return "GLSLStd140";
    case spv::DecorationGLSLStd430:           return "GLSLStd430";
    case spv::DecorationGLSLPacked:           return "GLSLPacked";
    case spv::DecorationSmooth:               return "Smooth";
    case spv::DecorationNoperspective:        return "Noperspective";
    case spv::DecorationFlat:                 return "Flat";
    case spv::DecorationPatch:                return "Patch";
    case spv::DecorationCentroid:             return "Centroid";
    case spv::DecorationSample:               return "Sample";
    case spv::DecorationInvariant:            return "Invariant";
    case spv::DecorationRestrict:             return "Restrict";
    case spv::DecorationAliased:              return "Aliased";
    case spv::DecorationVolatile:             return "Volatile";
    case spv::DecorationConstant:             return "Constant";
    case spv::DecorationCoherent:             return "Coherent";
    case spv::DecorationNonwritable:          return "Nonwritable";
    case spv::DecorationNonreadable:          return "Nonreadable";
    case spv::DecorationUniform:              return "Uniform";
    case spv::DecorationNoStaticUse:          return "NoStaticUse";
    case spv::DecorationCPacked:              return "CPacked";
    case spv::DecorationSaturatedConversion:  return "SaturatedConversion";
    case spv::DecorationStream:               return "Stream";
    case spv::DecorationLocation:             return "Location";
    case spv::DecorationComponent:            return "Component";
    case spv::DecorationIndex:                return "Index";
    case spv::DecorationBinding:              return "Binding";
    case spv::DecorationDescriptorSet:        return "DescriptorSet";
    case spv::DecorationOffset:               return "Offset";
    case spv::DecorationAlignment:            return "Alignment";
    case spv::DecorationXfbBuffer:            return "XfbBuffer";
    case spv::DecorationStride:               return "Stride";
    case spv::DecorationBuiltIn:              return "BuiltIn";
    case spv::DecorationFuncParamAttr:        return "FuncParamAttr";
    case spv::DecorationFPRoundingMode:       return "FPRoundingMode";
    case spv::DecorationFPFastMathMode:       return "FPFastMathMode";
    case spv::DecorationLinkageAttributes:    return "LinkageAttributes";
    case spv::DecorationSpecId:               return "SpecId";
		default: break;
	}
#endif
	
	return "Unrecognised";
}

template<>
string ToStrHelper<false, spv::StorageClass>::Get(const spv::StorageClass &el)
{
#if 0
	switch(el)
	{
    case spv::StorageClassUniformConstant:    return "UniformConstant";
    case spv::StorageClassInput:              return "Input";
    case spv::StorageClassUniform:            return "Uniform";
    case spv::StorageClassOutput:             return "Output";
    case spv::StorageClassWorkgroupLocal:     return "WorkgroupLocal";
    case spv::StorageClassWorkgroupGlobal:    return "WorkgroupGlobal";
    case spv::StorageClassPrivateGlobal:      return "PrivateGlobal";
    case spv::StorageClassFunction:           return "Function";
    case spv::StorageClassGeneric:            return "Generic";
    case spv::StorageClassPrivate:            return "Private";
    case spv::StorageClassAtomicCounter:      return "AtomicCounter";
		default: break;
	}
#endif
	
	return "Unrecognised";
}

template<>
string ToStrHelper<false, spv::FunctionControlMask>::Get(const spv::FunctionControlMask &el)
{
	string ret;

	if(el & spv::FunctionControlInlineMask)     ret += ", Inline";
	if(el & spv::FunctionControlDontInlineMask) ret += ", DontInline";
	if(el & spv::FunctionControlPureMask)       ret += ", Pure";
	if(el & spv::FunctionControlConstMask)      ret += ", Const";
	
	if(!ret.empty())
		ret = ret.substr(2);

	return ret;
}

template<>
string ToStrHelper<false, spv::SelectionControlMask>::Get(const spv::SelectionControlMask &el)
{
	string ret;

	if(el & spv::SelectionControlFlattenMask)     ret += ", Flatten";
	if(el & spv::SelectionControlDontFlattenMask) ret += ", DontFlatten";
	
	if(!ret.empty())
		ret = ret.substr(2);

	return ret;
}

template<>
string ToStrHelper<false, spv::LoopControlMask>::Get(const spv::LoopControlMask &el)
{
	string ret;

	if(el & spv::LoopControlUnrollMask)     ret += ", Unroll";
	if(el & spv::LoopControlDontUnrollMask) ret += ", DontUnroll";
	
	if(!ret.empty())
		ret = ret.substr(2);

	return ret;
}

template<>
string ToStrHelper<false, spv::MemoryAccessMask>::Get(const spv::MemoryAccessMask &el)
{
	string ret;
	
	if(el & spv::MemoryAccessVolatileMask)     ret += ", Volatile";
	if(el & spv::MemoryAccessAlignedMask) ret += ", Aligned";
	
	if(!ret.empty())
		ret = ret.substr(2);

	return ret;
}
