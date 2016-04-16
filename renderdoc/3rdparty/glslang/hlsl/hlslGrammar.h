//
//Copyright (C) 2016 Google, Inc.
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
//    Neither the name of Google, Inc., nor the names of its
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

#ifndef HLSLGRAMMAR_H_
#define HLSLGRAMMAR_H_

#include "hlslScanContext.h"
#include "hlslParseHelper.h"

namespace glslang {

    // Should just be the grammar aspect of HLSL.
    // Described in more detail in hlslGrammar.cpp.

    class HlslGrammar {
    public:
        HlslGrammar(HlslScanContext& scanner, HlslParseContext& parseContext)
            : scanner(scanner), parseContext(parseContext), intermediate(parseContext.intermediate) { }
        virtual ~HlslGrammar() { }

        bool parse();

    protected:
        void expected(const char*);
        void advanceToken();
        bool acceptTokenClass(EHlslTokenClass);
        bool peekTokenClass(EHlslTokenClass);
        bool acceptIdentifier(HlslToken&);

        bool acceptCompilationUnit();
        bool acceptDeclaration(TIntermNode*& node);
        bool acceptFullySpecifiedType(TType&);
        void acceptQualifier(TQualifier&);
        bool acceptType(TType&);
        bool acceptFunctionParameters(TFunction&);
        bool acceptParameterDeclaration(TFunction&);
        bool acceptFunctionDefinition(TFunction&, TIntermNode*&);
        bool acceptExpression(TIntermTyped*&);
        bool acceptConstructor(TIntermTyped*&);
        bool acceptArguments(TFunction*, TIntermAggregate*&);
        bool acceptLiteral(TIntermTyped*&);
        bool acceptOperator(TOperator& op);
        bool acceptCompoundStatement(TIntermAggregate*&);
        bool acceptStatement(TIntermNode*&);
        bool acceptSemantic();

        HlslScanContext& scanner;        // lexical scanner, to get next token
        HlslParseContext& parseContext;  // state of parsing and helper functions for building the intermediate
        TIntermediate& intermediate;     // the final product, the intermediate representation, includes the AST

        HlslToken token;                 // the current token we are processing
    };

} // end namespace glslang

#endif // HLSLGRAMMAR_H_
