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
|      634 |   36 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |   37 | `{` |
|      639 |   38 | `	ph7_vm_func *pAnnonFunc = 0; /* Annonymous function body */` |
|        - |   39 | `	char zName[512];         /* Unique lambda name */` |
|        - |   40 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|        - |   41 | `							  * one thread is allowed to compile the script.` |
|        - |   42 | `						      */` |
|        - |   43 | `	SyString sName;` |
|      639 |   44 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `$f = #[A] function…` trivia`` |
|        - |   45 | `	                              * is keyed to this ['static'] 'function' token */` |
|        - |   46 | `	sxu32 nKwLine;` |
|      639 |   47 | `	sxi32 iFlags = 0;` |
|        - |   48 | `	sxu32 nLen;` |
|        - |   49 | `	sxi32 rc;` |
|      317 |   50 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |   51 |  |
|      639 |   52 | `	nKwLine = pGen->pIn->nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|      634 |   53 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      639 |   54 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        - |   55 | `		/* Static closure: no $this auto-capture, bind refused */` |
|       29 |   56 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|       29 |   57 | `		pGen->pIn++; /* Jump the 'static' keyword */` |
|       13 |   58 | `	}` |
|      639 |   59 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|      639 |   60 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|      ! 0 |   61 | `		pGen->pIn++;` |
|      ! 0 |   62 | `	}` |
|        - |   63 | `	/* Generate a unique name */` |
|      639 |   64 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|        - |   65 | `	/* Make sure the generated name is unique */` |
|      639 |   66 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|      ! 0 |   67 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|      ! 0 |   68 | `	}` |
|      639 |   69 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|        - |   70 | `	/* Compile the lambda body */` |
|      639 |   71 | `	rc = GenStateCompileFunc(&(*pGen),&sName,iFlags,TRUE,&pAnnonFunc);` |
|      639 |   72 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |   73 | `		return SXERR_ABORT;` |
|        - |   74 | `	}` |
|      639 |   75 | `	if( pAnnonFunc ){` |
|      639 |   76 | `		pAnnonFunc->nLine = nKwLine;` |
|        - |   77 | ``		/* Expression-position attributes (`$f = #[A] function () {}`): the trivia`` |
|        - |   78 | `		 * sidecar keys them to the closure's first keyword token. */` |
|      639 |   79 | `		if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnnonFunc->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |   80 | `			return SXERR_ABORT;` |
|        - |   81 | `		}` |
|      317 |   82 | `	}` |
|        - |   83 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|        - |   84 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|        - |   85 | `	 * the handler wraps either in a Closure instance. */` |
|      639 |   86 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|        - |   87 | `	/* Node successfully compiled */` |
|      639 |   88 | `	return SXRET_OK;` |
|      322 |   89 | `}` |
|        - |   90 | `/*` |
|        - |   91 | ` * Add a free variable to the arrow function's closure environment, unless` |
|        - |   92 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|        - |   93 | ` * enclosing arrow level, or has already been captured.` |
|        - |   94 | ` */` |
|      274 |   95 | `static sxi32 GenStateArrowAddCapture(` |
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
|      279 |  107 | `	if( nByte == 0 ){` |
|      ! 0 |  108 | `		return SXRET_OK;` |
|        - |  109 | `	}` |
|      274 |  110 | `	if( nByte == sizeof("this")-1` |
|      151 |  111 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|        9 |  112 | `		return SXRET_OK;` |
|        - |  113 | `	}` |
|      339 |  114 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|      256 |  115 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|      249 |  116 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|      193 |  117 | `			return SXRET_OK;` |
|        - |  118 | `		}` |
|       36 |  119 | `	}` |
|       80 |  120 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       80 |  121 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|      108 |  122 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|       30 |  123 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|       29 |  124 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|        3 |  125 | `			return SXRET_OK;` |
|        - |  126 | `		}` |
|       15 |  127 | `	}` |
|       78 |  128 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|       78 |  129 | `	if( zDup == 0 ){` |
|      ! 0 |  130 | `		return SXERR_ABORT;` |
|        - |  131 | `	}` |
|       78 |  132 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|       78 |  133 | `	sEnv.iFlags = 0;` |
|       78 |  134 | `	sEnv.nIdx = SXU32_HIGH;` |
|       78 |  135 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|       78 |  136 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|       78 |  137 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|       78 |  138 | `	return SXRET_OK;` |
|      142 |  139 | `}` |
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
|      572 |  208 | `static sxi32 GenStateArrowCaptureScan(` |
|        - |  209 | `	ph7_gen_state *pGen,` |
|        - |  210 | `	ph7_vm_func *pFunc,` |
|        - |  211 | `	SyToken *pStart,` |
|        - |  212 | `	SyToken *pEnd,` |
|        - |  213 | `	SyString *aShadow,` |
|        - |  214 | `	sxu32 nShadow)` |
|        5 |  215 | `{` |
|      577 |  216 | `	SyToken *pScan = pStart;` |
|        - |  217 | `	sxi32 rc;` |
|     3875 |  218 | `	while( pScan < pEnd ){` |
|     3303 |  219 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
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
|     3179 |  230 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
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
|     3155 |  382 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|     2905 |  383 | `			pScan++;` |
|     2905 |  384 | `			continue;` |
|        - |  385 | `		}` |
|        - |  386 | `		{` |
|        - |  387 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|      255 |  388 | `			SyToken *pDollar = pScan;` |
|      375 |  389 | `			while( &pDollar[1] < pEnd` |
|      255 |  390 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|      ! 0 |  391 | `				pDollar++;` |
|      ! 0 |  392 | `			}` |
|      255 |  393 | `			if( &pDollar[1] >= pEnd ){` |
|      ! 0 |  394 | `				break;` |
|        - |  395 | `			}` |
|      255 |  396 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  397 | `				pScan = pDollar + 1;` |
|      ! 0 |  398 | `				continue;` |
|        - |  399 | `			}` |
|      380 |  400 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|      250 |  401 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|      125 |  402 | `				aShadow,nShadow);` |
|      255 |  403 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  404 | `				return SXERR_ABORT;` |
|        - |  405 | `			}` |
|      255 |  406 | `			pScan = pDollar + 2;` |
|        - |  407 | `		}` |
|        5 |  408 | `	}` |
|      577 |  409 | `	return SXRET_OK;` |
|      291 |  410 | `}` |
|        - |  411 | `/*` |
|        - |  412 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|        - |  413 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|        - |  414 | ` * variables by value. The body is a single expression that acts as an` |
|        - |  415 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|        - |  416 | ` * $this is also made available.` |
|        - |  417 | ` */` |
|      548 |  418 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
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
|      553 |  435 | `	sxi32 iFlags = 0;` |
|      553 |  436 | `	int bStatic = 0;` |
|        - |  437 | `	sxi32 rc;` |
|        - |  438 | `	sxu32 n;` |
|      274 |  439 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |  440 |  |
|      553 |  441 | `	nLine = pGen->pIn->nLine;` |
|        - |  442 | ``	/* Attribute-sidecar key: `#[A] [static] fn` trivia is keyed to this token */`` |
|      553 |  443 | `	pTokKw = pGen->pIn;` |
|        - |  444 | `	/* Optional 'static' prefix */` |
|      548 |  445 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      553 |  446 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        9 |  447 | `		bStatic = 1;` |
|        9 |  448 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|        9 |  449 | `		pGen->pIn++;` |
|        4 |  450 | `	}` |
|        - |  451 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|      548 |  452 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      553 |  453 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|      ! 0 |  454 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  455 | `			"Arrow function: expected 'fn' keyword");` |
|      ! 0 |  456 | `		return SXERR_SYNTAX;` |
|        - |  457 | `	}` |
|      553 |  458 | `	pGen->pIn++; /* Jump 'fn' */` |
|        - |  459 | `	/* Optional '&' — return by reference */` |
|      553 |  460 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|      ! 0 |  461 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|      ! 0 |  462 | `		pGen->pIn++;` |
|      ! 0 |  463 | `	}` |
|        - |  464 | `	/* Expect '(' */` |
|      553 |  465 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
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
|      551 |  476 | `	pGen->pIn++; /* Jump '(' */` |
|        - |  477 | `	/* Delimit the parameter list */` |
|      551 |  478 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|      551 |  479 | `	if( pSigEnd >= pGen->pEnd ){` |
|        3 |  480 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - |  481 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|        3 |  482 | `		return SXERR_SYNTAX;` |
|        - |  483 | `	}` |
|        - |  484 | `	/* Allocate the function state */` |
|      549 |  485 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|      549 |  486 | `	if( pFunc == 0 ){` |
|      ! 0 |  487 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  488 | `			"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  489 | `		return SXERR_ABORT;` |
|        - |  490 | `	}` |
|        - |  491 | `	/* Generate a unique lambda name */` |
|      549 |  492 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|      551 |  493 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|        3 |  494 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|        1 |  495 | `	}` |
|      549 |  496 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|      549 |  497 | `	if( zDup == 0 ){` |
|      ! 0 |  498 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  499 | `			"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  500 | `		return SXERR_ABORT;` |
|        - |  501 | `	}` |
|      549 |  502 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|        - |  503 | `	/* Reflection getStartLine(): line of the ['static'] 'fn' keyword */` |
|      549 |  504 | `	pFunc->nLine = nLine;` |
|        - |  505 | ``	/* Expression-position attributes (`$f = #[A] fn () => …`) */`` |
|      549 |  506 | `	if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pFunc->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  507 | `		return SXERR_ABORT;` |
|        - |  508 | `	}` |
|        - |  509 | `	/* Collect function arguments */` |
|      549 |  510 | `	if( pGen->pIn < pSigEnd ){` |
|      157 |  511 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|      157 |  512 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  513 | `			return SXERR_ABORT;` |
|        - |  514 | `		}` |
|       76 |  515 | `	}` |
|        - |  516 | `	/* Point past ')' and parse optional return type */` |
|      549 |  517 | `	pGen->pIn = &pSigEnd[1];` |
|      549 |  518 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|      549 |  519 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  520 | `		return SXERR_ABORT;` |
|      549 |  521 | `	}else if( rc == SXERR_SYNTAX ){` |
|      ! 0 |  522 | `		return SXERR_SYNTAX;` |
|        - |  523 | `	}` |
|        - |  524 | `	/* Expect '=>' */` |
|      549 |  525 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
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
|      547 |  536 | `	pGen->pIn++; /* Jump '=>' */` |
|      547 |  537 | `	pBodyStart = pGen->pIn;` |
|      547 |  538 | `	pBodyEnd = pGen->pEnd;` |
|        - |  539 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|        - |  540 | `	 * recursively collect free-variable references from the body. The scan` |
|        - |  541 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|        - |  542 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|      547 |  543 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|        - |  544 | `	{` |
|      547 |  545 | `		SyString *aShadow = 0;` |
|      547 |  546 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|      547 |  547 | `		if( nShadow > 0 ){` |
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
|      818 |  559 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|      271 |  560 | `			aShadow,nShadow);` |
|      547 |  561 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  562 | `			return SXERR_ABORT;` |
|        - |  563 | `		}` |
|        - |  564 | `	}` |
|        - |  565 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|        - |  566 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|        - |  567 | `	 * captured value is silently dropped when the enclosing scope has no` |
|        - |  568 | `	 * $this. */` |
|      547 |  569 | `	if( !bStatic ){` |
|        - |  570 | `		char *zThisDup;` |
|      539 |  571 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|      539 |  572 | `		if( zThisDup == 0 ){` |
|      ! 0 |  573 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  574 | `				"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  575 | `			return SXERR_ABORT;` |
|        - |  576 | `		}` |
|      539 |  577 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      539 |  578 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|      539 |  579 | `		sEnv.nIdx = SXU32_HIGH;` |
|      539 |  580 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      539 |  581 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|      539 |  582 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      267 |  583 | `	}` |
|        - |  584 | `	/* Arrow functions are always closures; the ARROW mark tells OP_LOAD_CLOSURE` |
|        - |  585 | `	 * these captures are implicit (auto-scanned) so an undefined one stays silent` |
|        - |  586 | `	 * at creation — php only warns when the body reads it. */` |
|      547 |  587 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE \| VM_FUNC_ARROW;` |
|        - |  588 | `	/* Compile the body expression as an implicit return */` |
|      818 |  589 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      271 |  590 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|      547 |  591 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  592 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  593 | `			"PH7 engine is running out-of-memory");` |
|      ! 0 |  594 | `		return SXERR_ABORT;` |
|        - |  595 | `	}` |
|      547 |  596 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      547 |  597 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|      547 |  598 | `	pSavedEnd = pGen->pEnd;` |
|      547 |  599 | `	pGen->pIn = pBodyStart;` |
|      547 |  600 | `	pGen->pEnd = pBodyEnd;` |
|      547 |  601 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      547 |  602 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  603 | `		return SXERR_ABORT;` |
|        - |  604 | `	}` |
|        - |  605 | `	/* The cursor stopped just past the body expression */` |
|      547 |  606 | `	pFunc->nEndLine = (pGen->pIn > pBodyStart) ? pGen->pIn[-1].nLine : nLine;` |
|        - |  607 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|        - |  608 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|        - |  609 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|        - |  610 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|      547 |  611 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      547 |  612 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|      547 |  613 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|      547 |  614 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      547 |  615 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - |  616 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|      547 |  617 | `	pGen->pIn = pBodyEnd;` |
|      547 |  618 | `	pGen->pEnd = pSavedEnd;` |
|        - |  619 | `	/* Emit the load-closure instruction */` |
|      547 |  620 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|      547 |  621 | `	return SXRET_OK;` |
|      279 |  622 | `}` |
|        - |  623 | `/*` |
|        - |  624 | ` * Compile a single arm's expression range into a freshly-allocated` |
|        - |  625 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|        - |  626 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|        - |  627 | ` * expression's value.` |
|        - |  628 | ` */` |
|      364 |  629 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|        - |  630 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|        2 |  631 | `{` |
|        - |  632 | `	SySet *pInstrContainer;` |
|        - |  633 | `	SyToken *pTmpIn,*pTmpEnd;` |
|        - |  634 | `	GenBlock *pArmBlock;` |
|        - |  635 | `	sxi32 rc;` |
|      366 |  636 | `	pTmpIn  = pGen->pIn;` |
|      366 |  637 | `	pTmpEnd = pGen->pEnd;` |
|      366 |  638 | `	pGen->pIn  = pStart;` |
|      366 |  639 | `	pGen->pEnd = pStop;` |
|      366 |  640 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      366 |  641 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|        - |  642 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|        - |  643 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|        - |  644 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|        - |  645 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|        - |  646 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|      548 |  647 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      182 |  648 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|      366 |  649 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  650 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 |  651 | `		pGen->pIn  = pTmpIn;` |
|      ! 0 |  652 | `		pGen->pEnd = pTmpEnd;` |
|      ! 0 |  653 | `		return SXERR_ABORT;` |
|        - |  654 | `	}` |
|      366 |  655 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      366 |  656 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      366 |  657 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|      366 |  658 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|      366 |  659 | `	GenStateLeaveBlock(&(*pGen),0);` |
|      366 |  660 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      366 |  661 | `	pGen->pIn  = pTmpIn;` |
|      366 |  662 | `	pGen->pEnd = pTmpEnd;` |
|      366 |  663 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  664 | `		return SXERR_ABORT;` |
|        - |  665 | `	}` |
|      366 |  666 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 |  667 | `		return SXERR_EMPTY;` |
|        - |  668 | `	}` |
|      366 |  669 | `	return SXRET_OK;` |
|      184 |  670 | `}` |
|        - |  671 | `/*` |
|        - |  672 | ` * Compile a PHP 8.0 match expression:` |
|        - |  673 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|        - |  674 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|        - |  675 | ` * Strict comparison (===) is used between the subject and each condition.` |
|        - |  676 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|        - |  677 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|        - |  678 | ` */` |
|        - |  679 | `/*` |
|        - |  680 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|        - |  681 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|        - |  682 | ` * caller can bail out of the current expression.` |
|        - |  683 | ` */` |
|        2 |  684 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|        1 |  685 | `{` |
|        - |  686 | `	va_list ap;` |
|        - |  687 | `	sxi32 rc;` |
|        - |  688 | `	SyBlob sMsg;` |
|        3 |  689 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|        3 |  690 | `	va_start(ap,zFmt);` |
|        3 |  691 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|        3 |  692 | `	va_end(ap);` |
|        3 |  693 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|        3 |  694 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|        3 |  695 | `	SyBlobRelease(&sMsg);` |
|        3 |  696 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  697 | `		return SXERR_ABORT;` |
|        - |  698 | `	}` |
|        3 |  699 | `	return SXERR_SYNTAX;` |
|        2 |  700 | `}` |
|        - |  701 | `/*` |
|        - |  702 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|        - |  703 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|        - |  704 | ` * Returns the stop token pointer (or pEnd if none found).` |
|        - |  705 | ` */` |
|      366 |  706 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|        3 |  707 | `{` |
|      369 |  708 | `	SyToken *pCur = pStart;` |
|      369 |  709 | `	int iNest = 0;` |
|      889 |  710 | `	while( pCur < pEnd ){` |
|      853 |  711 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       17 |  712 | `			iNest++;` |
|      845 |  713 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       17 |  714 | `			iNest--;` |
|      829 |  715 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|      332 |  716 | `			return pCur;` |
|        - |  717 | `		}` |
|      523 |  718 | `		pCur++;` |
|        3 |  719 | `	}` |
|       39 |  720 | `	return pEnd;` |
|      186 |  721 | `}` |
|       74 |  722 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        4 |  723 | `{` |
|        - |  724 | `	ph7_match *pMatch;` |
|        - |  725 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|       78 |  726 | `	int bHasDefault = 0;` |
|        - |  727 | `	sxu32 nLine;` |
|        - |  728 | `	sxi32 rc;` |
|       37 |  729 | `	SXUNUSED(iCompileFlag);` |
|       78 |  730 | `	nLine = pGen->pIn->nLine;` |
|       78 |  731 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|        - |  732 | `	/* Expect '(' */` |
|       78 |  733 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 |  734 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  735 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|      ! 0 |  736 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|        - |  737 | `	}` |
|       78 |  738 | `	pGen->pIn++; /* Jump '(' */` |
|       78 |  739 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|       78 |  740 | `	if( pSubjEnd >= pGen->pEnd ){` |
|      ! 0 |  741 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  742 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|        - |  743 | `	}` |
|       78 |  744 | `	if( pGen->pIn >= pSubjEnd ){` |
|      ! 0 |  745 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  746 | `			"syntax error, unexpected \")\", expecting match subject");` |
|        - |  747 | `	}` |
|        - |  748 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|       78 |  749 | `	pSavedEnd = pGen->pEnd;` |
|       78 |  750 | `	pGen->pEnd = pSubjEnd;` |
|       78 |  751 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       78 |  752 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  753 | `		return SXERR_ABORT;` |
|        - |  754 | `	}` |
|       78 |  755 | `	pGen->pEnd = pSavedEnd;` |
|       78 |  756 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|        - |  757 | `	/* Expect '{' */` |
|       78 |  758 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|      ! 0 |  759 | `		return GenStateMatchError(pGen,` |
|      ! 0 |  760 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|        - |  761 | `			"syntax error, expecting \"{\" after match subject");` |
|        - |  762 | `	}` |
|       78 |  763 | `	pGen->pIn++; /* Jump '{' */` |
|       78 |  764 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|       78 |  765 | `	if( pBodyEnd >= pGen->pEnd ){` |
|      ! 0 |  766 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  767 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|        - |  768 | `	}` |
|        - |  769 | `	/* Allocate ph7_match container */` |
|       78 |  770 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|       78 |  771 | `	if( pMatch == 0 ){` |
|      ! 0 |  772 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  773 | `			"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  774 | `		return SXERR_ABORT;` |
|        - |  775 | `	}` |
|       78 |  776 | `	SyZero(pMatch,sizeof(ph7_match));` |
|       78 |  777 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|        - |  778 | `	/* Iterate arms */` |
|      266 |  779 | `	while( pGen->pIn < pBodyEnd ){` |
|        - |  780 | `		ph7_match_arm sArm;` |
|        - |  781 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|      195 |  782 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|      195 |  783 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|      195 |  784 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|      195 |  785 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - |  786 | `		/* 'default' arm? */` |
|      192 |  787 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      110 |  788 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|       24 |  789 | `			if( bHasDefault ){` |
|        3 |  790 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|        - |  791 | `					"Match expressions may only contain one default arm");` |
|        4 |  792 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  793 | `			}` |
|       22 |  794 | `			sArm.bDefault = 1;` |
|       22 |  795 | `			bHasDefault = 1;` |
|       22 |  796 | `			pGen->pIn++;` |
|       22 |  797 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|      ! 0 |  798 | `				return GenStateMatchError(pGen,nArmLine,` |
|        - |  799 | `					"syntax error, expecting \"=>\" after 'default'");` |
|        - |  800 | `			}` |
|       22 |  801 | `			pGen->pIn++; /* Jump '=>' */` |
|       12 |  802 | `		}else{` |
|        - |  803 | `			/* Condition list: cond (',' cond)* '=>' */` |
|      173 |  804 | `			pCondStart = pGen->pIn;` |
|      173 |  805 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|        - |  806 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|      181 |  807 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|        - |  808 | `				SySet sCondBc;` |
|        9 |  809 | `				if( pCondStart >= pArrow ){` |
|      ! 0 |  810 | `					return GenStateMatchError(pGen,nArmLine,` |
|        - |  811 | `						"syntax error, empty match condition expression");` |
|        - |  812 | `				}` |
|        9 |  813 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        9 |  814 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|        9 |  815 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  816 | `					return SXERR_ABORT;` |
|        - |  817 | `				}` |
|        9 |  818 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|        9 |  819 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|        9 |  820 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|        - |  821 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|        1 |  822 | `			}` |
|      173 |  823 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|        3 |  824 | `				return GenStateMatchError(pGen,nArmLine,` |
|        - |  825 | `					"syntax error, expecting \"=>\" in match arm");` |
|        - |  826 | `			}` |
|      170 |  827 | `			if( pCondStart >= pArrow ){` |
|      ! 0 |  828 | `				return GenStateMatchError(pGen,nArmLine,` |
|        - |  829 | `					"syntax error, empty match condition expression");` |
|        - |  830 | `			}` |
|        - |  831 | `			{` |
|        - |  832 | `				SySet sCondBc;` |
|      170 |  833 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      170 |  834 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|      170 |  835 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  836 | `					return SXERR_ABORT;` |
|        - |  837 | `				}` |
|      170 |  838 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|        - |  839 | `			}` |
|      170 |  840 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|        - |  841 | `		}` |
|        - |  842 | `		/* Compile result expression: up to top-level ',' or body end */` |
|      190 |  843 | `		pResStart = pGen->pIn;` |
|      190 |  844 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|      190 |  845 | `		if( pResStart >= pResEnd ){` |
|      ! 0 |  846 | `			return GenStateMatchError(pGen,nArmLine,` |
|        - |  847 | `				"syntax error, expected expression after \"=>\"");` |
|        - |  848 | `		}` |
|      190 |  849 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|      190 |  850 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  851 | `			return SXERR_ABORT;` |
|        - |  852 | `		}` |
|      190 |  853 | `		pGen->pIn = pResEnd;` |
|      190 |  854 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|      156 |  855 | `			pGen->pIn++; /* Skip trailing ',' */` |
|       77 |  856 | `		}` |
|      190 |  857 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|        2 |  858 | `	}` |
|       73 |  859 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|       73 |  860 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|       73 |  861 | `	return SXRET_OK;` |
|       41 |  862 | `}` |
|        - |  863 | `/*` |
|        - |  864 | ` * Compile a backtick quoted string.` |
|        - |  865 | ` */` |
|        2 |  866 | `PH7_PRIVATE sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        1 |  867 | `{` |
|        1 |  868 | `	SXUNUSED(iCompileFlag);` |
|        - |  869 | `	/*` |
|        - |  870 | ``	 * The backtick (`) operator was DEPRECATED by php (shell_exec() is the replacement).`` |
|        - |  871 | `	 * PHL targets php's *non-deprecated* surface, so it is a hard parse error — never` |
|        - |  872 | `	 * compiled to a shell_exec() call.` |
|        - |  873 | `	 */` |
|        3 |  874 | `	PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|        - |  875 | ``		"syntax error, the backtick (`) operator was removed, use shell_exec() instead");`` |
|        3 |  876 | `	return SXERR_ABORT;` |
|        1 |  877 | `}` |
|        - |  878 | `/*` |
|        - |  879 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|        - |  880 | ` * construct.` |
|        - |  881 | ` */` |
|       62 |  882 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  883 | `{` |
|        - |  884 | `	SyString *pName;` |
|        - |  885 | `	sxu32 nKeyID;` |
|        - |  886 | `	sxi32 rc;` |
|        - |  887 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|       67 |  888 | `	pName = &pGen->pIn->sData;` |
|       67 |  889 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       67 |  890 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|       67 |  891 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|        6 |  892 | `		SyToken *pTmp,*pNext = 0;` |
|        - |  893 | ``		/* A STATEMENT `echo` never reaches here — it dispatches through the statement`` |
|        - |  894 | `		 * table. Arriving in expression position means source like` |
|        - |  895 | ``		 * `fopen('f','r') or echo "IO error";`, which was a Symisc extension and is a`` |
|        - |  896 | `		 * php parse error (§10: a PH7-ism that changes the meaning of valid source is a` |
|        - |  897 | ``		 * bug). The one legitimate expression-echo is the token a `<?= ... ?>` short tag`` |
|        - |  898 | `		 * synthesizes, which raises nExprEchoOk around its own compile. */` |
|        6 |  899 | `		if( pGen->nExprEchoOk < 1 ){` |
|        3 |  900 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn - 1,0);` |
|        3 |  901 | `			return SXERR_ABORT;` |
|        - |  902 | `		}` |
|        - |  903 | `		/* Compile arguments one after one */` |
|        3 |  904 | `		pTmp = pGen->pEnd;` |
|        3 |  905 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|        5 |  906 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|        3 |  907 | `			if( pGen->pIn < pNext ){` |
|        3 |  908 | `				pGen->pEnd = pNext;` |
|        3 |  909 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|        3 |  910 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  911 | `					return SXERR_ABORT;` |
|        - |  912 | `				}` |
|        3 |  913 | `				if( rc != SXERR_EMPTY ){` |
|        - |  914 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|        - |  915 | `					 * without the overhead of a function call.` |
|        - |  916 | `					 * This is a very powerful optimization that improve` |
|        - |  917 | `					 * performance greatly.` |
|        - |  918 | `					 */` |
|        3 |  919 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|        1 |  920 | `				}` |
|        1 |  921 | `			}` |
|        - |  922 | `			/* Jump trailing commas */` |
|        3 |  923 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|      ! 0 |  924 | `				pNext++;` |
|      ! 0 |  925 | `			}` |
|        3 |  926 | `			pGen->pIn = pNext;` |
|        1 |  927 | `		}` |
|        - |  928 | `		/* Restore token stream */` |
|        3 |  929 | `		pGen->pEnd = pTmp;` |
|        2 |  930 | `	}else{` |
|       63 |  931 | `		sxi32 nArg = 0;` |
|       63 |  932 | `		sxu32 nIdx = 0;` |
|       63 |  933 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|       63 |  934 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  935 | `			return SXERR_ABORT;` |
|       63 |  936 | `		}else if(rc != SXERR_EMPTY ){` |
|       63 |  937 | `			nArg = 1;` |
|       29 |  938 | `		}` |
|       63 |  939 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|        - |  940 | `			ph7_value *pObj;` |
|        - |  941 | `			/* Emit the call instruction */` |
|       35 |  942 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       35 |  943 | `			if( pObj == 0 ){` |
|      ! 0 |  944 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  945 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |  946 | `				return SXERR_ABORT;` |
|        - |  947 | `			}` |
|       35 |  948 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|        - |  949 | `			/* Install in the literal table */` |
|       35 |  950 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       15 |  951 | `		}` |
|        - |  952 | `		/* Emit the call instruction */` |
|       63 |  953 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       63 |  954 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        - |  955 | `	}` |
|        - |  956 | `	/* Node successfully compiled */` |
|       65 |  957 | `	return SXRET_OK;` |
|       36 |  958 | `}` |
|        - |  959 | `/*` |
|        - |  960 | ` * Compile a node holding a variable declaration.` |
|        - |  961 | ` * According to the PHP language reference` |
|        - |  962 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|        - |  963 | ` *  The variable name is case-sensitive.` |
|        - |  964 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|        - |  965 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|        - |  966 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|        - |  967 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|        - |  968 | ` *  Note: $this is a special variable that can't be assigned.` |
|        - |  969 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|        - |  970 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|        - |  971 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|        - |  972 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|        - |  973 | ` *  the chapter on Expressions.` |
|        - |  974 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|        - |  975 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|        - |  976 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|        - |  977 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|        - |  978 | ` *  is being assigned (the source variable).` |
|        - |  979 | ` */` |
| 21556364 |  980 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  981 | `{` |
| 21556369 |  982 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|        - |  983 | `	sxi32 iVv;` |
|        - |  984 | `	sxi32 iP1;` |
| 21556369 |  985 | `	sxi32 iP2 = 0; /* 1 = quiet read (isset/empty): a missing variable must not warn */` |
|        - |  986 | `	void *p3;` |
|        - |  987 | `	sxi32 rc;` |
| 21556369 |  988 | `	iVv = -1; /* Variable variable counter */` |
| 43112745 |  989 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
| 21556381 |  990 | `		pGen->pIn++;` |
| 21556381 |  991 | `		iVv++;` |
|        5 |  992 | `	}` |
| 21556369 |  993 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|        - |  994 | `		/* Invalid variable name */` |
|      ! 0 |  995 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable or \"{\" or \"$\"");` |
|      ! 0 |  996 | `		if( rc == SXERR_ABORT ){` |
|        - |  997 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  998 | `			return SXERR_ABORT;` |
|        - |  999 | `		}` |
|      ! 0 | 1000 | `		return SXRET_OK;` |
|        - | 1001 | `	}` |
| 21556369 | 1002 | `	p3  = 0;` |
| 21556369 | 1003 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|        - | 1004 | `		/* Dynamic variable creation */` |
|       19 | 1005 | `		pGen->pIn++;  /* Jump the open curly */` |
|       19 | 1006 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|       19 | 1007 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 1008 | `			/* Empty expression */` |
|        - | 1009 | `			{` |
|        - | 1010 | `			/* php names the offending token and, for an empty "${}", stops there:` |
|        - | 1011 | `			 * the "expecting" tail only appears when something could still follow. */` |
|        - | 1012 | ``			/* `${}`: pEnd was stepped back past the trailing '}', so the token php`` |
|        - | 1013 | `			 * names sits AT pEnd. Reach for it before deciding the tail -- php stops` |
|        - | 1014 | `			 * at "unexpected token \"}\"" with no "expecting" clause, which the` |
|        - | 1015 | `			 * NULL-token path could not express because it never saw the '}'. */` |
|        3 | 1016 | `			SyToken *pBad = pGen->pIn < pGen->pEnd ? pGen->pIn : 0;` |
|        3 | 1017 | `			if( pBad == 0 && pGen->pTokenSet ){` |
|        3 | 1018 | `				SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        3 | 1019 | `				SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|        3 | 1020 | `				if( pGen->pEnd >= pBase && pGen->pEnd < pStreamEnd ){` |
|        3 | 1021 | `					pBad = pGen->pEnd;` |
|        1 | 1022 | `				}` |
|        1 | 1023 | `			}` |
|        5 | 1024 | `			PH7_GenSyntaxError(&(*pGen),pBad,` |
|        2 | 1025 | `				(pBad && (pBad->nType & PH7_TK_CCB)) ? 0 : "variable or \"{\" or \"$\"");` |
|        - | 1026 | `			}` |
|        3 | 1027 | `			return SXRET_OK;` |
|        - | 1028 | `		}` |
|        - | 1029 | `		/* Compile the expression holding the variable name */` |
|       16 | 1030 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       16 | 1031 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1032 | `			return SXERR_ABORT;` |
|       16 | 1033 | `		}else if( rc == SXERR_EMPTY ){` |
|        3 | 1034 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,0);` |
|        3 | 1035 | `			return SXRET_OK;` |
|        - | 1036 | `		}` |
|        7 | 1037 | `	}else{` |
|        - | 1038 | `		SyHashEntry *pEntry;` |
|        - | 1039 | `		SyString *pName;` |
| 21556353 | 1040 | `		char *zName = 0;` |
|        - | 1041 | `		/* Extract variable name */` |
| 21556353 | 1042 | `		pName = &pGen->pIn->sData;` |
|        - | 1043 | `		/* Advance the stream cursor */` |
| 21556353 | 1044 | `		pGen->pIn++;` |
| 21556353 | 1045 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
| 21556353 | 1046 | `		if( pEntry == 0 ){` |
|        - | 1047 | `			/* Duplicate name */` |
|  1291731 | 1048 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  1291731 | 1049 | `			if( zName == 0 ){` |
|      ! 0 | 1050 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1051 | `				return SXERR_ABORT;` |
|        - | 1052 | `			}` |
|        - | 1053 | `			/* Install in the hashtable */` |
|  1291731 | 1054 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   645868 | 1055 | `		}else{` |
|        - | 1056 | `			/* Name already available */` |
| 20264627 | 1057 | `			zName = (char *)pEntry->pUserData;` |
|        - | 1058 | `		}` |
| 21556353 | 1059 | `		p3 = (void *)zName;` |
|        - | 1060 | `	}` |
| 21556365 | 1061 | `	iP1 = 0;` |
| 21556365 | 1062 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
| 17011683 | 1063 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|        - | 1064 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
| 11588725 | 1065 | `			iP1 = 1;` |
|  5794360 | 1066 | `		}` |
|  8505839 | 1067 | `	}` |
|        - | 1068 | ``	/* iP2 marks a QUIET read: `isset($x)` / `empty($x)` inspect a variable`` |
|        - | 1069 | `	 * without reading it, so an undefined one must not warn (php stays silent` |
|        - | 1070 | `	 * for both). Every other read of a missing variable warns — see OP_LOAD.` |
|        - | 1071 | `	 * The two flags are cleared before recursing into a subscript's index` |
|        - | 1072 | ``	 * expression, so `isset($a[$i])` still warns for an undefined $i, as php`` |
|        - | 1073 | ``	 * does. Contexts that VIVIFY (assignment targets, `??`, appends) already`` |
|        - | 1074 | `	 * emit iP1 = 0 and never reach the warning. */` |
| 21556365 | 1075 | `	if( iCompileFlag & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_QUIET_VAR) ){` |
|   383873 | 1076 | `		iP2 = 1;` |
| 21364431 | 1077 | `	}else if( iCompileFlag & EXPR_FLAG_RMW_LOAD ){` |
|        - | 1078 | ``		/* Warn-then-create: the read half of `$x++` / `$x .= ...` still needs a`` |
|        - | 1079 | `		 * writable slot, so it cannot use the read-only load above. */` |
|   666463 | 1080 | `		iP2 = 2;` |
|   333229 | 1081 | `	}` |
|        - | 1082 | `	/* Emit the load instruction */` |
| 21556365 | 1083 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,iP2,p3,0);` |
| 21556377 | 1084 | `	while( iVv > 0 ){` |
|       13 | 1085 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,iP2,0,0);` |
|       13 | 1086 | `		iVv--;` |
|        1 | 1087 | `	}` |
|        - | 1088 | `	/* Node successfully compiled */` |
| 21556365 | 1089 | `	return SXRET_OK;` |
| 10778187 | 1090 | `}` |
|        - | 1091 | `/*` |
|        - | 1092 | ` * Load a literal.` |
|        - | 1093 | ` */` |
| 14062588 | 1094 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|        5 | 1095 | `{` |
| 14062593 | 1096 | `	SyToken *pToken = pGen->pIn;` |
|        - | 1097 | `	ph7_value *pObj;` |
|        - | 1098 | `	SyString *pStr;` |
|        - | 1099 | `	sxu32 nIdx;` |
|        - | 1100 | `	/* Extract token value */` |
| 14062593 | 1101 | `	pStr = &pToken->sData;` |
|        - | 1102 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first. A reserved` |
|        - | 1103 | `	 * word used as a member NAME (Enum::Null, C::Array, $o->list()) is a plain` |
|        - | 1104 | `	 * identifier — the parser flagged its token so the whole value-folding chain is` |
|        - | 1105 | `	 * skipped and it falls through to the ordinary string-literal emit below. */` |
| 14062593 | 1106 | `	if( pToken->nType & PH7_TK_MEMBER_NAME ){` |
|        - | 1107 | `		/* fall through to the plain-string literal path */` |
| 11194721 | 1108 | `	}else if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  1683243 | 1109 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|        - | 1110 | `			/* NULL constant are always indexed at 0 */` |
|  1154239 | 1111 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|  1154239 | 1112 | `			return SXRET_OK;` |
|   529009 | 1113 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|        - | 1114 | `			/* TRUE constant are always indexed at 1 */` |
|   357359 | 1115 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|   357359 | 1116 | `			return SXRET_OK;` |
|        5 | 1117 | `		}` |
|  7520807 | 1118 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|  1582742 | 1119 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|        - | 1120 | `			/* FALSE constant are always indexed at 2 */` |
|   794047 | 1121 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   794047 | 1122 | `			return SXRET_OK;` |
|  5995861 | 1123 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   292584 | 1124 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|        - | 1125 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|     3883 | 1126 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     3883 | 1127 | `			if( pObj == 0 ){` |
|      ! 0 | 1128 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1129 | `				return SXERR_ABORT;` |
|        - | 1130 | `			}` |
|     3883 | 1131 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|        - | 1132 | `			/* Emit the load constant instruction */` |
|     3883 | 1133 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     3883 | 1134 | `			return SXRET_OK;` |
|  5990039 | 1135 | `	}else if( (pStr->nByte == sizeof("__FILE__") - 1 &&` |
|   512932 | 1136 | `		SyMemcmp(pStr->zString,"__FILE__",sizeof("__FILE__")-1) == 0) \|\|` |
|  6065959 | 1137 | `		(pStr->nByte == sizeof("__DIR__") - 1 &&` |
|   448442 | 1138 | `		SyMemcmp(pStr->zString,"__DIR__",sizeof("__DIR__")-1) == 0) ){` |
|        - | 1139 | `			/* __FILE__ / __DIR__ are magic constants resolved at COMPILE time to the` |
|        - | 1140 | `			 * file being compiled (where the token is written), NOT the runtime` |
|        - | 1141 | `			 * execution file. A function defined in a.php reporting __FILE__ must say` |
|        - | 1142 | `			 * a.php even when called from b.php — php semantics, and what Composer's` |
|        - | 1143 | ``			 * autoloader (loadClassLoader's `require __DIR__ . '/ClassLoader.php'`)`` |
|        - | 1144 | `			 * relies on. The runtime-constant path returned the caller's file. */` |
|     3979 | 1145 | `			int bDir = (pStr->zString[2] == 'D'); /* __DIR__ vs __FILE__ */` |
|     3979 | 1146 | `			SyString *pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     3979 | 1147 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     3979 | 1148 | `			if( pObj == 0 ){` |
|      ! 0 | 1149 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1150 | `				return SXERR_ABORT;` |
|        - | 1151 | `			}` |
|     3979 | 1152 | `			if( pFile && pFile->nByte > 0 ){` |
|      109 | 1153 | `				if( bDir ){` |
|        - | 1154 | `					const char *zDir;` |
|        - | 1155 | `					int nLen;` |
|        - | 1156 | `					SyString sDir;` |
|       56 | 1157 | `					zDir = PH7_ExtractDirName(pFile->zString,(int)pFile->nByte,&nLen);` |
|       56 | 1158 | `					SyStringInitFromBuf(&sDir,zDir,nLen);` |
|       56 | 1159 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&sDir);` |
|       30 | 1160 | `				}else{` |
|       57 | 1161 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,pFile);` |
|        - | 1162 | `				}` |
|       57 | 1163 | `			}else{` |
|        - | 1164 | `				SyString sMem;` |
|     3875 | 1165 | `				SyStringInitFromBuf(&sMem,":MEMORY:",sizeof(":MEMORY:")-1);` |
|     3875 | 1166 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sMem);` |
|        - | 1167 | `			}` |
|     3979 | 1168 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     3979 | 1169 | `			return SXRET_OK;` |
|  5962033 | 1170 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   240632 | 1171 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|        - | 1172 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|        8 | 1173 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        8 | 1174 | `			if( pObj == 0 ){` |
|      ! 0 | 1175 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1176 | `				return SXERR_ABORT;` |
|        - | 1177 | `			}` |
|        8 | 1178 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        - | 1179 | `				SyString sNs;` |
|        8 | 1180 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        8 | 1181 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|        5 | 1182 | `			}else{` |
|      ! 0 | 1183 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|        - | 1184 | `			}` |
|        8 | 1185 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        8 | 1186 | `			return SXRET_OK;` |
|  5973879 | 1187 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   427937 | 1188 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  6005285 | 1189 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   327172 | 1190 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|       11 | 1191 | `			GenBlock *pBlock = pGen->pCurrent;` |
|        - | 1192 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|       21 | 1193 | `			while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|        - | 1194 | `				/* Point to the upper block */` |
|       11 | 1195 | `				pBlock = pBlock->pParent;` |
|        1 | 1196 | `			}` |
|       11 | 1197 | `			if( pBlock == 0 ){` |
|        - | 1198 | `				/* Called in the global scope,load NULL */` |
|        5 | 1199 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|        3 | 1200 | `			}else{` |
|        - | 1201 | `				/* Extract the target function/method */` |
|        7 | 1202 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        7 | 1203 | `				int bMethod = (pStr->zString[2] == 'M'); /* __METHOD__ vs __FUNCTION__ */` |
|        7 | 1204 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|        7 | 1205 | `				if( pObj == 0 ){` |
|      ! 0 | 1206 | `					PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1207 | `					return SXERR_ABORT;` |
|        - | 1208 | `				}` |
|        - | 1209 | `				/*` |
|        - | 1210 | `				 * __METHOD__ is the QUALIFIED name: "C::m" inside a method, and the plain` |
|        - | 1211 | `				 * function name inside a plain function (php does not answer "" there —` |
|        - | 1212 | `				 * PH7 loaded NULL, so __METHOD__ was empty in every free function and` |
|        - | 1213 | `				 * unqualified in every method).` |
|        - | 1214 | `				 */` |
|        8 | 1215 | `				if( bMethod && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|        3 | 1216 | `					SyString *pCls = &((ph7_class *)pFunc->pUserData)->sName;` |
|        - | 1217 | `					SyBlob sQual;` |
|        - | 1218 | `					SyString sOut;` |
|        3 | 1219 | `					SyBlobInit(&sQual,&pGen->pVm->sAllocator);` |
|        3 | 1220 | `					SyBlobFormat(&sQual,"%z::%z",pCls,&pFunc->sName);` |
|        3 | 1221 | `					SyStringInitFromBuf(&sOut,SyBlobData(&sQual),SyBlobLength(&sQual));` |
|        3 | 1222 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&sOut);` |
|        3 | 1223 | `					SyBlobRelease(&sQual);` |
|        2 | 1224 | `				}else{` |
|        5 | 1225 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&pFunc->sName);` |
|        - | 1226 | `				}` |
|        - | 1227 | `				/* Emit the load constant instruction */` |
|        7 | 1228 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - | 1229 | `			}` |
|       11 | 1230 | `			return SXRET_OK;` |
|        - | 1231 | `	}` |
|        - | 1232 | `	/* Query literal table */` |
| 11749095 | 1233 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&pToken->sData,&nIdx) ){` |
|        - | 1234 | `		ph7_value *pLitObj;` |
|        - | 1235 | `		/* Unknown literal,install it in the literal table */` |
|  2421741 | 1236 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  2421741 | 1237 | `		if( pLitObj == 0 ){` |
|      ! 0 | 1238 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 | 1239 | `			return SXERR_ABORT;` |
|        - | 1240 | `		}` |
|  2421741 | 1241 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,&pToken->sData);` |
|  2421741 | 1242 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  1210868 | 1243 | `	}` |
|        - | 1244 | `	/* Emit the load constant instruction */` |
| 11749095 | 1245 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,1,nIdx,0,0);` |
| 11749095 | 1246 | `	return SXRET_OK;` |
|  7031299 | 1247 | `}` |
|        - | 1248 | `/*` |
|        - | 1249 | ` * Resolve a namespace path or simply load a literal.` |
|        - | 1250 | ` * If the token stream contains namespace separators (backslashes),` |
|        - | 1251 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|        - | 1252 | ` * Otherwise, load the simple literal directly.` |
|        - | 1253 | ` */` |
| 14066548 | 1254 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|        5 | 1255 | `{` |
|        - | 1256 | `	sxi32 rc;` |
| 14066553 | 1257 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 1258 | `		return SXRET_OK;` |
|        - | 1259 | `	}` |
|        - | 1260 | `	/* Check if this is a multi-token namespace path */` |
| 14066553 | 1261 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|        - | 1262 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|     3965 | 1263 | `		SyBlob *pWorker = &pGen->sWorker;` |
|     3965 | 1264 | `		int isAbsolute = 0;` |
|     3965 | 1265 | `		SyBlobReset(pWorker);` |
|        - | 1266 | `		/* Check for leading backslash (absolute path) */` |
|     3965 | 1267 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|     3943 | 1268 | `			isAbsolute = 1;` |
|     3943 | 1269 | `			pGen->pIn++; /* Skip leading backslash */` |
|     1969 | 1270 | `		}` |
|        - | 1271 | `		/* Collect the raw path (no prefix yet) so its FIRST segment can be resolved` |
|        - | 1272 | ``		 * against use-imports below — php resolves `A\B\C` by mapping the leading`` |
|        - | 1273 | ``		 * `A` through the imports (`use X\A;` makes it `X\A\B\C`), and only prepends`` |
|        - | 1274 | ``		 * the current namespace when `A` matches no import. Blindly prefixing the`` |
|        - | 1275 | ``		 * namespace here produced e.g. `Ns\Ev\Bus` for `use ...\Event as Ev; Ev\Bus`. */`` |
|        - | 1276 | `		{` |
|        - | 1277 | `			SyBlob sRaw;` |
|     3965 | 1278 | `			SyBlobInit(&sRaw,&pGen->pVm->sAllocator);` |
|     4129 | 1279 | `			while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|     4129 | 1280 | `				if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|       86 | 1281 | `					SyBlobAppend(&sRaw,"\\",1);` |
|       45 | 1282 | `				}else{` |
|     4047 | 1283 | `					SyBlobAppend(&sRaw,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        - | 1284 | `				}` |
|     4129 | 1285 | `				if( pGen->pIn == &pGen->pEnd[-1] ){` |
|     3965 | 1286 | `					pGen->pIn++;` |
|     3965 | 1287 | `					break;` |
|        - | 1288 | `				}` |
|      168 | 1289 | `				pGen->pIn++;` |
|        4 | 1290 | `			}` |
|     3965 | 1291 | `			if( isAbsolute ){` |
|     3943 | 1292 | `				SyBlobAppend(pWorker,SyBlobData(&sRaw),SyBlobLength(&sRaw)); /* FQN as written */` |
|     1974 | 1293 | `			}else{` |
|       24 | 1294 | `				const char *zRaw = (const char *)SyBlobData(&sRaw);` |
|       24 | 1295 | `				sxu32 nRaw = SyBlobLength(&sRaw);` |
|       24 | 1296 | `				sxu32 nFirst = 0;` |
|        - | 1297 | `				SyHashEntry *pNsImp;` |
|      108 | 1298 | `				while( nFirst < nRaw && zRaw[nFirst] != '\\' ){ nFirst++; }` |
|       24 | 1299 | `				pNsImp = SyHashGet(&pGen->hUseImports,(const void *)zRaw,nFirst);` |
|       24 | 1300 | `				if( pNsImp ){` |
|        - | 1301 | `					/* Leading segment is an imported alias: substitute its FQN. */` |
|       21 | 1302 | `					const char *zFQN = (const char *)pNsImp->pUserData;` |
|       21 | 1303 | `					SyBlobAppend(pWorker,zFQN,SyStrlen(zFQN));` |
|       21 | 1304 | `					SyBlobAppend(pWorker,zRaw + nFirst,nRaw - nFirst);` |
|       13 | 1305 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        3 | 1306 | `					SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        3 | 1307 | `					SyBlobAppend(pWorker,"\\",1);` |
|        3 | 1308 | `					SyBlobAppend(pWorker,zRaw,nRaw);` |
|        2 | 1309 | `				}else{` |
|      ! 0 | 1310 | `					SyBlobAppend(pWorker,zRaw,nRaw); /* global scope, no import */` |
|        - | 1311 | `				}` |
|        - | 1312 | `			}` |
|     3965 | 1313 | `			SyBlobRelease(&sRaw);` |
|        - | 1314 | `		}` |
|     3965 | 1315 | `		if( SyBlobLength(pWorker) > 0 ){` |
|        - | 1316 | `			ph7_value *pObj;` |
|        - | 1317 | `			SyString sPath;` |
|        - | 1318 | `			sxu32 nIdx;` |
|     3965 | 1319 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|        - | 1320 | `			/* Install in the literal table */` |
|     3965 | 1321 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|     3913 | 1322 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     3913 | 1323 | `				if( pObj == 0 ){` |
|      ! 0 | 1324 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 | 1325 | `					return SXERR_ABORT;` |
|        - | 1326 | `				}` |
|     3913 | 1327 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|     3913 | 1328 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     1954 | 1329 | `			}` |
|        - | 1330 | `			/* Emit the load constant instruction.` |
|        - | 1331 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|        - | 1332 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|     5945 | 1333 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|     1980 | 1334 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|     1980 | 1335 | `				nIdx,0,0);` |
|     3965 | 1336 | `			return SXRET_OK;` |
|        - | 1337 | `		}` |
|      ! 0 | 1338 | `	}` |
|        - | 1339 | `	/* Single-token literal: load directly */` |
| 14062593 | 1340 | `	rc = GenStateLoadLiteral(&(*pGen));` |
| 14062593 | 1341 | `	return rc;` |
|  7033279 | 1342 | `}` |
|        - | 1343 | `/*` |
|        - | 1344 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|        - | 1345 | ` */` |
|        - | 1346 | `/*` |
|        - | 1347 | `` * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of`` |
|        - | 1348 | `` * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument`` |
|        - | 1349 | ``  * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...` `` |
|        - | 1350 | ` * appeared outside a call argument list — a syntax error (PHP rejects it likewise).` |
|        - | 1351 | ` */` |
|      ! 0 | 1352 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|      ! 0 | 1353 | `{` |
|      ! 0 | 1354 | `	SXUNUSED(iCompileFlag);` |
|      ! 0 | 1355 | `	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|        - | 1356 | `		"Cannot use the first-class callable syntax '...' here");` |
|      ! 0 | 1357 | `	return SXERR_SYNTAX;` |
|      ! 0 | 1358 | `}` |
| 14066548 | 1359 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 1360 | `{` |
|        - | 1361 | `	sxi32 rc;` |
| 14066553 | 1362 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
| 14066553 | 1363 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1364 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 | 1365 | `		return rc;` |
|        - | 1366 | `	}` |
|        - | 1367 | `	/* Node successfully compiled */` |
| 14066553 | 1368 | `	return SXRET_OK;` |
|  7033279 | 1369 | `}` |
|        - | 1370 |  |
