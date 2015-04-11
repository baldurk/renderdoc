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

#ifndef _TYPES_INCLUDED
#define _TYPES_INCLUDED

#include "../Include/Common.h"
#include "../Include/BaseTypes.h"
#include "../Public/ShaderLang.h"
#include "arrays.h"

namespace glslang {

const int GlslangMaxTypeLength = 200;  // TODO: need to print block/struct one member per line, so this can stay bounded

const char* const AnonymousPrefix = "anon@"; // for something like a block whose members can be directly accessed
inline bool IsAnonymous(const TString& name)
{
    return name.compare(0, 5, AnonymousPrefix) == 0;
}

//
// Details within a sampler type
//
enum TSamplerDim {
    EsdNone,
    Esd1D,
    Esd2D,
    Esd3D,
    EsdCube,
    EsdRect,
    EsdBuffer,
    EsdNumDims
};

struct TSampler {
    TBasicType type : 8;  // type returned by sampler
    TSamplerDim dim : 8;
    bool    arrayed : 1;
    bool     shadow : 1;
    bool         ms : 1;
    bool      image : 1;
    bool   external : 1;  // GL_OES_EGL_image_external

    void clear()
    {
        type = EbtVoid;
        dim = EsdNone;
        arrayed = false;
        shadow = false;
        ms = false;
        image = false;
        external = false;
    }

    void set(TBasicType t, TSamplerDim d, bool a = false, bool s = false, bool m = false)
    {
        type = t;
        dim = d;
        arrayed = a;
        shadow = s;
        ms = m;
        image = false;
        external = false;
    }

    void setImage(TBasicType t, TSamplerDim d, bool a = false, bool s = false, bool m = false)
    {
        type = t;
        dim = d;
        arrayed = a;
        shadow = s;
        ms = m;
        image = true;
        external = false;
    }

    bool operator==(const TSampler& right) const
    {
        return type == right.type &&
                dim == right.dim &&
            arrayed == right.arrayed &&
             shadow == right.shadow &&
                 ms == right.ms &&
              image == right.image &&
           external == right.external;
    }

    TString getString() const
    {
        TString s;

        switch (type) {
        case EbtFloat:               break;
        case EbtInt:  s.append("i"); break;
        case EbtUint: s.append("u"); break;
        default:  break;  // some compilers want this
        }
        if (image)
            s.append("image");
        else
            s.append("sampler");
        if (external) {
            s.append("ExternalOES");
            return s;
        }
        switch (dim) {
        case Esd1D:      s.append("1D");     break;
        case Esd2D:      s.append("2D");     break;
        case Esd3D:      s.append("3D");     break;
        case EsdCube:    s.append("Cube");   break;
        case EsdRect:    s.append("2DRect"); break;
        case EsdBuffer:  s.append("Buffer"); break;
        default:  break;  // some compilers want this
        }
        if (ms)
            s.append("MS");
        if (arrayed)
            s.append("Array");
        if (shadow)
            s.append("Shadow");

        return s;
    }
};

//
// Need to have association of line numbers to types in a list for building structs.
//
class TType;
struct TTypeLoc {
    TType* type;
    TSourceLoc loc;
};
typedef TVector<TTypeLoc> TTypeList;

typedef TVector<TString*> TIdentifierList;

//
// Following are a series of helper enums for managing layouts and qualifiers,
// used for TPublicType, TType, others.
//

enum TLayoutPacking {
    ElpNone,
    ElpShared,      // default, but different than saying nothing
    ElpStd140,
    ElpStd430,
    ElpPacked
    // If expanding, see bitfield width below
};

enum TLayoutMatrix {
    ElmNone,
    ElmRowMajor,
    ElmColumnMajor  // default, but different than saying nothing
    // If expanding, see bitfield width below
};

// Union of geometry shader and tessellation shader geometry types.
// They don't go into TType, but rather have current state per shader or
// active parser type (TPublicType).
enum TLayoutGeometry {
    ElgNone,
    ElgPoints,
    ElgLines,
    ElgLinesAdjacency,
    ElgLineStrip,
    ElgTriangles,
    ElgTrianglesAdjacency,
    ElgTriangleStrip,
    ElgQuads,
    ElgIsolines,
};

enum TVertexSpacing {
    EvsNone,
    EvsEqual,
    EvsFractionalEven,
    EvsFractionalOdd
};

enum TVertexOrder {
    EvoNone,
    EvoCw,
    EvoCcw
};

// Note: order matters, as type of format is done by comparison.
enum TLayoutFormat {
    ElfNone,

    // Float image
    ElfRgba32f,
    ElfRgba16f,
    ElfR32f,
    ElfRgba8,
    ElfRgba8Snorm,

    ElfEsFloatGuard,    // to help with comparisons

    ElfRg32f,
    ElfRg16f,
    ElfR11fG11fB10f,
    ElfR16f,
    ElfRgba16,
    ElfRgb10A2,
    ElfRg16,
    ElfRg8,
    ElfR16,
    ElfR8,
    ElfRgba16Snorm,
    ElfRg16Snorm,
    ElfRg8Snorm,
    ElfR16Snorm,
    ElfR8Snorm,

    ElfFloatGuard,      // to help with comparisons

    // Int image
    ElfRgba32i,
    ElfRgba16i,
    ElfRgba8i,
    ElfR32i,

    ElfEsIntGuard,     // to help with comparisons

    ElfRg32i,
    ElfRg16i,
    ElfRg8i,
    ElfR16i,
    ElfR8i,

    ElfIntGuard,       // to help with comparisons

    // Uint image
    ElfRgba32ui,
    ElfRgba16ui,
    ElfRgba8ui,
    ElfR32ui,

    ElfEsUintGuard,    // to help with comparisons

    ElfRg32ui,
    ElfRg16ui,
    ElfRgb10a2ui,
    ElfRg8ui,
    ElfR16ui,
    ElfR8ui,

    ElfCount
};

enum TLayoutDepth {
    EldNone,
    EldAny,
    EldGreater,
    EldLess,
    EldUnchanged,
    
    EldCount
};

class TQualifier {
public:
    void clear()
    {
        precision = EpqNone;
        invariant = false;
        makeTemporary();
    }

    // drop qualifiers that don't belong in a temporary variable
    void makeTemporary()
    {
        storage   = EvqTemporary;
        builtIn   = EbvNone;
        centroid  = false;
        smooth    = false;
        flat      = false;
        nopersp   = false;
        patch     = false;
        sample    = false;
        coherent  = false;
        volatil   = false;
        restrict  = false;
        readonly  = false;
        writeonly = false;
        clearLayout();
    }

    TStorageQualifier   storage   : 6;
    TBuiltInVariable    builtIn   : 8;
    TPrecisionQualifier precision : 3;
    bool invariant : 1;
    bool centroid  : 1;
    bool smooth    : 1;
    bool flat      : 1;
    bool nopersp   : 1;
    bool patch     : 1;
    bool sample    : 1;
    bool coherent  : 1;
    bool volatil   : 1;
    bool restrict  : 1;
    bool readonly  : 1;
    bool writeonly : 1;

    bool isMemory() const
    {
        return coherent || volatil || restrict || readonly || writeonly;
    }
    bool isInterpolation() const
    {
        return flat || smooth || nopersp;
    }
    bool isAuxiliary() const
    {
        return centroid || patch || sample;
    }

    bool isPipeInput() const
    {
        switch (storage) {
        case EvqVaryingIn:
        case EvqFragCoord:
        case EvqPointCoord:
        case EvqFace:
        case EvqVertexId:
        case EvqInstanceId:
            return true;
        default:
            return false;
        }
    }

    bool isPipeOutput() const
    {
        switch (storage) {
        case EvqPosition:
        case EvqPointSize:
        case EvqClipVertex:
        case EvqVaryingOut:
        case EvqFragColor:
        case EvqFragDepth:
            return true;
        default:
            return false;
        }
    }

    bool isParamInput() const
    {
        switch (storage) {
        case EvqIn:
        case EvqInOut:
        case EvqConstReadOnly:
            return true;
        default:
            return false;
        }
    }

    bool isParamOutput() const
    {
        switch (storage) {
        case EvqOut:
        case EvqInOut:
            return true;
        default:
            return false;
        }
    }

    bool isUniformOrBuffer() const
    {
        switch (storage) {
        case EvqUniform:
        case EvqBuffer:
            return true;
        default:
            return false;
        }
    }

    bool isIo() const
    {
        switch (storage) {
        case EvqUniform:
        case EvqBuffer:
        case EvqVaryingIn:
        case EvqFragCoord:
        case EvqPointCoord:
        case EvqFace:
        case EvqVertexId:
        case EvqInstanceId:
        case EvqPosition:
        case EvqPointSize:
        case EvqClipVertex:
        case EvqVaryingOut:
        case EvqFragColor:
        case EvqFragDepth:
            return true;
        default:
            return false;
        }
    }

    // True if this type of IO is supposed to be arrayed with extra level for per-vertex data
    bool isArrayedIo(EShLanguage language) const
    {
        switch (language) {
        case EShLangGeometry:
            return isPipeInput();
        case EShLangTessControl:
            return ! patch && (isPipeInput() || isPipeOutput());
        case EShLangTessEvaluation:
            return ! patch && isPipeInput();
        default:
            return false;
        }
    }

    // Implementing an embedded layout-qualifier class here, since C++ can't have a real class bitfield
    void clearLayout()
    {
        layoutMatrix = ElmNone;
        layoutPacking = ElpNone;
        layoutOffset = -1;
        layoutAlign = -1;

        layoutLocation = layoutLocationEnd;
        layoutComponent = layoutComponentEnd;
        layoutSet = layoutSetEnd;
        layoutBinding = layoutBindingEnd;
        layoutIndex = layoutIndexEnd;

        layoutStream = layoutStreamEnd;

        layoutXfbBuffer = layoutXfbBufferEnd;
        layoutXfbStride = layoutXfbStrideEnd;
        layoutXfbOffset = layoutXfbOffsetEnd;

        layoutFormat = ElfNone;
    }
    bool hasLayout() const
    {
        return hasUniformLayout() || 
               hasAnyLocation() ||
               hasBinding() ||
               hasStream() ||
               hasXfb() ||
               hasFormat();
    }
    TLayoutMatrix  layoutMatrix  : 3;
    TLayoutPacking layoutPacking : 4;
    int layoutOffset;
    int layoutAlign;

                 unsigned int layoutLocation         : 7;
    static const unsigned int layoutLocationEnd =   0x3F;

                 unsigned int layoutComponent        : 3;
    static const unsigned int layoutComponentEnd =     4;

                 unsigned int layoutSet              : 7;
    static const unsigned int layoutSetEnd      =   0x3F;

                 unsigned int layoutBinding          : 8;
    static const unsigned int layoutBindingEnd =    0xFF;

                 unsigned int layoutIndex           :  8;
    static const unsigned int layoutIndexEnd =      0xFF;

                 unsigned int layoutStream           : 8;
    static const unsigned int layoutStreamEnd =     0xFF;

                 unsigned int layoutXfbBuffer        : 4;
    static const unsigned int layoutXfbBufferEnd =   0xF;

                 unsigned int layoutXfbStride       : 10;
    static const unsigned int layoutXfbStrideEnd = 0x3FF;

                 unsigned int layoutXfbOffset       : 10;
    static const unsigned int layoutXfbOffsetEnd = 0x3FF;

    TLayoutFormat layoutFormat                      :  8;

    bool hasUniformLayout() const
    {
        return hasMatrix() ||
               hasPacking() ||
               hasOffset() ||
               hasBinding() ||
               hasAlign();
    }
    bool hasMatrix() const
    {
        return layoutMatrix != ElmNone;
    }
    bool hasPacking() const
    {
        return layoutPacking != ElpNone;
    }
    bool hasOffset() const
    {
        return layoutOffset != -1;
    }
    bool hasAlign() const
    {
        return layoutAlign != -1;
    }
    bool hasAnyLocation() const
    {
        return hasLocation() ||
               hasComponent() ||
               hasIndex();
    }
    bool hasLocation() const
    {
        return layoutLocation  != layoutLocationEnd;
    }
    bool hasComponent() const
    {
        return layoutComponent != layoutComponentEnd;
    }
    bool hasIndex() const
    {
        return layoutIndex != layoutIndexEnd;
    }
    bool hasSet() const
    {
        return layoutSet != layoutSetEnd;
    }
    bool hasBinding() const
    {
        return layoutBinding != layoutBindingEnd;
    }
    bool hasStream() const
    {
        return layoutStream != layoutStreamEnd;
    }
    bool hasFormat() const
    {
        return layoutFormat != ElfNone;
    }
    bool hasXfb() const
    {
        return hasXfbBuffer() ||
               hasXfbStride() ||
               hasXfbOffset();
    }
    bool hasXfbBuffer() const
    {
        return layoutXfbBuffer != layoutXfbBufferEnd;
    }
    bool hasXfbStride() const
    {
        return layoutXfbStride != layoutXfbStrideEnd;
    }
    bool hasXfbOffset() const
    {
        return layoutXfbOffset != layoutXfbOffsetEnd;
    }
    static const char* getLayoutPackingString(TLayoutPacking packing)
    {
        switch (packing) {
        case ElpPacked:   return "packed";
        case ElpShared:   return "shared";
        case ElpStd140:   return "std140";
        case ElpStd430:   return "std430";
        default:          return "none";
        }
    }
    static const char* getLayoutMatrixString(TLayoutMatrix m)
    {
        switch (m) {
        case ElmColumnMajor: return "column_major";
        case ElmRowMajor:    return "row_major";
        default:             return "none";
        }
    }
    static const char* getLayoutFormatString(TLayoutFormat f)
    {
        switch (f) {
        case ElfRgba32f:      return "rgba32f";
        case ElfRgba16f:      return "rgba16f";
        case ElfRg32f:        return "rg32f";
        case ElfRg16f:        return "rg16f";
        case ElfR11fG11fB10f: return "r11f_g11f_b10f";
        case ElfR32f:         return "r32f";
        case ElfR16f:         return "r16f";
        case ElfRgba16:       return "rgba16";
        case ElfRgb10A2:      return "rgb10_a2";
        case ElfRgba8:        return "rgba8";
        case ElfRg16:         return "rg16";
        case ElfRg8:          return "rg8";
        case ElfR16:          return "r16";
        case ElfR8:           return "r8";
        case ElfRgba16Snorm:  return "rgba16_snorm";
        case ElfRgba8Snorm:   return "rgba8_snorm";
        case ElfRg16Snorm:    return "rg16_snorm";
        case ElfRg8Snorm:     return "rg8_snorm";
        case ElfR16Snorm:     return "r16_snorm";
        case ElfR8Snorm:      return "r8_snorm";

        case ElfRgba32i:      return "rgba32i";
        case ElfRgba16i:      return "rgba16i";
        case ElfRgba8i:       return "rgba8i";
        case ElfRg32i:        return "rg32i";
        case ElfRg16i:        return "rg16i";
        case ElfRg8i:         return "rg8i";
        case ElfR32i:         return "r32i";
        case ElfR16i:         return "r16i";
        case ElfR8i:          return "r8i";

        case ElfRgba32ui:     return "rgba32ui";
        case ElfRgba16ui:     return "rgba16ui";
        case ElfRgba8ui:      return "rgba8ui";
        case ElfRg32ui:       return "rg32ui";
        case ElfRg16ui:       return "rg16ui";
        case ElfRgb10a2ui:    return "rgb10a2ui";
        case ElfRg8ui:        return "rg8ui";
        case ElfR32ui:        return "r32ui";
        case ElfR16ui:        return "r16ui";
        case ElfR8ui:         return "r8ui";
        default:              return "none";
        }
    }
    static const char* getLayoutDepthString(TLayoutDepth d)
    {
        switch (d) {
        case EldAny:       return "depth_any";
        case EldGreater:   return "depth_greater";
        case EldLess:      return "depth_less";
        case EldUnchanged: return "depth_unchanged";
        default:           return "none";
        }
    }
    static const char* getGeometryString(TLayoutGeometry geometry)
    {
        switch (geometry) {
        case ElgPoints:             return "points";
        case ElgLines:              return "lines";
        case ElgLinesAdjacency:     return "lines_adjacency";
        case ElgLineStrip:          return "line_strip";
        case ElgTriangles:          return "triangles";
        case ElgTrianglesAdjacency: return "triangles_adjacency";
        case ElgTriangleStrip:      return "triangle_strip";
        case ElgQuads:              return "quads";
        case ElgIsolines:           return "isolines";
        default:                    return "none";
        }
    }
    static const char* getVertexSpacingString(TVertexSpacing spacing)
    {
        switch (spacing) {
        case EvsEqual:              return "equal_spacing";
        case EvsFractionalEven:     return "fractional_even_spacing";
        case EvsFractionalOdd:      return "fractional_odd_spacing";
        default:                    return "none";
        }
    }
    static const char* getVertexOrderString(TVertexOrder order)
    {
        switch (order) {
        case EvoCw:                 return "cw";
        case EvoCcw:                return "ccw";
        default:                    return "none";
        }
    }
    static int mapGeometryToSize(TLayoutGeometry geometry)
    {
        switch (geometry) {
        case ElgPoints:             return 1;
        case ElgLines:              return 2;
        case ElgLinesAdjacency:     return 4;
        case ElgTriangles:          return 3;
        case ElgTrianglesAdjacency: return 6;
        default:                    return 0;
        }
    }
};

// Qualifiers that don't need to be keep per object.  They have shader scope, not object scope.
// So, they will not be part of TType, TQualifier, etc.
struct TShaderQualifiers {
    TLayoutGeometry geometry; // geometry/tessellation shader in/out primitives
    bool pixelCenterInteger;  // fragment shader
    bool originUpperLeft;     // fragment shader
    int invocations;          // 0 means no declaration
    int vertices;             // both for tessellation "vertices" and geometry "max_vertices"
    TVertexSpacing spacing;
    TVertexOrder order;
    bool pointMode;
    int localSize[3];         // compute shader
    bool earlyFragmentTests;  // fragment input
    TLayoutDepth layoutDepth;

    void init()
    {
        geometry = ElgNone;
        originUpperLeft = false;
        pixelCenterInteger = false;
        invocations = 0;        // 0 means no declaration
        vertices = 0;
        spacing = EvsNone;
        order = EvoNone;
        pointMode = false;
        localSize[0] = 1;
        localSize[1] = 1;
        localSize[2] = 1;
        earlyFragmentTests = false;
        layoutDepth = EldNone;
    }

    // Merge in characteristics from the 'src' qualifier.  They can override when
    // set, but never erase when not set.
    void merge(const TShaderQualifiers& src)
    {
        if (src.geometry != ElgNone)
            geometry = src.geometry;
        if (src.pixelCenterInteger)
            pixelCenterInteger = src.pixelCenterInteger;
        if (src.originUpperLeft)
            originUpperLeft = src.originUpperLeft;
        if (src.invocations != 0)
            invocations = src.invocations;
        if (src.vertices != 0)
            vertices = src.vertices;
        if (src.spacing != EvsNone)
            spacing = src.spacing;
        if (src.order != EvoNone)
            order = src.order;
        if (src.pointMode)
            pointMode = true;
        for (int i = 0; i < 3; ++i) {
            if (src.localSize[i] > 1)
                localSize[i] = src.localSize[i];
        }
        if (src.earlyFragmentTests)
            earlyFragmentTests = true;
        if (src.layoutDepth)
            layoutDepth = src.layoutDepth;
    }
};

//
// TPublicType is just temporarily used while parsing and not quite the same
// information kept per node in TType.  Due to the bison stack, it can't have
// types that it thinks have non-trivial constructors.  It should
// just be used while recognizing the grammar, not anything else.
// Once enough is known about the situation, the proper information
// moved into a TType, or the parse context, etc.
//
class TPublicType {
public:
    TBasicType basicType;
    TSampler sampler;
    TQualifier qualifier;
    TShaderQualifiers shaderQualifiers;
    int vectorSize : 4;
    int matrixCols : 4;
    int matrixRows : 4;
    TArraySizes* arraySizes;
    const TType* userDef;
    TSourceLoc loc;

    void initType(TSourceLoc l)
    {
        basicType = EbtVoid;
        vectorSize = 1;
        matrixRows = 0;
        matrixCols = 0;
        arraySizes = 0;
        userDef = 0;
        loc = l;
    }

    void initQualifiers(bool global = false)
    {
        qualifier.clear();
        if (global)
            qualifier.storage = EvqGlobal;
    }

    void init(TSourceLoc loc, bool global = false)
    {
        initType(loc);
        sampler.clear();
        initQualifiers(global);
        shaderQualifiers.init();
    }

    void setVector(int s)
    {
        matrixRows = 0;
        matrixCols = 0;
        vectorSize = s;
    }

    void setMatrix(int c, int r)
    {
        matrixRows = r;
        matrixCols = c;
        vectorSize = 0;
    }

    bool isScalar() const
    {
        return matrixCols == 0 && vectorSize == 1 && arraySizes == 0 && userDef == 0;
    }

    bool isImage() const
    {
        return basicType == EbtSampler && sampler.image;
    }
};

typedef std::map<TTypeList*, TTypeList*> TStructureMap;
typedef std::map<TTypeList*, TTypeList*>::const_iterator TStructureMapIterator;

//
// Base class for things that have a type.
//
class TType {
public:
    POOL_ALLOCATOR_NEW_DELETE(GetThreadPoolAllocator())

    // for "empty" type (no args) or simple scalar/vector/matrix
    explicit TType(TBasicType t = EbtVoid, TStorageQualifier q = EvqTemporary, int vs = 1, int mc = 0, int mr = 0) :
                            basicType(t), vectorSize(vs), matrixCols(mc), matrixRows(mr), arraySizes(0),
                            structure(0), fieldName(0), typeName(0)
                            {
                                sampler.clear();
                                qualifier.clear();
                                qualifier.storage = q;
                            }
    // for explicit precision qualifier
    TType(TBasicType t, TStorageQualifier q, TPrecisionQualifier p, int vs = 1, int mc = 0, int mr = 0) :
                            basicType(t), vectorSize(vs), matrixCols(mc), matrixRows(mr), arraySizes(0),
                            structure(0), fieldName(0), typeName(0)
                            {
                                sampler.clear();
                                qualifier.clear();
                                qualifier.storage = q;
                                qualifier.precision = p;
                                assert(p >= 0 && p <= EpqHigh);
                            }
    // for turning a TPublicType into a TType
    explicit TType(const TPublicType& p) :
                            basicType(p.basicType), vectorSize(p.vectorSize), matrixCols(p.matrixCols), matrixRows(p.matrixRows), arraySizes(p.arraySizes),
                            structure(0), fieldName(0), typeName(0)
                            {
                                if (basicType == EbtSampler)
                                    sampler = p.sampler;
                                else
                                    sampler.clear();
                                qualifier = p.qualifier;
                                if (p.userDef) {
                                    structure = p.userDef->getWritableStruct();  // public type is short-lived; there are no sharing issues
                                    typeName = NewPoolTString(p.userDef->getTypeName().c_str());
                                }
                            }
    // to efficiently make a dereferenced type
    // without ever duplicating the outer structure that will be thrown away
    // and using only shallow copy
    TType(const TType& type, int derefIndex, bool rowMajor = false)
                            {
                                if (! type.isArray() && (type.basicType == EbtStruct || type.basicType == EbtBlock)) {
                                    // do a structure dereference
                                    const TTypeList& memberList = *type.getStruct();
                                    shallowCopy(*memberList[derefIndex].type);
                                    return;
                                } else {
                                    // do an array/vector/matrix dereference
                                    shallowCopy(type);
                                    dereference(rowMajor);
                                }
                            }
    // for making structures, ...
    TType(TTypeList* userDef, const TString& n) :
                            basicType(EbtStruct), vectorSize(1), matrixCols(0), matrixRows(0),
                            arraySizes(0), structure(userDef), fieldName(0)
                            {
                                sampler.clear();
                                qualifier.clear();
                                typeName = NewPoolTString(n.c_str());
                            }
    // For interface blocks
    TType(TTypeList* userDef, const TString& n, const TQualifier& q) : 
                            basicType(EbtBlock), vectorSize(1), matrixCols(0), matrixRows(0),
                            qualifier(q), arraySizes(0), structure(userDef), fieldName(0)
                            {
                                sampler.clear();
                                typeName = NewPoolTString(n.c_str());
                            }
    virtual ~TType() {}
    
    // Not for use across pool pops; it will cause multiple instances of TType to point to the same information.
    // This only works if that information (like a structure's list of types) does not change and 
    // the instances are sharing the same pool. 
    void shallowCopy(const TType& copyOf)
    {
        basicType = copyOf.basicType;
        sampler = copyOf.sampler;
        qualifier = copyOf.qualifier;
        vectorSize = copyOf.vectorSize;
        matrixCols = copyOf.matrixCols;
        matrixRows = copyOf.matrixRows;
        arraySizes = copyOf.arraySizes;  // copying the pointer only, not the contents
        structure = copyOf.structure;
        fieldName = copyOf.fieldName;
        typeName = copyOf.typeName;
    }

    void deepCopy(const TType& copyOf)
    {
        shallowCopy(copyOf);

        if (copyOf.arraySizes) {
            arraySizes = new TArraySizes;
            *arraySizes = *copyOf.arraySizes;
        }

        if (copyOf.structure) {
            structure = new TTypeList;
            TStructureMapIterator iter;
            for (unsigned int i = 0; i < copyOf.structure->size(); ++i) {
                TTypeLoc typeLoc;
                typeLoc.loc = (*copyOf.structure)[i].loc;
                typeLoc.type = new TType();
                typeLoc.type->deepCopy(*(*copyOf.structure)[i].type);
                structure->push_back(typeLoc);
            }
        }

        if (copyOf.fieldName)
            fieldName = NewPoolTString(copyOf.fieldName->c_str());
        if (copyOf.typeName)
            typeName = NewPoolTString(copyOf.typeName->c_str());
    }
    
    TType* clone()
    {
        TType *newType = new TType();
        newType->deepCopy(*this);

        return newType;
    }

    // Merge type from parent, where a parentType is at the beginning of a declaration,
    // establishing some characteristics for all subsequent names, while this type
    // is on the individual names.
    void mergeType(const TPublicType& parentType)
    {
        // arrayness is currently the only child aspect that has to be preserved
        basicType = parentType.basicType;
        vectorSize = parentType.vectorSize;
        matrixCols = parentType.matrixCols;
        matrixRows = parentType.matrixRows;
        qualifier = parentType.qualifier;
        sampler = parentType.sampler;
        if (parentType.arraySizes)
            setArraySizes(parentType.arraySizes);
        if (parentType.userDef) {
            structure = parentType.userDef->getWritableStruct();
            setTypeName(parentType.userDef->getTypeName());
        }
    }

    virtual void dereference(bool rowMajor = false)
    {
        if (arraySizes)
            arraySizes = 0;
        else if (matrixCols > 0) {
            if (rowMajor)
                vectorSize = matrixCols;
            else
                vectorSize = matrixRows;
            matrixCols = 0;
            matrixRows = 0;
        } else if (vectorSize > 1)
            vectorSize = 1;
    }

    virtual void hideMember() { basicType = EbtVoid; vectorSize = 1; }
    virtual bool hiddenMember() const { return basicType == EbtVoid; }

    virtual void setTypeName(const TString& n) { typeName = NewPoolTString(n.c_str()); }
    virtual void setFieldName(const TString& n) { fieldName = NewPoolTString(n.c_str()); }
    virtual const TString& getTypeName() const
    {
        assert(typeName);
        return *typeName;
    }

    virtual const TString& getFieldName() const
    {
        assert(fieldName);
        return *fieldName;
    }

    virtual TBasicType getBasicType() const { return basicType; }
    virtual const TSampler& getSampler() const { return sampler; }

    virtual       TQualifier& getQualifier()       { return qualifier; }
    virtual const TQualifier& getQualifier() const { return qualifier; }

    virtual int getVectorSize() const { return vectorSize; }
    virtual int getMatrixCols() const { return matrixCols; }
    virtual int getMatrixRows() const { return matrixRows; }
    virtual int getArraySize()  const { return arraySizes->getOuterSize(); }
    virtual bool isArrayOfArrays() const { return arraySizes && arraySizes->getNumDims() > 1; }
    virtual int getImplicitArraySize() const { return arraySizes->getImplicitSize(); }

    virtual bool isScalar() const { return vectorSize == 1 && ! isStruct() && ! isArray(); }
    virtual bool isVector() const { return vectorSize > 1; }
    virtual bool isMatrix() const { return matrixCols ? true : false; }
    virtual bool isArray()  const { return arraySizes != 0; }
    virtual bool isImplicitlySizedArray() const { return isArray() && ! getArraySize() && qualifier.storage != EvqBuffer; }
    virtual bool isExplicitlySizedArray() const { return isArray() && getArraySize(); }
    virtual bool isRuntimeSizedArray() const { return isArray() && ! getArraySize() && qualifier.storage == EvqBuffer; }
    virtual bool isStruct() const { return structure != 0; }
    virtual bool isImage() const { return basicType == EbtSampler && getSampler().image; }

    // Recursively checks if the type contains the given basic type
    virtual bool containsBasicType(TBasicType checkType) const
    {
        if (basicType == checkType)
            return true;
        if (! structure)
            return false;
        for (unsigned int i = 0; i < structure->size(); ++i) {
            if ((*structure)[i].type->containsBasicType(checkType))
                return true;
        }
        return false;
    }

    // Recursively check the structure for any arrays, needed for some error checks
    virtual bool containsArray() const
    {
        if (isArray())
            return true;
        if (! structure)
            return false;
        for (unsigned int i = 0; i < structure->size(); ++i) {
            if ((*structure)[i].type->containsArray())
                return true;
        }
        return false;
    }

    // Check the structure for any structures, needed for some error checks
    virtual bool containsStructure() const
    {
        if (! structure)
            return false;
        for (unsigned int i = 0; i < structure->size(); ++i) {
            if ((*structure)[i].type->structure)
                return true;
        }
        return false;
    }

    // Recursively check the structure for any implicitly-sized arrays, needed for triggering a copyUp().
    virtual bool containsImplicitlySizedArray() const
    {
        if (isImplicitlySizedArray())
            return true;
        if (! structure)
            return false;
        for (unsigned int i = 0; i < structure->size(); ++i) {
            if ((*structure)[i].type->containsImplicitlySizedArray())
                return true;
        }
        return false;
    }

    // Array editing methods.  Array descriptors can be shared across
    // type instances.  This allows all uses of the same array
    // to be updated at once.  E.g., all nodes can be explicitly sized
    // by tracking and correcting one implicit size.  Or, all nodes
    // can get the explicit size on a redeclaration that gives size.
    //
    // N.B.:  Don't share with the shared symbol tables (symbols are
    // marked as isReadOnly().  Such symbols with arrays that will be
    // edited need to copyUp() on first use, so that 
    // A) the edits don't effect the shared symbol table, and
    // B) the edits are shared across all users.
    void updateArraySizes(const TType& type)
    {
        // For when we may already be sharing existing array descriptors,
        // keeping the pointers the same, just updating the contents.
        assert(arraySizes != nullptr);
        assert(type.arraySizes != nullptr);
        *arraySizes = *type.arraySizes;
    }
    void setArraySizes(TArraySizes* s)
    {
        // For setting a fresh new set of array sizes, not yet worrying about sharing.
        arraySizes = new TArraySizes;
        assert(s != nullptr);
        *arraySizes = *s;
    }
    void setArraySizes(const TType& type) { setArraySizes(type.arraySizes); }
    void changeArraySize(int s) { arraySizes->changeOuterSize(s); }
    void setImplicitArraySize (int s) { arraySizes->setImplicitSize(s); }

    // Recursively make the implicit array size the explicit array size, through the type tree.
    void adoptImplicitArraySizes()
    {
        if (isImplicitlySizedArray())
            changeArraySize(getImplicitArraySize());
        if (isStruct()) {
            for (int i = 0; i < (int)structure->size(); ++i)
                (*structure)[i].type->adoptImplicitArraySizes();
        }
    }

    const char* getBasicString() const 
    {
        return TType::getBasicString(basicType);
    }
    
    static const char* getBasicString(TBasicType t)
    {
        switch (t) {
        case EbtVoid:              return "void";
        case EbtFloat:             return "float";
        case EbtDouble:            return "double";
        case EbtInt:               return "int";
        case EbtUint:              return "uint";
        case EbtBool:              return "bool";
        case EbtAtomicUint:        return "atomic_uint";
        case EbtSampler:           return "sampler/image";
        case EbtStruct:            return "structure";
        case EbtBlock:             return "block";
        default:                   return "unknown type";
        }
    }

    TString getCompleteString() const
    {
        const int maxSize = GlslangMaxTypeLength;
        char buf[maxSize];
        char* p = &buf[0];
        char* end = &buf[maxSize];

        if (qualifier.hasLayout()) {
            // To reduce noise, skip this if the only layout is an xfb_buffer
            // with no triggering xfb_offset.
            TQualifier noXfbBuffer = qualifier;
            noXfbBuffer.layoutXfbBuffer = TQualifier::layoutXfbBufferEnd;
            if (noXfbBuffer.hasLayout()) {
                p += snprintf(p, end - p, "layout(");
                if (qualifier.hasAnyLocation()) {
                    p += snprintf(p, end - p, "location=%d ", qualifier.layoutLocation);
                    if (qualifier.hasComponent())
                        p += snprintf(p, end - p, "component=%d ", qualifier.layoutComponent);
                    if (qualifier.hasIndex())
                        p += snprintf(p, end - p, "index=%d ", qualifier.layoutIndex);
                }
                if (qualifier.hasSet())
                    p += snprintf(p, end - p, "set=%d ", qualifier.layoutSet);
                if (qualifier.hasBinding())
                    p += snprintf(p, end - p, "binding=%d ", qualifier.layoutBinding);
                if (qualifier.hasStream())
                    p += snprintf(p, end - p, "stream=%d ", qualifier.layoutStream);
                if (qualifier.hasMatrix())
                    p += snprintf(p, end - p, "%s ", TQualifier::getLayoutMatrixString(qualifier.layoutMatrix));
                if (qualifier.hasPacking())
                    p += snprintf(p, end - p, "%s ", TQualifier::getLayoutPackingString(qualifier.layoutPacking));
                if (qualifier.hasOffset())
                    p += snprintf(p, end - p, "offset=%d ", qualifier.layoutOffset);
                if (qualifier.hasAlign())
                    p += snprintf(p, end - p, "align=%d ", qualifier.layoutAlign);
                if (qualifier.hasFormat())
                    p += snprintf(p, end - p, "%s ", TQualifier::getLayoutFormatString(qualifier.layoutFormat));
                if (qualifier.hasXfbBuffer() && qualifier.hasXfbOffset())
                    p += snprintf(p, end - p, "xfb_buffer=%d ", qualifier.layoutXfbBuffer);
                if (qualifier.hasXfbOffset())
                    p += snprintf(p, end - p, "xfb_offset=%d ", qualifier.layoutXfbOffset);
                if (qualifier.hasXfbStride())
                    p += snprintf(p, end - p, "xfb_stride=%d ", qualifier.layoutXfbStride);
                p += snprintf(p, end - p, ") ");
            }
        }

        if (qualifier.invariant)
            p += snprintf(p, end - p, "invariant ");
        if (qualifier.centroid)
            p += snprintf(p, end - p, "centroid ");
        if (qualifier.smooth)
            p += snprintf(p, end - p, "smooth ");
        if (qualifier.flat)
            p += snprintf(p, end - p, "flat ");
        if (qualifier.nopersp)
            p += snprintf(p, end - p, "noperspective ");
        if (qualifier.patch)
            p += snprintf(p, end - p, "patch ");
        if (qualifier.sample)
            p += snprintf(p, end - p, "sample ");
        if (qualifier.coherent)
            p += snprintf(p, end - p, "coherent ");
        if (qualifier.volatil)
            p += snprintf(p, end - p, "volatile ");
        if (qualifier.restrict)
            p += snprintf(p, end - p, "restrict ");
        if (qualifier.readonly)
            p += snprintf(p, end - p, "readonly ");
        if (qualifier.writeonly)
            p += snprintf(p, end - p, "writeonly ");
        p += snprintf(p, end - p, "%s ", getStorageQualifierString());
        if (arraySizes) {
            if (arraySizes->getOuterSize() == 0) {
                p += snprintf(p, end - p, "implicitly-sized array of ");
            } else {
                for(int i = 0; i < (int)arraySizes->getNumDims() ; ++i) {
//                    p += snprintf(p, end - p, "%s%d", (i == 0 ? "" : "x"), arraySizes->sizes[numDimensions-1-i]);
                    p += snprintf(p, end - p, "%d-element array of ", (*arraySizes)[i]);
                }
            }
        }
        if (qualifier.precision != EpqNone)
            p += snprintf(p, end - p, "%s ", getPrecisionQualifierString());
        if (matrixCols > 0)
            p += snprintf(p, end - p, "%dX%d matrix of ", matrixCols, matrixRows);
        else if (vectorSize > 1)
            p += snprintf(p, end - p, "%d-component vector of ", vectorSize);

        *p = 0;
        TString s(buf);
        s.append(getBasicTypeString());

        if (qualifier.builtIn != EbvNone) {
            s.append(" ");
            s.append(getBuiltInVariableString());
        }

        // Add struct/block members
        if (structure) {
            s.append("{");
            for (size_t i = 0; i < structure->size(); ++i) {
                if (! (*structure)[i].type->hiddenMember()) {
                    s.append((*structure)[i].type->getCompleteString());
                    s.append(" ");
                    s.append((*structure)[i].type->getFieldName());
                    if (i < structure->size() - 1)
                        s.append(", ");
                }
            }
            s.append("}");
        }

        return s;
    }

    TString getBasicTypeString() const
    {
        if (basicType == EbtSampler)
            return sampler.getString();
        else
            return getBasicString();
    }

    const char* getStorageQualifierString() const { return GetStorageQualifierString(qualifier.storage); }
    const char* getBuiltInVariableString() const { return GetBuiltInVariableString(qualifier.builtIn); }
    const char* getPrecisionQualifierString() const { return GetPrecisionQualifierString(qualifier.precision); }
    const TTypeList* getStruct() const { return structure; }
    TTypeList* getWritableStruct() const { return structure; }  // This should only be used when known to not be sharing with other threads

    int computeNumComponents() const
    {
        int components = 0;

        if (getBasicType() == EbtStruct || getBasicType() == EbtBlock) {
            for (TTypeList::const_iterator tl = getStruct()->begin(); tl != getStruct()->end(); tl++)
                components += ((*tl).type)->computeNumComponents();
        } else if (matrixCols)
            components = matrixCols * matrixRows;
        else
            components = vectorSize;

        if (isArray()) {
            // this function can only be used in paths that have a known array size
            assert(isExplicitlySizedArray());
            components *= getArraySize();
        }

        return components;
    }

    // append this type's mangled name to the passed in 'name'
    void appendMangledName(TString& name)
    {
        buildMangledName(name);
        name += ';' ;
    }

    // Do two structure types match?  They could be declared independently,
    // in different places, but still might satisfy the definition of matching.
    // From the spec:
    //
    // "Structures must have the same name, sequence of type names, and 
    //  type definitions, and member names to be considered the same type. 
    //  This rule applies recursively for nested or embedded types."
    //
    bool sameStructType(const TType& right) const
    {
        // Most commonly, they are both 0, or the same pointer to the same actual structure
        if (structure == right.structure)
            return true;

        // Both being 0 was caught above, now they both have to be structures of the same number of elements
        if (structure == 0 || right.structure == 0 ||
            structure->size() != right.structure->size())
            return false;

        // Structure names have to match
        if (*typeName != *right.typeName)
            return false;

        // Compare the names and types of all the members, which have to match
        for (unsigned int i = 0; i < structure->size(); ++i) {
            if ((*structure)[i].type->getFieldName() != (*right.structure)[i].type->getFieldName())
                return false;

            if (*(*structure)[i].type != *(*right.structure)[i].type)
                return false;
        }

        return true;
    }

    // See if two types match, in all aspects except arrayness
    bool sameElementType(const TType& right) const
    {
        return basicType == right.basicType && sameElementShape(right);
    }

    // See if two type's arrayness match
    bool sameArrayness(const TType& right) const
    {
        return ((arraySizes == 0 && right.arraySizes == 0) ||
                (arraySizes && right.arraySizes && *arraySizes == *right.arraySizes));
    }

    // See if two type's elements match in all ways except basic type
    bool sameElementShape(const TType& right) const
    {
        return    sampler == right.sampler    &&
               vectorSize == right.vectorSize &&
               matrixCols == right.matrixCols &&
               matrixRows == right.matrixRows &&
               sameStructType(right);
    }

    // See if two types match in all ways (just the actual type, not qualification)
    bool operator==(const TType& right) const
    {
        return sameElementType(right) && sameArrayness(right);
    }

    bool operator!=(const TType& right) const
    {
        return ! operator==(right);
    }

protected:
    // Require consumer to pick between deep copy and shallow copy.
    TType(const TType& type);
    TType& operator=(const TType& type);

    void buildMangledName(TString&);

    TBasicType basicType : 8;
    int vectorSize       : 4;
    int matrixCols       : 4;
    int matrixRows       : 4;
    TSampler sampler;
    TQualifier qualifier;

    TArraySizes* arraySizes;    // 0 unless an array; can be shared across types
    TTypeList* structure;       // 0 unless this is a struct; can be shared across types
    TString *fieldName;         // for structure field names
    TString *typeName;          // for structure type name
};

} // end namespace glslang

#endif // _TYPES_INCLUDED_
