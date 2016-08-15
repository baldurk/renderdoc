/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

#ifndef YY_YY_MACHINEINDEPENDENT_GLSLANG_TAB_CPP_H_INCLUDED
# define YY_YY_MACHINEINDEPENDENT_GLSLANG_TAB_CPP_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 1
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    ATTRIBUTE = 258,
    VARYING = 259,
    CONST = 260,
    BOOL = 261,
    FLOAT = 262,
    DOUBLE = 263,
    INT = 264,
    UINT = 265,
    INT64_T = 266,
    UINT64_T = 267,
    BREAK = 268,
    CONTINUE = 269,
    DO = 270,
    ELSE = 271,
    FOR = 272,
    IF = 273,
    DISCARD = 274,
    RETURN = 275,
    SWITCH = 276,
    CASE = 277,
    DEFAULT = 278,
    SUBROUTINE = 279,
    BVEC2 = 280,
    BVEC3 = 281,
    BVEC4 = 282,
    IVEC2 = 283,
    IVEC3 = 284,
    IVEC4 = 285,
    I64VEC2 = 286,
    I64VEC3 = 287,
    I64VEC4 = 288,
    UVEC2 = 289,
    UVEC3 = 290,
    UVEC4 = 291,
    U64VEC2 = 292,
    U64VEC3 = 293,
    U64VEC4 = 294,
    VEC2 = 295,
    VEC3 = 296,
    VEC4 = 297,
    MAT2 = 298,
    MAT3 = 299,
    MAT4 = 300,
    CENTROID = 301,
    IN = 302,
    OUT = 303,
    INOUT = 304,
    UNIFORM = 305,
    PATCH = 306,
    SAMPLE = 307,
    BUFFER = 308,
    SHARED = 309,
    COHERENT = 310,
    VOLATILE = 311,
    RESTRICT = 312,
    READONLY = 313,
    WRITEONLY = 314,
    DVEC2 = 315,
    DVEC3 = 316,
    DVEC4 = 317,
    DMAT2 = 318,
    DMAT3 = 319,
    DMAT4 = 320,
    NOPERSPECTIVE = 321,
    FLAT = 322,
    SMOOTH = 323,
    LAYOUT = 324,
    __EXPLICITINTERPAMD = 325,
    MAT2X2 = 326,
    MAT2X3 = 327,
    MAT2X4 = 328,
    MAT3X2 = 329,
    MAT3X3 = 330,
    MAT3X4 = 331,
    MAT4X2 = 332,
    MAT4X3 = 333,
    MAT4X4 = 334,
    DMAT2X2 = 335,
    DMAT2X3 = 336,
    DMAT2X4 = 337,
    DMAT3X2 = 338,
    DMAT3X3 = 339,
    DMAT3X4 = 340,
    DMAT4X2 = 341,
    DMAT4X3 = 342,
    DMAT4X4 = 343,
    ATOMIC_UINT = 344,
    SAMPLER1D = 345,
    SAMPLER2D = 346,
    SAMPLER3D = 347,
    SAMPLERCUBE = 348,
    SAMPLER1DSHADOW = 349,
    SAMPLER2DSHADOW = 350,
    SAMPLERCUBESHADOW = 351,
    SAMPLER1DARRAY = 352,
    SAMPLER2DARRAY = 353,
    SAMPLER1DARRAYSHADOW = 354,
    SAMPLER2DARRAYSHADOW = 355,
    ISAMPLER1D = 356,
    ISAMPLER2D = 357,
    ISAMPLER3D = 358,
    ISAMPLERCUBE = 359,
    ISAMPLER1DARRAY = 360,
    ISAMPLER2DARRAY = 361,
    USAMPLER1D = 362,
    USAMPLER2D = 363,
    USAMPLER3D = 364,
    USAMPLERCUBE = 365,
    USAMPLER1DARRAY = 366,
    USAMPLER2DARRAY = 367,
    SAMPLER2DRECT = 368,
    SAMPLER2DRECTSHADOW = 369,
    ISAMPLER2DRECT = 370,
    USAMPLER2DRECT = 371,
    SAMPLERBUFFER = 372,
    ISAMPLERBUFFER = 373,
    USAMPLERBUFFER = 374,
    SAMPLERCUBEARRAY = 375,
    SAMPLERCUBEARRAYSHADOW = 376,
    ISAMPLERCUBEARRAY = 377,
    USAMPLERCUBEARRAY = 378,
    SAMPLER2DMS = 379,
    ISAMPLER2DMS = 380,
    USAMPLER2DMS = 381,
    SAMPLER2DMSARRAY = 382,
    ISAMPLER2DMSARRAY = 383,
    USAMPLER2DMSARRAY = 384,
    SAMPLEREXTERNALOES = 385,
    SAMPLER = 386,
    SAMPLERSHADOW = 387,
    TEXTURE1D = 388,
    TEXTURE2D = 389,
    TEXTURE3D = 390,
    TEXTURECUBE = 391,
    TEXTURE1DARRAY = 392,
    TEXTURE2DARRAY = 393,
    ITEXTURE1D = 394,
    ITEXTURE2D = 395,
    ITEXTURE3D = 396,
    ITEXTURECUBE = 397,
    ITEXTURE1DARRAY = 398,
    ITEXTURE2DARRAY = 399,
    UTEXTURE1D = 400,
    UTEXTURE2D = 401,
    UTEXTURE3D = 402,
    UTEXTURECUBE = 403,
    UTEXTURE1DARRAY = 404,
    UTEXTURE2DARRAY = 405,
    TEXTURE2DRECT = 406,
    ITEXTURE2DRECT = 407,
    UTEXTURE2DRECT = 408,
    TEXTUREBUFFER = 409,
    ITEXTUREBUFFER = 410,
    UTEXTUREBUFFER = 411,
    TEXTURECUBEARRAY = 412,
    ITEXTURECUBEARRAY = 413,
    UTEXTURECUBEARRAY = 414,
    TEXTURE2DMS = 415,
    ITEXTURE2DMS = 416,
    UTEXTURE2DMS = 417,
    TEXTURE2DMSARRAY = 418,
    ITEXTURE2DMSARRAY = 419,
    UTEXTURE2DMSARRAY = 420,
    SUBPASSINPUT = 421,
    SUBPASSINPUTMS = 422,
    ISUBPASSINPUT = 423,
    ISUBPASSINPUTMS = 424,
    USUBPASSINPUT = 425,
    USUBPASSINPUTMS = 426,
    IMAGE1D = 427,
    IIMAGE1D = 428,
    UIMAGE1D = 429,
    IMAGE2D = 430,
    IIMAGE2D = 431,
    UIMAGE2D = 432,
    IMAGE3D = 433,
    IIMAGE3D = 434,
    UIMAGE3D = 435,
    IMAGE2DRECT = 436,
    IIMAGE2DRECT = 437,
    UIMAGE2DRECT = 438,
    IMAGECUBE = 439,
    IIMAGECUBE = 440,
    UIMAGECUBE = 441,
    IMAGEBUFFER = 442,
    IIMAGEBUFFER = 443,
    UIMAGEBUFFER = 444,
    IMAGE1DARRAY = 445,
    IIMAGE1DARRAY = 446,
    UIMAGE1DARRAY = 447,
    IMAGE2DARRAY = 448,
    IIMAGE2DARRAY = 449,
    UIMAGE2DARRAY = 450,
    IMAGECUBEARRAY = 451,
    IIMAGECUBEARRAY = 452,
    UIMAGECUBEARRAY = 453,
    IMAGE2DMS = 454,
    IIMAGE2DMS = 455,
    UIMAGE2DMS = 456,
    IMAGE2DMSARRAY = 457,
    IIMAGE2DMSARRAY = 458,
    UIMAGE2DMSARRAY = 459,
    STRUCT = 460,
    VOID = 461,
    WHILE = 462,
    IDENTIFIER = 463,
    TYPE_NAME = 464,
    FLOATCONSTANT = 465,
    DOUBLECONSTANT = 466,
    INTCONSTANT = 467,
    UINTCONSTANT = 468,
    INT64CONSTANT = 469,
    UINT64CONSTANT = 470,
    BOOLCONSTANT = 471,
    LEFT_OP = 472,
    RIGHT_OP = 473,
    INC_OP = 474,
    DEC_OP = 475,
    LE_OP = 476,
    GE_OP = 477,
    EQ_OP = 478,
    NE_OP = 479,
    AND_OP = 480,
    OR_OP = 481,
    XOR_OP = 482,
    MUL_ASSIGN = 483,
    DIV_ASSIGN = 484,
    ADD_ASSIGN = 485,
    MOD_ASSIGN = 486,
    LEFT_ASSIGN = 487,
    RIGHT_ASSIGN = 488,
    AND_ASSIGN = 489,
    XOR_ASSIGN = 490,
    OR_ASSIGN = 491,
    SUB_ASSIGN = 492,
    LEFT_PAREN = 493,
    RIGHT_PAREN = 494,
    LEFT_BRACKET = 495,
    RIGHT_BRACKET = 496,
    LEFT_BRACE = 497,
    RIGHT_BRACE = 498,
    DOT = 499,
    COMMA = 500,
    COLON = 501,
    EQUAL = 502,
    SEMICOLON = 503,
    BANG = 504,
    DASH = 505,
    TILDE = 506,
    PLUS = 507,
    STAR = 508,
    SLASH = 509,
    PERCENT = 510,
    LEFT_ANGLE = 511,
    RIGHT_ANGLE = 512,
    VERTICAL_BAR = 513,
    CARET = 514,
    AMPERSAND = 515,
    QUESTION = 516,
    INVARIANT = 517,
    PRECISE = 518,
    HIGH_PRECISION = 519,
    MEDIUM_PRECISION = 520,
    LOW_PRECISION = 521,
    PRECISION = 522,
    PACKED = 523,
    RESOURCE = 524,
    SUPERP = 525
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED

union YYSTYPE
{
#line 66 "MachineIndependent/glslang.y" /* yacc.c:1909  */

    struct {
        glslang::TSourceLoc loc;
        union {
            glslang::TString *string;
            int i;
            unsigned int u;
            long long i64;
            unsigned long long u64;
            bool b;
            double d;
        };
        glslang::TSymbol* symbol;
    } lex;
    struct {
        glslang::TSourceLoc loc;
        glslang::TOperator op;
        union {
            TIntermNode* intermNode;
            glslang::TIntermNodePair nodePair;
            glslang::TIntermTyped* intermTypedNode;
        };
        union {
            glslang::TPublicType type;
            glslang::TFunction* function;
            glslang::TParameter param;
            glslang::TTypeLoc typeLine;
            glslang::TTypeList* typeList;
            glslang::TArraySizes* arraySizes;
            glslang::TIdentifierList* identifierList;
        };
    } interm;

#line 359 "MachineIndependent/glslang_tab.cpp.h" /* yacc.c:1909  */
};

typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif



int yyparse (glslang::TParseContext* pParseContext);

#endif /* !YY_YY_MACHINEINDEPENDENT_GLSLANG_TAB_CPP_H_INCLUDED  */
