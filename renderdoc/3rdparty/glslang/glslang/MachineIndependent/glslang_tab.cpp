/* A Bison parser, made by GNU Bison 2.7.  */

/* Bison implementation for Yacc-like parsers in C
   
      Copyright (C) 1984, 1989-1990, 2000-2012 Free Software Foundation, Inc.
   
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

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.7"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* Copy the first part of user declarations.  */
/* Line 371 of yacc.c  */
#line 41 "glslang.y"


/* Based on:
ANSI C Yacc grammar

In 1985, Jeff Lee published his Yacc grammar (which is accompanied by a
matching Lex specification) for the April 30, 1985 draft version of the
ANSI C standard.  Tom Stockfisch reposted it to net.sources in 1987; that
original, as mentioned in the answer to question 17.25 of the comp.lang.c
FAQ, can be ftp'ed from ftp.uu.net, file usenet/net.sources/ansi.c.grammar.Z.

I intend to keep this version as close to the current C Standard grammar as
possible; please let me know if you discover discrepancies.

Jutta Degener, 1995
*/

#include "SymbolTable.h"
#include "ParseHelper.h"
#include "../Public/ShaderLang.h"

using namespace glslang;


/* Line 371 of yacc.c  */
#line 93 "glslang_tab.cpp"

# ifndef YY_NULL
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULL nullptr
#  else
#   define YY_NULL 0
#  endif
# endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* In a future release of Bison, this section will be replaced
   by #include "glslang_tab.cpp.h".  */
#ifndef YY_YY_GLSLANG_TAB_CPP_H_INCLUDED
# define YY_YY_GLSLANG_TAB_CPP_H_INCLUDED
/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 1
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     ATTRIBUTE = 258,
     VARYING = 259,
     CONST = 260,
     BOOL = 261,
     FLOAT = 262,
     DOUBLE = 263,
     INT = 264,
     UINT = 265,
     BREAK = 266,
     CONTINUE = 267,
     DO = 268,
     ELSE = 269,
     FOR = 270,
     IF = 271,
     DISCARD = 272,
     RETURN = 273,
     SWITCH = 274,
     CASE = 275,
     DEFAULT = 276,
     SUBROUTINE = 277,
     BVEC2 = 278,
     BVEC3 = 279,
     BVEC4 = 280,
     IVEC2 = 281,
     IVEC3 = 282,
     IVEC4 = 283,
     UVEC2 = 284,
     UVEC3 = 285,
     UVEC4 = 286,
     VEC2 = 287,
     VEC3 = 288,
     VEC4 = 289,
     MAT2 = 290,
     MAT3 = 291,
     MAT4 = 292,
     CENTROID = 293,
     IN = 294,
     OUT = 295,
     INOUT = 296,
     UNIFORM = 297,
     PATCH = 298,
     SAMPLE = 299,
     BUFFER = 300,
     SHARED = 301,
     COHERENT = 302,
     VOLATILE = 303,
     RESTRICT = 304,
     READONLY = 305,
     WRITEONLY = 306,
     DVEC2 = 307,
     DVEC3 = 308,
     DVEC4 = 309,
     DMAT2 = 310,
     DMAT3 = 311,
     DMAT4 = 312,
     NOPERSPECTIVE = 313,
     FLAT = 314,
     SMOOTH = 315,
     LAYOUT = 316,
     MAT2X2 = 317,
     MAT2X3 = 318,
     MAT2X4 = 319,
     MAT3X2 = 320,
     MAT3X3 = 321,
     MAT3X4 = 322,
     MAT4X2 = 323,
     MAT4X3 = 324,
     MAT4X4 = 325,
     DMAT2X2 = 326,
     DMAT2X3 = 327,
     DMAT2X4 = 328,
     DMAT3X2 = 329,
     DMAT3X3 = 330,
     DMAT3X4 = 331,
     DMAT4X2 = 332,
     DMAT4X3 = 333,
     DMAT4X4 = 334,
     ATOMIC_UINT = 335,
     SAMPLER1D = 336,
     SAMPLER2D = 337,
     SAMPLER3D = 338,
     SAMPLERCUBE = 339,
     SAMPLER1DSHADOW = 340,
     SAMPLER2DSHADOW = 341,
     SAMPLERCUBESHADOW = 342,
     SAMPLER1DARRAY = 343,
     SAMPLER2DARRAY = 344,
     SAMPLER1DARRAYSHADOW = 345,
     SAMPLER2DARRAYSHADOW = 346,
     ISAMPLER1D = 347,
     ISAMPLER2D = 348,
     ISAMPLER3D = 349,
     ISAMPLERCUBE = 350,
     ISAMPLER1DARRAY = 351,
     ISAMPLER2DARRAY = 352,
     USAMPLER1D = 353,
     USAMPLER2D = 354,
     USAMPLER3D = 355,
     USAMPLERCUBE = 356,
     USAMPLER1DARRAY = 357,
     USAMPLER2DARRAY = 358,
     SAMPLER2DRECT = 359,
     SAMPLER2DRECTSHADOW = 360,
     ISAMPLER2DRECT = 361,
     USAMPLER2DRECT = 362,
     SAMPLERBUFFER = 363,
     ISAMPLERBUFFER = 364,
     USAMPLERBUFFER = 365,
     SAMPLERCUBEARRAY = 366,
     SAMPLERCUBEARRAYSHADOW = 367,
     ISAMPLERCUBEARRAY = 368,
     USAMPLERCUBEARRAY = 369,
     SAMPLER2DMS = 370,
     ISAMPLER2DMS = 371,
     USAMPLER2DMS = 372,
     SAMPLER2DMSARRAY = 373,
     ISAMPLER2DMSARRAY = 374,
     USAMPLER2DMSARRAY = 375,
     SAMPLEREXTERNALOES = 376,
     IMAGE1D = 377,
     IIMAGE1D = 378,
     UIMAGE1D = 379,
     IMAGE2D = 380,
     IIMAGE2D = 381,
     UIMAGE2D = 382,
     IMAGE3D = 383,
     IIMAGE3D = 384,
     UIMAGE3D = 385,
     IMAGE2DRECT = 386,
     IIMAGE2DRECT = 387,
     UIMAGE2DRECT = 388,
     IMAGECUBE = 389,
     IIMAGECUBE = 390,
     UIMAGECUBE = 391,
     IMAGEBUFFER = 392,
     IIMAGEBUFFER = 393,
     UIMAGEBUFFER = 394,
     IMAGE1DARRAY = 395,
     IIMAGE1DARRAY = 396,
     UIMAGE1DARRAY = 397,
     IMAGE2DARRAY = 398,
     IIMAGE2DARRAY = 399,
     UIMAGE2DARRAY = 400,
     IMAGECUBEARRAY = 401,
     IIMAGECUBEARRAY = 402,
     UIMAGECUBEARRAY = 403,
     IMAGE2DMS = 404,
     IIMAGE2DMS = 405,
     UIMAGE2DMS = 406,
     IMAGE2DMSARRAY = 407,
     IIMAGE2DMSARRAY = 408,
     UIMAGE2DMSARRAY = 409,
     STRUCT = 410,
     VOID = 411,
     WHILE = 412,
     IDENTIFIER = 413,
     TYPE_NAME = 414,
     FLOATCONSTANT = 415,
     DOUBLECONSTANT = 416,
     INTCONSTANT = 417,
     UINTCONSTANT = 418,
     BOOLCONSTANT = 419,
     FIELD_SELECTION = 420,
     LEFT_OP = 421,
     RIGHT_OP = 422,
     INC_OP = 423,
     DEC_OP = 424,
     LE_OP = 425,
     GE_OP = 426,
     EQ_OP = 427,
     NE_OP = 428,
     AND_OP = 429,
     OR_OP = 430,
     XOR_OP = 431,
     MUL_ASSIGN = 432,
     DIV_ASSIGN = 433,
     ADD_ASSIGN = 434,
     MOD_ASSIGN = 435,
     LEFT_ASSIGN = 436,
     RIGHT_ASSIGN = 437,
     AND_ASSIGN = 438,
     XOR_ASSIGN = 439,
     OR_ASSIGN = 440,
     SUB_ASSIGN = 441,
     LEFT_PAREN = 442,
     RIGHT_PAREN = 443,
     LEFT_BRACKET = 444,
     RIGHT_BRACKET = 445,
     LEFT_BRACE = 446,
     RIGHT_BRACE = 447,
     DOT = 448,
     COMMA = 449,
     COLON = 450,
     EQUAL = 451,
     SEMICOLON = 452,
     BANG = 453,
     DASH = 454,
     TILDE = 455,
     PLUS = 456,
     STAR = 457,
     SLASH = 458,
     PERCENT = 459,
     LEFT_ANGLE = 460,
     RIGHT_ANGLE = 461,
     VERTICAL_BAR = 462,
     CARET = 463,
     AMPERSAND = 464,
     QUESTION = 465,
     INVARIANT = 466,
     PRECISE = 467,
     HIGH_PRECISION = 468,
     MEDIUM_PRECISION = 469,
     LOW_PRECISION = 470,
     PRECISION = 471,
     PACKED = 472,
     RESOURCE = 473,
     SUPERP = 474
   };
#endif


#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{
/* Line 387 of yacc.c  */
#line 66 "glslang.y"

    struct {
        glslang::TSourceLoc loc;
        union {
            glslang::TString *string;
            int i;
            unsigned int u;
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


/* Line 387 of yacc.c  */
#line 388 "glslang_tab.cpp"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif


#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (glslang::TParseContext* pParseContext);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */

#endif /* !YY_YY_GLSLANG_TAB_CPP_H_INCLUDED  */

/* Copy the second part of user declarations.  */
/* Line 390 of yacc.c  */
#line 98 "glslang.y"

    
#pragma warning(disable : 4065)
#pragma warning(disable : 4127)
#pragma warning(disable : 4244)

#define parseContext (*pParseContext)
#define yyerror(context, msg) context->parserError(msg)

extern int yylex(YYSTYPE*, TParseContext&);


/* Line 390 of yacc.c  */
#line 428 "glslang_tab.cpp"

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(N) (N)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int yyi)
#else
static int
YYID (yyi)
    int yyi;
#endif
{
  return yyi;
}
#endif

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
	 || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)				\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack_alloc, Stack, yysize);			\
	Stack = &yyptr->Stack_alloc;					\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, (Count) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYSIZE_T yyi;                         \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (YYID (0))
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  199
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   4737

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  220
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  100
/* YYNRULES -- Number of rules.  */
#define YYNRULES  370
/* YYNRULES -- Number of states.  */
#define YYNSTATES  502

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   474

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   134,
     135,   136,   137,   138,   139,   140,   141,   142,   143,   144,
     145,   146,   147,   148,   149,   150,   151,   152,   153,   154,
     155,   156,   157,   158,   159,   160,   161,   162,   163,   164,
     165,   166,   167,   168,   169,   170,   171,   172,   173,   174,
     175,   176,   177,   178,   179,   180,   181,   182,   183,   184,
     185,   186,   187,   188,   189,   190,   191,   192,   193,   194,
     195,   196,   197,   198,   199,   200,   201,   202,   203,   204,
     205,   206,   207,   208,   209,   210,   211,   212,   213,   214,
     215,   216,   217,   218,   219
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint16 yyprhs[] =
{
       0,     0,     3,     5,     7,     9,    11,    13,    15,    17,
      21,    23,    28,    30,    34,    37,    40,    42,    44,    46,
      49,    52,    55,    57,    60,    64,    67,    69,    71,    73,
      76,    79,    82,    84,    86,    88,    90,    92,    96,   100,
     104,   106,   110,   114,   116,   120,   124,   126,   130,   134,
     138,   142,   144,   148,   152,   154,   158,   160,   164,   166,
     170,   172,   176,   178,   182,   184,   188,   190,   191,   198,
     200,   204,   206,   208,   210,   212,   214,   216,   218,   220,
     222,   224,   226,   228,   232,   234,   237,   240,   245,   248,
     252,   257,   260,   264,   269,   270,   277,   280,   284,   287,
     289,   291,   294,   298,   302,   305,   309,   312,   314,   317,
     319,   321,   323,   327,   332,   339,   345,   347,   350,   354,
     360,   365,   367,   370,   372,   374,   376,   378,   383,   385,
     389,   391,   395,   397,   399,   401,   404,   406,   408,   410,
     412,   414,   416,   418,   420,   422,   424,   426,   428,   430,
     432,   434,   436,   438,   440,   442,   444,   446,   448,   450,
     452,   457,   459,   463,   465,   468,   471,   475,   479,   484,
     486,   488,   490,   492,   494,   496,   498,   500,   502,   504,
     506,   508,   510,   512,   514,   516,   518,   520,   522,   524,
     526,   528,   530,   532,   534,   536,   538,   540,   542,   544,
     546,   548,   550,   552,   554,   556,   558,   560,   562,   564,
     566,   568,   570,   572,   574,   576,   578,   580,   582,   584,
     586,   588,   590,   592,   594,   596,   598,   600,   602,   604,
     606,   608,   610,   612,   614,   616,   618,   620,   622,   624,
     626,   628,   630,   632,   634,   636,   638,   640,   642,   644,
     646,   648,   650,   652,   654,   656,   658,   660,   662,   664,
     666,   668,   670,   672,   674,   676,   678,   680,   682,   684,
     686,   688,   690,   692,   694,   696,   698,   700,   702,   704,
     706,   708,   710,   712,   714,   716,   718,   720,   722,   724,
     726,   728,   730,   732,   734,   735,   742,   743,   749,   751,
     754,   758,   763,   765,   769,   771,   774,   776,   780,   785,
     787,   791,   793,   795,   797,   799,   801,   803,   805,   807,
     809,   811,   814,   815,   816,   822,   824,   826,   827,   830,
     831,   834,   837,   841,   843,   846,   848,   851,   857,   861,
     863,   865,   870,   871,   880,   881,   883,   887,   890,   891,
     898,   899,   908,   909,   917,   919,   921,   923,   924,   927,
     931,   934,   937,   940,   944,   947,   949,   952,   954,   956,
     957
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int16 yyrhs[] =
{
     316,     0,    -1,   158,    -1,   221,    -1,   162,    -1,   163,
      -1,   160,    -1,   161,    -1,   164,    -1,   187,   249,   188,
      -1,   222,    -1,   223,   189,   224,   190,    -1,   225,    -1,
     223,   193,   165,    -1,   223,   168,    -1,   223,   169,    -1,
     249,    -1,   226,    -1,   227,    -1,   229,   188,    -1,   228,
     188,    -1,   230,   156,    -1,   230,    -1,   230,   247,    -1,
     229,   194,   247,    -1,   231,   187,    -1,   275,    -1,   223,
      -1,   223,    -1,   168,   232,    -1,   169,   232,    -1,   233,
     232,    -1,   201,    -1,   199,    -1,   198,    -1,   200,    -1,
     232,    -1,   234,   202,   232,    -1,   234,   203,   232,    -1,
     234,   204,   232,    -1,   234,    -1,   235,   201,   234,    -1,
     235,   199,   234,    -1,   235,    -1,   236,   166,   235,    -1,
     236,   167,   235,    -1,   236,    -1,   237,   205,   236,    -1,
     237,   206,   236,    -1,   237,   170,   236,    -1,   237,   171,
     236,    -1,   237,    -1,   238,   172,   237,    -1,   238,   173,
     237,    -1,   238,    -1,   239,   209,   238,    -1,   239,    -1,
     240,   208,   239,    -1,   240,    -1,   241,   207,   240,    -1,
     241,    -1,   242,   174,   241,    -1,   242,    -1,   243,   176,
     242,    -1,   243,    -1,   244,   175,   243,    -1,   244,    -1,
      -1,   244,   210,   246,   249,   195,   247,    -1,   245,    -1,
     232,   248,   247,    -1,   196,    -1,   177,    -1,   178,    -1,
     180,    -1,   179,    -1,   186,    -1,   181,    -1,   182,    -1,
     183,    -1,   184,    -1,   185,    -1,   247,    -1,   249,   194,
     247,    -1,   245,    -1,   255,   197,    -1,   262,   197,    -1,
     216,   278,   275,   197,    -1,   252,   197,    -1,   252,   158,
     197,    -1,   252,   158,   276,   197,    -1,   271,   197,    -1,
     271,   158,   197,    -1,   271,   158,   254,   197,    -1,    -1,
     271,   158,   191,   253,   282,   192,    -1,   194,   158,    -1,
     254,   194,   158,    -1,   256,   188,    -1,   258,    -1,   257,
      -1,   258,   260,    -1,   257,   194,   260,    -1,   264,   158,
     187,    -1,   275,   158,    -1,   275,   158,   276,    -1,   271,
     259,    -1,   259,    -1,   271,   261,    -1,   261,    -1,   275,
      -1,   263,    -1,   262,   194,   158,    -1,   262,   194,   158,
     276,    -1,   262,   194,   158,   276,   196,   286,    -1,   262,
     194,   158,   196,   286,    -1,   264,    -1,   264,   158,    -1,
     264,   158,   276,    -1,   264,   158,   276,   196,   286,    -1,
     264,   158,   196,   286,    -1,   275,    -1,   271,   275,    -1,
     211,    -1,    60,    -1,    59,    -1,    58,    -1,    61,   187,
     268,   188,    -1,   269,    -1,   268,   194,   269,    -1,   158,
      -1,   158,   196,   250,    -1,    46,    -1,   212,    -1,   272,
      -1,   271,   272,    -1,   273,    -1,   267,    -1,   278,    -1,
     266,    -1,   265,    -1,   270,    -1,     5,    -1,     3,    -1,
       4,    -1,    41,    -1,    39,    -1,    40,    -1,    38,    -1,
      43,    -1,    44,    -1,    42,    -1,    45,    -1,    46,    -1,
      47,    -1,    48,    -1,    49,    -1,    50,    -1,    51,    -1,
      22,    -1,    22,   187,   274,   188,    -1,   159,    -1,   274,
     194,   159,    -1,   277,    -1,   277,   276,    -1,   189,   190,
      -1,   189,   250,   190,    -1,   276,   189,   190,    -1,   276,
     189,   250,   190,    -1,   156,    -1,     7,    -1,     8,    -1,
       9,    -1,    10,    -1,     6,    -1,    32,    -1,    33,    -1,
      34,    -1,    52,    -1,    53,    -1,    54,    -1,    23,    -1,
      24,    -1,    25,    -1,    26,    -1,    27,    -1,    28,    -1,
      29,    -1,    30,    -1,    31,    -1,    35,    -1,    36,    -1,
      37,    -1,    62,    -1,    63,    -1,    64,    -1,    65,    -1,
      66,    -1,    67,    -1,    68,    -1,    69,    -1,    70,    -1,
      55,    -1,    56,    -1,    57,    -1,    71,    -1,    72,    -1,
      73,    -1,    74,    -1,    75,    -1,    76,    -1,    77,    -1,
      78,    -1,    79,    -1,    80,    -1,    81,    -1,    82,    -1,
      83,    -1,    84,    -1,    85,    -1,    86,    -1,    87,    -1,
      88,    -1,    89,    -1,    90,    -1,    91,    -1,   111,    -1,
     112,    -1,    92,    -1,    93,    -1,    94,    -1,    95,    -1,
      96,    -1,    97,    -1,   113,    -1,    98,    -1,    99,    -1,
     100,    -1,   101,    -1,   102,    -1,   103,    -1,   114,    -1,
     104,    -1,   105,    -1,   106,    -1,   107,    -1,   108,    -1,
     109,    -1,   110,    -1,   115,    -1,   116,    -1,   117,    -1,
     118,    -1,   119,    -1,   120,    -1,   122,    -1,   123,    -1,
     124,    -1,   125,    -1,   126,    -1,   127,    -1,   128,    -1,
     129,    -1,   130,    -1,   131,    -1,   132,    -1,   133,    -1,
     134,    -1,   135,    -1,   136,    -1,   137,    -1,   138,    -1,
     139,    -1,   140,    -1,   141,    -1,   142,    -1,   143,    -1,
     144,    -1,   145,    -1,   146,    -1,   147,    -1,   148,    -1,
     149,    -1,   150,    -1,   151,    -1,   152,    -1,   153,    -1,
     154,    -1,   121,    -1,   279,    -1,   159,    -1,   213,    -1,
     214,    -1,   215,    -1,    -1,   155,   158,   191,   280,   282,
     192,    -1,    -1,   155,   191,   281,   282,   192,    -1,   283,
      -1,   282,   283,    -1,   275,   284,   197,    -1,   271,   275,
     284,   197,    -1,   285,    -1,   284,   194,   285,    -1,   158,
      -1,   158,   276,    -1,   247,    -1,   191,   287,   192,    -1,
     191,   287,   194,   192,    -1,   286,    -1,   287,   194,   286,
      -1,   251,    -1,   291,    -1,   290,    -1,   288,    -1,   300,
      -1,   301,    -1,   304,    -1,   307,    -1,   308,    -1,   315,
      -1,   191,   192,    -1,    -1,    -1,   191,   292,   299,   293,
     192,    -1,   298,    -1,   290,    -1,    -1,   296,   291,    -1,
      -1,   297,   290,    -1,   191,   192,    -1,   191,   299,   192,
      -1,   289,    -1,   299,   289,    -1,   197,    -1,   249,   197,
      -1,    16,   187,   249,   188,   302,    -1,   295,    14,   295,
      -1,   295,    -1,   249,    -1,   264,   158,   196,   286,    -1,
      -1,    19,   187,   249,   188,   305,   191,   306,   192,    -1,
      -1,   299,    -1,    20,   249,   195,    -1,    21,   195,    -1,
      -1,   157,   187,   309,   303,   188,   294,    -1,    -1,    13,
     310,   289,   157,   187,   249,   188,   197,    -1,    -1,    15,
     187,   311,   312,   314,   188,   294,    -1,   300,    -1,   288,
      -1,   303,    -1,    -1,   313,   197,    -1,   313,   197,   249,
      -1,    12,   197,    -1,    11,   197,    -1,    18,   197,    -1,
      18,   249,   197,    -1,    17,   197,    -1,   317,    -1,   316,
     317,    -1,   318,    -1,   251,    -1,    -1,   255,   319,   298,
      -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   223,   223,   229,   232,   235,   239,   242,   246,   249,
     257,   260,   263,   266,   269,   274,   282,   289,   296,   302,
     306,   313,   316,   322,   329,   339,   347,   352,   382,   388,
     392,   396,   416,   417,   418,   419,   425,   426,   431,   436,
     445,   446,   451,   459,   460,   466,   475,   476,   481,   486,
     491,   499,   500,   507,   517,   518,   527,   528,   537,   538,
     547,   548,   556,   557,   565,   566,   574,   575,   575,   593,
     594,   608,   612,   616,   620,   625,   629,   633,   637,   641,
     645,   649,   656,   659,   669,   676,   681,   686,   694,   698,
     702,   706,   711,   716,   725,   725,   736,   740,   747,   754,
     757,   764,   772,   792,   810,   825,   848,   859,   869,   879,
     889,   898,   901,   905,   909,   914,   922,   927,   932,   937,
     942,   951,   962,   989,   998,  1005,  1012,  1022,  1028,  1031,
    1038,  1042,  1046,  1054,  1060,  1063,  1074,  1077,  1080,  1083,
    1087,  1091,  1098,  1102,  1114,  1128,  1133,  1139,  1145,  1152,
    1158,  1163,  1168,  1173,  1180,  1184,  1188,  1192,  1196,  1200,
    1205,  1216,  1219,  1224,  1228,  1237,  1242,  1250,  1254,  1264,
    1268,  1272,  1277,  1281,  1286,  1290,  1295,  1300,  1305,  1311,
    1317,  1323,  1328,  1333,  1338,  1343,  1348,  1353,  1359,  1365,
    1371,  1376,  1381,  1386,  1391,  1396,  1401,  1406,  1411,  1416,
    1421,  1426,  1431,  1437,  1443,  1449,  1455,  1461,  1467,  1473,
    1479,  1485,  1491,  1497,  1503,  1507,  1512,  1517,  1522,  1527,
    1532,  1537,  1542,  1547,  1552,  1557,  1562,  1567,  1572,  1577,
    1582,  1587,  1592,  1597,  1602,  1607,  1612,  1617,  1622,  1627,
    1632,  1637,  1642,  1647,  1652,  1657,  1662,  1667,  1672,  1677,
    1682,  1687,  1692,  1697,  1702,  1707,  1712,  1717,  1722,  1727,
    1732,  1737,  1742,  1747,  1752,  1757,  1762,  1767,  1772,  1777,
    1782,  1787,  1792,  1797,  1802,  1807,  1812,  1817,  1822,  1827,
    1832,  1837,  1842,  1847,  1852,  1857,  1862,  1867,  1872,  1878,
    1883,  1899,  1905,  1911,  1920,  1920,  1931,  1931,  1941,  1944,
    1957,  1975,  1999,  2003,  2009,  2014,  2025,  2028,  2034,  2043,
    2046,  2052,  2056,  2057,  2063,  2064,  2065,  2066,  2067,  2068,
    2069,  2073,  2074,  2078,  2074,  2090,  2091,  2095,  2095,  2102,
    2102,  2116,  2119,  2127,  2135,  2146,  2147,  2151,  2158,  2162,
    2170,  2174,  2187,  2187,  2207,  2210,  2216,  2228,  2240,  2240,
    2255,  2255,  2271,  2271,  2292,  2295,  2301,  2304,  2310,  2314,
    2321,  2326,  2331,  2338,  2356,  2365,  2369,  2376,  2379,  2385,
    2385
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 0
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "ATTRIBUTE", "VARYING", "CONST", "BOOL",
  "FLOAT", "DOUBLE", "INT", "UINT", "BREAK", "CONTINUE", "DO", "ELSE",
  "FOR", "IF", "DISCARD", "RETURN", "SWITCH", "CASE", "DEFAULT",
  "SUBROUTINE", "BVEC2", "BVEC3", "BVEC4", "IVEC2", "IVEC3", "IVEC4",
  "UVEC2", "UVEC3", "UVEC4", "VEC2", "VEC3", "VEC4", "MAT2", "MAT3",
  "MAT4", "CENTROID", "IN", "OUT", "INOUT", "UNIFORM", "PATCH", "SAMPLE",
  "BUFFER", "SHARED", "COHERENT", "VOLATILE", "RESTRICT", "READONLY",
  "WRITEONLY", "DVEC2", "DVEC3", "DVEC4", "DMAT2", "DMAT3", "DMAT4",
  "NOPERSPECTIVE", "FLAT", "SMOOTH", "LAYOUT", "MAT2X2", "MAT2X3",
  "MAT2X4", "MAT3X2", "MAT3X3", "MAT3X4", "MAT4X2", "MAT4X3", "MAT4X4",
  "DMAT2X2", "DMAT2X3", "DMAT2X4", "DMAT3X2", "DMAT3X3", "DMAT3X4",
  "DMAT4X2", "DMAT4X3", "DMAT4X4", "ATOMIC_UINT", "SAMPLER1D", "SAMPLER2D",
  "SAMPLER3D", "SAMPLERCUBE", "SAMPLER1DSHADOW", "SAMPLER2DSHADOW",
  "SAMPLERCUBESHADOW", "SAMPLER1DARRAY", "SAMPLER2DARRAY",
  "SAMPLER1DARRAYSHADOW", "SAMPLER2DARRAYSHADOW", "ISAMPLER1D",
  "ISAMPLER2D", "ISAMPLER3D", "ISAMPLERCUBE", "ISAMPLER1DARRAY",
  "ISAMPLER2DARRAY", "USAMPLER1D", "USAMPLER2D", "USAMPLER3D",
  "USAMPLERCUBE", "USAMPLER1DARRAY", "USAMPLER2DARRAY", "SAMPLER2DRECT",
  "SAMPLER2DRECTSHADOW", "ISAMPLER2DRECT", "USAMPLER2DRECT",
  "SAMPLERBUFFER", "ISAMPLERBUFFER", "USAMPLERBUFFER", "SAMPLERCUBEARRAY",
  "SAMPLERCUBEARRAYSHADOW", "ISAMPLERCUBEARRAY", "USAMPLERCUBEARRAY",
  "SAMPLER2DMS", "ISAMPLER2DMS", "USAMPLER2DMS", "SAMPLER2DMSARRAY",
  "ISAMPLER2DMSARRAY", "USAMPLER2DMSARRAY", "SAMPLEREXTERNALOES",
  "IMAGE1D", "IIMAGE1D", "UIMAGE1D", "IMAGE2D", "IIMAGE2D", "UIMAGE2D",
  "IMAGE3D", "IIMAGE3D", "UIMAGE3D", "IMAGE2DRECT", "IIMAGE2DRECT",
  "UIMAGE2DRECT", "IMAGECUBE", "IIMAGECUBE", "UIMAGECUBE", "IMAGEBUFFER",
  "IIMAGEBUFFER", "UIMAGEBUFFER", "IMAGE1DARRAY", "IIMAGE1DARRAY",
  "UIMAGE1DARRAY", "IMAGE2DARRAY", "IIMAGE2DARRAY", "UIMAGE2DARRAY",
  "IMAGECUBEARRAY", "IIMAGECUBEARRAY", "UIMAGECUBEARRAY", "IMAGE2DMS",
  "IIMAGE2DMS", "UIMAGE2DMS", "IMAGE2DMSARRAY", "IIMAGE2DMSARRAY",
  "UIMAGE2DMSARRAY", "STRUCT", "VOID", "WHILE", "IDENTIFIER", "TYPE_NAME",
  "FLOATCONSTANT", "DOUBLECONSTANT", "INTCONSTANT", "UINTCONSTANT",
  "BOOLCONSTANT", "FIELD_SELECTION", "LEFT_OP", "RIGHT_OP", "INC_OP",
  "DEC_OP", "LE_OP", "GE_OP", "EQ_OP", "NE_OP", "AND_OP", "OR_OP",
  "XOR_OP", "MUL_ASSIGN", "DIV_ASSIGN", "ADD_ASSIGN", "MOD_ASSIGN",
  "LEFT_ASSIGN", "RIGHT_ASSIGN", "AND_ASSIGN", "XOR_ASSIGN", "OR_ASSIGN",
  "SUB_ASSIGN", "LEFT_PAREN", "RIGHT_PAREN", "LEFT_BRACKET",
  "RIGHT_BRACKET", "LEFT_BRACE", "RIGHT_BRACE", "DOT", "COMMA", "COLON",
  "EQUAL", "SEMICOLON", "BANG", "DASH", "TILDE", "PLUS", "STAR", "SLASH",
  "PERCENT", "LEFT_ANGLE", "RIGHT_ANGLE", "VERTICAL_BAR", "CARET",
  "AMPERSAND", "QUESTION", "INVARIANT", "PRECISE", "HIGH_PRECISION",
  "MEDIUM_PRECISION", "LOW_PRECISION", "PRECISION", "PACKED", "RESOURCE",
  "SUPERP", "$accept", "variable_identifier", "primary_expression",
  "postfix_expression", "integer_expression", "function_call",
  "function_call_or_method", "function_call_generic",
  "function_call_header_no_parameters",
  "function_call_header_with_parameters", "function_call_header",
  "function_identifier", "unary_expression", "unary_operator",
  "multiplicative_expression", "additive_expression", "shift_expression",
  "relational_expression", "equality_expression", "and_expression",
  "exclusive_or_expression", "inclusive_or_expression",
  "logical_and_expression", "logical_xor_expression",
  "logical_or_expression", "conditional_expression", "$@1",
  "assignment_expression", "assignment_operator", "expression",
  "constant_expression", "declaration", "block_structure", "$@2",
  "identifier_list", "function_prototype", "function_declarator",
  "function_header_with_parameters", "function_header",
  "parameter_declarator", "parameter_declaration",
  "parameter_type_specifier", "init_declarator_list", "single_declaration",
  "fully_specified_type", "invariant_qualifier", "interpolation_qualifier",
  "layout_qualifier", "layout_qualifier_id_list", "layout_qualifier_id",
  "precise_qualifier", "type_qualifier", "single_type_qualifier",
  "storage_qualifier", "type_name_list", "type_specifier",
  "array_specifier", "type_specifier_nonarray", "precision_qualifier",
  "struct_specifier", "$@3", "$@4", "struct_declaration_list",
  "struct_declaration", "struct_declarator_list", "struct_declarator",
  "initializer", "initializer_list", "declaration_statement", "statement",
  "simple_statement", "compound_statement", "$@5", "$@6",
  "statement_no_new_scope", "statement_scoped", "$@7", "$@8",
  "compound_statement_no_new_scope", "statement_list",
  "expression_statement", "selection_statement",
  "selection_rest_statement", "condition", "switch_statement", "$@9",
  "switch_statement_list", "case_label", "iteration_statement", "$@10",
  "$@11", "$@12", "for_init_statement", "conditionopt",
  "for_rest_statement", "jump_statement", "translation_unit",
  "external_declaration", "function_definition", "$@13", YY_NULL
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,   310,   311,   312,   313,   314,
     315,   316,   317,   318,   319,   320,   321,   322,   323,   324,
     325,   326,   327,   328,   329,   330,   331,   332,   333,   334,
     335,   336,   337,   338,   339,   340,   341,   342,   343,   344,
     345,   346,   347,   348,   349,   350,   351,   352,   353,   354,
     355,   356,   357,   358,   359,   360,   361,   362,   363,   364,
     365,   366,   367,   368,   369,   370,   371,   372,   373,   374,
     375,   376,   377,   378,   379,   380,   381,   382,   383,   384,
     385,   386,   387,   388,   389,   390,   391,   392,   393,   394,
     395,   396,   397,   398,   399,   400,   401,   402,   403,   404,
     405,   406,   407,   408,   409,   410,   411,   412,   413,   414,
     415,   416,   417,   418,   419,   420,   421,   422,   423,   424,
     425,   426,   427,   428,   429,   430,   431,   432,   433,   434,
     435,   436,   437,   438,   439,   440,   441,   442,   443,   444,
     445,   446,   447,   448,   449,   450,   451,   452,   453,   454,
     455,   456,   457,   458,   459,   460,   461,   462,   463,   464,
     465,   466,   467,   468,   469,   470,   471,   472,   473,   474
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint16 yyr1[] =
{
       0,   220,   221,   222,   222,   222,   222,   222,   222,   222,
     223,   223,   223,   223,   223,   223,   224,   225,   226,   227,
     227,   228,   228,   229,   229,   230,   231,   231,   232,   232,
     232,   232,   233,   233,   233,   233,   234,   234,   234,   234,
     235,   235,   235,   236,   236,   236,   237,   237,   237,   237,
     237,   238,   238,   238,   239,   239,   240,   240,   241,   241,
     242,   242,   243,   243,   244,   244,   245,   246,   245,   247,
     247,   248,   248,   248,   248,   248,   248,   248,   248,   248,
     248,   248,   249,   249,   250,   251,   251,   251,   251,   251,
     251,   251,   251,   251,   253,   252,   254,   254,   255,   256,
     256,   257,   257,   258,   259,   259,   260,   260,   260,   260,
     261,   262,   262,   262,   262,   262,   263,   263,   263,   263,
     263,   264,   264,   265,   266,   266,   266,   267,   268,   268,
     269,   269,   269,   270,   271,   271,   272,   272,   272,   272,
     272,   272,   273,   273,   273,   273,   273,   273,   273,   273,
     273,   273,   273,   273,   273,   273,   273,   273,   273,   273,
     273,   274,   274,   275,   275,   276,   276,   276,   276,   277,
     277,   277,   277,   277,   277,   277,   277,   277,   277,   277,
     277,   277,   277,   277,   277,   277,   277,   277,   277,   277,
     277,   277,   277,   277,   277,   277,   277,   277,   277,   277,
     277,   277,   277,   277,   277,   277,   277,   277,   277,   277,
     277,   277,   277,   277,   277,   277,   277,   277,   277,   277,
     277,   277,   277,   277,   277,   277,   277,   277,   277,   277,
     277,   277,   277,   277,   277,   277,   277,   277,   277,   277,
     277,   277,   277,   277,   277,   277,   277,   277,   277,   277,
     277,   277,   277,   277,   277,   277,   277,   277,   277,   277,
     277,   277,   277,   277,   277,   277,   277,   277,   277,   277,
     277,   277,   277,   277,   277,   277,   277,   277,   277,   277,
     277,   277,   277,   277,   277,   277,   277,   277,   277,   277,
     277,   278,   278,   278,   280,   279,   281,   279,   282,   282,
     283,   283,   284,   284,   285,   285,   286,   286,   286,   287,
     287,   288,   289,   289,   290,   290,   290,   290,   290,   290,
     290,   291,   292,   293,   291,   294,   294,   296,   295,   297,
     295,   298,   298,   299,   299,   300,   300,   301,   302,   302,
     303,   303,   305,   304,   306,   306,   307,   307,   309,   308,
     310,   308,   311,   308,   312,   312,   313,   313,   314,   314,
     315,   315,   315,   315,   315,   316,   316,   317,   317,   319,
     318
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     1,     1,     1,     1,     1,     1,     1,     3,
       1,     4,     1,     3,     2,     2,     1,     1,     1,     2,
       2,     2,     1,     2,     3,     2,     1,     1,     1,     2,
       2,     2,     1,     1,     1,     1,     1,     3,     3,     3,
       1,     3,     3,     1,     3,     3,     1,     3,     3,     3,
       3,     1,     3,     3,     1,     3,     1,     3,     1,     3,
       1,     3,     1,     3,     1,     3,     1,     0,     6,     1,
       3,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     3,     1,     2,     2,     4,     2,     3,
       4,     2,     3,     4,     0,     6,     2,     3,     2,     1,
       1,     2,     3,     3,     2,     3,     2,     1,     2,     1,
       1,     1,     3,     4,     6,     5,     1,     2,     3,     5,
       4,     1,     2,     1,     1,     1,     1,     4,     1,     3,
       1,     3,     1,     1,     1,     2,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       4,     1,     3,     1,     2,     2,     3,     3,     4,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     0,     6,     0,     5,     1,     2,
       3,     4,     1,     3,     1,     2,     1,     3,     4,     1,
       3,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     2,     0,     0,     5,     1,     1,     0,     2,     0,
       2,     2,     3,     1,     2,     1,     2,     5,     3,     1,
       1,     4,     0,     8,     0,     1,     3,     2,     0,     6,
       0,     8,     0,     7,     1,     1,     1,     0,     2,     3,
       2,     2,     2,     3,     2,     1,     2,     1,     1,     0,
       3
};

/* YYDEFACT[STATE-NAME] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint16 yydefact[] =
{
       0,   143,   144,   142,   174,   170,   171,   172,   173,   159,
     181,   182,   183,   184,   185,   186,   187,   188,   189,   175,
     176,   177,   190,   191,   192,   148,   146,   147,   145,   151,
     149,   150,   152,   153,   154,   155,   156,   157,   158,   178,
     179,   180,   202,   203,   204,   126,   125,   124,     0,   193,
     194,   195,   196,   197,   198,   199,   200,   201,   205,   206,
     207,   208,   209,   210,   211,   212,   213,   214,   215,   216,
     217,   218,   219,   220,   221,   222,   223,   224,   225,   228,
     229,   230,   231,   232,   233,   235,   236,   237,   238,   239,
     240,   242,   243,   244,   245,   246,   247,   248,   226,   227,
     234,   241,   249,   250,   251,   252,   253,   254,   288,   255,
     256,   257,   258,   259,   260,   261,   262,   263,   264,   265,
     266,   267,   268,   269,   270,   271,   272,   273,   274,   275,
     276,   277,   278,   279,   280,   281,   282,   283,   284,   285,
     286,   287,     0,   169,   290,   123,   133,   291,   292,   293,
       0,   368,     0,   369,     0,   100,    99,     0,   111,   116,
     140,   139,   137,   141,     0,   134,   136,   121,   163,   138,
     289,     0,   365,   367,     0,     0,     0,   296,     0,     0,
      88,    85,     0,    98,     0,   107,   101,   109,     0,   110,
       0,    86,   117,     0,    91,   135,   122,     0,   164,     1,
     366,   161,     0,   132,   130,     0,   128,   294,     0,     0,
      89,     0,     0,   370,   102,   106,   108,   104,   112,   103,
       0,   118,    94,     0,    92,     0,     2,     6,     7,     4,
       5,     8,     0,     0,     0,   165,    34,    33,    35,    32,
       3,    10,    28,    12,    17,    18,     0,     0,    22,     0,
      36,     0,    40,    43,    46,    51,    54,    56,    58,    60,
      62,    64,    66,    84,     0,    26,     0,   160,     0,     0,
     127,     0,     0,     0,     0,     0,   298,    87,    90,     0,
       0,   350,     0,     0,     0,     0,     0,     0,     0,     0,
     322,   331,   335,    36,    69,    82,     0,   311,     0,   121,
     314,   333,   313,   312,     0,   315,   316,   317,   318,   319,
     320,   105,     0,   113,     0,   306,   120,     0,     0,    96,
       0,    93,    29,    30,     0,    14,    15,     0,     0,    20,
      19,     0,   169,    23,    25,    31,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    67,   166,   167,     0,   162,
     131,   129,     0,     0,   304,     0,   302,   297,   299,   361,
     360,     0,   352,     0,   364,   362,     0,     0,     0,   347,
     348,   321,     0,    72,    73,    75,    74,    77,    78,    79,
      80,    81,    76,    71,     0,     0,   336,   332,   334,   115,
       0,   309,     0,   119,     0,    97,     9,     0,    16,    13,
      24,    37,    38,    39,    42,    41,    44,    45,    49,    50,
      47,    48,    52,    53,    55,    57,    59,    61,    63,    65,
       0,   168,   295,     0,   305,     0,   300,     0,     0,     0,
     363,     0,   346,     0,   323,    70,    83,   114,   307,     0,
      95,    11,     0,   301,   303,     0,   355,   354,   357,   329,
     342,   340,     0,     0,     0,     0,   308,   310,     0,     0,
     356,     0,     0,   339,     0,     0,   337,     0,     0,     0,
     324,    68,     0,   358,     0,   329,   328,   330,   344,     0,
     326,   349,   325,     0,   359,   353,   338,   345,     0,   341,
     351,   343
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,   240,   241,   242,   407,   243,   244,   245,   246,   247,
     248,   249,   293,   251,   252,   253,   254,   255,   256,   257,
     258,   259,   260,   261,   262,   294,   430,   295,   394,   296,
     264,   297,   152,   318,   225,   298,   154,   155,   156,   185,
     186,   187,   157,   158,   159,   160,   161,   162,   205,   206,
     163,   164,   165,   166,   202,   265,   198,   168,   169,   170,
     272,   208,   275,   276,   365,   366,   316,   402,   300,   301,
     302,   303,   382,   465,   491,   473,   474,   475,   492,   304,
     305,   306,   476,   464,   307,   477,   498,   308,   309,   443,
     371,   438,   458,   471,   472,   310,   171,   172,   173,   182
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -449
static const yytype_int16 yypact[] =
{
    1954,  -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,  -163,
    -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,
    -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,
    -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,
    -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,  -123,  -449,
    -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,
    -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,
    -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,
    -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,
    -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,
    -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,
    -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,
    -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,
    -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,
    -449,  -449,  -130,  -449,  -449,  -449,  -449,  -449,  -449,  -449,
    -147,  -449,  -140,  -157,   -83,   -79,  3125,  -105,  -449,   -33,
    -449,  -449,  -449,  -449,  2348,  -449,  -449,  -449,   -62,  -449,
    -449,   486,  -449,  -449,   -25,   -37,   -32,  -449,  4578,  -159,
    -449,  -449,   -31,  -449,  3125,  -449,  -449,  -449,  3125,     3,
       4,  -449,  -164,   -90,  -449,  -449,  -449,  3476,   -24,  -449,
    -449,  -449,  -103,  -449,   -30,  -101,  -449,  -449,  3125,   -29,
    -449,  -150,   700,  -449,  -449,  -449,  -449,   -62,  -148,  -449,
    3661,  -137,  -449,     9,  -449,   -84,  -449,  -449,  -449,  -449,
    -449,  -449,  4214,  4214,  4214,  -449,  -449,  -449,  -449,  -449,
    -449,  -449,  -124,  -449,  -449,  -449,   -19,   -85,  4396,   -17,
    -449,  4214,   -51,  -117,  -132,  -151,  -122,   -36,   -34,   -35,
       1,     5,  -154,  -449,    -4,  -449,  3847,  -449,    17,  4214,
    -449,   -37,  3125,  3125,    32,  2543,  -449,  -449,  -449,    -6,
      -5,  -449,     7,     8,     2,  4032,    10,  4214,    -2,    13,
      12,  -449,  -449,  -106,  -449,  -449,   -71,  -449,  -157,    14,
    -449,  -449,  -449,  -449,   914,  -449,  -449,  -449,  -449,  -449,
    -449,   -24,  3661,  -108,  3661,  -449,  -449,  3661,  3125,  -449,
      40,  -449,  -449,  -449,   -82,  -449,  -449,  4214,    37,  -449,
    -449,  4214,    18,  -449,  -449,  -449,  4214,  4214,  4214,  4214,
    4214,  4214,  4214,  4214,  4214,  4214,  4214,  4214,  4214,  4214,
    4214,  4214,  4214,  4214,  4214,  -449,  -449,  -449,    15,  -449,
    -449,  -449,  2737,    32,   -62,   -61,  -449,  -449,  -449,  -449,
    -449,  1128,  -449,  4214,  -449,  -449,   -59,  4214,   -54,  -449,
    -449,  -449,  1128,  -449,  -449,  -449,  -449,  -449,  -449,  -449,
    -449,  -449,  -449,  -449,  4214,  4214,  -449,  -449,  -449,  -449,
    3661,  -449,   -98,  -449,  2931,  -449,  -449,    19,    16,  -449,
    -449,  -449,  -449,  -449,   -51,   -51,  -117,  -117,  -132,  -132,
    -132,  -132,  -151,  -151,  -122,   -36,   -34,   -35,     1,     5,
    4214,  -449,  -449,   -55,   -24,    32,  -449,    54,  1755,   -80,
    -449,   -70,  -449,  2149,  1128,  -449,  -449,  -449,  -449,  3289,
    -449,  -449,   -50,  -449,  -449,    26,  -449,  -449,  2149,    23,
    -449,    16,    57,  3125,    29,    28,  -449,  -449,  4214,  4214,
    -449,    24,    34,   209,    33,  1556,  -449,    35,    31,  1342,
    -449,  -449,   -66,  4214,  1342,    23,  -449,  -449,  1128,  3661,
    -449,  -449,  -449,    36,    16,  -449,  -449,  1128,    38,  -449,
    -449,  -449
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,  -449,
    -449,  -449,   -14,  -449,  -191,  -187,  -214,  -190,  -121,  -125,
    -119,  -116,  -118,  -115,  -449,  -183,  -449,  -215,  -449,  -227,
    -220,     6,  -449,  -449,  -449,    11,  -449,  -449,  -449,    41,
      56,    50,  -449,  -449,  -421,  -449,  -449,  -449,  -449,   -28,
    -449,  -155,  -162,  -449,  -449,     0,  -175,  -449,    91,  -449,
    -449,  -449,  -256,  -267,  -113,  -189,  -302,  -449,  -194,  -301,
    -448,  -229,  -449,  -449,  -237,  -234,  -449,  -449,    66,  -369,
    -184,  -449,  -449,  -201,  -449,  -449,  -449,  -449,  -449,  -449,
    -449,  -449,  -449,  -449,  -449,  -449,  -449,    87,  -449,  -449
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -328
static const yytype_int16 yytable[] =
{
     167,   188,   195,   398,   211,   315,   151,   324,   368,   203,
     399,   153,   401,   444,   263,   403,   362,   221,   179,   343,
     344,   354,   462,   219,   174,   197,   195,   487,   176,   188,
     197,   490,   220,   333,   341,   342,   490,   462,   210,   266,
     181,   197,   311,   313,   325,   326,   358,   278,   312,   360,
     347,   348,   266,   273,   345,   346,   355,   180,   376,   317,
     378,   177,   404,   -27,   175,   327,   147,   148,   149,   328,
     437,   383,   384,   385,   386,   387,   388,   389,   390,   391,
     392,   266,   339,   263,   340,   267,   263,   270,   400,   190,
     393,   268,   191,   271,   448,   368,   449,   315,   447,   315,
     408,   222,   315,   330,   223,   183,   406,   224,   459,   331,
     320,   195,   395,   321,   395,   184,   410,   273,   460,   497,
     273,   204,   493,   395,   395,   192,   396,   197,   395,   418,
     419,   420,   421,   435,   201,   395,   436,   368,   440,   435,
     395,   442,   453,   398,   395,   468,   439,   467,   414,   415,
     441,   336,   337,   338,   416,   417,   189,   422,   423,   207,
     212,   217,   218,   273,   196,   266,   269,   319,   277,   329,
     334,   167,   351,   349,   350,   352,   359,   151,   209,   445,
     446,   353,   153,   250,   189,   315,   356,   499,   189,   434,
     364,   369,   370,   379,   372,   373,   398,   377,   405,   374,
     380,   -26,   409,   452,   381,   431,   -21,   273,   274,   451,
     395,   455,   299,   469,  -327,   478,   461,   479,   322,   323,
     480,   483,   484,   485,   290,   425,   488,   489,   424,   215,
     501,   461,   426,   500,   315,   428,   427,   335,   216,   429,
     214,   178,   482,   361,   456,   486,   454,   495,   213,   273,
     433,   496,   250,   481,   457,   250,   494,   470,   200,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   274,   363,   315,   274,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   463,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   195,     0,   463,   299,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   274,     0,
       0,     0,   411,   412,   413,   250,   250,   250,   250,   250,
     250,   250,   250,   250,   250,   250,   250,   250,   250,   250,
     250,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   274,     0,     0,     0,     0,     0,     0,     0,
       0,   299,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   299,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   274,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   299,     0,
       0,     0,     0,   299,   299,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   299,     0,
       0,     0,     0,   196,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   299,     0,     0,     0,   299,
       0,     0,     0,     0,   299,     0,   199,     0,   299,     1,
       2,     3,     4,     5,     6,     7,     8,   299,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    67,    68,    69,    70,
      71,    72,    73,    74,    75,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    88,    89,    90,
      91,    92,    93,    94,    95,    96,    97,    98,    99,   100,
     101,   102,   103,   104,   105,   106,   107,   108,   109,   110,
     111,   112,   113,   114,   115,   116,   117,   118,   119,   120,
     121,   122,   123,   124,   125,   126,   127,   128,   129,   130,
     131,   132,   133,   134,   135,   136,   137,   138,   139,   140,
     141,   142,   143,     0,     0,   144,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   145,   146,   147,
     148,   149,   150,     1,     2,     3,     4,     5,     6,     7,
       8,   279,   280,   281,     0,   282,   283,   284,   285,   286,
     287,   288,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    62,    63,    64,    65,    66,
      67,    68,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,    79,    80,    81,    82,    83,    84,    85,    86,
      87,    88,    89,    90,    91,    92,    93,    94,    95,    96,
      97,    98,    99,   100,   101,   102,   103,   104,   105,   106,
     107,   108,   109,   110,   111,   112,   113,   114,   115,   116,
     117,   118,   119,   120,   121,   122,   123,   124,   125,   126,
     127,   128,   129,   130,   131,   132,   133,   134,   135,   136,
     137,   138,   139,   140,   141,   142,   143,   289,   226,   144,
     227,   228,   229,   230,   231,     0,     0,     0,   232,   233,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   234,     0,     0,
       0,   290,   291,     0,     0,     0,     0,   292,   236,   237,
     238,   239,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   145,   146,   147,   148,   149,   150,     1,     2,     3,
       4,     5,     6,     7,     8,   279,   280,   281,     0,   282,
     283,   284,   285,   286,   287,   288,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    76,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    90,    91,    92,
      93,    94,    95,    96,    97,    98,    99,   100,   101,   102,
     103,   104,   105,   106,   107,   108,   109,   110,   111,   112,
     113,   114,   115,   116,   117,   118,   119,   120,   121,   122,
     123,   124,   125,   126,   127,   128,   129,   130,   131,   132,
     133,   134,   135,   136,   137,   138,   139,   140,   141,   142,
     143,   289,   226,   144,   227,   228,   229,   230,   231,     0,
       0,     0,   232,   233,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   234,     0,     0,     0,   290,   397,     0,     0,     0,
       0,   292,   236,   237,   238,   239,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   145,   146,   147,   148,   149,
     150,     1,     2,     3,     4,     5,     6,     7,     8,   279,
     280,   281,     0,   282,   283,   284,   285,   286,   287,   288,
       9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,    21,    22,    23,    24,    25,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    58,
      59,    60,    61,    62,    63,    64,    65,    66,    67,    68,
      69,    70,    71,    72,    73,    74,    75,    76,    77,    78,
      79,    80,    81,    82,    83,    84,    85,    86,    87,    88,
      89,    90,    91,    92,    93,    94,    95,    96,    97,    98,
      99,   100,   101,   102,   103,   104,   105,   106,   107,   108,
     109,   110,   111,   112,   113,   114,   115,   116,   117,   118,
     119,   120,   121,   122,   123,   124,   125,   126,   127,   128,
     129,   130,   131,   132,   133,   134,   135,   136,   137,   138,
     139,   140,   141,   142,   143,   289,   226,   144,   227,   228,
     229,   230,   231,     0,     0,     0,   232,   233,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   234,     0,     0,     0,   290,
       0,     0,     0,     0,     0,   292,   236,   237,   238,   239,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   145,
     146,   147,   148,   149,   150,     1,     2,     3,     4,     5,
       6,     7,     8,   279,   280,   281,     0,   282,   283,   284,
     285,   286,   287,   288,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   134,
     135,   136,   137,   138,   139,   140,   141,   142,   143,   289,
     226,   144,   227,   228,   229,   230,   231,     0,     0,     0,
     232,   233,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   234,
       0,     0,     0,   212,     0,     0,     0,     0,     0,   292,
     236,   237,   238,   239,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   145,   146,   147,   148,   149,   150,     1,
       2,     3,     4,     5,     6,     7,     8,   279,   280,   281,
       0,   282,   283,   284,   285,   286,   287,   288,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    67,    68,    69,    70,
      71,    72,    73,    74,    75,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    88,    89,    90,
      91,    92,    93,    94,    95,    96,    97,    98,    99,   100,
     101,   102,   103,   104,   105,   106,   107,   108,   109,   110,
     111,   112,   113,   114,   115,   116,   117,   118,   119,   120,
     121,   122,   123,   124,   125,   126,   127,   128,   129,   130,
     131,   132,   133,   134,   135,   136,   137,   138,   139,   140,
     141,   142,   143,   289,   226,   144,   227,   228,   229,   230,
     231,     0,     0,     0,   232,   233,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   234,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   292,   236,   237,   238,   239,     1,     2,
       3,     4,     5,     6,     7,     8,     0,   145,   146,   147,
     148,   149,   150,     0,     0,     0,     0,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    68,    69,    70,    71,
      72,    73,    74,    75,    76,    77,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,    88,    89,    90,    91,
      92,    93,    94,    95,    96,    97,    98,    99,   100,   101,
     102,   103,   104,   105,   106,   107,   108,   109,   110,   111,
     112,   113,   114,   115,   116,   117,   118,   119,   120,   121,
     122,   123,   124,   125,   126,   127,   128,   129,   130,   131,
     132,   133,   134,   135,   136,   137,   138,   139,   140,   141,
     142,   143,     0,   226,   144,   227,   228,   229,   230,   231,
       0,     0,     0,   232,   233,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   234,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   292,   236,   237,   238,   239,     1,     2,     3,
       4,     5,     6,     7,     8,     0,   145,   146,   147,   148,
     149,   150,     0,     0,     0,     0,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    76,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    90,    91,    92,
      93,    94,    95,    96,    97,    98,    99,   100,   101,   102,
     103,   104,   105,   106,   107,   108,   109,   110,   111,   112,
     113,   114,   115,   116,   117,   118,   119,   120,   121,   122,
     123,   124,   125,   126,   127,   128,   129,   130,   131,   132,
     133,   134,   135,   136,   137,   138,   139,   140,   141,   142,
     143,     0,     0,   144,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     1,     2,     3,     4,     5,     6,     7,     8,
       0,     0,     0,     0,     0,   145,   146,   147,   148,   149,
     150,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    65,    66,    67,
      68,    69,    70,    71,    72,    73,    74,    75,    76,    77,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
      88,    89,    90,    91,    92,    93,    94,    95,    96,    97,
      98,    99,   100,   101,   102,   103,   104,   105,   106,   107,
     108,   109,   110,   111,   112,   113,   114,   115,   116,   117,
     118,   119,   120,   121,   122,   123,   124,   125,   126,   127,
     128,   129,   130,   131,   132,   133,   134,   135,   136,   137,
     138,   139,   140,   141,   142,   143,     0,   226,   144,   227,
     228,   229,   230,   231,     0,     0,     0,   232,   233,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   234,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   236,   237,   238,
     239,     1,     2,     3,     4,     5,     6,     7,     8,     0,
     145,   146,   147,   148,   149,     0,     0,     0,     0,     0,
       9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,    21,    22,    23,    24,    25,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    58,
      59,    60,    61,    62,    63,    64,    65,    66,    67,    68,
      69,    70,    71,    72,    73,    74,    75,    76,    77,    78,
      79,    80,    81,    82,    83,    84,    85,    86,    87,    88,
      89,    90,    91,    92,    93,    94,    95,    96,    97,    98,
      99,   100,   101,   102,   103,   104,   105,   106,   107,   108,
     109,   110,   111,   112,   113,   114,   115,   116,   117,   118,
     119,   120,   121,   122,   123,   124,   125,   126,   127,   128,
     129,   130,   131,   132,   133,   134,   135,   136,   137,   138,
     139,   140,   141,   142,   143,     0,   193,   144,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   194,     1,     2,     3,     4,
       5,     6,     7,     8,     0,     0,     0,     0,     0,   145,
     146,   147,   148,   149,     0,     9,    10,    11,    12,    13,
      14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    65,    66,    67,    68,    69,    70,    71,    72,    73,
      74,    75,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
      94,    95,    96,    97,    98,    99,   100,   101,   102,   103,
     104,   105,   106,   107,   108,   109,   110,   111,   112,   113,
     114,   115,   116,   117,   118,   119,   120,   121,   122,   123,
     124,   125,   126,   127,   128,   129,   130,   131,   132,   133,
     134,   135,   136,   137,   138,   139,   140,   141,   142,   143,
       0,     0,   144,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   367,     0,     0,     0,     0,
       1,     2,     3,     4,     5,     6,     7,     8,     0,     0,
       0,     0,     0,     0,   145,   146,   147,   148,   149,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    63,    64,    65,    66,    67,    68,    69,
      70,    71,    72,    73,    74,    75,    76,    77,    78,    79,
      80,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,    94,    95,    96,    97,    98,    99,
     100,   101,   102,   103,   104,   105,   106,   107,   108,   109,
     110,   111,   112,   113,   114,   115,   116,   117,   118,   119,
     120,   121,   122,   123,   124,   125,   126,   127,   128,   129,
     130,   131,   132,   133,   134,   135,   136,   137,   138,   139,
     140,   141,   142,   143,     0,     0,   144,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   432,
       0,     0,     0,     0,     1,     2,     3,     4,     5,     6,
       7,     8,     0,     0,     0,     0,     0,     0,   145,   146,
     147,   148,   149,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
      66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    88,    89,    90,    91,    92,    93,    94,    95,
      96,    97,    98,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,   109,   110,   111,   112,   113,   114,   115,
     116,   117,   118,   119,   120,   121,   122,   123,   124,   125,
     126,   127,   128,   129,   130,   131,   132,   133,   134,   135,
     136,   137,   138,   139,   140,   141,   142,   143,     0,     0,
     144,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   450,     0,     0,     0,     0,     1,     2,
       3,     4,     5,     6,     7,     8,     0,     0,     0,     0,
       0,     0,   145,   146,   147,   148,   149,     9,    10,    11,
      12,    13,    14,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    68,    69,    70,    71,
      72,    73,    74,    75,    76,    77,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,    88,    89,    90,    91,
      92,    93,    94,    95,    96,    97,    98,    99,   100,   101,
     102,   103,   104,   105,   106,   107,   108,   109,   110,   111,
     112,   113,   114,   115,   116,   117,   118,   119,   120,   121,
     122,   123,   124,   125,   126,   127,   128,   129,   130,   131,
     132,   133,   134,   135,   136,   137,   138,   139,   140,   141,
     142,   143,     0,     0,   144,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     4,     5,     6,     7,     8,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   145,   146,   147,   148,
     149,    39,    40,    41,    42,    43,    44,     0,     0,     0,
       0,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    65,    66,    67,
      68,    69,    70,    71,    72,    73,    74,    75,    76,    77,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
      88,    89,    90,    91,    92,    93,    94,    95,    96,    97,
      98,    99,   100,   101,   102,   103,   104,   105,   106,   107,
     108,   109,   110,   111,   112,   113,   114,   115,   116,   117,
     118,   119,   120,   121,   122,   123,   124,   125,   126,   127,
     128,   129,   130,   131,   132,   133,   134,   135,   136,   137,
     138,   139,   140,   141,   142,   143,     0,   226,   144,   227,
     228,   229,   230,   231,     0,     0,     0,   232,   233,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,   234,     0,     0,     0,
     314,   466,     4,     5,     6,     7,     8,   236,   237,   238,
     239,     0,     0,     0,     0,     0,     0,     0,     0,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    39,    40,
      41,    42,    43,    44,     0,     0,     0,     0,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    67,    68,    69,    70,
      71,    72,    73,    74,    75,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    88,    89,    90,
      91,    92,    93,    94,    95,    96,    97,    98,    99,   100,
     101,   102,   103,   104,   105,   106,   107,   108,   109,   110,
     111,   112,   113,   114,   115,   116,   117,   118,   119,   120,
     121,   122,   123,   124,   125,   126,   127,   128,   129,   130,
     131,   132,   133,   134,   135,   136,   137,   138,   139,   140,
     141,   142,   143,     0,   226,   144,   227,   228,   229,   230,
     231,     0,     0,     0,   232,   233,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   234,     0,     0,   235,     4,     5,     6,
       7,     8,     0,     0,   236,   237,   238,   239,     0,     0,
       0,     0,     0,     0,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    39,    40,    41,    42,    43,    44,     0,
       0,     0,     0,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
      66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    88,    89,    90,    91,    92,    93,    94,    95,
      96,    97,    98,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,   109,   110,   111,   112,   113,   114,   115,
     116,   117,   118,   119,   120,   121,   122,   123,   124,   125,
     126,   127,   128,   129,   130,   131,   132,   133,   134,   135,
     136,   137,   138,   139,   140,   141,   142,   143,     0,   226,
     144,   227,   228,   229,   230,   231,     0,     0,     0,   232,
     233,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   234,     0,
       0,     0,   314,     4,     5,     6,     7,     8,     0,   236,
     237,   238,   239,     0,     0,     0,     0,     0,     0,     0,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    39,
      40,    41,    42,    43,    44,     0,     0,     0,     0,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    63,    64,    65,    66,    67,    68,    69,
      70,    71,    72,    73,    74,    75,    76,    77,    78,    79,
      80,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,    94,    95,    96,    97,    98,    99,
     100,   101,   102,   103,   104,   105,   106,   107,   108,   109,
     110,   111,   112,   113,   114,   115,   116,   117,   118,   119,
     120,   121,   122,   123,   124,   125,   126,   127,   128,   129,
     130,   131,   132,   133,   134,   135,   136,   137,   138,   139,
     140,   141,   142,   143,     0,   226,   144,   227,   228,   229,
     230,   231,     0,     0,     0,   232,   233,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   234,     0,     0,   357,     4,     5,
       6,     7,     8,     0,     0,   236,   237,   238,   239,     0,
       0,     0,     0,     0,     0,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    39,    40,    41,    42,    43,    44,
       0,     0,     0,     0,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   134,
     135,   136,   137,   138,   139,   140,   141,   142,   143,     0,
     226,   144,   227,   228,   229,   230,   231,     0,     0,     0,
     232,   233,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   234,
       4,     5,     6,     7,     8,     0,     0,     0,     0,   375,
     236,   237,   238,   239,     0,     0,     0,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    39,    40,    41,    42,
      43,    44,     0,     0,     0,     0,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    76,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    90,    91,    92,
      93,    94,    95,    96,    97,    98,    99,   100,   101,   102,
     103,   104,   105,   106,   107,   108,   109,   110,   111,   112,
     113,   114,   115,   116,   117,   118,   119,   120,   121,   122,
     123,   124,   125,   126,   127,   128,   129,   130,   131,   132,
     133,   134,   135,   136,   137,   138,   139,   140,   141,   142,
     143,     0,   226,   144,   227,   228,   229,   230,   231,     0,
       0,     0,   232,   233,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   234,     4,     5,     6,     7,     8,     0,     0,     0,
       0,     0,   236,   237,   238,   239,     0,     0,     0,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    39,    40,
      41,    42,    43,    44,     0,     0,     0,     0,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    67,    68,    69,    70,
      71,    72,    73,    74,    75,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    88,    89,    90,
      91,    92,    93,    94,    95,    96,    97,    98,    99,   100,
     101,   102,   103,   104,   105,   106,   107,   108,   109,   110,
     111,   112,   113,   114,   115,   116,   117,   118,   119,   120,
     121,   122,   123,   124,   125,   126,   127,   128,   129,   130,
     131,   132,   133,   134,   135,   136,   137,   138,   139,   140,
     141,   142,   332,     0,   226,   144,   227,   228,   229,   230,
     231,     0,     0,     0,   232,   233,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,   234,     4,     5,     6,     7,     8,     0,
       0,     0,     0,     0,   236,   237,   238,   239,     0,     0,
       0,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,    21,    22,    23,    24,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      39,    40,    41,    42,    43,    44,     0,     0,     0,     0,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    58,
      59,    60,    61,    62,    63,    64,    65,    66,    67,    68,
      69,    70,    71,    72,    73,    74,    75,    76,    77,    78,
      79,    80,    81,    82,    83,    84,    85,    86,    87,    88,
      89,    90,    91,    92,    93,    94,    95,    96,    97,    98,
      99,   100,   101,   102,   103,   104,   105,   106,   107,   108,
     109,   110,   111,   112,   113,   114,   115,   116,   117,   118,
     119,   120,   121,   122,   123,   124,   125,   126,   127,   128,
     129,   130,   131,   132,   133,   134,   135,   136,   137,   138,
     139,   140,   141,   142,   143,     0,     0,   144
};

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-449)))

#define yytable_value_is_error(Yytable_value) \
  YYID (0)

static const yytype_int16 yycheck[] =
{
       0,   156,   164,   304,   179,   220,     0,   234,   275,    46,
     312,     0,   314,   382,   197,   317,   272,   192,   158,   170,
     171,   175,   443,   187,   187,   189,   188,   475,   158,   184,
     189,   479,   196,   248,   166,   167,   484,   458,   197,   189,
     197,   189,   217,   218,   168,   169,   266,   197,   196,   269,
     172,   173,   189,   208,   205,   206,   210,   197,   285,   196,
     287,   191,   318,   187,   187,   189,   213,   214,   215,   193,
     371,   177,   178,   179,   180,   181,   182,   183,   184,   185,
     186,   189,   199,   266,   201,   188,   269,   188,   196,   194,
     196,   194,   197,   194,   192,   362,   194,   312,   400,   314,
     327,   191,   317,   188,   194,   188,   188,   197,   188,   194,
     194,   273,   194,   197,   194,   194,   331,   272,   188,   488,
     275,   158,   188,   194,   194,   158,   197,   189,   194,   343,
     344,   345,   346,   194,   159,   194,   197,   404,   197,   194,
     194,   195,   197,   444,   194,   195,   373,   449,   339,   340,
     377,   202,   203,   204,   341,   342,   156,   347,   348,   191,
     191,   158,   158,   318,   164,   189,   196,   158,   197,   188,
     187,   171,   207,   209,   208,   174,   159,   171,   178,   394,
     395,   176,   171,   197,   184,   400,   190,   489,   188,   364,
     158,   197,   197,   195,   187,   187,   497,   187,   158,   197,
     187,   187,   165,   430,   192,   190,   188,   362,   208,   190,
     194,   157,   212,   187,   191,   158,   443,   188,   232,   233,
     192,   197,   188,    14,   191,   350,   191,   196,   349,   188,
     192,   458,   351,   197,   449,   353,   352,   251,   188,   354,
     184,   150,   469,   271,   438,   474,   435,   484,   182,   404,
     363,   485,   266,   468,   438,   269,   483,   458,   171,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   272,   273,   489,   275,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   443,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   463,    -1,   458,   304,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   318,    -1,
      -1,    -1,   336,   337,   338,   339,   340,   341,   342,   343,
     344,   345,   346,   347,   348,   349,   350,   351,   352,   353,
     354,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   362,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   371,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   382,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   404,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   438,    -1,
      -1,    -1,    -1,   443,   444,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   458,    -1,
      -1,    -1,    -1,   463,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   475,    -1,    -1,    -1,   479,
      -1,    -1,    -1,    -1,   484,    -1,     0,    -1,   488,     3,
       4,     5,     6,     7,     8,     9,    10,   497,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    65,    66,    67,    68,    69,    70,    71,    72,    73,
      74,    75,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
      94,    95,    96,    97,    98,    99,   100,   101,   102,   103,
     104,   105,   106,   107,   108,   109,   110,   111,   112,   113,
     114,   115,   116,   117,   118,   119,   120,   121,   122,   123,
     124,   125,   126,   127,   128,   129,   130,   131,   132,   133,
     134,   135,   136,   137,   138,   139,   140,   141,   142,   143,
     144,   145,   146,   147,   148,   149,   150,   151,   152,   153,
     154,   155,   156,    -1,    -1,   159,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   211,   212,   213,
     214,   215,   216,     3,     4,     5,     6,     7,     8,     9,
      10,    11,    12,    13,    -1,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    63,    64,    65,    66,    67,    68,    69,
      70,    71,    72,    73,    74,    75,    76,    77,    78,    79,
      80,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,    94,    95,    96,    97,    98,    99,
     100,   101,   102,   103,   104,   105,   106,   107,   108,   109,
     110,   111,   112,   113,   114,   115,   116,   117,   118,   119,
     120,   121,   122,   123,   124,   125,   126,   127,   128,   129,
     130,   131,   132,   133,   134,   135,   136,   137,   138,   139,
     140,   141,   142,   143,   144,   145,   146,   147,   148,   149,
     150,   151,   152,   153,   154,   155,   156,   157,   158,   159,
     160,   161,   162,   163,   164,    -1,    -1,    -1,   168,   169,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   187,    -1,    -1,
      -1,   191,   192,    -1,    -1,    -1,    -1,   197,   198,   199,
     200,   201,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   211,   212,   213,   214,   215,   216,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    12,    13,    -1,    15,
      16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
      66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    88,    89,    90,    91,    92,    93,    94,    95,
      96,    97,    98,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,   109,   110,   111,   112,   113,   114,   115,
     116,   117,   118,   119,   120,   121,   122,   123,   124,   125,
     126,   127,   128,   129,   130,   131,   132,   133,   134,   135,
     136,   137,   138,   139,   140,   141,   142,   143,   144,   145,
     146,   147,   148,   149,   150,   151,   152,   153,   154,   155,
     156,   157,   158,   159,   160,   161,   162,   163,   164,    -1,
      -1,    -1,   168,   169,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   187,    -1,    -1,    -1,   191,   192,    -1,    -1,    -1,
      -1,   197,   198,   199,   200,   201,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   211,   212,   213,   214,   215,
     216,     3,     4,     5,     6,     7,     8,     9,    10,    11,
      12,    13,    -1,    15,    16,    17,    18,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    68,    69,    70,    71,
      72,    73,    74,    75,    76,    77,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,    88,    89,    90,    91,
      92,    93,    94,    95,    96,    97,    98,    99,   100,   101,
     102,   103,   104,   105,   106,   107,   108,   109,   110,   111,
     112,   113,   114,   115,   116,   117,   118,   119,   120,   121,
     122,   123,   124,   125,   126,   127,   128,   129,   130,   131,
     132,   133,   134,   135,   136,   137,   138,   139,   140,   141,
     142,   143,   144,   145,   146,   147,   148,   149,   150,   151,
     152,   153,   154,   155,   156,   157,   158,   159,   160,   161,
     162,   163,   164,    -1,    -1,    -1,   168,   169,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   187,    -1,    -1,    -1,   191,
      -1,    -1,    -1,    -1,    -1,   197,   198,   199,   200,   201,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   211,
     212,   213,   214,   215,   216,     3,     4,     5,     6,     7,
       8,     9,    10,    11,    12,    13,    -1,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,    53,    54,    55,    56,    57,
      58,    59,    60,    61,    62,    63,    64,    65,    66,    67,
      68,    69,    70,    71,    72,    73,    74,    75,    76,    77,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
      88,    89,    90,    91,    92,    93,    94,    95,    96,    97,
      98,    99,   100,   101,   102,   103,   104,   105,   106,   107,
     108,   109,   110,   111,   112,   113,   114,   115,   116,   117,
     118,   119,   120,   121,   122,   123,   124,   125,   126,   127,
     128,   129,   130,   131,   132,   133,   134,   135,   136,   137,
     138,   139,   140,   141,   142,   143,   144,   145,   146,   147,
     148,   149,   150,   151,   152,   153,   154,   155,   156,   157,
     158,   159,   160,   161,   162,   163,   164,    -1,    -1,    -1,
     168,   169,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   187,
      -1,    -1,    -1,   191,    -1,    -1,    -1,    -1,    -1,   197,
     198,   199,   200,   201,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   211,   212,   213,   214,   215,   216,     3,
       4,     5,     6,     7,     8,     9,    10,    11,    12,    13,
      -1,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    65,    66,    67,    68,    69,    70,    71,    72,    73,
      74,    75,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
      94,    95,    96,    97,    98,    99,   100,   101,   102,   103,
     104,   105,   106,   107,   108,   109,   110,   111,   112,   113,
     114,   115,   116,   117,   118,   119,   120,   121,   122,   123,
     124,   125,   126,   127,   128,   129,   130,   131,   132,   133,
     134,   135,   136,   137,   138,   139,   140,   141,   142,   143,
     144,   145,   146,   147,   148,   149,   150,   151,   152,   153,
     154,   155,   156,   157,   158,   159,   160,   161,   162,   163,
     164,    -1,    -1,    -1,   168,   169,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   187,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   197,   198,   199,   200,   201,     3,     4,
       5,     6,     7,     8,     9,    10,    -1,   211,   212,   213,
     214,   215,   216,    -1,    -1,    -1,    -1,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   134,
     135,   136,   137,   138,   139,   140,   141,   142,   143,   144,
     145,   146,   147,   148,   149,   150,   151,   152,   153,   154,
     155,   156,    -1,   158,   159,   160,   161,   162,   163,   164,
      -1,    -1,    -1,   168,   169,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   187,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   197,   198,   199,   200,   201,     3,     4,     5,
       6,     7,     8,     9,    10,    -1,   211,   212,   213,   214,
     215,   216,    -1,    -1,    -1,    -1,    22,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    51,    52,    53,    54,    55,
      56,    57,    58,    59,    60,    61,    62,    63,    64,    65,
      66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    88,    89,    90,    91,    92,    93,    94,    95,
      96,    97,    98,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,   109,   110,   111,   112,   113,   114,   115,
     116,   117,   118,   119,   120,   121,   122,   123,   124,   125,
     126,   127,   128,   129,   130,   131,   132,   133,   134,   135,
     136,   137,   138,   139,   140,   141,   142,   143,   144,   145,
     146,   147,   148,   149,   150,   151,   152,   153,   154,   155,
     156,    -1,    -1,   159,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,     3,     4,     5,     6,     7,     8,     9,    10,
      -1,    -1,    -1,    -1,    -1,   211,   212,   213,   214,   215,
     216,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45,    46,    47,    48,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    67,    68,    69,    70,
      71,    72,    73,    74,    75,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    88,    89,    90,
      91,    92,    93,    94,    95,    96,    97,    98,    99,   100,
     101,   102,   103,   104,   105,   106,   107,   108,   109,   110,
     111,   112,   113,   114,   115,   116,   117,   118,   119,   120,
     121,   122,   123,   124,   125,   126,   127,   128,   129,   130,
     131,   132,   133,   134,   135,   136,   137,   138,   139,   140,
     141,   142,   143,   144,   145,   146,   147,   148,   149,   150,
     151,   152,   153,   154,   155,   156,    -1,   158,   159,   160,
     161,   162,   163,   164,    -1,    -1,    -1,   168,   169,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   187,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   198,   199,   200,
     201,     3,     4,     5,     6,     7,     8,     9,    10,    -1,
     211,   212,   213,   214,   215,    -1,    -1,    -1,    -1,    -1,
      22,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    51,
      52,    53,    54,    55,    56,    57,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    68,    69,    70,    71,
      72,    73,    74,    75,    76,    77,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,    88,    89,    90,    91,
      92,    93,    94,    95,    96,    97,    98,    99,   100,   101,
     102,   103,   104,   105,   106,   107,   108,   109,   110,   111,
     112,   113,   114,   115,   116,   117,   118,   119,   120,   121,
     122,   123,   124,   125,   126,   127,   128,   129,   130,   131,
     132,   133,   134,   135,   136,   137,   138,   139,   140,   141,
     142,   143,   144,   145,   146,   147,   148,   149,   150,   151,
     152,   153,   154,   155,   156,    -1,   158,   159,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   197,     3,     4,     5,     6,
       7,     8,     9,    10,    -1,    -1,    -1,    -1,    -1,   211,
     212,   213,   214,   215,    -1,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    62,    63,    64,    65,    66,
      67,    68,    69,    70,    71,    72,    73,    74,    75,    76,
      77,    78,    79,    80,    81,    82,    83,    84,    85,    86,
      87,    88,    89,    90,    91,    92,    93,    94,    95,    96,
      97,    98,    99,   100,   101,   102,   103,   104,   105,   106,
     107,   108,   109,   110,   111,   112,   113,   114,   115,   116,
     117,   118,   119,   120,   121,   122,   123,   124,   125,   126,
     127,   128,   129,   130,   131,   132,   133,   134,   135,   136,
     137,   138,   139,   140,   141,   142,   143,   144,   145,   146,
     147,   148,   149,   150,   151,   152,   153,   154,   155,   156,
      -1,    -1,   159,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   192,    -1,    -1,    -1,    -1,
       3,     4,     5,     6,     7,     8,     9,    10,    -1,    -1,
      -1,    -1,    -1,    -1,   211,   212,   213,   214,   215,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    76,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    90,    91,    92,
      93,    94,    95,    96,    97,    98,    99,   100,   101,   102,
     103,   104,   105,   106,   107,   108,   109,   110,   111,   112,
     113,   114,   115,   116,   117,   118,   119,   120,   121,   122,
     123,   124,   125,   126,   127,   128,   129,   130,   131,   132,
     133,   134,   135,   136,   137,   138,   139,   140,   141,   142,
     143,   144,   145,   146,   147,   148,   149,   150,   151,   152,
     153,   154,   155,   156,    -1,    -1,   159,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   192,
      -1,    -1,    -1,    -1,     3,     4,     5,     6,     7,     8,
       9,    10,    -1,    -1,    -1,    -1,    -1,    -1,   211,   212,
     213,   214,   215,    22,    23,    24,    25,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    48,
      49,    50,    51,    52,    53,    54,    55,    56,    57,    58,
      59,    60,    61,    62,    63,    64,    65,    66,    67,    68,
      69,    70,    71,    72,    73,    74,    75,    76,    77,    78,
      79,    80,    81,    82,    83,    84,    85,    86,    87,    88,
      89,    90,    91,    92,    93,    94,    95,    96,    97,    98,
      99,   100,   101,   102,   103,   104,   105,   106,   107,   108,
     109,   110,   111,   112,   113,   114,   115,   116,   117,   118,
     119,   120,   121,   122,   123,   124,   125,   126,   127,   128,
     129,   130,   131,   132,   133,   134,   135,   136,   137,   138,
     139,   140,   141,   142,   143,   144,   145,   146,   147,   148,
     149,   150,   151,   152,   153,   154,   155,   156,    -1,    -1,
     159,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   192,    -1,    -1,    -1,    -1,     3,     4,
       5,     6,     7,     8,     9,    10,    -1,    -1,    -1,    -1,
      -1,    -1,   211,   212,   213,   214,   215,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   134,
     135,   136,   137,   138,   139,   140,   141,   142,   143,   144,
     145,   146,   147,   148,   149,   150,   151,   152,   153,   154,
     155,   156,    -1,    -1,   159,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,     6,     7,     8,     9,    10,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   211,   212,   213,   214,
     215,    52,    53,    54,    55,    56,    57,    -1,    -1,    -1,
      -1,    62,    63,    64,    65,    66,    67,    68,    69,    70,
      71,    72,    73,    74,    75,    76,    77,    78,    79,    80,
      81,    82,    83,    84,    85,    86,    87,    88,    89,    90,
      91,    92,    93,    94,    95,    96,    97,    98,    99,   100,
     101,   102,   103,   104,   105,   106,   107,   108,   109,   110,
     111,   112,   113,   114,   115,   116,   117,   118,   119,   120,
     121,   122,   123,   124,   125,   126,   127,   128,   129,   130,
     131,   132,   133,   134,   135,   136,   137,   138,   139,   140,
     141,   142,   143,   144,   145,   146,   147,   148,   149,   150,
     151,   152,   153,   154,   155,   156,    -1,   158,   159,   160,
     161,   162,   163,   164,    -1,    -1,    -1,   168,   169,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   187,    -1,    -1,    -1,
     191,   192,     6,     7,     8,     9,    10,   198,   199,   200,
     201,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    52,    53,
      54,    55,    56,    57,    -1,    -1,    -1,    -1,    62,    63,
      64,    65,    66,    67,    68,    69,    70,    71,    72,    73,
      74,    75,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
      94,    95,    96,    97,    98,    99,   100,   101,   102,   103,
     104,   105,   106,   107,   108,   109,   110,   111,   112,   113,
     114,   115,   116,   117,   118,   119,   120,   121,   122,   123,
     124,   125,   126,   127,   128,   129,   130,   131,   132,   133,
     134,   135,   136,   137,   138,   139,   140,   141,   142,   143,
     144,   145,   146,   147,   148,   149,   150,   151,   152,   153,
     154,   155,   156,    -1,   158,   159,   160,   161,   162,   163,
     164,    -1,    -1,    -1,   168,   169,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   187,    -1,    -1,   190,     6,     7,     8,
       9,    10,    -1,    -1,   198,   199,   200,   201,    -1,    -1,
      -1,    -1,    -1,    -1,    23,    24,    25,    26,    27,    28,
      29,    30,    31,    32,    33,    34,    35,    36,    37,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    52,    53,    54,    55,    56,    57,    -1,
      -1,    -1,    -1,    62,    63,    64,    65,    66,    67,    68,
      69,    70,    71,    72,    73,    74,    75,    76,    77,    78,
      79,    80,    81,    82,    83,    84,    85,    86,    87,    88,
      89,    90,    91,    92,    93,    94,    95,    96,    97,    98,
      99,   100,   101,   102,   103,   104,   105,   106,   107,   108,
     109,   110,   111,   112,   113,   114,   115,   116,   117,   118,
     119,   120,   121,   122,   123,   124,   125,   126,   127,   128,
     129,   130,   131,   132,   133,   134,   135,   136,   137,   138,
     139,   140,   141,   142,   143,   144,   145,   146,   147,   148,
     149,   150,   151,   152,   153,   154,   155,   156,    -1,   158,
     159,   160,   161,   162,   163,   164,    -1,    -1,    -1,   168,
     169,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   187,    -1,
      -1,    -1,   191,     6,     7,     8,     9,    10,    -1,   198,
     199,   200,   201,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    52,
      53,    54,    55,    56,    57,    -1,    -1,    -1,    -1,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    76,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    90,    91,    92,
      93,    94,    95,    96,    97,    98,    99,   100,   101,   102,
     103,   104,   105,   106,   107,   108,   109,   110,   111,   112,
     113,   114,   115,   116,   117,   118,   119,   120,   121,   122,
     123,   124,   125,   126,   127,   128,   129,   130,   131,   132,
     133,   134,   135,   136,   137,   138,   139,   140,   141,   142,
     143,   144,   145,   146,   147,   148,   149,   150,   151,   152,
     153,   154,   155,   156,    -1,   158,   159,   160,   161,   162,
     163,   164,    -1,    -1,    -1,   168,   169,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,   187,    -1,    -1,   190,     6,     7,
       8,     9,    10,    -1,    -1,   198,   199,   200,   201,    -1,
      -1,    -1,    -1,    -1,    -1,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    52,    53,    54,    55,    56,    57,
      -1,    -1,    -1,    -1,    62,    63,    64,    65,    66,    67,
      68,    69,    70,    71,    72,    73,    74,    75,    76,    77,
      78,    79,    80,    81,    82,    83,    84,    85,    86,    87,
      88,    89,    90,    91,    92,    93,    94,    95,    96,    97,
      98,    99,   100,   101,   102,   103,   104,   105,   106,   107,
     108,   109,   110,   111,   112,   113,   114,   115,   116,   117,
     118,   119,   120,   121,   122,   123,   124,   125,   126,   127,
     128,   129,   130,   131,   132,   133,   134,   135,   136,   137,
     138,   139,   140,   141,   142,   143,   144,   145,   146,   147,
     148,   149,   150,   151,   152,   153,   154,   155,   156,    -1,
     158,   159,   160,   161,   162,   163,   164,    -1,    -1,    -1,
     168,   169,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   187,
       6,     7,     8,     9,    10,    -1,    -1,    -1,    -1,   197,
     198,   199,   200,   201,    -1,    -1,    -1,    23,    24,    25,
      26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
      36,    37,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    52,    53,    54,    55,
      56,    57,    -1,    -1,    -1,    -1,    62,    63,    64,    65,
      66,    67,    68,    69,    70,    71,    72,    73,    74,    75,
      76,    77,    78,    79,    80,    81,    82,    83,    84,    85,
      86,    87,    88,    89,    90,    91,    92,    93,    94,    95,
      96,    97,    98,    99,   100,   101,   102,   103,   104,   105,
     106,   107,   108,   109,   110,   111,   112,   113,   114,   115,
     116,   117,   118,   119,   120,   121,   122,   123,   124,   125,
     126,   127,   128,   129,   130,   131,   132,   133,   134,   135,
     136,   137,   138,   139,   140,   141,   142,   143,   144,   145,
     146,   147,   148,   149,   150,   151,   152,   153,   154,   155,
     156,    -1,   158,   159,   160,   161,   162,   163,   164,    -1,
      -1,    -1,   168,   169,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   187,     6,     7,     8,     9,    10,    -1,    -1,    -1,
      -1,    -1,   198,   199,   200,   201,    -1,    -1,    -1,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    52,    53,
      54,    55,    56,    57,    -1,    -1,    -1,    -1,    62,    63,
      64,    65,    66,    67,    68,    69,    70,    71,    72,    73,
      74,    75,    76,    77,    78,    79,    80,    81,    82,    83,
      84,    85,    86,    87,    88,    89,    90,    91,    92,    93,
      94,    95,    96,    97,    98,    99,   100,   101,   102,   103,
     104,   105,   106,   107,   108,   109,   110,   111,   112,   113,
     114,   115,   116,   117,   118,   119,   120,   121,   122,   123,
     124,   125,   126,   127,   128,   129,   130,   131,   132,   133,
     134,   135,   136,   137,   138,   139,   140,   141,   142,   143,
     144,   145,   146,   147,   148,   149,   150,   151,   152,   153,
     154,   155,   156,    -1,   158,   159,   160,   161,   162,   163,
     164,    -1,    -1,    -1,   168,   169,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,   187,     6,     7,     8,     9,    10,    -1,
      -1,    -1,    -1,    -1,   198,   199,   200,   201,    -1,    -1,
      -1,    23,    24,    25,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      52,    53,    54,    55,    56,    57,    -1,    -1,    -1,    -1,
      62,    63,    64,    65,    66,    67,    68,    69,    70,    71,
      72,    73,    74,    75,    76,    77,    78,    79,    80,    81,
      82,    83,    84,    85,    86,    87,    88,    89,    90,    91,
      92,    93,    94,    95,    96,    97,    98,    99,   100,   101,
     102,   103,   104,   105,   106,   107,   108,   109,   110,   111,
     112,   113,   114,   115,   116,   117,   118,   119,   120,   121,
     122,   123,   124,   125,   126,   127,   128,   129,   130,   131,
     132,   133,   134,   135,   136,   137,   138,   139,   140,   141,
     142,   143,   144,   145,   146,   147,   148,   149,   150,   151,
     152,   153,   154,   155,   156,    -1,    -1,   159
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint16 yystos[] =
{
       0,     3,     4,     5,     6,     7,     8,     9,    10,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51,    52,
      53,    54,    55,    56,    57,    58,    59,    60,    61,    62,
      63,    64,    65,    66,    67,    68,    69,    70,    71,    72,
      73,    74,    75,    76,    77,    78,    79,    80,    81,    82,
      83,    84,    85,    86,    87,    88,    89,    90,    91,    92,
      93,    94,    95,    96,    97,    98,    99,   100,   101,   102,
     103,   104,   105,   106,   107,   108,   109,   110,   111,   112,
     113,   114,   115,   116,   117,   118,   119,   120,   121,   122,
     123,   124,   125,   126,   127,   128,   129,   130,   131,   132,
     133,   134,   135,   136,   137,   138,   139,   140,   141,   142,
     143,   144,   145,   146,   147,   148,   149,   150,   151,   152,
     153,   154,   155,   156,   159,   211,   212,   213,   214,   215,
     216,   251,   252,   255,   256,   257,   258,   262,   263,   264,
     265,   266,   267,   270,   271,   272,   273,   275,   277,   278,
     279,   316,   317,   318,   187,   187,   158,   191,   278,   158,
     197,   197,   319,   188,   194,   259,   260,   261,   271,   275,
     194,   197,   158,   158,   197,   272,   275,   189,   276,     0,
     317,   159,   274,    46,   158,   268,   269,   191,   281,   275,
     197,   276,   191,   298,   260,   259,   261,   158,   158,   187,
     196,   276,   191,   194,   197,   254,   158,   160,   161,   162,
     163,   164,   168,   169,   187,   190,   198,   199,   200,   201,
     221,   222,   223,   225,   226,   227,   228,   229,   230,   231,
     232,   233,   234,   235,   236,   237,   238,   239,   240,   241,
     242,   243,   244,   245,   250,   275,   189,   188,   194,   196,
     188,   194,   280,   271,   275,   282,   283,   197,   197,    11,
      12,    13,    15,    16,    17,    18,    19,    20,    21,   157,
     191,   192,   197,   232,   245,   247,   249,   251,   255,   275,
     288,   289,   290,   291,   299,   300,   301,   304,   307,   308,
     315,   276,   196,   276,   191,   247,   286,   196,   253,   158,
     194,   197,   232,   232,   249,   168,   169,   189,   193,   188,
     188,   194,   156,   247,   187,   232,   202,   203,   204,   199,
     201,   166,   167,   170,   171,   205,   206,   172,   173,   209,
     208,   207,   174,   176,   175,   210,   190,   190,   250,   159,
     250,   269,   282,   275,   158,   284,   285,   192,   283,   197,
     197,   310,   187,   187,   197,   197,   249,   187,   249,   195,
     187,   192,   292,   177,   178,   179,   180,   181,   182,   183,
     184,   185,   186,   196,   248,   194,   197,   192,   289,   286,
     196,   286,   287,   286,   282,   158,   188,   224,   249,   165,
     247,   232,   232,   232,   234,   234,   235,   235,   236,   236,
     236,   236,   237,   237,   238,   239,   240,   241,   242,   243,
     246,   190,   192,   284,   276,   194,   197,   289,   311,   249,
     197,   249,   195,   309,   299,   247,   247,   286,   192,   194,
     192,   190,   249,   197,   285,   157,   288,   300,   312,   188,
     188,   249,   264,   271,   303,   293,   192,   286,   195,   187,
     303,   313,   314,   295,   296,   297,   302,   305,   158,   188,
     192,   247,   249,   197,   188,    14,   291,   290,   191,   196,
     290,   294,   298,   188,   249,   294,   295,   299,   306,   286,
     197,   192
};

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  However,
   YYFAIL appears to be in use.  Nevertheless, it is formally deprecated
   in Bison 2.4.2's NEWS entry, where a plan to phase it out is
   discussed.  */

#define YYFAIL		goto yyerrlab
#if defined YYFAIL
  /* This is here to suppress warnings from the GCC cpp's
     -Wunused-macros.  Normally we don't worry about that warning, but
     some users do, and we want to make it easy for users to remove
     YYFAIL uses, which will produce warnings from Bison 2.5.  */
#endif

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                  \
do                                                              \
  if (yychar == YYEMPTY)                                        \
    {                                                           \
      yychar = (Token);                                         \
      yylval = (Value);                                         \
      YYPOPSTACK (yylen);                                       \
      yystate = *yyssp;                                         \
      goto yybackup;                                            \
    }                                                           \
  else                                                          \
    {                                                           \
      yyerror (pParseContext, YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))

/* Error token number */
#define YYTERROR	1
#define YYERRCODE	256


/* This macro is provided for backward compatibility. */
#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */
#ifdef YYLEX_PARAM
# define YYLEX yylex (&yylval, YYLEX_PARAM)
#else
# define YYLEX yylex (&yylval, parseContext)
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value, pParseContext); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, glslang::TParseContext* pParseContext)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep, pParseContext)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    glslang::TParseContext* pParseContext;
#endif
{
  FILE *yyo = yyoutput;
  YYUSE (yyo);
  if (!yyvaluep)
    return;
  YYUSE (pParseContext);
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
        break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, glslang::TParseContext* pParseContext)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep, pParseContext)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    glslang::TParseContext* pParseContext;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep, pParseContext);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
#else
static void
yy_stack_print (yybottom, yytop)
    yytype_int16 *yybottom;
    yytype_int16 *yytop;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, int yyrule, glslang::TParseContext* pParseContext)
#else
static void
yy_reduce_print (yyvsp, yyrule, pParseContext)
    YYSTYPE *yyvsp;
    int yyrule;
    glslang::TParseContext* pParseContext;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       		       , pParseContext);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, Rule, pParseContext); \
} while (YYID (0))

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
	switch (*++yyp)
	  {
	  case '\'':
	  case ',':
	    goto do_not_strip_quotes;

	  case '\\':
	    if (*++yyp != '\\')
	      goto do_not_strip_quotes;
	    /* Fall through.  */
	  default:
	    if (yyres)
	      yyres[yyn] = *yyp;
	    yyn++;
	    break;

	  case '"':
	    if (yyres)
	      yyres[yyn] = '\0';
	    return yyn;
	  }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (YY_NULL, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULL;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - Assume YYFAIL is not used.  It's too flawed to consider.  See
       <http://lists.gnu.org/archive/html/bison-patches/2009-12/msg00024.html>
       for details.  YYERROR is fine as it does not invoke this
       function.
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[*yyssp];
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                {
                  YYSIZE_T yysize1 = yysize + yytnamerr (YY_NULL, yytname[yyx]);
                  if (! (yysize <= yysize1
                         && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                    return 2;
                  yysize = yysize1;
                }
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  {
    YYSIZE_T yysize1 = yysize + yystrlen (yyformat);
    if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
      return 2;
    yysize = yysize1;
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          yyp++;
          yyformat++;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, glslang::TParseContext* pParseContext)
#else
static void
yydestruct (yymsg, yytype, yyvaluep, pParseContext)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
    glslang::TParseContext* pParseContext;
#endif
{
  YYUSE (yyvaluep);
  YYUSE (pParseContext);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
        break;
    }
}




/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (glslang::TParseContext* pParseContext)
#else
int
yyparse (pParseContext)
    glslang::TParseContext* pParseContext;
#endif
#endif
{
/* The lookahead symbol.  */
int yychar;


#if defined __GNUC__ && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN \
    _Pragma ("GCC diagnostic push") \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")\
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END \
    _Pragma ("GCC diagnostic pop")
#else
/* Default value used for initialization, for pacifying older GCCs
   or non-GCC compilers.  */
static YYSTYPE yyval_default;
# define YY_INITIAL_VALUE(Value) = Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval YY_INITIAL_VALUE(yyval_default);

    /* Number of syntax errors so far.  */
    int yynerrs;

    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       `yyss': related to states.
       `yyvs': related to semantic values.

       Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yyssp = yyss = yyssa;
  yyvsp = yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */
  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack.  Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	yytype_int16 *yyss1 = yyss;

	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow (YY_("memory exhausted"),
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),
		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	yytype_int16 *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyexhaustedlab;
	YYSTACK_RELOCATE (yyss_alloc, yyss);
	YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 2:
/* Line 1792 of yacc.c  */
#line 223 "glslang.y"
    {
        (yyval.interm.intermTypedNode) = parseContext.handleVariable((yyvsp[(1) - (1)].lex).loc, (yyvsp[(1) - (1)].lex).symbol, (yyvsp[(1) - (1)].lex).string);
    }
    break;

  case 3:
/* Line 1792 of yacc.c  */
#line 229 "glslang.y"
    {
        (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode);
    }
    break;

  case 4:
/* Line 1792 of yacc.c  */
#line 232 "glslang.y"
    {
        (yyval.interm.intermTypedNode) = parseContext.intermediate.addConstantUnion((yyvsp[(1) - (1)].lex).i, (yyvsp[(1) - (1)].lex).loc, true);
    }
    break;

  case 5:
/* Line 1792 of yacc.c  */
#line 235 "glslang.y"
    {
        parseContext.fullIntegerCheck((yyvsp[(1) - (1)].lex).loc, "unsigned literal");
        (yyval.interm.intermTypedNode) = parseContext.intermediate.addConstantUnion((yyvsp[(1) - (1)].lex).u, (yyvsp[(1) - (1)].lex).loc, true);
    }
    break;

  case 6:
/* Line 1792 of yacc.c  */
#line 239 "glslang.y"
    {
        (yyval.interm.intermTypedNode) = parseContext.intermediate.addConstantUnion((yyvsp[(1) - (1)].lex).d, EbtFloat, (yyvsp[(1) - (1)].lex).loc, true);
    }
    break;

  case 7:
/* Line 1792 of yacc.c  */
#line 242 "glslang.y"
    {
        parseContext.doubleCheck((yyvsp[(1) - (1)].lex).loc, "double literal");
        (yyval.interm.intermTypedNode) = parseContext.intermediate.addConstantUnion((yyvsp[(1) - (1)].lex).d, EbtDouble, (yyvsp[(1) - (1)].lex).loc, true);
    }
    break;

  case 8:
/* Line 1792 of yacc.c  */
#line 246 "glslang.y"
    {
        (yyval.interm.intermTypedNode) = parseContext.intermediate.addConstantUnion((yyvsp[(1) - (1)].lex).b, (yyvsp[(1) - (1)].lex).loc, true);
    }
    break;

  case 9:
/* Line 1792 of yacc.c  */
#line 249 "glslang.y"
    {
        (yyval.interm.intermTypedNode) = (yyvsp[(2) - (3)].interm.intermTypedNode);
        if ((yyval.interm.intermTypedNode)->getAsConstantUnion())
            (yyval.interm.intermTypedNode)->getAsConstantUnion()->setExpression();
    }
    break;

  case 10:
/* Line 1792 of yacc.c  */
#line 257 "glslang.y"
    {
        (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode);
    }
    break;

  case 11:
/* Line 1792 of yacc.c  */
#line 260 "glslang.y"
    {
        (yyval.interm.intermTypedNode) = parseContext.handleBracketDereference((yyvsp[(2) - (4)].lex).loc, (yyvsp[(1) - (4)].interm.intermTypedNode), (yyvsp[(3) - (4)].interm.intermTypedNode));
    }
    break;

  case 12:
/* Line 1792 of yacc.c  */
#line 263 "glslang.y"
    {
        (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode);
    }
    break;

  case 13:
/* Line 1792 of yacc.c  */
#line 266 "glslang.y"
    {
        (yyval.interm.intermTypedNode) = parseContext.handleDotDereference((yyvsp[(3) - (3)].lex).loc, (yyvsp[(1) - (3)].interm.intermTypedNode), *(yyvsp[(3) - (3)].lex).string);
    }
    break;

  case 14:
/* Line 1792 of yacc.c  */
#line 269 "glslang.y"
    {
        parseContext.variableCheck((yyvsp[(1) - (2)].interm.intermTypedNode));
        parseContext.lValueErrorCheck((yyvsp[(2) - (2)].lex).loc, "++", (yyvsp[(1) - (2)].interm.intermTypedNode));
        (yyval.interm.intermTypedNode) = parseContext.handleUnaryMath((yyvsp[(2) - (2)].lex).loc, "++", EOpPostIncrement, (yyvsp[(1) - (2)].interm.intermTypedNode));
    }
    break;

  case 15:
/* Line 1792 of yacc.c  */
#line 274 "glslang.y"
    {
        parseContext.variableCheck((yyvsp[(1) - (2)].interm.intermTypedNode));
        parseContext.lValueErrorCheck((yyvsp[(2) - (2)].lex).loc, "--", (yyvsp[(1) - (2)].interm.intermTypedNode));
        (yyval.interm.intermTypedNode) = parseContext.handleUnaryMath((yyvsp[(2) - (2)].lex).loc, "--", EOpPostDecrement, (yyvsp[(1) - (2)].interm.intermTypedNode));
    }
    break;

  case 16:
/* Line 1792 of yacc.c  */
#line 282 "glslang.y"
    {
        parseContext.integerCheck((yyvsp[(1) - (1)].interm.intermTypedNode), "[]");
        (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode);
    }
    break;

  case 17:
/* Line 1792 of yacc.c  */
#line 289 "glslang.y"
    {
        (yyval.interm.intermTypedNode) = parseContext.handleFunctionCall((yyvsp[(1) - (1)].interm).loc, (yyvsp[(1) - (1)].interm).function, (yyvsp[(1) - (1)].interm).intermNode);
        delete (yyvsp[(1) - (1)].interm).function;
    }
    break;

  case 18:
/* Line 1792 of yacc.c  */
#line 296 "glslang.y"
    {
        (yyval.interm) = (yyvsp[(1) - (1)].interm);
    }
    break;

  case 19:
/* Line 1792 of yacc.c  */
#line 302 "glslang.y"
    {
        (yyval.interm) = (yyvsp[(1) - (2)].interm);
        (yyval.interm).loc = (yyvsp[(2) - (2)].lex).loc;
    }
    break;

  case 20:
/* Line 1792 of yacc.c  */
#line 306 "glslang.y"
    {
        (yyval.interm) = (yyvsp[(1) - (2)].interm);
        (yyval.interm).loc = (yyvsp[(2) - (2)].lex).loc;
    }
    break;

  case 21:
/* Line 1792 of yacc.c  */
#line 313 "glslang.y"
    {
        (yyval.interm) = (yyvsp[(1) - (2)].interm);
    }
    break;

  case 22:
/* Line 1792 of yacc.c  */
#line 316 "glslang.y"
    {
        (yyval.interm) = (yyvsp[(1) - (1)].interm);
    }
    break;

  case 23:
/* Line 1792 of yacc.c  */
#line 322 "glslang.y"
    {
        TParameter param = { 0, new TType };
        param.type->shallowCopy((yyvsp[(2) - (2)].interm.intermTypedNode)->getType());
        (yyvsp[(1) - (2)].interm).function->addParameter(param);
        (yyval.interm).function = (yyvsp[(1) - (2)].interm).function;
        (yyval.interm).intermNode = (yyvsp[(2) - (2)].interm.intermTypedNode);
    }
    break;

  case 24:
/* Line 1792 of yacc.c  */
#line 329 "glslang.y"
    {
        TParameter param = { 0, new TType };
        param.type->shallowCopy((yyvsp[(3) - (3)].interm.intermTypedNode)->getType());
        (yyvsp[(1) - (3)].interm).function->addParameter(param);
        (yyval.interm).function = (yyvsp[(1) - (3)].interm).function;
        (yyval.interm).intermNode = parseContext.intermediate.growAggregate((yyvsp[(1) - (3)].interm).intermNode, (yyvsp[(3) - (3)].interm.intermTypedNode), (yyvsp[(2) - (3)].lex).loc);
    }
    break;

  case 25:
/* Line 1792 of yacc.c  */
#line 339 "glslang.y"
    {
        (yyval.interm) = (yyvsp[(1) - (2)].interm);
    }
    break;

  case 26:
/* Line 1792 of yacc.c  */
#line 347 "glslang.y"
    {
        // Constructor
        (yyval.interm).intermNode = 0;
        (yyval.interm).function = parseContext.handleConstructorCall((yyvsp[(1) - (1)].interm.type).loc, (yyvsp[(1) - (1)].interm.type));
    }
    break;

  case 27:
/* Line 1792 of yacc.c  */
#line 352 "glslang.y"
    {
        //
        // Should be a method or subroutine call, but we haven't recognized the arguments yet.
        //
        (yyval.interm).function = 0;
        (yyval.interm).intermNode = 0;

        TIntermMethod* method = (yyvsp[(1) - (1)].interm.intermTypedNode)->getAsMethodNode();
        if (method) {
            (yyval.interm).function = new TFunction(&method->getMethodName(), TType(EbtInt), EOpArrayLength);
            (yyval.interm).intermNode = method->getObject();
        } else {
            TIntermSymbol* symbol = (yyvsp[(1) - (1)].interm.intermTypedNode)->getAsSymbolNode();
            if (symbol) {
                parseContext.reservedErrorCheck(symbol->getLoc(), symbol->getName());
                TFunction *function = new TFunction(&symbol->getName(), TType(EbtVoid));
                (yyval.interm).function = function;
            } else
                parseContext.error((yyvsp[(1) - (1)].interm.intermTypedNode)->getLoc(), "function call, method, or subroutine call expected", "", "");
        }

        if ((yyval.interm).function == 0) {
            // error recover
            TString empty("");
            (yyval.interm).function = new TFunction(&empty, TType(EbtVoid), EOpNull);
        }
    }
    break;

  case 28:
/* Line 1792 of yacc.c  */
#line 382 "glslang.y"
    {
        parseContext.variableCheck((yyvsp[(1) - (1)].interm.intermTypedNode));
        (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode);
        if (TIntermMethod* method = (yyvsp[(1) - (1)].interm.intermTypedNode)->getAsMethodNode())
            parseContext.error((yyvsp[(1) - (1)].interm.intermTypedNode)->getLoc(), "incomplete method syntax", method->getMethodName().c_str(), "");
    }
    break;

  case 29:
/* Line 1792 of yacc.c  */
#line 388 "glslang.y"
    {
        parseContext.lValueErrorCheck((yyvsp[(1) - (2)].lex).loc, "++", (yyvsp[(2) - (2)].interm.intermTypedNode));
        (yyval.interm.intermTypedNode) = parseContext.handleUnaryMath((yyvsp[(1) - (2)].lex).loc, "++", EOpPreIncrement, (yyvsp[(2) - (2)].interm.intermTypedNode));
    }
    break;

  case 30:
/* Line 1792 of yacc.c  */
#line 392 "glslang.y"
    {
        parseContext.lValueErrorCheck((yyvsp[(1) - (2)].lex).loc, "--", (yyvsp[(2) - (2)].interm.intermTypedNode));
        (yyval.interm.intermTypedNode) = parseContext.handleUnaryMath((yyvsp[(1) - (2)].lex).loc, "--", EOpPreDecrement, (yyvsp[(2) - (2)].interm.intermTypedNode));
    }
    break;

  case 31:
/* Line 1792 of yacc.c  */
#line 396 "glslang.y"
    {
        if ((yyvsp[(1) - (2)].interm).op != EOpNull) {
            char errorOp[2] = {0, 0};
            switch((yyvsp[(1) - (2)].interm).op) {
            case EOpNegative:   errorOp[0] = '-'; break;
            case EOpLogicalNot: errorOp[0] = '!'; break;
            case EOpBitwiseNot: errorOp[0] = '~'; break;
            default: break; // some compilers want this
            }
            (yyval.interm.intermTypedNode) = parseContext.handleUnaryMath((yyvsp[(1) - (2)].interm).loc, errorOp, (yyvsp[(1) - (2)].interm).op, (yyvsp[(2) - (2)].interm.intermTypedNode));
        } else {
            (yyval.interm.intermTypedNode) = (yyvsp[(2) - (2)].interm.intermTypedNode);
            if ((yyval.interm.intermTypedNode)->getAsConstantUnion())
                (yyval.interm.intermTypedNode)->getAsConstantUnion()->setExpression();
        }
    }
    break;

  case 32:
/* Line 1792 of yacc.c  */
#line 416 "glslang.y"
    { (yyval.interm).loc = (yyvsp[(1) - (1)].lex).loc; (yyval.interm).op = EOpNull; }
    break;

  case 33:
/* Line 1792 of yacc.c  */
#line 417 "glslang.y"
    { (yyval.interm).loc = (yyvsp[(1) - (1)].lex).loc; (yyval.interm).op = EOpNegative; }
    break;

  case 34:
/* Line 1792 of yacc.c  */
#line 418 "glslang.y"
    { (yyval.interm).loc = (yyvsp[(1) - (1)].lex).loc; (yyval.interm).op = EOpLogicalNot; }
    break;

  case 35:
/* Line 1792 of yacc.c  */
#line 419 "glslang.y"
    { (yyval.interm).loc = (yyvsp[(1) - (1)].lex).loc; (yyval.interm).op = EOpBitwiseNot;
              parseContext.fullIntegerCheck((yyvsp[(1) - (1)].lex).loc, "bitwise not"); }
    break;

  case 36:
/* Line 1792 of yacc.c  */
#line 425 "glslang.y"
    { (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode); }
    break;

  case 37:
/* Line 1792 of yacc.c  */
#line 426 "glslang.y"
    {
        (yyval.interm.intermTypedNode) = parseContext.handleBinaryMath((yyvsp[(2) - (3)].lex).loc, "*", EOpMul, (yyvsp[(1) - (3)].interm.intermTypedNode), (yyvsp[(3) - (3)].interm.intermTypedNode));
        if ((yyval.interm.intermTypedNode) == 0)
            (yyval.interm.intermTypedNode) = (yyvsp[(1) - (3)].interm.intermTypedNode);
    }
    break;

  case 38:
/* Line 1792 of yacc.c  */
#line 431 "glslang.y"
    {
        (yyval.interm.intermTypedNode) = parseContext.handleBinaryMath((yyvsp[(2) - (3)].lex).loc, "/", EOpDiv, (yyvsp[(1) - (3)].interm.intermTypedNode), (yyvsp[(3) - (3)].interm.intermTypedNode));
        if ((yyval.interm.intermTypedNode) == 0)
            (yyval.interm.intermTypedNode) = (yyvsp[(1) - (3)].interm.intermTypedNode);
    }
    break;

  case 39:
/* Line 1792 of yacc.c  */
#line 436 "glslang.y"
    {
        parseContext.fullIntegerCheck((yyvsp[(2) - (3)].lex).loc, "%");
        (yyval.interm.intermTypedNode) = parseContext.handleBinaryMath((yyvsp[(2) - (3)].lex).loc, "%", EOpMod, (yyvsp[(1) - (3)].interm.intermTypedNode), (yyvsp[(3) - (3)].interm.intermTypedNode));
        if ((yyval.interm.intermTypedNode) == 0)
            (yyval.interm.intermTypedNode) = (yyvsp[(1) - (3)].interm.intermTypedNode);
    }
    break;

  case 40:
/* Line 1792 of yacc.c  */
#line 445 "glslang.y"
    { (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode); }
    break;

  case 41:
/* Line 1792 of yacc.c  */
#line 446 "glslang.y"
    {
        (yyval.interm.intermTypedNode) = parseContext.handleBinaryMath((yyvsp[(2) - (3)].lex).loc, "+", EOpAdd, (yyvsp[(1) - (3)].interm.intermTypedNode), (yyvsp[(3) - (3)].interm.intermTypedNode));
        if ((yyval.interm.intermTypedNode) == 0)
            (yyval.interm.intermTypedNode) = (yyvsp[(1) - (3)].interm.intermTypedNode);
    }
    break;

  case 42:
/* Line 1792 of yacc.c  */
#line 451 "glslang.y"
    {
        (yyval.interm.intermTypedNode) = parseContext.handleBinaryMath((yyvsp[(2) - (3)].lex).loc, "-", EOpSub, (yyvsp[(1) - (3)].interm.intermTypedNode), (yyvsp[(3) - (3)].interm.intermTypedNode));
        if ((yyval.interm.intermTypedNode) == 0)
            (yyval.interm.intermTypedNode) = (yyvsp[(1) - (3)].interm.intermTypedNode);
    }
    break;

  case 43:
/* Line 1792 of yacc.c  */
#line 459 "glslang.y"
    { (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode); }
    break;

  case 44:
/* Line 1792 of yacc.c  */
#line 460 "glslang.y"
    {
        parseContext.fullIntegerCheck((yyvsp[(2) - (3)].lex).loc, "bit shift left");
        (yyval.interm.intermTypedNode) = parseContext.handleBinaryMath((yyvsp[(2) - (3)].lex).loc, "<<", EOpLeftShift, (yyvsp[(1) - (3)].interm.intermTypedNode), (yyvsp[(3) - (3)].interm.intermTypedNode));
        if ((yyval.interm.intermTypedNode) == 0)
            (yyval.interm.intermTypedNode) = (yyvsp[(1) - (3)].interm.intermTypedNode);
    }
    break;

  case 45:
/* Line 1792 of yacc.c  */
#line 466 "glslang.y"
    {
        parseContext.fullIntegerCheck((yyvsp[(2) - (3)].lex).loc, "bit shift right");
        (yyval.interm.intermTypedNode) = parseContext.handleBinaryMath((yyvsp[(2) - (3)].lex).loc, ">>", EOpRightShift, (yyvsp[(1) - (3)].interm.intermTypedNode), (yyvsp[(3) - (3)].interm.intermTypedNode));
        if ((yyval.interm.intermTypedNode) == 0)
            (yyval.interm.intermTypedNode) = (yyvsp[(1) - (3)].interm.intermTypedNode);
    }
    break;

  case 46:
/* Line 1792 of yacc.c  */
#line 475 "glslang.y"
    { (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode); }
    break;

  case 47:
/* Line 1792 of yacc.c  */
#line 476 "glslang.y"
    {
        (yyval.interm.intermTypedNode) = parseContext.handleBinaryMath((yyvsp[(2) - (3)].lex).loc, "<", EOpLessThan, (yyvsp[(1) - (3)].interm.intermTypedNode), (yyvsp[(3) - (3)].interm.intermTypedNode));
        if ((yyval.interm.intermTypedNode) == 0)
            (yyval.interm.intermTypedNode) = parseContext.intermediate.addConstantUnion(false, (yyvsp[(2) - (3)].lex).loc);
    }
    break;

  case 48:
/* Line 1792 of yacc.c  */
#line 481 "glslang.y"
    {
        (yyval.interm.intermTypedNode) = parseContext.handleBinaryMath((yyvsp[(2) - (3)].lex).loc, ">", EOpGreaterThan, (yyvsp[(1) - (3)].interm.intermTypedNode), (yyvsp[(3) - (3)].interm.intermTypedNode));
        if ((yyval.interm.intermTypedNode) == 0)
            (yyval.interm.intermTypedNode) = parseContext.intermediate.addConstantUnion(false, (yyvsp[(2) - (3)].lex).loc);
    }
    break;

  case 49:
/* Line 1792 of yacc.c  */
#line 486 "glslang.y"
    {
        (yyval.interm.intermTypedNode) = parseContext.handleBinaryMath((yyvsp[(2) - (3)].lex).loc, "<=", EOpLessThanEqual, (yyvsp[(1) - (3)].interm.intermTypedNode), (yyvsp[(3) - (3)].interm.intermTypedNode));
        if ((yyval.interm.intermTypedNode) == 0)
            (yyval.interm.intermTypedNode) = parseContext.intermediate.addConstantUnion(false, (yyvsp[(2) - (3)].lex).loc);
    }
    break;

  case 50:
/* Line 1792 of yacc.c  */
#line 491 "glslang.y"
    {
        (yyval.interm.intermTypedNode) = parseContext.handleBinaryMath((yyvsp[(2) - (3)].lex).loc, ">=", EOpGreaterThanEqual, (yyvsp[(1) - (3)].interm.intermTypedNode), (yyvsp[(3) - (3)].interm.intermTypedNode));
        if ((yyval.interm.intermTypedNode) == 0)
            (yyval.interm.intermTypedNode) = parseContext.intermediate.addConstantUnion(false, (yyvsp[(2) - (3)].lex).loc);
    }
    break;

  case 51:
/* Line 1792 of yacc.c  */
#line 499 "glslang.y"
    { (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode); }
    break;

  case 52:
/* Line 1792 of yacc.c  */
#line 500 "glslang.y"
    {
        parseContext.arrayObjectCheck((yyvsp[(2) - (3)].lex).loc, (yyvsp[(1) - (3)].interm.intermTypedNode)->getType(), "array comparison");
        parseContext.opaqueCheck((yyvsp[(2) - (3)].lex).loc, (yyvsp[(1) - (3)].interm.intermTypedNode)->getType(), "==");
        (yyval.interm.intermTypedNode) = parseContext.handleBinaryMath((yyvsp[(2) - (3)].lex).loc, "==", EOpEqual, (yyvsp[(1) - (3)].interm.intermTypedNode), (yyvsp[(3) - (3)].interm.intermTypedNode));
        if ((yyval.interm.intermTypedNode) == 0)
            (yyval.interm.intermTypedNode) = parseContext.intermediate.addConstantUnion(false, (yyvsp[(2) - (3)].lex).loc);
    }
    break;

  case 53:
/* Line 1792 of yacc.c  */
#line 507 "glslang.y"
    {
        parseContext.arrayObjectCheck((yyvsp[(2) - (3)].lex).loc, (yyvsp[(1) - (3)].interm.intermTypedNode)->getType(), "array comparison");
        parseContext.opaqueCheck((yyvsp[(2) - (3)].lex).loc, (yyvsp[(1) - (3)].interm.intermTypedNode)->getType(), "!=");
        (yyval.interm.intermTypedNode) = parseContext.handleBinaryMath((yyvsp[(2) - (3)].lex).loc, "!=", EOpNotEqual, (yyvsp[(1) - (3)].interm.intermTypedNode), (yyvsp[(3) - (3)].interm.intermTypedNode));
        if ((yyval.interm.intermTypedNode) == 0)
            (yyval.interm.intermTypedNode) = parseContext.intermediate.addConstantUnion(false, (yyvsp[(2) - (3)].lex).loc);
    }
    break;

  case 54:
/* Line 1792 of yacc.c  */
#line 517 "glslang.y"
    { (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode); }
    break;

  case 55:
/* Line 1792 of yacc.c  */
#line 518 "glslang.y"
    {
        parseContext.fullIntegerCheck((yyvsp[(2) - (3)].lex).loc, "bitwise and");
        (yyval.interm.intermTypedNode) = parseContext.handleBinaryMath((yyvsp[(2) - (3)].lex).loc, "&", EOpAnd, (yyvsp[(1) - (3)].interm.intermTypedNode), (yyvsp[(3) - (3)].interm.intermTypedNode));
        if ((yyval.interm.intermTypedNode) == 0)
            (yyval.interm.intermTypedNode) = (yyvsp[(1) - (3)].interm.intermTypedNode);
    }
    break;

  case 56:
/* Line 1792 of yacc.c  */
#line 527 "glslang.y"
    { (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode); }
    break;

  case 57:
/* Line 1792 of yacc.c  */
#line 528 "glslang.y"
    {
        parseContext.fullIntegerCheck((yyvsp[(2) - (3)].lex).loc, "bitwise exclusive or");
        (yyval.interm.intermTypedNode) = parseContext.handleBinaryMath((yyvsp[(2) - (3)].lex).loc, "^", EOpExclusiveOr, (yyvsp[(1) - (3)].interm.intermTypedNode), (yyvsp[(3) - (3)].interm.intermTypedNode));
        if ((yyval.interm.intermTypedNode) == 0)
            (yyval.interm.intermTypedNode) = (yyvsp[(1) - (3)].interm.intermTypedNode);
    }
    break;

  case 58:
/* Line 1792 of yacc.c  */
#line 537 "glslang.y"
    { (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode); }
    break;

  case 59:
/* Line 1792 of yacc.c  */
#line 538 "glslang.y"
    {
        parseContext.fullIntegerCheck((yyvsp[(2) - (3)].lex).loc, "bitwise inclusive or");
        (yyval.interm.intermTypedNode) = parseContext.handleBinaryMath((yyvsp[(2) - (3)].lex).loc, "|", EOpInclusiveOr, (yyvsp[(1) - (3)].interm.intermTypedNode), (yyvsp[(3) - (3)].interm.intermTypedNode));
        if ((yyval.interm.intermTypedNode) == 0)
            (yyval.interm.intermTypedNode) = (yyvsp[(1) - (3)].interm.intermTypedNode);
    }
    break;

  case 60:
/* Line 1792 of yacc.c  */
#line 547 "glslang.y"
    { (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode); }
    break;

  case 61:
/* Line 1792 of yacc.c  */
#line 548 "glslang.y"
    {
        (yyval.interm.intermTypedNode) = parseContext.handleBinaryMath((yyvsp[(2) - (3)].lex).loc, "&&", EOpLogicalAnd, (yyvsp[(1) - (3)].interm.intermTypedNode), (yyvsp[(3) - (3)].interm.intermTypedNode));
        if ((yyval.interm.intermTypedNode) == 0)
            (yyval.interm.intermTypedNode) = parseContext.intermediate.addConstantUnion(false, (yyvsp[(2) - (3)].lex).loc);
    }
    break;

  case 62:
/* Line 1792 of yacc.c  */
#line 556 "glslang.y"
    { (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode); }
    break;

  case 63:
/* Line 1792 of yacc.c  */
#line 557 "glslang.y"
    {
        (yyval.interm.intermTypedNode) = parseContext.handleBinaryMath((yyvsp[(2) - (3)].lex).loc, "^^", EOpLogicalXor, (yyvsp[(1) - (3)].interm.intermTypedNode), (yyvsp[(3) - (3)].interm.intermTypedNode));
        if ((yyval.interm.intermTypedNode) == 0)
            (yyval.interm.intermTypedNode) = parseContext.intermediate.addConstantUnion(false, (yyvsp[(2) - (3)].lex).loc);
    }
    break;

  case 64:
/* Line 1792 of yacc.c  */
#line 565 "glslang.y"
    { (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode); }
    break;

  case 65:
/* Line 1792 of yacc.c  */
#line 566 "glslang.y"
    {
        (yyval.interm.intermTypedNode) = parseContext.handleBinaryMath((yyvsp[(2) - (3)].lex).loc, "||", EOpLogicalOr, (yyvsp[(1) - (3)].interm.intermTypedNode), (yyvsp[(3) - (3)].interm.intermTypedNode));
        if ((yyval.interm.intermTypedNode) == 0)
            (yyval.interm.intermTypedNode) = parseContext.intermediate.addConstantUnion(false, (yyvsp[(2) - (3)].lex).loc);
    }
    break;

  case 66:
/* Line 1792 of yacc.c  */
#line 574 "glslang.y"
    { (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode); }
    break;

  case 67:
/* Line 1792 of yacc.c  */
#line 575 "glslang.y"
    {
        ++parseContext.controlFlowNestingLevel;
    }
    break;

  case 68:
/* Line 1792 of yacc.c  */
#line 578 "glslang.y"
    {
        --parseContext.controlFlowNestingLevel;
        parseContext.boolCheck((yyvsp[(2) - (6)].lex).loc, (yyvsp[(1) - (6)].interm.intermTypedNode));
        parseContext.rValueErrorCheck((yyvsp[(2) - (6)].lex).loc, "?", (yyvsp[(1) - (6)].interm.intermTypedNode));
        parseContext.rValueErrorCheck((yyvsp[(5) - (6)].lex).loc, ":", (yyvsp[(4) - (6)].interm.intermTypedNode));
        parseContext.rValueErrorCheck((yyvsp[(5) - (6)].lex).loc, ":", (yyvsp[(6) - (6)].interm.intermTypedNode));
        (yyval.interm.intermTypedNode) = parseContext.intermediate.addSelection((yyvsp[(1) - (6)].interm.intermTypedNode), (yyvsp[(4) - (6)].interm.intermTypedNode), (yyvsp[(6) - (6)].interm.intermTypedNode), (yyvsp[(2) - (6)].lex).loc);
        if ((yyval.interm.intermTypedNode) == 0) {
            parseContext.binaryOpError((yyvsp[(2) - (6)].lex).loc, ":", (yyvsp[(4) - (6)].interm.intermTypedNode)->getCompleteString(), (yyvsp[(6) - (6)].interm.intermTypedNode)->getCompleteString());
            (yyval.interm.intermTypedNode) = (yyvsp[(6) - (6)].interm.intermTypedNode);
        }
    }
    break;

  case 69:
/* Line 1792 of yacc.c  */
#line 593 "glslang.y"
    { (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode); }
    break;

  case 70:
/* Line 1792 of yacc.c  */
#line 594 "glslang.y"
    {
        parseContext.arrayObjectCheck((yyvsp[(2) - (3)].interm).loc, (yyvsp[(1) - (3)].interm.intermTypedNode)->getType(), "array assignment");
        parseContext.opaqueCheck((yyvsp[(2) - (3)].interm).loc, (yyvsp[(1) - (3)].interm.intermTypedNode)->getType(), "=");
        parseContext.lValueErrorCheck((yyvsp[(2) - (3)].interm).loc, "assign", (yyvsp[(1) - (3)].interm.intermTypedNode));
        parseContext.rValueErrorCheck((yyvsp[(2) - (3)].interm).loc, "assign", (yyvsp[(3) - (3)].interm.intermTypedNode));
        (yyval.interm.intermTypedNode) = parseContext.intermediate.addAssign((yyvsp[(2) - (3)].interm).op, (yyvsp[(1) - (3)].interm.intermTypedNode), (yyvsp[(3) - (3)].interm.intermTypedNode), (yyvsp[(2) - (3)].interm).loc);
        if ((yyval.interm.intermTypedNode) == 0) {
            parseContext.assignError((yyvsp[(2) - (3)].interm).loc, "assign", (yyvsp[(1) - (3)].interm.intermTypedNode)->getCompleteString(), (yyvsp[(3) - (3)].interm.intermTypedNode)->getCompleteString());
            (yyval.interm.intermTypedNode) = (yyvsp[(1) - (3)].interm.intermTypedNode);
        }
    }
    break;

  case 71:
/* Line 1792 of yacc.c  */
#line 608 "glslang.y"
    {
        (yyval.interm).loc = (yyvsp[(1) - (1)].lex).loc;
        (yyval.interm).op = EOpAssign;
    }
    break;

  case 72:
/* Line 1792 of yacc.c  */
#line 612 "glslang.y"
    {
        (yyval.interm).loc = (yyvsp[(1) - (1)].lex).loc;
        (yyval.interm).op = EOpMulAssign;
    }
    break;

  case 73:
/* Line 1792 of yacc.c  */
#line 616 "glslang.y"
    {
        (yyval.interm).loc = (yyvsp[(1) - (1)].lex).loc;
        (yyval.interm).op = EOpDivAssign;
    }
    break;

  case 74:
/* Line 1792 of yacc.c  */
#line 620 "glslang.y"
    {
        parseContext.fullIntegerCheck((yyvsp[(1) - (1)].lex).loc, "%=");
        (yyval.interm).loc = (yyvsp[(1) - (1)].lex).loc;
        (yyval.interm).op = EOpModAssign;
    }
    break;

  case 75:
/* Line 1792 of yacc.c  */
#line 625 "glslang.y"
    {
        (yyval.interm).loc = (yyvsp[(1) - (1)].lex).loc;
        (yyval.interm).op = EOpAddAssign;
    }
    break;

  case 76:
/* Line 1792 of yacc.c  */
#line 629 "glslang.y"
    {
        (yyval.interm).loc = (yyvsp[(1) - (1)].lex).loc;
        (yyval.interm).op = EOpSubAssign;
    }
    break;

  case 77:
/* Line 1792 of yacc.c  */
#line 633 "glslang.y"
    {
        parseContext.fullIntegerCheck((yyvsp[(1) - (1)].lex).loc, "bit-shift left assign");
        (yyval.interm).loc = (yyvsp[(1) - (1)].lex).loc; (yyval.interm).op = EOpLeftShiftAssign;
    }
    break;

  case 78:
/* Line 1792 of yacc.c  */
#line 637 "glslang.y"
    {
        parseContext.fullIntegerCheck((yyvsp[(1) - (1)].lex).loc, "bit-shift right assign");
        (yyval.interm).loc = (yyvsp[(1) - (1)].lex).loc; (yyval.interm).op = EOpRightShiftAssign;
    }
    break;

  case 79:
/* Line 1792 of yacc.c  */
#line 641 "glslang.y"
    {
        parseContext.fullIntegerCheck((yyvsp[(1) - (1)].lex).loc, "bitwise-and assign");
        (yyval.interm).loc = (yyvsp[(1) - (1)].lex).loc; (yyval.interm).op = EOpAndAssign;
    }
    break;

  case 80:
/* Line 1792 of yacc.c  */
#line 645 "glslang.y"
    {
        parseContext.fullIntegerCheck((yyvsp[(1) - (1)].lex).loc, "bitwise-xor assign");
        (yyval.interm).loc = (yyvsp[(1) - (1)].lex).loc; (yyval.interm).op = EOpExclusiveOrAssign;
    }
    break;

  case 81:
/* Line 1792 of yacc.c  */
#line 649 "glslang.y"
    {
        parseContext.fullIntegerCheck((yyvsp[(1) - (1)].lex).loc, "bitwise-or assign");
        (yyval.interm).loc = (yyvsp[(1) - (1)].lex).loc; (yyval.interm).op = EOpInclusiveOrAssign;
    }
    break;

  case 82:
/* Line 1792 of yacc.c  */
#line 656 "glslang.y"
    {
        (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode);
    }
    break;

  case 83:
/* Line 1792 of yacc.c  */
#line 659 "glslang.y"
    {
        (yyval.interm.intermTypedNode) = parseContext.intermediate.addComma((yyvsp[(1) - (3)].interm.intermTypedNode), (yyvsp[(3) - (3)].interm.intermTypedNode), (yyvsp[(2) - (3)].lex).loc);
        if ((yyval.interm.intermTypedNode) == 0) {
            parseContext.binaryOpError((yyvsp[(2) - (3)].lex).loc, ",", (yyvsp[(1) - (3)].interm.intermTypedNode)->getCompleteString(), (yyvsp[(3) - (3)].interm.intermTypedNode)->getCompleteString());
            (yyval.interm.intermTypedNode) = (yyvsp[(3) - (3)].interm.intermTypedNode);
        }
    }
    break;

  case 84:
/* Line 1792 of yacc.c  */
#line 669 "glslang.y"
    {
        parseContext.constantValueCheck((yyvsp[(1) - (1)].interm.intermTypedNode), "");
        (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode);
    }
    break;

  case 85:
/* Line 1792 of yacc.c  */
#line 676 "glslang.y"
    {
        parseContext.handleFunctionDeclarator((yyvsp[(1) - (2)].interm).loc, *(yyvsp[(1) - (2)].interm).function, true /* prototype */);
        (yyval.interm.intermNode) = 0;
        // TODO: 4.0 functionality: subroutines: make the identifier a user type for this signature
    }
    break;

  case 86:
/* Line 1792 of yacc.c  */
#line 681 "glslang.y"
    {
        if ((yyvsp[(1) - (2)].interm).intermNode && (yyvsp[(1) - (2)].interm).intermNode->getAsAggregate())
            (yyvsp[(1) - (2)].interm).intermNode->getAsAggregate()->setOperator(EOpSequence);
        (yyval.interm.intermNode) = (yyvsp[(1) - (2)].interm).intermNode;
    }
    break;

  case 87:
/* Line 1792 of yacc.c  */
#line 686 "glslang.y"
    {
        parseContext.profileRequires((yyvsp[(1) - (4)].lex).loc, ENoProfile, 130, 0, "precision statement");

        // lazy setting of the previous scope's defaults, has effect only the first time it is called in a particular scope
        parseContext.symbolTable.setPreviousDefaultPrecisions(&parseContext.defaultPrecision[0]);
		parseContext.setDefaultPrecision((yyvsp[(1) - (4)].lex).loc, (yyvsp[(3) - (4)].interm.type), (yyvsp[(2) - (4)].interm.type).qualifier.precision);
        (yyval.interm.intermNode) = 0;
    }
    break;

  case 88:
/* Line 1792 of yacc.c  */
#line 694 "glslang.y"
    {
        parseContext.declareBlock((yyvsp[(1) - (2)].interm).loc, *(yyvsp[(1) - (2)].interm).typeList);
        (yyval.interm.intermNode) = 0;
    }
    break;

  case 89:
/* Line 1792 of yacc.c  */
#line 698 "glslang.y"
    {
        parseContext.declareBlock((yyvsp[(1) - (3)].interm).loc, *(yyvsp[(1) - (3)].interm).typeList, (yyvsp[(2) - (3)].lex).string);
        (yyval.interm.intermNode) = 0;
    }
    break;

  case 90:
/* Line 1792 of yacc.c  */
#line 702 "glslang.y"
    {
        parseContext.declareBlock((yyvsp[(1) - (4)].interm).loc, *(yyvsp[(1) - (4)].interm).typeList, (yyvsp[(2) - (4)].lex).string, (yyvsp[(3) - (4)].interm).arraySizes);
        (yyval.interm.intermNode) = 0;
    }
    break;

  case 91:
/* Line 1792 of yacc.c  */
#line 706 "glslang.y"
    {
        parseContext.globalQualifierFixCheck((yyvsp[(1) - (2)].interm.type).loc, (yyvsp[(1) - (2)].interm.type).qualifier);
        parseContext.updateStandaloneQualifierDefaults((yyvsp[(1) - (2)].interm.type).loc, (yyvsp[(1) - (2)].interm.type));
        (yyval.interm.intermNode) = 0;
    }
    break;

  case 92:
/* Line 1792 of yacc.c  */
#line 711 "glslang.y"
    {
        parseContext.checkNoShaderLayouts((yyvsp[(1) - (3)].interm.type).loc, (yyvsp[(1) - (3)].interm.type).shaderQualifiers);
        parseContext.addQualifierToExisting((yyvsp[(1) - (3)].interm.type).loc, (yyvsp[(1) - (3)].interm.type).qualifier, *(yyvsp[(2) - (3)].lex).string);
        (yyval.interm.intermNode) = 0;
    }
    break;

  case 93:
/* Line 1792 of yacc.c  */
#line 716 "glslang.y"
    {
        parseContext.checkNoShaderLayouts((yyvsp[(1) - (4)].interm.type).loc, (yyvsp[(1) - (4)].interm.type).shaderQualifiers);
        (yyvsp[(3) - (4)].interm.identifierList)->push_back((yyvsp[(2) - (4)].lex).string);
        parseContext.addQualifierToExisting((yyvsp[(1) - (4)].interm.type).loc, (yyvsp[(1) - (4)].interm.type).qualifier, *(yyvsp[(3) - (4)].interm.identifierList));
        (yyval.interm.intermNode) = 0;
    }
    break;

  case 94:
/* Line 1792 of yacc.c  */
#line 725 "glslang.y"
    { parseContext.nestedBlockCheck((yyvsp[(1) - (3)].interm.type).loc); }
    break;

  case 95:
/* Line 1792 of yacc.c  */
#line 725 "glslang.y"
    {
        --parseContext.structNestingLevel;
        parseContext.blockName = (yyvsp[(2) - (6)].lex).string;
        parseContext.globalQualifierFixCheck((yyvsp[(1) - (6)].interm.type).loc, (yyvsp[(1) - (6)].interm.type).qualifier);
        parseContext.checkNoShaderLayouts((yyvsp[(1) - (6)].interm.type).loc, (yyvsp[(1) - (6)].interm.type).shaderQualifiers);
        parseContext.currentBlockQualifier = (yyvsp[(1) - (6)].interm.type).qualifier;
        (yyval.interm).loc = (yyvsp[(1) - (6)].interm.type).loc;
        (yyval.interm).typeList = (yyvsp[(5) - (6)].interm.typeList);
    }
    break;

  case 96:
/* Line 1792 of yacc.c  */
#line 736 "glslang.y"
    {
        (yyval.interm.identifierList) = new TIdentifierList;
        (yyval.interm.identifierList)->push_back((yyvsp[(2) - (2)].lex).string);
    }
    break;

  case 97:
/* Line 1792 of yacc.c  */
#line 740 "glslang.y"
    {
        (yyval.interm.identifierList) = (yyvsp[(1) - (3)].interm.identifierList);
        (yyval.interm.identifierList)->push_back((yyvsp[(3) - (3)].lex).string);
    }
    break;

  case 98:
/* Line 1792 of yacc.c  */
#line 747 "glslang.y"
    {
        (yyval.interm).function = (yyvsp[(1) - (2)].interm.function);
        (yyval.interm).loc = (yyvsp[(2) - (2)].lex).loc;
    }
    break;

  case 99:
/* Line 1792 of yacc.c  */
#line 754 "glslang.y"
    {
        (yyval.interm.function) = (yyvsp[(1) - (1)].interm.function);
    }
    break;

  case 100:
/* Line 1792 of yacc.c  */
#line 757 "glslang.y"
    {
        (yyval.interm.function) = (yyvsp[(1) - (1)].interm.function);
    }
    break;

  case 101:
/* Line 1792 of yacc.c  */
#line 764 "glslang.y"
    {
        // Add the parameter
        (yyval.interm.function) = (yyvsp[(1) - (2)].interm.function);
        if ((yyvsp[(2) - (2)].interm).param.type->getBasicType() != EbtVoid)
            (yyvsp[(1) - (2)].interm.function)->addParameter((yyvsp[(2) - (2)].interm).param);
        else
            delete (yyvsp[(2) - (2)].interm).param.type;
    }
    break;

  case 102:
/* Line 1792 of yacc.c  */
#line 772 "glslang.y"
    {
        //
        // Only first parameter of one-parameter functions can be void
        // The check for named parameters not being void is done in parameter_declarator
        //
        if ((yyvsp[(3) - (3)].interm).param.type->getBasicType() == EbtVoid) {
            //
            // This parameter > first is void
            //
            parseContext.error((yyvsp[(2) - (3)].lex).loc, "cannot be an argument type except for '(void)'", "void", "");
            delete (yyvsp[(3) - (3)].interm).param.type;
        } else {
            // Add the parameter
            (yyval.interm.function) = (yyvsp[(1) - (3)].interm.function);
            (yyvsp[(1) - (3)].interm.function)->addParameter((yyvsp[(3) - (3)].interm).param);
        }
    }
    break;

  case 103:
/* Line 1792 of yacc.c  */
#line 792 "glslang.y"
    {
        if ((yyvsp[(1) - (3)].interm.type).qualifier.storage != EvqGlobal && (yyvsp[(1) - (3)].interm.type).qualifier.storage != EvqTemporary) {
            parseContext.error((yyvsp[(2) - (3)].lex).loc, "no qualifiers allowed for function return",
                               GetStorageQualifierString((yyvsp[(1) - (3)].interm.type).qualifier.storage), "");
        }
        if ((yyvsp[(1) - (3)].interm.type).arraySizes)
            parseContext.arraySizeRequiredCheck((yyvsp[(1) - (3)].interm.type).loc, (yyvsp[(1) - (3)].interm.type).arraySizes->getOuterSize());

        // Add the function as a prototype after parsing it (we do not support recursion)
        TFunction *function;
        TType type((yyvsp[(1) - (3)].interm.type));
        function = new TFunction((yyvsp[(2) - (3)].lex).string, type);
        (yyval.interm.function) = function;
    }
    break;

  case 104:
/* Line 1792 of yacc.c  */
#line 810 "glslang.y"
    {
        if ((yyvsp[(1) - (2)].interm.type).arraySizes) {
            parseContext.profileRequires((yyvsp[(1) - (2)].interm.type).loc, ENoProfile, 120, E_GL_3DL_array_objects, "arrayed type");
            parseContext.profileRequires((yyvsp[(1) - (2)].interm.type).loc, EEsProfile, 300, 0, "arrayed type");
            parseContext.arraySizeRequiredCheck((yyvsp[(1) - (2)].interm.type).loc, (yyvsp[(1) - (2)].interm.type).arraySizes->getOuterSize());
        }
        if ((yyvsp[(1) - (2)].interm.type).basicType == EbtVoid) {
            parseContext.error((yyvsp[(2) - (2)].lex).loc, "illegal use of type 'void'", (yyvsp[(2) - (2)].lex).string->c_str(), "");
        }
        parseContext.reservedErrorCheck((yyvsp[(2) - (2)].lex).loc, *(yyvsp[(2) - (2)].lex).string);

        TParameter param = {(yyvsp[(2) - (2)].lex).string, new TType((yyvsp[(1) - (2)].interm.type))};
        (yyval.interm).loc = (yyvsp[(2) - (2)].lex).loc;
        (yyval.interm).param = param;
    }
    break;

  case 105:
/* Line 1792 of yacc.c  */
#line 825 "glslang.y"
    {
        if ((yyvsp[(1) - (3)].interm.type).arraySizes) {
            parseContext.profileRequires((yyvsp[(1) - (3)].interm.type).loc, ENoProfile, 120, E_GL_3DL_array_objects, "arrayed type");
            parseContext.profileRequires((yyvsp[(1) - (3)].interm.type).loc, EEsProfile, 300, 0, "arrayed type");
            parseContext.arraySizeRequiredCheck((yyvsp[(1) - (3)].interm.type).loc, (yyvsp[(1) - (3)].interm.type).arraySizes->getOuterSize());
        }
        parseContext.arrayDimCheck((yyvsp[(2) - (3)].lex).loc, (yyvsp[(1) - (3)].interm.type).arraySizes, (yyvsp[(3) - (3)].interm).arraySizes);

        parseContext.arraySizeRequiredCheck((yyvsp[(3) - (3)].interm).loc, (yyvsp[(3) - (3)].interm).arraySizes->getOuterSize());
        parseContext.reservedErrorCheck((yyvsp[(2) - (3)].lex).loc, *(yyvsp[(2) - (3)].lex).string);

        (yyvsp[(1) - (3)].interm.type).arraySizes = (yyvsp[(3) - (3)].interm).arraySizes;

        TParameter param = { (yyvsp[(2) - (3)].lex).string, new TType((yyvsp[(1) - (3)].interm.type))};
        (yyval.interm).loc = (yyvsp[(2) - (3)].lex).loc;
        (yyval.interm).param = param;
    }
    break;

  case 106:
/* Line 1792 of yacc.c  */
#line 848 "glslang.y"
    {
        (yyval.interm) = (yyvsp[(2) - (2)].interm);
        if ((yyvsp[(1) - (2)].interm.type).qualifier.precision != EpqNone)
            (yyval.interm).param.type->getQualifier().precision = (yyvsp[(1) - (2)].interm.type).qualifier.precision;
        parseContext.precisionQualifierCheck((yyval.interm).loc, (yyval.interm).param.type->getBasicType(), (yyval.interm).param.type->getQualifier());

        parseContext.checkNoShaderLayouts((yyvsp[(1) - (2)].interm.type).loc, (yyvsp[(1) - (2)].interm.type).shaderQualifiers);
        parseContext.parameterTypeCheck((yyvsp[(2) - (2)].interm).loc, (yyvsp[(1) - (2)].interm.type).qualifier.storage, *(yyval.interm).param.type);
        parseContext.paramCheckFix((yyvsp[(1) - (2)].interm.type).loc, (yyvsp[(1) - (2)].interm.type).qualifier, *(yyval.interm).param.type);

    }
    break;

  case 107:
/* Line 1792 of yacc.c  */
#line 859 "glslang.y"
    {
        (yyval.interm) = (yyvsp[(1) - (1)].interm);

        parseContext.parameterTypeCheck((yyvsp[(1) - (1)].interm).loc, EvqIn, *(yyvsp[(1) - (1)].interm).param.type);
        parseContext.paramCheckFix((yyvsp[(1) - (1)].interm).loc, EvqTemporary, *(yyval.interm).param.type);
        parseContext.precisionQualifierCheck((yyval.interm).loc, (yyval.interm).param.type->getBasicType(), (yyval.interm).param.type->getQualifier());
    }
    break;

  case 108:
/* Line 1792 of yacc.c  */
#line 869 "glslang.y"
    {
        (yyval.interm) = (yyvsp[(2) - (2)].interm);
        if ((yyvsp[(1) - (2)].interm.type).qualifier.precision != EpqNone)
            (yyval.interm).param.type->getQualifier().precision = (yyvsp[(1) - (2)].interm.type).qualifier.precision;
        parseContext.precisionQualifierCheck((yyvsp[(1) - (2)].interm.type).loc, (yyval.interm).param.type->getBasicType(), (yyval.interm).param.type->getQualifier());

        parseContext.checkNoShaderLayouts((yyvsp[(1) - (2)].interm.type).loc, (yyvsp[(1) - (2)].interm.type).shaderQualifiers);
        parseContext.parameterTypeCheck((yyvsp[(2) - (2)].interm).loc, (yyvsp[(1) - (2)].interm.type).qualifier.storage, *(yyval.interm).param.type);
        parseContext.paramCheckFix((yyvsp[(1) - (2)].interm.type).loc, (yyvsp[(1) - (2)].interm.type).qualifier, *(yyval.interm).param.type);
    }
    break;

  case 109:
/* Line 1792 of yacc.c  */
#line 879 "glslang.y"
    {
        (yyval.interm) = (yyvsp[(1) - (1)].interm);

        parseContext.parameterTypeCheck((yyvsp[(1) - (1)].interm).loc, EvqIn, *(yyvsp[(1) - (1)].interm).param.type);
        parseContext.paramCheckFix((yyvsp[(1) - (1)].interm).loc, EvqTemporary, *(yyval.interm).param.type);
        parseContext.precisionQualifierCheck((yyval.interm).loc, (yyval.interm).param.type->getBasicType(), (yyval.interm).param.type->getQualifier());
    }
    break;

  case 110:
/* Line 1792 of yacc.c  */
#line 889 "glslang.y"
    {
        TParameter param = { 0, new TType((yyvsp[(1) - (1)].interm.type)) };
        (yyval.interm).param = param;
        if ((yyvsp[(1) - (1)].interm.type).arraySizes)
            parseContext.arraySizeRequiredCheck((yyvsp[(1) - (1)].interm.type).loc, (yyvsp[(1) - (1)].interm.type).arraySizes->getOuterSize());
    }
    break;

  case 111:
/* Line 1792 of yacc.c  */
#line 898 "glslang.y"
    {
        (yyval.interm) = (yyvsp[(1) - (1)].interm);
    }
    break;

  case 112:
/* Line 1792 of yacc.c  */
#line 901 "glslang.y"
    {
        (yyval.interm) = (yyvsp[(1) - (3)].interm);
        parseContext.declareVariable((yyvsp[(3) - (3)].lex).loc, *(yyvsp[(3) - (3)].lex).string, (yyvsp[(1) - (3)].interm).type);
    }
    break;

  case 113:
/* Line 1792 of yacc.c  */
#line 905 "glslang.y"
    {
        (yyval.interm) = (yyvsp[(1) - (4)].interm);
        parseContext.declareVariable((yyvsp[(3) - (4)].lex).loc, *(yyvsp[(3) - (4)].lex).string, (yyvsp[(1) - (4)].interm).type, (yyvsp[(4) - (4)].interm).arraySizes);
    }
    break;

  case 114:
/* Line 1792 of yacc.c  */
#line 909 "glslang.y"
    {
        (yyval.interm).type = (yyvsp[(1) - (6)].interm).type;
        TIntermNode* initNode = parseContext.declareVariable((yyvsp[(3) - (6)].lex).loc, *(yyvsp[(3) - (6)].lex).string, (yyvsp[(1) - (6)].interm).type, (yyvsp[(4) - (6)].interm).arraySizes, (yyvsp[(6) - (6)].interm.intermTypedNode));
        (yyval.interm).intermNode = parseContext.intermediate.growAggregate((yyvsp[(1) - (6)].interm).intermNode, initNode, (yyvsp[(5) - (6)].lex).loc);
    }
    break;

  case 115:
/* Line 1792 of yacc.c  */
#line 914 "glslang.y"
    {
        (yyval.interm).type = (yyvsp[(1) - (5)].interm).type;
        TIntermNode* initNode = parseContext.declareVariable((yyvsp[(3) - (5)].lex).loc, *(yyvsp[(3) - (5)].lex).string, (yyvsp[(1) - (5)].interm).type, 0, (yyvsp[(5) - (5)].interm.intermTypedNode));
        (yyval.interm).intermNode = parseContext.intermediate.growAggregate((yyvsp[(1) - (5)].interm).intermNode, initNode, (yyvsp[(4) - (5)].lex).loc);
    }
    break;

  case 116:
/* Line 1792 of yacc.c  */
#line 922 "glslang.y"
    {
        (yyval.interm).type = (yyvsp[(1) - (1)].interm.type);
        (yyval.interm).intermNode = 0;
        parseContext.declareTypeDefaults((yyval.interm).loc, (yyval.interm).type);
    }
    break;

  case 117:
/* Line 1792 of yacc.c  */
#line 927 "glslang.y"
    {
        (yyval.interm).type = (yyvsp[(1) - (2)].interm.type);
        (yyval.interm).intermNode = 0;
        parseContext.declareVariable((yyvsp[(2) - (2)].lex).loc, *(yyvsp[(2) - (2)].lex).string, (yyvsp[(1) - (2)].interm.type));
    }
    break;

  case 118:
/* Line 1792 of yacc.c  */
#line 932 "glslang.y"
    {
        (yyval.interm).type = (yyvsp[(1) - (3)].interm.type);
        (yyval.interm).intermNode = 0;
        parseContext.declareVariable((yyvsp[(2) - (3)].lex).loc, *(yyvsp[(2) - (3)].lex).string, (yyvsp[(1) - (3)].interm.type), (yyvsp[(3) - (3)].interm).arraySizes);
    }
    break;

  case 119:
/* Line 1792 of yacc.c  */
#line 937 "glslang.y"
    {
        (yyval.interm).type = (yyvsp[(1) - (5)].interm.type);
        TIntermNode* initNode = parseContext.declareVariable((yyvsp[(2) - (5)].lex).loc, *(yyvsp[(2) - (5)].lex).string, (yyvsp[(1) - (5)].interm.type), (yyvsp[(3) - (5)].interm).arraySizes, (yyvsp[(5) - (5)].interm.intermTypedNode));
        (yyval.interm).intermNode = parseContext.intermediate.growAggregate(0, initNode, (yyvsp[(4) - (5)].lex).loc);
    }
    break;

  case 120:
/* Line 1792 of yacc.c  */
#line 942 "glslang.y"
    {
        (yyval.interm).type = (yyvsp[(1) - (4)].interm.type);
        TIntermNode* initNode = parseContext.declareVariable((yyvsp[(2) - (4)].lex).loc, *(yyvsp[(2) - (4)].lex).string, (yyvsp[(1) - (4)].interm.type), 0, (yyvsp[(4) - (4)].interm.intermTypedNode));
        (yyval.interm).intermNode = parseContext.intermediate.growAggregate(0, initNode, (yyvsp[(3) - (4)].lex).loc);
    }
    break;

  case 121:
/* Line 1792 of yacc.c  */
#line 951 "glslang.y"
    {
        (yyval.interm.type) = (yyvsp[(1) - (1)].interm.type);

        parseContext.globalQualifierTypeCheck((yyvsp[(1) - (1)].interm.type).loc, (yyvsp[(1) - (1)].interm.type).qualifier, (yyval.interm.type));
        if ((yyvsp[(1) - (1)].interm.type).arraySizes) {
            parseContext.profileRequires((yyvsp[(1) - (1)].interm.type).loc, ENoProfile, 120, E_GL_3DL_array_objects, "arrayed type");
            parseContext.profileRequires((yyvsp[(1) - (1)].interm.type).loc, EEsProfile, 300, 0, "arrayed type");
        }

        parseContext.precisionQualifierCheck((yyval.interm.type).loc, (yyval.interm.type).basicType, (yyval.interm.type).qualifier);
    }
    break;

  case 122:
/* Line 1792 of yacc.c  */
#line 962 "glslang.y"
    {
        parseContext.globalQualifierFixCheck((yyvsp[(1) - (2)].interm.type).loc, (yyvsp[(1) - (2)].interm.type).qualifier);
        parseContext.globalQualifierTypeCheck((yyvsp[(1) - (2)].interm.type).loc, (yyvsp[(1) - (2)].interm.type).qualifier, (yyvsp[(2) - (2)].interm.type));

        if ((yyvsp[(2) - (2)].interm.type).arraySizes) {
            parseContext.profileRequires((yyvsp[(2) - (2)].interm.type).loc, ENoProfile, 120, E_GL_3DL_array_objects, "arrayed type");
            parseContext.profileRequires((yyvsp[(2) - (2)].interm.type).loc, EEsProfile, 300, 0, "arrayed type");
        }

        if ((yyvsp[(2) - (2)].interm.type).arraySizes && parseContext.arrayQualifierError((yyvsp[(2) - (2)].interm.type).loc, (yyvsp[(1) - (2)].interm.type).qualifier))
            (yyvsp[(2) - (2)].interm.type).arraySizes = 0;

        parseContext.checkNoShaderLayouts((yyvsp[(2) - (2)].interm.type).loc, (yyvsp[(1) - (2)].interm.type).shaderQualifiers);
        (yyvsp[(2) - (2)].interm.type).shaderQualifiers.merge((yyvsp[(1) - (2)].interm.type).shaderQualifiers);
        parseContext.mergeQualifiers((yyvsp[(2) - (2)].interm.type).loc, (yyvsp[(2) - (2)].interm.type).qualifier, (yyvsp[(1) - (2)].interm.type).qualifier, true);
        parseContext.precisionQualifierCheck((yyvsp[(2) - (2)].interm.type).loc, (yyvsp[(2) - (2)].interm.type).basicType, (yyvsp[(2) - (2)].interm.type).qualifier);

        (yyval.interm.type) = (yyvsp[(2) - (2)].interm.type);

        if (! (yyval.interm.type).qualifier.isInterpolation() &&
            ((parseContext.language == EShLangVertex   && (yyval.interm.type).qualifier.storage == EvqVaryingOut) ||
             (parseContext.language == EShLangFragment && (yyval.interm.type).qualifier.storage == EvqVaryingIn)))
            (yyval.interm.type).qualifier.smooth = true;
    }
    break;

  case 123:
/* Line 1792 of yacc.c  */
#line 989 "glslang.y"
    {
        parseContext.globalCheck((yyvsp[(1) - (1)].lex).loc, "invariant");
        parseContext.profileRequires((yyval.interm.type).loc, ENoProfile, 120, 0, "invariant");
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc);
        (yyval.interm.type).qualifier.invariant = true;
    }
    break;

  case 124:
/* Line 1792 of yacc.c  */
#line 998 "glslang.y"
    {
        parseContext.globalCheck((yyvsp[(1) - (1)].lex).loc, "smooth");
        parseContext.profileRequires((yyvsp[(1) - (1)].lex).loc, ENoProfile, 130, 0, "smooth");
        parseContext.profileRequires((yyvsp[(1) - (1)].lex).loc, EEsProfile, 300, 0, "smooth");
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc);
        (yyval.interm.type).qualifier.smooth = true;
    }
    break;

  case 125:
/* Line 1792 of yacc.c  */
#line 1005 "glslang.y"
    {
        parseContext.globalCheck((yyvsp[(1) - (1)].lex).loc, "flat");
        parseContext.profileRequires((yyvsp[(1) - (1)].lex).loc, ENoProfile, 130, 0, "flat");
        parseContext.profileRequires((yyvsp[(1) - (1)].lex).loc, EEsProfile, 300, 0, "flat");
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc);
        (yyval.interm.type).qualifier.flat = true;
    }
    break;

  case 126:
/* Line 1792 of yacc.c  */
#line 1012 "glslang.y"
    {
        parseContext.globalCheck((yyvsp[(1) - (1)].lex).loc, "noperspective");
        parseContext.requireProfile((yyvsp[(1) - (1)].lex).loc, ~EEsProfile, "noperspective");
        parseContext.profileRequires((yyvsp[(1) - (1)].lex).loc, ENoProfile, 130, 0, "noperspective");
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc);
        (yyval.interm.type).qualifier.nopersp = true;
    }
    break;

  case 127:
/* Line 1792 of yacc.c  */
#line 1022 "glslang.y"
    {
        (yyval.interm.type) = (yyvsp[(3) - (4)].interm.type);
    }
    break;

  case 128:
/* Line 1792 of yacc.c  */
#line 1028 "glslang.y"
    {
        (yyval.interm.type) = (yyvsp[(1) - (1)].interm.type);
    }
    break;

  case 129:
/* Line 1792 of yacc.c  */
#line 1031 "glslang.y"
    {
        (yyval.interm.type) = (yyvsp[(1) - (3)].interm.type);
        (yyval.interm.type).shaderQualifiers.merge((yyvsp[(3) - (3)].interm.type).shaderQualifiers);
        parseContext.mergeObjectLayoutQualifiers((yyval.interm.type).qualifier, (yyvsp[(3) - (3)].interm.type).qualifier, false);
    }
    break;

  case 130:
/* Line 1792 of yacc.c  */
#line 1038 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc);
        parseContext.setLayoutQualifier((yyvsp[(1) - (1)].lex).loc, (yyval.interm.type), *(yyvsp[(1) - (1)].lex).string);
    }
    break;

  case 131:
/* Line 1792 of yacc.c  */
#line 1042 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (3)].lex).loc);
        parseContext.setLayoutQualifier((yyvsp[(1) - (3)].lex).loc, (yyval.interm.type), *(yyvsp[(1) - (3)].lex).string, (yyvsp[(3) - (3)].interm.intermTypedNode));
    }
    break;

  case 132:
/* Line 1792 of yacc.c  */
#line 1046 "glslang.y"
    { // because "shared" is both an identifier and a keyword
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc);
        TString strShared("shared");
        parseContext.setLayoutQualifier((yyvsp[(1) - (1)].lex).loc, (yyval.interm.type), strShared);
    }
    break;

  case 133:
/* Line 1792 of yacc.c  */
#line 1054 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc);
    }
    break;

  case 134:
/* Line 1792 of yacc.c  */
#line 1060 "glslang.y"
    {
        (yyval.interm.type) = (yyvsp[(1) - (1)].interm.type);
    }
    break;

  case 135:
/* Line 1792 of yacc.c  */
#line 1063 "glslang.y"
    {
        (yyval.interm.type) = (yyvsp[(1) - (2)].interm.type);
        if ((yyval.interm.type).basicType == EbtVoid)
            (yyval.interm.type).basicType = (yyvsp[(2) - (2)].interm.type).basicType;

        (yyval.interm.type).shaderQualifiers.merge((yyvsp[(2) - (2)].interm.type).shaderQualifiers);
        parseContext.mergeQualifiers((yyval.interm.type).loc, (yyval.interm.type).qualifier, (yyvsp[(2) - (2)].interm.type).qualifier, false);
    }
    break;

  case 136:
/* Line 1792 of yacc.c  */
#line 1074 "glslang.y"
    {
        (yyval.interm.type) = (yyvsp[(1) - (1)].interm.type);
    }
    break;

  case 137:
/* Line 1792 of yacc.c  */
#line 1077 "glslang.y"
    {
        (yyval.interm.type) = (yyvsp[(1) - (1)].interm.type);
    }
    break;

  case 138:
/* Line 1792 of yacc.c  */
#line 1080 "glslang.y"
    {
        (yyval.interm.type) = (yyvsp[(1) - (1)].interm.type);
    }
    break;

  case 139:
/* Line 1792 of yacc.c  */
#line 1083 "glslang.y"
    {
        // allow inheritance of storage qualifier from block declaration
        (yyval.interm.type) = (yyvsp[(1) - (1)].interm.type);
    }
    break;

  case 140:
/* Line 1792 of yacc.c  */
#line 1087 "glslang.y"
    {
        // allow inheritance of storage qualifier from block declaration
        (yyval.interm.type) = (yyvsp[(1) - (1)].interm.type);
    }
    break;

  case 141:
/* Line 1792 of yacc.c  */
#line 1091 "glslang.y"
    {
        // allow inheritance of storage qualifier from block declaration
        (yyval.interm.type) = (yyvsp[(1) - (1)].interm.type);
    }
    break;

  case 142:
/* Line 1792 of yacc.c  */
#line 1098 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc);
        (yyval.interm.type).qualifier.storage = EvqConst;  // will later turn into EvqConstReadOnly, if the initializer is not constant
    }
    break;

  case 143:
/* Line 1792 of yacc.c  */
#line 1102 "glslang.y"
    {
        parseContext.requireStage((yyvsp[(1) - (1)].lex).loc, EShLangVertex, "attribute");
        parseContext.checkDeprecated((yyvsp[(1) - (1)].lex).loc, ECoreProfile, 130, "attribute");
        parseContext.checkDeprecated((yyvsp[(1) - (1)].lex).loc, ENoProfile, 130, "attribute");
        parseContext.requireNotRemoved((yyvsp[(1) - (1)].lex).loc, ECoreProfile, 420, "attribute");
        parseContext.requireNotRemoved((yyvsp[(1) - (1)].lex).loc, EEsProfile, 300, "attribute");

        parseContext.globalCheck((yyvsp[(1) - (1)].lex).loc, "attribute");

        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc);
        (yyval.interm.type).qualifier.storage = EvqVaryingIn;
    }
    break;

  case 144:
/* Line 1792 of yacc.c  */
#line 1114 "glslang.y"
    {
        parseContext.checkDeprecated((yyvsp[(1) - (1)].lex).loc, ENoProfile, 130, "varying");
        parseContext.checkDeprecated((yyvsp[(1) - (1)].lex).loc, ECoreProfile, 130, "varying");
        parseContext.requireNotRemoved((yyvsp[(1) - (1)].lex).loc, ECoreProfile, 420, "varying");
        parseContext.requireNotRemoved((yyvsp[(1) - (1)].lex).loc, EEsProfile, 300, "varying");

        parseContext.globalCheck((yyvsp[(1) - (1)].lex).loc, "varying");

        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc);
        if (parseContext.language == EShLangVertex)
            (yyval.interm.type).qualifier.storage = EvqVaryingOut;
        else
            (yyval.interm.type).qualifier.storage = EvqVaryingIn;
    }
    break;

  case 145:
/* Line 1792 of yacc.c  */
#line 1128 "glslang.y"
    {
        parseContext.globalCheck((yyvsp[(1) - (1)].lex).loc, "inout");
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc);
        (yyval.interm.type).qualifier.storage = EvqInOut;
    }
    break;

  case 146:
/* Line 1792 of yacc.c  */
#line 1133 "glslang.y"
    {
        parseContext.globalCheck((yyvsp[(1) - (1)].lex).loc, "in");
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc);
        // whether this is a parameter "in" or a pipeline "in" will get sorted out a bit later
        (yyval.interm.type).qualifier.storage = EvqIn;
    }
    break;

  case 147:
/* Line 1792 of yacc.c  */
#line 1139 "glslang.y"
    {
        parseContext.globalCheck((yyvsp[(1) - (1)].lex).loc, "out");
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc);
        // whether this is a parameter "out" or a pipeline "out" will get sorted out a bit later
        (yyval.interm.type).qualifier.storage = EvqOut;
    }
    break;

  case 148:
/* Line 1792 of yacc.c  */
#line 1145 "glslang.y"
    {
        parseContext.profileRequires((yyvsp[(1) - (1)].lex).loc, ENoProfile, 120, 0, "centroid");
        parseContext.profileRequires((yyvsp[(1) - (1)].lex).loc, EEsProfile, 300, 0, "centroid");
        parseContext.globalCheck((yyvsp[(1) - (1)].lex).loc, "centroid");
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc);
        (yyval.interm.type).qualifier.centroid = true;
    }
    break;

  case 149:
/* Line 1792 of yacc.c  */
#line 1152 "glslang.y"
    {
        parseContext.globalCheck((yyvsp[(1) - (1)].lex).loc, "patch");
        parseContext.requireStage((yyvsp[(1) - (1)].lex).loc, (EShLanguageMask)(EShLangTessControlMask | EShLangTessEvaluationMask), "patch");
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc);
        (yyval.interm.type).qualifier.patch = true;
    }
    break;

  case 150:
/* Line 1792 of yacc.c  */
#line 1158 "glslang.y"
    {
        parseContext.globalCheck((yyvsp[(1) - (1)].lex).loc, "sample");
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc);
        (yyval.interm.type).qualifier.sample = true;
    }
    break;

  case 151:
/* Line 1792 of yacc.c  */
#line 1163 "glslang.y"
    {
        parseContext.globalCheck((yyvsp[(1) - (1)].lex).loc, "uniform");
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc);
        (yyval.interm.type).qualifier.storage = EvqUniform;
    }
    break;

  case 152:
/* Line 1792 of yacc.c  */
#line 1168 "glslang.y"
    {
        parseContext.globalCheck((yyvsp[(1) - (1)].lex).loc, "buffer");
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc);
        (yyval.interm.type).qualifier.storage = EvqBuffer;
    }
    break;

  case 153:
/* Line 1792 of yacc.c  */
#line 1173 "glslang.y"
    {
        parseContext.profileRequires((yyvsp[(1) - (1)].lex).loc, ECoreProfile | ECompatibilityProfile, 430, 0, "shared");
        parseContext.profileRequires((yyvsp[(1) - (1)].lex).loc, EEsProfile, 310, 0, "shared");
        parseContext.requireStage((yyvsp[(1) - (1)].lex).loc, EShLangCompute, "shared");
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc);
        (yyval.interm.type).qualifier.storage = EvqShared;
    }
    break;

  case 154:
/* Line 1792 of yacc.c  */
#line 1180 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc);
        (yyval.interm.type).qualifier.coherent = true;
    }
    break;

  case 155:
/* Line 1792 of yacc.c  */
#line 1184 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc);
        (yyval.interm.type).qualifier.volatil = true;
    }
    break;

  case 156:
/* Line 1792 of yacc.c  */
#line 1188 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc);
        (yyval.interm.type).qualifier.restrict = true;
    }
    break;

  case 157:
/* Line 1792 of yacc.c  */
#line 1192 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc);
        (yyval.interm.type).qualifier.readonly = true;
    }
    break;

  case 158:
/* Line 1792 of yacc.c  */
#line 1196 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc);
        (yyval.interm.type).qualifier.writeonly = true;
    }
    break;

  case 159:
/* Line 1792 of yacc.c  */
#line 1200 "glslang.y"
    {
        parseContext.globalCheck((yyvsp[(1) - (1)].lex).loc, "subroutine");
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc);
        (yyval.interm.type).qualifier.storage = EvqUniform;
    }
    break;

  case 160:
/* Line 1792 of yacc.c  */
#line 1205 "glslang.y"
    {
        parseContext.globalCheck((yyvsp[(1) - (4)].lex).loc, "subroutine");
        (yyval.interm.type).init((yyvsp[(1) - (4)].lex).loc);
        (yyval.interm.type).qualifier.storage = EvqUniform;
        // TODO: 4.0 semantics: subroutines
        // 1) make sure each identifier is a type declared earlier with SUBROUTINE
        // 2) save all of the identifiers for future comparison with the declared function
    }
    break;

  case 161:
/* Line 1792 of yacc.c  */
#line 1216 "glslang.y"
    {
        // TODO: 4.0 functionality: subroutine type to list
    }
    break;

  case 162:
/* Line 1792 of yacc.c  */
#line 1219 "glslang.y"
    {
    }
    break;

  case 163:
/* Line 1792 of yacc.c  */
#line 1224 "glslang.y"
    {
        (yyval.interm.type) = (yyvsp[(1) - (1)].interm.type);
        (yyval.interm.type).qualifier.precision = parseContext.getDefaultPrecision((yyval.interm.type));
    }
    break;

  case 164:
/* Line 1792 of yacc.c  */
#line 1228 "glslang.y"
    {
        parseContext.arrayDimCheck((yyvsp[(2) - (2)].interm).loc, (yyvsp[(2) - (2)].interm).arraySizes, 0);
        (yyval.interm.type) = (yyvsp[(1) - (2)].interm.type);
        (yyval.interm.type).qualifier.precision = parseContext.getDefaultPrecision((yyval.interm.type));
        (yyval.interm.type).arraySizes = (yyvsp[(2) - (2)].interm).arraySizes;
    }
    break;

  case 165:
/* Line 1792 of yacc.c  */
#line 1237 "glslang.y"
    {
        (yyval.interm).loc = (yyvsp[(1) - (2)].lex).loc;
        (yyval.interm).arraySizes = new TArraySizes;
        (yyval.interm).arraySizes->setOuterSize(0);
    }
    break;

  case 166:
/* Line 1792 of yacc.c  */
#line 1242 "glslang.y"
    {
        (yyval.interm).loc = (yyvsp[(1) - (3)].lex).loc;
        (yyval.interm).arraySizes = new TArraySizes;

        int size;
        parseContext.arraySizeCheck((yyvsp[(2) - (3)].interm.intermTypedNode)->getLoc(), (yyvsp[(2) - (3)].interm.intermTypedNode), size);
        (yyval.interm).arraySizes->setOuterSize(size);
    }
    break;

  case 167:
/* Line 1792 of yacc.c  */
#line 1250 "glslang.y"
    {
        (yyval.interm) = (yyvsp[(1) - (3)].interm);
        (yyval.interm).arraySizes->setOuterSize(0);
    }
    break;

  case 168:
/* Line 1792 of yacc.c  */
#line 1254 "glslang.y"
    {
        (yyval.interm) = (yyvsp[(1) - (4)].interm);

        int size;
        parseContext.arraySizeCheck((yyvsp[(3) - (4)].interm.intermTypedNode)->getLoc(), (yyvsp[(3) - (4)].interm.intermTypedNode), size);
        (yyval.interm).arraySizes->setOuterSize(size);
    }
    break;

  case 169:
/* Line 1792 of yacc.c  */
#line 1264 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtVoid;
    }
    break;

  case 170:
/* Line 1792 of yacc.c  */
#line 1268 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtFloat;
    }
    break;

  case 171:
/* Line 1792 of yacc.c  */
#line 1272 "glslang.y"
    {
        parseContext.doubleCheck((yyvsp[(1) - (1)].lex).loc, "double");
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtDouble;
    }
    break;

  case 172:
/* Line 1792 of yacc.c  */
#line 1277 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtInt;
    }
    break;

  case 173:
/* Line 1792 of yacc.c  */
#line 1281 "glslang.y"
    {
        parseContext.fullIntegerCheck((yyvsp[(1) - (1)].lex).loc, "unsigned integer");
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtUint;
    }
    break;

  case 174:
/* Line 1792 of yacc.c  */
#line 1286 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtBool;
    }
    break;

  case 175:
/* Line 1792 of yacc.c  */
#line 1290 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtFloat;
        (yyval.interm.type).setVector(2);
    }
    break;

  case 176:
/* Line 1792 of yacc.c  */
#line 1295 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtFloat;
        (yyval.interm.type).setVector(3);
    }
    break;

  case 177:
/* Line 1792 of yacc.c  */
#line 1300 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtFloat;
        (yyval.interm.type).setVector(4);
    }
    break;

  case 178:
/* Line 1792 of yacc.c  */
#line 1305 "glslang.y"
    {
        parseContext.doubleCheck((yyvsp[(1) - (1)].lex).loc, "double vector");
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtDouble;
        (yyval.interm.type).setVector(2);
    }
    break;

  case 179:
/* Line 1792 of yacc.c  */
#line 1311 "glslang.y"
    {
        parseContext.doubleCheck((yyvsp[(1) - (1)].lex).loc, "double vector");
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtDouble;
        (yyval.interm.type).setVector(3);
    }
    break;

  case 180:
/* Line 1792 of yacc.c  */
#line 1317 "glslang.y"
    {
        parseContext.doubleCheck((yyvsp[(1) - (1)].lex).loc, "double vector");
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtDouble;
        (yyval.interm.type).setVector(4);
    }
    break;

  case 181:
/* Line 1792 of yacc.c  */
#line 1323 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtBool;
        (yyval.interm.type).setVector(2);
    }
    break;

  case 182:
/* Line 1792 of yacc.c  */
#line 1328 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtBool;
        (yyval.interm.type).setVector(3);
    }
    break;

  case 183:
/* Line 1792 of yacc.c  */
#line 1333 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtBool;
        (yyval.interm.type).setVector(4);
    }
    break;

  case 184:
/* Line 1792 of yacc.c  */
#line 1338 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtInt;
        (yyval.interm.type).setVector(2);
    }
    break;

  case 185:
/* Line 1792 of yacc.c  */
#line 1343 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtInt;
        (yyval.interm.type).setVector(3);
    }
    break;

  case 186:
/* Line 1792 of yacc.c  */
#line 1348 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtInt;
        (yyval.interm.type).setVector(4);
    }
    break;

  case 187:
/* Line 1792 of yacc.c  */
#line 1353 "glslang.y"
    {
        parseContext.fullIntegerCheck((yyvsp[(1) - (1)].lex).loc, "unsigned integer vector");
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtUint;
        (yyval.interm.type).setVector(2);
    }
    break;

  case 188:
/* Line 1792 of yacc.c  */
#line 1359 "glslang.y"
    {
        parseContext.fullIntegerCheck((yyvsp[(1) - (1)].lex).loc, "unsigned integer vector");
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtUint;
        (yyval.interm.type).setVector(3);
    }
    break;

  case 189:
/* Line 1792 of yacc.c  */
#line 1365 "glslang.y"
    {
        parseContext.fullIntegerCheck((yyvsp[(1) - (1)].lex).loc, "unsigned integer vector");
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtUint;
        (yyval.interm.type).setVector(4);
    }
    break;

  case 190:
/* Line 1792 of yacc.c  */
#line 1371 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtFloat;
        (yyval.interm.type).setMatrix(2, 2);
    }
    break;

  case 191:
/* Line 1792 of yacc.c  */
#line 1376 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtFloat;
        (yyval.interm.type).setMatrix(3, 3);
    }
    break;

  case 192:
/* Line 1792 of yacc.c  */
#line 1381 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtFloat;
        (yyval.interm.type).setMatrix(4, 4);
    }
    break;

  case 193:
/* Line 1792 of yacc.c  */
#line 1386 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtFloat;
        (yyval.interm.type).setMatrix(2, 2);
    }
    break;

  case 194:
/* Line 1792 of yacc.c  */
#line 1391 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtFloat;
        (yyval.interm.type).setMatrix(2, 3);
    }
    break;

  case 195:
/* Line 1792 of yacc.c  */
#line 1396 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtFloat;
        (yyval.interm.type).setMatrix(2, 4);
    }
    break;

  case 196:
/* Line 1792 of yacc.c  */
#line 1401 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtFloat;
        (yyval.interm.type).setMatrix(3, 2);
    }
    break;

  case 197:
/* Line 1792 of yacc.c  */
#line 1406 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtFloat;
        (yyval.interm.type).setMatrix(3, 3);
    }
    break;

  case 198:
/* Line 1792 of yacc.c  */
#line 1411 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtFloat;
        (yyval.interm.type).setMatrix(3, 4);
    }
    break;

  case 199:
/* Line 1792 of yacc.c  */
#line 1416 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtFloat;
        (yyval.interm.type).setMatrix(4, 2);
    }
    break;

  case 200:
/* Line 1792 of yacc.c  */
#line 1421 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtFloat;
        (yyval.interm.type).setMatrix(4, 3);
    }
    break;

  case 201:
/* Line 1792 of yacc.c  */
#line 1426 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtFloat;
        (yyval.interm.type).setMatrix(4, 4);
    }
    break;

  case 202:
/* Line 1792 of yacc.c  */
#line 1431 "glslang.y"
    {
        parseContext.doubleCheck((yyvsp[(1) - (1)].lex).loc, "double matrix");
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtDouble;
        (yyval.interm.type).setMatrix(2, 2);
    }
    break;

  case 203:
/* Line 1792 of yacc.c  */
#line 1437 "glslang.y"
    {
        parseContext.doubleCheck((yyvsp[(1) - (1)].lex).loc, "double matrix");
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtDouble;
        (yyval.interm.type).setMatrix(3, 3);
    }
    break;

  case 204:
/* Line 1792 of yacc.c  */
#line 1443 "glslang.y"
    {
        parseContext.doubleCheck((yyvsp[(1) - (1)].lex).loc, "double matrix");
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtDouble;
        (yyval.interm.type).setMatrix(4, 4);
    }
    break;

  case 205:
/* Line 1792 of yacc.c  */
#line 1449 "glslang.y"
    {
        parseContext.doubleCheck((yyvsp[(1) - (1)].lex).loc, "double matrix");
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtDouble;
        (yyval.interm.type).setMatrix(2, 2);
    }
    break;

  case 206:
/* Line 1792 of yacc.c  */
#line 1455 "glslang.y"
    {
        parseContext.doubleCheck((yyvsp[(1) - (1)].lex).loc, "double matrix");
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtDouble;
        (yyval.interm.type).setMatrix(2, 3);
    }
    break;

  case 207:
/* Line 1792 of yacc.c  */
#line 1461 "glslang.y"
    {
        parseContext.doubleCheck((yyvsp[(1) - (1)].lex).loc, "double matrix");
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtDouble;
        (yyval.interm.type).setMatrix(2, 4);
    }
    break;

  case 208:
/* Line 1792 of yacc.c  */
#line 1467 "glslang.y"
    {
        parseContext.doubleCheck((yyvsp[(1) - (1)].lex).loc, "double matrix");
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtDouble;
        (yyval.interm.type).setMatrix(3, 2);
    }
    break;

  case 209:
/* Line 1792 of yacc.c  */
#line 1473 "glslang.y"
    {
        parseContext.doubleCheck((yyvsp[(1) - (1)].lex).loc, "double matrix");
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtDouble;
        (yyval.interm.type).setMatrix(3, 3);
    }
    break;

  case 210:
/* Line 1792 of yacc.c  */
#line 1479 "glslang.y"
    {
        parseContext.doubleCheck((yyvsp[(1) - (1)].lex).loc, "double matrix");
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtDouble;
        (yyval.interm.type).setMatrix(3, 4);
    }
    break;

  case 211:
/* Line 1792 of yacc.c  */
#line 1485 "glslang.y"
    {
        parseContext.doubleCheck((yyvsp[(1) - (1)].lex).loc, "double matrix");
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtDouble;
        (yyval.interm.type).setMatrix(4, 2);
    }
    break;

  case 212:
/* Line 1792 of yacc.c  */
#line 1491 "glslang.y"
    {
        parseContext.doubleCheck((yyvsp[(1) - (1)].lex).loc, "double matrix");
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtDouble;
        (yyval.interm.type).setMatrix(4, 3);
    }
    break;

  case 213:
/* Line 1792 of yacc.c  */
#line 1497 "glslang.y"
    {
        parseContext.doubleCheck((yyvsp[(1) - (1)].lex).loc, "double matrix");
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtDouble;
        (yyval.interm.type).setMatrix(4, 4);
    }
    break;

  case 214:
/* Line 1792 of yacc.c  */
#line 1503 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtAtomicUint;
    }
    break;

  case 215:
/* Line 1792 of yacc.c  */
#line 1507 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtFloat, Esd1D);
    }
    break;

  case 216:
/* Line 1792 of yacc.c  */
#line 1512 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtFloat, Esd2D);
    }
    break;

  case 217:
/* Line 1792 of yacc.c  */
#line 1517 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtFloat, Esd3D);
    }
    break;

  case 218:
/* Line 1792 of yacc.c  */
#line 1522 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtFloat, EsdCube);
    }
    break;

  case 219:
/* Line 1792 of yacc.c  */
#line 1527 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtFloat, Esd1D, false, true);
    }
    break;

  case 220:
/* Line 1792 of yacc.c  */
#line 1532 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtFloat, Esd2D, false, true);
    }
    break;

  case 221:
/* Line 1792 of yacc.c  */
#line 1537 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtFloat, EsdCube, false, true);
    }
    break;

  case 222:
/* Line 1792 of yacc.c  */
#line 1542 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtFloat, Esd1D, true);
    }
    break;

  case 223:
/* Line 1792 of yacc.c  */
#line 1547 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtFloat, Esd2D, true);
    }
    break;

  case 224:
/* Line 1792 of yacc.c  */
#line 1552 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtFloat, Esd1D, true, true);
    }
    break;

  case 225:
/* Line 1792 of yacc.c  */
#line 1557 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtFloat, Esd2D, true, true);
    }
    break;

  case 226:
/* Line 1792 of yacc.c  */
#line 1562 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtFloat, EsdCube, true);
    }
    break;

  case 227:
/* Line 1792 of yacc.c  */
#line 1567 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtFloat, EsdCube, true, true);
    }
    break;

  case 228:
/* Line 1792 of yacc.c  */
#line 1572 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtInt, Esd1D);
    }
    break;

  case 229:
/* Line 1792 of yacc.c  */
#line 1577 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtInt, Esd2D);
    }
    break;

  case 230:
/* Line 1792 of yacc.c  */
#line 1582 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtInt, Esd3D);
    }
    break;

  case 231:
/* Line 1792 of yacc.c  */
#line 1587 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtInt, EsdCube);
    }
    break;

  case 232:
/* Line 1792 of yacc.c  */
#line 1592 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtInt, Esd1D, true);
    }
    break;

  case 233:
/* Line 1792 of yacc.c  */
#line 1597 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtInt, Esd2D, true);
    }
    break;

  case 234:
/* Line 1792 of yacc.c  */
#line 1602 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtInt, EsdCube, true);
    }
    break;

  case 235:
/* Line 1792 of yacc.c  */
#line 1607 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtUint, Esd1D);
    }
    break;

  case 236:
/* Line 1792 of yacc.c  */
#line 1612 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtUint, Esd2D);
    }
    break;

  case 237:
/* Line 1792 of yacc.c  */
#line 1617 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtUint, Esd3D);
    }
    break;

  case 238:
/* Line 1792 of yacc.c  */
#line 1622 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtUint, EsdCube);
    }
    break;

  case 239:
/* Line 1792 of yacc.c  */
#line 1627 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtUint, Esd1D, true);
    }
    break;

  case 240:
/* Line 1792 of yacc.c  */
#line 1632 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtUint, Esd2D, true);
    }
    break;

  case 241:
/* Line 1792 of yacc.c  */
#line 1637 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtUint, EsdCube, true);
    }
    break;

  case 242:
/* Line 1792 of yacc.c  */
#line 1642 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtFloat, EsdRect);
    }
    break;

  case 243:
/* Line 1792 of yacc.c  */
#line 1647 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtFloat, EsdRect, false, true);
    }
    break;

  case 244:
/* Line 1792 of yacc.c  */
#line 1652 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtInt, EsdRect);
    }
    break;

  case 245:
/* Line 1792 of yacc.c  */
#line 1657 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtUint, EsdRect);
    }
    break;

  case 246:
/* Line 1792 of yacc.c  */
#line 1662 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtFloat, EsdBuffer);
    }
    break;

  case 247:
/* Line 1792 of yacc.c  */
#line 1667 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtInt, EsdBuffer);
    }
    break;

  case 248:
/* Line 1792 of yacc.c  */
#line 1672 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtUint, EsdBuffer);
    }
    break;

  case 249:
/* Line 1792 of yacc.c  */
#line 1677 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtFloat, Esd2D, false, false, true);
    }
    break;

  case 250:
/* Line 1792 of yacc.c  */
#line 1682 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtInt, Esd2D, false, false, true);
    }
    break;

  case 251:
/* Line 1792 of yacc.c  */
#line 1687 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtUint, Esd2D, false, false, true);
    }
    break;

  case 252:
/* Line 1792 of yacc.c  */
#line 1692 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtFloat, Esd2D, true, false, true);
    }
    break;

  case 253:
/* Line 1792 of yacc.c  */
#line 1697 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtInt, Esd2D, true, false, true);
    }
    break;

  case 254:
/* Line 1792 of yacc.c  */
#line 1702 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtUint, Esd2D, true, false, true);
    }
    break;

  case 255:
/* Line 1792 of yacc.c  */
#line 1707 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.setImage(EbtFloat, Esd1D);
    }
    break;

  case 256:
/* Line 1792 of yacc.c  */
#line 1712 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.setImage(EbtInt, Esd1D);
    }
    break;

  case 257:
/* Line 1792 of yacc.c  */
#line 1717 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.setImage(EbtUint, Esd1D);
    }
    break;

  case 258:
/* Line 1792 of yacc.c  */
#line 1722 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.setImage(EbtFloat, Esd2D);
    }
    break;

  case 259:
/* Line 1792 of yacc.c  */
#line 1727 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.setImage(EbtInt, Esd2D);
    }
    break;

  case 260:
/* Line 1792 of yacc.c  */
#line 1732 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.setImage(EbtUint, Esd2D);
    }
    break;

  case 261:
/* Line 1792 of yacc.c  */
#line 1737 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.setImage(EbtFloat, Esd3D);
    }
    break;

  case 262:
/* Line 1792 of yacc.c  */
#line 1742 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.setImage(EbtInt, Esd3D);
    }
    break;

  case 263:
/* Line 1792 of yacc.c  */
#line 1747 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.setImage(EbtUint, Esd3D);
    }
    break;

  case 264:
/* Line 1792 of yacc.c  */
#line 1752 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.setImage(EbtFloat, EsdRect);
    }
    break;

  case 265:
/* Line 1792 of yacc.c  */
#line 1757 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.setImage(EbtInt, EsdRect);
    }
    break;

  case 266:
/* Line 1792 of yacc.c  */
#line 1762 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.setImage(EbtUint, EsdRect);
    }
    break;

  case 267:
/* Line 1792 of yacc.c  */
#line 1767 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.setImage(EbtFloat, EsdCube);
    }
    break;

  case 268:
/* Line 1792 of yacc.c  */
#line 1772 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.setImage(EbtInt, EsdCube);
    }
    break;

  case 269:
/* Line 1792 of yacc.c  */
#line 1777 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.setImage(EbtUint, EsdCube);
    }
    break;

  case 270:
/* Line 1792 of yacc.c  */
#line 1782 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.setImage(EbtFloat, EsdBuffer);
    }
    break;

  case 271:
/* Line 1792 of yacc.c  */
#line 1787 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.setImage(EbtInt, EsdBuffer);
    }
    break;

  case 272:
/* Line 1792 of yacc.c  */
#line 1792 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.setImage(EbtUint, EsdBuffer);
    }
    break;

  case 273:
/* Line 1792 of yacc.c  */
#line 1797 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.setImage(EbtFloat, Esd1D, true);
    }
    break;

  case 274:
/* Line 1792 of yacc.c  */
#line 1802 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.setImage(EbtInt, Esd1D, true);
    }
    break;

  case 275:
/* Line 1792 of yacc.c  */
#line 1807 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.setImage(EbtUint, Esd1D, true);
    }
    break;

  case 276:
/* Line 1792 of yacc.c  */
#line 1812 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.setImage(EbtFloat, Esd2D, true);
    }
    break;

  case 277:
/* Line 1792 of yacc.c  */
#line 1817 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.setImage(EbtInt, Esd2D, true);
    }
    break;

  case 278:
/* Line 1792 of yacc.c  */
#line 1822 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.setImage(EbtUint, Esd2D, true);
    }
    break;

  case 279:
/* Line 1792 of yacc.c  */
#line 1827 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.setImage(EbtFloat, EsdCube, true);
    }
    break;

  case 280:
/* Line 1792 of yacc.c  */
#line 1832 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.setImage(EbtInt, EsdCube, true);
    }
    break;

  case 281:
/* Line 1792 of yacc.c  */
#line 1837 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.setImage(EbtUint, EsdCube, true);
    }
    break;

  case 282:
/* Line 1792 of yacc.c  */
#line 1842 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.setImage(EbtFloat, Esd2D, false, false, true);
    }
    break;

  case 283:
/* Line 1792 of yacc.c  */
#line 1847 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.setImage(EbtInt, Esd2D, false, false, true);
    }
    break;

  case 284:
/* Line 1792 of yacc.c  */
#line 1852 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.setImage(EbtUint, Esd2D, false, false, true);
    }
    break;

  case 285:
/* Line 1792 of yacc.c  */
#line 1857 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.setImage(EbtFloat, Esd2D, true, false, true);
    }
    break;

  case 286:
/* Line 1792 of yacc.c  */
#line 1862 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.setImage(EbtInt, Esd2D, true, false, true);
    }
    break;

  case 287:
/* Line 1792 of yacc.c  */
#line 1867 "glslang.y"
    {
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.setImage(EbtUint, Esd2D, true, false, true);
    }
    break;

  case 288:
/* Line 1792 of yacc.c  */
#line 1872 "glslang.y"
    {  // GL_OES_EGL_image_external
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        (yyval.interm.type).basicType = EbtSampler;
        (yyval.interm.type).sampler.set(EbtFloat, Esd2D);
        (yyval.interm.type).sampler.external = true;
    }
    break;

  case 289:
/* Line 1792 of yacc.c  */
#line 1878 "glslang.y"
    {
        (yyval.interm.type) = (yyvsp[(1) - (1)].interm.type);
        (yyval.interm.type).qualifier.storage = parseContext.symbolTable.atGlobalLevel() ? EvqGlobal : EvqTemporary;
        parseContext.structTypeCheck((yyval.interm.type).loc, (yyval.interm.type));
    }
    break;

  case 290:
/* Line 1792 of yacc.c  */
#line 1883 "glslang.y"
    {
        //
        // This is for user defined type names.  The lexical phase looked up the
        // type.
        //
        if (const TVariable* variable = ((yyvsp[(1) - (1)].lex).symbol)->getAsVariable()) {
            const TType& structure = variable->getType();
            (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
            (yyval.interm.type).basicType = EbtStruct;
            (yyval.interm.type).userDef = &structure;
        } else
            parseContext.error((yyvsp[(1) - (1)].lex).loc, "expected type name", (yyvsp[(1) - (1)].lex).string->c_str(), "");
    }
    break;

  case 291:
/* Line 1792 of yacc.c  */
#line 1899 "glslang.y"
    {
        parseContext.profileRequires((yyvsp[(1) - (1)].lex).loc, ENoProfile, 130, 0, "highp precision qualifier");
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        if (parseContext.profile == EEsProfile)
		    (yyval.interm.type).qualifier.precision = EpqHigh;
    }
    break;

  case 292:
/* Line 1792 of yacc.c  */
#line 1905 "glslang.y"
    {
        parseContext.profileRequires((yyvsp[(1) - (1)].lex).loc, ENoProfile, 130, 0, "mediump precision qualifier");
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        if (parseContext.profile == EEsProfile)
	    	(yyval.interm.type).qualifier.precision = EpqMedium;
    }
    break;

  case 293:
/* Line 1792 of yacc.c  */
#line 1911 "glslang.y"
    {
        parseContext.profileRequires((yyvsp[(1) - (1)].lex).loc, ENoProfile, 130, 0, "lowp precision qualifier");
        (yyval.interm.type).init((yyvsp[(1) - (1)].lex).loc, parseContext.symbolTable.atGlobalLevel());
        if (parseContext.profile == EEsProfile)
    		(yyval.interm.type).qualifier.precision = EpqLow;
    }
    break;

  case 294:
/* Line 1792 of yacc.c  */
#line 1920 "glslang.y"
    { parseContext.nestedStructCheck((yyvsp[(1) - (3)].lex).loc); }
    break;

  case 295:
/* Line 1792 of yacc.c  */
#line 1920 "glslang.y"
    {
        TType* structure = new TType((yyvsp[(5) - (6)].interm.typeList), *(yyvsp[(2) - (6)].lex).string);
        parseContext.structArrayCheck((yyvsp[(2) - (6)].lex).loc, *structure);
        TVariable* userTypeDef = new TVariable((yyvsp[(2) - (6)].lex).string, *structure, true);
        if (! parseContext.symbolTable.insert(*userTypeDef))
            parseContext.error((yyvsp[(2) - (6)].lex).loc, "redefinition", (yyvsp[(2) - (6)].lex).string->c_str(), "struct");
        (yyval.interm.type).init((yyvsp[(1) - (6)].lex).loc);
        (yyval.interm.type).basicType = EbtStruct;
        (yyval.interm.type).userDef = structure;
        --parseContext.structNestingLevel;
    }
    break;

  case 296:
/* Line 1792 of yacc.c  */
#line 1931 "glslang.y"
    { parseContext.nestedStructCheck((yyvsp[(1) - (2)].lex).loc); }
    break;

  case 297:
/* Line 1792 of yacc.c  */
#line 1931 "glslang.y"
    {
        TType* structure = new TType((yyvsp[(4) - (5)].interm.typeList), TString(""));
        (yyval.interm.type).init((yyvsp[(1) - (5)].lex).loc);
        (yyval.interm.type).basicType = EbtStruct;
        (yyval.interm.type).userDef = structure;
        --parseContext.structNestingLevel;
    }
    break;

  case 298:
/* Line 1792 of yacc.c  */
#line 1941 "glslang.y"
    {
        (yyval.interm.typeList) = (yyvsp[(1) - (1)].interm.typeList);
    }
    break;

  case 299:
/* Line 1792 of yacc.c  */
#line 1944 "glslang.y"
    {
        (yyval.interm.typeList) = (yyvsp[(1) - (2)].interm.typeList);
        for (unsigned int i = 0; i < (yyvsp[(2) - (2)].interm.typeList)->size(); ++i) {
            for (unsigned int j = 0; j < (yyval.interm.typeList)->size(); ++j) {
                if ((*(yyval.interm.typeList))[j].type->getFieldName() == (*(yyvsp[(2) - (2)].interm.typeList))[i].type->getFieldName())
                    parseContext.error((*(yyvsp[(2) - (2)].interm.typeList))[i].loc, "duplicate member name:", "", (*(yyvsp[(2) - (2)].interm.typeList))[i].type->getFieldName().c_str());
            }
            (yyval.interm.typeList)->push_back((*(yyvsp[(2) - (2)].interm.typeList))[i]);
        }
    }
    break;

  case 300:
/* Line 1792 of yacc.c  */
#line 1957 "glslang.y"
    {
        if ((yyvsp[(1) - (3)].interm.type).arraySizes) {
            parseContext.profileRequires((yyvsp[(1) - (3)].interm.type).loc, ENoProfile, 120, E_GL_3DL_array_objects, "arrayed type");
            parseContext.profileRequires((yyvsp[(1) - (3)].interm.type).loc, EEsProfile, 300, 0, "arrayed type");
            if (parseContext.profile == EEsProfile)
                parseContext.arraySizeRequiredCheck((yyvsp[(1) - (3)].interm.type).loc, (yyvsp[(1) - (3)].interm.type).arraySizes->getOuterSize());
        }

        (yyval.interm.typeList) = (yyvsp[(2) - (3)].interm.typeList);

        parseContext.voidErrorCheck((yyvsp[(1) - (3)].interm.type).loc, (*(yyvsp[(2) - (3)].interm.typeList))[0].type->getFieldName(), (yyvsp[(1) - (3)].interm.type).basicType);
        parseContext.precisionQualifierCheck((yyvsp[(1) - (3)].interm.type).loc, (yyvsp[(1) - (3)].interm.type).basicType, (yyvsp[(1) - (3)].interm.type).qualifier);

        for (unsigned int i = 0; i < (yyval.interm.typeList)->size(); ++i) {
            parseContext.arrayDimCheck((yyvsp[(1) - (3)].interm.type).loc, (*(yyval.interm.typeList))[i].type, (yyvsp[(1) - (3)].interm.type).arraySizes);
            (*(yyval.interm.typeList))[i].type->mergeType((yyvsp[(1) - (3)].interm.type));
        }
    }
    break;

  case 301:
/* Line 1792 of yacc.c  */
#line 1975 "glslang.y"
    {
        parseContext.globalQualifierFixCheck((yyvsp[(1) - (4)].interm.type).loc, (yyvsp[(1) - (4)].interm.type).qualifier);
        if ((yyvsp[(2) - (4)].interm.type).arraySizes) {
            parseContext.profileRequires((yyvsp[(2) - (4)].interm.type).loc, ENoProfile, 120, E_GL_3DL_array_objects, "arrayed type");
            parseContext.profileRequires((yyvsp[(2) - (4)].interm.type).loc, EEsProfile, 300, 0, "arrayed type");
            if (parseContext.profile == EEsProfile)
                parseContext.arraySizeRequiredCheck((yyvsp[(2) - (4)].interm.type).loc, (yyvsp[(2) - (4)].interm.type).arraySizes->getOuterSize());
        }

        (yyval.interm.typeList) = (yyvsp[(3) - (4)].interm.typeList);

        parseContext.checkNoShaderLayouts((yyvsp[(1) - (4)].interm.type).loc, (yyvsp[(1) - (4)].interm.type).shaderQualifiers);
        parseContext.voidErrorCheck((yyvsp[(2) - (4)].interm.type).loc, (*(yyvsp[(3) - (4)].interm.typeList))[0].type->getFieldName(), (yyvsp[(2) - (4)].interm.type).basicType);
        parseContext.mergeQualifiers((yyvsp[(2) - (4)].interm.type).loc, (yyvsp[(2) - (4)].interm.type).qualifier, (yyvsp[(1) - (4)].interm.type).qualifier, true);
        parseContext.precisionQualifierCheck((yyvsp[(2) - (4)].interm.type).loc, (yyvsp[(2) - (4)].interm.type).basicType, (yyvsp[(2) - (4)].interm.type).qualifier);

        for (unsigned int i = 0; i < (yyval.interm.typeList)->size(); ++i) {
            parseContext.arrayDimCheck((yyvsp[(1) - (4)].interm.type).loc, (*(yyval.interm.typeList))[i].type, (yyvsp[(2) - (4)].interm.type).arraySizes);
            (*(yyval.interm.typeList))[i].type->mergeType((yyvsp[(2) - (4)].interm.type));
        }
    }
    break;

  case 302:
/* Line 1792 of yacc.c  */
#line 1999 "glslang.y"
    {
        (yyval.interm.typeList) = new TTypeList;
        (yyval.interm.typeList)->push_back((yyvsp[(1) - (1)].interm.typeLine));
    }
    break;

  case 303:
/* Line 1792 of yacc.c  */
#line 2003 "glslang.y"
    {
        (yyval.interm.typeList)->push_back((yyvsp[(3) - (3)].interm.typeLine));
    }
    break;

  case 304:
/* Line 1792 of yacc.c  */
#line 2009 "glslang.y"
    {
        (yyval.interm.typeLine).type = new TType(EbtVoid);
        (yyval.interm.typeLine).loc = (yyvsp[(1) - (1)].lex).loc;
        (yyval.interm.typeLine).type->setFieldName(*(yyvsp[(1) - (1)].lex).string);
    }
    break;

  case 305:
/* Line 1792 of yacc.c  */
#line 2014 "glslang.y"
    {
        parseContext.arrayDimCheck((yyvsp[(1) - (2)].lex).loc, (yyvsp[(2) - (2)].interm).arraySizes, 0);

        (yyval.interm.typeLine).type = new TType(EbtVoid);
        (yyval.interm.typeLine).loc = (yyvsp[(1) - (2)].lex).loc;
        (yyval.interm.typeLine).type->setFieldName(*(yyvsp[(1) - (2)].lex).string);
        (yyval.interm.typeLine).type->setArraySizes((yyvsp[(2) - (2)].interm).arraySizes);
    }
    break;

  case 306:
/* Line 1792 of yacc.c  */
#line 2025 "glslang.y"
    {
        (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode);
    }
    break;

  case 307:
/* Line 1792 of yacc.c  */
#line 2028 "glslang.y"
    {
        const char* initFeature = "{ } style initializers";
        parseContext.requireProfile((yyvsp[(1) - (3)].lex).loc, ~EEsProfile, initFeature);
        parseContext.profileRequires((yyvsp[(1) - (3)].lex).loc, ~EEsProfile, 420, E_GL_ARB_shading_language_420pack, initFeature);
        (yyval.interm.intermTypedNode) = (yyvsp[(2) - (3)].interm.intermTypedNode);
    }
    break;

  case 308:
/* Line 1792 of yacc.c  */
#line 2034 "glslang.y"
    {
        const char* initFeature = "{ } style initializers";
        parseContext.requireProfile((yyvsp[(1) - (4)].lex).loc, ~EEsProfile, initFeature);
        parseContext.profileRequires((yyvsp[(1) - (4)].lex).loc, ~EEsProfile, 420, E_GL_ARB_shading_language_420pack, initFeature);
        (yyval.interm.intermTypedNode) = (yyvsp[(2) - (4)].interm.intermTypedNode);
    }
    break;

  case 309:
/* Line 1792 of yacc.c  */
#line 2043 "glslang.y"
    {
        (yyval.interm.intermTypedNode) = parseContext.intermediate.growAggregate(0, (yyvsp[(1) - (1)].interm.intermTypedNode), (yyvsp[(1) - (1)].interm.intermTypedNode)->getLoc());
    }
    break;

  case 310:
/* Line 1792 of yacc.c  */
#line 2046 "glslang.y"
    {
        (yyval.interm.intermTypedNode) = parseContext.intermediate.growAggregate((yyvsp[(1) - (3)].interm.intermTypedNode), (yyvsp[(3) - (3)].interm.intermTypedNode));
    }
    break;

  case 311:
/* Line 1792 of yacc.c  */
#line 2052 "glslang.y"
    { (yyval.interm.intermNode) = (yyvsp[(1) - (1)].interm.intermNode); }
    break;

  case 312:
/* Line 1792 of yacc.c  */
#line 2056 "glslang.y"
    { (yyval.interm.intermNode) = (yyvsp[(1) - (1)].interm.intermNode); }
    break;

  case 313:
/* Line 1792 of yacc.c  */
#line 2057 "glslang.y"
    { (yyval.interm.intermNode) = (yyvsp[(1) - (1)].interm.intermNode); }
    break;

  case 314:
/* Line 1792 of yacc.c  */
#line 2063 "glslang.y"
    { (yyval.interm.intermNode) = (yyvsp[(1) - (1)].interm.intermNode); }
    break;

  case 315:
/* Line 1792 of yacc.c  */
#line 2064 "glslang.y"
    { (yyval.interm.intermNode) = (yyvsp[(1) - (1)].interm.intermNode); }
    break;

  case 316:
/* Line 1792 of yacc.c  */
#line 2065 "glslang.y"
    { (yyval.interm.intermNode) = (yyvsp[(1) - (1)].interm.intermNode); }
    break;

  case 317:
/* Line 1792 of yacc.c  */
#line 2066 "glslang.y"
    { (yyval.interm.intermNode) = (yyvsp[(1) - (1)].interm.intermNode); }
    break;

  case 318:
/* Line 1792 of yacc.c  */
#line 2067 "glslang.y"
    { (yyval.interm.intermNode) = (yyvsp[(1) - (1)].interm.intermNode); }
    break;

  case 319:
/* Line 1792 of yacc.c  */
#line 2068 "glslang.y"
    { (yyval.interm.intermNode) = (yyvsp[(1) - (1)].interm.intermNode); }
    break;

  case 320:
/* Line 1792 of yacc.c  */
#line 2069 "glslang.y"
    { (yyval.interm.intermNode) = (yyvsp[(1) - (1)].interm.intermNode); }
    break;

  case 321:
/* Line 1792 of yacc.c  */
#line 2073 "glslang.y"
    { (yyval.interm.intermNode) = 0; }
    break;

  case 322:
/* Line 1792 of yacc.c  */
#line 2074 "glslang.y"
    {
        parseContext.symbolTable.push();
        ++parseContext.statementNestingLevel;
    }
    break;

  case 323:
/* Line 1792 of yacc.c  */
#line 2078 "glslang.y"
    {
        parseContext.symbolTable.pop(&parseContext.defaultPrecision[0]);
        --parseContext.statementNestingLevel;
    }
    break;

  case 324:
/* Line 1792 of yacc.c  */
#line 2082 "glslang.y"
    {
        if ((yyvsp[(3) - (5)].interm.intermNode) && (yyvsp[(3) - (5)].interm.intermNode)->getAsAggregate())
            (yyvsp[(3) - (5)].interm.intermNode)->getAsAggregate()->setOperator(EOpSequence);
        (yyval.interm.intermNode) = (yyvsp[(3) - (5)].interm.intermNode);
    }
    break;

  case 325:
/* Line 1792 of yacc.c  */
#line 2090 "glslang.y"
    { (yyval.interm.intermNode) = (yyvsp[(1) - (1)].interm.intermNode); }
    break;

  case 326:
/* Line 1792 of yacc.c  */
#line 2091 "glslang.y"
    { (yyval.interm.intermNode) = (yyvsp[(1) - (1)].interm.intermNode); }
    break;

  case 327:
/* Line 1792 of yacc.c  */
#line 2095 "glslang.y"
    {
        ++parseContext.controlFlowNestingLevel;
    }
    break;

  case 328:
/* Line 1792 of yacc.c  */
#line 2098 "glslang.y"
    {
        --parseContext.controlFlowNestingLevel;
        (yyval.interm.intermNode) = (yyvsp[(2) - (2)].interm.intermNode);
    }
    break;

  case 329:
/* Line 1792 of yacc.c  */
#line 2102 "glslang.y"
    {
        parseContext.symbolTable.push();
        ++parseContext.statementNestingLevel;
        ++parseContext.controlFlowNestingLevel;
    }
    break;

  case 330:
/* Line 1792 of yacc.c  */
#line 2107 "glslang.y"
    {
        parseContext.symbolTable.pop(&parseContext.defaultPrecision[0]);
        --parseContext.statementNestingLevel;
        --parseContext.controlFlowNestingLevel;
        (yyval.interm.intermNode) = (yyvsp[(2) - (2)].interm.intermNode);
    }
    break;

  case 331:
/* Line 1792 of yacc.c  */
#line 2116 "glslang.y"
    {
        (yyval.interm.intermNode) = 0;
    }
    break;

  case 332:
/* Line 1792 of yacc.c  */
#line 2119 "glslang.y"
    {
        if ((yyvsp[(2) - (3)].interm.intermNode) && (yyvsp[(2) - (3)].interm.intermNode)->getAsAggregate())
            (yyvsp[(2) - (3)].interm.intermNode)->getAsAggregate()->setOperator(EOpSequence);
        (yyval.interm.intermNode) = (yyvsp[(2) - (3)].interm.intermNode);
    }
    break;

  case 333:
/* Line 1792 of yacc.c  */
#line 2127 "glslang.y"
    {
        (yyval.interm.intermNode) = parseContext.intermediate.makeAggregate((yyvsp[(1) - (1)].interm.intermNode));
        if ((yyvsp[(1) - (1)].interm.intermNode) && (yyvsp[(1) - (1)].interm.intermNode)->getAsBranchNode() && ((yyvsp[(1) - (1)].interm.intermNode)->getAsBranchNode()->getFlowOp() == EOpCase ||
                                            (yyvsp[(1) - (1)].interm.intermNode)->getAsBranchNode()->getFlowOp() == EOpDefault)) {
            parseContext.wrapupSwitchSubsequence(0, (yyvsp[(1) - (1)].interm.intermNode));
            (yyval.interm.intermNode) = 0;  // start a fresh subsequence for what's after this case
        }
    }
    break;

  case 334:
/* Line 1792 of yacc.c  */
#line 2135 "glslang.y"
    {
        if ((yyvsp[(2) - (2)].interm.intermNode) && (yyvsp[(2) - (2)].interm.intermNode)->getAsBranchNode() && ((yyvsp[(2) - (2)].interm.intermNode)->getAsBranchNode()->getFlowOp() == EOpCase ||
                                            (yyvsp[(2) - (2)].interm.intermNode)->getAsBranchNode()->getFlowOp() == EOpDefault)) {
            parseContext.wrapupSwitchSubsequence((yyvsp[(1) - (2)].interm.intermNode) ? (yyvsp[(1) - (2)].interm.intermNode)->getAsAggregate() : 0, (yyvsp[(2) - (2)].interm.intermNode));
            (yyval.interm.intermNode) = 0;  // start a fresh subsequence for what's after this case
        } else
            (yyval.interm.intermNode) = parseContext.intermediate.growAggregate((yyvsp[(1) - (2)].interm.intermNode), (yyvsp[(2) - (2)].interm.intermNode));
    }
    break;

  case 335:
/* Line 1792 of yacc.c  */
#line 2146 "glslang.y"
    { (yyval.interm.intermNode) = 0; }
    break;

  case 336:
/* Line 1792 of yacc.c  */
#line 2147 "glslang.y"
    { (yyval.interm.intermNode) = static_cast<TIntermNode*>((yyvsp[(1) - (2)].interm.intermTypedNode)); }
    break;

  case 337:
/* Line 1792 of yacc.c  */
#line 2151 "glslang.y"
    {
        parseContext.boolCheck((yyvsp[(1) - (5)].lex).loc, (yyvsp[(3) - (5)].interm.intermTypedNode));
        (yyval.interm.intermNode) = parseContext.intermediate.addSelection((yyvsp[(3) - (5)].interm.intermTypedNode), (yyvsp[(5) - (5)].interm.nodePair), (yyvsp[(1) - (5)].lex).loc);
    }
    break;

  case 338:
/* Line 1792 of yacc.c  */
#line 2158 "glslang.y"
    {
        (yyval.interm.nodePair).node1 = (yyvsp[(1) - (3)].interm.intermNode);
        (yyval.interm.nodePair).node2 = (yyvsp[(3) - (3)].interm.intermNode);
    }
    break;

  case 339:
/* Line 1792 of yacc.c  */
#line 2162 "glslang.y"
    {
        (yyval.interm.nodePair).node1 = (yyvsp[(1) - (1)].interm.intermNode);
        (yyval.interm.nodePair).node2 = 0;
    }
    break;

  case 340:
/* Line 1792 of yacc.c  */
#line 2170 "glslang.y"
    {
        (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode);
        parseContext.boolCheck((yyvsp[(1) - (1)].interm.intermTypedNode)->getLoc(), (yyvsp[(1) - (1)].interm.intermTypedNode));
    }
    break;

  case 341:
/* Line 1792 of yacc.c  */
#line 2174 "glslang.y"
    {
        parseContext.boolCheck((yyvsp[(2) - (4)].lex).loc, (yyvsp[(1) - (4)].interm.type));

        TType type((yyvsp[(1) - (4)].interm.type));
        TIntermNode* initNode = parseContext.declareVariable((yyvsp[(2) - (4)].lex).loc, *(yyvsp[(2) - (4)].lex).string, (yyvsp[(1) - (4)].interm.type), 0, (yyvsp[(4) - (4)].interm.intermTypedNode));
        if (initNode)
            (yyval.interm.intermTypedNode) = initNode->getAsTyped();
        else
            (yyval.interm.intermTypedNode) = 0;
    }
    break;

  case 342:
/* Line 1792 of yacc.c  */
#line 2187 "glslang.y"
    {
        // start new switch sequence on the switch stack
        ++parseContext.controlFlowNestingLevel;
        ++parseContext.statementNestingLevel;
        parseContext.switchSequenceStack.push_back(new TIntermSequence);
        parseContext.switchLevel.push_back(parseContext.statementNestingLevel);
        parseContext.symbolTable.push();
    }
    break;

  case 343:
/* Line 1792 of yacc.c  */
#line 2195 "glslang.y"
    {
        (yyval.interm.intermNode) = parseContext.addSwitch((yyvsp[(1) - (8)].lex).loc, (yyvsp[(3) - (8)].interm.intermTypedNode), (yyvsp[(7) - (8)].interm.intermNode) ? (yyvsp[(7) - (8)].interm.intermNode)->getAsAggregate() : 0);
        delete parseContext.switchSequenceStack.back();
        parseContext.switchSequenceStack.pop_back();
        parseContext.switchLevel.pop_back();
        parseContext.symbolTable.pop(&parseContext.defaultPrecision[0]);
        --parseContext.statementNestingLevel;
        --parseContext.controlFlowNestingLevel;
    }
    break;

  case 344:
/* Line 1792 of yacc.c  */
#line 2207 "glslang.y"
    {
        (yyval.interm.intermNode) = 0;
    }
    break;

  case 345:
/* Line 1792 of yacc.c  */
#line 2210 "glslang.y"
    {
        (yyval.interm.intermNode) = (yyvsp[(1) - (1)].interm.intermNode);
    }
    break;

  case 346:
/* Line 1792 of yacc.c  */
#line 2216 "glslang.y"
    {
        (yyval.interm.intermNode) = 0;
        if (parseContext.switchLevel.size() == 0)
            parseContext.error((yyvsp[(1) - (3)].lex).loc, "cannot appear outside switch statement", "case", "");
        else if (parseContext.switchLevel.back() != parseContext.statementNestingLevel)
            parseContext.error((yyvsp[(1) - (3)].lex).loc, "cannot be nested inside control flow", "case", "");
        else {
            parseContext.constantValueCheck((yyvsp[(2) - (3)].interm.intermTypedNode), "case");
            parseContext.integerCheck((yyvsp[(2) - (3)].interm.intermTypedNode), "case");
            (yyval.interm.intermNode) = parseContext.intermediate.addBranch(EOpCase, (yyvsp[(2) - (3)].interm.intermTypedNode), (yyvsp[(1) - (3)].lex).loc);
        }
    }
    break;

  case 347:
/* Line 1792 of yacc.c  */
#line 2228 "glslang.y"
    {
        (yyval.interm.intermNode) = 0;
        if (parseContext.switchLevel.size() == 0)
            parseContext.error((yyvsp[(1) - (2)].lex).loc, "cannot appear outside switch statement", "default", "");
        else if (parseContext.switchLevel.back() != parseContext.statementNestingLevel)
            parseContext.error((yyvsp[(1) - (2)].lex).loc, "cannot be nested inside control flow", "default", "");
        else
            (yyval.interm.intermNode) = parseContext.intermediate.addBranch(EOpDefault, (yyvsp[(1) - (2)].lex).loc);
    }
    break;

  case 348:
/* Line 1792 of yacc.c  */
#line 2240 "glslang.y"
    {
        if (! parseContext.limits.whileLoops)
            parseContext.error((yyvsp[(1) - (2)].lex).loc, "while loops not available", "limitation", "");
        parseContext.symbolTable.push();
        ++parseContext.loopNestingLevel;
        ++parseContext.statementNestingLevel;
        ++parseContext.controlFlowNestingLevel;
    }
    break;

  case 349:
/* Line 1792 of yacc.c  */
#line 2248 "glslang.y"
    {
        parseContext.symbolTable.pop(&parseContext.defaultPrecision[0]);
        (yyval.interm.intermNode) = parseContext.intermediate.addLoop((yyvsp[(6) - (6)].interm.intermNode), (yyvsp[(4) - (6)].interm.intermTypedNode), 0, true, (yyvsp[(1) - (6)].lex).loc);
        --parseContext.loopNestingLevel;
        --parseContext.statementNestingLevel;
        --parseContext.controlFlowNestingLevel;
    }
    break;

  case 350:
/* Line 1792 of yacc.c  */
#line 2255 "glslang.y"
    {
        ++parseContext.loopNestingLevel;
        ++parseContext.statementNestingLevel;
        ++parseContext.controlFlowNestingLevel;
    }
    break;

  case 351:
/* Line 1792 of yacc.c  */
#line 2260 "glslang.y"
    {
        if (! parseContext.limits.whileLoops)
            parseContext.error((yyvsp[(1) - (8)].lex).loc, "do-while loops not available", "limitation", "");

        parseContext.boolCheck((yyvsp[(8) - (8)].lex).loc, (yyvsp[(6) - (8)].interm.intermTypedNode));

        (yyval.interm.intermNode) = parseContext.intermediate.addLoop((yyvsp[(3) - (8)].interm.intermNode), (yyvsp[(6) - (8)].interm.intermTypedNode), 0, false, (yyvsp[(4) - (8)].lex).loc);
        --parseContext.loopNestingLevel;
        --parseContext.statementNestingLevel;
        --parseContext.controlFlowNestingLevel;
    }
    break;

  case 352:
/* Line 1792 of yacc.c  */
#line 2271 "glslang.y"
    {
        parseContext.symbolTable.push();
        ++parseContext.loopNestingLevel;
        ++parseContext.statementNestingLevel;
        ++parseContext.controlFlowNestingLevel;
    }
    break;

  case 353:
/* Line 1792 of yacc.c  */
#line 2277 "glslang.y"
    {
        parseContext.symbolTable.pop(&parseContext.defaultPrecision[0]);
        (yyval.interm.intermNode) = parseContext.intermediate.makeAggregate((yyvsp[(4) - (7)].interm.intermNode), (yyvsp[(2) - (7)].lex).loc);
        TIntermLoop* forLoop = parseContext.intermediate.addLoop((yyvsp[(7) - (7)].interm.intermNode), reinterpret_cast<TIntermTyped*>((yyvsp[(5) - (7)].interm.nodePair).node1), reinterpret_cast<TIntermTyped*>((yyvsp[(5) - (7)].interm.nodePair).node2), true, (yyvsp[(1) - (7)].lex).loc);
        if (! parseContext.limits.nonInductiveForLoops)
            parseContext.inductiveLoopCheck((yyvsp[(1) - (7)].lex).loc, (yyvsp[(4) - (7)].interm.intermNode), forLoop);
        (yyval.interm.intermNode) = parseContext.intermediate.growAggregate((yyval.interm.intermNode), forLoop, (yyvsp[(1) - (7)].lex).loc);
        (yyval.interm.intermNode)->getAsAggregate()->setOperator(EOpSequence);
        --parseContext.loopNestingLevel;
        --parseContext.statementNestingLevel;
        --parseContext.controlFlowNestingLevel;
    }
    break;

  case 354:
/* Line 1792 of yacc.c  */
#line 2292 "glslang.y"
    {
        (yyval.interm.intermNode) = (yyvsp[(1) - (1)].interm.intermNode);
    }
    break;

  case 355:
/* Line 1792 of yacc.c  */
#line 2295 "glslang.y"
    {
        (yyval.interm.intermNode) = (yyvsp[(1) - (1)].interm.intermNode);
    }
    break;

  case 356:
/* Line 1792 of yacc.c  */
#line 2301 "glslang.y"
    {
        (yyval.interm.intermTypedNode) = (yyvsp[(1) - (1)].interm.intermTypedNode);
    }
    break;

  case 357:
/* Line 1792 of yacc.c  */
#line 2304 "glslang.y"
    {
        (yyval.interm.intermTypedNode) = 0;
    }
    break;

  case 358:
/* Line 1792 of yacc.c  */
#line 2310 "glslang.y"
    {
        (yyval.interm.nodePair).node1 = (yyvsp[(1) - (2)].interm.intermTypedNode);
        (yyval.interm.nodePair).node2 = 0;
    }
    break;

  case 359:
/* Line 1792 of yacc.c  */
#line 2314 "glslang.y"
    {
        (yyval.interm.nodePair).node1 = (yyvsp[(1) - (3)].interm.intermTypedNode);
        (yyval.interm.nodePair).node2 = (yyvsp[(3) - (3)].interm.intermTypedNode);
    }
    break;

  case 360:
/* Line 1792 of yacc.c  */
#line 2321 "glslang.y"
    {
        if (parseContext.loopNestingLevel <= 0)
            parseContext.error((yyvsp[(1) - (2)].lex).loc, "continue statement only allowed in loops", "", "");
        (yyval.interm.intermNode) = parseContext.intermediate.addBranch(EOpContinue, (yyvsp[(1) - (2)].lex).loc);
    }
    break;

  case 361:
/* Line 1792 of yacc.c  */
#line 2326 "glslang.y"
    {
        if (parseContext.loopNestingLevel + parseContext.switchSequenceStack.size() <= 0)
            parseContext.error((yyvsp[(1) - (2)].lex).loc, "break statement only allowed in switch and loops", "", "");
        (yyval.interm.intermNode) = parseContext.intermediate.addBranch(EOpBreak, (yyvsp[(1) - (2)].lex).loc);
    }
    break;

  case 362:
/* Line 1792 of yacc.c  */
#line 2331 "glslang.y"
    {
        (yyval.interm.intermNode) = parseContext.intermediate.addBranch(EOpReturn, (yyvsp[(1) - (2)].lex).loc);
        if (parseContext.currentFunctionType->getBasicType() != EbtVoid)
            parseContext.error((yyvsp[(1) - (2)].lex).loc, "non-void function must return a value", "return", "");
        if (parseContext.inMain)
            parseContext.postMainReturn = true;
    }
    break;

  case 363:
/* Line 1792 of yacc.c  */
#line 2338 "glslang.y"
    {
        parseContext.functionReturnsValue = true;
        if (parseContext.currentFunctionType->getBasicType() == EbtVoid) {
            parseContext.error((yyvsp[(1) - (3)].lex).loc, "void function cannot return a value", "return", "");
            (yyval.interm.intermNode) = parseContext.intermediate.addBranch(EOpReturn, (yyvsp[(1) - (3)].lex).loc);
        } else if (*(parseContext.currentFunctionType) != (yyvsp[(2) - (3)].interm.intermTypedNode)->getType()) {
            TIntermTyped* converted = parseContext.intermediate.addConversion(EOpReturn, *parseContext.currentFunctionType, (yyvsp[(2) - (3)].interm.intermTypedNode));
            if (converted) {
                if (parseContext.version < 420)
                    parseContext.warn((yyvsp[(1) - (3)].lex).loc, "type conversion on return values was not explicitly allowed until version 420", "return", "");
                (yyval.interm.intermNode) = parseContext.intermediate.addBranch(EOpReturn, converted, (yyvsp[(1) - (3)].lex).loc);
            } else {
                parseContext.error((yyvsp[(1) - (3)].lex).loc, "type does not match, or is not convertible to, the function's return type", "return", "");
                (yyval.interm.intermNode) = parseContext.intermediate.addBranch(EOpReturn, (yyvsp[(2) - (3)].interm.intermTypedNode), (yyvsp[(1) - (3)].lex).loc);
            }
        } else
		    (yyval.interm.intermNode) = parseContext.intermediate.addBranch(EOpReturn, (yyvsp[(2) - (3)].interm.intermTypedNode), (yyvsp[(1) - (3)].lex).loc);
    }
    break;

  case 364:
/* Line 1792 of yacc.c  */
#line 2356 "glslang.y"
    {
        parseContext.requireStage((yyvsp[(1) - (2)].lex).loc, EShLangFragment, "discard");
        (yyval.interm.intermNode) = parseContext.intermediate.addBranch(EOpKill, (yyvsp[(1) - (2)].lex).loc);
    }
    break;

  case 365:
/* Line 1792 of yacc.c  */
#line 2365 "glslang.y"
    {
        (yyval.interm.intermNode) = (yyvsp[(1) - (1)].interm.intermNode);
        parseContext.intermediate.setTreeRoot((yyval.interm.intermNode));
    }
    break;

  case 366:
/* Line 1792 of yacc.c  */
#line 2369 "glslang.y"
    {
        (yyval.interm.intermNode) = parseContext.intermediate.growAggregate((yyvsp[(1) - (2)].interm.intermNode), (yyvsp[(2) - (2)].interm.intermNode));
        parseContext.intermediate.setTreeRoot((yyval.interm.intermNode));
    }
    break;

  case 367:
/* Line 1792 of yacc.c  */
#line 2376 "glslang.y"
    {
        (yyval.interm.intermNode) = (yyvsp[(1) - (1)].interm.intermNode);
    }
    break;

  case 368:
/* Line 1792 of yacc.c  */
#line 2379 "glslang.y"
    {
        (yyval.interm.intermNode) = (yyvsp[(1) - (1)].interm.intermNode);
    }
    break;

  case 369:
/* Line 1792 of yacc.c  */
#line 2385 "glslang.y"
    {
        (yyvsp[(1) - (1)].interm).function = parseContext.handleFunctionDeclarator((yyvsp[(1) - (1)].interm).loc, *(yyvsp[(1) - (1)].interm).function, false /* not prototype */);
        (yyvsp[(1) - (1)].interm).intermNode = parseContext.handleFunctionDefinition((yyvsp[(1) - (1)].interm).loc, *(yyvsp[(1) - (1)].interm).function);
    }
    break;

  case 370:
/* Line 1792 of yacc.c  */
#line 2389 "glslang.y"
    {
        //   May be best done as post process phase on intermediate code
        if (parseContext.currentFunctionType->getBasicType() != EbtVoid && ! parseContext.functionReturnsValue)
            parseContext.error((yyvsp[(1) - (3)].interm).loc, "function does not return a value:", "", (yyvsp[(1) - (3)].interm).function->getName().c_str());
        parseContext.symbolTable.pop(&parseContext.defaultPrecision[0]);
        (yyval.interm.intermNode) = parseContext.intermediate.growAggregate((yyvsp[(1) - (3)].interm).intermNode, (yyvsp[(3) - (3)].interm.intermNode));
        parseContext.intermediate.setAggregateOperator((yyval.interm.intermNode), EOpFunction, (yyvsp[(1) - (3)].interm).function->getType(), (yyvsp[(1) - (3)].interm).loc);
        (yyval.interm.intermNode)->getAsAggregate()->setName((yyvsp[(1) - (3)].interm).function->getMangledName().c_str());

        // store the pragma information for debug and optimize and other vendor specific
        // information. This information can be queried from the parse tree
        (yyval.interm.intermNode)->getAsAggregate()->setOptimize(parseContext.contextPragma.optimize);
        (yyval.interm.intermNode)->getAsAggregate()->setDebug(parseContext.contextPragma.debug);
        (yyval.interm.intermNode)->getAsAggregate()->addToPragmaTable(parseContext.contextPragma.pragmaTable);
    }
    break;


/* Line 1792 of yacc.c  */
#line 6855 "glslang_tab.cpp"
      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (pParseContext, YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (pParseContext, yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
	{
	  /* Return failure if at end of input.  */
	  if (yychar == YYEOF)
	    YYABORT;
	}
      else
	{
	  yydestruct ("Error: discarding",
		      yytoken, &yylval, pParseContext);
	  yychar = YYEMPTY;
	}
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule which action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
	    {
	      yyn = yytable[yyn];
	      if (0 < yyn)
		break;
	    }
	}

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
	YYABORT;


      yydestruct ("Error: popping",
		  yystos[yystate], yyvsp, pParseContext);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#if !defined yyoverflow || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (pParseContext, YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, pParseContext);
    }
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp, pParseContext);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}


/* Line 2055 of yacc.c  */
#line 2406 "glslang.y"

