# src/ph7/compile_node.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 669/804 lines (83.21%)

[Root index](../../index.md) | [Directory index](index.md)

|     Hits | Line | Source |
| -------: | ---: | :--- |
|        - |    1 | `/**` |
|        - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|        - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|        - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|        - |    5 | ` */` |
|        - |    6 | `#include "ph7int.h"` |
|        - |    7 | `#include "compile_int.h"` |
|        - |    8 | `/*` |
|        - |    9 | ` * Section:` |
|        - |   10 | ` *    Expression-node compilation: anonymous functions/closures, arrow` |
|        - |   11 | ` *    functions and their capture scan, match expressions, backtick,` |
|        - |   12 | ` *    language constructs, variables and literal/name resolution.` |
|        - |   13 | ` * Status:` |
|        - |   14 | ` *    Stable.` |
|        - |   15 | ` */` |
|        - |   16 | `/*` |
|        - |   17 | ` * Compile an annoynmous function or a closure.` |
|        - |   18 | ` * According to the PHP language reference` |
|        - |   19 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|        - |   20 | ` *  which have no specified name. They are most useful as the value of callback` |
|        - |   21 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|        - |   22 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|        - |   23 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|        - |   24 | ` *  Example Anonymous function variable assignment example` |
|        - |   25 | ` * <?php` |
|        - |   26 | ` * $greet = function($name)` |
|        - |   27 | ` * {` |
|        - |   28 | ` *    printf("Hello %s\r\n", $name);` |
|        - |   29 | ` * };` |
|        - |   30 | ` * $greet('World');` |
|        - |   31 | ` * $greet('PHP');` |
|        - |   32 | ` * ?>` |
|        - |   33 | ` * Note that the implementation of annoynmous function and closure under` |
|        - |   34 | ` * PH7 is completely different from the one used by the zend engine.` |
|        - |   35 | ` */` |
|      580 |   36 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |   37 | `{` |
|      585 |   38 | `	ph7_vm_func *pAnnonFunc = 0; /* Annonymous function body */` |
|        - |   39 | `	char zName[512];         /* Unique lambda name */` |
|        - |   40 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|        - |   41 | `							  * one thread is allowed to compile the script.` |
|        - |   42 | `						      */` |
|        - |   43 | `	SyString sName;` |
|      585 |   44 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `$f = #[A] function…` trivia`` |
|        - |   45 | `	                              * is keyed to this ['static'] 'function' token */` |
|        - |   46 | `	sxu32 nKwLine;` |
|      585 |   47 | `	sxi32 iFlags = 0;` |
|        - |   48 | `	sxu32 nLen;` |
|        - |   49 | `	sxi32 rc;` |
|      290 |   50 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |   51 |  |
|      585 |   52 | `	nKwLine = pGen->pIn->nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|      580 |   53 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      585 |   54 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        - |   55 | `		/* Static closure: no $this auto-capture, bind refused */` |
|       23 |   56 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|       23 |   57 | `		pGen->pIn++; /* Jump the 'static' keyword */` |
|       11 |   58 | `	}` |
|      585 |   59 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|      585 |   60 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|      ! 0 |   61 | `		pGen->pIn++;` |
|      ! 0 |   62 | `	}` |
|        - |   63 | `	/* Generate a unique name */` |
|      585 |   64 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|        - |   65 | `	/* Make sure the generated name is unique */` |
|      585 |   66 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|      ! 0 |   67 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|      ! 0 |   68 | `	}` |
|      585 |   69 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|        - |   70 | `	/* Compile the lambda body */` |
|      585 |   71 | `	rc = GenStateCompileFunc(&(*pGen),&sName,iFlags,TRUE,&pAnnonFunc);` |
|      585 |   72 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |   73 | `		return SXERR_ABORT;` |
|        - |   74 | `	}` |
|      585 |   75 | `	if( pAnnonFunc ){` |
|      585 |   76 | `		pAnnonFunc->nLine = nKwLine;` |
|        - |   77 | ``		/* Expression-position attributes (`$f = #[A] function () {}`): the trivia`` |
|        - |   78 | `		 * sidecar keys them to the closure's first keyword token. */` |
|      585 |   79 | `		if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnnonFunc->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |   80 | `			return SXERR_ABORT;` |
|        - |   81 | `		}` |
|      290 |   82 | `	}` |
|        - |   83 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|        - |   84 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|        - |   85 | `	 * the handler wraps either in a Closure instance. */` |
|      585 |   86 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|        - |   87 | `	/* Node successfully compiled */` |
|      585 |   88 | `	return SXRET_OK;` |
|      295 |   89 | `}` |
|        - |   90 | `/*` |
|        - |   91 | ` * Add a free variable to the arrow function's closure environment, unless` |
|        - |   92 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|        - |   93 | ` * enclosing arrow level, or has already been captured.` |
|        - |   94 | ` */` |
|      270 |   95 | `static sxi32 GenStateArrowAddCapture(` |
|        - |   96 | `	ph7_gen_state *pGen,` |
|        - |   97 | `	ph7_vm_func *pFunc,` |
|        - |   98 | `	const char *zName,` |
|        - |   99 | `	sxu32 nByte,` |
|        - |  100 | `	SyString *aShadow,` |
|        - |  101 | `	sxu32 nShadow)` |
|        3 |  102 | `{` |
|        - |  103 | `	ph7_vm_func_closure_env sEnv;` |
|        - |  104 | `	ph7_vm_func_closure_env *aEnv;` |
|        - |  105 | `	sxu32 n, nEnv;` |
|        - |  106 | `	char *zDup;` |
|      273 |  107 | `	if( nByte == 0 ){` |
|      ! 0 |  108 | `		return SXRET_OK;` |
|        - |  109 | `	}` |
|      270 |  110 | `	if( nByte == sizeof("this")-1` |
|      147 |  111 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|        9 |  112 | `		return SXRET_OK;` |
|        - |  113 | `	}` |
|      333 |  114 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|      254 |  115 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|      245 |  116 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|      189 |  117 | `			return SXRET_OK;` |
|        - |  118 | `		}` |
|       36 |  119 | `	}` |
|       77 |  120 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       77 |  121 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|      105 |  122 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|       30 |  123 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|       29 |  124 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|        3 |  125 | `			return SXRET_OK;` |
|        - |  126 | `		}` |
|       15 |  127 | `	}` |
|       75 |  128 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|       75 |  129 | `	if( zDup == 0 ){` |
|      ! 0 |  130 | `		return SXERR_ABORT;` |
|        - |  131 | `	}` |
|       75 |  132 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       75 |  133 | `	sEnv.iFlags = 0;` |
|       75 |  134 | `	sEnv.nIdx = SXU32_HIGH;` |
|       75 |  135 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       75 |  136 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|       75 |  137 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       75 |  138 | `	return SXRET_OK;` |
|      138 |  139 | `}` |
|        - |  140 | `/*` |
|        - |  141 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|        - |  142 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|        - |  143 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|        - |  144 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|        - |  145 | ` */` |
|      122 |  146 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|        - |  147 | `	ph7_gen_state *pGen,` |
|        - |  148 | `	ph7_vm_func *pFunc,` |
|        - |  149 | `	const char *zIn,` |
|        - |  150 | `	const char *zEnd,` |
|        - |  151 | `	SyString *aShadow,` |
|        - |  152 | `	sxu32 nShadow)` |
|        3 |  153 | `{` |
|        - |  154 | `	sxi32 rc;` |
|      625 |  155 | `	while( zIn < zEnd ){` |
|      503 |  156 | `		if( zIn[0] == '\\' ){` |
|       13 |  157 | `			zIn++;` |
|       13 |  158 | `			if( zIn < zEnd ){` |
|       13 |  159 | `				zIn++;` |
|        6 |  160 | `			}` |
|       13 |  161 | `			continue;` |
|        - |  162 | `		}` |
|      488 |  163 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|       27 |  164 | `			&& ((unsigned char)zIn[1] >= 0xc0` |
|       24 |  165 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|        - |  166 | `			const char *zName;` |
|       26 |  167 | `			zIn++; /* skip '$' */` |
|       26 |  168 | `			zName = zIn;` |
|       82 |  169 | `			while( zIn < zEnd ){` |
|       76 |  170 | `				unsigned char c = (unsigned char)zIn[0];` |
|       76 |  171 | `				if( c >= 0xc0 ){` |
|      ! 0 |  172 | `					zIn++;` |
|      ! 0 |  173 | `					while( zIn < zEnd` |
|      ! 0 |  174 | `						&& (((unsigned char)zIn[0] & 0xc0) == 0x80) ){` |
|      ! 0 |  175 | `						zIn++;` |
|      ! 0 |  176 | `					}` |
|      ! 0 |  177 | `					continue;` |
|        - |  178 | `				}` |
|       76 |  179 | `				if( !SyisAlphaNum(zIn[0]) && zIn[0] != '_' ){` |
|       20 |  180 | `					break;` |
|        - |  181 | `				}` |
|       58 |  182 | `				zIn++;` |
|        2 |  183 | `			}` |
|       26 |  184 | `			if( zIn > zName ){` |
|       38 |  185 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|       24 |  186 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|       26 |  187 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  188 | `					return SXERR_ABORT;` |
|        - |  189 | `				}` |
|       12 |  190 | `			}` |
|       26 |  191 | `			continue;` |
|        - |  192 | `		}` |
|      467 |  193 | `		zIn++;` |
|        3 |  194 | `	}` |
|      125 |  195 | `	return SXRET_OK;` |
|       64 |  196 | `}` |
|        - |  197 | `/*` |
|        - |  198 | ` * Scan the body token range of an arrow function for free-variable` |
|        - |  199 | ` * references and record them in pFunc's closure environment. Handles:` |
|        - |  200 | ` *   - plain $<id> pairs` |
|        - |  201 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|        - |  202 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|        - |  203 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|        - |  204 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|        - |  205 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|        - |  206 | ` *     are never mistakenly captured.` |
|        - |  207 | ` */` |
|      568 |  208 | `static sxi32 GenStateArrowCaptureScan(` |
|        - |  209 | `	ph7_gen_state *pGen,` |
|        - |  210 | `	ph7_vm_func *pFunc,` |
|        - |  211 | `	SyToken *pStart,` |
|        - |  212 | `	SyToken *pEnd,` |
|        - |  213 | `	SyString *aShadow,` |
|        - |  214 | `	sxu32 nShadow)` |
|        4 |  215 | `{` |
|      572 |  216 | `	SyToken *pScan = pStart;` |
|        - |  217 | `	sxi32 rc;` |
|     3852 |  218 | `	while( pScan < pEnd ){` |
|     3284 |  219 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|      186 |  220 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|       61 |  221 | `				pScan->sData.zString,` |
|      122 |  222 | `				pScan->sData.zString + pScan->sData.nByte,` |
|       61 |  223 | `				aShadow,nShadow);` |
|      125 |  224 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  225 | `				return SXERR_ABORT;` |
|        - |  226 | `			}` |
|      125 |  227 | `			pScan++;` |
|      125 |  228 | `			continue;` |
|        - |  229 | `		}` |
|     3162 |  230 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|       41 |  231 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|       41 |  232 | `			SyToken *pFnKw = pScan;` |
|       38 |  233 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|        2 |  234 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|        4 |  235 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|      ! 0 |  236 | `				pFnKw = &pScan[1];` |
|      ! 0 |  237 | `				nKw = PH7_TKWRD_FN;` |
|      ! 0 |  238 | `			}` |
|       41 |  239 | `			if( nKw == PH7_TKWRD_FN ){` |
|        - |  240 | `				SyToken *pInnerSigStart;` |
|        - |  241 | `				SyToken *pInnerSigEnd;` |
|        - |  242 | `				SyToken *pInnerBodyEnd;` |
|        - |  243 | `				SyString *aInnerShadow;` |
|        - |  244 | `				sxu32 nInnerShadow;` |
|        - |  245 | `				sxu32 nInnerParamMax;` |
|        - |  246 | `				SyToken *p;` |
|        - |  247 | `				int iNestInner;` |
|       26 |  248 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|       26 |  249 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|      ! 0 |  250 | `					pScan++;` |
|      ! 0 |  251 | `				}` |
|       26 |  252 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 |  253 | `					pScan++;` |
|      ! 0 |  254 | `					continue;` |
|        - |  255 | `				}` |
|       26 |  256 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|       26 |  257 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|        - |  258 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|       26 |  259 | `				if( pInnerSigEnd >= pEnd ){` |
|      ! 0 |  260 | `					pScan = pEnd;` |
|      ! 0 |  261 | `					continue;` |
|        - |  262 | `				}` |
|        - |  263 | `				/* Build an augmented shadow list: inherited + inner params */` |
|       26 |  264 | `				nInnerParamMax = 0;` |
|       76 |  265 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|       52 |  266 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|       20 |  267 | `						nInnerParamMax++;` |
|        9 |  268 | `					}` |
|       27 |  269 | `				}` |
|       26 |  270 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|       24 |  271 | `					&pGen->pVm->sAllocator,` |
|       24 |  272 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|       26 |  273 | `				if( aInnerShadow == 0 ){` |
|      ! 0 |  274 | `					return SXERR_ABORT;` |
|        - |  275 | `				}` |
|       26 |  276 | `				nInnerShadow = 0;` |
|       32 |  277 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|        7 |  278 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|        4 |  279 | `				}` |
|       76 |  280 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|       52 |  281 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|       34 |  282 | `						continue;` |
|        - |  283 | `					}` |
|       20 |  284 | `					if( &p[1] >= pInnerSigEnd ){` |
|      ! 0 |  285 | `						break;` |
|        - |  286 | `					}` |
|       20 |  287 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  288 | `						continue;` |
|        - |  289 | `					}` |
|       20 |  290 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|       11 |  291 | `				}` |
|       26 |  292 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|       26 |  293 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|      ! 0 |  294 | `					pScan++;` |
|      ! 0 |  295 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|      ! 0 |  296 | `						&& pScan->sData.nByte == 1` |
|      ! 0 |  297 | `						&& pScan->sData.zString[0] == '?' ){` |
|      ! 0 |  298 | `						pScan++;` |
|      ! 0 |  299 | `					}` |
|      ! 0 |  300 | `					if( pScan < pEnd` |
|      ! 0 |  301 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|      ! 0 |  302 | `						pScan++;` |
|      ! 0 |  303 | `					}` |
|      ! 0 |  304 | `				}` |
|       26 |  305 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|       26 |  306 | `					pScan++; /* past '=>' */` |
|       12 |  307 | `				}` |
|       26 |  308 | `				pInnerBodyEnd = pScan;` |
|       26 |  309 | `				iNestInner = 0;` |
|      156 |  310 | `				while( pInnerBodyEnd < pEnd ){` |
|      138 |  311 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|        - |  312 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|        - |  313 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|        7 |  314 | `						break;` |
|        - |  315 | `					}` |
|      132 |  316 | `					if( pInnerBodyEnd->nType &` |
|        - |  317 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        6 |  318 | `						iNestInner++;` |
|      130 |  319 | `					}else if( pInnerBodyEnd->nType &` |
|        - |  320 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        6 |  321 | `						iNestInner--;` |
|        2 |  322 | `					}` |
|      132 |  323 | `					pInnerBodyEnd++;` |
|        2 |  324 | `				}` |
|        - |  325 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|        - |  326 | `				 * the outer's body: a default value is evaluated at call time` |
|        - |  327 | `				 * in the outer frame, so any free variable it references is` |
|        - |  328 | `				 * an outer capture. We must NOT scan the parameter-name` |
|        - |  329 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|        - |  330 | `				 * or those names leak into the outer's closure environment.` |
|        - |  331 | `				 *` |
|        - |  332 | `				 * Walk the signature argument-by-argument, splitting on` |
|        - |  333 | `				 * top-level commas, and for each argument scan only the token` |
|        - |  334 | `				 * range after the '=' sign. */` |
|        - |  335 | `				{` |
|       26 |  336 | `					SyToken *pArgStart = pInnerSigStart;` |
|       44 |  337 | `					while( pArgStart < pInnerSigEnd ){` |
|       20 |  338 | `						SyToken *pArgEnd = pArgStart;` |
|       20 |  339 | `						SyToken *pEq = 0;` |
|       20 |  340 | `						int iNestArg = 0;` |
|       68 |  341 | `						while( pArgEnd < pInnerSigEnd ){` |
|       50 |  342 | `							if( iNestArg == 0` |
|       52 |  343 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|        3 |  344 | `								break;` |
|        - |  345 | `							}` |
|       50 |  346 | `							if( pArgEnd->nType &` |
|        - |  347 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      ! 0 |  348 | `								iNestArg++;` |
|       50 |  349 | `							}else if( pArgEnd->nType &` |
|        - |  350 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      ! 0 |  351 | `								iNestArg--;` |
|      ! 0 |  352 | `							}` |
|       48 |  353 | `							if( pEq == 0 && iNestArg == 0` |
|       44 |  354 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|        7 |  355 | `								pEq = pArgEnd;` |
|        3 |  356 | `							}` |
|       50 |  357 | `							pArgEnd++;` |
|        2 |  358 | `						}` |
|       20 |  359 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|       10 |  360 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|        3 |  361 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|        7 |  362 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 |  363 | `								return SXERR_ABORT;` |
|        - |  364 | `							}` |
|        3 |  365 | `						}` |
|       20 |  366 | `						pArgStart = pArgEnd;` |
|       18 |  367 | `						if( pArgStart < pInnerSigEnd` |
|       12 |  368 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|        3 |  369 | `							pArgStart++;` |
|        1 |  370 | `						}` |
|        2 |  371 | `					}` |
|        - |  372 | `				}` |
|       38 |  373 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       12 |  374 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|       26 |  375 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  376 | `					return SXERR_ABORT;` |
|        - |  377 | `				}` |
|       26 |  378 | `				pScan = pInnerBodyEnd;` |
|       26 |  379 | `				continue;` |
|        - |  380 | `			}` |
|        7 |  381 | `		}` |
|     3138 |  382 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|     2892 |  383 | `			pScan++;` |
|     2892 |  384 | `			continue;` |
|        - |  385 | `		}` |
|        - |  386 | `		{` |
|        - |  387 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|      249 |  388 | `			SyToken *pDollar = pScan;` |
|      369 |  389 | `			while( &pDollar[1] < pEnd` |
|      249 |  390 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|      ! 0 |  391 | `				pDollar++;` |
|      ! 0 |  392 | `			}` |
|      249 |  393 | `			if( &pDollar[1] >= pEnd ){` |
|      ! 0 |  394 | `				break;` |
|        - |  395 | `			}` |
|      249 |  396 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  397 | `				pScan = pDollar + 1;` |
|      ! 0 |  398 | `				continue;` |
|        - |  399 | `			}` |
|      372 |  400 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|      246 |  401 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|      123 |  402 | `				aShadow,nShadow);` |
|      249 |  403 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  404 | `				return SXERR_ABORT;` |
|        - |  405 | `			}` |
|      249 |  406 | `			pScan = pDollar + 2;` |
|        - |  407 | `		}` |
|        3 |  408 | `	}` |
|      572 |  409 | `	return SXRET_OK;` |
|      288 |  410 | `}` |
|        - |  411 | `/*` |
|        - |  412 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|        - |  413 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|        - |  414 | ` * variables by value. The body is a single expression that acts as an` |
|        - |  415 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|        - |  416 | ` * $this is also made available.` |
|        - |  417 | ` */` |
|      544 |  418 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  419 | `{` |
|        - |  420 | `	ph7_vm_func *pFunc;` |
|        - |  421 | `	ph7_vm_func_closure_env sEnv;` |
|        - |  422 | `	GenBlock *pBlock;` |
|        - |  423 | `	SySet *pInstrContainer;` |
|        - |  424 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|        - |  425 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|        - |  426 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|        - |  427 | `	SyToken *pSavedEnd;` |
|        - |  428 | `	ph7_vm_func_arg *aArgs;` |
|        - |  429 | `	char zName[512];` |
|        - |  430 | `	static int iCnt = 1;` |
|        - |  431 | `	char *zDup;` |
|        - |  432 | `	SyToken *pTokKw;` |
|        - |  433 | `	sxu32 nLen;` |
|        - |  434 | `	sxu32 nLine;` |
|      549 |  435 | `	sxi32 iFlags = 0;` |
|      549 |  436 | `	int bStatic = 0;` |
|        - |  437 | `	sxi32 rc;` |
|        - |  438 | `	sxu32 n;` |
|      272 |  439 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |  440 |  |
|      549 |  441 | `	nLine = pGen->pIn->nLine;` |
|        - |  442 | ``	/* Attribute-sidecar key: `#[A] [static] fn` trivia is keyed to this token */`` |
|      549 |  443 | `	pTokKw = pGen->pIn;` |
|        - |  444 | `	/* Optional 'static' prefix */` |
|      544 |  445 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      549 |  446 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        9 |  447 | `		bStatic = 1;` |
|        9 |  448 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|        9 |  449 | `		pGen->pIn++;` |
|        4 |  450 | `	}` |
|        - |  451 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|      544 |  452 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      549 |  453 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|      ! 0 |  454 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  455 | `			"Arrow function: expected 'fn' keyword");` |
|      ! 0 |  456 | `		return SXERR_SYNTAX;` |
|        - |  457 | `	}` |
|      549 |  458 | `	pGen->pIn++; /* Jump 'fn' */` |
|        - |  459 | `	/* Optional '&' — return by reference */` |
|      549 |  460 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|      ! 0 |  461 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|      ! 0 |  462 | `		pGen->pIn++;` |
|      ! 0 |  463 | `	}` |
|        - |  464 | `	/* Expect '(' */` |
|      549 |  465 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        3 |  466 | `		if( pGen->pIn < pGen->pEnd ){` |
|        4 |  467 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|        - |  468 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|        2 |  469 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        2 |  470 | `		}else{` |
|      ! 0 |  471 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - |  472 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|        - |  473 | `		}` |
|        3 |  474 | `		return SXERR_SYNTAX;` |
|        - |  475 | `	}` |
|      546 |  476 | `	pGen->pIn++; /* Jump '(' */` |
|        - |  477 | `	/* Delimit the parameter list */` |
|      546 |  478 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|      546 |  479 | `	if( pSigEnd >= pGen->pEnd ){` |
|        3 |  480 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - |  481 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|        3 |  482 | `		return SXERR_SYNTAX;` |
|        - |  483 | `	}` |
|        - |  484 | `	/* Allocate the function state */` |
|      544 |  485 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|      544 |  486 | `	if( pFunc == 0 ){` |
|      ! 0 |  487 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  488 | `			"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  489 | `		return SXERR_ABORT;` |
|        - |  490 | `	}` |
|        - |  491 | `	/* Generate a unique lambda name */` |
|      544 |  492 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|      546 |  493 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|        3 |  494 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|        1 |  495 | `	}` |
|      544 |  496 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|      544 |  497 | `	if( zDup == 0 ){` |
|      ! 0 |  498 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  499 | `			"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  500 | `		return SXERR_ABORT;` |
|        - |  501 | `	}` |
|      544 |  502 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|        - |  503 | `	/* Reflection getStartLine(): line of the ['static'] 'fn' keyword */` |
|      544 |  504 | `	pFunc->nLine = nLine;` |
|        - |  505 | ``	/* Expression-position attributes (`$f = #[A] fn () => …`) */`` |
|      544 |  506 | `	if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pFunc->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  507 | `		return SXERR_ABORT;` |
|        - |  508 | `	}` |
|        - |  509 | `	/* Collect function arguments */` |
|      544 |  510 | `	if( pGen->pIn < pSigEnd ){` |
|      153 |  511 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|      153 |  512 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  513 | `			return SXERR_ABORT;` |
|        - |  514 | `		}` |
|       75 |  515 | `	}` |
|        - |  516 | `	/* Point past ')' and parse optional return type */` |
|      544 |  517 | `	pGen->pIn = &pSigEnd[1];` |
|      544 |  518 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|      544 |  519 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  520 | `		return SXERR_ABORT;` |
|      544 |  521 | `	}else if( rc == SXERR_SYNTAX ){` |
|      ! 0 |  522 | `		return SXERR_SYNTAX;` |
|        - |  523 | `	}` |
|        - |  524 | `	/* Expect '=>' */` |
|      544 |  525 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|        3 |  526 | `		if( pGen->pIn < pGen->pEnd ){` |
|        4 |  527 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|        - |  528 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|        2 |  529 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        2 |  530 | `		}else{` |
|      ! 0 |  531 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - |  532 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|        - |  533 | `		}` |
|        3 |  534 | `		return SXERR_SYNTAX;` |
|        - |  535 | `	}` |
|      542 |  536 | `	pGen->pIn++; /* Jump '=>' */` |
|      542 |  537 | `	pBodyStart = pGen->pIn;` |
|      542 |  538 | `	pBodyEnd = pGen->pEnd;` |
|        - |  539 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|        - |  540 | `	 * recursively collect free-variable references from the body. The scan` |
|        - |  541 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|        - |  542 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|      542 |  543 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|        - |  544 | `	{` |
|      542 |  545 | `		SyString *aShadow = 0;` |
|      542 |  546 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|      542 |  547 | `		if( nShadow > 0 ){` |
|      151 |  548 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      148 |  549 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|      151 |  550 | `			if( aShadow == 0 ){` |
|      ! 0 |  551 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  552 | `					"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  553 | `				return SXERR_ABORT;` |
|        - |  554 | `			}` |
|      343 |  555 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|      195 |  556 | `				aShadow[n] = aArgs[n].sName;` |
|       99 |  557 | `			}` |
|       74 |  558 | `		}` |
|      811 |  559 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|      269 |  560 | `			aShadow,nShadow);` |
|      542 |  561 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  562 | `			return SXERR_ABORT;` |
|        - |  563 | `		}` |
|        - |  564 | `	}` |
|        - |  565 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|        - |  566 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|        - |  567 | `	 * captured value is silently dropped when the enclosing scope has no` |
|        - |  568 | `	 * $this. */` |
|      542 |  569 | `	if( !bStatic ){` |
|        - |  570 | `		char *zThisDup;` |
|      534 |  571 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|      534 |  572 | `		if( zThisDup == 0 ){` |
|      ! 0 |  573 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  574 | `				"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  575 | `			return SXERR_ABORT;` |
|        - |  576 | `		}` |
|      534 |  577 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      534 |  578 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|      534 |  579 | `		sEnv.nIdx = SXU32_HIGH;` |
|      534 |  580 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      534 |  581 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|      534 |  582 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      265 |  583 | `	}` |
|        - |  584 | `	/* Arrow functions are always closures */` |
|      542 |  585 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|        - |  586 | `	/* Compile the body expression as an implicit return */` |
|      811 |  587 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      269 |  588 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|      542 |  589 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  590 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  591 | `			"PH7 engine is running out-of-memory");` |
|      ! 0 |  592 | `		return SXERR_ABORT;` |
|        - |  593 | `	}` |
|      542 |  594 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      542 |  595 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|      542 |  596 | `	pSavedEnd = pGen->pEnd;` |
|      542 |  597 | `	pGen->pIn = pBodyStart;` |
|      542 |  598 | `	pGen->pEnd = pBodyEnd;` |
|      542 |  599 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      542 |  600 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  601 | `		return SXERR_ABORT;` |
|        - |  602 | `	}` |
|        - |  603 | `	/* The cursor stopped just past the body expression */` |
|      542 |  604 | `	pFunc->nEndLine = (pGen->pIn > pBodyStart) ? pGen->pIn[-1].nLine : nLine;` |
|        - |  605 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|        - |  606 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|        - |  607 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|        - |  608 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|      542 |  609 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      542 |  610 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|      542 |  611 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|      542 |  612 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      542 |  613 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - |  614 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|      542 |  615 | `	pGen->pIn = pBodyEnd;` |
|      542 |  616 | `	pGen->pEnd = pSavedEnd;` |
|        - |  617 | `	/* Emit the load-closure instruction */` |
|      542 |  618 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|      542 |  619 | `	return SXRET_OK;` |
|      277 |  620 | `}` |
|        - |  621 | `/*` |
|        - |  622 | ` * Compile a single arm's expression range into a freshly-allocated` |
|        - |  623 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|        - |  624 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|        - |  625 | ` * expression's value.` |
|        - |  626 | ` */` |
|      364 |  627 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|        - |  628 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|        2 |  629 | `{` |
|        - |  630 | `	SySet *pInstrContainer;` |
|        - |  631 | `	SyToken *pTmpIn,*pTmpEnd;` |
|        - |  632 | `	GenBlock *pArmBlock;` |
|        - |  633 | `	sxi32 rc;` |
|      366 |  634 | `	pTmpIn  = pGen->pIn;` |
|      366 |  635 | `	pTmpEnd = pGen->pEnd;` |
|      366 |  636 | `	pGen->pIn  = pStart;` |
|      366 |  637 | `	pGen->pEnd = pStop;` |
|      366 |  638 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      366 |  639 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|        - |  640 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|        - |  641 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|        - |  642 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|        - |  643 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|        - |  644 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|      548 |  645 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      182 |  646 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|      366 |  647 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  648 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 |  649 | `		pGen->pIn  = pTmpIn;` |
|      ! 0 |  650 | `		pGen->pEnd = pTmpEnd;` |
|      ! 0 |  651 | `		return SXERR_ABORT;` |
|        - |  652 | `	}` |
|      366 |  653 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      366 |  654 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      366 |  655 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|      366 |  656 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|      366 |  657 | `	GenStateLeaveBlock(&(*pGen),0);` |
|      366 |  658 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      366 |  659 | `	pGen->pIn  = pTmpIn;` |
|      366 |  660 | `	pGen->pEnd = pTmpEnd;` |
|      366 |  661 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  662 | `		return SXERR_ABORT;` |
|        - |  663 | `	}` |
|      366 |  664 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 |  665 | `		return SXERR_EMPTY;` |
|        - |  666 | `	}` |
|      366 |  667 | `	return SXRET_OK;` |
|      184 |  668 | `}` |
|        - |  669 | `/*` |
|        - |  670 | ` * Compile a PHP 8.0 match expression:` |
|        - |  671 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|        - |  672 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|        - |  673 | ` * Strict comparison (===) is used between the subject and each condition.` |
|        - |  674 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|        - |  675 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|        - |  676 | ` */` |
|        - |  677 | `/*` |
|        - |  678 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|        - |  679 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|        - |  680 | ` * caller can bail out of the current expression.` |
|        - |  681 | ` */` |
|        2 |  682 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|        1 |  683 | `{` |
|        - |  684 | `	va_list ap;` |
|        - |  685 | `	sxi32 rc;` |
|        - |  686 | `	SyBlob sMsg;` |
|        3 |  687 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|        3 |  688 | `	va_start(ap,zFmt);` |
|        3 |  689 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|        3 |  690 | `	va_end(ap);` |
|        3 |  691 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|        3 |  692 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|        3 |  693 | `	SyBlobRelease(&sMsg);` |
|        3 |  694 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  695 | `		return SXERR_ABORT;` |
|        - |  696 | `	}` |
|        3 |  697 | `	return SXERR_SYNTAX;` |
|        2 |  698 | `}` |
|        - |  699 | `/*` |
|        - |  700 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|        - |  701 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|        - |  702 | ` * Returns the stop token pointer (or pEnd if none found).` |
|        - |  703 | ` */` |
|      366 |  704 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|        3 |  705 | `{` |
|      369 |  706 | `	SyToken *pCur = pStart;` |
|      369 |  707 | `	int iNest = 0;` |
|      889 |  708 | `	while( pCur < pEnd ){` |
|      853 |  709 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       17 |  710 | `			iNest++;` |
|      845 |  711 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       17 |  712 | `			iNest--;` |
|      829 |  713 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|      332 |  714 | `			return pCur;` |
|        - |  715 | `		}` |
|      523 |  716 | `		pCur++;` |
|        3 |  717 | `	}` |
|       39 |  718 | `	return pEnd;` |
|      186 |  719 | `}` |
|       74 |  720 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        4 |  721 | `{` |
|        - |  722 | `	ph7_match *pMatch;` |
|        - |  723 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|       78 |  724 | `	int bHasDefault = 0;` |
|        - |  725 | `	sxu32 nLine;` |
|        - |  726 | `	sxi32 rc;` |
|       37 |  727 | `	SXUNUSED(iCompileFlag);` |
|       78 |  728 | `	nLine = pGen->pIn->nLine;` |
|       78 |  729 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|        - |  730 | `	/* Expect '(' */` |
|       78 |  731 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 |  732 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  733 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|      ! 0 |  734 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|        - |  735 | `	}` |
|       78 |  736 | `	pGen->pIn++; /* Jump '(' */` |
|       78 |  737 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|       78 |  738 | `	if( pSubjEnd >= pGen->pEnd ){` |
|      ! 0 |  739 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  740 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|        - |  741 | `	}` |
|       78 |  742 | `	if( pGen->pIn >= pSubjEnd ){` |
|      ! 0 |  743 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  744 | `			"syntax error, unexpected \")\", expecting match subject");` |
|        - |  745 | `	}` |
|        - |  746 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|       78 |  747 | `	pSavedEnd = pGen->pEnd;` |
|       78 |  748 | `	pGen->pEnd = pSubjEnd;` |
|       78 |  749 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       78 |  750 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  751 | `		return SXERR_ABORT;` |
|        - |  752 | `	}` |
|       78 |  753 | `	pGen->pEnd = pSavedEnd;` |
|       78 |  754 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|        - |  755 | `	/* Expect '{' */` |
|       78 |  756 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|      ! 0 |  757 | `		return GenStateMatchError(pGen,` |
|      ! 0 |  758 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|        - |  759 | `			"syntax error, expecting \"{\" after match subject");` |
|        - |  760 | `	}` |
|       78 |  761 | `	pGen->pIn++; /* Jump '{' */` |
|       78 |  762 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|       78 |  763 | `	if( pBodyEnd >= pGen->pEnd ){` |
|      ! 0 |  764 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  765 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|        - |  766 | `	}` |
|        - |  767 | `	/* Allocate ph7_match container */` |
|       78 |  768 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|       78 |  769 | `	if( pMatch == 0 ){` |
|      ! 0 |  770 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  771 | `			"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  772 | `		return SXERR_ABORT;` |
|        - |  773 | `	}` |
|       78 |  774 | `	SyZero(pMatch,sizeof(ph7_match));` |
|       78 |  775 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|        - |  776 | `	/* Iterate arms */` |
|      266 |  777 | `	while( pGen->pIn < pBodyEnd ){` |
|        - |  778 | `		ph7_match_arm sArm;` |
|        - |  779 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|      195 |  780 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|      195 |  781 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|      195 |  782 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|      195 |  783 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - |  784 | `		/* 'default' arm? */` |
|      192 |  785 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      110 |  786 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|       24 |  787 | `			if( bHasDefault ){` |
|        3 |  788 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|        - |  789 | `					"Match expressions may only contain one default arm");` |
|        4 |  790 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  791 | `			}` |
|       22 |  792 | `			sArm.bDefault = 1;` |
|       22 |  793 | `			bHasDefault = 1;` |
|       22 |  794 | `			pGen->pIn++;` |
|       22 |  795 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|      ! 0 |  796 | `				return GenStateMatchError(pGen,nArmLine,` |
|        - |  797 | `					"syntax error, expecting \"=>\" after 'default'");` |
|        - |  798 | `			}` |
|       22 |  799 | `			pGen->pIn++; /* Jump '=>' */` |
|       12 |  800 | `		}else{` |
|        - |  801 | `			/* Condition list: cond (',' cond)* '=>' */` |
|      173 |  802 | `			pCondStart = pGen->pIn;` |
|      173 |  803 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|        - |  804 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|      181 |  805 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|        - |  806 | `				SySet sCondBc;` |
|        9 |  807 | `				if( pCondStart >= pArrow ){` |
|      ! 0 |  808 | `					return GenStateMatchError(pGen,nArmLine,` |
|        - |  809 | `						"syntax error, empty match condition expression");` |
|        - |  810 | `				}` |
|        9 |  811 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        9 |  812 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|        9 |  813 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  814 | `					return SXERR_ABORT;` |
|        - |  815 | `				}` |
|        9 |  816 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|        9 |  817 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|        9 |  818 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|        - |  819 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|        1 |  820 | `			}` |
|      173 |  821 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|        3 |  822 | `				return GenStateMatchError(pGen,nArmLine,` |
|        - |  823 | `					"syntax error, expecting \"=>\" in match arm");` |
|        - |  824 | `			}` |
|      170 |  825 | `			if( pCondStart >= pArrow ){` |
|      ! 0 |  826 | `				return GenStateMatchError(pGen,nArmLine,` |
|        - |  827 | `					"syntax error, empty match condition expression");` |
|        - |  828 | `			}` |
|        - |  829 | `			{` |
|        - |  830 | `				SySet sCondBc;` |
|      170 |  831 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      170 |  832 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|      170 |  833 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  834 | `					return SXERR_ABORT;` |
|        - |  835 | `				}` |
|      170 |  836 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|        - |  837 | `			}` |
|      170 |  838 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|        - |  839 | `		}` |
|        - |  840 | `		/* Compile result expression: up to top-level ',' or body end */` |
|      190 |  841 | `		pResStart = pGen->pIn;` |
|      190 |  842 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|      190 |  843 | `		if( pResStart >= pResEnd ){` |
|      ! 0 |  844 | `			return GenStateMatchError(pGen,nArmLine,` |
|        - |  845 | `				"syntax error, expected expression after \"=>\"");` |
|        - |  846 | `		}` |
|      190 |  847 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|      190 |  848 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  849 | `			return SXERR_ABORT;` |
|        - |  850 | `		}` |
|      190 |  851 | `		pGen->pIn = pResEnd;` |
|      190 |  852 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|      156 |  853 | `			pGen->pIn++; /* Skip trailing ',' */` |
|       77 |  854 | `		}` |
|      190 |  855 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|        2 |  856 | `	}` |
|       73 |  857 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|       73 |  858 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|       73 |  859 | `	return SXRET_OK;` |
|       41 |  860 | `}` |
|        - |  861 | `/*` |
|        - |  862 | ` * Compile a backtick quoted string.` |
|        - |  863 | ` */` |
|        2 |  864 | `PH7_PRIVATE sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        1 |  865 | `{` |
|        1 |  866 | `	SXUNUSED(iCompileFlag);` |
|        - |  867 | `	/*` |
|        - |  868 | ``	 * The backtick (`) operator was DEPRECATED by php (shell_exec() is the replacement).`` |
|        - |  869 | `	 * PHL targets php's *non-deprecated* surface, so it is a hard parse error — never` |
|        - |  870 | `	 * compiled to a shell_exec() call.` |
|        - |  871 | `	 */` |
|        3 |  872 | `	PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|        - |  873 | ``		"syntax error, the backtick (`) operator was removed, use shell_exec() instead");`` |
|        3 |  874 | `	return SXERR_ABORT;` |
|        1 |  875 | `}` |
|        - |  876 | `/*` |
|        - |  877 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|        - |  878 | ` * construct.` |
|        - |  879 | ` */` |
|       66 |  880 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  881 | `{` |
|        - |  882 | `	SyString *pName;` |
|        - |  883 | `	sxu32 nKeyID;` |
|        - |  884 | `	sxi32 rc;` |
|        - |  885 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|       71 |  886 | `	pName = &pGen->pIn->sData;` |
|       71 |  887 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       71 |  888 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|       71 |  889 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|        9 |  890 | `		SyToken *pTmp,*pNext = 0;` |
|        - |  891 | `		/* Compile arguments one after one */` |
|        9 |  892 | `		pTmp = pGen->pEnd;` |
|        - |  893 | `		/* Symisc eXtension to the PHP programming language:` |
|        - |  894 | `		 * 'echo' can be used in the context of a function which` |
|        - |  895 | `		 *  mean that the following expression is valid:` |
|        - |  896 | `		 *      fopen('file.txt','r') or echo "IO error";` |
|        - |  897 | `		 */` |
|        9 |  898 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|       17 |  899 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|        9 |  900 | `			if( pGen->pIn < pNext ){` |
|        9 |  901 | `				pGen->pEnd = pNext;` |
|        9 |  902 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|        9 |  903 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  904 | `					return SXERR_ABORT;` |
|        - |  905 | `				}` |
|        9 |  906 | `				if( rc != SXERR_EMPTY ){` |
|        - |  907 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|        - |  908 | `					 * without the overhead of a function call.` |
|        - |  909 | `					 * This is a very powerful optimization that improve` |
|        - |  910 | `					 * performance greatly.` |
|        - |  911 | `					 */` |
|        9 |  912 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|        4 |  913 | `				}` |
|        4 |  914 | `			}` |
|        - |  915 | `			/* Jump trailing commas */` |
|        9 |  916 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|      ! 0 |  917 | `				pNext++;` |
|      ! 0 |  918 | `			}` |
|        9 |  919 | `			pGen->pIn = pNext;` |
|        1 |  920 | `		}` |
|        - |  921 | `		/* Restore token stream */` |
|        9 |  922 | `		pGen->pEnd = pTmp;` |
|        5 |  923 | `	}else{` |
|       63 |  924 | `		sxi32 nArg = 0;` |
|       63 |  925 | `		sxu32 nIdx = 0;` |
|       63 |  926 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|       63 |  927 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  928 | `			return SXERR_ABORT;` |
|       63 |  929 | `		}else if(rc != SXERR_EMPTY ){` |
|       63 |  930 | `			nArg = 1;` |
|       29 |  931 | `		}` |
|       63 |  932 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|        - |  933 | `			ph7_value *pObj;` |
|        - |  934 | `			/* Emit the call instruction */` |
|       35 |  935 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       35 |  936 | `			if( pObj == 0 ){` |
|      ! 0 |  937 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  938 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |  939 | `				return SXERR_ABORT;` |
|        - |  940 | `			}` |
|       35 |  941 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|        - |  942 | `			/* Install in the literal table */` |
|       35 |  943 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       15 |  944 | `		}` |
|        - |  945 | `		/* Emit the call instruction */` |
|       63 |  946 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       63 |  947 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        - |  948 | `	}` |
|        - |  949 | `	/* Node successfully compiled */` |
|       71 |  950 | `	return SXRET_OK;` |
|       38 |  951 | `}` |
|        - |  952 | `/*` |
|        - |  953 | ` * Compile a node holding a variable declaration.` |
|        - |  954 | ` * According to the PHP language reference` |
|        - |  955 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|        - |  956 | ` *  The variable name is case-sensitive.` |
|        - |  957 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|        - |  958 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|        - |  959 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|        - |  960 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|        - |  961 | ` *  Note: $this is a special variable that can't be assigned.` |
|        - |  962 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|        - |  963 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|        - |  964 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|        - |  965 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|        - |  966 | ` *  the chapter on Expressions.` |
|        - |  967 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|        - |  968 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|        - |  969 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|        - |  970 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|        - |  971 | ` *  is being assigned (the source variable).` |
|        - |  972 | ` */` |
| 21611282 |  973 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  974 | `{` |
| 21611287 |  975 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|        - |  976 | `	sxi32 iVv;` |
|        - |  977 | `	sxi32 iP1;` |
|        - |  978 | `	void *p3;` |
|        - |  979 | `	sxi32 rc;` |
| 21611287 |  980 | `	iVv = -1; /* Variable variable counter */` |
| 43222581 |  981 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
| 21611299 |  982 | `		pGen->pIn++;` |
| 21611299 |  983 | `		iVv++;` |
|        5 |  984 | `	}` |
| 21611287 |  985 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|        - |  986 | `		/* Invalid variable name */` |
|      ! 0 |  987 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable or \"{\" or \"$\"");` |
|      ! 0 |  988 | `		if( rc == SXERR_ABORT ){` |
|        - |  989 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  990 | `			return SXERR_ABORT;` |
|        - |  991 | `		}` |
|      ! 0 |  992 | `		return SXRET_OK;` |
|        - |  993 | `	}` |
| 21611287 |  994 | `	p3  = 0;` |
| 21611287 |  995 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|        - |  996 | `		/* Dynamic variable creation */` |
|       19 |  997 | `		pGen->pIn++;  /* Jump the open curly */` |
|       19 |  998 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|       19 |  999 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 1000 | `			/* Empty expression */` |
|        - | 1001 | `			{` |
|        - | 1002 | `			/* php names the offending token and, for an empty "${}", stops there:` |
|        - | 1003 | `			 * the "expecting" tail only appears when something could still follow. */` |
|        3 | 1004 | `			SyToken *pBad = pGen->pIn < pGen->pEnd ? pGen->pIn : 0;` |
|        3 | 1005 | `			PH7_GenSyntaxError(&(*pGen),pBad,` |
|        1 | 1006 | `				(pBad && (pBad->nType & PH7_TK_CCB)) ? 0 : "variable or \"{\" or \"$\"");` |
|        - | 1007 | `			}` |
|        3 | 1008 | `			return SXRET_OK;` |
|        - | 1009 | `		}` |
|        - | 1010 | `		/* Compile the expression holding the variable name */` |
|       16 | 1011 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       16 | 1012 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1013 | `			return SXERR_ABORT;` |
|       16 | 1014 | `		}else if( rc == SXERR_EMPTY ){` |
|        3 | 1015 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,0);` |
|        3 | 1016 | `			return SXRET_OK;` |
|        - | 1017 | `		}` |
|        7 | 1018 | `	}else{` |
|        - | 1019 | `		SyHashEntry *pEntry;` |
|        - | 1020 | `		SyString *pName;` |
| 21611271 | 1021 | `		char *zName = 0;` |
|        - | 1022 | `		/* Extract variable name */` |
| 21611271 | 1023 | `		pName = &pGen->pIn->sData;` |
|        - | 1024 | `		/* Advance the stream cursor */` |
| 21611271 | 1025 | `		pGen->pIn++;` |
| 21611271 | 1026 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
| 21611271 | 1027 | `		if( pEntry == 0 ){` |
|        - | 1028 | `			/* Duplicate name */` |
|  1294971 | 1029 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  1294971 | 1030 | `			if( zName == 0 ){` |
|      ! 0 | 1031 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1032 | `				return SXERR_ABORT;` |
|        - | 1033 | `			}` |
|        - | 1034 | `			/* Install in the hashtable */` |
|  1294971 | 1035 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   647488 | 1036 | `		}else{` |
|        - | 1037 | `			/* Name already available */` |
| 20316305 | 1038 | `			zName = (char *)pEntry->pUserData;` |
|        - | 1039 | `		}` |
| 21611271 | 1040 | `		p3 = (void *)zName;` |
|        - | 1041 | `	}` |
| 21611283 | 1042 | `	iP1 = 0;` |
| 21611283 | 1043 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
|  3397545 | 1044 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|        - | 1045 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
|  3393641 | 1046 | `			iP1 = 1;` |
|  1696818 | 1047 | `		}` |
|  1698770 | 1048 | `	}` |
|        - | 1049 | `	/* Emit the load instruction */` |
| 21611283 | 1050 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,p3,0);` |
| 21611295 | 1051 | `	while( iVv > 0 ){` |
|       13 | 1052 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,0,0,0);` |
|       13 | 1053 | `		iVv--;` |
|        1 | 1054 | `	}` |
|        - | 1055 | `	/* Node successfully compiled */` |
| 21611283 | 1056 | `	return SXRET_OK;` |
| 10805646 | 1057 | `}` |
|        - | 1058 | `/*` |
|        - | 1059 | ` * Load a literal.` |
|        - | 1060 | ` */` |
| 14097784 | 1061 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|        5 | 1062 | `{` |
| 14097789 | 1063 | `	SyToken *pToken = pGen->pIn;` |
|        - | 1064 | `	ph7_value *pObj;` |
|        - | 1065 | `	SyString *pStr;` |
|        - | 1066 | `	sxu32 nIdx;` |
|        - | 1067 | `	/* Extract token value */` |
| 14097789 | 1068 | `	pStr = &pToken->sData;` |
|        - | 1069 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first. A reserved` |
|        - | 1070 | `	 * word used as a member NAME (Enum::Null, C::Array, $o->list()) is a plain` |
|        - | 1071 | `	 * identifier — the parser flagged its token so the whole value-folding chain is` |
|        - | 1072 | `	 * skipped and it falls through to the ordinary string-literal emit below. */` |
| 14097789 | 1073 | `	if( pToken->nType & PH7_TK_MEMBER_NAME ){` |
|        - | 1074 | `		/* fall through to the plain-string literal path */` |
| 11222568 | 1075 | `	}else if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  1687389 | 1076 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|        - | 1077 | `			/* NULL constant are always indexed at 0 */` |
|  1157189 | 1078 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|  1157189 | 1079 | `			return SXRET_OK;` |
|   530205 | 1080 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|        - | 1081 | `			/* TRUE constant are always indexed at 1 */` |
|   358169 | 1082 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|   358169 | 1083 | `			return SXRET_OK;` |
|        5 | 1084 | `		}` |
|  7539356 | 1085 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|  1586750 | 1086 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|        - | 1087 | `			/* FALSE constant are always indexed at 2 */` |
|   796107 | 1088 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   796107 | 1089 | `			return SXRET_OK;` |
|  6010342 | 1090 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   292962 | 1091 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|        - | 1092 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|     3893 | 1093 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     3893 | 1094 | `			if( pObj == 0 ){` |
|      ! 0 | 1095 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1096 | `				return SXERR_ABORT;` |
|        - | 1097 | `			}` |
|     3893 | 1098 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|        - | 1099 | `			/* Emit the load constant instruction */` |
|     3893 | 1100 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     3893 | 1101 | `			return SXRET_OK;` |
|  6004505 | 1102 | `	}else if( (pStr->nByte == sizeof("__FILE__") - 1 &&` |
|   513828 | 1103 | `		SyMemcmp(pStr->zString,"__FILE__",sizeof("__FILE__")-1) == 0) \|\|` |
|  6080761 | 1104 | `		(pStr->nByte == sizeof("__DIR__") - 1 &&` |
|   449498 | 1105 | `		SyMemcmp(pStr->zString,"__DIR__",sizeof("__DIR__")-1) == 0) ){` |
|        - | 1106 | `			/* __FILE__ / __DIR__ are magic constants resolved at COMPILE time to the` |
|        - | 1107 | `			 * file being compiled (where the token is written), NOT the runtime` |
|        - | 1108 | `			 * execution file. A function defined in a.php reporting __FILE__ must say` |
|        - | 1109 | `			 * a.php even when called from b.php — php semantics, and what Composer's` |
|        - | 1110 | ``			 * autoloader (loadClassLoader's `require __DIR__ . '/ClassLoader.php'`)`` |
|        - | 1111 | `			 * relies on. The runtime-constant path returned the caller's file. */` |
|     3987 | 1112 | `			int bDir = (pStr->zString[2] == 'D'); /* __DIR__ vs __FILE__ */` |
|     3987 | 1113 | `			SyString *pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     3987 | 1114 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     3987 | 1115 | `			if( pObj == 0 ){` |
|      ! 0 | 1116 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1117 | `				return SXERR_ABORT;` |
|        - | 1118 | `			}` |
|     3987 | 1119 | `			if( pFile && pFile->nByte > 0 ){` |
|      107 | 1120 | `				if( bDir ){` |
|        - | 1121 | `					const char *zDir;` |
|        - | 1122 | `					int nLen;` |
|        - | 1123 | `					SyString sDir;` |
|       57 | 1124 | `					zDir = PH7_ExtractDirName(pFile->zString,(int)pFile->nByte,&nLen);` |
|       57 | 1125 | `					SyStringInitFromBuf(&sDir,zDir,nLen);` |
|       57 | 1126 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&sDir);` |
|       31 | 1127 | `				}else{` |
|       55 | 1128 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,pFile);` |
|        - | 1129 | `				}` |
|       56 | 1130 | `			}else{` |
|        - | 1131 | `				SyString sMem;` |
|     3885 | 1132 | `				SyStringInitFromBuf(&sMem,":MEMORY:",sizeof(":MEMORY:")-1);` |
|     3885 | 1133 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sMem);` |
|        - | 1134 | `			}` |
|     3987 | 1135 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     3987 | 1136 | `			return SXRET_OK;` |
|  5976603 | 1137 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   241224 | 1138 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|        - | 1139 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|        8 | 1140 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        8 | 1141 | `			if( pObj == 0 ){` |
|      ! 0 | 1142 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1143 | `				return SXERR_ABORT;` |
|        - | 1144 | `			}` |
|        8 | 1145 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        - | 1146 | `				SyString sNs;` |
|        8 | 1147 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        8 | 1148 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|        5 | 1149 | `			}else{` |
|      ! 0 | 1150 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|        - | 1151 | `			}` |
|        8 | 1152 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        8 | 1153 | `			return SXRET_OK;` |
|  5988470 | 1154 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   428950 | 1155 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  6019938 | 1156 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   327930 | 1157 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|       11 | 1158 | `			GenBlock *pBlock = pGen->pCurrent;` |
|        - | 1159 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|       21 | 1160 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|        - | 1161 | `				/* Point to the upper block */` |
|       11 | 1162 | `				pBlock = pBlock->pParent;` |
|        1 | 1163 | `			}` |
|       11 | 1164 | `			if( pBlock == 0 ){` |
|        - | 1165 | `				/* Called in the global scope,load NULL */` |
|        5 | 1166 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|        3 | 1167 | `			}else{` |
|        - | 1168 | `				/* Extract the target function/method */` |
|        7 | 1169 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        7 | 1170 | `				int bMethod = (pStr->zString[2] == 'M'); /* __METHOD__ vs __FUNCTION__ */` |
|        7 | 1171 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        7 | 1172 | `				if( pObj == 0 ){` |
|      ! 0 | 1173 | `					PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1174 | `					return SXERR_ABORT;` |
|        - | 1175 | `				}` |
|        - | 1176 | `				/*` |
|        - | 1177 | `				 * __METHOD__ is the QUALIFIED name: "C::m" inside a method, and the plain` |
|        - | 1178 | `				 * function name inside a plain function (php does not answer "" there —` |
|        - | 1179 | `				 * PH7 loaded NULL, so __METHOD__ was empty in every free function and` |
|        - | 1180 | `				 * unqualified in every method).` |
|        - | 1181 | `				 */` |
|        8 | 1182 | `				if( bMethod && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|        3 | 1183 | `					SyString *pCls = &((ph7_class *)pFunc->pUserData)->sName;` |
|        - | 1184 | `					SyBlob sQual;` |
|        - | 1185 | `					SyString sOut;` |
|        3 | 1186 | `					SyBlobInit(&sQual,&pGen->pVm->sAllocator);` |
|        3 | 1187 | `					SyBlobFormat(&sQual,"%z::%z",pCls,&pFunc->sName);` |
|        3 | 1188 | `					SyStringInitFromBuf(&sOut,SyBlobData(&sQual),SyBlobLength(&sQual));` |
|        3 | 1189 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&sOut);` |
|        3 | 1190 | `					SyBlobRelease(&sQual);` |
|        2 | 1191 | `				}else{` |
|        5 | 1192 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|        - | 1193 | `				}` |
|        - | 1194 | `				/* Emit the load constant instruction */` |
|        7 | 1195 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - | 1196 | `			}` |
|       11 | 1197 | `			return SXRET_OK;` |
|        - | 1198 | `	}` |
|        - | 1199 | `	/* Query literal table */` |
| 11778453 | 1200 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|        - | 1201 | `		ph7_value *pLitObj;` |
|        - | 1202 | `		/* Unknown literal,install it in the literal table */` |
|  2427787 | 1203 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  2427787 | 1204 | `		if( pLitObj == 0 ){` |
|      ! 0 | 1205 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 | 1206 | `			return SXERR_ABORT;` |
|        - | 1207 | `		}` |
|  2427787 | 1208 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  2427787 | 1209 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  1213891 | 1210 | `	}` |
|        - | 1211 | `	/* Emit the load constant instruction */` |
| 11778453 | 1212 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
| 11778453 | 1213 | `	return SXRET_OK;` |
|  7048897 | 1214 | `}` |
|        - | 1215 | `/*` |
|        - | 1216 | ` * Resolve a namespace path or simply load a literal.` |
|        - | 1217 | ` * If the token stream contains namespace separators (backslashes),` |
|        - | 1218 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|        - | 1219 | ` * Otherwise, load the simple literal directly.` |
|        - | 1220 | ` */` |
| 14101754 | 1221 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|        5 | 1222 | `{` |
|        - | 1223 | `	sxi32 rc;` |
| 14101759 | 1224 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 1225 | `		return SXRET_OK;` |
|        - | 1226 | `	}` |
|        - | 1227 | `	/* Check if this is a multi-token namespace path */` |
| 14101759 | 1228 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|        - | 1229 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|     3975 | 1230 | `		SyBlob *pWorker = &pGen->sWorker;` |
|     3975 | 1231 | `		int isAbsolute = 0;` |
|     3975 | 1232 | `		SyBlobReset(pWorker);` |
|        - | 1233 | `		/* Check for leading backslash (absolute path) */` |
|     3975 | 1234 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|     3953 | 1235 | `			isAbsolute = 1;` |
|     3953 | 1236 | `			pGen->pIn++; /* Skip leading backslash */` |
|     1974 | 1237 | `		}` |
|        - | 1238 | `		/* Collect the raw path (no prefix yet) so its FIRST segment can be resolved` |
|        - | 1239 | ``		 * against use-imports below — php resolves `A\B\C` by mapping the leading`` |
|        - | 1240 | ``		 * `A` through the imports (`use X\A;` makes it `X\A\B\C`), and only prepends`` |
|        - | 1241 | ``		 * the current namespace when `A` matches no import. Blindly prefixing the`` |
|        - | 1242 | ``		 * namespace here produced e.g. `Ns\Ev\Bus` for `use ...\Event as Ev; Ev\Bus`. */`` |
|        - | 1243 | `		{` |
|        - | 1244 | `			SyBlob sRaw;` |
|     3975 | 1245 | `			SyBlobInit(&sRaw,&pGen->pVm->sAllocator);` |
|     4139 | 1246 | `			while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|     4139 | 1247 | `				if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       87 | 1248 | `					SyBlobAppend(&sRaw,"\\",1);` |
|       46 | 1249 | `				}else{` |
|     4057 | 1250 | `					SyBlobAppend(&sRaw,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        - | 1251 | `				}` |
|     4139 | 1252 | `				if( pGen->pIn == &pGen->pEnd[-1] ){` |
|     3975 | 1253 | `					pGen->pIn++;` |
|     3975 | 1254 | `					break;` |
|        - | 1255 | `				}` |
|      169 | 1256 | `				pGen->pIn++;` |
|        5 | 1257 | `			}` |
|     3975 | 1258 | `			if( isAbsolute ){` |
|     3953 | 1259 | `				SyBlobAppend(pWorker,SyBlobData(&sRaw),SyBlobLength(&sRaw)); /* FQN as written */` |
|     1979 | 1260 | `			}else{` |
|       24 | 1261 | `				const char *zRaw = (const char *)SyBlobData(&sRaw);` |
|       24 | 1262 | `				sxu32 nRaw = SyBlobLength(&sRaw);` |
|       24 | 1263 | `				sxu32 nFirst = 0;` |
|        - | 1264 | `				SyHashEntry *pNsImp;` |
|      108 | 1265 | `				while( nFirst < nRaw && zRaw[nFirst] != '\\' ){ nFirst++; }` |
|       24 | 1266 | `				pNsImp = SyHashGet(&pGen->hUseImports,(const void *)zRaw,nFirst);` |
|       24 | 1267 | `				if( pNsImp ){` |
|        - | 1268 | `					/* Leading segment is an imported alias: substitute its FQN. */` |
|       21 | 1269 | `					const char *zFQN = (const char *)pNsImp->pUserData;` |
|       21 | 1270 | `					SyBlobAppend(pWorker,zFQN,SyStrlen(zFQN));` |
|       21 | 1271 | `					SyBlobAppend(pWorker,zRaw + nFirst,nRaw - nFirst);` |
|       13 | 1272 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        3 | 1273 | `					SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        3 | 1274 | `					SyBlobAppend(pWorker,"\\",1);` |
|        3 | 1275 | `					SyBlobAppend(pWorker,zRaw,nRaw);` |
|        2 | 1276 | `				}else{` |
|      ! 0 | 1277 | `					SyBlobAppend(pWorker,zRaw,nRaw); /* global scope, no import */` |
|        - | 1278 | `				}` |
|        - | 1279 | `			}` |
|     3975 | 1280 | `			SyBlobRelease(&sRaw);` |
|        - | 1281 | `		}` |
|     3975 | 1282 | `		if( SyBlobLength(pWorker) > 0 ){` |
|        - | 1283 | `			ph7_value *pObj;` |
|        - | 1284 | `			SyString sPath;` |
|        - | 1285 | `			sxu32 nIdx;` |
|     3975 | 1286 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|        - | 1287 | `			/* Install in the literal table */` |
|     3975 | 1288 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|     3923 | 1289 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     3923 | 1290 | `				if( pObj == 0 ){` |
|      ! 0 | 1291 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 | 1292 | `					return SXERR_ABORT;` |
|        - | 1293 | `				}` |
|     3923 | 1294 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|     3923 | 1295 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     1959 | 1296 | `			}` |
|        - | 1297 | `			/* Emit the load constant instruction.` |
|        - | 1298 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|        - | 1299 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|     5960 | 1300 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|     1985 | 1301 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|     1985 | 1302 | `				nIdx,0,0);` |
|     3975 | 1303 | `			return SXRET_OK;` |
|        - | 1304 | `		}` |
|      ! 0 | 1305 | `	}` |
|        - | 1306 | `	/* Single-token literal: load directly */` |
| 14097789 | 1307 | `	rc = GenStateLoadLiteral(&(*pGen));` |
| 14097789 | 1308 | `	return rc;` |
|  7050882 | 1309 | `}` |
|        - | 1310 | `/*` |
|        - | 1311 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|        - | 1312 | ` */` |
|        - | 1313 | `/*` |
|        - | 1314 | `` * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of`` |
|        - | 1315 | `` * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument`` |
|        - | 1316 | ``  * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...` `` |
|        - | 1317 | ` * appeared outside a call argument list — a syntax error (PHP rejects it likewise).` |
|        - | 1318 | ` */` |
|      ! 0 | 1319 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|      ! 0 | 1320 | `{` |
|      ! 0 | 1321 | `	SXUNUSED(iCompileFlag);` |
|      ! 0 | 1322 | `	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|        - | 1323 | `		"Cannot use the first-class callable syntax '...' here");` |
|      ! 0 | 1324 | `	return SXERR_SYNTAX;` |
|      ! 0 | 1325 | `}` |
| 14101754 | 1326 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 1327 | `{` |
|        - | 1328 | `	sxi32 rc;` |
| 14101759 | 1329 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
| 14101759 | 1330 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1331 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 | 1332 | `		return rc;` |
|        - | 1333 | `	}` |
|        - | 1334 | `	/* Node successfully compiled */` |
| 14101759 | 1335 | `	return SXRET_OK;` |
|  7050882 | 1336 | `}` |
|        - | 1337 |  |
