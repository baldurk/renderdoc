/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015-2016 Baldur Karlsson
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

#include <windows.h>
#include <string>

#include <stdio.h>
#include <stdint.h>
#include <wchar.h>
#include <tchar.h>

#include <assert.h>

#include "dbghelp/dbghelp.h"
#include <shlobj.h>

#include <vector>
using std::vector;
using std::wstring;

// Inline the couple of necessary definitions from dia2.h below
//#include <dia2.h>

// don't need these 
struct IDiaEnumSymbols;
struct IDiaEnumTables;
struct IDiaEnumSymbolsByAddr;
struct IDiaEnumSourceFiles;
struct IDiaEnumInjectedSources;
struct IDiaEnumDebugStreams;

enum SymTagEnum
{
    SymTagFunction = 5,
};

struct IDiaSourceFile : public IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE get_uniqueId(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_fileName(BSTR *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_checksumType(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_compilands(IDiaEnumSymbols **pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_checksum(DWORD cbData, DWORD *pcbData, BYTE *pbData) = 0;
};

struct IDiaSymbol : public IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE get_symIndexId(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_symTag(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_name(BSTR *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_lexicalParent(IDiaSymbol **pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_classParent(IDiaSymbol **pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_type(IDiaSymbol **pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_dataKind(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_locationType(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_addressSection(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_addressOffset(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_relativeVirtualAddress(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_virtualAddress(ULONGLONG *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_registerId(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_offset(LONG *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_length(ULONGLONG *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_slot(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_volatileType(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_constType(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_unalignedType(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_access(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_libraryName(BSTR *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_platform(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_language(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_editAndContinueEnabled(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_frontEndMajor(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_frontEndMinor(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_frontEndBuild(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_backEndMajor(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_backEndMinor(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_backEndBuild(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_sourceFileName(BSTR *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_unused(BSTR *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_thunkOrdinal(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_thisAdjust(LONG *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_virtualBaseOffset(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_virtual(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_intro(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_pure(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_callingConvention(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_value(VARIANT *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_baseType(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_token(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_timeStamp(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_guid(GUID *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_symbolsFileName(BSTR *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_reference(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_count(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_bitPosition(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_arrayIndexType(IDiaSymbol **pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_packed(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_constructor(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_overloadedOperator(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_nested(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_hasNestedTypes(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_hasAssignmentOperator(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_hasCastOperator(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_scoped(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_virtualBaseClass(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_indirectVirtualBaseClass(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_virtualBasePointerOffset(LONG *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_virtualTableShape(IDiaSymbol **pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_lexicalParentId(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_classParentId(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_typeId(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_arrayIndexTypeId(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_virtualTableShapeId(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_code(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_function(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_managed(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_msil(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_virtualBaseDispIndex(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_undecoratedName(BSTR *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_age(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_signature(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_compilerGenerated(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_addressTaken(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_rank(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_lowerBound(IDiaSymbol **pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_upperBound(IDiaSymbol **pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_lowerBoundId(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_upperBoundId(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_dataBytes(DWORD cbData, DWORD *pcbData, BYTE *pbData) = 0;
	virtual HRESULT STDMETHODCALLTYPE findChildren(enum SymTagEnum symtag, LPCOLESTR name, DWORD compareFlags, IDiaEnumSymbols **ppResult) = 0;
	virtual HRESULT STDMETHODCALLTYPE findChildrenEx(enum SymTagEnum symtag, LPCOLESTR name, DWORD compareFlags, IDiaEnumSymbols **ppResult) = 0;
	virtual HRESULT STDMETHODCALLTYPE findChildrenExByAddr(enum SymTagEnum symtag, LPCOLESTR name, DWORD compareFlags, DWORD isect, DWORD offset, IDiaEnumSymbols **ppResult) = 0;
	virtual HRESULT STDMETHODCALLTYPE findChildrenExByVA(enum SymTagEnum symtag, LPCOLESTR name, DWORD compareFlags, ULONGLONG va, IDiaEnumSymbols **ppResult) = 0;
	virtual HRESULT STDMETHODCALLTYPE findChildrenExByRVA(enum SymTagEnum symtag, LPCOLESTR name, DWORD compareFlags, DWORD rva, IDiaEnumSymbols **ppResult) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_targetSection(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_targetOffset(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_targetRelativeVirtualAddress(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_targetVirtualAddress(ULONGLONG *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_machineType(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_oemId(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_oemSymbolId(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_types(DWORD cTypes, DWORD *pcTypes, IDiaSymbol **pTypes) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_typeIds(DWORD cTypeIds, DWORD *pcTypeIds, DWORD *pdwTypeIds) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_objectPointerType(IDiaSymbol **pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_udtKind(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_undecoratedNameEx(DWORD undecorateOptions, BSTR *name) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_noReturn(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_customCallingConvention(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_noInline(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_optimizedCodeDebugInfo(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_notReached(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_interruptReturn(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_farReturn(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_isStatic(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_hasDebugInfo(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_isLTCG(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_isDataAligned(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_hasSecurityChecks(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_compilerName(BSTR *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_hasAlloca(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_hasSetJump(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_hasLongJump(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_hasInlAsm(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_hasEH(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_hasSEH(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_hasEHa(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_isNaked(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_isAggregated(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_isSplitted(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_container(IDiaSymbol **pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_inlSpec(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_noStackOrdering(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_virtualBaseTableType(IDiaSymbol **pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_hasManagedCode(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_isHotpatchable(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_isCVTCIL(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_isMSILNetmodule(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_isCTypes(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_isStripped(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_frontEndQFE(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_backEndQFE(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_wasInlined(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_strictGSCheck(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_isCxxReturnUdt(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_isConstructorVirtualBase(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_RValueReference(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_unmodifiedType(IDiaSymbol **pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_framePointerPresent(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_isSafeBuffers(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_intrinsic(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_sealed(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_hfaFloat(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_hfaDouble(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_liveRangeStartAddressSection(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_liveRangeStartAddressOffset(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_liveRangeStartRelativeVirtualAddress(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_countLiveRanges(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_liveRangeLength(ULONGLONG *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_offsetInUdt(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_paramBasePointerRegisterId(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_localBasePointerRegisterId(DWORD *pRetVal) = 0;
};

struct IDiaLineNumber : public IUnknown
{
public:
	virtual HRESULT STDMETHODCALLTYPE get_compiland(IDiaSymbol **pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_sourceFile(IDiaSourceFile **pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_lineNumber(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_lineNumberEnd(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_columnNumber(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_columnNumberEnd(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_addressSection(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_addressOffset(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_relativeVirtualAddress(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_virtualAddress(ULONGLONG *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_length(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_sourceFileId(DWORD *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_statement(BOOL *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_compilandId(DWORD *pRetVal) = 0;
};

struct IDiaEnumLineNumbers : public IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE get__NewEnum(IUnknown **pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_Count(LONG *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE Item(DWORD index, IDiaLineNumber **lineNumber) = 0;
	virtual HRESULT STDMETHODCALLTYPE Next(ULONG celt, IDiaLineNumber **rgelt, ULONG *pceltFetched) = 0;
	virtual HRESULT STDMETHODCALLTYPE Skip(ULONG celt) = 0;
	virtual HRESULT STDMETHODCALLTYPE Reset(void) = 0;
	virtual HRESULT STDMETHODCALLTYPE Clone(IDiaEnumLineNumbers **ppenum) = 0;
};

struct IDiaSession : public IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE get_loadAddress(ULONGLONG *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE put_loadAddress(ULONGLONG NewVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE get_globalScope(IDiaSymbol **pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE getEnumTables(IDiaEnumTables **ppEnumTables) = 0;
	virtual HRESULT STDMETHODCALLTYPE getSymbolsByAddr(IDiaEnumSymbolsByAddr **ppEnumbyAddr) = 0;
	virtual HRESULT STDMETHODCALLTYPE findChildren(IDiaSymbol *parent, enum SymTagEnum symtag, LPCOLESTR name, DWORD compareFlags, IDiaEnumSymbols **ppResult) = 0;
	virtual HRESULT STDMETHODCALLTYPE findChildrenEx(IDiaSymbol *parent, enum SymTagEnum symtag, LPCOLESTR name, DWORD compareFlags, IDiaEnumSymbols **ppResult) = 0;
	virtual HRESULT STDMETHODCALLTYPE findChildrenExByAddr(IDiaSymbol *parent, enum SymTagEnum symtag, LPCOLESTR name, DWORD compareFlags, DWORD isect, DWORD offset, IDiaEnumSymbols **ppResult) = 0;
	virtual HRESULT STDMETHODCALLTYPE findChildrenExByVA(IDiaSymbol *parent, enum SymTagEnum symtag, LPCOLESTR name, DWORD compareFlags, ULONGLONG va, IDiaEnumSymbols **ppResult) = 0;
	virtual HRESULT STDMETHODCALLTYPE findChildrenExByRVA(IDiaSymbol *parent, enum SymTagEnum symtag, LPCOLESTR name, DWORD compareFlags, DWORD rva, IDiaEnumSymbols **ppResult) = 0;
	virtual HRESULT STDMETHODCALLTYPE findSymbolByAddr(DWORD isect, DWORD offset, enum SymTagEnum symtag, IDiaSymbol **ppSymbol) = 0;
	virtual HRESULT STDMETHODCALLTYPE findSymbolByRVA(DWORD rva, enum SymTagEnum symtag, IDiaSymbol **ppSymbol) = 0;
	virtual HRESULT STDMETHODCALLTYPE findSymbolByVA(ULONGLONG va, enum SymTagEnum symtag, IDiaSymbol **ppSymbol) = 0;
	virtual HRESULT STDMETHODCALLTYPE findSymbolByToken(ULONG token, enum SymTagEnum symtag, IDiaSymbol **ppSymbol) = 0;
	virtual HRESULT STDMETHODCALLTYPE symsAreEquiv(IDiaSymbol *symbolA, IDiaSymbol *symbolB) = 0;
	virtual HRESULT STDMETHODCALLTYPE symbolById(DWORD id, IDiaSymbol **ppSymbol) = 0;
	virtual HRESULT STDMETHODCALLTYPE findSymbolByRVAEx(DWORD rva, enum SymTagEnum symtag, IDiaSymbol **ppSymbol, long *displacement) = 0;
	virtual HRESULT STDMETHODCALLTYPE findSymbolByVAEx(ULONGLONG va, enum SymTagEnum symtag, IDiaSymbol **ppSymbol, long *displacement) = 0;
	virtual HRESULT STDMETHODCALLTYPE findFile(IDiaSymbol *pCompiland, LPCOLESTR name, DWORD compareFlags, IDiaEnumSourceFiles **ppResult) = 0;
	virtual HRESULT STDMETHODCALLTYPE findFileById(DWORD uniqueId, IDiaSourceFile **ppResult) = 0;
	virtual HRESULT STDMETHODCALLTYPE findLines(IDiaSymbol *compiland, IDiaSourceFile *file, IDiaEnumLineNumbers **ppResult) = 0;
	virtual HRESULT STDMETHODCALLTYPE findLinesByAddr(DWORD seg, DWORD offset, DWORD length, IDiaEnumLineNumbers **ppResult) = 0;
	virtual HRESULT STDMETHODCALLTYPE findLinesByRVA(DWORD rva, DWORD length, IDiaEnumLineNumbers **ppResult) = 0;
	virtual HRESULT STDMETHODCALLTYPE findLinesByVA(ULONGLONG va, DWORD length, IDiaEnumLineNumbers **ppResult) = 0;
	virtual HRESULT STDMETHODCALLTYPE findLinesByLinenum(IDiaSymbol *compiland, IDiaSourceFile *file, DWORD linenum, DWORD column, IDiaEnumLineNumbers **ppResult) = 0;
	virtual HRESULT STDMETHODCALLTYPE findInjectedSource(LPCOLESTR srcFile, IDiaEnumInjectedSources **ppResult) = 0;
	virtual HRESULT STDMETHODCALLTYPE getEnumDebugStreams(IDiaEnumDebugStreams **ppEnumDebugStreams) = 0;
};

MIDL_INTERFACE("79F1BB5F-B66E-48e5-B6A9-1545C323CA3D")
IDiaDataSource : public IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE get_lastError(BSTR *pRetVal) = 0;
	virtual HRESULT STDMETHODCALLTYPE loadDataFromPdb(LPCOLESTR pdbPath) = 0;
	virtual HRESULT STDMETHODCALLTYPE loadAndValidateDataFromPdb(LPCOLESTR pdbPath, GUID *pcsig70, DWORD sig, DWORD age) = 0;
	virtual HRESULT STDMETHODCALLTYPE loadDataForExe(LPCOLESTR executable, LPCOLESTR searchPath, IUnknown *pCallback) = 0;
	virtual HRESULT STDMETHODCALLTYPE loadDataFromIStream(IStream *pIStream) = 0;
	virtual HRESULT STDMETHODCALLTYPE openSession(IDiaSession **ppSession) = 0;
};

class DECLSPEC_UUID("B86AE24D-BF2F-4ac9-B5A2-34B14E4CE11D") DiaSource;

// must match definition in callstack.h
struct AddrInfo
{
	wchar_t funcName[127];
	wchar_t fileName[127];
	unsigned long lineNum;
};

struct Module
{
	Module(IDiaDataSource* src, IDiaSession* sess) :
		pSource(src), pSession(sess) {}

	IDiaDataSource* pSource;
	IDiaSession* pSession;
};

vector<Module> modules;

typedef BOOL(WINAPI *PSYMINITIALIZEW)(
	__in HANDLE hProcess,
	__in_opt PCWSTR UserSearchPath,
	__in BOOL fInvadeProcess);
typedef BOOL(WINAPI *PSYMFINDFILEINPATHW)(
	__in HANDLE hprocess,
	__in_opt PCWSTR SearchPath,
	__in PCWSTR FileName,
	__in_opt PVOID id,
	__in DWORD two,
	__in DWORD three,
	__in DWORD flags,
	__out_ecount(MAX_PATH + 1) PWSTR FoundFile,
	__in_opt PFINDFILEINPATHCALLBACKW callback,
	__in_opt PVOID context);

PSYMINITIALIZEW dynSymInitializeW = NULL;
PSYMFINDFILEINPATHW dynSymFindFileInPathW = NULL;

wstring GetSymSearchPath()
{
	PWSTR appDataPath;
	SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_SIMPLE_IDLIST|KF_FLAG_DONT_UNEXPAND, NULL, &appDataPath);
	wstring appdata = appDataPath;
	CoTaskMemFree(appDataPath);

	wstring sympath = L".;";
	sympath += appdata;
	sympath += L"\\renderdoc\\symbols;SRV*";
	sympath += appdata;
	sympath += L"\\renderdoc\\symbols\\symsrv*http://msdl.microsoft.com/download/symbols";

	return sympath;
}

wstring LookupModule(wstring moduleDetails)
{
	uint32_t params[12];
	int charsRead = 0;
	swscanf_s(moduleDetails.c_str(), L"%d  %d %d %d  %d %d %d %d %d %d %d %d%n",
		&params[0], &params[1], &params[2], &params[3], &params[4], &params[5],
		&params[6], &params[7], &params[8], &params[9], &params[10], &params[11], &charsRead);

	wchar_t *modName = (wchar_t *)moduleDetails.c_str() + charsRead + 1;

	while(*modName != L'\0' && iswspace(*modName)) modName++;

	DWORD age = params[0];
	GUID guid;
	guid.Data1 = params[1];
	guid.Data2 = params[2];
	guid.Data3 = params[3];
	guid.Data4[0] = params[4];
	guid.Data4[1] = params[5];
	guid.Data4[2] = params[6];
	guid.Data4[3] = params[7];
	guid.Data4[4] = params[8];
	guid.Data4[5] = params[9];
	guid.Data4[6] = params[10];
	guid.Data4[7] = params[11];

	wchar_t *pdbName = modName;

	if(wcsrchr(pdbName, L'\\'))
		pdbName = wcsrchr(pdbName, L'\\')+1;

	if(wcsrchr(pdbName, L'/'))
		pdbName = wcsrchr(pdbName, L'/')+1;
	
	if(wcsstr(pdbName, L".pdb") == NULL &&
		wcsstr(pdbName, L".PDB") == NULL)
	{
		wchar_t *ext = wcsrchr(pdbName, L'.');

		if(ext)
		{
			ext[1] = L'p';
			ext[2] = L'd';
			ext[3] = L'b';
		}
	}
	
	wstring ret = modName;

	if(dynSymFindFileInPathW != NULL)
	{
		wstring sympath = GetSymSearchPath();

		wchar_t path[MAX_PATH+1] = {0};
		BOOL found = dynSymFindFileInPathW(GetCurrentProcess(), sympath.c_str(), pdbName, &guid, age, 0, SSRVOPT_GUIDPTR, path, NULL, NULL);
		DWORD err = GetLastError();

		if(found == TRUE && path[0] != 0)
			ret = path;
	}

	return ret;
}

uint32_t GetModule(wstring moduleDetails)
{
	uint32_t params[12];
	int charsRead = 0;
	swscanf_s(moduleDetails.c_str(), L"%d  %d %d %d  %d %d %d %d %d %d %d %d%n",
		&params[0], &params[1], &params[2], &params[3], &params[4], &params[5],
		&params[6], &params[7], &params[8], &params[9], &params[10], &params[11], &charsRead);

	wchar_t *pdbName = (wchar_t *)moduleDetails.c_str() + charsRead + 1;

	while(*pdbName != L'\0' && iswspace(*pdbName)) pdbName++;

	DWORD age = params[0];
	GUID guid;
	guid.Data1 = params[1];
	guid.Data2 = params[2];
	guid.Data3 = params[3];
	guid.Data4[0] = params[4];
	guid.Data4[1] = params[5];
	guid.Data4[2] = params[6];
	guid.Data4[3] = params[7];
	guid.Data4[4] = params[8];
	guid.Data4[5] = params[9];
	guid.Data4[6] = params[10];
	guid.Data4[7] = params[11];
	
	Module m(NULL, NULL);
	
	CoCreateInstance(__uuidof(DiaSource), NULL, CLSCTX_INPROC_SERVER, __uuidof(IDiaDataSource), (void **)&m.pSource);

	HRESULT hr = S_OK;

	// check this pdb is the one we expected from our chunk
	if(guid.Data1 == 0 && guid.Data2 == 0)
	{
		hr = m.pSource->loadDataFromPdb( pdbName );
	}
	else
	{
		hr = m.pSource->loadAndValidateDataFromPdb( pdbName, &guid, 0, age);
	}

	if(SUCCEEDED(hr))
	{
		// open the session
		hr = m.pSource->openSession( &m.pSession );
		if (FAILED(hr))
		{
			m.pSource->Release();
			return 0;
		}

		modules.push_back(m);

		return modules.size()-1;
	}
	
	m.pSource->Release();

	return 0;
}

void SetBaseAddress(wstring req)
{
	uint32_t module;
	uint64_t addr;
	int charsRead = swscanf_s(req.c_str(), L"%d %llu", &module, &addr);

	if(module > 0 && module < modules.size())
		modules[module].pSession->put_loadAddress(addr);
}

AddrInfo GetAddr(wstring req)
{
	uint32_t module;
	uint64_t addr;
	int charsRead = swscanf_s(req.c_str(), L"%d %llu", &module, &addr);

	AddrInfo ret;
	ZeroMemory(&ret, sizeof(ret));

	if(module > 0 && module < modules.size())
	{
		IDiaSymbol* pFunc = NULL;
		HRESULT hr = modules[module].pSession->findSymbolByVA( addr, SymTagFunction, &pFunc );

		if(hr != S_OK)
		{
			if(pFunc) pFunc->Release();
			return ret;
		}

		DWORD opts = 0;
		opts |= UNDNAME_NO_LEADING_UNDERSCORES;
		opts |= UNDNAME_NO_MS_KEYWORDS;
		opts |= UNDNAME_NO_FUNCTION_RETURNS;
		opts |= UNDNAME_NO_ALLOCATION_MODEL;
		opts |= UNDNAME_NO_ALLOCATION_LANGUAGE;
		opts |= UNDNAME_NO_THISTYPE;
		opts |= UNDNAME_NO_ACCESS_SPECIFIERS;
		opts |= UNDNAME_NO_THROW_SIGNATURES;
		opts |= UNDNAME_NO_MEMBER_TYPE;
		opts |= UNDNAME_NO_RETURN_UDT_MODEL;
		opts |= UNDNAME_32_BIT_DECODE;
		opts |= UNDNAME_NO_LEADING_UNDERSCORES;

		// first try undecorated name
		BSTR file;
		hr = pFunc->get_undecoratedNameEx(opts, &file);

		// if not, just try name
		if(hr != S_OK)
		{
			hr = pFunc->get_name(&file);

			if(hr != S_OK)
			{
				pFunc->Release();
				SysFreeString(file);
				return ret;
			}

			wcsncpy_s(ret.funcName, file, 126);
		}
		else
		{
			wcsncpy_s(ret.funcName, file, 126);

			wchar_t *voidparam = wcsstr(ret.funcName, L"(void)");

			// remove stupid (void) for empty parameters
			if (voidparam != NULL)
			{
				*(voidparam + 1) = L')';
				*(voidparam + 2) = 0;
			}
		}

		pFunc->Release();
		pFunc = NULL;

		SysFreeString(file);

		// find the line numbers touched by this address.
		IDiaEnumLineNumbers* lines = NULL;
		hr = modules[module].pSession->findLinesByVA(addr, DWORD(4), &lines);
		if(FAILED(hr))
		{
			if(lines) lines->Release();
			return ret;
		}

		IDiaLineNumber* line = NULL;
		ULONG count = 0;

		// just take the first one
		if(SUCCEEDED(lines->Next(1, &line, &count)) && count == 1)
		{
			IDiaSourceFile *dia_source = NULL;
			hr = line->get_sourceFile(&dia_source);
			if(FAILED(hr))
			{
				line->Release();
				lines->Release();
				if(dia_source) dia_source->Release();
				return ret;
			}

			hr = dia_source->get_fileName(&file);
			if(FAILED(hr))
			{
				line->Release();
				lines->Release();
				dia_source->Release();
				return ret;
			}

			wcsncpy_s(ret.fileName, file, 126);

			SysFreeString(file);

			dia_source->Release();
			dia_source = NULL;

			DWORD line_num = 0;
			hr = line->get_lineNumber(&line_num);
			if(FAILED(hr))
			{
				line->Release();
				lines->Release();
				return ret;
			}

			ret.lineNum = line_num;

			line->Release();
		}

		lines->Release();
	}

	return ret;
}

wstring HandleRequest(wstring req)
{
	size_t idx = req.find(L' ');

	if(idx == wstring::npos)
		return L".";

	wstring type = req.substr(0, idx);
	wstring payload = req.substr(idx+1);

	if(type == L"lookup")
		return LookupModule(payload);

	if(type == L"baseaddr")
	{
		SetBaseAddress(payload);
		return L".";
	}

	if(type == L"getmodule")
	{
		wstring ret;
		ret.resize(4);

		uint32_t *output = (uint32_t *)&ret[0];

		*output = GetModule(payload);

		return ret;
	}

	if(type == L"getaddr")
	{
		wstring ret;
		ret.resize(sizeof(AddrInfo)/sizeof(wchar_t));

		AddrInfo info = GetAddr(payload);

		memcpy(&ret[0], &info, sizeof(AddrInfo));

		return ret;
	}
	
	return L".";
}

int WINAPI wWinMain( __in HINSTANCE hInstance, __in_opt HINSTANCE hPrevInstance, __in LPWSTR lpCmdLine, __in int nShowCmd )
{
	modules.push_back(Module(NULL, NULL));

	// CreatePipe
	HANDLE pipe = CreateNamedPipeW( L"\\\\.\\pipe\\RenderDoc.pdblocate", PIPE_ACCESS_DUPLEX,
									PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
									1, 1024, 1024, 0, NULL);

	if(pipe == INVALID_HANDLE_VALUE)
		return 1;

	BOOL connected = ConnectNamedPipe(pipe, NULL);
	if(!connected && GetLastError() == ERROR_PIPE_CONNECTED)
		connected = true;

	if(!connected)
	{
		CloseHandle(pipe);
		return 1;
	}
		
	CoInitialize(NULL);
	
	HMODULE mod = LoadLibraryW(L"x86/dbghelp.dll");

	if(mod != NULL)
	{
		dynSymInitializeW = (PSYMINITIALIZEW)GetProcAddress(mod, "SymInitializeW");
		dynSymFindFileInPathW = (PSYMFINDFILEINPATHW)GetProcAddress(mod, "SymFindFileInPathW");
		
		wstring sympath = GetSymSearchPath();

		if(dynSymInitializeW != NULL)
		{
			dynSymInitializeW(GetCurrentProcess(), sympath.c_str(), TRUE);
		}
	}

	wchar_t buf[1024];

	while(true)
	{
		DWORD read = 0;
		BOOL success = ReadFile(pipe, buf, 1024, &read, NULL);

		if(!success || read == 0)
		{
			DWORD err = GetLastError();
			break;
		}

		wstring request(buf, buf+read/sizeof(wchar_t));
		if(request.back() != L'\0')
			request.push_back(L'\0');

		wstring reply = HandleRequest(request);

		reply.push_back(L'\0');

		DWORD msglen = reply.length()*sizeof(wchar_t);

		DWORD written = 0;
		success = WriteFile(pipe, reply.c_str(), msglen, &written, NULL);

		if(!success || written != msglen)
		{
			DWORD err = GetLastError();
			break;
		}
	}	

	if(mod != NULL)
		FreeLibrary(mod);

	CloseHandle(pipe);
	return 0;
}
