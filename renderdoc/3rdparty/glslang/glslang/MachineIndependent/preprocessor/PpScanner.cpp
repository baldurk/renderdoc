//
//Copyright (C) 2002-2005  3Dlabs Inc. Ltd.
//Copyright (C) 2013 LunarG, Inc.
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
/****************************************************************************\
Copyright (c) 2002, NVIDIA Corporation.

NVIDIA Corporation("NVIDIA") supplies this software to you in
consideration of your agreement to the following terms, and your use,
installation, modification or redistribution of this NVIDIA software
constitutes acceptance of these terms.  If you do not agree with these
terms, please do not use, install, modify or redistribute this NVIDIA
software.

In consideration of your agreement to abide by the following terms, and
subject to these terms, NVIDIA grants you a personal, non-exclusive
license, under NVIDIA's copyrights in this original NVIDIA software (the
"NVIDIA Software"), to use, reproduce, modify and redistribute the
NVIDIA Software, with or without modifications, in source and/or binary
forms; provided that if you redistribute the NVIDIA Software, you must
retain the copyright notice of NVIDIA, this notice and the following
text and disclaimers in all such redistributions of the NVIDIA Software.
Neither the name, trademarks, service marks nor logos of NVIDIA
Corporation may be used to endorse or promote products derived from the
NVIDIA Software without specific prior written permission from NVIDIA.
Except as expressly stated in this notice, no other rights or licenses
express or implied, are granted by NVIDIA herein, including but not
limited to any patent rights that may be infringed by your derivative
works or by other works in which the NVIDIA Software may be
incorporated. No hardware is licensed hereunder. 

THE NVIDIA SOFTWARE IS BEING PROVIDED ON AN "AS IS" BASIS, WITHOUT
WARRANTIES OR CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED,
INCLUDING WITHOUT LIMITATION, WARRANTIES OR CONDITIONS OF TITLE,
NON-INFRINGEMENT, MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR
ITS USE AND OPERATION EITHER ALONE OR IN COMBINATION WITH OTHER
PRODUCTS.

IN NO EVENT SHALL NVIDIA BE LIABLE FOR ANY SPECIAL, INDIRECT,
INCIDENTAL, EXEMPLARY, CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
TO, LOST PROFITS; PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) OR ARISING IN ANY WAY
OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE
NVIDIA SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT,
TORT (INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF
NVIDIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
\****************************************************************************/
//
// scanner.c
//

#define _CRT_SECURE_NO_WARNINGS

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "PpContext.h"
#include "PpTokens.h"
#include "../Scan.h"

namespace glslang {

int TPpContext::InitScanner()
{
    // Add various atoms needed by the CPP line scanner:
    if (!InitCPP())
        return 0;

    previous_token = '\n';

    return 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// Floating point constants: /////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////

/*
* lFloatConst() - Scan a single- or double-precision floating point constant.  Assumes that the scanner
*         has seen at least one digit, followed by either a decimal '.' or the
*         letter 'e', or a precision ending (e.g., F or LF).
*/

int TPpContext::lFloatConst(int len, int ch, TPpToken* ppToken)
{
    bool HasDecimalOrExponent = false;
    int declen, exp, ExpSign;
    int str_len;
    int isDouble = 0;

    declen = 0;
    exp = 0;

    str_len=len;
    char* str = ppToken->name;
    if (ch == '.') {
        HasDecimalOrExponent = true;
        str[len++] = (char)ch;
        ch = getChar();
        while (ch >= '0' && ch <= '9') {
            if (len < TPpToken::maxTokenLength) {
                declen++;
                if (len > 0 || ch != '0') {
                    str[len] = (char)ch;
                    len++;
                    str_len++;
                }
                ch = getChar();
            } else {
                parseContext.error(ppToken->loc, "float literal too long", "", "");
                len = 1;
                str_len = 1;
            }
        }
    }

    // Exponent:

    if (ch == 'e' || ch == 'E') {
        HasDecimalOrExponent = true;
        if (len >= TPpToken::maxTokenLength) {
            parseContext.error(ppToken->loc, "float literal too long", "", "");
            len = 1,str_len=1;
        } else {
            ExpSign = 1;
            str[len++] = (char)ch;
            ch = getChar();
            if (ch == '+') {
                str[len++] = (char)ch;
                ch = getChar();
            } else if (ch == '-') {
                ExpSign = -1;
                str[len++] = (char)ch;
                ch = getChar();
            }
            if (ch >= '0' && ch <= '9') {
                while (ch >= '0' && ch <= '9') {
                    if (len < TPpToken::maxTokenLength) {
                        exp = exp*10 + ch - '0';
                        str[len++] = (char)ch;
                        ch = getChar();
                    } else {
                        parseContext.error(ppToken->loc, "float literal too long", "", "");
                        len = 1,str_len=1;
                    }
                }
            } else {
                parseContext.error(ppToken->loc, "bad character in float exponent", "", "");
            }
            exp *= ExpSign;
        }
    }

    if (len == 0) {
        ppToken->dval = 0.0;
        strcpy(str, "0.0");
    } else {
        if (ch == 'l' || ch == 'L') {
            parseContext.doubleCheck(ppToken->loc, "double floating-point suffix");
            if (! HasDecimalOrExponent)
                parseContext.error(ppToken->loc, "float literal needs a decimal point or exponent", "", "");
            int ch2 = getChar();
            if (ch2 != 'f' && ch2 != 'F') {
                ungetChar();
                ungetChar();
            } else {
                if (len < TPpToken::maxTokenLength) {
                    str[len++] = (char)ch;
                    str[len++] = (char)ch2;
                    isDouble = 1;
                } else {
                    parseContext.error(ppToken->loc, "float literal too long", "", "");
                    len = 1,str_len=1;
                }
            }
        } else if (ch == 'f' || ch == 'F') {
            parseContext.profileRequires(ppToken->loc,  EEsProfile, 300, nullptr, "floating-point suffix");
            if ((parseContext.messages & EShMsgRelaxedErrors) == 0)
                parseContext.profileRequires(ppToken->loc, ~EEsProfile, 120, nullptr, "floating-point suffix");
            if (! HasDecimalOrExponent)
                parseContext.error(ppToken->loc, "float literal needs a decimal point or exponent", "", "");
            if (len < TPpToken::maxTokenLength)
                str[len++] = (char)ch;
            else {
                parseContext.error(ppToken->loc, "float literal too long", "", "");
                len = 1,str_len=1;
            }
        } else 
            ungetChar();

        str[len]='\0';

        ppToken->dval = strtod(str, nullptr);
    }

    if (isDouble)
        return CPP_DOUBLECONSTANT;
    else
        return CPP_FLOATCONSTANT;
}

//
// Scanner used to tokenize source stream.
//
int TPpContext::tStringInput::scan(TPpToken* ppToken)
{
    char tokenText[TPpToken::maxTokenLength + 1];
    int AlreadyComplained = 0;
    int len = 0;
    int ch = 0;
    int ii = 0;
    unsigned ival = 0;

    ppToken->ival = 0;
    ppToken->space = false;
    ch = pp->getChar();
    for (;;) {
        while (ch == ' ' || ch == '\t') {
            ppToken->space = true;
            ch = pp->getChar();
        }

        ppToken->loc = pp->parseContext.getCurrentLoc();
        len = 0;
        switch (ch) {
        default:
            return ch; // Single character token, including '#' and '\' (escaped newlines are handled at a lower level, so this is just a '\' token)

        case EOF:
            return endOfInput;

        case 'A': case 'B': case 'C': case 'D': case 'E':
        case 'F': case 'G': case 'H': case 'I': case 'J':
        case 'K': case 'L': case 'M': case 'N': case 'O':
        case 'P': case 'Q': case 'R': case 'S': case 'T':
        case 'U': case 'V': case 'W': case 'X': case 'Y':
        case 'Z': case '_':
        case 'a': case 'b': case 'c': case 'd': case 'e':
        case 'f': case 'g': case 'h': case 'i': case 'j':
        case 'k': case 'l': case 'm': case 'n': case 'o':
        case 'p': case 'q': case 'r': case 's': case 't':
        case 'u': case 'v': case 'w': case 'x': case 'y':
        case 'z':
            do {
                if (len < TPpToken::maxTokenLength) {
                    tokenText[len++] = (char)ch;
                    ch = pp->getChar();					
                } else {
                    if (! AlreadyComplained) {
                        pp->parseContext.error(ppToken->loc, "name too long", "", "");
                        AlreadyComplained = 1;
                    }
                    ch = pp->getChar();
                }
            } while ((ch >= 'a' && ch <= 'z') ||
                     (ch >= 'A' && ch <= 'Z') ||
                     (ch >= '0' && ch <= '9') ||
                     ch == '_');

            // line continuation with no token before or after makes len == 0, and need to start over skipping white space, etc.
            if (len == 0)
                continue;

            tokenText[len] = '\0';
            pp->ungetChar();
            ppToken->atom = pp->LookUpAddString(tokenText);

            return CPP_IDENTIFIER;
        case '0':
            ppToken->name[len++] = (char)ch;
            ch = pp->getChar();
            if (ch == 'x' || ch == 'X') {
                // must be hexidecimal

                bool isUnsigned = false;
                ppToken->name[len++] = (char)ch;
                ch = pp->getChar();
                if ((ch >= '0' && ch <= '9') ||
                    (ch >= 'A' && ch <= 'F') ||
                    (ch >= 'a' && ch <= 'f')) {

                    ival = 0;
                    do {
                        if (ival <= 0x0fffffff) {
                            ppToken->name[len++] = (char)ch;
                            if (ch >= '0' && ch <= '9') {
                                ii = ch - '0';
                            } else if (ch >= 'A' && ch <= 'F') {
                                ii = ch - 'A' + 10;
                            } else if (ch >= 'a' && ch <= 'f') {
                                ii = ch - 'a' + 10;
                            } else
                                pp->parseContext.error(ppToken->loc, "bad digit in hexidecimal literal", "", "");
                            ival = (ival << 4) | ii;
                        } else {
                            if (! AlreadyComplained) {
                                pp->parseContext.error(ppToken->loc, "hexidecimal literal too big", "", "");
                                AlreadyComplained = 1;
                            }
                            ival = 0xffffffff;
                        }
                        ch = pp->getChar();
                    } while ((ch >= '0' && ch <= '9') ||
                             (ch >= 'A' && ch <= 'F') ||
                             (ch >= 'a' && ch <= 'f'));
                } else {
                    pp->parseContext.error(ppToken->loc, "bad digit in hexidecimal literal", "", "");
                }
                if (ch == 'u' || ch == 'U') {
                    if (len < TPpToken::maxTokenLength)
                        ppToken->name[len++] = (char)ch;
                    isUnsigned = true;
                } else
                    pp->ungetChar();
                ppToken->name[len] = '\0';
                ppToken->ival = (int)ival;

                if (isUnsigned)
                    return CPP_UINTCONSTANT;
                else
                    return CPP_INTCONSTANT;
            } else {
                // could be octal integer or floating point, speculative pursue octal until it must be floating point

                bool isUnsigned = false;
                bool octalOverflow = false;
                bool nonOctal = false;
                ival = 0;

                // see how much octal-like stuff we can read
                while (ch >= '0' && ch <= '7') {
                    if (len < TPpToken::maxTokenLength)
                        ppToken->name[len++] = (char)ch;
                    else if (! AlreadyComplained) {
                        pp->parseContext.error(ppToken->loc, "numeric literal too long", "", "");
                        AlreadyComplained = 1;
                    }
                    if (ival <= 0x1fffffff) {
                        ii = ch - '0';
                        ival = (ival << 3) | ii;
                    } else
                        octalOverflow = true;
                    ch = pp->getChar();
                }

                // could be part of a float...
                if (ch == '8' || ch == '9') {
                    nonOctal = true;
                    do {
                        if (len < TPpToken::maxTokenLength)
                            ppToken->name[len++] = (char)ch;
                        else if (! AlreadyComplained) {
                            pp->parseContext.error(ppToken->loc, "numeric literal too long", "", "");
                            AlreadyComplained = 1;
                        }
                        ch = pp->getChar();
                    } while (ch >= '0' && ch <= '9');
                }
                if (ch == '.' || ch == 'e' || ch == 'f' || ch == 'E' || ch == 'F' || ch == 'l' || ch == 'L') 
                    return pp->lFloatConst(len, ch, ppToken);
                
                // wasn't a float, so must be octal...
                if (nonOctal)
                    pp->parseContext.error(ppToken->loc, "octal literal digit too large", "", "");

                if (ch == 'u' || ch == 'U') {
                    if (len < TPpToken::maxTokenLength)
                        ppToken->name[len++] = (char)ch;
                    isUnsigned = true;
                } else
                    pp->ungetChar();
                ppToken->name[len] = '\0';

                if (octalOverflow)
                    pp->parseContext.error(ppToken->loc, "octal literal too big", "", "");

                ppToken->ival = (int)ival;

                if (isUnsigned)
                    return CPP_UINTCONSTANT;
                else
                    return CPP_INTCONSTANT;
            }
            break;
        case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            // can't be hexidecimal or octal, is either decimal or floating point

            do {
                if (len < TPpToken::maxTokenLength)
                    ppToken->name[len++] = (char)ch;
                else if (! AlreadyComplained) {
                    pp->parseContext.error(ppToken->loc, "numeric literal too long", "", "");
                    AlreadyComplained = 1;
                }
                ch = pp->getChar();
            } while (ch >= '0' && ch <= '9');
            if (ch == '.' || ch == 'e' || ch == 'f' || ch == 'E' || ch == 'F' || ch == 'l' || ch == 'L') {
                return pp->lFloatConst(len, ch, ppToken);
            } else {
                // Finish handling signed and unsigned integers
                int numericLen = len;
                bool uint = false;
                if (ch == 'u' || ch == 'U') {
                    if (len < TPpToken::maxTokenLength)
                        ppToken->name[len++] = (char)ch;
                    uint = true;
                } else
                    pp->ungetChar();

                ppToken->name[len] = '\0';
                ival = 0;
                const unsigned oneTenthMaxInt  = 0xFFFFFFFFu / 10;
                const unsigned remainderMaxInt = 0xFFFFFFFFu - 10 * oneTenthMaxInt;
                for (int i = 0; i < numericLen; i++) {
                    ch = ppToken->name[i] - '0';
                    if ((ival > oneTenthMaxInt) || (ival == oneTenthMaxInt && ch > remainderMaxInt)) {
                        pp->parseContext.error(ppToken->loc, "numeric literal too big", "", "");
                        ival = 0xFFFFFFFFu;
                        break;
                    } else
                        ival = ival * 10 + ch;
                }
                ppToken->ival = (int)ival;

                if (uint)
                    return CPP_UINTCONSTANT;
                else
                    return CPP_INTCONSTANT;
            }
            break;
        case '-':
            ch = pp->getChar();
            if (ch == '-') {
                return CPP_DEC_OP;
            } else if (ch == '=') {
                return CPP_SUB_ASSIGN;
            } else {
                pp->ungetChar();
                return '-';
            }
        case '+':
            ch = pp->getChar();
            if (ch == '+') {
                return CPP_INC_OP;
            } else if (ch == '=') {
                return CPP_ADD_ASSIGN;
            } else {
                pp->ungetChar();
                return '+';
            }
        case '*':
            ch = pp->getChar();
            if (ch == '=') {
                return CPP_MUL_ASSIGN;
            } else {
                pp->ungetChar();
                return '*';
            }
        case '%':
            ch = pp->getChar();
            if (ch == '=') {
                return CPP_MOD_ASSIGN;
            } else if (ch == '>'){
                return CPP_RIGHT_BRACE;
            } else {
                pp->ungetChar();
                return '%';
            }
        case ':':
            ch = pp->getChar();
            if (ch == '>') {
                return CPP_RIGHT_BRACKET;
            } else {
                pp->ungetChar();
                return ':';
            }
        case '^':
            ch = pp->getChar();
            if (ch == '^') {
                return CPP_XOR_OP;
            } else {
                if (ch == '=')
                    return CPP_XOR_ASSIGN;
                else{
                    pp->ungetChar();
                    return '^';
                }
            }

        case '=':
            ch = pp->getChar();
            if (ch == '=') {
                return CPP_EQ_OP;
            } else {
                pp->ungetChar();
                return '=';
            }
        case '!':
            ch = pp->getChar();
            if (ch == '=') {
                return CPP_NE_OP;
            } else {
                pp->ungetChar();
                return '!';
            }
        case '|':
            ch = pp->getChar();
            if (ch == '|') {
                return CPP_OR_OP;
            } else {
                if (ch == '=')
                    return CPP_OR_ASSIGN;
                else{
                    pp->ungetChar();
                    return '|';
                }
            }
        case '&':
            ch = pp->getChar();
            if (ch == '&') {
                return CPP_AND_OP;
            } else {
                if (ch == '=')
                    return CPP_AND_ASSIGN;
                else{
                    pp->ungetChar();
                    return '&';
                }
            }
        case '<':
            ch = pp->getChar();
            if (ch == '<') {
                ch = pp->getChar();
                if (ch == '=')
                    return CPP_LEFT_ASSIGN;
                else{
                    pp->ungetChar();
                    return CPP_LEFT_OP;
                }
            } else {
                if (ch == '=') {
                    return CPP_LE_OP;
                } else {
                    if (ch == '%')
                        return CPP_LEFT_BRACE;
                    else if (ch == ':')
                        return CPP_LEFT_BRACKET;
                    else{
                        pp->ungetChar();
                        return '<';
                    }
                }
            }
        case '>':
            ch = pp->getChar();
            if (ch == '>') {
                ch = pp->getChar();
                if (ch == '=')
                    return CPP_RIGHT_ASSIGN;
                else{
                    pp->ungetChar();
                    return CPP_RIGHT_OP;
                }
            } else {
                if (ch == '=') {
                    return CPP_GE_OP;
                } else {
                    pp->ungetChar();
                    return '>';
                }
            }
        case '.':
            ch = pp->getChar();
            if (ch >= '0' && ch <= '9') {
                pp->ungetChar();
                return pp->lFloatConst(0, '.', ppToken);
            } else {
                pp->ungetChar();
                return '.';
            }
        case '/':
            ch = pp->getChar();
            if (ch == '/') {
                pp->inComment = true;
                do {
                    ch = pp->getChar();
                } while (ch != '\n' && ch != EOF);
                ppToken->space = true;
                pp->inComment = false;

                if (ch == EOF)
                    return endOfInput;

                return ch;
            } else if (ch == '*') {
                ch = pp->getChar();
                do {
                    while (ch != '*') {
                        if (ch == EOF) {
                            pp->parseContext.error(ppToken->loc, "EOF in comment", "comment", "");
                            return endOfInput;
                        }
                        ch = pp->getChar();
                    }
                    ch = pp->getChar();
                    if (ch == EOF) {
                        pp->parseContext.error(ppToken->loc, "EOF in comment", "comment", "");
                        return endOfInput;
                    }
                } while (ch != '/');
                ppToken->space = true;
                // loop again to get the next token...
                break;
            } else if (ch == '=') {
                return CPP_DIV_ASSIGN;
            } else {
                pp->ungetChar();
                return '/';
            }
            break;
        case '"':
            ch = pp->getChar();
            while (ch != '"' && ch != '\n' && ch != EOF) {
                if (len < TPpToken::maxTokenLength) {
                    tokenText[len] = (char)ch;
                    len++;
                    ch = pp->getChar();
                } else
                    break;
            };
            tokenText[len] = '\0';
            if (ch == '"') {
                ppToken->atom = pp->LookUpAddString(tokenText);
                return CPP_STRCONSTANT;
            } else {
                pp->parseContext.error(ppToken->loc, "end of line in string", "string", "");
                return CPP_ERROR_SY;
            }
        }

        ch = pp->getChar();
    }
}

//
// The main functional entry-point into the preprocessor, which will
// scan the source strings to figure out and return the next processing token.
//
// Return string pointer to next token.
// Return 0 when no more tokens.
//
const char* TPpContext::tokenize(TPpToken* ppToken)
{    
    int token = '\n';

    for(;;) {
        const char* tokenString = nullptr;
        token = scanToken(ppToken);
        ppToken->token = token;
        if (token == EOF) {
            missingEndifCheck();
            return nullptr;
        }
        if (token == '#') {
            if (previous_token == '\n') {
                token = readCPPline(ppToken);
                if (token == EOF) {
                    missingEndifCheck();
                    return nullptr;
                }
                continue;
            } else {
                parseContext.error(ppToken->loc, "preprocessor directive cannot be preceded by another token", "#", "");
                return nullptr;
            }
        }
        previous_token = token;

        if (token == '\n')
            continue;

        // expand macros
        if (token == CPP_IDENTIFIER && MacroExpand(ppToken->atom, ppToken, false, true) != 0)
            continue;

        if (token == CPP_IDENTIFIER)
            tokenString = GetAtomString(ppToken->atom);
        else if (token == CPP_INTCONSTANT || token == CPP_UINTCONSTANT ||
                 token == CPP_FLOATCONSTANT || token == CPP_DOUBLECONSTANT)
            tokenString = ppToken->name;
        else if (token == CPP_STRCONSTANT) {
            parseContext.error(ppToken->loc, "string literals not supported", "\"\"", "");
            tokenString = nullptr;
        } else if (token == '\'') {
            parseContext.error(ppToken->loc, "character literals not supported", "\'", "");
            tokenString = nullptr;
        } else
            tokenString = GetAtomString(token);

        if (tokenString) {
            if (tokenString[0] != 0)
                parseContext.tokensBeforeEOF = 1;

            return tokenString;
        }
    }
}

// Checks if we've seen balanced #if...#endif
void TPpContext::missingEndifCheck()
{
    if (ifdepth > 0)
        parseContext.error(parseContext.getCurrentLoc(), "missing #endif", "", "");
}

} // end namespace glslang
