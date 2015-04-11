//
//Copyright (C) 2014 LunarG, Inc.
//
//All rights reserved.
//
//Redistribution and use in source and binary forms, with or without
//modification, are permitted provided that the following conditions
//are met:
//
//    Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above
//    copyright notice, this list of conditions and the following
//    disclaimer in the documentation and/or other materials provided
//    with the distribution.
//
//    Neither the name of 3Dlabs Inc. Ltd. nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
//THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
//FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
//COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
//BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
//LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
//CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
//LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
//ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//POSSIBILITY OF SUCH DAMAGE.

//
// Author: John Kessenich, LunarG
//
// Visit the nodes in the glslang intermediate tree representation to
// translate them to SPIR-V.
//

#include "spirv.h"
#include "GlslangToSpv.h"
#include "SpvBuilder.h"
#include "GLSL450Lib.h"

// Glslang includes
#include "../glslang/MachineIndependent/localintermediate.h"
#include "../glslang/MachineIndependent/SymbolTable.h"

#include <string>
#include <map>
#include <list>
#include <vector>
#include <stack>
#include <fstream>

namespace {

const int GlslangMagic = 0x51a;

//
// The main holder of information for translating glslang to SPIR-V.
//
// Derives from the AST walking base class.
//
class TGlslangToSpvTraverser : public glslang::TIntermTraverser {
public:
    TGlslangToSpvTraverser(const glslang::TIntermediate*);
    virtual ~TGlslangToSpvTraverser();

    bool visitAggregate(glslang::TVisit, glslang::TIntermAggregate*);
    bool visitBinary(glslang::TVisit, glslang::TIntermBinary*);
    void visitConstantUnion(glslang::TIntermConstantUnion*);
    bool visitSelection(glslang::TVisit, glslang::TIntermSelection*);
    bool visitSwitch(glslang::TVisit, glslang::TIntermSwitch*);
    void visitSymbol(glslang::TIntermSymbol* symbol);
    bool visitUnary(glslang::TVisit, glslang::TIntermUnary*);
    bool visitLoop(glslang::TVisit, glslang::TIntermLoop*);
    bool visitBranch(glslang::TVisit visit, glslang::TIntermBranch*);

    void dumpSpv(std::vector<unsigned int>& out) { builder.dump(out); }

protected:
    spv::Id createSpvVariable(const glslang::TIntermSymbol*);
    spv::Id getSampledType(const glslang::TSampler&);
    spv::Id convertGlslangToSpvType(const glslang::TType& type);

    bool isShaderEntrypoint(const glslang::TIntermAggregate* node);
    void makeFunctions(const glslang::TIntermSequence&);
    void makeGlobalInitializers(const glslang::TIntermSequence&);
    void visitFunctions(const glslang::TIntermSequence&);
    void handleFunctionEntry(const glslang::TIntermAggregate* node);
    void translateArguments(const glslang::TIntermSequence& glslangArguments, std::vector<spv::Id>& arguments);
    spv::Id handleBuiltInFunctionCall(const glslang::TIntermAggregate*);
    spv::Id handleUserFunctionCall(const glslang::TIntermAggregate*);

    spv::Id createBinaryOperation(glslang::TOperator op, spv::Decoration precision, spv::Id typeId, spv::Id left, spv::Id right, glslang::TBasicType typeProxy, bool reduceComparison = true);
    spv::Id createUnaryOperation(glslang::TOperator op, spv::Decoration precision, spv::Id typeId, spv::Id operand, bool isFloat);
    spv::Id createConversion(glslang::TOperator op, spv::Decoration precision, spv::Id destTypeId, spv::Id operand);
    spv::Id makeSmearedConstant(spv::Id constant, int vectorSize);
    spv::Id createMiscOperation(glslang::TOperator op, spv::Decoration precision, spv::Id typeId, std::vector<spv::Id>& operands);
    spv::Id createNoArgOperation(glslang::TOperator op);
    spv::Id getSymbolId(const glslang::TIntermSymbol* node);
    void addDecoration(spv::Id id, spv::Decoration dec);
    void addMemberDecoration(spv::Id id, int member, spv::Decoration dec);
    spv::Id createSpvConstant(const glslang::TType& type, const glslang::TConstUnionArray&, int& nextConst);

    spv::Function* shaderEntry;
    int sequenceDepth;

    // There is a 1:1 mapping between a spv builder and a module; this is thread safe
    spv::Builder builder;
    bool inMain;
    bool mainTerminated;
    bool linkageOnly;
    const glslang::TIntermediate* glslangIntermediate;
    spv::Id stdBuiltins;

    std::map<int, spv::Id> symbolValues;
    std::set<int> constReadOnlyParameters;  // set of formal function parameters that have glslang qualifier constReadOnly, so we know they are not local function "const" that are write-once
    std::map<std::string, spv::Function*> functionMap;
    std::map<const glslang::TTypeList*, spv::Id> structMap;
    std::map<const glslang::TTypeList*, std::vector<int> > memberRemapper;  // for mapping glslang block indices to spv indices (e.g., due to hidden members)
    std::stack<bool> breakForLoop;  // false means break for switch
    std::stack<glslang::TIntermTyped*> loopTerminal;  // code from the last part of a for loop: for(...; ...; terminal), needed for e.g., continue };
};

//
// Helper functions for translating glslang representations to SPIR-V enumerants.
//

// Translate glslang profile to SPIR-V source language.
spv::SourceLanguage TranslateSourceLanguage(EProfile profile)
{
    switch (profile) {
    case ENoProfile:
    case ECoreProfile:
    case ECompatibilityProfile:
        return spv::SourceLanguageGLSL;
    case EEsProfile:
        return spv::SourceLanguageESSL;
    default:
        return spv::SourceLanguageUnknown;
    }
}

// Translate glslang language (stage) to SPIR-V execution model.
spv::ExecutionModel TranslateExecutionModel(EShLanguage stage)
{
    switch (stage) {
    case EShLangVertex:           return spv::ExecutionModelVertex;
    case EShLangTessControl:      return spv::ExecutionModelTessellationControl;
    case EShLangTessEvaluation:   return spv::ExecutionModelTessellationEvaluation;
    case EShLangGeometry:         return spv::ExecutionModelGeometry;
    case EShLangFragment:         return spv::ExecutionModelFragment;
    case EShLangCompute:          return spv::ExecutionModelGLCompute;
    default:
        spv::MissingFunctionality("GLSL stage");
        return spv::ExecutionModelFragment;
    }
}

// Translate glslang type to SPIR-V storage class.
spv::StorageClass TranslateStorageClass(const glslang::TType& type)
{
    if (type.getQualifier().isPipeInput())
        return spv::StorageClassInput;
    else if (type.getQualifier().isPipeOutput())
        return spv::StorageClassOutput;
    else if (type.getQualifier().isUniformOrBuffer()) {
        if (type.getBasicType() == glslang::EbtBlock)
            return spv::StorageClassUniform;
        else
            return spv::StorageClassUniformConstant;
        // TODO: how are we distuingishing between default and non-default non-writable uniforms?  Do default uniforms even exist?
    } else {
        switch (type.getQualifier().storage) {
        case glslang::EvqShared:        return spv::StorageClassWorkgroupLocal;  break;
        case glslang::EvqGlobal:        return spv::StorageClassPrivateGlobal;
        case glslang::EvqConstReadOnly: return spv::StorageClassFunction;
        case glslang::EvqTemporary:     return spv::StorageClassFunction;
        default: 
            spv::MissingFunctionality("unknown glslang storage class");
            return spv::StorageClassFunction;
        }
    }
}

// Translate glslang sampler type to SPIR-V dimensionality.
spv::Dim TranslateDimensionality(const glslang::TSampler& sampler)
{
    switch (sampler.dim) {
    case glslang::Esd1D:     return spv::Dim1D;
    case glslang::Esd2D:     return spv::Dim2D;
    case glslang::Esd3D:     return spv::Dim3D;
    case glslang::EsdCube:   return spv::DimCube;
    case glslang::EsdRect:   return spv::DimRect;
    case glslang::EsdBuffer: return spv::DimBuffer;
    default:
        spv::MissingFunctionality("unknown sampler dimension");
        return spv::Dim2D;
    }
}

// Translate glslang type to SPIR-V precision decorations.
spv::Decoration TranslatePrecisionDecoration(const glslang::TType& type)
{
    switch (type.getQualifier().precision) {
    case glslang::EpqLow:    return spv::DecorationPrecisionLow;
    case glslang::EpqMedium: return spv::DecorationPrecisionMedium;
    case glslang::EpqHigh:   return spv::DecorationPrecisionHigh;
    default:
        return spv::NoPrecision;
    }
}

// Translate glslang type to SPIR-V block decorations.
spv::Decoration TranslateBlockDecoration(const glslang::TType& type)
{
    if (type.getBasicType() == glslang::EbtBlock) {
        switch (type.getQualifier().storage) {
        case glslang::EvqUniform:      return spv::DecorationBlock;
        case glslang::EvqBuffer:       return spv::DecorationBufferBlock;
        case glslang::EvqVaryingIn:    return spv::DecorationBlock;
        case glslang::EvqVaryingOut:   return spv::DecorationBlock;
        default:
            spv::MissingFunctionality("kind of block");
            break;
        }
    }

    return (spv::Decoration)spv::BadValue;
}

// Translate glslang type to SPIR-V layout decorations.
spv::Decoration TranslateLayoutDecoration(const glslang::TType& type)
{
    if (type.isMatrix()) {
        switch (type.getQualifier().layoutMatrix) {
        case glslang::ElmRowMajor:
            return spv::DecorationRowMajor;
        default:
            return spv::DecorationColMajor;
        }
    } else {
        switch (type.getBasicType()) {
        default:
            return (spv::Decoration)spv::BadValue;
            break;
        case glslang::EbtBlock:
            switch (type.getQualifier().storage) {
            case glslang::EvqUniform:
            case glslang::EvqBuffer:
                switch (type.getQualifier().layoutPacking) {
                case glslang::ElpShared:  return spv::DecorationGLSLShared;
                case glslang::ElpStd140:  return spv::DecorationGLSLStd140;
                case glslang::ElpStd430:  return spv::DecorationGLSLStd430;
                case glslang::ElpPacked:  return spv::DecorationGLSLPacked;
                default:
                    spv::MissingFunctionality("uniform block layout");
                    return spv::DecorationGLSLShared;
                }
            case glslang::EvqVaryingIn:
            case glslang::EvqVaryingOut:
                if (type.getQualifier().layoutPacking != glslang::ElpNone)
                    spv::MissingFunctionality("in/out block layout");
                return (spv::Decoration)spv::BadValue;
            default:
                spv::MissingFunctionality("block storage qualification");
                return (spv::Decoration)spv::BadValue;
            }
        }
    }
}

// Translate glslang type to SPIR-V interpolation decorations.
spv::Decoration TranslateInterpolationDecoration(const glslang::TType& type)
{
    if (type.getQualifier().smooth)
        return spv::DecorationSmooth;
    if (type.getQualifier().nopersp)
        return spv::DecorationNoperspective;
    else if (type.getQualifier().patch)
        return spv::DecorationPatch;
    else if (type.getQualifier().flat)
        return spv::DecorationFlat;
    else if (type.getQualifier().centroid)
        return spv::DecorationCentroid;
    else if (type.getQualifier().sample)
        return spv::DecorationSample;
    else
        return (spv::Decoration)spv::BadValue;
}

// If glslang type is invaraiant, return SPIR-V invariant decoration.
spv::Decoration TranslateInvariantDecoration(const glslang::TType& type)
{
    if (type.getQualifier().invariant)
        return spv::DecorationInvariant;
    else
        return (spv::Decoration)spv::BadValue;
}

// Translate glslang built-in variable to SPIR-V built in decoration.
spv::BuiltIn TranslateBuiltInDecoration(glslang::TBuiltInVariable builtIn)
{
    switch (builtIn) {
    case glslang::EbvPosition:             return spv::BuiltInPosition;
    case glslang::EbvPointSize:            return spv::BuiltInPointSize;
    case glslang::EbvClipVertex:           return spv::BuiltInClipVertex;
    case glslang::EbvClipDistance:         return spv::BuiltInClipDistance;
    case glslang::EbvCullDistance:         return spv::BuiltInCullDistance;
    case glslang::EbvVertexId:             return spv::BuiltInVertexId;
    case glslang::EbvInstanceId:           return spv::BuiltInInstanceId;
    case glslang::EbvPrimitiveId:          return spv::BuiltInPrimitiveId;
    case glslang::EbvInvocationId:         return spv::BuiltInInvocationId;
    case glslang::EbvLayer:                return spv::BuiltInLayer;
    case glslang::EbvViewportIndex:        return spv::BuiltInViewportIndex;
    case glslang::EbvTessLevelInner:       return spv::BuiltInTessLevelInner;
    case glslang::EbvTessLevelOuter:       return spv::BuiltInTessLevelOuter;
    case glslang::EbvTessCoord:            return spv::BuiltInTessCoord;
    case glslang::EbvPatchVertices:        return spv::BuiltInPatchVertices;
    case glslang::EbvFragCoord:            return spv::BuiltInFragCoord;
    case glslang::EbvPointCoord:           return spv::BuiltInPointCoord;
    case glslang::EbvFace:                 return spv::BuiltInFrontFacing;
    case glslang::EbvSampleId:             return spv::BuiltInSampleId;
    case glslang::EbvSamplePosition:       return spv::BuiltInSamplePosition;
    case glslang::EbvSampleMask:           return spv::BuiltInSampleMask;
    case glslang::EbvFragColor:            return spv::BuiltInFragColor;
    case glslang::EbvFragData:             return spv::BuiltInFragColor;
    case glslang::EbvFragDepth:            return spv::BuiltInFragDepth;
    case glslang::EbvHelperInvocation:     return spv::BuiltInHelperInvocation;
    case glslang::EbvNumWorkGroups:        return spv::BuiltInNumWorkgroups;
    case glslang::EbvWorkGroupSize:        return spv::BuiltInWorkgroupSize;
    case glslang::EbvWorkGroupId:          return spv::BuiltInWorkgroupId;
    case glslang::EbvLocalInvocationId:    return spv::BuiltInLocalInvocationId;
    case glslang::EbvLocalInvocationIndex: return spv::BuiltInLocalInvocationIndex;
    case glslang::EbvGlobalInvocationId:   return spv::BuiltInGlobalInvocationId;
    default:                               return (spv::BuiltIn)spv::BadValue;
    }
}

//
// Implement the TGlslangToSpvTraverser class.
//

TGlslangToSpvTraverser::TGlslangToSpvTraverser(const glslang::TIntermediate* glslangIntermediate)
    : TIntermTraverser(true, false, true), shaderEntry(0), sequenceDepth(0),
      builder(GlslangMagic),
      inMain(false), mainTerminated(false), linkageOnly(false),
      glslangIntermediate(glslangIntermediate)
{
    spv::ExecutionModel executionModel = TranslateExecutionModel(glslangIntermediate->getStage());

    builder.clearAccessChain();
    builder.setSource(TranslateSourceLanguage(glslangIntermediate->getProfile()), glslangIntermediate->getVersion());
    stdBuiltins = builder.import("GLSL.std.450");
    builder.setMemoryModel(spv::AddressingModelLogical, spv::MemoryModelGLSL450);
    shaderEntry = builder.makeMain();
    builder.addEntryPoint(executionModel, shaderEntry);

    // Add the source extensions
    const std::set<std::string>& sourceExtensions = glslangIntermediate->getRequestedExtensions();
    for (std::set<std::string>::const_iterator it = sourceExtensions.begin(); it != sourceExtensions.end(); ++it)
        builder.addSourceExtension(it->c_str());

    // Add the top-level modes for this shader.

    if (glslangIntermediate->getXfbMode())
        builder.addExecutionMode(shaderEntry, spv::ExecutionModeXfb);

    unsigned int mode;
    switch (glslangIntermediate->getStage()) {
    case EShLangVertex:
        break;

    case EShLangTessControl:
        builder.addExecutionMode(shaderEntry, spv::ExecutionModeOutputVertices, glslangIntermediate->getVertices());
        break;

    case EShLangTessEvaluation:
        switch (glslangIntermediate->getInputPrimitive()) {
        case glslang::ElgTriangles:           mode = spv::ExecutionModeInputTriangles;     break;
        case glslang::ElgQuads:               mode = spv::ExecutionModeInputQuads;         break;
        case glslang::ElgIsolines:            mode = spv::ExecutionModeInputIsolines;      break;
        default:                              mode = spv::BadValue;    break;
        }
        if (mode != spv::BadValue)
            builder.addExecutionMode(shaderEntry, (spv::ExecutionMode)mode);

        // TODO
        //builder.addExecutionMode(spv::VertexSpacingMdName, glslangIntermediate->getVertexSpacing());
        //builder.addExecutionMode(spv::VertexOrderMdName, glslangIntermediate->getVertexOrder());
        //builder.addExecutionMode(spv::PointModeMdName, glslangIntermediate->getPointMode());
        break;

    case EShLangGeometry:
        switch (glslangIntermediate->getInputPrimitive()) {
        case glslang::ElgPoints:             mode = spv::ExecutionModeInputPoints;             break;
        case glslang::ElgLines:              mode = spv::ExecutionModeInputLines;              break;
        case glslang::ElgLinesAdjacency:     mode = spv::ExecutionModeInputLinesAdjacency;     break;
        case glslang::ElgTriangles:          mode = spv::ExecutionModeInputTriangles;          break;
        case glslang::ElgTrianglesAdjacency: mode = spv::ExecutionModeInputTrianglesAdjacency; break;
        default:                             mode = spv::BadValue;         break;
        }
        if (mode != spv::BadValue)
            builder.addExecutionMode(shaderEntry, (spv::ExecutionMode)mode);
        builder.addExecutionMode(shaderEntry, spv::ExecutionModeInvocations, glslangIntermediate->getInvocations());

        switch (glslangIntermediate->getOutputPrimitive()) {
        case glslang::ElgPoints:        mode = spv::ExecutionModeOutputPoints;                 break;
        case glslang::ElgLineStrip:     mode = spv::ExecutionModeOutputLineStrip;              break;
        case glslang::ElgTriangleStrip: mode = spv::ExecutionModeOutputTriangleStrip;          break;
        default:                        mode = spv::BadValue;              break;
        }
        if (mode != spv::BadValue)
            builder.addExecutionMode(shaderEntry, (spv::ExecutionMode)mode);
        builder.addExecutionMode(shaderEntry, spv::ExecutionModeOutputVertices, glslangIntermediate->getVertices());
        break;

    case EShLangFragment:
        if (glslangIntermediate->getPixelCenterInteger())
            builder.addExecutionMode(shaderEntry, spv::ExecutionModePixelCenterInteger);
        if (glslangIntermediate->getOriginUpperLeft())
            builder.addExecutionMode(shaderEntry, spv::ExecutionModeOriginUpperLeft);
        break;

    case EShLangCompute:
        break;

    default:
        break;
    }

}

TGlslangToSpvTraverser::~TGlslangToSpvTraverser()
{
    if (! mainTerminated) {
        spv::Block* lastMainBlock = shaderEntry->getLastBlock();
        builder.setBuildPoint(lastMainBlock);
        builder.leaveFunction(true);
    }
}

//
// Implement the traversal functions.
//
// Return true from interior nodes to have the external traversal
// continue on to children.  Return false if children were
// already processed.
//

//
// Symbols can turn into 
//  - uniform/input reads
//  - output writes
//  - complex lvalue base setups:  foo.bar[3]....  , where we see foo and start up an access chain
//  - something simple that degenerates into the last bullet
//
void TGlslangToSpvTraverser::visitSymbol(glslang::TIntermSymbol* symbol)
{
    // getSymbolId() will set up all the IO decorations on the first call.
    // Formal function parameters were mapped during makeFunctions().
    spv::Id id = getSymbolId(symbol);
    
    if (! linkageOnly) {
        // Prepare to generate code for the access

        // L-value chains will be computed left to right.  We're on the symbol now,
        // which is the left-most part of the access chain, so now is "clear" time,
        // followed by setting the base.
        builder.clearAccessChain();

        // For now, we consider all user variables as being in memory, so they are pointers,
        // except for "const in" arguments to a function, which are an intermediate object.
        // See comments in handleUserFunctionCall().
        glslang::TStorageQualifier qualifier = symbol->getQualifier().storage;
        if (qualifier == glslang::EvqConstReadOnly && constReadOnlyParameters.find(symbol->getId()) != constReadOnlyParameters.end())
            builder.setAccessChainRValue(id);
        else
            builder.setAccessChainLValue(id);
    }
}

bool TGlslangToSpvTraverser::visitBinary(glslang::TVisit /* visit */, glslang::TIntermBinary* node)
{
    // First, handle special cases
    switch (node->getOp()) {
    case glslang::EOpAssign:
    case glslang::EOpAddAssign:
    case glslang::EOpSubAssign:
    case glslang::EOpMulAssign:
    case glslang::EOpVectorTimesMatrixAssign:
    case glslang::EOpVectorTimesScalarAssign:
    case glslang::EOpMatrixTimesScalarAssign:
    case glslang::EOpMatrixTimesMatrixAssign:
    case glslang::EOpDivAssign:
    case glslang::EOpModAssign:
    case glslang::EOpAndAssign:
    case glslang::EOpInclusiveOrAssign:
    case glslang::EOpExclusiveOrAssign:
    case glslang::EOpLeftShiftAssign:
    case glslang::EOpRightShiftAssign:
        // A bin-op assign "a += b" means the same thing as "a = a + b"
        // where a is evaluated before b. For a simple assignment, GLSL
        // says to evaluate the left before the right.  So, always, left
        // node then right node.
        {
            // get the left l-value, save it away
            builder.clearAccessChain();
            node->getLeft()->traverse(this);
            spv::Builder::AccessChain lValue = builder.getAccessChain();

            // evaluate the right
            builder.clearAccessChain();
            node->getRight()->traverse(this);
            spv::Id rValue = builder.accessChainLoad(TranslatePrecisionDecoration(node->getRight()->getType()));

            if (node->getOp() != glslang::EOpAssign) {
                // the left is also an r-value
                builder.setAccessChain(lValue);
                spv::Id leftRValue = builder.accessChainLoad(TranslatePrecisionDecoration(node->getLeft()->getType()));

                // do the operation
                rValue = createBinaryOperation(node->getOp(), TranslatePrecisionDecoration(node->getType()), 
                                               convertGlslangToSpvType(node->getType()), leftRValue, rValue,
                                               node->getType().getBasicType());

                // these all need their counterparts in createBinaryOperation()
                if (rValue == 0)
                    spv::MissingFunctionality("createBinaryOperation");
            }

            // store the result
            builder.setAccessChain(lValue);
            builder.accessChainStore(rValue);

            // assignments are expressions having an rValue after they are evaluated...
            builder.clearAccessChain();
            builder.setAccessChainRValue(rValue);
        }
        return false;
    case glslang::EOpIndexDirect:
    case glslang::EOpIndexDirectStruct:
        {
            // Get the left part of the access chain.
            node->getLeft()->traverse(this);

            // Add the next element in the chain

            int index = 0;
            if (node->getRight()->getAsConstantUnion() == 0)
                spv::MissingFunctionality("direct index without a constant node");
            else 
                index = node->getRight()->getAsConstantUnion()->getConstArray()[0].getIConst();

            if (node->getLeft()->getBasicType() == glslang::EbtBlock && node->getOp() == glslang::EOpIndexDirectStruct) {
                // This may be, e.g., an anonymous block-member selection, which generally need
                // index remapping due to hidden members in anonymous blocks.
                std::vector<int>& remapper = memberRemapper[node->getLeft()->getType().getStruct()];
                if (remapper.size() == 0)
                    spv::MissingFunctionality("block without member remapping");
                else
                    index = remapper[index];
            }

            if (! node->getLeft()->getType().isArray() &&
                node->getLeft()->getType().isVector() &&
                node->getOp() == glslang::EOpIndexDirect) {
                // This is essentially a hard-coded vector swizzle of size 1,
                // so short circuit the access-chain stuff with a swizzle.
                std::vector<unsigned> swizzle;
                swizzle.push_back(node->getRight()->getAsConstantUnion()->getConstArray()[0].getIConst());
                builder.accessChainPushSwizzle(swizzle);
            } else {
                // normal case for indexing array or structure or block
                builder.accessChainPush(builder.makeIntConstant(index), convertGlslangToSpvType(node->getType()));
            }
        }
        return false;
    case glslang::EOpIndexIndirect:
        {
            // Structure or array or vector indirection.
            // Will use native SPIR-V access-chain for struct and array indirection;
            // matrices are arrays of vectors, so will also work for a matrix.
            // Will use the access chain's 'component' for variable index into a vector.

            // This adapter is building access chains left to right.
            // Set up the access chain to the left.
            node->getLeft()->traverse(this);

            // save it so that computing the right side doesn't trash it
            spv::Builder::AccessChain partial = builder.getAccessChain();

            // compute the next index in the chain
            builder.clearAccessChain();
            node->getRight()->traverse(this);
            spv::Id index = builder.accessChainLoad(TranslatePrecisionDecoration(node->getRight()->getType()));

            // restore the saved access chain
            builder.setAccessChain(partial);

            if (! node->getLeft()->getType().isArray() && node->getLeft()->getType().isVector())
                builder.accessChainPushComponent(index);
            else
                builder.accessChainPush(index, convertGlslangToSpvType(node->getType()));
        }
        return false;
    case glslang::EOpVectorSwizzle:
        {
            node->getLeft()->traverse(this);
            glslang::TIntermSequence& swizzleSequence = node->getRight()->getAsAggregate()->getSequence();
            std::vector<unsigned> swizzle;
            for (int i = 0; i < (int)swizzleSequence.size(); ++i)
                swizzle.push_back(swizzleSequence[i]->getAsConstantUnion()->getConstArray()[0].getIConst());
            builder.accessChainPushSwizzle(swizzle);
        }
        return false;
    default:
        break;
    }

    // Assume generic binary op...

    // Get the operands
    builder.clearAccessChain();
    node->getLeft()->traverse(this);
    spv::Id left = builder.accessChainLoad(TranslatePrecisionDecoration(node->getLeft()->getType()));

    builder.clearAccessChain();
    node->getRight()->traverse(this);
    spv::Id right = builder.accessChainLoad(TranslatePrecisionDecoration(node->getRight()->getType()));

    spv::Id result;
    spv::Decoration precision = TranslatePrecisionDecoration(node->getType());

    result = createBinaryOperation(node->getOp(), precision, 
                                   convertGlslangToSpvType(node->getType()), left, right,
                                   node->getLeft()->getType().getBasicType());

    if (! result) {
        spv::MissingFunctionality("glslang binary operation");
    } else {
        builder.clearAccessChain();
        builder.setAccessChainRValue(result);

        return false;
    }

    return true;
}

bool TGlslangToSpvTraverser::visitUnary(glslang::TVisit /* visit */, glslang::TIntermUnary* node)
{
    builder.clearAccessChain();
    node->getOperand()->traverse(this);
    spv::Id operand = builder.accessChainLoad(TranslatePrecisionDecoration(node->getOperand()->getType()));

    spv::Decoration precision = TranslatePrecisionDecoration(node->getType());

    // it could be a conversion
    spv::Id result = createConversion(node->getOp(), precision, convertGlslangToSpvType(node->getType()), operand);

    // if not, then possibly an operation
    if (! result)
        result = createUnaryOperation(node->getOp(), precision, convertGlslangToSpvType(node->getType()), operand, node->getBasicType() == glslang::EbtFloat || node->getBasicType() == glslang::EbtDouble);

    if (result) {
        builder.clearAccessChain();
        builder.setAccessChainRValue(result);

        return false; // done with this node
    }

    // it must be a special case, check...
    switch (node->getOp()) {
    case glslang::EOpPostIncrement:
    case glslang::EOpPostDecrement:
    case glslang::EOpPreIncrement:
    case glslang::EOpPreDecrement:
        {
            // we need the integer value "1" or the floating point "1.0" to add/subtract
            spv::Id one = node->getBasicType() == glslang::EbtFloat ?
                                     builder.makeFloatConstant(1.0F) :
                                     builder.makeIntConstant(1);
            glslang::TOperator op;
            if (node->getOp() == glslang::EOpPreIncrement ||
                node->getOp() == glslang::EOpPostIncrement)
                op = glslang::EOpAdd;
            else
                op = glslang::EOpSub;

            spv::Id result = createBinaryOperation(op, TranslatePrecisionDecoration(node->getType()), 
                                                     convertGlslangToSpvType(node->getType()), operand, one, 
                                                     node->getType().getBasicType());
            if (result == 0)
                spv::MissingFunctionality("createBinaryOperation for unary");

            // The result of operation is always stored, but conditionally the
            // consumed result.  The consumed result is always an r-value.
            builder.accessChainStore(result);
            builder.clearAccessChain();
            if (node->getOp() == glslang::EOpPreIncrement ||
                node->getOp() == glslang::EOpPreDecrement)
                builder.setAccessChainRValue(result);
            else
                builder.setAccessChainRValue(operand);
        }

        return false;

    case glslang::EOpEmitStreamVertex:
        builder.createNoResultOp(spv::OpEmitStreamVertex, operand);
        return false;
    case glslang::EOpEndStreamPrimitive:
        builder.createNoResultOp(spv::OpEndStreamPrimitive, operand);
        return false;

    default:
        spv::MissingFunctionality("glslang unary");
        break;
    }

    return true;
}

bool TGlslangToSpvTraverser::visitAggregate(glslang::TVisit visit, glslang::TIntermAggregate* node)
{
    spv::Id result;
    glslang::TOperator binOp = glslang::EOpNull;
    bool reduceComparison = true;
    bool isMatrix = false;
    bool noReturnValue = false;

    assert(node->getOp());

    spv::Decoration precision = TranslatePrecisionDecoration(node->getType());

    switch (node->getOp()) {
    case glslang::EOpSequence:
    {
        if (preVisit)
            ++sequenceDepth;
        else
            --sequenceDepth;

        if (sequenceDepth == 1) {
            // If this is the parent node of all the functions, we want to see them
            // early, so all call points have actual SPIR-V functions to reference.
            // In all cases, still let the traverser visit the children for us.
            makeFunctions(node->getAsAggregate()->getSequence());

            // Also, we want all globals initializers to go into the entry of main(), before
            // anything else gets there, so visit out of order, doing them all now.
            makeGlobalInitializers(node->getAsAggregate()->getSequence());

            // Initializers are done, don't want to visit again, but functions link objects need to be processed,
            // so do them manually.
            visitFunctions(node->getAsAggregate()->getSequence());

            return false;
        }

        return true;
    }
    case glslang::EOpLinkerObjects:
    {
        if (visit == glslang::EvPreVisit)
            linkageOnly = true;
        else
            linkageOnly = false;

        return true;
    }
    case glslang::EOpComma:
    {
        // processing from left to right naturally leaves the right-most
        // lying around in the access chain
        glslang::TIntermSequence& glslangOperands = node->getSequence();
        for (int i = 0; i < (int)glslangOperands.size(); ++i)
            glslangOperands[i]->traverse(this);

        return false;
    }
    case glslang::EOpFunction:
        if (visit == glslang::EvPreVisit) {
            if (isShaderEntrypoint(node)) {
                inMain = true;
                builder.setBuildPoint(shaderEntry->getLastBlock());
            } else {
                handleFunctionEntry(node);
            }
        } else {
            if (inMain)
                mainTerminated = true;
            builder.leaveFunction(inMain);
            inMain = false;
        }

        return true;
    case glslang::EOpParameters:
        // Parameters will have been consumed by EOpFunction processing, but not
        // the body, so we still visited the function node's children, making this
        // child redundant.
        return false;
    case glslang::EOpFunctionCall:
    {
        if (node->isUserDefined())
            result = handleUserFunctionCall(node);
        else
            result = handleBuiltInFunctionCall(node);

        if (! result) {
            spv::MissingFunctionality("glslang function call");
            glslang::TConstUnionArray emptyConsts;
            int nextConst = 0;
            result = createSpvConstant(node->getType(), emptyConsts, nextConst);
        }
        builder.clearAccessChain();
        builder.setAccessChainRValue(result);

        return false;
    }
    case glslang::EOpConstructMat2x2:
    case glslang::EOpConstructMat2x3:
    case glslang::EOpConstructMat2x4:
    case glslang::EOpConstructMat3x2:
    case glslang::EOpConstructMat3x3:
    case glslang::EOpConstructMat3x4:
    case glslang::EOpConstructMat4x2:
    case glslang::EOpConstructMat4x3:
    case glslang::EOpConstructMat4x4:
    case glslang::EOpConstructDMat2x2:
    case glslang::EOpConstructDMat2x3:
    case glslang::EOpConstructDMat2x4:
    case glslang::EOpConstructDMat3x2:
    case glslang::EOpConstructDMat3x3:
    case glslang::EOpConstructDMat3x4:
    case glslang::EOpConstructDMat4x2:
    case glslang::EOpConstructDMat4x3:
    case glslang::EOpConstructDMat4x4:
        isMatrix = true;
        // fall through
    case glslang::EOpConstructFloat:
    case glslang::EOpConstructVec2:
    case glslang::EOpConstructVec3:
    case glslang::EOpConstructVec4:
    case glslang::EOpConstructDouble:
    case glslang::EOpConstructDVec2:
    case glslang::EOpConstructDVec3:
    case glslang::EOpConstructDVec4:
    case glslang::EOpConstructBool:
    case glslang::EOpConstructBVec2:
    case glslang::EOpConstructBVec3:
    case glslang::EOpConstructBVec4:
    case glslang::EOpConstructInt:
    case glslang::EOpConstructIVec2:
    case glslang::EOpConstructIVec3:
    case glslang::EOpConstructIVec4:
    case glslang::EOpConstructUint:
    case glslang::EOpConstructUVec2:
    case glslang::EOpConstructUVec3:
    case glslang::EOpConstructUVec4:
    case glslang::EOpConstructStruct:
    {
        std::vector<spv::Id> arguments;
        translateArguments(node->getSequence(), arguments);
        spv::Id resultTypeId = convertGlslangToSpvType(node->getType());
        spv::Id constructed;
        if (node->getOp() == glslang::EOpConstructStruct || node->getType().isArray()) {
            std::vector<spv::Id> constituents;
            for (int c = 0; c < (int)arguments.size(); ++c)
                constituents.push_back(arguments[c]);
            constructed = builder.createCompositeConstruct(resultTypeId, constituents);
        } else {
            if (isMatrix)
                constructed = builder.createMatrixConstructor(precision, arguments, resultTypeId);
            else
                constructed = builder.createConstructor(precision, arguments, resultTypeId);
        }

        builder.clearAccessChain();
        builder.setAccessChainRValue(constructed);

        return false;
    }

    // These six are component-wise compares with component-wise results.
    // Forward on to createBinaryOperation(), requesting a vector result.
    case glslang::EOpLessThan:
    case glslang::EOpGreaterThan:
    case glslang::EOpLessThanEqual:
    case glslang::EOpGreaterThanEqual:
    case glslang::EOpVectorEqual:
    case glslang::EOpVectorNotEqual:
    {
        // Map the operation to a binary
        binOp = node->getOp();
        reduceComparison = false;
        switch (node->getOp()) {
        case glslang::EOpVectorEqual:     binOp = glslang::EOpVectorEqual;      break;
        case glslang::EOpVectorNotEqual:  binOp = glslang::EOpVectorNotEqual;   break;
        default:                          binOp = node->getOp();                break;
        }

        break;
    }
    case glslang::EOpMul:
        // compontent-wise matrix multiply      
        binOp = glslang::EOpMul;
        break;
    case glslang::EOpOuterProduct:
        // two vectors multiplied to make a matrix
        binOp = glslang::EOpOuterProduct;
        break;
    case glslang::EOpDot:
    {
        // for scalar dot product, use multiply        
        glslang::TIntermSequence& glslangOperands = node->getSequence();
        if (! glslangOperands[0]->getAsTyped()->isVector())
            binOp = glslang::EOpMul;
        break;
    }
    case glslang::EOpMod:
        // when an aggregate, this is the floating-point mod built-in function,
        // which can be emitted by the one in createBinaryOperation()
        binOp = glslang::EOpMod;
        break;
    case glslang::EOpArrayLength:
    {
        glslang::TIntermTyped* typedNode = node->getSequence()[0]->getAsTyped();
        assert(typedNode);
        spv::Id length = builder.makeIntConstant(typedNode->getType().getArraySize());

        builder.clearAccessChain();
        builder.setAccessChainRValue(length);

        return false;
    }
    case glslang::EOpEmitVertex:
    case glslang::EOpEndPrimitive:
    case glslang::EOpBarrier:
    case glslang::EOpMemoryBarrier:
    case glslang::EOpMemoryBarrierAtomicCounter:
    case glslang::EOpMemoryBarrierBuffer:
    case glslang::EOpMemoryBarrierImage:
    case glslang::EOpMemoryBarrierShared:
    case glslang::EOpGroupMemoryBarrier:
        noReturnValue = true;
        // These all have 0 operands and will naturally finish up in the code below for 0 operands
        break;

    default:
        break;
    }

    //
    // See if it maps to a regular operation.
    //

    if (binOp != glslang::EOpNull) {
        glslang::TIntermTyped* left = node->getSequence()[0]->getAsTyped();
        glslang::TIntermTyped* right = node->getSequence()[1]->getAsTyped();
        assert(left && right);

        builder.clearAccessChain();
        left->traverse(this);
        spv::Id leftId = builder.accessChainLoad(TranslatePrecisionDecoration(left->getType()));

        builder.clearAccessChain();
        right->traverse(this);
        spv::Id rightId = builder.accessChainLoad(TranslatePrecisionDecoration(right->getType()));

        result = createBinaryOperation(binOp, precision, 
                                       convertGlslangToSpvType(node->getType()), leftId, rightId, 
                                       left->getType().getBasicType(), reduceComparison);

        // code above should only make binOp that exists in createBinaryOperation
        if (result == 0)
            spv::MissingFunctionality("createBinaryOperation for aggregate");

        builder.clearAccessChain();
        builder.setAccessChainRValue(result);

        return false;
    }

    glslang::TIntermSequence& glslangOperands = node->getSequence();
    std::vector<spv::Id> operands;
    for (int arg = 0; arg < (int)glslangOperands.size(); ++arg) {
        builder.clearAccessChain();
        glslangOperands[arg]->traverse(this);

        // special case l-value operands; there are just a few
        bool lvalue = false;
        switch (node->getOp()) {
        //case glslang::EOpFrexp:
        case glslang::EOpModf:
            if (arg == 1)
                lvalue = true;
            break;
        //case glslang::EOpUAddCarry:
        //case glslang::EOpUSubBorrow:
        //case glslang::EOpUMulExtended:
        default:
            break;
        }
        if (lvalue)
            operands.push_back(builder.accessChainGetLValue());
        else
            operands.push_back(builder.accessChainLoad(TranslatePrecisionDecoration(glslangOperands[arg]->getAsTyped()->getType())));
    }
    switch (glslangOperands.size()) {
    case 0:
        result = createNoArgOperation(node->getOp());
        break;
    case 1:
        result = createUnaryOperation(node->getOp(), precision, convertGlslangToSpvType(node->getType()), operands.front(), node->getType().getBasicType() == glslang::EbtFloat || node->getType().getBasicType() == glslang::EbtDouble);
        break;
    default:
        result = createMiscOperation(node->getOp(), precision, convertGlslangToSpvType(node->getType()), operands);
        break;
    }

    if (noReturnValue)
        return false;

    if (! result) {
        spv::MissingFunctionality("glslang aggregate");
        return true;
    } else {
        builder.clearAccessChain();
        builder.setAccessChainRValue(result);
        return false;
    }
}

bool TGlslangToSpvTraverser::visitSelection(glslang::TVisit /* visit */, glslang::TIntermSelection* node)
{
    // This path handles both if-then-else and ?:
    // The if-then-else has a node type of void, while
    // ?: has a non-void node type
    spv::Id result = 0;
    if (node->getBasicType() != glslang::EbtVoid) {
        // don't handle this as just on-the-fly temporaries, because there will be two names
        // and better to leave SSA to later passes
        result = builder.createVariable(spv::StorageClassFunction, convertGlslangToSpvType(node->getType()));
    }

    // emit the condition before doing anything with selection
    node->getCondition()->traverse(this);

    // make an "if" based on the value created by the condition
    spv::Builder::If ifBuilder(builder.accessChainLoad(spv::NoPrecision), builder);

    if (node->getTrueBlock()) {
        // emit the "then" statement
        node->getTrueBlock()->traverse(this);
        if (result)
            builder.createStore(builder.accessChainLoad(TranslatePrecisionDecoration(node->getTrueBlock()->getAsTyped()->getType())), result);
    }

    if (node->getFalseBlock()) {
        ifBuilder.makeBeginElse();
        // emit the "else" statement
        node->getFalseBlock()->traverse(this);
        if (result)
            builder.createStore(builder.accessChainLoad(TranslatePrecisionDecoration(node->getFalseBlock()->getAsTyped()->getType())), result);
    }

    ifBuilder.makeEndIf();

    if (result) {
        // GLSL only has r-values as the result of a :?, but
        // if we have an l-value, that can be more efficient if it will
        // become the base of a complex r-value expression, because the
        // next layer copies r-values into memory to use the access-chain mechanism
        builder.clearAccessChain();
        builder.setAccessChainLValue(result);
    }

    return false;
}

bool TGlslangToSpvTraverser::visitSwitch(glslang::TVisit /* visit */, glslang::TIntermSwitch* node)
{
    // emit and get the condition before doing anything with switch
    node->getCondition()->traverse(this);
    spv::Id selector = builder.accessChainLoad(TranslatePrecisionDecoration(node->getCondition()->getAsTyped()->getType()));

    // browse the children to sort out code segments
    int defaultSegment = -1;
    std::vector<TIntermNode*> codeSegments;
    glslang::TIntermSequence& sequence = node->getBody()->getSequence();
    std::vector<int> caseValues;
    std::vector<int> valueIndexToSegment(sequence.size());  // note: probably not all are used, it is an overestimate
    for (glslang::TIntermSequence::iterator c = sequence.begin(); c != sequence.end(); ++c) {
        TIntermNode* child = *c;
        if (child->getAsBranchNode() && child->getAsBranchNode()->getFlowOp() == glslang::EOpDefault)
            defaultSegment = codeSegments.size();
        else if (child->getAsBranchNode() && child->getAsBranchNode()->getFlowOp() == glslang::EOpCase) {
            valueIndexToSegment[caseValues.size()] = codeSegments.size();
            caseValues.push_back(child->getAsBranchNode()->getExpression()->getAsConstantUnion()->getConstArray()[0].getIConst());
        } else
            codeSegments.push_back(child);
    }

    // handle the case where the last code segment is missing, due to no code 
    // statements between the last case and the end of the switch statement
    if ((caseValues.size() && (int)codeSegments.size() == valueIndexToSegment[caseValues.size() - 1]) ||
        (int)codeSegments.size() == defaultSegment)
        codeSegments.push_back(nullptr);

    // make the switch statement
    std::vector<spv::Block*> segmentBlocks; // returned, as the blocks allocated in the call
    builder.makeSwitch(selector, codeSegments.size(), caseValues, valueIndexToSegment, defaultSegment, segmentBlocks);

    // emit all the code in the segments
    breakForLoop.push(false);
    for (unsigned int s = 0; s < codeSegments.size(); ++s) {
        builder.nextSwitchSegment(segmentBlocks, s);
        if (codeSegments[s])
            codeSegments[s]->traverse(this);
        else
            builder.addSwitchBreak();
    }
    breakForLoop.pop();

    builder.endSwitch(segmentBlocks);

    return false;
}

void TGlslangToSpvTraverser::visitConstantUnion(glslang::TIntermConstantUnion* node)
{
    int nextConst = 0;
    spv::Id constant = createSpvConstant(node->getType(), node->getConstArray(), nextConst);

    builder.clearAccessChain();
    builder.setAccessChainRValue(constant);
}

bool TGlslangToSpvTraverser::visitLoop(glslang::TVisit /* visit */, glslang::TIntermLoop* node)
{
    // body emission needs to know what the for-loop terminal is when it sees a "continue"
    loopTerminal.push(node->getTerminal());

    builder.makeNewLoop();

    bool bodyOut = false;
    if (! node->testFirst()) {
        builder.endLoopHeaderWithoutTest();
        if (node->getBody()) {
            breakForLoop.push(true);
            node->getBody()->traverse(this);
            breakForLoop.pop();
        }
        bodyOut = true;
        builder.createBranchToLoopTest();
    }

    if (node->getTest()) {
        node->getTest()->traverse(this);
        // the AST only contained the test computation, not the branch, we have to add it
        spv::Id condition = builder.accessChainLoad(TranslatePrecisionDecoration(node->getTest()->getType()));
        builder.createLoopTestBranch(condition);
    }

    if (! bodyOut && node->getBody()) {
        breakForLoop.push(true);
        node->getBody()->traverse(this);
        breakForLoop.pop();
    }

    if (loopTerminal.top())
        loopTerminal.top()->traverse(this);

    builder.closeLoop();

    loopTerminal.pop();

    return false;
}

bool TGlslangToSpvTraverser::visitBranch(glslang::TVisit /* visit */, glslang::TIntermBranch* node)
{
    if (node->getExpression())
        node->getExpression()->traverse(this);

    switch (node->getFlowOp()) {
    case glslang::EOpKill:
        builder.makeDiscard();
        break;
    case glslang::EOpBreak:
        if (breakForLoop.top())
            builder.createLoopExit();
        else
            builder.addSwitchBreak();
        break;
    case glslang::EOpContinue:
        if (loopTerminal.top())
            loopTerminal.top()->traverse(this);
        builder.createLoopContinue();
        break;
    case glslang::EOpReturn:
        if (inMain)
            builder.makeMainReturn();
        else if (node->getExpression())
            builder.makeReturn(false, builder.accessChainLoad(TranslatePrecisionDecoration(node->getExpression()->getType())));
        else
            builder.makeReturn();

        builder.clearAccessChain();
        break;

    default:
        spv::MissingFunctionality("branch type");
        break;
    }

    return false;
}

spv::Id TGlslangToSpvTraverser::createSpvVariable(const glslang::TIntermSymbol* node)
{
    // First, steer off constants, which are not SPIR-V variables, but 
    // can still have a mapping to a SPIR-V Id.
    if (node->getQualifier().storage == glslang::EvqConst) {
        int nextConst = 0;
        return createSpvConstant(node->getType(), node->getConstArray(), nextConst);
    }

    // Now, handle actual variables
    spv::StorageClass storageClass = TranslateStorageClass(node->getType());
    spv::Id spvType = convertGlslangToSpvType(node->getType());

    const char* name = node->getName().c_str();
    if (glslang::IsAnonymous(name))
        name = "";

    return builder.createVariable(storageClass, spvType, name);
}

// Return type Id of the sampled type.
spv::Id TGlslangToSpvTraverser::getSampledType(const glslang::TSampler& sampler)
{
    switch (sampler.type) {
        case glslang::EbtFloat:    return builder.makeFloatType(32);
        case glslang::EbtInt:      return builder.makeIntType(32);
        case glslang::EbtUint:     return builder.makeUintType(32);
        default:
            spv::MissingFunctionality("sampled type");
            return builder.makeFloatType(32);
    }
}

// Do full recursive conversion of an arbitrary glslang type to a SPIR-V Id.
spv::Id TGlslangToSpvTraverser::convertGlslangToSpvType(const glslang::TType& type)
{
    spv::Id spvType = 0;

    switch (type.getBasicType()) {
    case glslang::EbtVoid:
        spvType = builder.makeVoidType();
        if (type.isArray())
            spv::MissingFunctionality("array of void");
        break;
    case glslang::EbtFloat:
        spvType = builder.makeFloatType(32);
        break;
    case glslang::EbtDouble:
        spvType = builder.makeFloatType(64);
        break;
    case glslang::EbtBool:
        spvType = builder.makeBoolType();
        break;
    case glslang::EbtInt:
        spvType = builder.makeIntType(32);
        break;
    case glslang::EbtUint:
        spvType = builder.makeUintType(32);
        break;
    case glslang::EbtSampler:
        {
            const glslang::TSampler& sampler = type.getSampler();
            spvType = builder.makeSampler(getSampledType(sampler), TranslateDimensionality(sampler), 
                                          sampler.image ? spv::Builder::samplerContentImage : spv::Builder::samplerContentTextureFilter,
                                          sampler.arrayed, sampler.shadow, sampler.ms);
        }
        break;
    case glslang::EbtStruct:
    case glslang::EbtBlock:
        {
            // If we've seen this struct type, return it
            const glslang::TTypeList* glslangStruct = type.getStruct();
            std::vector<spv::Id> structFields;
            spvType = structMap[glslangStruct];
            if (spvType)
                break;

            // else, we haven't seen it...

            // Create a vector of struct types for SPIR-V to consume
            int memberDelta = 0;  // how much the member's index changes from glslang to SPIR-V, normally 0, except sometimes for blocks
            if (type.getBasicType() == glslang::EbtBlock)
                memberRemapper[glslangStruct].resize(glslangStruct->size());
            for (int i = 0; i < (int)glslangStruct->size(); i++) {
                glslang::TType& glslangType = *(*glslangStruct)[i].type;
                if (glslangType.hiddenMember()) {
                    ++memberDelta;
                    if (type.getBasicType() == glslang::EbtBlock)
                        memberRemapper[glslangStruct][i] = -1;
                } else {
                    if (type.getBasicType() == glslang::EbtBlock)
                        memberRemapper[glslangStruct][i] = i - memberDelta;
                    structFields.push_back(convertGlslangToSpvType(glslangType));
                }
            }

            // Make the SPIR-V type
            spvType = builder.makeStructType(structFields, type.getTypeName().c_str());
            structMap[glslangStruct] = spvType;

            // Name and decorate the non-hidden members
            for (int i = 0; i < (int)glslangStruct->size(); i++) {
                glslang::TType& glslangType = *(*glslangStruct)[i].type;
                int member = i;
                if (type.getBasicType() == glslang::EbtBlock)
                    member = memberRemapper[glslangStruct][i];
                // using -1 above to indicate a hidden member
                if (member >= 0) {
                    builder.addMemberName(spvType, member, glslangType.getFieldName().c_str());
                    addMemberDecoration(spvType, member, TranslateLayoutDecoration(glslangType));
                    addMemberDecoration(spvType, member, TranslatePrecisionDecoration(glslangType));
                    addMemberDecoration(spvType, member, TranslateInterpolationDecoration(glslangType));
                    addMemberDecoration(spvType, member, TranslateInvariantDecoration(glslangType));
                    if (glslangType.getQualifier().hasLocation())
                        builder.addMemberDecoration(spvType, member, spv::DecorationLocation, glslangType.getQualifier().layoutLocation);
                    if (glslangType.getQualifier().hasComponent())
                        builder.addMemberDecoration(spvType, member, spv::DecorationComponent, glslangType.getQualifier().layoutComponent);
                    if (glslangType.getQualifier().hasXfbOffset())
                        builder.addMemberDecoration(spvType, member, spv::DecorationOffset, glslangType.getQualifier().layoutXfbOffset);

                    // built-in variable decorations
                    int builtIn = TranslateBuiltInDecoration(glslangType.getQualifier().builtIn);
                    if (builtIn != spv::BadValue)
                        builder.addMemberDecoration(spvType, member, spv::DecorationBuiltIn, builtIn);
                }
            }

            // Decorate the structure
            addDecoration(spvType, TranslateLayoutDecoration(type));
            addDecoration(spvType, TranslateBlockDecoration(type));
            if (type.getQualifier().hasStream())
                builder.addDecoration(spvType, spv::DecorationStream, type.getQualifier().layoutStream);
            if (glslangIntermediate->getXfbMode()) {
                if (type.getQualifier().hasXfbStride())
                    builder.addDecoration(spvType, spv::DecorationStride, type.getQualifier().layoutXfbStride);
                if (type.getQualifier().hasXfbBuffer())
                    builder.addDecoration(spvType, spv::DecorationXfbBuffer, type.getQualifier().layoutXfbBuffer);
            }
        }
        break;
    default:
        spv::MissingFunctionality("basic type");
        break;
    }

    if (type.isMatrix())
        spvType = builder.makeMatrixType(spvType, type.getMatrixCols(), type.getMatrixRows());
    else {
        // If this variable has a vector element count greater than 1, create a SPIR-V vector
        if (type.getVectorSize() > 1)
            spvType = builder.makeVectorType(spvType, type.getVectorSize());
    }

    if (type.isArray()) {
        unsigned arraySize;
        if (! type.isExplicitlySizedArray()) {
            spv::MissingFunctionality("Unsized array");
            arraySize = 8;
        } else
            arraySize = type.getArraySize();
        spvType = builder.makeArrayType(spvType, arraySize);
    }

    return spvType;
}

bool TGlslangToSpvTraverser::isShaderEntrypoint(const glslang::TIntermAggregate* node)
{
    return node->getName() == "main(";
}

// Make all the functions, skeletally, without actually visiting their bodies.
void TGlslangToSpvTraverser::makeFunctions(const glslang::TIntermSequence& glslFunctions)
{
    for (int f = 0; f < (int)glslFunctions.size(); ++f) {
        glslang::TIntermAggregate* glslFunction = glslFunctions[f]->getAsAggregate();
        if (! glslFunction || glslFunction->getOp() != glslang::EOpFunction || isShaderEntrypoint(glslFunction))
            continue;

        // We're on a user function.  Set up the basic interface for the function now,
        // so that it's available to call.
        // Translating the body will happen later.
        //
        // Typically (except for a "const in" parameter), an address will be passed to the 
        // function.  What it is an address of varies:
        //
        // - "in" parameters not marked as "const" can be written to without modifying the argument,
        //  so that write needs to be to a copy, hence the address of a copy works.
        //
        // - "const in" parameters can just be the r-value, as no writes need occur.
        //
        // - "out" and "inout" arguments can't be done as direct pointers, because GLSL has
        // copy-in/copy-out semantics.  They can be handled though with a pointer to a copy.

        std::vector<spv::Id> paramTypes;
        glslang::TIntermSequence& parameters = glslFunction->getSequence()[0]->getAsAggregate()->getSequence();

        for (int p = 0; p < (int)parameters.size(); ++p) {
            const glslang::TType& paramType = parameters[p]->getAsTyped()->getType();
            spv::Id typeId = convertGlslangToSpvType(paramType);
            if (paramType.getQualifier().storage != glslang::EvqConstReadOnly)
                typeId = builder.makePointer(spv::StorageClassFunction, typeId);
            else
                constReadOnlyParameters.insert(parameters[p]->getAsSymbolNode()->getId());
            paramTypes.push_back(typeId);
        }

        spv::Block* functionBlock;
        spv::Function *function = builder.makeFunctionEntry(convertGlslangToSpvType(glslFunction->getType()), glslFunction->getName().c_str(),
                                                              paramTypes, &functionBlock);

        // Track function to emit/call later
        functionMap[glslFunction->getName().c_str()] = function;

        // Set the parameter id's
        for (int p = 0; p < (int)parameters.size(); ++p) {
            symbolValues[parameters[p]->getAsSymbolNode()->getId()] = function->getParamId(p);
            // give a name too
            builder.addName(function->getParamId(p), parameters[p]->getAsSymbolNode()->getName().c_str());
        }
    }
}

// Process all the initializers, while skipping the functions and link objects
void TGlslangToSpvTraverser::makeGlobalInitializers(const glslang::TIntermSequence& initializers)
{
    builder.setBuildPoint(shaderEntry->getLastBlock());
    for (int i = 0; i < (int)initializers.size(); ++i) {
        glslang::TIntermAggregate* initializer = initializers[i]->getAsAggregate();
        if (initializer && initializer->getOp() != glslang::EOpFunction && initializer->getOp() != glslang::EOpLinkerObjects) {

            // We're on a top-level node that's not a function.  Treat as an initializer, whose
            // code goes into the beginning of main.
            initializer->traverse(this);
        }
    }
}

// Process all the functions, while skipping initializers.
void TGlslangToSpvTraverser::visitFunctions(const glslang::TIntermSequence& glslFunctions)
{
    for (int f = 0; f < (int)glslFunctions.size(); ++f) {
        glslang::TIntermAggregate* node = glslFunctions[f]->getAsAggregate();
        if (node && (node->getOp() == glslang::EOpFunction || node->getOp() == glslang ::EOpLinkerObjects))
            node->traverse(this);
    }
}

void TGlslangToSpvTraverser::handleFunctionEntry(const glslang::TIntermAggregate* node)
{
    // SPIR-V functions should already be in the functionMap from the prepass 
    // that called makeFunctions().
    spv::Function* function = functionMap[node->getName().c_str()];
    spv::Block* functionBlock = function->getEntryBlock();
    builder.setBuildPoint(functionBlock);
}

void TGlslangToSpvTraverser::translateArguments(const glslang::TIntermSequence& glslangArguments, std::vector<spv::Id>& arguments)
{
    for (int i = 0; i < (int)glslangArguments.size(); ++i) {
        builder.clearAccessChain();
        glslangArguments[i]->traverse(this);
        arguments.push_back(builder.accessChainLoad(TranslatePrecisionDecoration(glslangArguments[i]->getAsTyped()->getType())));
    }
}

spv::Id TGlslangToSpvTraverser::handleBuiltInFunctionCall(const glslang::TIntermAggregate* node)
{
    std::vector<spv::Id> arguments;
    translateArguments(node->getSequence(), arguments);

    std::vector<spv::Id> argTypes;
    for (int a = 0; a < (int)arguments.size(); ++a)
        argTypes.push_back(builder.getTypeId(arguments[a]));

    spv::Decoration precision = TranslatePrecisionDecoration(node->getType());

    if (node->getName() == "ftransform(") {
        spv::MissingFunctionality("ftransform()");
        //spv::Id vertex = builder.createVariable(spv::StorageShaderGlobal, spv::VectorType::get(spv::makeFloatType(), 4),
        //                                                             "gl_Vertex_sim");
        //spv::Id matrix = builder.createVariable(spv::StorageShaderGlobal, spv::VectorType::get(spv::makeFloatType(), 4),
        //                                                             "gl_ModelViewProjectionMatrix_sim");
        return 0;
    }

    if (node->getName().substr(0, 7) == "texture" || node->getName().substr(0, 5) == "texel" || node->getName().substr(0, 6) == "shadow") {
        const glslang::TSampler sampler = node->getSequence()[0]->getAsTyped()->getType().getSampler();
        spv::Builder::TextureParameters params = { };
        params.sampler = arguments[0];

        // special case size query
        if (node->getName().find("textureSize", 0) != std::string::npos) {
            if (arguments.size() > 1) {
                params.lod = arguments[1];
                return builder.createTextureQueryCall(spv::OpTextureQuerySizeLod, params);
            } else
                return builder.createTextureQueryCall(spv::OpTextureQuerySize, params);
        }

        // special case the number of samples query
        if (node->getName().find("textureSamples", 0) != std::string::npos)
            return builder.createTextureQueryCall(spv::OpTextureQuerySamples, params);

        // special case the other queries
        if (node->getName().find("Query", 0) != std::string::npos) {
            if (node->getName().find("Levels", 0) != std::string::npos)
                return builder.createTextureQueryCall(spv::OpTextureQueryLevels, params);
            else if (node->getName().find("Lod", 0) != std::string::npos) {
                params.coords = arguments[1];
                return builder.createTextureQueryCall(spv::OpTextureQueryLod, params);
            } else
                spv::MissingFunctionality("glslang texture query");
        }

        // This is no longer a query....

        bool lod = node->getName().find("Lod", 0) != std::string::npos;
        bool proj = node->getName().find("Proj", 0) != std::string::npos;
        bool offsets = node->getName().find("Offsets", 0) != std::string::npos;
        bool offset = ! offsets && node->getName().find("Offset", 0) != std::string::npos;
        bool fetch = node->getName().find("Fetch", 0) != std::string::npos;
        bool gather = node->getName().find("Gather", 0) != std::string::npos;
        bool grad = node->getName().find("Grad", 0) != std::string::npos;

        if (fetch)
            spv::MissingFunctionality("texel fetch");
        if (gather)
            spv::MissingFunctionality("texture gather");

        // check for bias argument
        bool bias = false;
        if (! lod && ! gather && ! grad && ! fetch) {
            int nonBiasArgCount = 2;
            if (offset)
                ++nonBiasArgCount;
            if (grad)
                nonBiasArgCount += 2;

            if ((int)arguments.size() > nonBiasArgCount)
                bias = true;
        }

        bool cubeCompare = sampler.dim == glslang::EsdCube && sampler.arrayed && sampler.shadow;

        // set the rest of the arguments
        params.coords = arguments[1];
        int extraArgs = 0;
        if (cubeCompare)
            params.Dref = arguments[2];
        if (lod) {
            params.lod = arguments[2];
            ++extraArgs;
        }
        if (grad) {
            params.gradX = arguments[2 + extraArgs];
            params.gradY = arguments[3 + extraArgs];
            extraArgs += 2;
        }
        //if (gather && compare) {
        //    params.compare = arguments[2 + extraArgs];
        //    ++extraArgs;
        //}
        if (offset | offsets) {
            params.offset = arguments[2 + extraArgs];
            ++extraArgs;
        }
        if (bias) {
            params.bias = arguments[2 + extraArgs];
            ++extraArgs;
        }

        return builder.createTextureCall(precision, convertGlslangToSpvType(node->getType()), proj, params);
    }

    spv::MissingFunctionality("built-in function call");

    return 0;
}

spv::Id TGlslangToSpvTraverser::handleUserFunctionCall(const glslang::TIntermAggregate* node)
{
    // Grab the function's pointer from the previously created function
    spv::Function* function = functionMap[node->getName().c_str()];
    if (! function)
        return 0;

    const glslang::TIntermSequence& glslangArgs = node->getSequence();
    const glslang::TQualifierList& qualifiers = node->getQualifierList();

    //  See comments in makeFunctions() for details about the semantics for parameter passing.
    //
    // These imply we need a four step process:
    // 1. Evaluate the arguments
    // 2. Allocate and make copies of in, out, and inout arguments
    // 3. Make the call
    // 4. Copy back the results

    // 1. Evaluate the arguments
    std::vector<spv::Builder::AccessChain> lValues;
    std::vector<spv::Id> rValues;
    for (int a = 0; a < (int)glslangArgs.size(); ++a) {
        // build l-value
        builder.clearAccessChain();
        glslangArgs[a]->traverse(this);
        // keep outputs as l-values, evaluate input-only as r-values
        if (qualifiers[a] != glslang::EvqConstReadOnly) {
            // save l-value
            lValues.push_back(builder.getAccessChain());
        } else {
            // process r-value
            rValues.push_back(builder.accessChainLoad(TranslatePrecisionDecoration(glslangArgs[a]->getAsTyped()->getType())));
        }
    }

    // 2. Allocate space for anything needing a copy, and if it's "in" or "inout"
    // copy the original into that space.
    //
    // Also, build up the list of actual arguments to pass in for the call
    int lValueCount = 0;
    int rValueCount = 0;
    std::vector<spv::Id> spvArgs;
    for (int a = 0; a < (int)glslangArgs.size(); ++a) {
        spv::Id arg;
        if (qualifiers[a] != glslang::EvqConstReadOnly) {
            // need space to hold the copy
            const glslang::TType& paramType = glslangArgs[a]->getAsTyped()->getType();
            arg = builder.createVariable(spv::StorageClassFunction, convertGlslangToSpvType(paramType), "param");
            if (qualifiers[a] == glslang::EvqIn || qualifiers[a] == glslang::EvqInOut) {
                // need to copy the input into output space
                builder.setAccessChain(lValues[lValueCount]);
                spv::Id copy = builder.accessChainLoad(spv::NoPrecision);  // TODO: get precision
                builder.createStore(copy, arg);
            }
            ++lValueCount;
        } else {
            arg = rValues[rValueCount];
            ++rValueCount;
        }
        spvArgs.push_back(arg);
    }

    // 3. Make the call.
    spv::Id result = builder.createFunctionCall(function, spvArgs);

    // 4. Copy back out an "out" arguments.
    lValueCount = 0;
    for (int a = 0; a < (int)glslangArgs.size(); ++a) {
        if (qualifiers[a] != glslang::EvqConstReadOnly) {
            if (qualifiers[a] == glslang::EvqOut || qualifiers[a] == glslang::EvqInOut) {
                spv::Id copy = builder.createLoad(spvArgs[a]);
                builder.setAccessChain(lValues[lValueCount]);
                builder.accessChainStore(copy);
            }
            ++lValueCount;
        }
    }

    return result;
}

// Translate AST operation to SPV operation, already having SPV-based operands/types.
spv::Id TGlslangToSpvTraverser::createBinaryOperation(glslang::TOperator op, spv::Decoration precision, 
                                                      spv::Id typeId, spv::Id left, spv::Id right,
                                                      glslang::TBasicType typeProxy, bool reduceComparison)
{
    bool isUnsigned = typeProxy == glslang::EbtUint;
    bool isFloat = typeProxy == glslang::EbtFloat || typeProxy == glslang::EbtDouble;

    spv::Op binOp = spv::OpNop;
    bool needMatchingVectors = true;  // for non-matrix ops, would a scalar need to smear to match a vector?
    bool comparison = false;

    switch (op) {
    case glslang::EOpAdd:
    case glslang::EOpAddAssign:
        if (isFloat)
            binOp = spv::OpFAdd;
        else
            binOp = spv::OpIAdd;
        break;
    case glslang::EOpSub:
    case glslang::EOpSubAssign:
        if (isFloat)
            binOp = spv::OpFSub;
        else
            binOp = spv::OpISub;
        break;
    case glslang::EOpMul:
    case glslang::EOpMulAssign:
        if (isFloat)
            binOp = spv::OpFMul;
        else
            binOp = spv::OpIMul;
        break;
    case glslang::EOpVectorTimesScalar:
    case glslang::EOpVectorTimesScalarAssign:
        if (isFloat) {
            if (builder.isVector(right))
                std::swap(left, right);
            assert(builder.isScalar(right));
            needMatchingVectors = false;
            binOp = spv::OpVectorTimesScalar;
        } else
            binOp = spv::OpIMul;
        break;
    case glslang::EOpVectorTimesMatrix:
    case glslang::EOpVectorTimesMatrixAssign:
        assert(builder.isVector(left));
        assert(builder.isMatrix(right));
        binOp = spv::OpVectorTimesMatrix;
        break;
    case glslang::EOpMatrixTimesVector:
        assert(builder.isMatrix(left));
        assert(builder.isVector(right));
        binOp = spv::OpMatrixTimesVector;
        break;
    case glslang::EOpMatrixTimesScalar:
    case glslang::EOpMatrixTimesScalarAssign:
        if (builder.isMatrix(right))
            std::swap(left, right);
        assert(builder.isScalar(right));
        binOp = spv::OpMatrixTimesScalar;
        break;
    case glslang::EOpMatrixTimesMatrix:
    case glslang::EOpMatrixTimesMatrixAssign:
        assert(builder.isMatrix(left));
        assert(builder.isMatrix(right));
        binOp = spv::OpMatrixTimesMatrix;
        break;
    case glslang::EOpOuterProduct:
        binOp = spv::OpOuterProduct;
        needMatchingVectors = false;
        break;

    case glslang::EOpDiv:
    case glslang::EOpDivAssign:
        if (isFloat)
            binOp = spv::OpFDiv;
        else if (isUnsigned)
            binOp = spv::OpUDiv;
        else
            binOp = spv::OpSDiv;
        break;
    case glslang::EOpMod:
    case glslang::EOpModAssign:
        if (isFloat)
            binOp = spv::OpFMod;
        else if (isUnsigned)
            binOp = spv::OpUMod;
        else
            binOp = spv::OpSMod;
        break;
    case glslang::EOpRightShift:
    case glslang::EOpRightShiftAssign:
        if (isUnsigned)
            binOp = spv::OpShiftRightLogical;
        else
            binOp = spv::OpShiftRightArithmetic;
        break;
    case glslang::EOpLeftShift:
    case glslang::EOpLeftShiftAssign:
        binOp = spv::OpShiftLeftLogical;
        break;
    case glslang::EOpAnd:
    case glslang::EOpAndAssign:
        binOp = spv::OpBitwiseAnd;
        break;
    case glslang::EOpLogicalAnd:
        needMatchingVectors = false;
        binOp = spv::OpLogicalAnd;
        break;
    case glslang::EOpInclusiveOr:
    case glslang::EOpInclusiveOrAssign:
        binOp = spv::OpBitwiseOr;
        break;
    case glslang::EOpLogicalOr:
        needMatchingVectors = false;
        binOp = spv::OpLogicalOr;
        break;
    case glslang::EOpExclusiveOr:
    case glslang::EOpExclusiveOrAssign:
        binOp = spv::OpBitwiseXor;
        break;
    case glslang::EOpLogicalXor:
        needMatchingVectors = false;
        binOp = spv::OpLogicalXor;
        break;

    case glslang::EOpLessThan:
    case glslang::EOpGreaterThan:
    case glslang::EOpLessThanEqual:
    case glslang::EOpGreaterThanEqual:
    case glslang::EOpEqual:
    case glslang::EOpNotEqual:
    case glslang::EOpVectorEqual:
    case glslang::EOpVectorNotEqual:
        comparison = true;
        break;
    default:
        break;
    }

    if (binOp != spv::OpNop) {
        if (builder.isMatrix(left) || builder.isMatrix(right)) {
            switch (binOp) {
            case spv::OpMatrixTimesScalar:
            case spv::OpVectorTimesMatrix:
            case spv::OpMatrixTimesVector:
            case spv::OpMatrixTimesMatrix:
                break;
            case spv::OpFDiv:
                // turn it into a multiply...
                assert(builder.isMatrix(left) && builder.isScalar(right));
                right = builder.createBinOp(spv::OpFDiv, builder.getTypeId(right), builder.makeFloatConstant(1.0F), right);
                binOp = spv::OpFMul;
                break;
            default:
                spv::MissingFunctionality("binary operation on matrix");
                break;
            }

            spv::Id id = builder.createBinOp(binOp, typeId, left, right);
            builder.setPrecision(id, precision);

            return id;
        }

        // No matrix involved; make both operands be the same number of components, if needed
        if (needMatchingVectors)
            builder.promoteScalar(precision, left, right);

        spv::Id id = builder.createBinOp(binOp, typeId, left, right);
        builder.setPrecision(id, precision);

        return id;
    }

    if (! comparison)
        return 0;

    // Comparison instructions

    if (reduceComparison && (builder.isVector(left) || builder.isMatrix(left) || builder.isAggregate(left))) {
        assert(op == glslang::EOpEqual || op == glslang::EOpNotEqual);

        return builder.createCompare(precision, left, right, op == glslang::EOpEqual);
    }

    switch (op) {
    case glslang::EOpLessThan:
        if (isFloat)
            binOp = spv::OpFOrdLessThan;
        else if (isUnsigned)
            binOp = spv::OpULessThan;
        else
            binOp = spv::OpSLessThan;
        break;
    case glslang::EOpGreaterThan:
        if (isFloat)
            binOp = spv::OpFOrdGreaterThan;
        else if (isUnsigned)
            binOp = spv::OpUGreaterThan;
        else
            binOp = spv::OpSGreaterThan;
        break;
    case glslang::EOpLessThanEqual:
        if (isFloat)
            binOp = spv::OpFOrdLessThanEqual;
        else if (isUnsigned)
            binOp = spv::OpULessThanEqual;
        else
            binOp = spv::OpSLessThanEqual;
        break;
    case glslang::EOpGreaterThanEqual:
        if (isFloat)
            binOp = spv::OpFOrdGreaterThanEqual;
        else if (isUnsigned)
            binOp = spv::OpUGreaterThanEqual;
        else
            binOp = spv::OpSGreaterThanEqual;
        break;
    case glslang::EOpEqual:
    case glslang::EOpVectorEqual:
        if (isFloat)
            binOp = spv::OpFOrdEqual;
        else
            binOp = spv::OpIEqual;
        break;
    case glslang::EOpNotEqual:
    case glslang::EOpVectorNotEqual:
        if (isFloat)
            binOp = spv::OpFOrdNotEqual;
        else
            binOp = spv::OpINotEqual;
        break;
    default:
        break;
    }

    if (binOp != spv::OpNop) {
        spv::Id id = builder.createBinOp(binOp, typeId, left, right);
        builder.setPrecision(id, precision);

        return id;
    }

    return 0;
}

spv::Id TGlslangToSpvTraverser::createUnaryOperation(glslang::TOperator op, spv::Decoration precision, spv::Id typeId, spv::Id operand, bool isFloat)
{
    spv::Op unaryOp = spv::OpNop;
    int libCall = -1;

    switch (op) {
    case glslang::EOpNegative:
        if (isFloat)
            unaryOp = spv::OpFNegate;
        else
            unaryOp = spv::OpSNegate;
        break;

    case glslang::EOpLogicalNot:
    case glslang::EOpVectorLogicalNot:
    case glslang::EOpBitwiseNot:
        unaryOp = spv::OpNot;
        break;
    
    case glslang::EOpDeterminant:
        libCall = GLSL_STD_450::Determinant;
        break;
    case glslang::EOpMatrixInverse:
        libCall = GLSL_STD_450::MatrixInverse;
        break;
    case glslang::EOpTranspose:
        unaryOp = spv::OpTranspose;
        break;

    case glslang::EOpRadians:
        libCall = GLSL_STD_450::Radians;
        break;
    case glslang::EOpDegrees:
        libCall = GLSL_STD_450::Degrees;
        break;
    case glslang::EOpSin:
        libCall = GLSL_STD_450::Sin;
        break;
    case glslang::EOpCos:
        libCall = GLSL_STD_450::Cos;
        break;
    case glslang::EOpTan:
        libCall = GLSL_STD_450::Tan;
        break;
    case glslang::EOpAcos:
        libCall = GLSL_STD_450::Acos;
        break;
    case glslang::EOpAsin:
        libCall = GLSL_STD_450::Asin;
        break;
    case glslang::EOpAtan:
        libCall = GLSL_STD_450::Atan;
        break;

    case glslang::EOpAcosh:
        libCall = GLSL_STD_450::Acosh;
        break;
    case glslang::EOpAsinh:
        libCall = GLSL_STD_450::Asinh;
        break;
    case glslang::EOpAtanh:
        libCall = GLSL_STD_450::Atanh;
        break;
    case glslang::EOpTanh:
        libCall = GLSL_STD_450::Tanh;
        break;
    case glslang::EOpCosh:
        libCall = GLSL_STD_450::Cosh;
        break;
    case glslang::EOpSinh:
        libCall = GLSL_STD_450::Sinh;
        break;

    case glslang::EOpLength:
        libCall = GLSL_STD_450::Length;
        break;
    case glslang::EOpNormalize:
        libCall = GLSL_STD_450::Normalize;
        break;

    case glslang::EOpExp:
        libCall = GLSL_STD_450::Exp;
        break;
    case glslang::EOpLog:
        libCall = GLSL_STD_450::Log;
        break;
    case glslang::EOpExp2:
        libCall = GLSL_STD_450::Exp2;
        break;
    case glslang::EOpLog2:
        libCall = GLSL_STD_450::Log2;
        break;
    case glslang::EOpSqrt:
        libCall = GLSL_STD_450::Sqrt;
        break;
    case glslang::EOpInverseSqrt:
        libCall = GLSL_STD_450::InverseSqrt;
        break;

    case glslang::EOpFloor:
        libCall = GLSL_STD_450::Floor;
        break;
    case glslang::EOpTrunc:
        libCall = GLSL_STD_450::Trunc;
        break;
    case glslang::EOpRound:
        libCall = GLSL_STD_450::Round;
        break;
    case glslang::EOpRoundEven:
        libCall = GLSL_STD_450::RoundEven;
        break;
    case glslang::EOpCeil:
        libCall = GLSL_STD_450::Ceil;
        break;
    case glslang::EOpFract:
        libCall = GLSL_STD_450::Fract;
        break;

    case glslang::EOpIsNan:
        unaryOp = spv::OpIsNan;
        break;
    case glslang::EOpIsInf:
        unaryOp = spv::OpIsInf;
        break;

    case glslang::EOpFloatBitsToInt:
        libCall = GLSL_STD_450::FloatBitsToInt;
        break;
    case glslang::EOpFloatBitsToUint:
        libCall = GLSL_STD_450::FloatBitsToUint;
        break;
    case glslang::EOpIntBitsToFloat:
        libCall = GLSL_STD_450::IntBitsToFloat;
        break;
    case glslang::EOpUintBitsToFloat:
        libCall = GLSL_STD_450::UintBitsToFloat;
        break;
    case glslang::EOpPackSnorm2x16:
        libCall = GLSL_STD_450::PackSnorm2x16;
        break;
    case glslang::EOpUnpackSnorm2x16:
        libCall = GLSL_STD_450::UnpackSnorm2x16;
        break;
    case glslang::EOpPackUnorm2x16:
        libCall = GLSL_STD_450::PackUnorm2x16;
        break;
    case glslang::EOpUnpackUnorm2x16:
        libCall = GLSL_STD_450::UnpackUnorm2x16;
        break;
    case glslang::EOpPackHalf2x16:
        libCall = GLSL_STD_450::PackHalf2x16;
        break;
    case glslang::EOpUnpackHalf2x16:
        libCall = GLSL_STD_450::UnpackHalf2x16;
        break;

    case glslang::EOpDPdx:
        unaryOp = spv::OpDPdx;
        break;
    case glslang::EOpDPdy:
        unaryOp = spv::OpDPdy;
        break;
    case glslang::EOpFwidth:
        unaryOp = spv::OpFwidth;
        break;
    case glslang::EOpDPdxFine:
        unaryOp = spv::OpDPdxFine;
        break;
    case glslang::EOpDPdyFine:
        unaryOp = spv::OpDPdyFine;
        break;
    case glslang::EOpFwidthFine:
        unaryOp = spv::OpFwidthFine;
        break;
    case glslang::EOpDPdxCoarse:
        unaryOp = spv::OpDPdxCoarse;
        break;
    case glslang::EOpDPdyCoarse:
        unaryOp = spv::OpDPdyCoarse;
        break;
    case glslang::EOpFwidthCoarse:
        unaryOp = spv::OpFwidthCoarse;
        break;

    case glslang::EOpAny:
        unaryOp = spv::OpAny;
        break;
    case glslang::EOpAll:
        unaryOp = spv::OpAll;
        break;

    case glslang::EOpAbs:
        libCall = GLSL_STD_450::Abs;
        break;
    case glslang::EOpSign:
        libCall = GLSL_STD_450::Sign;
        break;

    default:
        return 0;
    }

    spv::Id id;
    if (libCall >= 0) {
        std::vector<spv::Id> args;
        args.push_back(operand);
        id = builder.createBuiltinCall(precision, typeId, stdBuiltins, libCall, args);
    } else
        id = builder.createUnaryOp(unaryOp, typeId, operand);

    builder.setPrecision(id, precision);

    return id;
}

spv::Id TGlslangToSpvTraverser::createConversion(glslang::TOperator op, spv::Decoration precision, spv::Id destType, spv::Id operand)
{
    spv::Op convOp = spv::OpNop;
    spv::Id zero = 0;
    spv::Id one = 0;

    int vectorSize = builder.isVectorType(destType) ? builder.getNumTypeComponents(destType) : 0;

    switch (op) {
    case glslang::EOpConvIntToBool:
    case glslang::EOpConvUintToBool:
        zero = builder.makeUintConstant(0);
        zero = makeSmearedConstant(zero, vectorSize);
        return builder.createBinOp(spv::OpINotEqual, destType, operand, zero);

    case glslang::EOpConvFloatToBool:
        zero = builder.makeFloatConstant(0.0F);
        zero = makeSmearedConstant(zero, vectorSize);
        return builder.createBinOp(spv::OpFOrdNotEqual, destType, operand, zero);

    case glslang::EOpConvDoubleToBool:
        zero = builder.makeDoubleConstant(0.0);
        zero = makeSmearedConstant(zero, vectorSize);
        return builder.createBinOp(spv::OpFOrdNotEqual, destType, operand, zero);

    case glslang::EOpConvBoolToFloat:
        convOp = spv::OpSelect;
        zero = builder.makeFloatConstant(0.0);
        one  = builder.makeFloatConstant(1.0);
        break;
    case glslang::EOpConvBoolToDouble:
        convOp = spv::OpSelect;
        zero = builder.makeDoubleConstant(0.0);
        one  = builder.makeDoubleConstant(1.0);
        break;
    case glslang::EOpConvBoolToInt:
        zero = builder.makeIntConstant(0);
        one  = builder.makeIntConstant(1);
        convOp = spv::OpSelect;
        break;
    case glslang::EOpConvBoolToUint:
        zero = builder.makeUintConstant(0);
        one  = builder.makeUintConstant(1);
        convOp = spv::OpSelect;
        break;

    case glslang::EOpConvIntToFloat:
    case glslang::EOpConvIntToDouble:
        convOp = spv::OpConvertSToF;
        break;

    case glslang::EOpConvUintToFloat:
    case glslang::EOpConvUintToDouble:
        convOp = spv::OpConvertUToF;
        break;

    case glslang::EOpConvDoubleToFloat:
    case glslang::EOpConvFloatToDouble:
        convOp = spv::OpFConvert;
        break;

    case glslang::EOpConvFloatToInt:
    case glslang::EOpConvDoubleToInt:
        convOp = spv::OpConvertFToS;
        break;

    case glslang::EOpConvUintToInt:
    case glslang::EOpConvIntToUint:
        convOp = spv::OpBitcast;
        break;

    case glslang::EOpConvFloatToUint:
    case glslang::EOpConvDoubleToUint:
        convOp = spv::OpConvertFToU;
        break;
    default:
        break;
    }

    spv::Id result = 0;
    if (convOp == spv::OpNop)
        return result;

    if (convOp == spv::OpSelect) {
        zero = makeSmearedConstant(zero, vectorSize);
        one  = makeSmearedConstant(one, vectorSize);
        result = builder.createTriOp(convOp, destType, operand, one, zero);
    } else
        result = builder.createUnaryOp(convOp, destType, operand);

    builder.setPrecision(result, precision);

    return result;
}

spv::Id TGlslangToSpvTraverser::makeSmearedConstant(spv::Id constant, int vectorSize)
{
    if (vectorSize == 0)
        return constant;

    spv::Id vectorTypeId = builder.makeVectorType(builder.getTypeId(constant), vectorSize);
    std::vector<spv::Id> components;
    for (int c = 0; c < vectorSize; ++c)
        components.push_back(constant);
    return builder.makeCompositeConstant(vectorTypeId, components);
}

spv::Id TGlslangToSpvTraverser::createMiscOperation(glslang::TOperator op, spv::Decoration precision, spv::Id typeId, std::vector<spv::Id>& operands)
{
    spv::Op opCode = spv::OpNop;
    int libCall = -1;

    switch (op) {
    case glslang::EOpMin:
        libCall = GLSL_STD_450::Min;
        break;
    case glslang::EOpModf:
        libCall = GLSL_STD_450::Modf;
        break;
    case glslang::EOpMax:
        libCall = GLSL_STD_450::Max;
        break;
    case glslang::EOpPow:
        libCall = GLSL_STD_450::Pow;
        break;
    case glslang::EOpDot:
        opCode = spv::OpDot;
        break;
    case glslang::EOpAtan:
        libCall = GLSL_STD_450::Atan2;
        break;

    case glslang::EOpClamp:
        libCall = GLSL_STD_450::Clamp;
        break;
    case glslang::EOpMix:
        libCall = GLSL_STD_450::Mix;
        break;
    case glslang::EOpStep:
        libCall = GLSL_STD_450::Step;
        break;
    case glslang::EOpSmoothStep:
        libCall = GLSL_STD_450::SmoothStep;
        break;

    case glslang::EOpDistance:
        libCall = GLSL_STD_450::Distance;
        break;
    case glslang::EOpCross:
        libCall = GLSL_STD_450::Cross;
        break;
    case glslang::EOpFaceForward:
        libCall = GLSL_STD_450::FaceForward;
        break;
    case glslang::EOpReflect:
        libCall = GLSL_STD_450::Reflect;
        break;
    case glslang::EOpRefract:
        libCall = GLSL_STD_450::Refract;
        break;
    default:
        return 0;
    }

    spv::Id id = 0;
    if (libCall >= 0)
        id = builder.createBuiltinCall(precision, typeId, stdBuiltins, libCall, operands);
    else {
        switch (operands.size()) {
        case 0:
            // should all be handled by visitAggregate and createNoArgOperation
            assert(0);
            return 0;
        case 1:
            // should all be handled by createUnaryOperation
            assert(0);
            return 0;
        case 2:
            id = builder.createBinOp(opCode, typeId, operands[0], operands[1]);
            break;
        case 3:
            id = builder.createTernaryOp(opCode, typeId, operands[0], operands[1], operands[2]);
            break;
        default:
            // These do not exist yet
            assert(0 && "operation with more than 3 operands");
            break;
        }
    }

    builder.setPrecision(id, precision);

    return id;
}

// Intrinsics with no arguments, no return value, and no precision.
spv::Id TGlslangToSpvTraverser::createNoArgOperation(glslang::TOperator op)
{
    // TODO: get the barrier operands correct

    switch (op) {
    case glslang::EOpEmitVertex:
        builder.createNoResultOp(spv::OpEmitVertex);
        return 0;
    case glslang::EOpEndPrimitive:
        builder.createNoResultOp(spv::OpEndPrimitive);
        return 0;
    case glslang::EOpBarrier:
        builder.createMemoryBarrier(spv::ExecutionScopeDevice, spv::MemorySemanticsAllMemory);
        builder.createControlBarrier(spv::ExecutionScopeDevice);
        return 0;
    case glslang::EOpMemoryBarrier:
        builder.createMemoryBarrier(spv::ExecutionScopeDevice, spv::MemorySemanticsAllMemory);
        return 0;
    case glslang::EOpMemoryBarrierAtomicCounter:
        builder.createMemoryBarrier(spv::ExecutionScopeDevice, spv::MemorySemanticsAtomicCounterMemoryMask);
        return 0;
    case glslang::EOpMemoryBarrierBuffer:
        builder.createMemoryBarrier(spv::ExecutionScopeDevice, spv::MemorySemanticsUniformMemoryMask);
        return 0;
    case glslang::EOpMemoryBarrierImage:
        builder.createMemoryBarrier(spv::ExecutionScopeDevice, spv::MemorySemanticsImageMemoryMask);
        return 0;
    case glslang::EOpMemoryBarrierShared:
        builder.createMemoryBarrier(spv::ExecutionScopeDevice, spv::MemorySemanticsWorkgroupLocalMemoryMask);
        return 0;
    case glslang::EOpGroupMemoryBarrier:
        builder.createMemoryBarrier(spv::ExecutionScopeDevice, spv::MemorySemanticsWorkgroupGlobalMemoryMask);
        return 0;
    default:
        spv::MissingFunctionality("operation with no arguments");
        return 0;
    }
}

spv::Id TGlslangToSpvTraverser::getSymbolId(const glslang::TIntermSymbol* symbol)
{
    std::map<int, spv::Id>::iterator iter;
    iter = symbolValues.find(symbol->getId());
    spv::Id id;
    if (symbolValues.end() != iter) {
        id = iter->second;
        return id;
    }

    // it was not found, create it
    id = createSpvVariable(symbol);
    symbolValues[symbol->getId()] = id;

    if (! symbol->getType().isStruct()) {
        addDecoration(id, TranslatePrecisionDecoration(symbol->getType()));
        addDecoration(id, TranslateInterpolationDecoration(symbol->getType()));
        if (symbol->getQualifier().hasLocation())
            builder.addDecoration(id, spv::DecorationLocation, symbol->getQualifier().layoutLocation);
        if (symbol->getQualifier().hasIndex())
            builder.addDecoration(id, spv::DecorationIndex, symbol->getQualifier().layoutIndex);
        if (symbol->getQualifier().hasComponent())
            builder.addDecoration(id, spv::DecorationComponent, symbol->getQualifier().layoutComponent);
        if (glslangIntermediate->getXfbMode()) {
            if (symbol->getQualifier().hasXfbStride())
                builder.addDecoration(id, spv::DecorationStride, symbol->getQualifier().layoutXfbStride);
            if (symbol->getQualifier().hasXfbBuffer())
                builder.addDecoration(id, spv::DecorationXfbBuffer, symbol->getQualifier().layoutXfbBuffer);
            if (symbol->getQualifier().hasXfbOffset())
                builder.addDecoration(id, spv::DecorationOffset, symbol->getQualifier().layoutXfbOffset);
        }
    }

    addDecoration(id, TranslateInvariantDecoration(symbol->getType()));
    if (symbol->getQualifier().hasStream())
        builder.addDecoration(id, spv::DecorationStream, symbol->getQualifier().layoutStream);
    if (symbol->getQualifier().hasSet())
        builder.addDecoration(id, spv::DecorationDescriptorSet, symbol->getQualifier().layoutSet);
    if (symbol->getQualifier().hasBinding())
        builder.addDecoration(id, spv::DecorationBinding, symbol->getQualifier().layoutBinding);
    if (glslangIntermediate->getXfbMode()) {
        if (symbol->getQualifier().hasXfbStride())
            builder.addDecoration(id, spv::DecorationStride, symbol->getQualifier().layoutXfbStride);
        if (symbol->getQualifier().hasXfbBuffer())
            builder.addDecoration(id, spv::DecorationXfbBuffer, symbol->getQualifier().layoutXfbBuffer);
    }

    // built-in variable decorations
    int builtIn = TranslateBuiltInDecoration(symbol->getQualifier().builtIn);
    if (builtIn != spv::BadValue)
        builder.addDecoration(id, spv::DecorationBuiltIn, builtIn);

    if (linkageOnly)
        builder.addDecoration(id, spv::DecorationNoStaticUse);

    return id;
}

void TGlslangToSpvTraverser::addDecoration(spv::Id id, spv::Decoration dec)
{
    if (dec != spv::BadValue)
        builder.addDecoration(id, dec);
}

void TGlslangToSpvTraverser::addMemberDecoration(spv::Id id, int member, spv::Decoration dec)
{
    if (dec != spv::BadValue)
        builder.addMemberDecoration(id, (unsigned)member, dec);
}

// Use 'consts' as the flattened glslang source of scalar constants to recursively
// build the aggregate SPIR-V constant.
//
// If there are not enough elements present in 'consts', 0 will be substituted;
// an empty 'consts' can be used to create a fully zeroed SPIR-V constant.
//
spv::Id TGlslangToSpvTraverser::createSpvConstant(const glslang::TType& glslangType, const glslang::TConstUnionArray& consts, int& nextConst)
{
    // vector of constants for SPIR-V
    std::vector<spv::Id> spvConsts;

    // Type is used for struct and array constants
    spv::Id typeId = convertGlslangToSpvType(glslangType);

    if (glslangType.isArray()) {
        glslang::TType elementType;
        elementType.deepCopy(glslangType);
        elementType.dereference();
        for (int i = 0; i < glslangType.getArraySize(); ++i)
            spvConsts.push_back(createSpvConstant(elementType, consts, nextConst));
    } else if (glslangType.isMatrix()) {
        glslang::TType vectorType;
        vectorType.shallowCopy(glslangType);
        vectorType.dereference();
        for (int col = 0; col < glslangType.getMatrixCols(); ++col)
            spvConsts.push_back(createSpvConstant(vectorType, consts, nextConst));
    } else if (glslangType.getStruct()) {
        glslang::TVector<glslang::TTypeLoc>::const_iterator iter;
        for (iter = glslangType.getStruct()->begin(); iter != glslangType.getStruct()->end(); ++iter)
            spvConsts.push_back(createSpvConstant(*iter->type, consts, nextConst));
    } else if (glslangType.isVector()) {
        for (unsigned int i = 0; i < (unsigned int)glslangType.getVectorSize(); ++i) {
            bool zero = nextConst >= consts.size();
            switch (glslangType.getBasicType()) {
            case glslang::EbtInt:
                spvConsts.push_back(builder.makeIntConstant(zero ? 0 : consts[nextConst].getIConst()));
                break;
            case glslang::EbtUint:
                spvConsts.push_back(builder.makeUintConstant(zero ? 0 : consts[nextConst].getUConst()));
                break;
            case glslang::EbtFloat:
                spvConsts.push_back(builder.makeFloatConstant(zero ? 0.0F : (float)consts[nextConst].getDConst()));
                break;
            case glslang::EbtDouble:
                spvConsts.push_back(builder.makeDoubleConstant(zero ? 0.0 : consts[nextConst].getDConst()));
                break;
            case glslang::EbtBool:
                spvConsts.push_back(builder.makeBoolConstant(zero ? false : consts[nextConst].getBConst()));
                break;
            default:
                spv::MissingFunctionality("constant vector type");
                break;
            }
            ++nextConst;
        }
    } else {
        // we have a non-aggregate (scalar) constant
        bool zero = nextConst >= consts.size();
        spv::Id scalar = 0;
        switch (glslangType.getBasicType()) {
        case glslang::EbtInt:
            scalar = builder.makeIntConstant(zero ? 0 : consts[nextConst].getIConst());
            break;
        case glslang::EbtUint:
            scalar = builder.makeUintConstant(zero ? 0 : consts[nextConst].getUConst());
            break;
        case glslang::EbtFloat:
            scalar = builder.makeFloatConstant(zero ? 0.0F : (float)consts[nextConst].getDConst());
            break;
        case glslang::EbtDouble:
            scalar = builder.makeDoubleConstant(zero ? 0.0 : consts[nextConst].getDConst());
            break;
        case glslang::EbtBool:
            scalar = builder.makeBoolConstant(zero ? false : consts[nextConst].getBConst());
            break;
        default:
            spv::MissingFunctionality("constant scalar type");
            break;
        }
        ++nextConst;
        return scalar;
    }

    return builder.makeCompositeConstant(typeId, spvConsts);
}

};  // end anonymous namespace

namespace glslang {

// Write SPIR-V out to a binary file
void OutputSpv(const std::vector<unsigned int>& spirv, const char* baseName)
{
    std::ofstream out;
    std::string fileName(baseName);
    fileName.append(".spv");
    out.open(fileName.c_str(), std::ios::binary | std::ios::out);
    for (int i = 0; i < (int)spirv.size(); ++i) {
        unsigned int word = spirv[i];
        out.write((const char*)&word, 4);
    }
    out.close();
}

//
// Set up the glslang traversal
//
void GlslangToSpv(const glslang::TIntermediate& intermediate, std::vector<unsigned int>& spirv)
{
    TIntermNode* root = intermediate.getTreeRoot();

    if (root == 0)
        return;

    glslang::GetThreadPoolAllocator().push();

    TGlslangToSpvTraverser it(&intermediate);

    root->traverse(&it);

    it.dumpSpv(spirv);

    glslang::GetThreadPoolAllocator().pop();
}

}; // end namespace glslang
