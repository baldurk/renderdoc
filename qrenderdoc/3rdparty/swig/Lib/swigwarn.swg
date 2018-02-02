/* SWIG warning codes */


%define SWIGWARN_NONE                     0 %enddef

/* -- Deprecated features -- */

%define SWIGWARN_DEPRECATED_EXTERN        101 %enddef
%define SWIGWARN_DEPRECATED_VAL           102 %enddef
%define SWIGWARN_DEPRECATED_OUT           103 %enddef
%define SWIGWARN_DEPRECATED_DISABLEDOC    104 %enddef
%define SWIGWARN_DEPRECATED_ENABLEDOC     105 %enddef
%define SWIGWARN_DEPRECATED_DOCONLY       106 %enddef
%define SWIGWARN_DEPRECATED_STYLE         107 %enddef
%define SWIGWARN_DEPRECATED_LOCALSTYLE    108 %enddef
%define SWIGWARN_DEPRECATED_TITLE         109 %enddef
%define SWIGWARN_DEPRECATED_SECTION       110 %enddef
%define SWIGWARN_DEPRECATED_SUBSECTION    111 %enddef
%define SWIGWARN_DEPRECATED_SUBSUBSECTION 112 %enddef
%define SWIGWARN_DEPRECATED_ADDMETHODS    113 %enddef
%define SWIGWARN_DEPRECATED_READONLY      114 %enddef
%define SWIGWARN_DEPRECATED_READWRITE     115 %enddef
%define SWIGWARN_DEPRECATED_EXCEPT        116 %enddef
%define SWIGWARN_DEPRECATED_NEW           117 %enddef
%define SWIGWARN_DEPRECATED_EXCEPT_TM     118 %enddef
%define SWIGWARN_DEPRECATED_IGNORE_TM     119 %enddef
%define SWIGWARN_DEPRECATED_OPTC          120 %enddef
%define SWIGWARN_DEPRECATED_NAME          121 %enddef
%define SWIGWARN_DEPRECATED_NOEXTERN      122 %enddef
%define SWIGWARN_DEPRECATED_NODEFAULT     123 %enddef
%define SWIGWARN_DEPRECATED_TYPEMAP_LANG  124 %enddef
%define SWIGWARN_DEPRECATED_INPUT_FILE    125 %enddef
%define SWIGWARN_DEPRECATED_NESTED_WORKAROUND 126 %enddef

/* -- Preprocessor -- */

%define SWIGWARN_PP_MISSING_FILE          201 %enddef
%define SWIGWARN_PP_EVALUATION            202 %enddef
%define SWIGWARN_PP_INCLUDEALL_IMPORTALL  203 %enddef
%define SWIGWARN_PP_CPP_WARNING           204 %enddef
%define SWIGWARN_PP_CPP_ERROR             205 %enddef
%define SWIGWARN_PP_UNEXPECTED_TOKENS     206 %enddef

/* -- C/C++ Parser -- */

%define SWIGWARN_PARSE_CLASS_KEYWORD      301 %enddef
%define SWIGWARN_PARSE_REDEFINED          302 %enddef
%define SWIGWARN_PARSE_EXTEND_UNDEF       303 %enddef
%define SWIGWARN_PARSE_UNSUPPORTED_VALUE  304 %enddef
%define SWIGWARN_PARSE_BAD_VALUE          305 %enddef
%define SWIGWARN_PARSE_PRIVATE            306 %enddef
%define SWIGWARN_PARSE_BAD_DEFAULT        307 %enddef
%define SWIGWARN_PARSE_NAMESPACE_ALIAS    308 %enddef
%define SWIGWARN_PARSE_PRIVATE_INHERIT    309 %enddef
%define SWIGWARN_PARSE_TEMPLATE_REPEAT    310 %enddef
%define SWIGWARN_PARSE_TEMPLATE_PARTIAL   311 %enddef
%define SWIGWARN_PARSE_UNNAMED_NESTED_CLASS 312 %enddef
%define SWIGWARN_PARSE_UNDEFINED_EXTERN   313 %enddef
%define SWIGWARN_PARSE_KEYWORD            314 %enddef
%define SWIGWARN_PARSE_USING_UNDEF        315 %enddef
%define SWIGWARN_PARSE_MODULE_REPEAT      316 %enddef
%define SWIGWARN_PARSE_TEMPLATE_SP_UNDEF  317 %enddef
%define SWIGWARN_PARSE_TEMPLATE_AMBIG     318 %enddef
%define SWIGWARN_PARSE_NO_ACCESS          319 %enddef
%define SWIGWARN_PARSE_EXPLICIT_TEMPLATE  320 %enddef
%define SWIGWARN_PARSE_BUILTIN_NAME       321 %enddef
%define SWIGWARN_PARSE_REDUNDANT          322 %enddef
%define SWIGWARN_PARSE_REC_INHERITANCE    323 %enddef
%define SWIGWARN_PARSE_NESTED_TEMPLATE    324 %enddef
%define SWIGWARN_PARSE_NAMED_NESTED_CLASS 325 %enddef
%define SWIGWARN_PARSE_EXTEND_NAME        326 %enddef

%define SWIGWARN_CPP11_LAMBDA             340 %enddef
%define SWIGWARN_CPP11_ALIAS_DECLARATION  341 %enddef  /* redundant now */
%define SWIGWARN_CPP11_ALIAS_TEMPLATE     342 %enddef  /* redundant now */
%define SWIGWARN_CPP11_VARIADIC_TEMPLATE  343 %enddef

%define SWIGWARN_IGNORE_OPERATOR_NEW        350 %enddef	/* new */
%define SWIGWARN_IGNORE_OPERATOR_DELETE     351 %enddef	/* delete */
%define SWIGWARN_IGNORE_OPERATOR_PLUS       352 %enddef	/* + */
%define SWIGWARN_IGNORE_OPERATOR_MINUS      353 %enddef	/* - */
%define SWIGWARN_IGNORE_OPERATOR_MUL        354 %enddef	/* * */
%define SWIGWARN_IGNORE_OPERATOR_DIV        355 %enddef	/* / */
%define SWIGWARN_IGNORE_OPERATOR_MOD        356 %enddef	/* % */
%define SWIGWARN_IGNORE_OPERATOR_XOR        357 %enddef	/* ^ */
%define SWIGWARN_IGNORE_OPERATOR_AND        358 %enddef	/* & */
%define SWIGWARN_IGNORE_OPERATOR_OR         359 %enddef	/* | */
%define SWIGWARN_IGNORE_OPERATOR_NOT        360 %enddef	/* ~ */
%define SWIGWARN_IGNORE_OPERATOR_LNOT       361 %enddef	/* ! */
%define SWIGWARN_IGNORE_OPERATOR_EQ         362 %enddef	/* = */
%define SWIGWARN_IGNORE_OPERATOR_LT         363 %enddef	/* < */
%define SWIGWARN_IGNORE_OPERATOR_GT         364 %enddef	/* > */
%define SWIGWARN_IGNORE_OPERATOR_PLUSEQ     365 %enddef	/* += */
%define SWIGWARN_IGNORE_OPERATOR_MINUSEQ    366 %enddef	/* -= */
%define SWIGWARN_IGNORE_OPERATOR_MULEQ      367 %enddef	/* *= */
%define SWIGWARN_IGNORE_OPERATOR_DIVEQ      368 %enddef	/* /= */
%define SWIGWARN_IGNORE_OPERATOR_MODEQ      369 %enddef	/* %= */
%define SWIGWARN_IGNORE_OPERATOR_XOREQ      370 %enddef	/* ^= */
%define SWIGWARN_IGNORE_OPERATOR_ANDEQ      371 %enddef	/* &= */
%define SWIGWARN_IGNORE_OPERATOR_OREQ       372 %enddef	/* |= */
%define SWIGWARN_IGNORE_OPERATOR_LSHIFT     373 %enddef	/* << */
%define SWIGWARN_IGNORE_OPERATOR_RSHIFT     374 %enddef	/* >> */
%define SWIGWARN_IGNORE_OPERATOR_LSHIFTEQ   375 %enddef	/* <<= */
%define SWIGWARN_IGNORE_OPERATOR_RSHIFTEQ   376 %enddef	/* >>= */
%define SWIGWARN_IGNORE_OPERATOR_EQUALTO    377 %enddef	/* == */
%define SWIGWARN_IGNORE_OPERATOR_NOTEQUAL   378 %enddef	/* != */
%define SWIGWARN_IGNORE_OPERATOR_LTEQUAL    379 %enddef	/* <= */
%define SWIGWARN_IGNORE_OPERATOR_GTEQUAL    380 %enddef	/* >= */
%define SWIGWARN_IGNORE_OPERATOR_LAND       381 %enddef	/* && */
%define SWIGWARN_IGNORE_OPERATOR_LOR        382 %enddef	/* || */
%define SWIGWARN_IGNORE_OPERATOR_PLUSPLUS   383 %enddef	/* ++ */
%define SWIGWARN_IGNORE_OPERATOR_MINUSMINUS 384 %enddef	/* -- */
%define SWIGWARN_IGNORE_OPERATOR_COMMA      385 %enddef	/* , */
%define SWIGWARN_IGNORE_OPERATOR_ARROWSTAR  386 %enddef	/* ->* */
%define SWIGWARN_IGNORE_OPERATOR_ARROW      387 %enddef	/* -> */
%define SWIGWARN_IGNORE_OPERATOR_CALL       388 %enddef	/* () */
%define SWIGWARN_IGNORE_OPERATOR_INDEX      389 %enddef	/* [] */
%define SWIGWARN_IGNORE_OPERATOR_UPLUS      390 %enddef	/* + */
%define SWIGWARN_IGNORE_OPERATOR_UMINUS     391 %enddef	/* - */
%define SWIGWARN_IGNORE_OPERATOR_UMUL       392 %enddef	/* * */
%define SWIGWARN_IGNORE_OPERATOR_UAND       393 %enddef	/* & */
%define SWIGWARN_IGNORE_OPERATOR_NEWARR     394 %enddef	/* new [] */
%define SWIGWARN_IGNORE_OPERATOR_DELARR     395 %enddef	/* delete [] */
%define SWIGWARN_IGNORE_OPERATOR_REF        396 %enddef	/* operator *() */

/* 394-399 are reserved */

/* -- Type system and typemaps -- */

%define SWIGWARN_TYPE_UNDEFINED_CLASS     401 %enddef
%define SWIGWARN_TYPE_INCOMPLETE          402 %enddef
%define SWIGWARN_TYPE_ABSTRACT            403 %enddef
%define SWIGWARN_TYPE_REDEFINED           404 %enddef

%define SWIGWARN_TYPEMAP_SOURCETARGET     450 %enddef
%define SWIGWARN_TYPEMAP_CHARLEAK         451 %enddef
%define SWIGWARN_TYPEMAP_SWIGTYPE         452 %enddef
%define SWIGWARN_TYPEMAP_APPLY_UNDEF      453 %enddef
%define SWIGWARN_TYPEMAP_SWIGTYPELEAK     454 %enddef

%define SWIGWARN_TYPEMAP_IN_UNDEF         460 %enddef
%define SWIGWARN_TYPEMAP_OUT_UNDEF        461 %enddef
%define SWIGWARN_TYPEMAP_VARIN_UNDEF      462 %enddef
%define SWIGWARN_TYPEMAP_VAROUT_UNDEF     463 %enddef
%define SWIGWARN_TYPEMAP_CONST_UNDEF      464 %enddef
%define SWIGWARN_TYPEMAP_UNDEF            465 %enddef
%define SWIGWARN_TYPEMAP_VAR_UNDEF        466 %enddef
%define SWIGWARN_TYPEMAP_TYPECHECK        467 %enddef
%define SWIGWARN_TYPEMAP_THROW            468 %enddef
%define SWIGWARN_TYPEMAP_DIRECTORIN_UNDEF  469 %enddef
%define SWIGWARN_TYPEMAP_THREAD_UNSAFE     470 %enddef	/* mostly used in directorout typemaps */
%define SWIGWARN_TYPEMAP_DIRECTOROUT_UNDEF 471 %enddef
%define SWIGWARN_TYPEMAP_TYPECHECK_UNDEF   472 %enddef
%define SWIGWARN_TYPEMAP_DIRECTOROUT_PTR   473 %enddef
%define SWIGWARN_TYPEMAP_OUT_OPTIMAL_IGNORED  474 %enddef
%define SWIGWARN_TYPEMAP_OUT_OPTIMAL_MULTIPLE 475 %enddef
%define SWIGWARN_TYPEMAP_INITIALIZER_LIST  476 %enddef
%define SWIGWARN_TYPEMAP_DIRECTORTHROWS_UNDEF 477 %enddef

/* -- Fragments -- */
%define SWIGWARN_FRAGMENT_NOT_FOUND       490 %enddef

/* -- General code generation -- */

%define SWIGWARN_LANG_OVERLOAD_DECL       501 %enddef
%define SWIGWARN_LANG_OVERLOAD_CONSTRUCT  502 %enddef
%define SWIGWARN_LANG_IDENTIFIER          503 %enddef
%define SWIGWARN_LANG_RETURN_TYPE         504 %enddef
%define SWIGWARN_LANG_VARARGS             505 %enddef
%define SWIGWARN_LANG_VARARGS_KEYWORD     506 %enddef
%define SWIGWARN_LANG_NATIVE_UNIMPL       507 %enddef
%define SWIGWARN_LANG_DEREF_SHADOW        508 %enddef
%define SWIGWARN_LANG_OVERLOAD_SHADOW     509 %enddef
%define SWIGWARN_LANG_FRIEND_IGNORE       510 %enddef
%define SWIGWARN_LANG_OVERLOAD_KEYWORD    511 %enddef
%define SWIGWARN_LANG_OVERLOAD_CONST      512 %enddef
%define SWIGWARN_LANG_CLASS_UNNAMED       513 %enddef
%define SWIGWARN_LANG_DIRECTOR_VDESTRUCT  514 %enddef
%define SWIGWARN_LANG_DISCARD_CONST       515 %enddef
%define SWIGWARN_LANG_OVERLOAD_IGNORED    516 %enddef
%define SWIGWARN_LANG_DIRECTOR_ABSTRACT   517 %enddef
%define SWIGWARN_LANG_PORTABILITY_FILENAME 518 %enddef
%define SWIGWARN_LANG_TEMPLATE_METHOD_IGNORE 519 %enddef
%define SWIGWARN_LANG_SMARTPTR_MISSING    520 %enddef
%define SWIGWARN_LANG_ILLEGAL_DESTRUCTOR  521 %enddef
%define SWIGWARN_LANG_EXTEND_CONSTRUCTOR  522 %enddef
%define SWIGWARN_LANG_EXTEND_DESTRUCTOR   523 %enddef
%define SWIGWARN_LANG_NESTED_NAMED_ENUM   524 %enddef

/* -- Reserved (600-799) -- */

/* -- Language module specific warnings (700 - 899) -- */


%define SWIGWARN_D_TYPEMAP_CTYPE_UNDEF            700 %enddef
%define SWIGWARN_D_TYPEMAP_IMTYPE_UNDEF           701 %enddef
%define SWIGWARN_D_TYPEMAP_DTYPE_UNDEF            702 %enddef
%define SWIGWARN_D_MULTIPLE_INHERITANCE           703 %enddef
%define SWIGWARN_D_TYPEMAP_CLASSMOD_UNDEF         704 %enddef
%define SWIGWARN_D_TYPEMAP_DBODY_UNDEF            705 %enddef
%define SWIGWARN_D_TYPEMAP_DOUT_UNDEF             706 %enddef
%define SWIGWARN_D_TYPEMAP_DIN_UNDEF              707 %enddef
%define SWIGWARN_D_TYPEMAP_DDIRECTORIN_UNDEF      708 %enddef
%define SWIGWARN_D_TYPEMAP_DCONSTRUCTOR_UNDEF     709 %enddef
%define SWIGWARN_D_EXCODE_MISSING                 710 %enddef
%define SWIGWARN_D_CANTHROW_MISSING               711 %enddef
%define SWIGWARN_D_NO_DIRECTORCONNECT_ATTR        712 %enddef
%define SWIGWARN_D_NAME_COLLISION                 713 %enddef

/* please leave 700-719 free for D */

%define SWIGWARN_SCILAB_TRUNCATED_NAME            720 %enddef

/* please leave 720-739 free for Scilab */

%define SWIGWARN_PYTHON_INDENT_MISMATCH           740 %enddef

/* please leave 740-759 free for Python */

%define SWIGWARN_RUBY_WRONG_NAME                  801 %enddef
%define SWIGWARN_RUBY_MULTIPLE_INHERITANCE        802 %enddef

/* please leave 800-809 free for Ruby */

%define SWIGWARN_JAVA_TYPEMAP_JNI_UNDEF           810 %enddef
%define SWIGWARN_JAVA_TYPEMAP_JTYPE_UNDEF         811 %enddef
%define SWIGWARN_JAVA_TYPEMAP_JSTYPE_UNDEF        812 %enddef
%define SWIGWARN_JAVA_MULTIPLE_INHERITANCE        813 %enddef
%define SWIGWARN_JAVA_TYPEMAP_GETCPTR_UNDEF       814 %enddef
%define SWIGWARN_JAVA_TYPEMAP_CLASSMOD_UNDEF      815 %enddef
%define SWIGWARN_JAVA_TYPEMAP_JAVABODY_UNDEF      816 %enddef
%define SWIGWARN_JAVA_TYPEMAP_JAVAOUT_UNDEF       817 %enddef
%define SWIGWARN_JAVA_TYPEMAP_JAVAIN_UNDEF        818 %enddef
%define SWIGWARN_JAVA_TYPEMAP_JAVADIRECTORIN_UNDEF    819 %enddef
%define SWIGWARN_JAVA_TYPEMAP_JAVADIRECTOROUT_UNDEF   820 %enddef
%define SWIGWARN_JAVA_TYPEMAP_INTERFACECODE_UNDEF 821 %enddef
%define SWIGWARN_JAVA_COVARIANT_RET               822 %enddef
%define SWIGWARN_JAVA_TYPEMAP_JAVACONSTRUCT_UNDEF 823 %enddef
%define SWIGWARN_JAVA_TYPEMAP_DIRECTORIN_NODESC   824 %enddef
%define SWIGWARN_JAVA_NO_DIRECTORCONNECT_ATTR     825 %enddef
%define SWIGWARN_JAVA_NSPACE_WITHOUT_PACKAGE      826 %enddef

/* please leave 810-829 free for Java */

%define SWIGWARN_CSHARP_TYPEMAP_CTYPE_UNDEF       830 %enddef
%define SWIGWARN_CSHARP_TYPEMAP_CSTYPE_UNDEF      831 %enddef
%define SWIGWARN_CSHARP_TYPEMAP_CSWTYPE_UNDEF     832 %enddef
%define SWIGWARN_CSHARP_MULTIPLE_INHERITANCE      833 %enddef
%define SWIGWARN_CSHARP_TYPEMAP_GETCPTR_UNDEF     834 %enddef
%define SWIGWARN_CSHARP_TYPEMAP_CLASSMOD_UNDEF    835 %enddef
%define SWIGWARN_CSHARP_TYPEMAP_CSBODY_UNDEF      836 %enddef
%define SWIGWARN_CSHARP_TYPEMAP_CSOUT_UNDEF       837 %enddef
%define SWIGWARN_CSHARP_TYPEMAP_CSIN_UNDEF        838 %enddef
%define SWIGWARN_CSHARP_TYPEMAP_CSDIRECTORIN_UNDEF    839 %enddef
%define SWIGWARN_CSHARP_TYPEMAP_CSDIRECTOROUT_UNDEF   840 %enddef
%define SWIGWARN_CSHARP_TYPEMAP_INTERFACECODE_UNDEF   841 %enddef
%define SWIGWARN_CSHARP_COVARIANT_RET             842 %enddef
%define SWIGWARN_CSHARP_TYPEMAP_CSCONSTRUCT_UNDEF 843 %enddef
%define SWIGWARN_CSHARP_EXCODE                    844 %enddef
%define SWIGWARN_CSHARP_CANTHROW                  845 %enddef
%define SWIGWARN_CSHARP_NO_DIRECTORCONNECT_ATTR   846 %enddef

/* please leave 830-849 free for C# */

%define SWIGWARN_MODULA3_TYPEMAP_TYPE_UNDEF        850 %enddef
%define SWIGWARN_MODULA3_TYPEMAP_GETCPTR_UNDEF     851 %enddef
%define SWIGWARN_MODULA3_TYPEMAP_CLASSMOD_UNDEF    852 %enddef
%define SWIGWARN_MODULA3_TYPEMAP_PTRCONSTMOD_UNDEF 853 %enddef
%define SWIGWARN_MODULA3_TYPEMAP_MULTIPLE_RETURN   854 %enddef
%define SWIGWARN_MODULA3_MULTIPLE_INHERITANCE      855 %enddef
%define SWIGWARN_MODULA3_TYPECONSTRUCTOR_UNKNOWN   856 %enddef
%define SWIGWARN_MODULA3_UNKNOWN_PRAGMA            857 %enddef
%define SWIGWARN_MODULA3_BAD_ENUMERATION           858 %enddef
%define SWIGWARN_MODULA3_DOUBLE_ID                 859 %enddef
%define SWIGWARN_MODULA3_BAD_IMPORT                860 %enddef

/* please leave 850-869 free for Modula 3 */

%define SWIGWARN_PHP_MULTIPLE_INHERITANCE         870 %enddef
%define SWIGWARN_PHP_UNKNOWN_PRAGMA               871 %enddef
%define SWIGWARN_PHP_PUBLIC_BASE                  872 %enddef

/* please leave 870-889 free for PHP */

%define SWIGWARN_GO_NAME_CONFLICT                 890 %enddef

/* please leave 890-899 free for Go */

/* -- User defined warnings (900 - 999) -- */

