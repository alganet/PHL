# src/ph7/compile_node.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 678/813 lines (83.39%)

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
|      616 |   36 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |   37 | `{` |
|      621 |   38 | `	ph7_vm_func *pAnnonFunc = 0; /* Annonymous function body */` |
|        - |   39 | `	char zName[512];         /* Unique lambda name */` |
|        - |   40 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|        - |   41 | `							  * one thread is allowed to compile the script.` |
|        - |   42 | `						      */` |
|        - |   43 | `	SyString sName;` |
|      621 |   44 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `$f = #[A] function…` trivia`` |
|        - |   45 | `	                              * is keyed to this ['static'] 'function' token */` |
|        - |   46 | `	sxu32 nKwLine;` |
|      621 |   47 | `	sxi32 iFlags = 0;` |
|        - |   48 | `	sxu32 nLen;` |
|        - |   49 | `	sxi32 rc;` |
|      308 |   50 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |   51 |  |
|      621 |   52 | `	nKwLine = pGen->pIn->nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|      616 |   53 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      621 |   54 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        - |   55 | `		/* Static closure: no $this auto-capture, bind refused */` |
|       26 |   56 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|       26 |   57 | `		pGen->pIn++; /* Jump the 'static' keyword */` |
|       12 |   58 | `	}` |
|      621 |   59 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|      621 |   60 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|      ! 0 |   61 | `		pGen->pIn++;` |
|      ! 0 |   62 | `	}` |
|        - |   63 | `	/* Generate a unique name */` |
|      621 |   64 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|        - |   65 | `	/* Make sure the generated name is unique */` |
|      621 |   66 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|      ! 0 |   67 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|      ! 0 |   68 | `	}` |
|      621 |   69 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|        - |   70 | `	/* Compile the lambda body */` |
|      621 |   71 | `	rc = GenStateCompileFunc(&(*pGen),&sName,iFlags,TRUE,&pAnnonFunc);` |
|      621 |   72 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |   73 | `		return SXERR_ABORT;` |
|        - |   74 | `	}` |
|      621 |   75 | `	if( pAnnonFunc ){` |
|      621 |   76 | `		pAnnonFunc->nLine = nKwLine;` |
|        - |   77 | ``		/* Expression-position attributes (`$f = #[A] function () {}`): the trivia`` |
|        - |   78 | `		 * sidecar keys them to the closure's first keyword token. */` |
|      621 |   79 | `		if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnnonFunc->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |   80 | `			return SXERR_ABORT;` |
|        - |   81 | `		}` |
|      308 |   82 | `	}` |
|        - |   83 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|        - |   84 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|        - |   85 | `	 * the handler wraps either in a Closure instance. */` |
|      621 |   86 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|        - |   87 | `	/* Node successfully compiled */` |
|      621 |   88 | `	return SXRET_OK;` |
|      313 |   89 | `}` |
|        - |   90 | `/*` |
|        - |   91 | ` * Add a free variable to the arrow function's closure environment, unless` |
|        - |   92 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|        - |   93 | ` * enclosing arrow level, or has already been captured.` |
|        - |   94 | ` */` |
|      272 |   95 | `static sxi32 GenStateArrowAddCapture(` |
|        - |   96 | `	ph7_gen_state *pGen,` |
|        - |   97 | `	ph7_vm_func *pFunc,` |
|        - |   98 | `	const char *zName,` |
|        - |   99 | `	sxu32 nByte,` |
|        - |  100 | `	SyString *aShadow,` |
|        - |  101 | `	sxu32 nShadow)` |
|        5 |  102 | `{` |
|        - |  103 | `	ph7_vm_func_closure_env sEnv;` |
|        - |  104 | `	ph7_vm_func_closure_env *aEnv;` |
|        - |  105 | `	sxu32 n, nEnv;` |
|        - |  106 | `	char *zDup;` |
|      277 |  107 | `	if( nByte == 0 ){` |
|      ! 0 |  108 | `		return SXRET_OK;` |
|        - |  109 | `	}` |
|      272 |  110 | `	if( nByte == sizeof("this")-1` |
|      150 |  111 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|        9 |  112 | `		return SXRET_OK;` |
|        - |  113 | `	}` |
|      337 |  114 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|      256 |  115 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|      249 |  116 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|      193 |  117 | `			return SXRET_OK;` |
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
|      141 |  139 | `}` |
|        - |  140 | `/*` |
|        - |  141 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|        - |  142 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|        - |  143 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|        - |  144 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|        - |  145 | ` */` |
|      124 |  146 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|        - |  147 | `	ph7_gen_state *pGen,` |
|        - |  148 | `	ph7_vm_func *pFunc,` |
|        - |  149 | `	const char *zIn,` |
|        - |  150 | `	const char *zEnd,` |
|        - |  151 | `	SyString *aShadow,` |
|        - |  152 | `	sxu32 nShadow)` |
|        3 |  153 | `{` |
|        - |  154 | `	sxi32 rc;` |
|      629 |  155 | `	while( zIn < zEnd ){` |
|      505 |  156 | `		if( zIn[0] == '\\' ){` |
|       16 |  157 | `			zIn++;` |
|       16 |  158 | `			if( zIn < zEnd ){` |
|       16 |  159 | `				zIn++;` |
|        7 |  160 | `			}` |
|       16 |  161 | `			continue;` |
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
|      127 |  195 | `	return SXRET_OK;` |
|       65 |  196 | `}` |
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
|      570 |  208 | `static sxi32 GenStateArrowCaptureScan(` |
|        - |  209 | `	ph7_gen_state *pGen,` |
|        - |  210 | `	ph7_vm_func *pFunc,` |
|        - |  211 | `	SyToken *pStart,` |
|        - |  212 | `	SyToken *pEnd,` |
|        - |  213 | `	SyString *aShadow,` |
|        - |  214 | `	sxu32 nShadow)` |
|        5 |  215 | `{` |
|      575 |  216 | `	SyToken *pScan = pStart;` |
|        - |  217 | `	sxi32 rc;` |
|     3871 |  218 | `	while( pScan < pEnd ){` |
|     3301 |  219 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|      189 |  220 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|       62 |  221 | `				pScan->sData.zString,` |
|      124 |  222 | `				pScan->sData.zString + pScan->sData.nByte,` |
|       62 |  223 | `				aShadow,nShadow);` |
|      127 |  224 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  225 | `				return SXERR_ABORT;` |
|        - |  226 | `			}` |
|      127 |  227 | `			pScan++;` |
|      127 |  228 | `			continue;` |
|        - |  229 | `		}` |
|     3177 |  230 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
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
|     3153 |  382 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|     2905 |  383 | `			pScan++;` |
|     2905 |  384 | `			continue;` |
|        - |  385 | `		}` |
|        - |  386 | `		{` |
|        - |  387 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|      253 |  388 | `			SyToken *pDollar = pScan;` |
|      372 |  389 | `			while( &pDollar[1] < pEnd` |
|      253 |  390 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|      ! 0 |  391 | `				pDollar++;` |
|      ! 0 |  392 | `			}` |
|      253 |  393 | `			if( &pDollar[1] >= pEnd ){` |
|      ! 0 |  394 | `				break;` |
|        - |  395 | `			}` |
|      253 |  396 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  397 | `				pScan = pDollar + 1;` |
|      ! 0 |  398 | `				continue;` |
|        - |  399 | `			}` |
|      377 |  400 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|      248 |  401 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|      124 |  402 | `				aShadow,nShadow);` |
|      253 |  403 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  404 | `				return SXERR_ABORT;` |
|        - |  405 | `			}` |
|      253 |  406 | `			pScan = pDollar + 2;` |
|        - |  407 | `		}` |
|        5 |  408 | `	}` |
|      575 |  409 | `	return SXRET_OK;` |
|      290 |  410 | `}` |
|        - |  411 | `/*` |
|        - |  412 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|        - |  413 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|        - |  414 | ` * variables by value. The body is a single expression that acts as an` |
|        - |  415 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|        - |  416 | ` * $this is also made available.` |
|        - |  417 | ` */` |
|      546 |  418 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
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
|      551 |  435 | `	sxi32 iFlags = 0;` |
|      551 |  436 | `	int bStatic = 0;` |
|        - |  437 | `	sxi32 rc;` |
|        - |  438 | `	sxu32 n;` |
|      273 |  439 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |  440 |  |
|      551 |  441 | `	nLine = pGen->pIn->nLine;` |
|        - |  442 | ``	/* Attribute-sidecar key: `#[A] [static] fn` trivia is keyed to this token */`` |
|      551 |  443 | `	pTokKw = pGen->pIn;` |
|        - |  444 | `	/* Optional 'static' prefix */` |
|      546 |  445 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      551 |  446 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        9 |  447 | `		bStatic = 1;` |
|        9 |  448 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|        9 |  449 | `		pGen->pIn++;` |
|        4 |  450 | `	}` |
|        - |  451 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|      546 |  452 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      551 |  453 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|      ! 0 |  454 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  455 | `			"Arrow function: expected 'fn' keyword");` |
|      ! 0 |  456 | `		return SXERR_SYNTAX;` |
|        - |  457 | `	}` |
|      551 |  458 | `	pGen->pIn++; /* Jump 'fn' */` |
|        - |  459 | `	/* Optional '&' — return by reference */` |
|      551 |  460 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|      ! 0 |  461 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|      ! 0 |  462 | `		pGen->pIn++;` |
|      ! 0 |  463 | `	}` |
|        - |  464 | `	/* Expect '(' */` |
|      551 |  465 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
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
|      549 |  476 | `	pGen->pIn++; /* Jump '(' */` |
|        - |  477 | `	/* Delimit the parameter list */` |
|      549 |  478 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|      549 |  479 | `	if( pSigEnd >= pGen->pEnd ){` |
|        3 |  480 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - |  481 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|        3 |  482 | `		return SXERR_SYNTAX;` |
|        - |  483 | `	}` |
|        - |  484 | `	/* Allocate the function state */` |
|      547 |  485 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|      547 |  486 | `	if( pFunc == 0 ){` |
|      ! 0 |  487 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  488 | `			"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  489 | `		return SXERR_ABORT;` |
|        - |  490 | `	}` |
|        - |  491 | `	/* Generate a unique lambda name */` |
|      547 |  492 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|      549 |  493 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|        3 |  494 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|        1 |  495 | `	}` |
|      547 |  496 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|      547 |  497 | `	if( zDup == 0 ){` |
|      ! 0 |  498 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  499 | `			"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  500 | `		return SXERR_ABORT;` |
|        - |  501 | `	}` |
|      547 |  502 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|        - |  503 | `	/* Reflection getStartLine(): line of the ['static'] 'fn' keyword */` |
|      547 |  504 | `	pFunc->nLine = nLine;` |
|        - |  505 | ``	/* Expression-position attributes (`$f = #[A] fn () => …`) */`` |
|      547 |  506 | `	if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pFunc->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  507 | `		return SXERR_ABORT;` |
|        - |  508 | `	}` |
|        - |  509 | `	/* Collect function arguments */` |
|      547 |  510 | `	if( pGen->pIn < pSigEnd ){` |
|      157 |  511 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|      157 |  512 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  513 | `			return SXERR_ABORT;` |
|        - |  514 | `		}` |
|       76 |  515 | `	}` |
|        - |  516 | `	/* Point past ')' and parse optional return type */` |
|      547 |  517 | `	pGen->pIn = &pSigEnd[1];` |
|      547 |  518 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|      547 |  519 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  520 | `		return SXERR_ABORT;` |
|      547 |  521 | `	}else if( rc == SXERR_SYNTAX ){` |
|      ! 0 |  522 | `		return SXERR_SYNTAX;` |
|        - |  523 | `	}` |
|        - |  524 | `	/* Expect '=>' */` |
|      547 |  525 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
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
|      545 |  536 | `	pGen->pIn++; /* Jump '=>' */` |
|      545 |  537 | `	pBodyStart = pGen->pIn;` |
|      545 |  538 | `	pBodyEnd = pGen->pEnd;` |
|        - |  539 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|        - |  540 | `	 * recursively collect free-variable references from the body. The scan` |
|        - |  541 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|        - |  542 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|      545 |  543 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|        - |  544 | `	{` |
|      545 |  545 | `		SyString *aShadow = 0;` |
|      545 |  546 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|      545 |  547 | `		if( nShadow > 0 ){` |
|      155 |  548 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      150 |  549 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|      155 |  550 | `			if( aShadow == 0 ){` |
|      ! 0 |  551 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  552 | `					"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  553 | `				return SXERR_ABORT;` |
|        - |  554 | `			}` |
|      349 |  555 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|      199 |  556 | `				aShadow[n] = aArgs[n].sName;` |
|      102 |  557 | `			}` |
|       75 |  558 | `		}` |
|      815 |  559 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|      270 |  560 | `			aShadow,nShadow);` |
|      545 |  561 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  562 | `			return SXERR_ABORT;` |
|        - |  563 | `		}` |
|        - |  564 | `	}` |
|        - |  565 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|        - |  566 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|        - |  567 | `	 * captured value is silently dropped when the enclosing scope has no` |
|        - |  568 | `	 * $this. */` |
|      545 |  569 | `	if( !bStatic ){` |
|        - |  570 | `		char *zThisDup;` |
|      537 |  571 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|      537 |  572 | `		if( zThisDup == 0 ){` |
|      ! 0 |  573 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  574 | `				"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  575 | `			return SXERR_ABORT;` |
|        - |  576 | `		}` |
|      537 |  577 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      537 |  578 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|      537 |  579 | `		sEnv.nIdx = SXU32_HIGH;` |
|      537 |  580 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      537 |  581 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|      537 |  582 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      266 |  583 | `	}` |
|        - |  584 | `	/* Arrow functions are always closures */` |
|      545 |  585 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE;` |
|        - |  586 | `	/* Compile the body expression as an implicit return */` |
|      815 |  587 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      270 |  588 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|      545 |  589 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  590 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  591 | `			"PH7 engine is running out-of-memory");` |
|      ! 0 |  592 | `		return SXERR_ABORT;` |
|        - |  593 | `	}` |
|      545 |  594 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      545 |  595 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|      545 |  596 | `	pSavedEnd = pGen->pEnd;` |
|      545 |  597 | `	pGen->pIn = pBodyStart;` |
|      545 |  598 | `	pGen->pEnd = pBodyEnd;` |
|      545 |  599 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      545 |  600 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  601 | `		return SXERR_ABORT;` |
|        - |  602 | `	}` |
|        - |  603 | `	/* The cursor stopped just past the body expression */` |
|      545 |  604 | `	pFunc->nEndLine = (pGen->pIn > pBodyStart) ? pGen->pIn[-1].nLine : nLine;` |
|        - |  605 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|        - |  606 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|        - |  607 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|        - |  608 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|      545 |  609 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      545 |  610 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|      545 |  611 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|      545 |  612 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      545 |  613 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - |  614 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|      545 |  615 | `	pGen->pIn = pBodyEnd;` |
|      545 |  616 | `	pGen->pEnd = pSavedEnd;` |
|        - |  617 | `	/* Emit the load-closure instruction */` |
|      545 |  618 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|      545 |  619 | `	return SXRET_OK;` |
|      278 |  620 | `}` |
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
|       62 |  880 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  881 | `{` |
|        - |  882 | `	SyString *pName;` |
|        - |  883 | `	sxu32 nKeyID;` |
|        - |  884 | `	sxi32 rc;` |
|        - |  885 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|       67 |  886 | `	pName = &pGen->pIn->sData;` |
|       67 |  887 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       67 |  888 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|       67 |  889 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|        6 |  890 | `		SyToken *pTmp,*pNext = 0;` |
|        - |  891 | ``		/* A STATEMENT `echo` never reaches here — it dispatches through the statement`` |
|        - |  892 | `		 * table. Arriving in expression position means source like` |
|        - |  893 | ``		 * `fopen('f','r') or echo "IO error";`, which was a Symisc extension and is a`` |
|        - |  894 | `		 * php parse error (§10: a PH7-ism that changes the meaning of valid source is a` |
|        - |  895 | ``		 * bug). The one legitimate expression-echo is the token a `<?= ... ?>` short tag`` |
|        - |  896 | `		 * synthesizes, which raises nExprEchoOk around its own compile. */` |
|        6 |  897 | `		if( pGen->nExprEchoOk < 1 ){` |
|        3 |  898 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn - 1,0);` |
|        3 |  899 | `			return SXERR_ABORT;` |
|        - |  900 | `		}` |
|        - |  901 | `		/* Compile arguments one after one */` |
|        3 |  902 | `		pTmp = pGen->pEnd;` |
|        3 |  903 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|        5 |  904 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|        3 |  905 | `			if( pGen->pIn < pNext ){` |
|        3 |  906 | `				pGen->pEnd = pNext;` |
|        3 |  907 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|        3 |  908 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  909 | `					return SXERR_ABORT;` |
|        - |  910 | `				}` |
|        3 |  911 | `				if( rc != SXERR_EMPTY ){` |
|        - |  912 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|        - |  913 | `					 * without the overhead of a function call.` |
|        - |  914 | `					 * This is a very powerful optimization that improve` |
|        - |  915 | `					 * performance greatly.` |
|        - |  916 | `					 */` |
|        3 |  917 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|        1 |  918 | `				}` |
|        1 |  919 | `			}` |
|        - |  920 | `			/* Jump trailing commas */` |
|        3 |  921 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|      ! 0 |  922 | `				pNext++;` |
|      ! 0 |  923 | `			}` |
|        3 |  924 | `			pGen->pIn = pNext;` |
|        1 |  925 | `		}` |
|        - |  926 | `		/* Restore token stream */` |
|        3 |  927 | `		pGen->pEnd = pTmp;` |
|        2 |  928 | `	}else{` |
|       63 |  929 | `		sxi32 nArg = 0;` |
|       63 |  930 | `		sxu32 nIdx = 0;` |
|       63 |  931 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|       63 |  932 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  933 | `			return SXERR_ABORT;` |
|       63 |  934 | `		}else if(rc != SXERR_EMPTY ){` |
|       63 |  935 | `			nArg = 1;` |
|       29 |  936 | `		}` |
|       63 |  937 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|        - |  938 | `			ph7_value *pObj;` |
|        - |  939 | `			/* Emit the call instruction */` |
|       35 |  940 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       35 |  941 | `			if( pObj == 0 ){` |
|      ! 0 |  942 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  943 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |  944 | `				return SXERR_ABORT;` |
|        - |  945 | `			}` |
|       35 |  946 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|        - |  947 | `			/* Install in the literal table */` |
|       35 |  948 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       15 |  949 | `		}` |
|        - |  950 | `		/* Emit the call instruction */` |
|       63 |  951 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       63 |  952 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        - |  953 | `	}` |
|        - |  954 | `	/* Node successfully compiled */` |
|       65 |  955 | `	return SXRET_OK;` |
|       36 |  956 | `}` |
|        - |  957 | `/*` |
|        - |  958 | ` * Compile a node holding a variable declaration.` |
|        - |  959 | ` * According to the PHP language reference` |
|        - |  960 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|        - |  961 | ` *  The variable name is case-sensitive.` |
|        - |  962 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|        - |  963 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|        - |  964 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|        - |  965 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|        - |  966 | ` *  Note: $this is a special variable that can't be assigned.` |
|        - |  967 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|        - |  968 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|        - |  969 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|        - |  970 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|        - |  971 | ` *  the chapter on Expressions.` |
|        - |  972 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|        - |  973 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|        - |  974 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|        - |  975 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|        - |  976 | ` *  is being assigned (the source variable).` |
|        - |  977 | ` */` |
| 21711938 |  978 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  979 | `{` |
| 21711943 |  980 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|        - |  981 | `	sxi32 iVv;` |
|        - |  982 | `	sxi32 iP1;` |
| 21711943 |  983 | `	sxi32 iP2 = 0; /* 1 = quiet read (isset/empty): a missing variable must not warn */` |
|        - |  984 | `	void *p3;` |
|        - |  985 | `	sxi32 rc;` |
| 21711943 |  986 | `	iVv = -1; /* Variable variable counter */` |
| 43423893 |  987 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
| 21711955 |  988 | `		pGen->pIn++;` |
| 21711955 |  989 | `		iVv++;` |
|        5 |  990 | `	}` |
| 21711943 |  991 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|        - |  992 | `		/* Invalid variable name */` |
|      ! 0 |  993 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable or \"{\" or \"$\"");` |
|      ! 0 |  994 | `		if( rc == SXERR_ABORT ){` |
|        - |  995 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  996 | `			return SXERR_ABORT;` |
|        - |  997 | `		}` |
|      ! 0 |  998 | `		return SXRET_OK;` |
|        - |  999 | `	}` |
| 21711943 | 1000 | `	p3  = 0;` |
| 21711943 | 1001 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|        - | 1002 | `		/* Dynamic variable creation */` |
|       19 | 1003 | `		pGen->pIn++;  /* Jump the open curly */` |
|       19 | 1004 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|       19 | 1005 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 1006 | `			/* Empty expression */` |
|        - | 1007 | `			{` |
|        - | 1008 | `			/* php names the offending token and, for an empty "${}", stops there:` |
|        - | 1009 | `			 * the "expecting" tail only appears when something could still follow. */` |
|        3 | 1010 | `			SyToken *pBad = pGen->pIn < pGen->pEnd ? pGen->pIn : 0;` |
|        3 | 1011 | `			PH7_GenSyntaxError(&(*pGen),pBad,` |
|        1 | 1012 | `				(pBad && (pBad->nType & PH7_TK_CCB)) ? 0 : "variable or \"{\" or \"$\"");` |
|        - | 1013 | `			}` |
|        3 | 1014 | `			return SXRET_OK;` |
|        - | 1015 | `		}` |
|        - | 1016 | `		/* Compile the expression holding the variable name */` |
|       16 | 1017 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       16 | 1018 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1019 | `			return SXERR_ABORT;` |
|       16 | 1020 | `		}else if( rc == SXERR_EMPTY ){` |
|        3 | 1021 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,0);` |
|        3 | 1022 | `			return SXRET_OK;` |
|        - | 1023 | `		}` |
|        7 | 1024 | `	}else{` |
|        - | 1025 | `		SyHashEntry *pEntry;` |
|        - | 1026 | `		SyString *pName;` |
| 21711927 | 1027 | `		char *zName = 0;` |
|        - | 1028 | `		/* Extract variable name */` |
| 21711927 | 1029 | `		pName = &pGen->pIn->sData;` |
|        - | 1030 | `		/* Advance the stream cursor */` |
| 21711927 | 1031 | `		pGen->pIn++;` |
| 21711927 | 1032 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
| 21711927 | 1033 | `		if( pEntry == 0 ){` |
|        - | 1034 | `			/* Duplicate name */` |
|  1301041 | 1035 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  1301041 | 1036 | `			if( zName == 0 ){` |
|      ! 0 | 1037 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1038 | `				return SXERR_ABORT;` |
|        - | 1039 | `			}` |
|        - | 1040 | `			/* Install in the hashtable */` |
|  1301041 | 1041 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   650523 | 1042 | `		}else{` |
|        - | 1043 | `			/* Name already available */` |
| 20410891 | 1044 | `			zName = (char *)pEntry->pUserData;` |
|        - | 1045 | `		}` |
| 21711927 | 1046 | `		p3 = (void *)zName;` |
|        - | 1047 | `	}` |
| 21711939 | 1048 | `	iP1 = 0;` |
| 21711939 | 1049 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
| 17134451 | 1050 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|        - | 1051 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
| 11672395 | 1052 | `			iP1 = 1;` |
|  5836195 | 1053 | `		}` |
|  8567223 | 1054 | `	}` |
|        - | 1055 | ``	/* iP2 marks a QUIET read: `isset($x)` / `empty($x)` inspect a variable`` |
|        - | 1056 | `	 * without reading it, so an undefined one must not warn (php stays silent` |
|        - | 1057 | `	 * for both). Every other read of a missing variable warns — see OP_LOAD.` |
|        - | 1058 | `	 * The two flags are cleared before recursing into a subscript's index` |
|        - | 1059 | ``	 * expression, so `isset($a[$i])` still warns for an undefined $i, as php`` |
|        - | 1060 | ``	 * does. Contexts that VIVIFY (assignment targets, `??`, appends) already`` |
|        - | 1061 | `	 * emit iP1 = 0 and never reach the warning. */` |
| 21711939 | 1062 | `	if( iCompileFlag & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_QUIET_VAR) ){` |
|   386637 | 1063 | `		iP2 = 1;` |
| 21518623 | 1064 | `	}else if( iCompileFlag & EXPR_FLAG_RMW_LOAD ){` |
|        - | 1065 | ``		/* Warn-then-create: the read half of `$x++` / `$x .= ...` still needs a`` |
|        - | 1066 | `		 * writable slot, so it cannot use the read-only load above. */` |
|   671269 | 1067 | `		iP2 = 2;` |
|   335632 | 1068 | `	}` |
|        - | 1069 | `	/* Emit the load instruction */` |
| 21711939 | 1070 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,iP2,p3,0);` |
| 21711951 | 1071 | `	while( iVv > 0 ){` |
|       13 | 1072 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,iP2,0,0);` |
|       13 | 1073 | `		iVv--;` |
|        1 | 1074 | `	}` |
|        - | 1075 | `	/* Node successfully compiled */` |
| 21711939 | 1076 | `	return SXRET_OK;` |
| 10855974 | 1077 | `}` |
|        - | 1078 | `/*` |
|        - | 1079 | ` * Load a literal.` |
|        - | 1080 | ` */` |
| 14163792 | 1081 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|        5 | 1082 | `{` |
| 14163797 | 1083 | `	SyToken *pToken = pGen->pIn;` |
|        - | 1084 | `	ph7_value *pObj;` |
|        - | 1085 | `	SyString *pStr;` |
|        - | 1086 | `	sxu32 nIdx;` |
|        - | 1087 | `	/* Extract token value */` |
| 14163797 | 1088 | `	pStr = &pToken->sData;` |
|        - | 1089 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first. A reserved` |
|        - | 1090 | `	 * word used as a member NAME (Enum::Null, C::Array, $o->list()) is a plain` |
|        - | 1091 | `	 * identifier — the parser flagged its token so the whole value-folding chain is` |
|        - | 1092 | `	 * skipped and it falls through to the ordinary string-literal emit below. */` |
| 14163797 | 1093 | `	if( pToken->nType & PH7_TK_MEMBER_NAME ){` |
|        - | 1094 | `		/* fall through to the plain-string literal path */` |
| 11275212 | 1095 | `	}else if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  1695333 | 1096 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|        - | 1097 | `			/* NULL constant are always indexed at 0 */` |
|  1162579 | 1098 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|  1162579 | 1099 | `			return SXRET_OK;` |
|   532759 | 1100 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|        - | 1101 | `			/* TRUE constant are always indexed at 1 */` |
|   359917 | 1102 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|   359917 | 1103 | `			return SXRET_OK;` |
|        5 | 1104 | `		}` |
|  7574787 | 1105 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|  1594134 | 1106 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|        - | 1107 | `			/* FALSE constant are always indexed at 2 */` |
|   799773 | 1108 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   799773 | 1109 | `			return SXRET_OK;` |
|  6038796 | 1110 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   294530 | 1111 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|        - | 1112 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|     3911 | 1113 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     3911 | 1114 | `			if( pObj == 0 ){` |
|      ! 0 | 1115 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1116 | `				return SXERR_ABORT;` |
|        - | 1117 | `			}` |
|     3911 | 1118 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|        - | 1119 | `			/* Emit the load constant instruction */` |
|     3911 | 1120 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     3911 | 1121 | `			return SXRET_OK;` |
|  6032932 | 1122 | `	}else if( (pStr->nByte == sizeof("__FILE__") - 1 &&` |
|   516444 | 1123 | `		SyMemcmp(pStr->zString,"__FILE__",sizeof("__FILE__")-1) == 0) \|\|` |
|  6109459 | 1124 | `		(pStr->nByte == sizeof("__DIR__") - 1 &&` |
|   451630 | 1125 | `		SyMemcmp(pStr->zString,"__DIR__",sizeof("__DIR__")-1) == 0) ){` |
|        - | 1126 | `			/* __FILE__ / __DIR__ are magic constants resolved at COMPILE time to the` |
|        - | 1127 | `			 * file being compiled (where the token is written), NOT the runtime` |
|        - | 1128 | `			 * execution file. A function defined in a.php reporting __FILE__ must say` |
|        - | 1129 | `			 * a.php even when called from b.php — php semantics, and what Composer's` |
|        - | 1130 | ``			 * autoloader (loadClassLoader's `require __DIR__ . '/ClassLoader.php'`)`` |
|        - | 1131 | `			 * relies on. The runtime-constant path returned the caller's file. */` |
|     4007 | 1132 | `			int bDir = (pStr->zString[2] == 'D'); /* __DIR__ vs __FILE__ */` |
|     4007 | 1133 | `			SyString *pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     4007 | 1134 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     4007 | 1135 | `			if( pObj == 0 ){` |
|      ! 0 | 1136 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1137 | `				return SXERR_ABORT;` |
|        - | 1138 | `			}` |
|     4007 | 1139 | `			if( pFile && pFile->nByte > 0 ){` |
|      109 | 1140 | `				if( bDir ){` |
|        - | 1141 | `					const char *zDir;` |
|        - | 1142 | `					int nLen;` |
|        - | 1143 | `					SyString sDir;` |
|       56 | 1144 | `					zDir = PH7_ExtractDirName(pFile->zString,(int)pFile->nByte,&nLen);` |
|       56 | 1145 | `					SyStringInitFromBuf(&sDir,zDir,nLen);` |
|       56 | 1146 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&sDir);` |
|       30 | 1147 | `				}else{` |
|       57 | 1148 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,pFile);` |
|        - | 1149 | `				}` |
|       57 | 1150 | `			}else{` |
|        - | 1151 | `				SyString sMem;` |
|     3903 | 1152 | `				SyStringInitFromBuf(&sMem,":MEMORY:",sizeof(":MEMORY:")-1);` |
|     3903 | 1153 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sMem);` |
|        - | 1154 | `			}` |
|     4007 | 1155 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     4007 | 1156 | `			return SXRET_OK;` |
|  6004803 | 1157 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   242360 | 1158 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|        - | 1159 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|        8 | 1160 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        8 | 1161 | `			if( pObj == 0 ){` |
|      ! 0 | 1162 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1163 | `				return SXERR_ABORT;` |
|        - | 1164 | `			}` |
|        8 | 1165 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        - | 1166 | `				SyString sNs;` |
|        8 | 1167 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        8 | 1168 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|        5 | 1169 | `			}else{` |
|      ! 0 | 1170 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|        - | 1171 | `			}` |
|        8 | 1172 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        8 | 1173 | `			return SXRET_OK;` |
|  6016733 | 1174 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   431007 | 1175 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  6048365 | 1176 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   329520 | 1177 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|       11 | 1178 | `			GenBlock *pBlock = pGen->pCurrent;` |
|        - | 1179 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|       21 | 1180 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|        - | 1181 | `				/* Point to the upper block */` |
|       11 | 1182 | `				pBlock = pBlock->pParent;` |
|        1 | 1183 | `			}` |
|       11 | 1184 | `			if( pBlock == 0 ){` |
|        - | 1185 | `				/* Called in the global scope,load NULL */` |
|        5 | 1186 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|        3 | 1187 | `			}else{` |
|        - | 1188 | `				/* Extract the target function/method */` |
|        7 | 1189 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        7 | 1190 | `				int bMethod = (pStr->zString[2] == 'M'); /* __METHOD__ vs __FUNCTION__ */` |
|        7 | 1191 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        7 | 1192 | `				if( pObj == 0 ){` |
|      ! 0 | 1193 | `					PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1194 | `					return SXERR_ABORT;` |
|        - | 1195 | `				}` |
|        - | 1196 | `				/*` |
|        - | 1197 | `				 * __METHOD__ is the QUALIFIED name: "C::m" inside a method, and the plain` |
|        - | 1198 | `				 * function name inside a plain function (php does not answer "" there —` |
|        - | 1199 | `				 * PH7 loaded NULL, so __METHOD__ was empty in every free function and` |
|        - | 1200 | `				 * unqualified in every method).` |
|        - | 1201 | `				 */` |
|        8 | 1202 | `				if( bMethod && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|        3 | 1203 | `					SyString *pCls = &((ph7_class *)pFunc->pUserData)->sName;` |
|        - | 1204 | `					SyBlob sQual;` |
|        - | 1205 | `					SyString sOut;` |
|        3 | 1206 | `					SyBlobInit(&sQual,&pGen->pVm->sAllocator);` |
|        3 | 1207 | `					SyBlobFormat(&sQual,"%z::%z",pCls,&pFunc->sName);` |
|        3 | 1208 | `					SyStringInitFromBuf(&sOut,SyBlobData(&sQual),SyBlobLength(&sQual));` |
|        3 | 1209 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&sOut);` |
|        3 | 1210 | `					SyBlobRelease(&sQual);` |
|        2 | 1211 | `				}else{` |
|        5 | 1212 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|        - | 1213 | `				}` |
|        - | 1214 | `				/* Emit the load constant instruction */` |
|        7 | 1215 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - | 1216 | `			}` |
|       11 | 1217 | `			return SXRET_OK;` |
|        - | 1218 | `	}` |
|        - | 1219 | `	/* Query literal table */` |
| 11833619 | 1220 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|        - | 1221 | `		ph7_value *pLitObj;` |
|        - | 1222 | `		/* Unknown literal,install it in the literal table */` |
|  2439153 | 1223 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  2439153 | 1224 | `		if( pLitObj == 0 ){` |
|      ! 0 | 1225 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 | 1226 | `			return SXERR_ABORT;` |
|        - | 1227 | `		}` |
|  2439153 | 1228 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  2439153 | 1229 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  1219574 | 1230 | `	}` |
|        - | 1231 | `	/* Emit the load constant instruction */` |
| 11833619 | 1232 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
| 11833619 | 1233 | `	return SXRET_OK;` |
|  7081901 | 1234 | `}` |
|        - | 1235 | `/*` |
|        - | 1236 | ` * Resolve a namespace path or simply load a literal.` |
|        - | 1237 | ` * If the token stream contains namespace separators (backslashes),` |
|        - | 1238 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|        - | 1239 | ` * Otherwise, load the simple literal directly.` |
|        - | 1240 | ` */` |
| 14167780 | 1241 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|        5 | 1242 | `{` |
|        - | 1243 | `	sxi32 rc;` |
| 14167785 | 1244 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 1245 | `		return SXRET_OK;` |
|        - | 1246 | `	}` |
|        - | 1247 | `	/* Check if this is a multi-token namespace path */` |
| 14167785 | 1248 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|        - | 1249 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|     3993 | 1250 | `		SyBlob *pWorker = &pGen->sWorker;` |
|     3993 | 1251 | `		int isAbsolute = 0;` |
|     3993 | 1252 | `		SyBlobReset(pWorker);` |
|        - | 1253 | `		/* Check for leading backslash (absolute path) */` |
|     3993 | 1254 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|     3971 | 1255 | `			isAbsolute = 1;` |
|     3971 | 1256 | `			pGen->pIn++; /* Skip leading backslash */` |
|     1983 | 1257 | `		}` |
|        - | 1258 | `		/* Collect the raw path (no prefix yet) so its FIRST segment can be resolved` |
|        - | 1259 | ``		 * against use-imports below — php resolves `A\B\C` by mapping the leading`` |
|        - | 1260 | ``		 * `A` through the imports (`use X\A;` makes it `X\A\B\C`), and only prepends`` |
|        - | 1261 | ``		 * the current namespace when `A` matches no import. Blindly prefixing the`` |
|        - | 1262 | ``		 * namespace here produced e.g. `Ns\Ev\Bus` for `use ...\Event as Ev; Ev\Bus`. */`` |
|        - | 1263 | `		{` |
|        - | 1264 | `			SyBlob sRaw;` |
|     3993 | 1265 | `			SyBlobInit(&sRaw,&pGen->pVm->sAllocator);` |
|     4157 | 1266 | `			while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|     4157 | 1267 | `				if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       86 | 1268 | `					SyBlobAppend(&sRaw,"\\",1);` |
|       45 | 1269 | `				}else{` |
|     4075 | 1270 | `					SyBlobAppend(&sRaw,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        - | 1271 | `				}` |
|     4157 | 1272 | `				if( pGen->pIn == &pGen->pEnd[-1] ){` |
|     3993 | 1273 | `					pGen->pIn++;` |
|     3993 | 1274 | `					break;` |
|        - | 1275 | `				}` |
|      168 | 1276 | `				pGen->pIn++;` |
|        4 | 1277 | `			}` |
|     3993 | 1278 | `			if( isAbsolute ){` |
|     3971 | 1279 | `				SyBlobAppend(pWorker,SyBlobData(&sRaw),SyBlobLength(&sRaw)); /* FQN as written */` |
|     1988 | 1280 | `			}else{` |
|       24 | 1281 | `				const char *zRaw = (const char *)SyBlobData(&sRaw);` |
|       24 | 1282 | `				sxu32 nRaw = SyBlobLength(&sRaw);` |
|       24 | 1283 | `				sxu32 nFirst = 0;` |
|        - | 1284 | `				SyHashEntry *pNsImp;` |
|      108 | 1285 | `				while( nFirst < nRaw && zRaw[nFirst] != '\\' ){ nFirst++; }` |
|       24 | 1286 | `				pNsImp = SyHashGet(&pGen->hUseImports,(const void *)zRaw,nFirst);` |
|       24 | 1287 | `				if( pNsImp ){` |
|        - | 1288 | `					/* Leading segment is an imported alias: substitute its FQN. */` |
|       21 | 1289 | `					const char *zFQN = (const char *)pNsImp->pUserData;` |
|       21 | 1290 | `					SyBlobAppend(pWorker,zFQN,SyStrlen(zFQN));` |
|       21 | 1291 | `					SyBlobAppend(pWorker,zRaw + nFirst,nRaw - nFirst);` |
|       13 | 1292 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        3 | 1293 | `					SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        3 | 1294 | `					SyBlobAppend(pWorker,"\\",1);` |
|        3 | 1295 | `					SyBlobAppend(pWorker,zRaw,nRaw);` |
|        2 | 1296 | `				}else{` |
|      ! 0 | 1297 | `					SyBlobAppend(pWorker,zRaw,nRaw); /* global scope, no import */` |
|        - | 1298 | `				}` |
|        - | 1299 | `			}` |
|     3993 | 1300 | `			SyBlobRelease(&sRaw);` |
|        - | 1301 | `		}` |
|     3993 | 1302 | `		if( SyBlobLength(pWorker) > 0 ){` |
|        - | 1303 | `			ph7_value *pObj;` |
|        - | 1304 | `			SyString sPath;` |
|        - | 1305 | `			sxu32 nIdx;` |
|     3993 | 1306 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|        - | 1307 | `			/* Install in the literal table */` |
|     3993 | 1308 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|     3941 | 1309 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     3941 | 1310 | `				if( pObj == 0 ){` |
|      ! 0 | 1311 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 | 1312 | `					return SXERR_ABORT;` |
|        - | 1313 | `				}` |
|     3941 | 1314 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|     3941 | 1315 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     1968 | 1316 | `			}` |
|        - | 1317 | `			/* Emit the load constant instruction.` |
|        - | 1318 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|        - | 1319 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|     5987 | 1320 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|     1994 | 1321 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|     1994 | 1322 | `				nIdx,0,0);` |
|     3993 | 1323 | `			return SXRET_OK;` |
|        - | 1324 | `		}` |
|      ! 0 | 1325 | `	}` |
|        - | 1326 | `	/* Single-token literal: load directly */` |
| 14163797 | 1327 | `	rc = GenStateLoadLiteral(&(*pGen));` |
| 14163797 | 1328 | `	return rc;` |
|  7083895 | 1329 | `}` |
|        - | 1330 | `/*` |
|        - | 1331 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|        - | 1332 | ` */` |
|        - | 1333 | `/*` |
|        - | 1334 | `` * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of`` |
|        - | 1335 | `` * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument`` |
|        - | 1336 | ``  * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...` `` |
|        - | 1337 | ` * appeared outside a call argument list — a syntax error (PHP rejects it likewise).` |
|        - | 1338 | ` */` |
|      ! 0 | 1339 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|      ! 0 | 1340 | `{` |
|      ! 0 | 1341 | `	SXUNUSED(iCompileFlag);` |
|      ! 0 | 1342 | `	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|        - | 1343 | `		"Cannot use the first-class callable syntax '...' here");` |
|      ! 0 | 1344 | `	return SXERR_SYNTAX;` |
|      ! 0 | 1345 | `}` |
| 14167780 | 1346 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 1347 | `{` |
|        - | 1348 | `	sxi32 rc;` |
| 14167785 | 1349 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
| 14167785 | 1350 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1351 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 | 1352 | `		return rc;` |
|        - | 1353 | `	}` |
|        - | 1354 | `	/* Node successfully compiled */` |
| 14167785 | 1355 | `	return SXRET_OK;` |
|  7083895 | 1356 | `}` |
|        - | 1357 |  |
