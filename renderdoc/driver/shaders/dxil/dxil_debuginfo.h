/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Baldur Karlsson
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

#include "dxil_bytecode.h"

namespace DXIL
{
enum DW_LANG
{
  DW_LANG_Unknown = 0,
  DW_LANG_C89 = 0x0001,
  DW_LANG_C = 0x0002,
  DW_LANG_Ada83 = 0x0003,
  DW_LANG_C_plus_plus = 0x0004,
  DW_LANG_Cobol74 = 0x0005,
  DW_LANG_Cobol85 = 0x0006,
  DW_LANG_Fortran77 = 0x0007,
  DW_LANG_Fortran90 = 0x0008,
  DW_LANG_Pascal83 = 0x0009,
  DW_LANG_Modula2 = 0x000a,
  DW_LANG_Java = 0x000b,
  DW_LANG_C99 = 0x000c,
  DW_LANG_Ada95 = 0x000d,
  DW_LANG_Fortran95 = 0x000e,
  DW_LANG_PLI = 0x000f,
  DW_LANG_ObjC = 0x0010,
  DW_LANG_ObjC_plus_plus = 0x0011,
  DW_LANG_UPC = 0x0012,
  DW_LANG_D = 0x0013,
  DW_LANG_Python = 0x0014,
  DW_LANG_OpenCL = 0x0015,
  DW_LANG_Go = 0x0016,
  DW_LANG_Modula3 = 0x0017,
  DW_LANG_Haskell = 0x0018,
  DW_LANG_C_plus_plus_03 = 0x0019,
  DW_LANG_C_plus_plus_11 = 0x001a,
  DW_LANG_OCaml = 0x001b,
  DW_LANG_Rust = 0x001c,
  DW_LANG_C11 = 0x001d,
  DW_LANG_Swift = 0x001e,
  DW_LANG_Julia = 0x001f,
  DW_LANG_Dylan = 0x0020,
  DW_LANG_C_plus_plus_14 = 0x0021,
  DW_LANG_Fortran03 = 0x0022,
  DW_LANG_Fortran08 = 0x0023,
  DW_LANG_Mips_Assembler = 0x8001,
};

enum DW_TAG
{
  DW_TAG_array_type = 0x0001,
  DW_TAG_class_type = 0x0002,
  DW_TAG_entry_point = 0x0003,
  DW_TAG_enumeration_type = 0x0004,
  DW_TAG_formal_parameter = 0x0005,
  DW_TAG_imported_declaration = 0x0008,
  DW_TAG_label = 0x000a,
  DW_TAG_lexical_block = 0x000b,
  DW_TAG_member = 0x000d,
  DW_TAG_pointer_type = 0x000f,
  DW_TAG_reference_type = 0x0010,
  DW_TAG_compile_unit = 0x0011,
  DW_TAG_string_type = 0x0012,
  DW_TAG_structure_type = 0x0013,
  DW_TAG_subroutine_type = 0x0015,
  DW_TAG_typedef = 0x0016,
  DW_TAG_union_type = 0x0017,
  DW_TAG_unspecified_parameters = 0x0018,
  DW_TAG_variant = 0x0019,
  DW_TAG_common_block = 0x001a,
  DW_TAG_common_inclusion = 0x001b,
  DW_TAG_inheritance = 0x001c,
  DW_TAG_inlined_subroutine = 0x001d,
  DW_TAG_module = 0x001e,
  DW_TAG_ptr_to_member_type = 0x001f,
  DW_TAG_set_type = 0x0020,
  DW_TAG_subrange_type = 0x0021,
  DW_TAG_with_stmt = 0x0022,
  DW_TAG_access_declaration = 0x0023,
  DW_TAG_base_type = 0x0024,
  DW_TAG_catch_block = 0x0025,
  DW_TAG_const_type = 0x0026,
  DW_TAG_constant = 0x0027,
  DW_TAG_enumerator = 0x0028,
  DW_TAG_file_type = 0x0029,
  DW_TAG_friend = 0x002a,
  DW_TAG_namelist = 0x002b,
  DW_TAG_namelist_item = 0x002c,
  DW_TAG_packed_type = 0x002d,
  DW_TAG_subprogram = 0x002e,
  DW_TAG_template_type_parameter = 0x002f,
  DW_TAG_template_value_parameter = 0x0030,
  DW_TAG_thrown_type = 0x0031,
  DW_TAG_try_block = 0x0032,
  DW_TAG_variant_part = 0x0033,
  DW_TAG_variable = 0x0034,
  DW_TAG_volatile_type = 0x0035,
  DW_TAG_dwarf_procedure = 0x0036,
  DW_TAG_restrict_type = 0x0037,
  DW_TAG_interface_type = 0x0038,
  DW_TAG_namespace = 0x0039,
  DW_TAG_imported_module = 0x003a,
  DW_TAG_unspecified_type = 0x003b,
  DW_TAG_partial_unit = 0x003c,
  DW_TAG_imported_unit = 0x003d,
  DW_TAG_condition = 0x003f,
  DW_TAG_shared_type = 0x0040,
  DW_TAG_type_unit = 0x0041,
  DW_TAG_rvalue_reference_type = 0x0042,
  DW_TAG_template_alias = 0x0043,
  DW_TAG_auto_variable = 0x0100,
  DW_TAG_arg_variable = 0x0101,
  DW_TAG_coarray_type = 0x0044,
  DW_TAG_generic_subrange = 0x0045,
  DW_TAG_dynamic_type = 0x0046,
  DW_TAG_MIPS_loop = 0x4081,
  DW_TAG_format_label = 0x4101,
  DW_TAG_function_template = 0x4102,
  DW_TAG_class_template = 0x4103,
  DW_TAG_GNU_template_template_param = 0x4106,
  DW_TAG_GNU_template_parameter_pack = 0x4107,
  DW_TAG_GNU_formal_parameter_pack = 0x4108,
  DW_TAG_APPLE_property = 0x4200,
};

enum DW_ENCODING
{
  DW_ATE_address = 0x01,
  DW_ATE_boolean = 0x02,
  DW_ATE_complex_float = 0x03,
  DW_ATE_float = 0x04,
  DW_ATE_signed = 0x05,
  DW_ATE_signed_char = 0x06,
  DW_ATE_unsigned = 0x07,
  DW_ATE_unsigned_char = 0x08,
  DW_ATE_imaginary_float = 0x09,
  DW_ATE_packed_decimal = 0x0a,
  DW_ATE_numeric_string = 0x0b,
  DW_ATE_edited = 0x0c,
  DW_ATE_signed_fixed = 0x0d,
  DW_ATE_unsigned_fixed = 0x0e,
  DW_ATE_decimal_float = 0x0f,
  DW_ATE_UTF = 0x10,
};

enum DW_VIRTUALITY
{
  DW_VIRTUALITY_none = 0x00,
  DW_VIRTUALITY_virtual = 0x01,
  DW_VIRTUALITY_pure_virtual = 0x02,
};

enum DIFlags
{
  DIFlagPrivate = 1,
  DIFlagProtected = 2,
  DIFlagPublic = 3,
  DIFlagFwdDecl = (1 << 2),
  DIFlagAppleBlock = (1 << 3),
  DIFlagBlockByrefStruct = (1 << 4),
  DIFlagVirtual = (1 << 5),
  DIFlagArtificial = (1 << 6),
  DIFlagExplicit = (1 << 7),
  DIFlagPrototyped = (1 << 8),
  DIFlagObjcClassComplete = (1 << 9),
  DIFlagObjectPointer = (1 << 10),
  DIFlagVector = (1 << 11),
  DIFlagStaticMember = (1 << 12),
  DIFlagLValueReference = (1 << 13),
  DIFlagRValueReference = (1 << 14),
};

struct DIFile : public DIBase
{
  static const DIBase::Type DIType = DIBase::File;
  DIFile(const Metadata *file, const Metadata *dir) : DIBase(DIType), file(file), dir(dir) {}
  const Metadata *file;
  const Metadata *dir;

  virtual rdcstr toString() const;
};

struct DICompileUnit : public DIBase
{
  static const DIBase::Type DIType = DIBase::CompileUnit;
  DICompileUnit(DW_LANG lang, const Metadata *file, const rdcstr *producer, bool isOptimized,
                const rdcstr *flags, uint64_t runtimeVersion, const rdcstr *splitDebugFilename,
                uint64_t emissionKind, const Metadata *enums, const Metadata *retainedTypes,
                const Metadata *subprograms, const Metadata *globals, const Metadata *imports)
      : DIBase(DIType),
        lang(lang),
        file(file),
        producer(producer),
        isOptimized(isOptimized),
        flags(flags),
        runtimeVersion(runtimeVersion),
        splitDebugFilename(splitDebugFilename),
        emissionKind(emissionKind),
        enums(enums),
        retainedTypes(retainedTypes),
        subprograms(subprograms),
        globals(globals),
        imports(imports)
  {
  }

  DW_LANG lang;
  const Metadata *file;
  const rdcstr *producer;
  bool isOptimized;
  const rdcstr *flags;
  uint64_t runtimeVersion;
  const rdcstr *splitDebugFilename;
  uint64_t emissionKind;
  const Metadata *enums;
  const Metadata *retainedTypes;
  const Metadata *subprograms;
  const Metadata *globals;
  const Metadata *imports;

  virtual rdcstr toString() const;
};

struct DIBasicType : public DIBase
{
  static const DIBase::Type DIType = DIBase::BasicType;
  DIBasicType(DW_TAG tag, const rdcstr *name, uint64_t sizeInBits, uint64_t alignInBits,
              DW_ENCODING encoding)
      : DIBase(DIType),
        tag(tag),
        name(name),
        sizeInBits(sizeInBits),
        alignInBits(alignInBits),
        encoding(encoding)
  {
  }

  DW_TAG tag;
  const rdcstr *name;
  uint64_t sizeInBits;
  uint64_t alignInBits;
  DW_ENCODING encoding;

  virtual rdcstr toString() const;
};

struct DIDerivedType : public DIBase
{
  static const DIBase::Type DIType = DIBase::DerivedType;
  DIDerivedType(DW_TAG tag, const rdcstr *name, const Metadata *file, uint64_t line,
                const Metadata *scope, const Metadata *base, uint64_t sizeInBits,
                uint64_t alignInBits, uint64_t offsetInBits, DIFlags flags, const Metadata *extra)
      : DIBase(DIType),
        tag(tag),
        name(name),
        file(file),
        line(line),
        scope(scope),
        base(base),
        sizeInBits(sizeInBits),
        alignInBits(alignInBits),
        offsetInBits(offsetInBits),
        flags(flags),
        extra(extra)
  {
  }

  DW_TAG tag;
  const rdcstr *name;
  const Metadata *file;
  uint64_t line;
  const Metadata *scope;
  const Metadata *base;
  uint64_t sizeInBits;
  uint64_t alignInBits;
  uint64_t offsetInBits;
  DIFlags flags;
  const Metadata *extra;

  virtual rdcstr toString() const;
};

struct DICompositeType : public DIBase
{
  static const DIBase::Type DIType = DIBase::CompositeType;
  DICompositeType(DW_TAG tag, const rdcstr *name, const Metadata *file, uint64_t line,
                  const Metadata *scope, const Metadata *base, uint64_t sizeInBits,
                  uint64_t alignInBits, uint64_t offsetInBits, DIFlags flags,
                  const Metadata *elements, const Metadata *templateParams)
      : DIBase(DIType),
        tag(tag),
        name(name),
        file(file),
        line(line),
        scope(scope),
        base(base),
        sizeInBits(sizeInBits),
        alignInBits(alignInBits),
        offsetInBits(offsetInBits),
        flags(flags),
        elements(elements),
        templateParams(templateParams)
  {
  }

  DW_TAG tag;
  const rdcstr *name;
  const Metadata *file;
  uint64_t line;
  const Metadata *scope;
  const Metadata *base;
  uint64_t sizeInBits;
  uint64_t alignInBits;
  uint64_t offsetInBits;
  DIFlags flags;
  const Metadata *elements;
  const Metadata *templateParams;

  virtual rdcstr toString() const;
};

struct DITemplateTypeParameter : public DIBase
{
  static const DIBase::Type DIType = DIBase::TemplateTypeParameter;
  DITemplateTypeParameter(const rdcstr *name, const Metadata *type)
      : DIBase(DIType), name(name), type(type)
  {
  }
  const rdcstr *name;
  const Metadata *type;

  virtual rdcstr toString() const;
};

struct DITemplateValueParameter : public DIBase
{
  static const DIBase::Type DIType = DIBase::TemplateValueParameter;
  DITemplateValueParameter(DW_TAG tag, const rdcstr *name, const Metadata *type, const Metadata *value)
      : DIBase(DIType), tag(tag), name(name), type(type), value(value)
  {
  }

  DW_TAG tag;
  const rdcstr *name;
  const Metadata *type;
  const Metadata *value;

  virtual rdcstr toString() const;
};

struct DISubprogram : public DIBase
{
  static const DIBase::Type DIType = DIBase::Subprogram;
  DISubprogram(const Metadata *scope, const rdcstr *name, const rdcstr *linkageName,
               const Metadata *file, uint64_t line, const Metadata *type, bool isLocal,
               bool isDefinition, uint64_t scopeLine, const Metadata *containingType,
               DW_VIRTUALITY virtuality, uint64_t virtualIndex, DIFlags flags, bool isOptimized,
               const Metadata *function, const Metadata *templateParams,
               const Metadata *declaration, const Metadata *variables)
      : DIBase(DIType),
        scope(scope),
        name(name),
        linkageName(linkageName),
        file(file),
        line(line),
        type(type),
        isLocal(isLocal),
        isDefinition(isDefinition),
        scopeLine(scopeLine),
        containingType(containingType),
        virtuality(virtuality),
        virtualIndex(virtualIndex),
        flags(flags),
        isOptimized(isOptimized),
        function(function),
        templateParams(templateParams),
        declaration(declaration),
        variables(variables)
  {
  }

  const Metadata *scope;
  const rdcstr *name;
  const rdcstr *linkageName;
  const Metadata *file;
  uint64_t line;
  const Metadata *type;
  bool isLocal;
  bool isDefinition;
  uint64_t scopeLine;
  const Metadata *containingType;
  DW_VIRTUALITY virtuality;
  uint64_t virtualIndex;
  DIFlags flags;
  bool isOptimized;
  const Metadata *function;
  const Metadata *templateParams;
  const Metadata *declaration;
  const Metadata *variables;

  virtual rdcstr toString() const;
};

struct DISubroutineType : public DIBase
{
  static const DIBase::Type DIType = DIBase::SubroutineType;
  DISubroutineType(const Metadata *types) : DIBase(DIType), types(types) {}
  const Metadata *types;

  virtual rdcstr toString() const;
};

struct DIGlobalVariable : public DIBase
{
  static const DIBase::Type DIType = DIBase::GlobalVariable;
  DIGlobalVariable(const Metadata *scope, const rdcstr *name, const rdcstr *linkageName,
                   const Metadata *file, uint64_t line, const Metadata *type, bool isLocal,
                   bool isDefinition, const Metadata *variable, const Metadata *staticData)
      : DIBase(DIType),
        scope(scope),
        name(name),
        linkageName(linkageName),
        file(file),
        line(line),
        type(type),
        isLocal(isLocal),
        isDefinition(isDefinition),
        variable(variable),
        staticData(staticData)
  {
  }

  const Metadata *scope;
  const rdcstr *name;
  const rdcstr *linkageName;
  const Metadata *file;
  uint64_t line;
  const Metadata *type;
  bool isLocal;
  bool isDefinition;
  const Metadata *variable;
  const Metadata *staticData;

  virtual rdcstr toString() const;
};

};    // namespace DXIL

DECLARE_REFLECTION_ENUM(DXIL::Attribute);
DECLARE_REFLECTION_ENUM(DXIL::DW_LANG);
