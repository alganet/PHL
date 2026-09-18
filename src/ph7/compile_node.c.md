# src/ph7/compile_node.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 685/820 lines (83.54%)

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
|      632 |   36 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |   37 | `{` |
|      637 |   38 | `	ph7_vm_func *pAnnonFunc = 0; /* Annonymous function body */` |
|        - |   39 | `	char zName[512];         /* Unique lambda name */` |
|        - |   40 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|        - |   41 | `							  * one thread is allowed to compile the script.` |
|        - |   42 | `						      */` |
|        - |   43 | `	SyString sName;` |
|      637 |   44 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `$f = #[A] function…` trivia`` |
|        - |   45 | `	                              * is keyed to this ['static'] 'function' token */` |
|        - |   46 | `	sxu32 nKwLine;` |
|      637 |   47 | `	sxi32 iFlags = 0;` |
|        - |   48 | `	sxu32 nLen;` |
|        - |   49 | `	sxi32 rc;` |
|      316 |   50 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |   51 |  |
|      637 |   52 | `	nKwLine = pGen->pIn->nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|      632 |   53 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      637 |   54 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        - |   55 | `		/* Static closure: no $this auto-capture, bind refused */` |
|       29 |   56 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|       29 |   57 | `		pGen->pIn++; /* Jump the 'static' keyword */` |
|       13 |   58 | `	}` |
|      637 |   59 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|      637 |   60 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|      ! 0 |   61 | `		pGen->pIn++;` |
|      ! 0 |   62 | `	}` |
|        - |   63 | `	/* Generate a unique name */` |
|      637 |   64 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|        - |   65 | `	/* Make sure the generated name is unique */` |
|      637 |   66 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|      ! 0 |   67 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|      ! 0 |   68 | `	}` |
|      637 |   69 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|        - |   70 | `	/* Compile the lambda body */` |
|      637 |   71 | `	rc = GenStateCompileFunc(&(*pGen),&sName,iFlags,TRUE,&pAnnonFunc);` |
|      637 |   72 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |   73 | `		return SXERR_ABORT;` |
|        - |   74 | `	}` |
|      637 |   75 | `	if( pAnnonFunc ){` |
|      637 |   76 | `		pAnnonFunc->nLine = nKwLine;` |
|        - |   77 | ``		/* Expression-position attributes (`$f = #[A] function () {}`): the trivia`` |
|        - |   78 | `		 * sidecar keys them to the closure's first keyword token. */` |
|      637 |   79 | `		if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnnonFunc->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |   80 | `			return SXERR_ABORT;` |
|        - |   81 | `		}` |
|      316 |   82 | `	}` |
|        - |   83 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|        - |   84 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|        - |   85 | `	 * the handler wraps either in a Closure instance. */` |
|      637 |   86 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|        - |   87 | `	/* Node successfully compiled */` |
|      637 |   88 | `	return SXRET_OK;` |
|      321 |   89 | `}` |
|        - |   90 | `/*` |
|        - |   91 | ` * Add a free variable to the arrow function's closure environment, unless` |
|        - |   92 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|        - |   93 | ` * enclosing arrow level, or has already been captured.` |
|        - |   94 | ` */` |
|      296 |   95 | `static sxi32 GenStateArrowAddCapture(` |
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
|      301 |  107 | `	if( nByte == 0 ){` |
|      ! 0 |  108 | `		return SXRET_OK;` |
|        - |  109 | `	}` |
|      296 |  110 | `	if( nByte == sizeof("this")-1` |
|      164 |  111 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|        9 |  112 | `		return SXRET_OK;` |
|        - |  113 | `	}` |
|      361 |  114 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|      256 |  115 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|      249 |  116 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|      193 |  117 | `			return SXRET_OK;` |
|        - |  118 | `		}` |
|       36 |  119 | `	}` |
|      103 |  120 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      103 |  121 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|      133 |  122 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|       34 |  123 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|       34 |  124 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|        6 |  125 | `			return SXRET_OK;` |
|        - |  126 | `		}` |
|       17 |  127 | `	}` |
|       99 |  128 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|       99 |  129 | `	if( zDup == 0 ){` |
|      ! 0 |  130 | `		return SXERR_ABORT;` |
|        - |  131 | `	}` |
|       99 |  132 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       99 |  133 | `	sEnv.iFlags = 0;` |
|       99 |  134 | `	sEnv.nIdx = SXU32_HIGH;` |
|       99 |  135 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       99 |  136 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|       99 |  137 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       99 |  138 | `	return SXRET_OK;` |
|      153 |  139 | `}` |
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
|      592 |  208 | `static sxi32 GenStateArrowCaptureScan(` |
|        - |  209 | `	ph7_gen_state *pGen,` |
|        - |  210 | `	ph7_vm_func *pFunc,` |
|        - |  211 | `	SyToken *pStart,` |
|        - |  212 | `	SyToken *pEnd,` |
|        - |  213 | `	SyString *aShadow,` |
|        - |  214 | `	sxu32 nShadow)` |
|        5 |  215 | `{` |
|      597 |  216 | `	SyToken *pScan = pStart;` |
|        - |  217 | `	sxi32 rc;` |
|     3931 |  218 | `	while( pScan < pEnd ){` |
|     3339 |  219 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
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
|     3215 |  230 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|       43 |  231 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|       43 |  232 | `			SyToken *pFnKw = pScan;` |
|       40 |  233 | `			if( nKw == PH7_TKWRD_STATIC && &pScan[1] < pEnd` |
|        2 |  234 | `				&& (pScan[1].nType & PH7_TK_KEYWORD)` |
|        4 |  235 | `				&& SX_PTR_TO_INT(pScan[1].pUserData) == PH7_TKWRD_FN ){` |
|      ! 0 |  236 | `				pFnKw = &pScan[1];` |
|      ! 0 |  237 | `				nKw = PH7_TKWRD_FN;` |
|      ! 0 |  238 | `			}` |
|       43 |  239 | `			if( nKw == PH7_TKWRD_FN ){` |
|        - |  240 | `				SyToken *pInnerSigStart;` |
|        - |  241 | `				SyToken *pInnerSigEnd;` |
|        - |  242 | `				SyToken *pInnerBodyEnd;` |
|        - |  243 | `				SyString *aInnerShadow;` |
|        - |  244 | `				sxu32 nInnerShadow;` |
|        - |  245 | `				sxu32 nInnerParamMax;` |
|        - |  246 | `				SyToken *p;` |
|        - |  247 | `				int iNestInner;` |
|       28 |  248 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|       28 |  249 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|      ! 0 |  250 | `					pScan++;` |
|      ! 0 |  251 | `				}` |
|       28 |  252 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 |  253 | `					pScan++;` |
|      ! 0 |  254 | `					continue;` |
|        - |  255 | `				}` |
|       28 |  256 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|       28 |  257 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|        - |  258 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|       28 |  259 | `				if( pInnerSigEnd >= pEnd ){` |
|      ! 0 |  260 | `					pScan = pEnd;` |
|      ! 0 |  261 | `					continue;` |
|        - |  262 | `				}` |
|        - |  263 | `				/* Build an augmented shadow list: inherited + inner params */` |
|       28 |  264 | `				nInnerParamMax = 0;` |
|       78 |  265 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|       52 |  266 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|       20 |  267 | `						nInnerParamMax++;` |
|        9 |  268 | `					}` |
|       27 |  269 | `				}` |
|       28 |  270 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|       26 |  271 | `					&pGen->pVm->sAllocator,` |
|       26 |  272 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|       28 |  273 | `				if( aInnerShadow == 0 ){` |
|      ! 0 |  274 | `					return SXERR_ABORT;` |
|        - |  275 | `				}` |
|       28 |  276 | `				nInnerShadow = 0;` |
|       34 |  277 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|        7 |  278 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|        4 |  279 | `				}` |
|       78 |  280 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
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
|       28 |  292 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|       28 |  293 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
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
|       28 |  305 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|       28 |  306 | `					pScan++; /* past '=>' */` |
|       13 |  307 | `				}` |
|       28 |  308 | `				pInnerBodyEnd = pScan;` |
|       28 |  309 | `				iNestInner = 0;` |
|      162 |  310 | `				while( pInnerBodyEnd < pEnd ){` |
|      142 |  311 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|        - |  312 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|        - |  313 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|        7 |  314 | `						break;` |
|        - |  315 | `					}` |
|      136 |  316 | `					if( pInnerBodyEnd->nType &` |
|        - |  317 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        6 |  318 | `						iNestInner++;` |
|      134 |  319 | `					}else if( pInnerBodyEnd->nType &` |
|        - |  320 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        6 |  321 | `						iNestInner--;` |
|        2 |  322 | `					}` |
|      136 |  323 | `					pInnerBodyEnd++;` |
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
|       28 |  336 | `					SyToken *pArgStart = pInnerSigStart;` |
|       46 |  337 | `					while( pArgStart < pInnerSigEnd ){` |
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
|       41 |  373 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       13 |  374 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|       28 |  375 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  376 | `					return SXERR_ABORT;` |
|        - |  377 | `				}` |
|       28 |  378 | `				pScan = pInnerBodyEnd;` |
|       28 |  379 | `				continue;` |
|        - |  380 | `			}` |
|        7 |  381 | `		}` |
|     3189 |  382 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|     2917 |  383 | `			pScan++;` |
|     2917 |  384 | `			continue;` |
|        - |  385 | `		}` |
|        - |  386 | `		{` |
|        - |  387 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|      277 |  388 | `			SyToken *pDollar = pScan;` |
|      408 |  389 | `			while( &pDollar[1] < pEnd` |
|      277 |  390 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|      ! 0 |  391 | `				pDollar++;` |
|      ! 0 |  392 | `			}` |
|      277 |  393 | `			if( &pDollar[1] >= pEnd ){` |
|      ! 0 |  394 | `				break;` |
|        - |  395 | `			}` |
|      277 |  396 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  397 | `				pScan = pDollar + 1;` |
|      ! 0 |  398 | `				continue;` |
|        - |  399 | `			}` |
|      413 |  400 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|      272 |  401 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|      136 |  402 | `				aShadow,nShadow);` |
|      277 |  403 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  404 | `				return SXERR_ABORT;` |
|        - |  405 | `			}` |
|      277 |  406 | `			pScan = pDollar + 2;` |
|        - |  407 | `		}` |
|        5 |  408 | `	}` |
|      597 |  409 | `	return SXRET_OK;` |
|      301 |  410 | `}` |
|        - |  411 | `/*` |
|        - |  412 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|        - |  413 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|        - |  414 | ` * variables by value. The body is a single expression that acts as an` |
|        - |  415 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|        - |  416 | ` * $this is also made available.` |
|        - |  417 | ` */` |
|      566 |  418 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
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
|      571 |  435 | `	sxi32 iFlags = 0;` |
|      571 |  436 | `	int bStatic = 0;` |
|        - |  437 | `	sxi32 rc;` |
|        - |  438 | `	sxu32 n;` |
|      283 |  439 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |  440 |  |
|      571 |  441 | `	nLine = pGen->pIn->nLine;` |
|        - |  442 | ``	/* Attribute-sidecar key: `#[A] [static] fn` trivia is keyed to this token */`` |
|      571 |  443 | `	pTokKw = pGen->pIn;` |
|        - |  444 | `	/* Optional 'static' prefix */` |
|      566 |  445 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      571 |  446 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        9 |  447 | `		bStatic = 1;` |
|        9 |  448 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|        9 |  449 | `		pGen->pIn++;` |
|        4 |  450 | `	}` |
|        - |  451 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|      566 |  452 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      571 |  453 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|      ! 0 |  454 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  455 | `			"Arrow function: expected 'fn' keyword");` |
|      ! 0 |  456 | `		return SXERR_SYNTAX;` |
|        - |  457 | `	}` |
|      571 |  458 | `	pGen->pIn++; /* Jump 'fn' */` |
|        - |  459 | `	/* Optional '&' — return by reference */` |
|      571 |  460 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|      ! 0 |  461 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|      ! 0 |  462 | `		pGen->pIn++;` |
|      ! 0 |  463 | `	}` |
|        - |  464 | `	/* Expect '(' */` |
|      571 |  465 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
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
|      569 |  476 | `	pGen->pIn++; /* Jump '(' */` |
|        - |  477 | `	/* Delimit the parameter list */` |
|      569 |  478 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|      569 |  479 | `	if( pSigEnd >= pGen->pEnd ){` |
|        3 |  480 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - |  481 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|        3 |  482 | `		return SXERR_SYNTAX;` |
|        - |  483 | `	}` |
|        - |  484 | `	/* Allocate the function state */` |
|      567 |  485 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|      567 |  486 | `	if( pFunc == 0 ){` |
|      ! 0 |  487 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  488 | `			"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  489 | `		return SXERR_ABORT;` |
|        - |  490 | `	}` |
|        - |  491 | `	/* Generate a unique lambda name */` |
|      567 |  492 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|      569 |  493 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|        3 |  494 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|        1 |  495 | `	}` |
|      567 |  496 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|      567 |  497 | `	if( zDup == 0 ){` |
|      ! 0 |  498 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  499 | `			"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  500 | `		return SXERR_ABORT;` |
|        - |  501 | `	}` |
|      567 |  502 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|        - |  503 | `	/* Reflection getStartLine(): line of the ['static'] 'fn' keyword */` |
|      567 |  504 | `	pFunc->nLine = nLine;` |
|        - |  505 | ``	/* Expression-position attributes (`$f = #[A] fn () => …`) */`` |
|      567 |  506 | `	if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pFunc->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  507 | `		return SXERR_ABORT;` |
|        - |  508 | `	}` |
|        - |  509 | `	/* Collect function arguments */` |
|      567 |  510 | `	if( pGen->pIn < pSigEnd ){` |
|      157 |  511 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|      157 |  512 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  513 | `			return SXERR_ABORT;` |
|        - |  514 | `		}` |
|       76 |  515 | `	}` |
|        - |  516 | `	/* Point past ')' and parse optional return type */` |
|      567 |  517 | `	pGen->pIn = &pSigEnd[1];` |
|      567 |  518 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|      567 |  519 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  520 | `		return SXERR_ABORT;` |
|      567 |  521 | `	}else if( rc == SXERR_SYNTAX ){` |
|      ! 0 |  522 | `		return SXERR_SYNTAX;` |
|        - |  523 | `	}` |
|        - |  524 | `	/* Expect '=>' */` |
|      567 |  525 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
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
|      565 |  536 | `	pGen->pIn++; /* Jump '=>' */` |
|      565 |  537 | `	pBodyStart = pGen->pIn;` |
|      565 |  538 | `	pBodyEnd = pGen->pEnd;` |
|        - |  539 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|        - |  540 | `	 * recursively collect free-variable references from the body. The scan` |
|        - |  541 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|        - |  542 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|      565 |  543 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|        - |  544 | `	{` |
|      565 |  545 | `		SyString *aShadow = 0;` |
|      565 |  546 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|      565 |  547 | `		if( nShadow > 0 ){` |
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
|      845 |  559 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|      280 |  560 | `			aShadow,nShadow);` |
|      565 |  561 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  562 | `			return SXERR_ABORT;` |
|        - |  563 | `		}` |
|        - |  564 | `	}` |
|        - |  565 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|        - |  566 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|        - |  567 | `	 * captured value is silently dropped when the enclosing scope has no` |
|        - |  568 | `	 * $this. */` |
|      565 |  569 | `	if( !bStatic ){` |
|        - |  570 | `		char *zThisDup;` |
|      557 |  571 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|      557 |  572 | `		if( zThisDup == 0 ){` |
|      ! 0 |  573 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  574 | `				"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  575 | `			return SXERR_ABORT;` |
|        - |  576 | `		}` |
|      557 |  577 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      557 |  578 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|      557 |  579 | `		sEnv.nIdx = SXU32_HIGH;` |
|      557 |  580 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      557 |  581 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|      557 |  582 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      276 |  583 | `	}` |
|        - |  584 | `	/* Arrow functions are always closures; the ARROW mark tells OP_LOAD_CLOSURE` |
|        - |  585 | `	 * these captures are implicit (auto-scanned) so an undefined one stays silent` |
|        - |  586 | `	 * at creation — php only warns when the body reads it. */` |
|      565 |  587 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE \| VM_FUNC_ARROW;` |
|        - |  588 | `	/* Compile the body expression as an implicit return */` |
|      845 |  589 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      280 |  590 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|      565 |  591 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  592 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  593 | `			"PH7 engine is running out-of-memory");` |
|      ! 0 |  594 | `		return SXERR_ABORT;` |
|        - |  595 | `	}` |
|      565 |  596 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      565 |  597 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|      565 |  598 | `	pSavedEnd = pGen->pEnd;` |
|      565 |  599 | `	pGen->pIn = pBodyStart;` |
|      565 |  600 | `	pGen->pEnd = pBodyEnd;` |
|        - |  601 | ``	/* The body is an implicit `return <expr>`, which READS its operands. Compile`` |
|        - |  602 | `	 * it read-only (like echo / string interpolation) so a lone undefined variable` |
|        - |  603 | ``	 * — e.g. `fn()=>$z` for an auto-capture that was undefined at creation and so`` |
|        - |  604 | `	 * never captured (see VmExecOpLoadClosure) — raises php's "Undefined variable"` |
|        - |  605 | `	 * warning at the read instead of being loaded quietly as a plain expression` |
|        - |  606 | ``	 * statement (`$z;`, silent in both engines) would be. */`` |
|      565 |  607 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|      565 |  608 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  609 | `		return SXERR_ABORT;` |
|        - |  610 | `	}` |
|        - |  611 | `	/* The cursor stopped just past the body expression */` |
|      565 |  612 | `	pFunc->nEndLine = (pGen->pIn > pBodyStart) ? pGen->pIn[-1].nLine : nLine;` |
|        - |  613 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|        - |  614 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|        - |  615 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|        - |  616 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|      565 |  617 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      565 |  618 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|      565 |  619 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|      565 |  620 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      565 |  621 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - |  622 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|      565 |  623 | `	pGen->pIn = pBodyEnd;` |
|      565 |  624 | `	pGen->pEnd = pSavedEnd;` |
|        - |  625 | `	/* Emit the load-closure instruction */` |
|      565 |  626 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|      565 |  627 | `	return SXRET_OK;` |
|      288 |  628 | `}` |
|        - |  629 | `/*` |
|        - |  630 | ` * Compile a single arm's expression range into a freshly-allocated` |
|        - |  631 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|        - |  632 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|        - |  633 | ` * expression's value.` |
|        - |  634 | ` */` |
|      364 |  635 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|        - |  636 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|        2 |  637 | `{` |
|        - |  638 | `	SySet *pInstrContainer;` |
|        - |  639 | `	SyToken *pTmpIn,*pTmpEnd;` |
|        - |  640 | `	GenBlock *pArmBlock;` |
|        - |  641 | `	sxi32 rc;` |
|      366 |  642 | `	pTmpIn  = pGen->pIn;` |
|      366 |  643 | `	pTmpEnd = pGen->pEnd;` |
|      366 |  644 | `	pGen->pIn  = pStart;` |
|      366 |  645 | `	pGen->pEnd = pStop;` |
|      366 |  646 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      366 |  647 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|        - |  648 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|        - |  649 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|        - |  650 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|        - |  651 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|        - |  652 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|      548 |  653 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      182 |  654 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|      366 |  655 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  656 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 |  657 | `		pGen->pIn  = pTmpIn;` |
|      ! 0 |  658 | `		pGen->pEnd = pTmpEnd;` |
|      ! 0 |  659 | `		return SXERR_ABORT;` |
|        - |  660 | `	}` |
|      366 |  661 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      366 |  662 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      366 |  663 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|      366 |  664 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|      366 |  665 | `	GenStateLeaveBlock(&(*pGen),0);` |
|      366 |  666 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      366 |  667 | `	pGen->pIn  = pTmpIn;` |
|      366 |  668 | `	pGen->pEnd = pTmpEnd;` |
|      366 |  669 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  670 | `		return SXERR_ABORT;` |
|        - |  671 | `	}` |
|      366 |  672 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 |  673 | `		return SXERR_EMPTY;` |
|        - |  674 | `	}` |
|      366 |  675 | `	return SXRET_OK;` |
|      184 |  676 | `}` |
|        - |  677 | `/*` |
|        - |  678 | ` * Compile a PHP 8.0 match expression:` |
|        - |  679 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|        - |  680 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|        - |  681 | ` * Strict comparison (===) is used between the subject and each condition.` |
|        - |  682 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|        - |  683 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|        - |  684 | ` */` |
|        - |  685 | `/*` |
|        - |  686 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|        - |  687 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|        - |  688 | ` * caller can bail out of the current expression.` |
|        - |  689 | ` */` |
|        2 |  690 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|        1 |  691 | `{` |
|        - |  692 | `	va_list ap;` |
|        - |  693 | `	sxi32 rc;` |
|        - |  694 | `	SyBlob sMsg;` |
|        3 |  695 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|        3 |  696 | `	va_start(ap,zFmt);` |
|        3 |  697 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|        3 |  698 | `	va_end(ap);` |
|        3 |  699 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|        3 |  700 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|        3 |  701 | `	SyBlobRelease(&sMsg);` |
|        3 |  702 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  703 | `		return SXERR_ABORT;` |
|        - |  704 | `	}` |
|        3 |  705 | `	return SXERR_SYNTAX;` |
|        2 |  706 | `}` |
|        - |  707 | `/*` |
|        - |  708 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|        - |  709 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|        - |  710 | ` * Returns the stop token pointer (or pEnd if none found).` |
|        - |  711 | ` */` |
|      366 |  712 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|        3 |  713 | `{` |
|      369 |  714 | `	SyToken *pCur = pStart;` |
|      369 |  715 | `	int iNest = 0;` |
|      889 |  716 | `	while( pCur < pEnd ){` |
|      853 |  717 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       17 |  718 | `			iNest++;` |
|      845 |  719 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       17 |  720 | `			iNest--;` |
|      829 |  721 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|      332 |  722 | `			return pCur;` |
|        - |  723 | `		}` |
|      523 |  724 | `		pCur++;` |
|        3 |  725 | `	}` |
|       39 |  726 | `	return pEnd;` |
|      186 |  727 | `}` |
|       74 |  728 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        4 |  729 | `{` |
|        - |  730 | `	ph7_match *pMatch;` |
|        - |  731 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|       78 |  732 | `	int bHasDefault = 0;` |
|        - |  733 | `	sxu32 nLine;` |
|        - |  734 | `	sxi32 rc;` |
|       37 |  735 | `	SXUNUSED(iCompileFlag);` |
|       78 |  736 | `	nLine = pGen->pIn->nLine;` |
|       78 |  737 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|        - |  738 | `	/* Expect '(' */` |
|       78 |  739 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 |  740 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  741 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|      ! 0 |  742 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|        - |  743 | `	}` |
|       78 |  744 | `	pGen->pIn++; /* Jump '(' */` |
|       78 |  745 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|       78 |  746 | `	if( pSubjEnd >= pGen->pEnd ){` |
|      ! 0 |  747 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  748 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|        - |  749 | `	}` |
|       78 |  750 | `	if( pGen->pIn >= pSubjEnd ){` |
|      ! 0 |  751 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  752 | `			"syntax error, unexpected \")\", expecting match subject");` |
|        - |  753 | `	}` |
|        - |  754 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|       78 |  755 | `	pSavedEnd = pGen->pEnd;` |
|       78 |  756 | `	pGen->pEnd = pSubjEnd;` |
|       78 |  757 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       78 |  758 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  759 | `		return SXERR_ABORT;` |
|        - |  760 | `	}` |
|       78 |  761 | `	pGen->pEnd = pSavedEnd;` |
|       78 |  762 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|        - |  763 | `	/* Expect '{' */` |
|       78 |  764 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|      ! 0 |  765 | `		return GenStateMatchError(pGen,` |
|      ! 0 |  766 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|        - |  767 | `			"syntax error, expecting \"{\" after match subject");` |
|        - |  768 | `	}` |
|       78 |  769 | `	pGen->pIn++; /* Jump '{' */` |
|       78 |  770 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|       78 |  771 | `	if( pBodyEnd >= pGen->pEnd ){` |
|      ! 0 |  772 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  773 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|        - |  774 | `	}` |
|        - |  775 | `	/* Allocate ph7_match container */` |
|       78 |  776 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|       78 |  777 | `	if( pMatch == 0 ){` |
|      ! 0 |  778 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  779 | `			"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  780 | `		return SXERR_ABORT;` |
|        - |  781 | `	}` |
|       78 |  782 | `	SyZero(pMatch,sizeof(ph7_match));` |
|       78 |  783 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|        - |  784 | `	/* Iterate arms */` |
|      266 |  785 | `	while( pGen->pIn < pBodyEnd ){` |
|        - |  786 | `		ph7_match_arm sArm;` |
|        - |  787 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|      195 |  788 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|      195 |  789 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|      195 |  790 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|      195 |  791 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - |  792 | `		/* 'default' arm? */` |
|      192 |  793 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      110 |  794 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|       24 |  795 | `			if( bHasDefault ){` |
|        3 |  796 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|        - |  797 | `					"Match expressions may only contain one default arm");` |
|        4 |  798 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  799 | `			}` |
|       22 |  800 | `			sArm.bDefault = 1;` |
|       22 |  801 | `			bHasDefault = 1;` |
|       22 |  802 | `			pGen->pIn++;` |
|       22 |  803 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|      ! 0 |  804 | `				return GenStateMatchError(pGen,nArmLine,` |
|        - |  805 | `					"syntax error, expecting \"=>\" after 'default'");` |
|        - |  806 | `			}` |
|       22 |  807 | `			pGen->pIn++; /* Jump '=>' */` |
|       12 |  808 | `		}else{` |
|        - |  809 | `			/* Condition list: cond (',' cond)* '=>' */` |
|      173 |  810 | `			pCondStart = pGen->pIn;` |
|      173 |  811 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|        - |  812 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|      181 |  813 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|        - |  814 | `				SySet sCondBc;` |
|        9 |  815 | `				if( pCondStart >= pArrow ){` |
|      ! 0 |  816 | `					return GenStateMatchError(pGen,nArmLine,` |
|        - |  817 | `						"syntax error, empty match condition expression");` |
|        - |  818 | `				}` |
|        9 |  819 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        9 |  820 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|        9 |  821 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  822 | `					return SXERR_ABORT;` |
|        - |  823 | `				}` |
|        9 |  824 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|        9 |  825 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|        9 |  826 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|        - |  827 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|        1 |  828 | `			}` |
|      173 |  829 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|        3 |  830 | `				return GenStateMatchError(pGen,nArmLine,` |
|        - |  831 | `					"syntax error, expecting \"=>\" in match arm");` |
|        - |  832 | `			}` |
|      170 |  833 | `			if( pCondStart >= pArrow ){` |
|      ! 0 |  834 | `				return GenStateMatchError(pGen,nArmLine,` |
|        - |  835 | `					"syntax error, empty match condition expression");` |
|        - |  836 | `			}` |
|        - |  837 | `			{` |
|        - |  838 | `				SySet sCondBc;` |
|      170 |  839 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      170 |  840 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|      170 |  841 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  842 | `					return SXERR_ABORT;` |
|        - |  843 | `				}` |
|      170 |  844 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|        - |  845 | `			}` |
|      170 |  846 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|        - |  847 | `		}` |
|        - |  848 | `		/* Compile result expression: up to top-level ',' or body end */` |
|      190 |  849 | `		pResStart = pGen->pIn;` |
|      190 |  850 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|      190 |  851 | `		if( pResStart >= pResEnd ){` |
|      ! 0 |  852 | `			return GenStateMatchError(pGen,nArmLine,` |
|        - |  853 | `				"syntax error, expected expression after \"=>\"");` |
|        - |  854 | `		}` |
|      190 |  855 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|      190 |  856 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  857 | `			return SXERR_ABORT;` |
|        - |  858 | `		}` |
|      190 |  859 | `		pGen->pIn = pResEnd;` |
|      190 |  860 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|      156 |  861 | `			pGen->pIn++; /* Skip trailing ',' */` |
|       77 |  862 | `		}` |
|      190 |  863 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|        2 |  864 | `	}` |
|       73 |  865 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|       73 |  866 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|       73 |  867 | `	return SXRET_OK;` |
|       41 |  868 | `}` |
|        - |  869 | `/*` |
|        - |  870 | ` * Compile a backtick quoted string.` |
|        - |  871 | ` */` |
|        2 |  872 | `PH7_PRIVATE sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        1 |  873 | `{` |
|        1 |  874 | `	SXUNUSED(iCompileFlag);` |
|        - |  875 | `	/*` |
|        - |  876 | ``	 * The backtick (`) operator was DEPRECATED by php (shell_exec() is the replacement).`` |
|        - |  877 | `	 * PHL targets php's *non-deprecated* surface, so it is a hard parse error — never` |
|        - |  878 | `	 * compiled to a shell_exec() call.` |
|        - |  879 | `	 */` |
|        3 |  880 | `	PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|        - |  881 | ``		"syntax error, the backtick (`) operator was removed, use shell_exec() instead");`` |
|        3 |  882 | `	return SXERR_ABORT;` |
|        1 |  883 | `}` |
|        - |  884 | `/*` |
|        - |  885 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|        - |  886 | ` * construct.` |
|        - |  887 | ` */` |
|       62 |  888 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  889 | `{` |
|        - |  890 | `	SyString *pName;` |
|        - |  891 | `	sxu32 nKeyID;` |
|        - |  892 | `	sxi32 rc;` |
|        - |  893 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|       67 |  894 | `	pName = &pGen->pIn->sData;` |
|       67 |  895 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       67 |  896 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|       67 |  897 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|        6 |  898 | `		SyToken *pTmp,*pNext = 0;` |
|        - |  899 | ``		/* A STATEMENT `echo` never reaches here — it dispatches through the statement`` |
|        - |  900 | `		 * table. Arriving in expression position means source like` |
|        - |  901 | ``		 * `fopen('f','r') or echo "IO error";`, which was a Symisc extension and is a`` |
|        - |  902 | `		 * php parse error (§10: a PH7-ism that changes the meaning of valid source is a` |
|        - |  903 | ``		 * bug). The one legitimate expression-echo is the token a `<?= ... ?>` short tag`` |
|        - |  904 | `		 * synthesizes, which raises nExprEchoOk around its own compile. */` |
|        6 |  905 | `		if( pGen->nExprEchoOk < 1 ){` |
|        3 |  906 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn - 1,0);` |
|        3 |  907 | `			return SXERR_ABORT;` |
|        - |  908 | `		}` |
|        - |  909 | `		/* Compile arguments one after one */` |
|        3 |  910 | `		pTmp = pGen->pEnd;` |
|        3 |  911 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|        5 |  912 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|        3 |  913 | `			if( pGen->pIn < pNext ){` |
|        3 |  914 | `				pGen->pEnd = pNext;` |
|        3 |  915 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|        3 |  916 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  917 | `					return SXERR_ABORT;` |
|        - |  918 | `				}` |
|        3 |  919 | `				if( rc != SXERR_EMPTY ){` |
|        - |  920 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|        - |  921 | `					 * without the overhead of a function call.` |
|        - |  922 | `					 * This is a very powerful optimization that improve` |
|        - |  923 | `					 * performance greatly.` |
|        - |  924 | `					 */` |
|        3 |  925 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|        1 |  926 | `				}` |
|        1 |  927 | `			}` |
|        - |  928 | `			/* Jump trailing commas */` |
|        3 |  929 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|      ! 0 |  930 | `				pNext++;` |
|      ! 0 |  931 | `			}` |
|        3 |  932 | `			pGen->pIn = pNext;` |
|        1 |  933 | `		}` |
|        - |  934 | `		/* Restore token stream */` |
|        3 |  935 | `		pGen->pEnd = pTmp;` |
|        2 |  936 | `	}else{` |
|       63 |  937 | `		sxi32 nArg = 0;` |
|       63 |  938 | `		sxu32 nIdx = 0;` |
|       63 |  939 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|       63 |  940 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  941 | `			return SXERR_ABORT;` |
|       63 |  942 | `		}else if(rc != SXERR_EMPTY ){` |
|       63 |  943 | `			nArg = 1;` |
|       29 |  944 | `		}` |
|       63 |  945 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|        - |  946 | `			ph7_value *pObj;` |
|        - |  947 | `			/* Emit the call instruction */` |
|       35 |  948 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       35 |  949 | `			if( pObj == 0 ){` |
|      ! 0 |  950 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  951 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |  952 | `				return SXERR_ABORT;` |
|        - |  953 | `			}` |
|       35 |  954 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|        - |  955 | `			/* Install in the literal table */` |
|       35 |  956 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       15 |  957 | `		}` |
|        - |  958 | `		/* Emit the call instruction */` |
|       63 |  959 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       63 |  960 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        - |  961 | `	}` |
|        - |  962 | `	/* Node successfully compiled */` |
|       65 |  963 | `	return SXRET_OK;` |
|       36 |  964 | `}` |
|        - |  965 | `/*` |
|        - |  966 | ` * Compile a node holding a variable declaration.` |
|        - |  967 | ` * According to the PHP language reference` |
|        - |  968 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|        - |  969 | ` *  The variable name is case-sensitive.` |
|        - |  970 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|        - |  971 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|        - |  972 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|        - |  973 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|        - |  974 | ` *  Note: $this is a special variable that can't be assigned.` |
|        - |  975 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|        - |  976 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|        - |  977 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|        - |  978 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|        - |  979 | ` *  the chapter on Expressions.` |
|        - |  980 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|        - |  981 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|        - |  982 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|        - |  983 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|        - |  984 | ` *  is being assigned (the source variable).` |
|        - |  985 | ` */` |
| 21589842 |  986 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  987 | `{` |
| 21589847 |  988 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|        - |  989 | `	sxi32 iVv;` |
|        - |  990 | `	sxi32 iP1;` |
| 21589847 |  991 | `	sxi32 iP2 = 0; /* 1 = quiet read (isset/empty): a missing variable must not warn */` |
|        - |  992 | `	void *p3;` |
|        - |  993 | `	sxi32 rc;` |
| 21589847 |  994 | `	iVv = -1; /* Variable variable counter */` |
| 43179701 |  995 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
| 21589859 |  996 | `		pGen->pIn++;` |
| 21589859 |  997 | `		iVv++;` |
|        5 |  998 | `	}` |
| 21589847 |  999 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|        - | 1000 | `		/* Invalid variable name */` |
|      ! 0 | 1001 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable or \"{\" or \"$\"");` |
|      ! 0 | 1002 | `		if( rc == SXERR_ABORT ){` |
|        - | 1003 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1004 | `			return SXERR_ABORT;` |
|        - | 1005 | `		}` |
|      ! 0 | 1006 | `		return SXRET_OK;` |
|        - | 1007 | `	}` |
| 21589847 | 1008 | `	p3  = 0;` |
| 21589847 | 1009 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|        - | 1010 | `		/* Dynamic variable creation */` |
|       19 | 1011 | `		pGen->pIn++;  /* Jump the open curly */` |
|       19 | 1012 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|       19 | 1013 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 1014 | `			/* Empty expression */` |
|        - | 1015 | `			{` |
|        - | 1016 | `			/* php names the offending token and, for an empty "${}", stops there:` |
|        - | 1017 | `			 * the "expecting" tail only appears when something could still follow. */` |
|        - | 1018 | ``			/* `${}`: pEnd was stepped back past the trailing '}', so the token php`` |
|        - | 1019 | `			 * names sits AT pEnd. Reach for it before deciding the tail -- php stops` |
|        - | 1020 | `			 * at "unexpected token \"}\"" with no "expecting" clause, which the` |
|        - | 1021 | `			 * NULL-token path could not express because it never saw the '}'. */` |
|        3 | 1022 | `			SyToken *pBad = pGen->pIn < pGen->pEnd ? pGen->pIn : 0;` |
|        3 | 1023 | `			if( pBad == 0 && pGen->pTokenSet ){` |
|        3 | 1024 | `				SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        3 | 1025 | `				SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|        3 | 1026 | `				if( pGen->pEnd >= pBase && pGen->pEnd < pStreamEnd ){` |
|        3 | 1027 | `					pBad = pGen->pEnd;` |
|        1 | 1028 | `				}` |
|        1 | 1029 | `			}` |
|        5 | 1030 | `			PH7_GenSyntaxError(&(*pGen),pBad,` |
|        2 | 1031 | `				(pBad && (pBad->nType & PH7_TK_CCB)) ? 0 : "variable or \"{\" or \"$\"");` |
|        - | 1032 | `			}` |
|        3 | 1033 | `			return SXRET_OK;` |
|        - | 1034 | `		}` |
|        - | 1035 | `		/* Compile the expression holding the variable name */` |
|       16 | 1036 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       16 | 1037 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1038 | `			return SXERR_ABORT;` |
|       16 | 1039 | `		}else if( rc == SXERR_EMPTY ){` |
|        3 | 1040 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,0);` |
|        3 | 1041 | `			return SXRET_OK;` |
|        - | 1042 | `		}` |
|        7 | 1043 | `	}else{` |
|        - | 1044 | `		SyHashEntry *pEntry;` |
|        - | 1045 | `		SyString *pName;` |
| 21589831 | 1046 | `		char *zName = 0;` |
|        - | 1047 | `		/* Extract variable name */` |
| 21589831 | 1048 | `		pName = &pGen->pIn->sData;` |
|        - | 1049 | `		/* Advance the stream cursor */` |
| 21589831 | 1050 | `		pGen->pIn++;` |
| 21589831 | 1051 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
| 21589831 | 1052 | `		if( pEntry == 0 ){` |
|        - | 1053 | `			/* Duplicate name */` |
|  1293769 | 1054 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  1293769 | 1055 | `			if( zName == 0 ){` |
|      ! 0 | 1056 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1057 | `				return SXERR_ABORT;` |
|        - | 1058 | `			}` |
|        - | 1059 | `			/* Install in the hashtable */` |
|  1293769 | 1060 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   646887 | 1061 | `		}else{` |
|        - | 1062 | `			/* Name already available */` |
| 20296067 | 1063 | `			zName = (char *)pEntry->pUserData;` |
|        - | 1064 | `		}` |
| 21589831 | 1065 | `		p3 = (void *)zName;` |
|        - | 1066 | `	}` |
| 21589843 | 1067 | `	iP1 = 0;` |
| 21589843 | 1068 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
| 17038123 | 1069 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|        - | 1070 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
| 11606745 | 1071 | `			iP1 = 1;` |
|  5803370 | 1072 | `		}` |
|  8519059 | 1073 | `	}` |
|        - | 1074 | ``	/* iP2 marks a QUIET read: `isset($x)` / `empty($x)` inspect a variable`` |
|        - | 1075 | `	 * without reading it, so an undefined one must not warn (php stays silent` |
|        - | 1076 | `	 * for both). Every other read of a missing variable warns — see OP_LOAD.` |
|        - | 1077 | `	 * The two flags are cleared before recursing into a subscript's index` |
|        - | 1078 | ``	 * expression, so `isset($a[$i])` still warns for an undefined $i, as php`` |
|        - | 1079 | ``	 * does. Contexts that VIVIFY (assignment targets, `??`, appends) already`` |
|        - | 1080 | `	 * emit iP1 = 0 and never reach the warning. */` |
| 21589843 | 1081 | `	if( iCompileFlag & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_QUIET_VAR) ){` |
|   384477 | 1082 | `		iP2 = 1;` |
| 21397607 | 1083 | `	}else if( iCompileFlag & EXPR_FLAG_RMW_LOAD ){` |
|        - | 1084 | ``		/* Warn-then-create: the read half of `$x++` / `$x .= ...` still needs a`` |
|        - | 1085 | `		 * writable slot, so it cannot use the read-only load above. */` |
|   667495 | 1086 | `		iP2 = 2;` |
|   333745 | 1087 | `	}` |
|        - | 1088 | `	/* Emit the load instruction */` |
| 21589843 | 1089 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,iP2,p3,0);` |
| 21589855 | 1090 | `	while( iVv > 0 ){` |
|       13 | 1091 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,iP2,0,0);` |
|       13 | 1092 | `		iVv--;` |
|        1 | 1093 | `	}` |
|        - | 1094 | `	/* Node successfully compiled */` |
| 21589843 | 1095 | `	return SXRET_OK;` |
| 10794926 | 1096 | `}` |
|        - | 1097 | `/*` |
|        - | 1098 | ` * Load a literal.` |
|        - | 1099 | ` */` |
| 14084380 | 1100 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|        5 | 1101 | `{` |
| 14084385 | 1102 | `	SyToken *pToken = pGen->pIn;` |
|        - | 1103 | `	ph7_value *pObj;` |
|        - | 1104 | `	SyString *pStr;` |
|        - | 1105 | `	sxu32 nIdx;` |
|        - | 1106 | `	/* Extract token value */` |
| 14084385 | 1107 | `	pStr = &pToken->sData;` |
|        - | 1108 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first. A reserved` |
|        - | 1109 | `	 * word used as a member NAME (Enum::Null, C::Array, $o->list()) is a plain` |
|        - | 1110 | `	 * identifier — the parser flagged its token so the whole value-folding chain is` |
|        - | 1111 | `	 * skipped and it falls through to the ordinary string-literal emit below. */` |
| 14084385 | 1112 | `	if( pToken->nType & PH7_TK_MEMBER_NAME ){` |
|        - | 1113 | `		/* fall through to the plain-string literal path */` |
| 11212072 | 1114 | `	}else if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  1685849 | 1115 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|        - | 1116 | `			/* NULL constant are always indexed at 0 */` |
|  1156025 | 1117 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|  1156025 | 1118 | `			return SXRET_OK;` |
|   529829 | 1119 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|        - | 1120 | `			/* TRUE constant are always indexed at 1 */` |
|   357915 | 1121 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|   357915 | 1122 | `			return SXRET_OK;` |
|        5 | 1123 | `		}` |
|  7532470 | 1124 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|  1585196 | 1125 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|        - | 1126 | `			/* FALSE constant are always indexed at 2 */` |
|   795277 | 1127 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   795277 | 1128 | `			return SXRET_OK;` |
|  6005177 | 1129 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   293068 | 1130 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|        - | 1131 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|     3889 | 1132 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     3889 | 1133 | `			if( pObj == 0 ){` |
|      ! 0 | 1134 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1135 | `				return SXERR_ABORT;` |
|        - | 1136 | `			}` |
|     3889 | 1137 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|        - | 1138 | `			/* Emit the load constant instruction */` |
|     3889 | 1139 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     3889 | 1140 | `			return SXRET_OK;` |
|  5999346 | 1141 | `	}else if( (pStr->nByte == sizeof("__FILE__") - 1 &&` |
|   513756 | 1142 | `		SyMemcmp(pStr->zString,"__FILE__",sizeof("__FILE__")-1) == 0) \|\|` |
|  6075367 | 1143 | `		(pStr->nByte == sizeof("__DIR__") - 1 &&` |
|   449134 | 1144 | `		SyMemcmp(pStr->zString,"__DIR__",sizeof("__DIR__")-1) == 0) ){` |
|        - | 1145 | `			/* __FILE__ / __DIR__ are magic constants resolved at COMPILE time to the` |
|        - | 1146 | `			 * file being compiled (where the token is written), NOT the runtime` |
|        - | 1147 | `			 * execution file. A function defined in a.php reporting __FILE__ must say` |
|        - | 1148 | `			 * a.php even when called from b.php — php semantics, and what Composer's` |
|        - | 1149 | ``			 * autoloader (loadClassLoader's `require __DIR__ . '/ClassLoader.php'`)`` |
|        - | 1150 | `			 * relies on. The runtime-constant path returned the caller's file. */` |
|     3985 | 1151 | `			int bDir = (pStr->zString[2] == 'D'); /* __DIR__ vs __FILE__ */` |
|     3985 | 1152 | `			SyString *pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     3985 | 1153 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     3985 | 1154 | `			if( pObj == 0 ){` |
|      ! 0 | 1155 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1156 | `				return SXERR_ABORT;` |
|        - | 1157 | `			}` |
|     3985 | 1158 | `			if( pFile && pFile->nByte > 0 ){` |
|      109 | 1159 | `				if( bDir ){` |
|        - | 1160 | `					const char *zDir;` |
|        - | 1161 | `					int nLen;` |
|        - | 1162 | `					SyString sDir;` |
|       56 | 1163 | `					zDir = PH7_ExtractDirName(pFile->zString,(int)pFile->nByte,&nLen);` |
|       56 | 1164 | `					SyStringInitFromBuf(&sDir,zDir,nLen);` |
|       56 | 1165 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&sDir);` |
|       30 | 1166 | `				}else{` |
|       57 | 1167 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,pFile);` |
|        - | 1168 | `				}` |
|       57 | 1169 | `			}else{` |
|        - | 1170 | `				SyString sMem;` |
|     3881 | 1171 | `				SyStringInitFromBuf(&sMem,":MEMORY:",sizeof(":MEMORY:")-1);` |
|     3881 | 1172 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sMem);` |
|        - | 1173 | `			}` |
|     3985 | 1174 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     3985 | 1175 | `			return SXRET_OK;` |
|  5971281 | 1176 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   241004 | 1177 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|        - | 1178 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|        8 | 1179 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        8 | 1180 | `			if( pObj == 0 ){` |
|      ! 0 | 1181 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1182 | `				return SXERR_ABORT;` |
|        - | 1183 | `			}` |
|        8 | 1184 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        - | 1185 | `				SyString sNs;` |
|        8 | 1186 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        8 | 1187 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|        5 | 1188 | `			}else{` |
|      ! 0 | 1189 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|        - | 1190 | `			}` |
|        8 | 1191 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        8 | 1192 | `			return SXRET_OK;` |
|  5983145 | 1193 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   428598 | 1194 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  6014600 | 1195 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   327678 | 1196 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|       11 | 1197 | `			GenBlock *pBlock = pGen->pCurrent;` |
|        - | 1198 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|       21 | 1199 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|        - | 1200 | `				/* Point to the upper block */` |
|       11 | 1201 | `				pBlock = pBlock->pParent;` |
|        1 | 1202 | `			}` |
|       11 | 1203 | `			if( pBlock == 0 ){` |
|        - | 1204 | `				/* Called in the global scope,load NULL */` |
|        5 | 1205 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|        3 | 1206 | `			}else{` |
|        - | 1207 | `				/* Extract the target function/method */` |
|        7 | 1208 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        7 | 1209 | `				int bMethod = (pStr->zString[2] == 'M'); /* __METHOD__ vs __FUNCTION__ */` |
|        7 | 1210 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        7 | 1211 | `				if( pObj == 0 ){` |
|      ! 0 | 1212 | `					PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1213 | `					return SXERR_ABORT;` |
|        - | 1214 | `				}` |
|        - | 1215 | `				/*` |
|        - | 1216 | `				 * __METHOD__ is the QUALIFIED name: "C::m" inside a method, and the plain` |
|        - | 1217 | `				 * function name inside a plain function (php does not answer "" there —` |
|        - | 1218 | `				 * PH7 loaded NULL, so __METHOD__ was empty in every free function and` |
|        - | 1219 | `				 * unqualified in every method).` |
|        - | 1220 | `				 */` |
|        8 | 1221 | `				if( bMethod && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|        3 | 1222 | `					SyString *pCls = &((ph7_class *)pFunc->pUserData)->sName;` |
|        - | 1223 | `					SyBlob sQual;` |
|        - | 1224 | `					SyString sOut;` |
|        3 | 1225 | `					SyBlobInit(&sQual,&pGen->pVm->sAllocator);` |
|        3 | 1226 | `					SyBlobFormat(&sQual,"%z::%z",pCls,&pFunc->sName);` |
|        3 | 1227 | `					SyStringInitFromBuf(&sOut,SyBlobData(&sQual),SyBlobLength(&sQual));` |
|        3 | 1228 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&sOut);` |
|        3 | 1229 | `					SyBlobRelease(&sQual);` |
|        2 | 1230 | `				}else{` |
|        5 | 1231 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|        - | 1232 | `				}` |
|        - | 1233 | `				/* Emit the load constant instruction */` |
|        7 | 1234 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - | 1235 | `			}` |
|       11 | 1236 | `			return SXRET_OK;` |
|        - | 1237 | `	}` |
|        - | 1238 | `	/* Query literal table */` |
| 11767303 | 1239 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|        - | 1240 | `		ph7_value *pLitObj;` |
|        - | 1241 | `		/* Unknown literal,install it in the literal table */` |
|  2425495 | 1242 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  2425495 | 1243 | `		if( pLitObj == 0 ){` |
|      ! 0 | 1244 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 | 1245 | `			return SXERR_ABORT;` |
|        - | 1246 | `		}` |
|  2425495 | 1247 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  2425495 | 1248 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  1212745 | 1249 | `	}` |
|        - | 1250 | `	/* Emit the load constant instruction */` |
| 11767303 | 1251 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
| 11767303 | 1252 | `	return SXRET_OK;` |
|  7042195 | 1253 | `}` |
|        - | 1254 | `/*` |
|        - | 1255 | ` * Resolve a namespace path or simply load a literal.` |
|        - | 1256 | ` * If the token stream contains namespace separators (backslashes),` |
|        - | 1257 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|        - | 1258 | ` * Otherwise, load the simple literal directly.` |
|        - | 1259 | ` */` |
| 14088346 | 1260 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|        5 | 1261 | `{` |
|        - | 1262 | `	sxi32 rc;` |
| 14088351 | 1263 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 1264 | `		return SXRET_OK;` |
|        - | 1265 | `	}` |
|        - | 1266 | `	/* Check if this is a multi-token namespace path */` |
| 14088351 | 1267 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|        - | 1268 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|     3971 | 1269 | `		SyBlob *pWorker = &pGen->sWorker;` |
|     3971 | 1270 | `		int isAbsolute = 0;` |
|     3971 | 1271 | `		SyBlobReset(pWorker);` |
|        - | 1272 | `		/* Check for leading backslash (absolute path) */` |
|     3971 | 1273 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|     3949 | 1274 | `			isAbsolute = 1;` |
|     3949 | 1275 | `			pGen->pIn++; /* Skip leading backslash */` |
|     1972 | 1276 | `		}` |
|        - | 1277 | `		/* Collect the raw path (no prefix yet) so its FIRST segment can be resolved` |
|        - | 1278 | ``		 * against use-imports below — php resolves `A\B\C` by mapping the leading`` |
|        - | 1279 | ``		 * `A` through the imports (`use X\A;` makes it `X\A\B\C`), and only prepends`` |
|        - | 1280 | ``		 * the current namespace when `A` matches no import. Blindly prefixing the`` |
|        - | 1281 | ``		 * namespace here produced e.g. `Ns\Ev\Bus` for `use ...\Event as Ev; Ev\Bus`. */`` |
|        - | 1282 | `		{` |
|        - | 1283 | `			SyBlob sRaw;` |
|     3971 | 1284 | `			SyBlobInit(&sRaw,&pGen->pVm->sAllocator);` |
|     4135 | 1285 | `			while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|     4135 | 1286 | `				if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       86 | 1287 | `					SyBlobAppend(&sRaw,"\\",1);` |
|       45 | 1288 | `				}else{` |
|     4053 | 1289 | `					SyBlobAppend(&sRaw,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        - | 1290 | `				}` |
|     4135 | 1291 | `				if( pGen->pIn == &pGen->pEnd[-1] ){` |
|     3971 | 1292 | `					pGen->pIn++;` |
|     3971 | 1293 | `					break;` |
|        - | 1294 | `				}` |
|      168 | 1295 | `				pGen->pIn++;` |
|        4 | 1296 | `			}` |
|     3971 | 1297 | `			if( isAbsolute ){` |
|     3949 | 1298 | `				SyBlobAppend(pWorker,SyBlobData(&sRaw),SyBlobLength(&sRaw)); /* FQN as written */` |
|     1977 | 1299 | `			}else{` |
|       24 | 1300 | `				const char *zRaw = (const char *)SyBlobData(&sRaw);` |
|       24 | 1301 | `				sxu32 nRaw = SyBlobLength(&sRaw);` |
|       24 | 1302 | `				sxu32 nFirst = 0;` |
|        - | 1303 | `				SyHashEntry *pNsImp;` |
|      108 | 1304 | `				while( nFirst < nRaw && zRaw[nFirst] != '\\' ){ nFirst++; }` |
|       24 | 1305 | `				pNsImp = SyHashGet(&pGen->hUseImports,(const void *)zRaw,nFirst);` |
|       24 | 1306 | `				if( pNsImp ){` |
|        - | 1307 | `					/* Leading segment is an imported alias: substitute its FQN. */` |
|       21 | 1308 | `					const char *zFQN = (const char *)pNsImp->pUserData;` |
|       21 | 1309 | `					SyBlobAppend(pWorker,zFQN,SyStrlen(zFQN));` |
|       21 | 1310 | `					SyBlobAppend(pWorker,zRaw + nFirst,nRaw - nFirst);` |
|       13 | 1311 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        3 | 1312 | `					SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        3 | 1313 | `					SyBlobAppend(pWorker,"\\",1);` |
|        3 | 1314 | `					SyBlobAppend(pWorker,zRaw,nRaw);` |
|        2 | 1315 | `				}else{` |
|      ! 0 | 1316 | `					SyBlobAppend(pWorker,zRaw,nRaw); /* global scope, no import */` |
|        - | 1317 | `				}` |
|        - | 1318 | `			}` |
|     3971 | 1319 | `			SyBlobRelease(&sRaw);` |
|        - | 1320 | `		}` |
|     3971 | 1321 | `		if( SyBlobLength(pWorker) > 0 ){` |
|        - | 1322 | `			ph7_value *pObj;` |
|        - | 1323 | `			SyString sPath;` |
|        - | 1324 | `			sxu32 nIdx;` |
|     3971 | 1325 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|        - | 1326 | `			/* Install in the literal table */` |
|     3971 | 1327 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|     3919 | 1328 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     3919 | 1329 | `				if( pObj == 0 ){` |
|      ! 0 | 1330 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 | 1331 | `					return SXERR_ABORT;` |
|        - | 1332 | `				}` |
|     3919 | 1333 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|     3919 | 1334 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     1957 | 1335 | `			}` |
|        - | 1336 | `			/* Emit the load constant instruction.` |
|        - | 1337 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|        - | 1338 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|     5954 | 1339 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|     1983 | 1340 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|     1983 | 1341 | `				nIdx,0,0);` |
|     3971 | 1342 | `			return SXRET_OK;` |
|        - | 1343 | `		}` |
|      ! 0 | 1344 | `	}` |
|        - | 1345 | `	/* Single-token literal: load directly */` |
| 14084385 | 1346 | `	rc = GenStateLoadLiteral(&(*pGen));` |
| 14084385 | 1347 | `	return rc;` |
|  7044178 | 1348 | `}` |
|        - | 1349 | `/*` |
|        - | 1350 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|        - | 1351 | ` */` |
|        - | 1352 | `/*` |
|        - | 1353 | `` * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of`` |
|        - | 1354 | `` * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument`` |
|        - | 1355 | ``  * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...` `` |
|        - | 1356 | ` * appeared outside a call argument list — a syntax error (PHP rejects it likewise).` |
|        - | 1357 | ` */` |
|      ! 0 | 1358 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|      ! 0 | 1359 | `{` |
|      ! 0 | 1360 | `	SXUNUSED(iCompileFlag);` |
|      ! 0 | 1361 | `	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|        - | 1362 | `		"Cannot use the first-class callable syntax '...' here");` |
|      ! 0 | 1363 | `	return SXERR_SYNTAX;` |
|      ! 0 | 1364 | `}` |
| 14088346 | 1365 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 1366 | `{` |
|        - | 1367 | `	sxi32 rc;` |
| 14088351 | 1368 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
| 14088351 | 1369 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1370 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 | 1371 | `		return rc;` |
|        - | 1372 | `	}` |
|        - | 1373 | `	/* Node successfully compiled */` |
| 14088351 | 1374 | `	return SXRET_OK;` |
|  7044178 | 1375 | `}` |
|        - | 1376 |  |
