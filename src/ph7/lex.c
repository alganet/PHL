/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef PH7_AMALGAMATION
#include "ph7int.h"
#endif
/*
 * This file implement an efficient hand-coded,thread-safe and full-reentrant 
 * lexical analyzer/Tokenizer for the PH7 engine.
 */
/* Forward declaration */
static sxu32 KeywordCode(const char *z, int n);
static sxi32 LexExtractHeredoc(SyStream *pStream,SyToken *pToken);
/*
 * Tokenize a raw PHP input.
 * Get a single low-level token from the input file. Update the stream pointer so that
 * it points to the first character beyond the extracted token.
 */
static sxi32 TokenizePHP(SyStream *pStream,SyToken *pToken,void *pUserData,void *pCtxData)
{
	SyString *pStr;
	sxi32 rc;
	/* Ignore leading white spaces */
	while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisSpace(pStream->zText[0]) ){
		/* Advance the stream cursor */
		if( pStream->zText[0] == '\n' ){
			/* Update line counter */
			pStream->nLine++;
		}
		pStream->zText++;
	}
	if( pStream->zText >= pStream->zEnd ){
		/* End of input reached */
		return SXERR_EOF;
	}
	/* Record token starting position and line */
	pToken->nLine = pStream->nLine;
	pToken->pUserData = 0;
	pStr = &pToken->sData;
	SyStringInitFromBuf(pStr,pStream->zText,0);
	if( pStream->zText[0] >= 0xc0 || SyisAlpha(pStream->zText[0]) || pStream->zText[0] == '_' ){
		/* The following code fragment is taken verbatim from the xPP source tree.
		 * xPP is a modern embeddable macro processor with advanced features useful for
		 * application seeking for a production quality,ready to use macro processor.
		 * xPP is a widely used library developed and maintened by Symisc Systems.
		 * You can reach the xPP home page by following this link:
		 * http://xpp.symisc.net/
		 */
		const unsigned char *zIn;
		sxu32 nKeyword;
		/* Isolate UTF-8 or alphanumeric stream */
		if( pStream->zText[0] < 0xc0 ){
			pStream->zText++;
		}
		for(;;){
			zIn = pStream->zText;
			if( zIn[0] >= 0xc0 ){
				zIn++;
				/* UTF-8 stream */
				while( zIn < pStream->zEnd && ((zIn[0] & 0xc0) == 0x80) ){
					zIn++;
				}
			}
			/* Skip alphanumeric stream */
			while( zIn < pStream->zEnd && zIn[0] < 0xc0 && (SyisAlphaNum(zIn[0]) || zIn[0] == '_') ){
				zIn++;
			}
			if( zIn == pStream->zText ){
				/* Not an UTF-8 or alphanumeric stream */
				break;
			}
			/* Synchronize pointers */
			pStream->zText = zIn;
		}
		/* Record token length */
		pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);
		nKeyword = KeywordCode(pStr->zString,(int)pStr->nByte);
		if( nKeyword != PH7_TK_ID ){
			if( nKeyword & 
				(PH7_TKWRD_NEW|PH7_TKWRD_CLONE|PH7_TKWRD_AND|PH7_TKWRD_XOR|PH7_TKWRD_OR|PH7_TKWRD_INSTANCEOF|PH7_TKWRD_SEQ|PH7_TKWRD_SNE) ){
					/* Alpha stream operators [i.e: new,clone,and,instanceof,eq,ne,or,xor],save the operator instance for later processing */
					pToken->pUserData = (void *)PH7_ExprExtractOperator(pStr,0);
					/* Mark as an operator */
					pToken->nType = PH7_TK_ID|PH7_TK_OP;
			}else{
				/* We are dealing with a keyword [i.e: while,foreach,class...],save the keyword ID */
				pToken->nType = PH7_TK_KEYWORD;
				pToken->pUserData = SX_INT_TO_PTR(nKeyword);
			}
		}else{
			/* A simple identifier */
			pToken->nType = PH7_TK_ID;
		}
	}else{
		sxi32 c;
		/* Non-alpha stream */
		if( pStream->zText[0] == '#' || 
			( pStream->zText[0] == '/' &&  &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '/') ){
				pStream->zText++;
				/* Inline comments */
				while( pStream->zText < pStream->zEnd && pStream->zText[0] != '\n' ){
					pStream->zText++;
				}
				/* Tell the upper-layer to ignore this token */ 
				return SXERR_CONTINUE;
		}else if( pStream->zText[0] == '/' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '*' ){
			pStream->zText += 2;
			/* Block comment */
			while( pStream->zText < pStream->zEnd ){
				if( pStream->zText[0] == '*' ){
					if( &pStream->zText[1] >= pStream->zEnd || pStream->zText[1] == '/'  ){
						break;
					}
				}
				if( pStream->zText[0] == '\n' ){
					pStream->nLine++;
				}
				pStream->zText++;
			}
			pStream->zText += 2;
			/* Tell the upper-layer to ignore this token */
			return SXERR_CONTINUE;
		}else if( SyisDigit(pStream->zText[0]) ){
			pStream->zText++;
			/* Decimal digit stream */
			while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){
				pStream->zText++;
			}
			/* Mark the token as integer until we encounter a real number */
			pToken->nType = PH7_TK_INTEGER;
			if( pStream->zText < pStream->zEnd ){
				c = pStream->zText[0];
				if( c == '.' ){
					/* Real number */
					pStream->zText++;
					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){
						pStream->zText++;
					}
					if( pStream->zText < pStream->zEnd ){
						c = pStream->zText[0];
						if( c=='e' || c=='E' ){
							pStream->zText++;
							if( pStream->zText < pStream->zEnd ){
								c = pStream->zText[0];
								if( (c =='+' || c=='-') && &pStream->zText[1] < pStream->zEnd  &&
									pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1]) ){
										pStream->zText++;
								}
								while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){
									pStream->zText++;
								}
							}
						}
					}
					pToken->nType = PH7_TK_REAL;
				}else if( c=='e' || c=='E' ){
					SXUNUSED(pUserData); /* Prevent compiler warning */
					SXUNUSED(pCtxData);
					pStream->zText++;
					if( pStream->zText < pStream->zEnd ){
						c = pStream->zText[0];
						if( (c =='+' || c=='-') && &pStream->zText[1] < pStream->zEnd  &&
							pStream->zText[1] < 0xc0 && SyisDigit(pStream->zText[1]) ){
								pStream->zText++;
						}
						while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisDigit(pStream->zText[0]) ){
							pStream->zText++;
						}
					}
					pToken->nType = PH7_TK_REAL;
				}else if( c == 'x' || c == 'X' ){
					/* Hex digit stream */
					pStream->zText++;
					while( pStream->zText < pStream->zEnd && pStream->zText[0] < 0xc0 && SyisHex(pStream->zText[0]) ){
						pStream->zText++;
					}
				}else if(c  == 'b' || c == 'B' ){
					/* Binary digit stream */
					pStream->zText++;
					while( pStream->zText < pStream->zEnd && (pStream->zText[0] == '0' || pStream->zText[0] == '1') ){
						pStream->zText++;
					}
				}
			}
			/* Record token length */
			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);
			return SXRET_OK;
		}
		c = pStream->zText[0];
		pStream->zText++; /* Advance the stream cursor */
		/* Assume we are dealing with an operator*/
		pToken->nType = PH7_TK_OP;
		switch(c){
		case '$': pToken->nType = PH7_TK_DOLLAR; break;
		case '{': pToken->nType = PH7_TK_OCB;    break;
		case '}': pToken->nType = PH7_TK_CCB;    break;
		case '(': pToken->nType = PH7_TK_LPAREN; break; 
		case '[': pToken->nType |= PH7_TK_OSB;   break; /* Bitwise operation here,since the square bracket token '[' 
														 * is a potential operator [i.e: subscripting] */
		case ']': pToken->nType = PH7_TK_CSB;    break;
		case ')': {
			SySet *pTokSet = pStream->pSet;
			/* Assemble type cast operators [i.e: (int),(float),(bool)...] */ 
			if( pTokSet->nUsed >= 2 ){
				SyToken *pTmp;
				/* Peek the last recongnized token */
				pTmp = (SyToken *)SySetPeek(pTokSet);
				if( pTmp->nType & PH7_TK_KEYWORD ){
					sxi32 nID = SX_PTR_TO_INT(pTmp->pUserData);
					if( (sxu32)nID & (PH7_TKWRD_ARRAY|PH7_TKWRD_INT|PH7_TKWRD_FLOAT|PH7_TKWRD_STRING|PH7_TKWRD_OBJECT|PH7_TKWRD_BOOL|PH7_TKWRD_UNSET) ){
						pTmp = (SyToken *)SySetAt(pTokSet,pTokSet->nUsed - 2);
						if( pTmp->nType & PH7_TK_LPAREN ){
							/* Merge the three tokens '(' 'TYPE' ')' into a single one */
							const char * zTypeCast = "(int)";
							if( nID & PH7_TKWRD_FLOAT ){
								zTypeCast = "(float)";
							}else if( nID & PH7_TKWRD_BOOL ){
								zTypeCast = "(bool)";
							}else if( nID & PH7_TKWRD_STRING ){
								zTypeCast = "(string)";
							}else if( nID & PH7_TKWRD_ARRAY ){
								zTypeCast = "(array)";
							}else if( nID & PH7_TKWRD_OBJECT ){
								zTypeCast = "(object)";
							}else if( nID & PH7_TKWRD_UNSET ){
								zTypeCast = "(unset)";
							}
							/* Reflect the change */
							pToken->nType = PH7_TK_OP;
							SyStringInitFromBuf(&pToken->sData,zTypeCast,SyStrlen(zTypeCast));
							/* Save the instance associated with the type cast operator */
							pToken->pUserData = (void *)PH7_ExprExtractOperator(&pToken->sData,0);
							/* Remove the two previous tokens */
							pTokSet->nUsed -= 2;
							return SXRET_OK;
						}
					}
				}
			}
			pToken->nType = PH7_TK_RPAREN;
			break;
				  }
		case '\'':{
			/* Single quoted string */
			pStr->zString++;
			while( pStream->zText < pStream->zEnd ){
				if( pStream->zText[0] == '\''  ){
					if( pStream->zText[-1] != '\\' ){
						break;
					}else{
						const unsigned char *zPtr = &pStream->zText[-2];
						sxi32 i = 1;
						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){
							zPtr--;
							i++;
						}
						if((i&1)==0){
							break;
						}
					}
				}
				if( pStream->zText[0] == '\n' ){
					pStream->nLine++;
				}
				pStream->zText++;
			}
			/* Record token length and type */
			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);
			pToken->nType = PH7_TK_SSTR;
			/* Jump the trailing single quote */
			pStream->zText++;
			return SXRET_OK;
				  }
		case '"':{
			sxi32 iNest;
			/* Double quoted string */
			pStr->zString++;
			while( pStream->zText < pStream->zEnd ){
				if( pStream->zText[0] == '{' && &pStream->zText[1] < pStream->zEnd && pStream->zText[1] == '$'){
					iNest = 1;
					pStream->zText++;
					/* TICKET 1433-40: Hnadle braces'{}' in double quoted string where everything is allowed */
					while(pStream->zText < pStream->zEnd ){
						if( pStream->zText[0] == '{' ){
							iNest++;
						}else if (pStream->zText[0] == '}' ){
							iNest--;
							if( iNest <= 0 ){
								pStream->zText++;
								break;
							}
						}else if( pStream->zText[0] == '\n' ){
							pStream->nLine++;
						}
						pStream->zText++;
					}
					if( pStream->zText >= pStream->zEnd ){
						break;
					}
				}
				if( pStream->zText[0] == '"' ){
					if( pStream->zText[-1] != '\\' ){
						break;
					}else{
						const unsigned char *zPtr = &pStream->zText[-2];
						sxi32 i = 1;
						while( zPtr > pStream->zInput && zPtr[0] == '\\' ){
							zPtr--;
							i++;
						}
						if((i&1)==0){
							break;
						}
					}
				}
				if( pStream->zText[0] == '\n' ){
					pStream->nLine++;
				}
				pStream->zText++;
			}
			/* Record token length and type */
			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);
			pToken->nType = PH7_TK_DSTR;
			/* Jump the trailing quote */
			pStream->zText++;
			return SXRET_OK;
				  }
		case '`':{
			/* Backtick quoted string */
			pStr->zString++;
			while( pStream->zText < pStream->zEnd ){
				if( pStream->zText[0] == '`' && pStream->zText[-1] != '\\' ){
					break;
				}
				if( pStream->zText[0] == '\n' ){
					pStream->nLine++;
				}
				pStream->zText++;
			}
			/* Record token length and type */
			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);
			pToken->nType = PH7_TK_BSTR;
			/* Jump the trailing backtick */
			pStream->zText++;
			return SXRET_OK;
				  }
		case '\\': pToken->nType = PH7_TK_NSSEP;  break;
		case ':':
			if( pStream->zText < pStream->zEnd && pStream->zText[0] == ':' ){
				/* Current operator: '::' */
				pStream->zText++;
			}else{
				pToken->nType = PH7_TK_COLON; /* Single colon */
			}
			break;
		case ',': pToken->nType |= PH7_TK_COMMA;  break; /* Comma is also an operator */
		case ';': pToken->nType = PH7_TK_SEMI;    break;
			/* Handle combined operators [i.e: +=,===,!=== ...] */
		case '=':
			pToken->nType |= PH7_TK_EQUAL;
			if( pStream->zText < pStream->zEnd ){
				if( pStream->zText[0] == '=' ){
					pToken->nType &= ~PH7_TK_EQUAL;
					/* Current operator: == */
					pStream->zText++;
					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){
						/* Current operator: === */
						pStream->zText++;
					}
				}else if( pStream->zText[0] == '>' ){
					/* Array operator: => */
					pToken->nType = PH7_TK_ARRAY_OP;
					pStream->zText++;
				}else{
					/* TICKET 1433-0010: Reference operator '=&' */
					const unsigned char *zCur = pStream->zText;
					sxu32 nLine = 0;
					while( zCur < pStream->zEnd && zCur[0] < 0xc0 && SyisSpace(zCur[0]) ){
						if( zCur[0] == '\n' ){
							nLine++;
						}
						zCur++;
					}
					if( zCur < pStream->zEnd && zCur[0] == '&' ){
						/* Current operator: =& */
						pToken->nType &= ~PH7_TK_EQUAL;
						SyStringInitFromBuf(pStr,"=&",sizeof("=&")-1);
						/* Update token stream */
						pStream->zText = &zCur[1];
						pStream->nLine += nLine; 
					}
				}
			}
			break;
		case '!':
			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){
				/* Current operator: != */
				pStream->zText++;
				if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){
					/* Current operator: !== */
					pStream->zText++;
				}
			}
			break;
		case '&':
			pToken->nType |= PH7_TK_AMPER;
			if( pStream->zText < pStream->zEnd ){
				if( pStream->zText[0] == '&' ){
					pToken->nType &= ~PH7_TK_AMPER;
					/* Current operator: && */
					pStream->zText++;
				}else if( pStream->zText[0] == '=' ){
					pToken->nType &= ~PH7_TK_AMPER;
					/* Current operator: &= */
					pStream->zText++;
				}
			}
			break;
		case '|':
			if( pStream->zText < pStream->zEnd ){
				if( pStream->zText[0] == '|' ){
					/* Current operator: || */
					pStream->zText++;
				}else if( pStream->zText[0] == '=' ){
					/* Current operator: |= */
					pStream->zText++;
				}
			}
			break;
		case '+':
			if( pStream->zText < pStream->zEnd ){
				if( pStream->zText[0] == '+' ){
					/* Current operator: ++ */
					pStream->zText++;
				}else if( pStream->zText[0] == '=' ){
					/* Current operator: += */
					pStream->zText++;
				}
			}
			break;
		case '-':
			if( pStream->zText < pStream->zEnd ){
				if( pStream->zText[0] == '-' ){
					/* Current operator: -- */
					pStream->zText++;
				}else if( pStream->zText[0] == '=' ){
					/* Current operator: -= */
					pStream->zText++;
				}else if( pStream->zText[0] == '>' ){
					/* Current operator: -> */
					pStream->zText++;
				}
			}
			break;
		case '*':
			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){
				/* Current operator: *= */
				pStream->zText++;
			}
			break;
		case '/':
			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){
				/* Current operator: /= */
				pStream->zText++;
			}
			break;
		case '%':
			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){
				/* Current operator: %= */
				pStream->zText++;
			}
			break;
		case '^':
			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){
				/* Current operator: ^= */
				pStream->zText++;
			}
			break;
		case '.':
			if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){
				/* Current operator: .= */
				pStream->zText++;
			}
			break;
		case '<':
			if( pStream->zText < pStream->zEnd ){
				if( pStream->zText[0] == '<' ){
					/* Current operator: << */
					pStream->zText++;
					if( pStream->zText < pStream->zEnd ){
						if( pStream->zText[0] == '=' ){
							/* Current operator: <<= */
							pStream->zText++;
						}else if( pStream->zText[0] == '<' ){
							/* Current Token: <<<  */
							pStream->zText++;
							/* This may be the beginning of a Heredoc/Nowdoc string,try to delimit it */
							rc = LexExtractHeredoc(&(*pStream),&(*pToken));
							if( rc == SXRET_OK ){
								/* Here/Now doc successfuly extracted */
								return SXRET_OK;
							}
						}
					}
				}else if( pStream->zText[0] == '>' ){
					/* Current operator: <> */
					pStream->zText++;
				}else if( pStream->zText[0] == '=' ){
					/* Current operator: <= */
					pStream->zText++;
				}
			}
			break;
		case '>':
			if( pStream->zText < pStream->zEnd ){
				if( pStream->zText[0] == '>' ){
					/* Current operator: >> */
					pStream->zText++;
					if( pStream->zText < pStream->zEnd && pStream->zText[0] == '=' ){
						/* Current operator: >>= */
						pStream->zText++;
					}
				}else if( pStream->zText[0] == '=' ){
					/* Current operator: >= */
					pStream->zText++;
				}
			}
			break;
		default:
			break;
		}
		if( pStr->nByte <= 0 ){
			/* Record token length */
			pStr->nByte = (sxu32)((const char *)pStream->zText-pStr->zString);
		}
		if( pToken->nType & PH7_TK_OP ){
			const ph7_expr_op *pOp;
			/* Check if the extracted token is an operator */
			pOp = PH7_ExprExtractOperator(pStr,(SyToken *)SySetPeek(pStream->pSet));
			if( pOp == 0 ){
				/* Not an operator */
				pToken->nType &= ~PH7_TK_OP;
				if( pToken->nType <= 0 ){
					pToken->nType = PH7_TK_OTHER;
				}
			}else{
				/* Save the instance associated with this operator for later processing */
				pToken->pUserData = (void *)pOp;
			}
		}
	}
	/* Tell the upper-layer to save the extracted token for later processing */
	return SXRET_OK;
}
/***** This file contains automatically generated code ******
**
** The code in this file has been automatically generated by
**
**     $Header: /sqlite/sqlite/tool/mkkeywordhash.c
**
** Sligthly modified by Chems mrad <chm@symisc.net> for the PH7 engine.
**
** The code in this file implements a function that determines whether
** or not a given identifier is really a PHP keyword.  The same thing
** might be implemented more directly using a hand-written hash table.
** But by using this automatically generated code, the size of the code
** is substantially reduced.  This is important for embedded applications
** on platforms with limited memory.
*/
/* Hash score: 103 */
static sxu32 KeywordCode(const char *z, int n){
  /* zText[] encodes 532 bytes of keywords in 333 bytes */
  /*   extendswitchprintegerequire_oncenddeclareturnamespacechobject      */
  /*   hrowbooleandefaultrycaselfinalistaticlonewconstringlobaluse        */
  /*   lseifloatvarrayANDIEchoUSECHOabstractclasscontinuendifunction      */
  /*   diendwhilevaldoexitgotoimplementsinclude_oncemptyinstanceof        */
  /*   interfacendforeachissetparentprivateprotectedpublicatchunset       */
  /*   xorARRAYASArrayEXITUNSETXORbreak                                   */
  static const char zText[332] = {
    'e','x','t','e','n','d','s','w','i','t','c','h','p','r','i','n','t','e',
    'g','e','r','e','q','u','i','r','e','_','o','n','c','e','n','d','d','e',
    'c','l','a','r','e','t','u','r','n','a','m','e','s','p','a','c','e','c',
    'h','o','b','j','e','c','t','h','r','o','w','b','o','o','l','e','a','n',
    'd','e','f','a','u','l','t','r','y','c','a','s','e','l','f','i','n','a',
    'l','i','s','t','a','t','i','c','l','o','n','e','w','c','o','n','s','t',
    'r','i','n','g','l','o','b','a','l','u','s','e','l','s','e','i','f','l',
    'o','a','t','v','a','r','r','a','y','A','N','D','I','E','c','h','o','U',
    'S','E','C','H','O','a','b','s','t','r','a','c','t','c','l','a','s','s',
    'c','o','n','t','i','n','u','e','n','d','i','f','u','n','c','t','i','o',
    'n','d','i','e','n','d','w','h','i','l','e','v','a','l','d','o','e','x',
    'i','t','g','o','t','o','i','m','p','l','e','m','e','n','t','s','i','n',
    'c','l','u','d','e','_','o','n','c','e','m','p','t','y','i','n','s','t',
    'a','n','c','e','o','f','i','n','t','e','r','f','a','c','e','n','d','f',
    'o','r','e','a','c','h','i','s','s','e','t','p','a','r','e','n','t','p',
    'r','i','v','a','t','e','p','r','o','t','e','c','t','e','d','p','u','b',
    'l','i','c','a','t','c','h','u','n','s','e','t','x','o','r','A','R','R',
    'A','Y','A','S','A','r','r','a','y','E','X','I','T','U','N','S','E','T',
    'X','O','R','b','r','e','a','k'
  };
  static const unsigned char aHash[151] = {
       0,   0,   4,  83,   0,  61,  39,  12,   0,  33,  77,   0,  48,
       0,   2,  65,  67,   0,   0,   0,  47,   0,   0,  40,   0,  15,
      74,   0,  51,   0,  76,   0,   0,  20,   0,   0,   0,  50,   0,
      80,  34,   0,  36,   0,   0,  64,  16,   0,   0,  17,   0,   1,
      19,  84,  66,   0,  43,  45,  78,   0,   0,  53,  56,   0,   0,
       0,  23,  49,   0,   0,  13,  31,  54,   7,   0,   0,  25,   0,
      72,  14,   0,  71,   0,  38,   6,   0,   0,   0,  73,   0,   0,
       3,   0,  41,   5,  52,  57,  32,   0,  60,  63,   0,  69,  82,
      30,   0,  79,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  81,   0,   0,
      62,   0,  11,   0,   0,  58,   0,   0,   0,   0,  59,  75,   0,
       0,   0,   0,   0,   0,  35,  27,   0
  };
  static const unsigned char aNext[84] = {
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   8,   0,   0,   0,  10,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,  28,   0,   0,   0,   0,   0,   0,   0,   0,
       0,   0,   0,   0,   0,  44,   0,  18,   0,   0,   0,   0,   0,
       0,  46,   0,  29,   0,   0,   0,  22,   0,   0,   0,   0,  26,
       0,  21,  24,   0,   0,  68,   0,   0,   9,  37,   0,   0,   0,
      42,   0,   0,   0,  70,  55
  };
  static const unsigned char aLen[84] = {
       7,   9,   6,   5,   7,  12,   7,   2,  10,   7,   6,   9,   4,
       6,   5,   7,   4,   3,   7,   3,   4,   4,   5,   4,   6,   5,
       2,   3,   5,   6,   6,   3,   6,   4,   2,   5,   3,   5,   3,
       3,   4,   3,   4,   8,   5,   2,   8,   5,   8,   3,   8,   5,
       4,   2,   4,   4,  10,  12,   7,   5,  10,   9,   3,   6,  10,
       3,   7,   2,   5,   6,   7,   9,   6,   5,   5,   3,   5,   2,
       5,   4,   5,   3,   2,   5
  };
  static const sxu16 aOffset[84] = {
       0,   3,   6,  12,  14,  20,  20,  21,  31,  34,  39,  44,  52,
      55,  60,  65,  65,  70,  72,  78,  81,  83,  86,  90,  92,  97,
     100, 100, 103, 106, 111, 117, 119, 119, 123, 124, 129, 130, 135,
     137, 139, 143, 145, 149, 157, 159, 162, 169, 173, 181, 183, 186,
     190, 194, 196, 200, 204, 214, 214, 225, 230, 240, 240, 248, 248,
     251, 251, 252, 258, 263, 269, 276, 285, 290, 295, 300, 303, 308,
     310, 315, 319, 324, 325, 327
  };
  static const sxu32 aCode[84] = {
    PH7_TKWRD_EXTENDS,   PH7_TKWRD_ENDSWITCH,   PH7_TKWRD_SWITCH,    PH7_TKWRD_PRINT,   PH7_TKWRD_INT, 
    PH7_TKWRD_REQONCE,   PH7_TKWRD_REQUIRE,     PH7_TKWRD_SEQ,       PH7_TKWRD_ENDDEC,    PH7_TKWRD_DECLARE,   
    PH7_TKWRD_RETURN,    PH7_TKWRD_NAMESPACE,   PH7_TKWRD_ECHO,      PH7_TKWRD_OBJECT,    PH7_TKWRD_THROW, 
    PH7_TKWRD_BOOL,      PH7_TKWRD_BOOL,        PH7_TKWRD_AND,       PH7_TKWRD_DEFAULT,   PH7_TKWRD_TRY, 
    PH7_TKWRD_CASE,      PH7_TKWRD_SELF,        PH7_TKWRD_FINAL,     PH7_TKWRD_LIST,      PH7_TKWRD_STATIC,  
    PH7_TKWRD_CLONE,     PH7_TKWRD_SNE,         PH7_TKWRD_NEW,       PH7_TKWRD_CONST,     PH7_TKWRD_STRING,  
    PH7_TKWRD_GLOBAL,    PH7_TKWRD_USE,         PH7_TKWRD_ELIF,      PH7_TKWRD_ELSE,      PH7_TKWRD_IF,  
    PH7_TKWRD_FLOAT,     PH7_TKWRD_VAR,         PH7_TKWRD_ARRAY,     PH7_TKWRD_AND,       PH7_TKWRD_DIE, 
    PH7_TKWRD_ECHO,      PH7_TKWRD_USE,         PH7_TKWRD_ECHO,      PH7_TKWRD_ABSTRACT,  PH7_TKWRD_CLASS, 
    PH7_TKWRD_AS,        PH7_TKWRD_CONTINUE,    PH7_TKWRD_ENDIF,     PH7_TKWRD_FUNCTION,  PH7_TKWRD_DIE, 
    PH7_TKWRD_ENDWHILE,  PH7_TKWRD_WHILE,       PH7_TKWRD_EVAL,      PH7_TKWRD_DO,        PH7_TKWRD_EXIT,
    PH7_TKWRD_GOTO,      PH7_TKWRD_IMPLEMENTS,  PH7_TKWRD_INCONCE,   PH7_TKWRD_INCLUDE,   PH7_TKWRD_EMPTY, 
    PH7_TKWRD_INSTANCEOF,PH7_TKWRD_INTERFACE,   PH7_TKWRD_INT,       PH7_TKWRD_ENDFOR,    PH7_TKWRD_END4EACH,    
    PH7_TKWRD_FOR,       PH7_TKWRD_FOREACH,     PH7_TKWRD_OR,        PH7_TKWRD_ISSET,     PH7_TKWRD_PARENT,  
    PH7_TKWRD_PRIVATE,   PH7_TKWRD_PROTECTED,   PH7_TKWRD_PUBLIC,    PH7_TKWRD_CATCH,     PH7_TKWRD_UNSET, 
    PH7_TKWRD_XOR,       PH7_TKWRD_ARRAY,       PH7_TKWRD_AS,        PH7_TKWRD_ARRAY,     PH7_TKWRD_EXIT,
    PH7_TKWRD_UNSET,     PH7_TKWRD_XOR,         PH7_TKWRD_OR,        PH7_TKWRD_BREAK 
  };
  int h, i;
  if( n<2 ) return PH7_TK_ID;
  h = (((int)z[0]*4) ^ ((int)z[n-1]*3) ^ n) % 151;
  for(i=((int)aHash[h])-1; i>=0; i=((int)aNext[i])-1){
    if( (int)aLen[i]==n && SyMemcmp(&zText[aOffset[i]],z,n)==0 ){
       /* PH7_TKWRD_EXTENDS */
       /* PH7_TKWRD_ENDSWITCH */
       /* PH7_TKWRD_SWITCH */
       /* PH7_TKWRD_PRINT */
       /* PH7_TKWRD_INT */
       /* PH7_TKWRD_REQONCE */
       /* PH7_TKWRD_REQUIRE */
       /* PH7_TKWRD_SEQ */
       /* PH7_TKWRD_ENDDEC */
       /* PH7_TKWRD_DECLARE */
       /* PH7_TKWRD_RETURN */
       /* PH7_TKWRD_NAMESPACE */
       /* PH7_TKWRD_ECHO */
       /* PH7_TKWRD_OBJECT */
       /* PH7_TKWRD_THROW */
       /* PH7_TKWRD_BOOL */
       /* PH7_TKWRD_BOOL */
       /* PH7_TKWRD_AND */
       /* PH7_TKWRD_DEFAULT */
       /* PH7_TKWRD_TRY */
       /* PH7_TKWRD_CASE */
       /* PH7_TKWRD_SELF */
       /* PH7_TKWRD_FINAL */
       /* PH7_TKWRD_LIST */
       /* PH7_TKWRD_STATIC */
       /* PH7_TKWRD_CLONE */
       /* PH7_TKWRD_SNE */
       /* PH7_TKWRD_NEW */
       /* PH7_TKWRD_CONST */
       /* PH7_TKWRD_STRING */
       /* PH7_TKWRD_GLOBAL */
       /* PH7_TKWRD_USE */
       /* PH7_TKWRD_ELIF */
       /* PH7_TKWRD_ELSE */
       /* PH7_TKWRD_IF */
       /* PH7_TKWRD_FLOAT */
       /* PH7_TKWRD_VAR */
       /* PH7_TKWRD_ARRAY */
       /* PH7_TKWRD_AND */
       /* PH7_TKWRD_DIE */
       /* PH7_TKWRD_ECHO */
       /* PH7_TKWRD_USE */
       /* PH7_TKWRD_ECHO */
       /* PH7_TKWRD_ABSTRACT */
       /* PH7_TKWRD_CLASS */
       /* PH7_TKWRD_AS */
       /* PH7_TKWRD_CONTINUE */
       /* PH7_TKWRD_ENDIF */
       /* PH7_TKWRD_FUNCTION */
       /* PH7_TKWRD_DIE */
       /* PH7_TKWRD_ENDWHILE */
       /* PH7_TKWRD_WHILE */
       /* PH7_TKWRD_EVAL */
       /* PH7_TKWRD_DO */
       /* PH7_TKWRD_EXIT */
       /* PH7_TKWRD_GOTO */
       /* PH7_TKWRD_IMPLEMENTS */
       /* PH7_TKWRD_INCONCE */
       /* PH7_TKWRD_INCLUDE */
       /* PH7_TKWRD_EMPTY */
       /* PH7_TKWRD_INSTANCEOF */
       /* PH7_TKWRD_INTERFACE */
       /* PH7_TKWRD_INT */
       /* PH7_TKWRD_ENDFOR */
       /* PH7_TKWRD_END4EACH */
       /* PH7_TKWRD_FOR */
       /* PH7_TKWRD_FOREACH */
       /* PH7_TKWRD_OR */
       /* PH7_TKWRD_ISSET */
       /* PH7_TKWRD_PARENT */
       /* PH7_TKWRD_PRIVATE */
       /* PH7_TKWRD_PROTECTED */
       /* PH7_TKWRD_PUBLIC */
       /* PH7_TKWRD_CATCH */
       /* PH7_TKWRD_UNSET */
       /* PH7_TKWRD_XOR */
       /* PH7_TKWRD_ARRAY */
       /* PH7_TKWRD_AS */
       /* PH7_TKWRD_ARRAY */
       /* PH7_TKWRD_EXIT */
       /* PH7_TKWRD_UNSET */
       /* PH7_TKWRD_XOR */
       /* PH7_TKWRD_OR */
       /* PH7_TKWRD_BREAK */
      return aCode[i];
    }
  }
  return PH7_TK_ID;
}
/* --- End of Automatically generated code --- */
/*
 * Extract a heredoc/nowdoc text from a raw PHP input.
 * According to the PHP language reference manual:
 *  A third way to delimit strings is the heredoc syntax: <<<. After this operator, an identifier
 *  is provided, then a newline. The string itself follows, and then the same identifier again
 *  to close the quotation.
 *  The closing identifier must begin in the first column of the line. Also, the identifier must 
 *  follow the same naming rules as any other label in PHP: it must contain only alphanumeric 
 *  characters and underscores, and must start with a non-digit character or underscore. 
 *  Heredoc text behaves just like a double-quoted string, without the double quotes.
 *  This means that quotes in a heredoc do not need to be escaped, but the escape codes listed
 *  above can still be used. Variables are expanded, but the same care must be taken when expressing
 *  complex variables inside a heredoc as with strings. 
 *  Nowdocs are to single-quoted strings what heredocs are to double-quoted strings.
 *  A nowdoc is specified similarly to a heredoc, but no parsing is done inside a nowdoc.
 *  The construct is ideal for embedding PHP code or other large blocks of text without the need
 *  for escaping. It shares some features in common with the SGML <![CDATA[ ]]> construct, in that
 *  it declares a block of text which is not for parsing.
 *  A nowdoc is identified with the same <<< sequence used for heredocs, but the identifier which follows
 *  is enclosed in single quotes, e.g. <<<'EOT'. All the rules for heredoc identifiers also apply to nowdoc
 *  identifiers, especially those regarding the appearance of the closing identifier. 
 * Symisc Extension:
 * The closing delimiter can now start with a digit or undersocre or it can be an UTF-8 stream.
 * Example:
 *  <<<123
 *    HEREDOC Here
 * 123
 *  or
 *  <<<___
 *   HEREDOC Here
 *  ___
 */
static sxi32 LexExtractHeredoc(SyStream *pStream,SyToken *pToken)
{
	const unsigned char *zIn  = pStream->zText;
	const unsigned char *zEnd = pStream->zEnd;
	const unsigned char *zPtr;
	sxu8 bNowDoc = FALSE;
	SyString sDelim;
	SyString sStr;
	/* Jump leading white spaces */
	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){
		zIn++;
	}
	if( zIn >= zEnd ){
		/* A simple symbol,return immediately */
		return SXERR_CONTINUE;
	}
	if( zIn[0] == '\'' || zIn[0] == '"' ){
		/* Make sure we are dealing with a nowdoc */
		bNowDoc =  zIn[0] == '\'' ? TRUE : FALSE;
		zIn++;
	}
	if( zIn[0] < 0xc0 && !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){
		/* Invalid delimiter,return immediately */
		return SXERR_CONTINUE;
	}
	/* Isolate the identifier */
	sDelim.zString = (const char *)zIn;
	for(;;){
		zPtr = zIn;
		/* Skip alphanumeric stream */
		while( zPtr < zEnd && zPtr[0] < 0xc0 && (SyisAlphaNum(zPtr[0]) || zPtr[0] == '_') ){
			zPtr++;
		}
		if( zPtr < zEnd && zPtr[0] >= 0xc0 ){
			zPtr++;
			/* UTF-8 stream */
			while( zPtr < zEnd && ((zPtr[0] & 0xc0) == 0x80) ){
				zPtr++;
			}
		}
		if( zPtr == zIn ){
			/* Not an UTF-8 or alphanumeric stream */
			break;
		}
		/* Synchronize pointers */
		zIn = zPtr;
	}
	/* Get the identifier length */
	sDelim.nByte = (sxu32)((const char *)zIn-sDelim.zString);
	if( zIn[0] == '"' || (bNowDoc && zIn[0] == '\'') ){
		/* Jump the trailing single quote */
		zIn++;
	}
	/* Jump trailing white spaces */
	while( zIn < zEnd && zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){
		zIn++;
	}
	if( sDelim.nByte <= 0 || zIn >= zEnd || zIn[0] != '\n' ){
		/* Invalid syntax */
		return SXERR_CONTINUE;
	}
	pStream->nLine++; /* Increment line counter */
	zIn++;
	/* Isolate the delimited string */
	sStr.zString = (const char *)zIn;
	/* Go and found the closing delimiter */
	for(;;){
		/* Synchronize with the next line */
		while( zIn < zEnd && zIn[0] != '\n' ){
			zIn++;
		}
		if( zIn >= zEnd ){
			/* End of the input reached, break immediately */
			pStream->zText = pStream->zEnd;
			break;
		}
		pStream->nLine++; /* Increment line counter */
		zIn++;
		if( (sxu32)(zEnd - zIn) >= sDelim.nByte && SyMemcmp((const void *)sDelim.zString,(const void *)zIn,sDelim.nByte) == 0 ){
			zPtr = &zIn[sDelim.nByte];
			while( zPtr < zEnd && zPtr[0] < 0xc0 && SyisSpace(zPtr[0]) && zPtr[0] != '\n' ){
				zPtr++;
			}
			if( zPtr >= zEnd ){
				/* End of input */
				pStream->zText = zPtr;
				break;
			}
			if( zPtr[0] == ';' ){
				const unsigned char *zCur = zPtr;
				zPtr++;
				while( zPtr < zEnd && zPtr[0] < 0xc0 && SyisSpace(zPtr[0]) && zPtr[0] != '\n' ){
					zPtr++;
				}
				if( zPtr >= zEnd || zPtr[0] == '\n' ){
					/* Closing delimiter found,break immediately */
					pStream->zText = zCur; /* Keep the semi-colon */
					break;
				}
			}else if( zPtr[0] == '\n' ){
				/* Closing delimiter found,break immediately */
				pStream->zText = zPtr; /* Synchronize with the stream cursor */
				break;
			}
			/* Synchronize pointers and continue searching */
			zIn = zPtr;
		}
	} /* For(;;) */
	/* Get the delimited string length */
	sStr.nByte = (sxu32)((const char *)zIn-sStr.zString);
	/* Record token type and length */
	pToken->nType = bNowDoc ? PH7_TK_NOWDOC : PH7_TK_HEREDOC;
	SyStringDupPtr(&pToken->sData,&sStr);
	/* Remove trailing white spaces */
	SyStringRightTrim(&pToken->sData);
	/* All done */
	return SXRET_OK;
}
/*
 * Tokenize a raw PHP input.
 * This is the public tokenizer called by most code generator routines. 
 */
PH7_PRIVATE sxi32 PH7_TokenizePHP(const char *zInput,sxu32 nLen,sxu32 nLineStart,SySet *pOut)
{
	SyLex sLexer;
	sxi32 rc;
	/* Initialize the lexer */
	rc = SyLexInit(&sLexer,&(*pOut),TokenizePHP,0);
	if( rc != SXRET_OK ){
		return rc;
	}
	sLexer.sStream.nLine = nLineStart;
	/* Tokenize input */
	rc = SyLexTokenizeInput(&sLexer,zInput,nLen,0,0,0);
	/* Release the lexer */
	SyLexRelease(&sLexer);
	/* Tokenization result */
	return rc;
}
/*
 * High level public tokenizer.
 *  Tokenize the input into PHP tokens and raw tokens [i.e: HTML,XML,Raw text...].
 * According to the PHP language reference manual
 *   When PHP parses a file, it looks for opening and closing tags, which tell PHP
 *   to start and stop interpreting the code between them. Parsing in this manner allows
 *   PHP to be embedded in all sorts of different documents, as everything outside of a pair
 *   of opening and closing tags is ignored by the PHP parser. Most of the time you will see
 *   PHP embedded in HTML documents, as in this example. 
 *   <?php echo 'While this is going to be parsed.'; ?>
 *   <p>This will also be ignored.</p>
 *   You can also use more advanced structures:
 *   Example #1 Advanced escaping
 * <?php
 * if ($expression) {
 *   ?>
 *   <strong>This is true.</strong>
 *   <?php
 * } else {
 *   ?>
 *   <strong>This is false.</strong>
 *   <?php
 * }
 * ?>
 * This works as expected, because when PHP hits the ?> closing tags, it simply starts outputting
 * whatever it finds (except for an immediately following newline - see instruction separation ) until it hits
 * another opening tag. The example given here is contrived, of course, but for outputting large blocks of text
 * dropping out of PHP parsing mode is generally more efficient than sending all of the text through echo() or print().
 * There are four different pairs of opening and closing tags which can be used in PHP. Three of those, <?php ?> 
 * <script language="php"> </script>  and <? ?> are always available. The other two are short tags and ASP style
 * tags, and can be turned on and off from the php.ini configuration file. As such, while some people find short tags
 * and ASP style tags convenient, they are less portable, and generally not recommended.
 * Note:
 * Also note that if you are embedding PHP within XML or XHTML you will need to use the <?php ?> tags to remain
 * compliant with standards.
 * Example #2 PHP Opening and Closing Tags
 * 1.  <?php echo 'if you want to serve XHTML or XML documents, do it like this'; ?>
 * 2.  <script language="php">
 *       echo 'some editors (like FrontPage) don\'t
 *             like processing instructions';
 *   </script>
 *
 * 3.  <? echo 'this is the simplest, an SGML processing instruction'; ?>
 *   <?= expression ?> This is a shortcut for "<? echo expression ?>"
 */
PH7_PRIVATE sxi32 PH7_TokenizeRawText(const char *zInput,sxu32 nLen,SySet *pOut)
{
	const char *zEnd = &zInput[nLen];
	const char *zIn  = zInput;
	const char *zCur,*zCurEnd;
	SyString sCtag = { 0, 0 };     /* Closing tag */
	SyToken sToken;
	SyString sDoc;
	sxu32 nLine;
	sxi32 iNest;
	sxi32 rc;
	/* Tokenize the input into PHP tokens and raw tokens */
	nLine = 1;
	zCur = zCurEnd   = 0; /* Prevent compiler warning */
	sToken.pUserData = 0;
	iNest = 0;
	sDoc.nByte = 0;
	sDoc.zString = ""; /* cc warning */
	for(;;){
		if( zIn >= zEnd ){
			/* End of input reached */
			break;
		}
		sToken.nLine = nLine;
		zCur = zIn;
		zCurEnd = 0;
		while( zIn < zEnd ){
			 if( zIn[0] == '<' ){
				const char *zTmp = zIn; /* End of raw input marker */
				zIn++;
				if( zIn < zEnd ){
					if( zIn[0] == '?' ){
						zIn++;
						if( (sxu32)(zEnd - zIn) >= sizeof("php")-1 &&  SyStrnicmp(zIn,"php",sizeof("php")-1) == 0 ){
							/* opening tag: <?php */ 
							zIn += sizeof("php")-1;
						}
						/* Look for the closing tag '?>' */
						SyStringInitFromBuf(&sCtag,"?>",sizeof("?>")-1);
						zCurEnd = zTmp;
						break;
					}
				}
			}else{
				if( zIn[0] == '\n' ){
					nLine++;
				}
				zIn++;
			 }
		} /* While(zIn < zEnd) */
		if( zCurEnd == 0 ){
			zCurEnd = zIn;
		}
		/* Save the raw token */
		SyStringInitFromBuf(&sToken.sData,zCur,zCurEnd - zCur);
		sToken.nType = PH7_TOKEN_RAW;
		rc = SySetPut(&(*pOut),(const void *)&sToken);
		if( rc != SXRET_OK ){
			return rc;
		}
		if( zIn >= zEnd ){
			break;
		}
		/* Ignore leading white space */
		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){
			if( zIn[0] == '\n' ){
				nLine++;
			}
			zIn++;
		}
		/* Delimit the PHP chunk */
		sToken.nLine = nLine;
		zCur = zIn;
		while( (sxu32)(zEnd - zIn) >= sCtag.nByte ){
			const char *zPtr;
			if( SyMemcmp(zIn,sCtag.zString,sCtag.nByte) == 0 && iNest < 1 ){
				break;
			}
			for(;;){
				if( zIn[0] != '/' || (zIn[1] != '*' && zIn[1] != '/') /* && sCtag.nByte >= 2 */ ){
					break;
				}
				zIn += 2;
				if( zIn[-1] == '/' ){
					/* Inline comment */
					while( zIn < zEnd && zIn[0] != '\n' ){
						zIn++;
					}
					if( zIn >= zEnd ){
						zIn--;
					}
				}else{
					/* Block comment */
					while( (sxu32)(zEnd-zIn) >= sizeof("*/") - 1 ){
						if( zIn[0] == '*' && zIn[1] == '/' ){
							zIn += 2;
							break;
						}
						if( zIn[0] == '\n' ){
							nLine++;
						}
						zIn++;
					}
				}
			}
			if( zIn[0] == '\n' ){
				nLine++;
				if( iNest > 0 ){
					zIn++;
					while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){
						zIn++;
					}
					zPtr = zIn;
					while( zIn < zEnd ){
						if( (unsigned char)zIn[0] >= 0xc0 ){
							/* UTF-8 stream */
							zIn++;
							SX_JMP_UTF8(zIn,zEnd);
						}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){
							break;
						}else{
							zIn++;
						}
					}
					if( (sxu32)(zIn - zPtr) == sDoc.nByte && SyMemcmp(sDoc.zString,zPtr,sDoc.nByte) == 0 ){
						iNest = 0;
					}
					continue;
				}
			}else if ( (sxu32)(zEnd - zIn) >= sizeof("<<<") && zIn[0] == '<' && zIn[1] == '<' && zIn[2] == '<' && iNest < 1){
				zIn += sizeof("<<<")-1;
				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) && zIn[0] != '\n' ){
					zIn++;
				}
				if( zIn[0] == '"' || zIn[0] == '\'' ){
					zIn++;
				}
				zPtr = zIn;
				while( zIn < zEnd ){
					if( (unsigned char)zIn[0] >= 0xc0 ){
						/* UTF-8 stream */
						zIn++;
						SX_JMP_UTF8(zIn,zEnd);
					}else if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){
						break;
					}else{
						zIn++;
					}
				}
				SyStringInitFromBuf(&sDoc,zPtr,zIn-zPtr);
				SyStringFullTrim(&sDoc);
				if( sDoc.nByte > 0 ){
					iNest++;
				}
				continue;
			}
			zIn++;
			
			if ( zIn >= zEnd )
				break;
		}
		if( (sxu32)(zEnd - zIn) < sCtag.nByte ){
			zIn = zEnd;
		}
		if( zCur < zIn ){
			/* Save the PHP chunk for later processing */
			sToken.nType = PH7_TOKEN_PHP;
			SyStringInitFromBuf(&sToken.sData,zCur,zIn-zCur);
			SyStringRightTrim(&sToken.sData); /* Trim trailing white spaces */
			rc = SySetPut(&(*pOut),(const void *)&sToken);
			if( rc != SXRET_OK ){
				return rc;
			}
		}
		if( zIn < zEnd ){
			/* Jump the trailing closing tag */
			zIn += sCtag.nByte;
		}
	} /* For(;;) */
	
 	return SXRET_OK;
}
