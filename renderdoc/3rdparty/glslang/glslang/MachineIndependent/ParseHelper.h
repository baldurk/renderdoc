//
//Copyright (C) 2002-2005  3Dlabs Inc. Ltd.
//Copyright (C) 2012-2013 LunarG, Inc.
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
#ifndef _PARSER_HELPER_INCLUDED_
#define _PARSER_HELPER_INCLUDED_

#include "Versions.h"
#include "../Include/ShHandle.h"
#include "SymbolTable.h"
#include "localintermediate.h"
#include "Scan.h"
#include <functional>

#include <functional>

namespace glslang {

struct TPragma {
	TPragma(bool o, bool d) : optimize(o), debug(d) { }
	bool optimize;
	bool debug;
	TPragmaTable pragmaTable;
};

class TScanContext;
class TPpContext;

typedef std::set<int> TIdSetType;

//
// The following are extra variables needed during parsing, grouped together so
// they can be passed to the parser without needing a global.
//
class TParseContext {
public:
    TParseContext(TSymbolTable&, TIntermediate&, bool parsingBuiltins, int version, EProfile, EShLanguage, TInfoSink&,
                  bool forwardCompatible = false, EShMessages messages = EShMsgDefault);
    virtual ~TParseContext();

    void setLimits(const TBuiltInResource&);
    bool parseShaderStrings(TPpContext&, TInputScanner& input, bool versionWillBeError = false);
    void parserError(const char* s);     // for bison's yyerror
    const char* getPreamble();

    void C_DECL error(TSourceLoc, const char* szReason, const char* szToken,
                      const char* szExtraInfoFormat, ...);
    void C_DECL  warn(TSourceLoc, const char* szReason, const char* szToken,
                      const char* szExtraInfoFormat, ...);
    void reservedErrorCheck(TSourceLoc, const TString&);
    void reservedPpErrorCheck(TSourceLoc, const char* name, const char* op);
    bool lineContinuationCheck(TSourceLoc, bool endOfComment);
    bool builtInName(const TString&);

    void handlePragma(TSourceLoc, const TVector<TString>&);
    TIntermTyped* handleVariable(TSourceLoc, TSymbol* symbol, const TString* string);
    TIntermTyped* handleBracketDereference(TSourceLoc, TIntermTyped* base, TIntermTyped* index);
    void checkIndex(TSourceLoc, const TType&, int& index);
    void handleIndexLimits(TSourceLoc, TIntermTyped* base, TIntermTyped* index);

    void makeEditable(TSymbol*&);
    bool isIoResizeArray(const TType&) const;
    void fixIoArraySize(TSourceLoc, TType&);
    void ioArrayCheck(TSourceLoc, const TType&, const TString& identifier);
    void handleIoResizeArrayAccess(TSourceLoc, TIntermTyped* base);
    void checkIoArraysConsistency(TSourceLoc, bool tailOnly = false);
    int getIoArrayImplicitSize() const;
    void checkIoArrayConsistency(TSourceLoc, int requiredSize, const char* feature, TType&, const TString&);

    TIntermTyped* handleBinaryMath(TSourceLoc, const char* str, TOperator op, TIntermTyped* left, TIntermTyped* right);
    TIntermTyped* handleUnaryMath(TSourceLoc, const char* str, TOperator op, TIntermTyped* childNode);
    TIntermTyped* handleDotDereference(TSourceLoc, TIntermTyped* base, const TString& field);
    void blockMemberExtensionCheck(TSourceLoc, const TIntermTyped* base, const TString& field);
    TFunction* handleFunctionDeclarator(TSourceLoc, TFunction& function, bool prototype);
    TIntermAggregate* handleFunctionDefinition(TSourceLoc, TFunction&);
    TIntermTyped* handleFunctionCall(TSourceLoc, TFunction*, TIntermNode*);
    void checkLocation(TSourceLoc, TOperator);
    TIntermTyped* handleLengthMethod(TSourceLoc, TFunction*, TIntermNode*);
    void addInputArgumentConversions(const TFunction&, TIntermNode*&) const;
    TIntermTyped* addOutputArgumentConversions(const TFunction&, TIntermAggregate&) const;
    void nonOpBuiltInCheck(TSourceLoc, const TFunction&, TIntermAggregate&);
    TFunction* handleConstructorCall(TSourceLoc, const TPublicType&);

    bool parseVectorFields(TSourceLoc, const TString&, int vecSize, TVectorFields&);
    void assignError(TSourceLoc, const char* op, TString left, TString right);
    void unaryOpError(TSourceLoc, const char* op, TString operand);
    void binaryOpError(TSourceLoc, const char* op, TString left, TString right);
    void variableCheck(TIntermTyped*& nodePtr);
    bool lValueErrorCheck(TSourceLoc, const char* op, TIntermTyped*);
    void rValueErrorCheck(TSourceLoc, const char* op, TIntermTyped*);
    void constantValueCheck(TIntermTyped* node, const char* token);
    void integerCheck(const TIntermTyped* node, const char* token);
    void globalCheck(TSourceLoc, const char* token);
    bool constructorError(TSourceLoc, TIntermNode*, TFunction&, TOperator, TType&);
    void arraySizeCheck(TSourceLoc, TIntermTyped* expr, int& size);
    bool arrayQualifierError(TSourceLoc, const TQualifier&);
    bool arrayError(TSourceLoc, const TType&);
    void arraySizeRequiredCheck(TSourceLoc, int size);
    void structArrayCheck(TSourceLoc, const TType& structure);
    void arrayUnsizedCheck(TSourceLoc, const TQualifier&, int size, bool initializer);
    void arrayOfArrayVersionCheck(TSourceLoc);
    void arrayDimCheck(TSourceLoc, TArraySizes* sizes1, TArraySizes* sizes2);
    void arrayDimCheck(TSourceLoc, const TType*, TArraySizes*);
    bool voidErrorCheck(TSourceLoc, const TString&, TBasicType);
    void boolCheck(TSourceLoc, const TIntermTyped*);
    void boolCheck(TSourceLoc, const TPublicType&);
    void samplerCheck(TSourceLoc, const TType&, const TString& identifier);
    void atomicUintCheck(TSourceLoc, const TType&, const TString& identifier);
    void globalQualifierFixCheck(TSourceLoc, TQualifier&);
    void globalQualifierTypeCheck(TSourceLoc, const TQualifier&, const TPublicType&);
    bool structQualifierErrorCheck(TSourceLoc, const TPublicType& pType);
    void mergeQualifiers(TSourceLoc, TQualifier& dst, const TQualifier& src, bool force);
    void setDefaultPrecision(TSourceLoc, TPublicType&, TPrecisionQualifier);
    int computeSamplerTypeIndex(TSampler&);
    TPrecisionQualifier getDefaultPrecision(TPublicType&);
    void precisionQualifierCheck(TSourceLoc, TBasicType, TQualifier&);
    void parameterTypeCheck(TSourceLoc, TStorageQualifier qualifier, const TType& type);
    bool containsFieldWithBasicType(const TType& type ,TBasicType basicType);
    TSymbol* redeclareBuiltinVariable(TSourceLoc, const TString&, const TQualifier&, const TShaderQualifiers&, bool& newDeclaration);
    void redeclareBuiltinBlock(TSourceLoc, TTypeList& typeList, const TString& blockName, const TString* instanceName, TArraySizes* arraySizes);
    void paramCheckFix(TSourceLoc, const TStorageQualifier&, TType& type);
    void paramCheckFix(TSourceLoc, const TQualifier&, TType& type);
    void nestedBlockCheck(TSourceLoc);
    void nestedStructCheck(TSourceLoc);
    void arrayObjectCheck(TSourceLoc, const TType&, const char* op);
    void opaqueCheck(TSourceLoc, const TType&, const char* op);
    void structTypeCheck(TSourceLoc, TPublicType&);
    void inductiveLoopCheck(TSourceLoc, TIntermNode* init, TIntermLoop* loop);
    void arrayLimitCheck(TSourceLoc, const TString&, int size);
    void limitCheck(TSourceLoc, int value, const char* limit, const char* feature);

    void inductiveLoopBodyCheck(TIntermNode*, int loopIndexId, TSymbolTable&);
    void constantIndexExpressionCheck(TIntermNode*);

    void setLayoutQualifier(TSourceLoc, TPublicType&, TString&);
    void setLayoutQualifier(TSourceLoc, TPublicType&, TString&, const TIntermTyped*);
    void mergeObjectLayoutQualifiers(TQualifier& dest, const TQualifier& src, bool inheritOnly);
    void layoutObjectCheck(TSourceLoc, const TSymbol&);
    void layoutTypeCheck(TSourceLoc, const TType&);
    void layoutQualifierCheck(TSourceLoc, const TQualifier&);
    void checkNoShaderLayouts(TSourceLoc, const TShaderQualifiers&);
    void fixOffset(TSourceLoc, TSymbol&);

    const TFunction* findFunction(TSourceLoc loc, const TFunction& call, bool& builtIn);
    const TFunction* findFunctionExact(TSourceLoc loc, const TFunction& call, bool& builtIn);
    const TFunction* findFunction120(TSourceLoc loc, const TFunction& call, bool& builtIn);
    const TFunction* findFunction400(TSourceLoc loc, const TFunction& call, bool& builtIn);
    void declareTypeDefaults(TSourceLoc, const TPublicType&);
    TIntermNode* declareVariable(TSourceLoc, TString& identifier, const TPublicType&, TArraySizes* typeArray = 0, TIntermTyped* initializer = 0);
    TIntermTyped* addConstructor(TSourceLoc, TIntermNode*, const TType&, TOperator);
    TIntermTyped* constructStruct(TIntermNode*, const TType&, int, TSourceLoc);
    TIntermTyped* constructBuiltIn(const TType&, TOperator, TIntermTyped*, TSourceLoc, bool subset);
    void declareBlock(TSourceLoc, TTypeList& typeList, const TString* instanceName = 0, TArraySizes* arraySizes = 0);
    void blockStageIoCheck(TSourceLoc, const TQualifier&);
    void fixBlockLocations(TSourceLoc, TQualifier&, TTypeList&, bool memberWithLocation, bool memberWithoutLocation);
    void fixBlockXfbOffsets(TQualifier&, TTypeList&);
    void fixBlockUniformOffsets(TQualifier&, TTypeList&);
    void addQualifierToExisting(TSourceLoc, TQualifier, const TString& identifier);
    void addQualifierToExisting(TSourceLoc, TQualifier, TIdentifierList&);
    void invariantCheck(TSourceLoc, const TQualifier&);
    void updateStandaloneQualifierDefaults(TSourceLoc, const TPublicType&);
    void wrapupSwitchSubsequence(TIntermAggregate* statements, TIntermNode* branchNode);
    TIntermNode* addSwitch(TSourceLoc, TIntermTyped* expression, TIntermAggregate* body);

    void updateImplicitArraySize(TSourceLoc, TIntermNode*, int index);

    void setScanContext(TScanContext* c) { scanContext = c; }
    TScanContext* getScanContext() const { return scanContext; }
    void setPpContext(TPpContext* c) { ppContext = c; }
    TPpContext* getPpContext() const { return ppContext; }
    void addError() { ++numErrors; }
    int getNumErrors() const { return numErrors; }
    const TSourceLoc& getCurrentLoc() const { return currentScanner->getSourceLoc(); }
    void setCurrentLine(int line) { currentScanner->setLine(line); }
    void setCurrentString(int string) { currentScanner->setString(string); }
    void setScanner(TInputScanner* scanner) { currentScanner  = scanner; }

    void notifyVersion(int line, int version, const char* type_string);
    void notifyErrorDirective(int line, const char* error_message);
    void notifyLineDirective(int line, bool has_source, int source);

    // The following are implemented in Versions.cpp to localize version/profile/stage/extensions control
    void initializeExtensionBehavior();
    void requireProfile(TSourceLoc, int queryProfiles, const char* featureDesc);
    void profileRequires(TSourceLoc, int queryProfiles, int minVersion, int numExtensions, const char* const extensions[], const char* featureDesc);
    void profileRequires(TSourceLoc, int queryProfiles, int minVersion, const char* const extension, const char* featureDesc);
    void requireStage(TSourceLoc, EShLanguageMask, const char* featureDesc);
    void requireStage(TSourceLoc, EShLanguage, const char* featureDesc);
    void checkDeprecated(TSourceLoc, int queryProfiles, int depVersion, const char* featureDesc);
    void requireNotRemoved(TSourceLoc, int queryProfiles, int removedVersion, const char* featureDesc);
    void requireExtensions(TSourceLoc, int numExtensions, const char* const extensions[], const char* featureDesc);
    TExtensionBehavior getExtensionBehavior(const char*);
    bool extensionsTurnedOn(int numExtensions, const char* const extensions[]);
    void updateExtensionBehavior(int line, const char* const extension, const char* behavior);
    void fullIntegerCheck(TSourceLoc, const char* op);
    void doubleCheck(TSourceLoc, const char* op);

    void setVersionCallback(const std::function<void(int, int, const char*)>& func) { versionCallback = func; }
    void setPragmaCallback(const std::function<void(int, const TVector<TString>&)>& func) { pragmaCallback = func; }
    void setLineCallback(const std::function<void(int, bool, int)>& func) { lineCallback = func; }
    void setExtensionCallback(const std::function<void(int, const char*, const char*)>& func) { extensionCallback = func; }
    void setErrorCallback(const std::function<void(int, const char*)>& func) { errorCallback = func; }

protected:
    void nonInitConstCheck(TSourceLoc, TString& identifier, TType& type);
	void inheritGlobalDefaults(TQualifier& dst) const;
    TVariable* makeInternalVariable(const char* name, const TType&) const;
    TVariable* declareNonArray(TSourceLoc, TString& identifier, TType&, bool& newDeclaration);
    void declareArray(TSourceLoc, TString& identifier, const TType&, TSymbol*&, bool& newDeclaration);
    TIntermNode* executeInitializer(TSourceLoc, TIntermTyped* initializer, TVariable* variable);
    TIntermTyped* convertInitializerList(TSourceLoc, const TType&, TIntermTyped* initializer);
    TOperator mapTypeToConstructorOp(const TType&) const;
    void updateExtensionBehavior(const char* const extension, TExtensionBehavior);
    void finalErrorCheck();

public:
    //
    // Generally, bison productions, the scanner, and the PP need read/write access to these; just give them direct access
    //

    TIntermediate& intermediate; // helper for making and hooking up pieces of the parse tree
    TSymbolTable& symbolTable;   // symbol table that goes with the current language, version, and profile
    TInfoSink& infoSink;

    // compilation mode
    EShLanguage language;        // vertex or fragment language
    int version;                 // version, updated by #version in the shader
    EProfile profile;            // the declared profile in the shader (core by default)
    bool forwardCompatible;      // true if errors are to be given for use of deprecated features
    EShMessages messages;        // errors/warnings

    // Current state of parsing
    struct TPragma contextPragma;
    int loopNestingLevel;        // 0 if outside all loops
    int structNestingLevel;      // 0 if outside blocks and structures
    int controlFlowNestingLevel; // 0 if outside all flow control
    int statementNestingLevel;   // 0 if outside all flow control or compound statements
    TList<TIntermSequence*> switchSequenceStack;  // case, node, case, case, node, ...; ensure only one node between cases;   stack of them for nesting
    TList<int> switchLevel;      // the statementNestingLevel the current switch statement is at, which must match the level of its case statements
    bool inMain;                 // if inside a function, true if the function is main
    bool postMainReturn;         // if inside a function, true if the function is main and this is after a return statement
    const TType* currentFunctionType;  // the return type of the function that's currently being parsed
    bool functionReturnsValue;   // true if a non-void function has a return
    const TString* blockName;
    TQualifier currentBlockQualifier;
    TIntermAggregate *linkage;   // aggregate node of objects the linker may need, if not referenced by the rest of the AST
    TPrecisionQualifier defaultPrecision[EbtNumTypes];
    bool tokensBeforeEOF;
    TBuiltInResource resources;
    TLimits& limits;

protected:
    TParseContext(TParseContext&);
    TParseContext& operator=(TParseContext&);

    TScanContext* scanContext;
    TPpContext* ppContext;
    TInputScanner* currentScanner;
    int numErrors;               // number of compile-time errors encountered
    bool parsingBuiltins;        // true if parsing built-in symbols/functions
    TMap<TString, TExtensionBehavior> extensionBehavior;    // for each extension string, what its current behavior is set to
    static const int maxSamplerIndex = EsdNumDims * (EbtNumTypes * (2 * 2 * 2)); // see computeSamplerTypeIndex()
    TPrecisionQualifier defaultSamplerPrecision[maxSamplerIndex];
    bool afterEOF;
    TQualifier globalBufferDefaults;
    TQualifier globalUniformDefaults;
    TQualifier globalInputDefaults;
    TQualifier globalOutputDefaults;
    int* atomicUintOffsets;       // to become an array of the right size to hold an offset per binding point
    TString currentCaller;
    TIdSetType inductiveLoopIds;
    bool anyIndexLimits;
    TVector<TIntermTyped*> needsIndexLimitationChecking;

    //
    // Geometry shader input arrays:
    //  - array sizing is based on input primitive and/or explicit size
    //
    // Tessellation control output arrays:
    //  - array sizing is based on output layout(vertices=...) and/or explicit size
    //
    // Both:
    //  - array sizing is retroactive
    //  - built-in block redeclarations interact with this
    //
    // Design:
    //  - use a per-context "resize-list", a list of symbols whose array sizes
    //    can be fixed
    //
    //  - the resize-list starts empty at beginning of user-shader compilation, it does
    //    not have built-ins in it
    //
    //  - on built-in array use: copyUp() symbol and add it to the resize-list
    //
    //  - on user array declaration: add it to the resize-list
    //
    //  - on block redeclaration: copyUp() symbol and add it to the resize-list
    //     * note, that appropriately gives an error if redeclaring a block that
    //       was already used and hence already copied-up
    //
    //  - on seeing a layout declaration that sizes the array, fix everything in the 
    //    resize-list, giving errors for mismatch
    //
    //  - on seeing an array size declaration, give errors on mismatch between it and previous
    //    array-sizing declarations
    //
    TVector<TSymbol*> ioArraySymbolResizeList;

    // These, if set, will be called when a line, pragma ... is preprocessed.
    // They will be called with any parameters to the original directive.
    std::function<void(int, bool, int)> lineCallback;
    std::function<void(int, const TVector<TString>&)> pragmaCallback;
    std::function<void(int, int, const char*)> versionCallback;
    std::function<void(int, const char*, const char*)> extensionCallback;
    std::function<void(int, const char*)> errorCallback;
};

} // end namespace glslang

#endif // _PARSER_HELPER_INCLUDED_
