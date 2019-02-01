/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2019 Baldur Karlsson
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

#pragma once

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
  SymTagPublicSymbol = 10,
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
  virtual HRESULT STDMETHODCALLTYPE findChildren(enum SymTagEnum symtag, LPCOLESTR name,
                                                 DWORD compareFlags, IDiaEnumSymbols **ppResult) = 0;
  virtual HRESULT STDMETHODCALLTYPE findChildrenEx(enum SymTagEnum symtag, LPCOLESTR name,
                                                   DWORD compareFlags,
                                                   IDiaEnumSymbols **ppResult) = 0;
  virtual HRESULT STDMETHODCALLTYPE findChildrenExByAddr(enum SymTagEnum symtag, LPCOLESTR name,
                                                         DWORD compareFlags, DWORD isect,
                                                         DWORD offset,
                                                         IDiaEnumSymbols **ppResult) = 0;
  virtual HRESULT STDMETHODCALLTYPE findChildrenExByVA(enum SymTagEnum symtag, LPCOLESTR name,
                                                       DWORD compareFlags, ULONGLONG va,
                                                       IDiaEnumSymbols **ppResult) = 0;
  virtual HRESULT STDMETHODCALLTYPE findChildrenExByRVA(enum SymTagEnum symtag, LPCOLESTR name,
                                                        DWORD compareFlags, DWORD rva,
                                                        IDiaEnumSymbols **ppResult) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_targetSection(DWORD *pRetVal) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_targetOffset(DWORD *pRetVal) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_targetRelativeVirtualAddress(DWORD *pRetVal) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_targetVirtualAddress(ULONGLONG *pRetVal) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_machineType(DWORD *pRetVal) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_oemId(DWORD *pRetVal) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_oemSymbolId(DWORD *pRetVal) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_types(DWORD cTypes, DWORD *pcTypes, IDiaSymbol **pTypes) = 0;
  virtual HRESULT STDMETHODCALLTYPE get_typeIds(DWORD cTypeIds, DWORD *pcTypeIds,
                                                DWORD *pdwTypeIds) = 0;
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
  virtual HRESULT STDMETHODCALLTYPE findChildren(IDiaSymbol *parent, enum SymTagEnum symtag,
                                                 LPCOLESTR name, DWORD compareFlags,
                                                 IDiaEnumSymbols **ppResult) = 0;
  virtual HRESULT STDMETHODCALLTYPE findChildrenEx(IDiaSymbol *parent, enum SymTagEnum symtag,
                                                   LPCOLESTR name, DWORD compareFlags,
                                                   IDiaEnumSymbols **ppResult) = 0;
  virtual HRESULT STDMETHODCALLTYPE findChildrenExByAddr(IDiaSymbol *parent, enum SymTagEnum symtag,
                                                         LPCOLESTR name, DWORD compareFlags,
                                                         DWORD isect, DWORD offset,
                                                         IDiaEnumSymbols **ppResult) = 0;
  virtual HRESULT STDMETHODCALLTYPE findChildrenExByVA(IDiaSymbol *parent, enum SymTagEnum symtag,
                                                       LPCOLESTR name, DWORD compareFlags,
                                                       ULONGLONG va, IDiaEnumSymbols **ppResult) = 0;
  virtual HRESULT STDMETHODCALLTYPE findChildrenExByRVA(IDiaSymbol *parent, enum SymTagEnum symtag,
                                                        LPCOLESTR name, DWORD compareFlags,
                                                        DWORD rva, IDiaEnumSymbols **ppResult) = 0;
  virtual HRESULT STDMETHODCALLTYPE findSymbolByAddr(DWORD isect, DWORD offset,
                                                     enum SymTagEnum symtag,
                                                     IDiaSymbol **ppSymbol) = 0;
  virtual HRESULT STDMETHODCALLTYPE findSymbolByRVA(DWORD rva, enum SymTagEnum symtag,
                                                    IDiaSymbol **ppSymbol) = 0;
  virtual HRESULT STDMETHODCALLTYPE findSymbolByVA(ULONGLONG va, enum SymTagEnum symtag,
                                                   IDiaSymbol **ppSymbol) = 0;
  virtual HRESULT STDMETHODCALLTYPE findSymbolByToken(ULONG token, enum SymTagEnum symtag,
                                                      IDiaSymbol **ppSymbol) = 0;
  virtual HRESULT STDMETHODCALLTYPE symsAreEquiv(IDiaSymbol *symbolA, IDiaSymbol *symbolB) = 0;
  virtual HRESULT STDMETHODCALLTYPE symbolById(DWORD id, IDiaSymbol **ppSymbol) = 0;
  virtual HRESULT STDMETHODCALLTYPE findSymbolByRVAEx(DWORD rva, enum SymTagEnum symtag,
                                                      IDiaSymbol **ppSymbol, long *displacement) = 0;
  virtual HRESULT STDMETHODCALLTYPE findSymbolByVAEx(ULONGLONG va, enum SymTagEnum symtag,
                                                     IDiaSymbol **ppSymbol, long *displacement) = 0;
  virtual HRESULT STDMETHODCALLTYPE findFile(IDiaSymbol *pCompiland, LPCOLESTR name,
                                             DWORD compareFlags, IDiaEnumSourceFiles **ppResult) = 0;
  virtual HRESULT STDMETHODCALLTYPE findFileById(DWORD uniqueId, IDiaSourceFile **ppResult) = 0;
  virtual HRESULT STDMETHODCALLTYPE findLines(IDiaSymbol *compiland, IDiaSourceFile *file,
                                              IDiaEnumLineNumbers **ppResult) = 0;
  virtual HRESULT STDMETHODCALLTYPE findLinesByAddr(DWORD seg, DWORD offset, DWORD length,
                                                    IDiaEnumLineNumbers **ppResult) = 0;
  virtual HRESULT STDMETHODCALLTYPE findLinesByRVA(DWORD rva, DWORD length,
                                                   IDiaEnumLineNumbers **ppResult) = 0;
  virtual HRESULT STDMETHODCALLTYPE findLinesByVA(ULONGLONG va, DWORD length,
                                                  IDiaEnumLineNumbers **ppResult) = 0;
  virtual HRESULT STDMETHODCALLTYPE findLinesByLinenum(IDiaSymbol *compiland, IDiaSourceFile *file,
                                                       DWORD linenum, DWORD column,
                                                       IDiaEnumLineNumbers **ppResult) = 0;
  virtual HRESULT STDMETHODCALLTYPE findInjectedSource(LPCOLESTR srcFile,
                                                       IDiaEnumInjectedSources **ppResult) = 0;
  virtual HRESULT STDMETHODCALLTYPE getEnumDebugStreams(IDiaEnumDebugStreams **ppEnumDebugStreams) = 0;
};

MIDL_INTERFACE("79F1BB5F-B66E-48e5-B6A9-1545C323CA3D")
IDiaDataSource : public IUnknown
{
  virtual HRESULT STDMETHODCALLTYPE get_lastError(BSTR * pRetVal) = 0;
  virtual HRESULT STDMETHODCALLTYPE loadDataFromPdb(LPCOLESTR pdbPath) = 0;
  virtual HRESULT STDMETHODCALLTYPE loadAndValidateDataFromPdb(LPCOLESTR pdbPath, GUID * pcsig70,
                                                               DWORD sig, DWORD age) = 0;
  virtual HRESULT STDMETHODCALLTYPE loadDataForExe(LPCOLESTR executable, LPCOLESTR searchPath,
                                                   IUnknown * pCallback) = 0;
  virtual HRESULT STDMETHODCALLTYPE loadDataFromIStream(IStream * pIStream) = 0;
  virtual HRESULT STDMETHODCALLTYPE openSession(IDiaSession * *ppSession) = 0;
};

class DECLSPEC_UUID("e6756135-1e65-4d17-8576-610761398c3c") DiaSource;
