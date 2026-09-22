# src/ph7/compile_node.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 802/929 lines (86.33%)

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
|     1582 |   36 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |   37 | `{` |
|     1587 |   38 | `	ph7_vm_func *pAnnonFunc = 0; /* Annonymous function body */` |
|        - |   39 | `	char zName[512];         /* Unique lambda name */` |
|        - |   40 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|        - |   41 | `							  * one thread is allowed to compile the script.` |
|        - |   42 | `						      */` |
|        - |   43 | `	SyString sName;` |
|     1587 |   44 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `$f = #[A] function…` trivia`` |
|        - |   45 | `	                              * is keyed to this ['static'] 'function' token */` |
|        - |   46 | `	sxu32 nKwLine;` |
|     1587 |   47 | `	sxi32 iFlags = 0;` |
|        - |   48 | `	sxu32 nLen;` |
|        - |   49 | `	sxi32 rc;` |
|      791 |   50 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |   51 |  |
|     1587 |   52 | `	nKwLine = pGen->pIn->nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|     1582 |   53 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     1587 |   54 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|        - |   55 | `		/* Static closure: no $this auto-capture, bind refused */` |
|       46 |   56 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|       46 |   57 | `		pGen->pIn++; /* Jump the 'static' keyword */` |
|       21 |   58 | `	}` |
|     1587 |   59 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|     1587 |   60 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|      ! 0 |   61 | `		pGen->pIn++;` |
|      ! 0 |   62 | `	}` |
|        - |   63 | `	/* Generate a unique name */` |
|     1587 |   64 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|        - |   65 | `	/* Make sure the generated name is unique */` |
|     1587 |   66 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|      ! 0 |   67 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|      ! 0 |   68 | `	}` |
|     1587 |   69 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|        - |   70 | `	/* Compile the lambda body */` |
|     1587 |   71 | `	rc = GenStateCompileFunc(&(*pGen),&sName,iFlags,TRUE,&pAnnonFunc);` |
|     1587 |   72 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |   73 | `		return SXERR_ABORT;` |
|        - |   74 | `	}` |
|     1587 |   75 | `	if( pAnnonFunc ){` |
|     1585 |   76 | `		pAnnonFunc->nLine = nKwLine;` |
|        - |   77 | ``		/* Expression-position attributes (`$f = #[A] function () {}`): the trivia`` |
|        - |   78 | `		 * sidecar keys them to the closure's first keyword token. */` |
|     1585 |   79 | `		if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnnonFunc->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |   80 | `			return SXERR_ABORT;` |
|        - |   81 | `		}` |
|      790 |   82 | `	}` |
|        - |   83 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|        - |   84 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|        - |   85 | `	 * the handler wraps either in a Closure instance. */` |
|     1587 |   86 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|        - |   87 | `	/* Node successfully compiled */` |
|     1587 |   88 | `	return SXRET_OK;` |
|      796 |   89 | `}` |
|        - |   90 | `/*` |
|        - |   91 | ` * Add a free variable to the arrow function's closure environment, unless` |
|        - |   92 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|        - |   93 | ` * enclosing arrow level, or has already been captured.` |
|        - |   94 | ` */` |
|      592 |   95 | `static sxi32 GenStateArrowAddCapture(` |
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
|      597 |  107 | `	if( nByte == 0 ){` |
|      ! 0 |  108 | `		return SXRET_OK;` |
|        - |  109 | `	}` |
|      592 |  110 | `	if( nByte == sizeof("this")-1` |
|      313 |  111 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|        9 |  112 | `		return SXRET_OK;` |
|        - |  113 | `	}` |
|      589 |  114 | `	if( PH7_VmIsAutoGlobal(zName,nByte) ){` |
|        - |  115 | `		/* php never auto-captures an auto-global — it is already visible inside` |
|        - |  116 | `		 * the arrow function. Capturing one was actively destructive here: the` |
|        - |  117 | `		 * install resolves the name through hSuper and so wrote the by-value` |
|        - |  118 | `		 * SNAPSHOT over the superglobal's own slot, which for $GLOBALS froze the` |
|        - |  119 | `		 * whole symbol-table view at closure-creation time for the rest of the` |
|        - |  120 | `		 * program. */` |
|        7 |  121 | `		return SXRET_OK;` |
|        - |  122 | `	}` |
|      679 |  123 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|      408 |  124 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|      401 |  125 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|      317 |  126 | `			return SXRET_OK;` |
|        - |  127 | `		}` |
|       52 |  128 | `	}` |
|      271 |  129 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      271 |  130 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|      301 |  131 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|       34 |  132 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|       34 |  133 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|        6 |  134 | `			return SXRET_OK;` |
|        - |  135 | `		}` |
|       17 |  136 | `	}` |
|      267 |  137 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|      267 |  138 | `	if( zDup == 0 ){` |
|      ! 0 |  139 | `		return SXERR_ABORT;` |
|        - |  140 | `	}` |
|      267 |  141 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|      267 |  142 | `	sEnv.iFlags = 0;` |
|      267 |  143 | `	sEnv.nIdx = SXU32_HIGH;` |
|      267 |  144 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|      267 |  145 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|      267 |  146 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      267 |  147 | `	return SXRET_OK;` |
|      301 |  148 | `}` |
|        - |  149 | `/*` |
|        - |  150 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|        - |  151 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|        - |  152 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|        - |  153 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|        - |  154 | ` */` |
|      758 |  155 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|        - |  156 | `	ph7_gen_state *pGen,` |
|        - |  157 | `	ph7_vm_func *pFunc,` |
|        - |  158 | `	const char *zIn,` |
|        - |  159 | `	const char *zEnd,` |
|        - |  160 | `	SyString *aShadow,` |
|        - |  161 | `	sxu32 nShadow)` |
|        5 |  162 | `{` |
|        - |  163 | `	sxi32 rc;` |
|     2495 |  164 | `	while( zIn < zEnd ){` |
|     1737 |  165 | `		if( zIn[0] == '\\' ){` |
|       46 |  166 | `			zIn++;` |
|       46 |  167 | `			if( zIn < zEnd ){` |
|       46 |  168 | `				zIn++;` |
|       21 |  169 | `			}` |
|       46 |  170 | `			continue;` |
|        - |  171 | `		}` |
|     1690 |  172 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|       29 |  173 | `			&& ((unsigned char)zIn[1] >= 0x80` |
|       24 |  174 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|        - |  175 | `			/* php's label bytes, the flat set (LEX_LABEL_START in lex.c). */` |
|        - |  176 | `			const char *zName;` |
|       26 |  177 | `			zIn++; /* skip '$' */` |
|       26 |  178 | `			zName = zIn;` |
|       64 |  179 | `			while( zIn < zEnd` |
|       82 |  180 | `				&& ((unsigned char)zIn[0] >= 0x80` |
|       74 |  181 | `					\|\| SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_') ){` |
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
|     1671 |  193 | `		zIn++;` |
|        5 |  194 | `	}` |
|      763 |  195 | `	return SXRET_OK;` |
|      384 |  196 | `}` |
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
|     1728 |  208 | `static sxi32 GenStateArrowCaptureScan(` |
|        - |  209 | `	ph7_gen_state *pGen,` |
|        - |  210 | `	ph7_vm_func *pFunc,` |
|        - |  211 | `	SyToken *pStart,` |
|        - |  212 | `	SyToken *pEnd,` |
|        - |  213 | `	SyString *aShadow,` |
|        - |  214 | `	sxu32 nShadow)` |
|        5 |  215 | `{` |
|     1733 |  216 | `	SyToken *pScan = pStart;` |
|        - |  217 | `	sxi32 rc;` |
|    13663 |  218 | `	while( pScan < pEnd ){` |
|    11935 |  219 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|     1142 |  220 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|      379 |  221 | `				pScan->sData.zString,` |
|      758 |  222 | `				pScan->sData.zString + pScan->sData.nByte,` |
|      379 |  223 | `				aShadow,nShadow);` |
|      763 |  224 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  225 | `				return SXERR_ABORT;` |
|        - |  226 | `			}` |
|      763 |  227 | `			pScan++;` |
|      763 |  228 | `			continue;` |
|        - |  229 | `		}` |
|    11177 |  230 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|       73 |  231 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|       73 |  232 | `			SyToken *pFnKw = (nKw == PH7_TKWRD_STATIC) ? &pScan[1] : pScan;` |
|        - |  233 | ``			/* A NESTED arrow function, not a `$fn`/`C::fn` name that merely`` |
|        - |  234 | `			 * spells the keyword (see PH7_TokenOpensArrowFunc). */` |
|       73 |  235 | `			if( PH7_TokenOpensArrowFunc(pStart,pScan,pEnd) ){` |
|        - |  236 | `				SyToken *pInnerSigStart;` |
|        - |  237 | `				SyToken *pInnerSigEnd;` |
|        - |  238 | `				SyToken *pInnerBodyEnd;` |
|        - |  239 | `				SyString *aInnerShadow;` |
|        - |  240 | `				sxu32 nInnerShadow;` |
|        - |  241 | `				sxu32 nInnerParamMax;` |
|        - |  242 | `				SyToken *p;` |
|        - |  243 | `				int iNestInner;` |
|       41 |  244 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|       41 |  245 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|      ! 0 |  246 | `					pScan++;` |
|      ! 0 |  247 | `				}` |
|       41 |  248 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 |  249 | `					pScan++;` |
|      ! 0 |  250 | `					continue;` |
|        - |  251 | `				}` |
|       41 |  252 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|       41 |  253 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|        - |  254 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|       41 |  255 | `				if( pInnerSigEnd >= pEnd ){` |
|      ! 0 |  256 | `					pScan = pEnd;` |
|      ! 0 |  257 | `					continue;` |
|        - |  258 | `				}` |
|        - |  259 | `				/* Build an augmented shadow list: inherited + inner params */` |
|       41 |  260 | `				nInnerParamMax = 0;` |
|      145 |  261 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      107 |  262 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|       43 |  263 | `						nInnerParamMax++;` |
|       20 |  264 | `					}` |
|       55 |  265 | `				}` |
|       41 |  266 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|       38 |  267 | `					&pGen->pVm->sAllocator,` |
|       38 |  268 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|       41 |  269 | `				if( aInnerShadow == 0 ){` |
|      ! 0 |  270 | `					return SXERR_ABORT;` |
|        - |  271 | `				}` |
|       41 |  272 | `				nInnerShadow = 0;` |
|       49 |  273 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|        9 |  274 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|        5 |  275 | `				}` |
|      145 |  276 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|      107 |  277 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|       67 |  278 | `						continue;` |
|        - |  279 | `					}` |
|       43 |  280 | `					if( &p[1] >= pInnerSigEnd ){` |
|      ! 0 |  281 | `						break;` |
|        - |  282 | `					}` |
|       43 |  283 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  284 | `						continue;` |
|        - |  285 | `					}` |
|       43 |  286 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|       23 |  287 | `				}` |
|       41 |  288 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|       41 |  289 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|      ! 0 |  290 | `					pScan++;` |
|      ! 0 |  291 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|      ! 0 |  292 | `						&& pScan->sData.nByte == 1` |
|      ! 0 |  293 | `						&& pScan->sData.zString[0] == '?' ){` |
|      ! 0 |  294 | `						pScan++;` |
|      ! 0 |  295 | `					}` |
|      ! 0 |  296 | `					if( pScan < pEnd` |
|      ! 0 |  297 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|      ! 0 |  298 | `						pScan++;` |
|      ! 0 |  299 | `					}` |
|      ! 0 |  300 | `				}` |
|       41 |  301 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|       41 |  302 | `					pScan++; /* past '=>' */` |
|       19 |  303 | `				}` |
|       41 |  304 | `				pInnerBodyEnd = pScan;` |
|       41 |  305 | `				iNestInner = 0;` |
|      253 |  306 | `				while( pInnerBodyEnd < pEnd ){` |
|      231 |  307 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|        - |  308 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|        - |  309 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|       18 |  310 | `						break;` |
|        - |  311 | `					}` |
|      215 |  312 | `					if( pInnerBodyEnd->nType &` |
|        - |  313 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       13 |  314 | `						iNestInner++;` |
|      210 |  315 | `					}else if( pInnerBodyEnd->nType &` |
|        - |  316 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       13 |  317 | `						iNestInner--;` |
|        5 |  318 | `					}` |
|      215 |  319 | `					pInnerBodyEnd++;` |
|        3 |  320 | `				}` |
|        - |  321 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|        - |  322 | `				 * the outer's body: a default value is evaluated at call time` |
|        - |  323 | `				 * in the outer frame, so any free variable it references is` |
|        - |  324 | `				 * an outer capture. We must NOT scan the parameter-name` |
|        - |  325 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|        - |  326 | `				 * or those names leak into the outer's closure environment.` |
|        - |  327 | `				 *` |
|        - |  328 | `				 * Walk the signature argument-by-argument, splitting on` |
|        - |  329 | `				 * top-level commas, and for each argument scan only the token` |
|        - |  330 | `				 * range after the '=' sign. */` |
|        - |  331 | `				{` |
|       41 |  332 | `					SyToken *pArgStart = pInnerSigStart;` |
|       81 |  333 | `					while( pArgStart < pInnerSigEnd ){` |
|       43 |  334 | `						SyToken *pArgEnd = pArgStart;` |
|       43 |  335 | `						SyToken *pEq = 0;` |
|       43 |  336 | `						int iNestArg = 0;` |
|      135 |  337 | `						while( pArgEnd < pInnerSigEnd ){` |
|      104 |  338 | `							if( iNestArg == 0` |
|      107 |  339 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|       14 |  340 | `								break;` |
|        - |  341 | `							}` |
|       95 |  342 | `							if( pArgEnd->nType &` |
|        - |  343 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      ! 0 |  344 | `								iNestArg++;` |
|       95 |  345 | `							}else if( pArgEnd->nType &` |
|        - |  346 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      ! 0 |  347 | `								iNestArg--;` |
|      ! 0 |  348 | `							}` |
|       92 |  349 | `							if( pEq == 0 && iNestArg == 0` |
|       89 |  350 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|        7 |  351 | `								pEq = pArgEnd;` |
|        3 |  352 | `							}` |
|       95 |  353 | `							pArgEnd++;` |
|        3 |  354 | `						}` |
|       43 |  355 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|       10 |  356 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|        3 |  357 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|        7 |  358 | `							if( rc == SXERR_ABORT ){` |
|      ! 0 |  359 | `								return SXERR_ABORT;` |
|        - |  360 | `							}` |
|        3 |  361 | `						}` |
|       43 |  362 | `						pArgStart = pArgEnd;` |
|       40 |  363 | `						if( pArgStart < pInnerSigEnd` |
|       29 |  364 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|       14 |  365 | `							pArgStart++;` |
|        6 |  366 | `						}` |
|        3 |  367 | `					}` |
|        - |  368 | `				}` |
|       60 |  369 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       19 |  370 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|       41 |  371 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  372 | `					return SXERR_ABORT;` |
|        - |  373 | `				}` |
|       41 |  374 | `				pScan = pInnerBodyEnd;` |
|       41 |  375 | `				continue;` |
|        - |  376 | `			}` |
|       15 |  377 | `		}` |
|    11139 |  378 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|    10571 |  379 | `			pScan++;` |
|    10571 |  380 | `			continue;` |
|        - |  381 | `		}` |
|        - |  382 | `		{` |
|        - |  383 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|      573 |  384 | `			SyToken *pDollar = pScan;` |
|      852 |  385 | `			while( &pDollar[1] < pEnd` |
|      573 |  386 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|      ! 0 |  387 | `				pDollar++;` |
|      ! 0 |  388 | `			}` |
|      573 |  389 | `			if( &pDollar[1] >= pEnd ){` |
|      ! 0 |  390 | `				break;` |
|        - |  391 | `			}` |
|      573 |  392 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 |  393 | `				pScan = pDollar + 1;` |
|      ! 0 |  394 | `				continue;` |
|        - |  395 | `			}` |
|      857 |  396 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|      568 |  397 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|      284 |  398 | `				aShadow,nShadow);` |
|      573 |  399 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  400 | `				return SXERR_ABORT;` |
|        - |  401 | `			}` |
|      573 |  402 | `			pScan = pDollar + 2;` |
|        - |  403 | `		}` |
|        5 |  404 | `	}` |
|     1733 |  405 | `	return SXRET_OK;` |
|      869 |  406 | `}` |
|        - |  407 | `/*` |
|        - |  408 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|        - |  409 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|        - |  410 | ` * variables by value. The body is a single expression that acts as an` |
|        - |  411 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|        - |  412 | ` * $this is also made available.` |
|        - |  413 | ` */` |
|     1690 |  414 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  415 | `{` |
|        - |  416 | `	ph7_vm_func *pFunc;` |
|        - |  417 | `	ph7_vm_func_closure_env sEnv;` |
|        - |  418 | `	GenBlock *pBlock;` |
|        - |  419 | `	SySet *pInstrContainer;` |
|        - |  420 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|        - |  421 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|        - |  422 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|        - |  423 | `	SyToken *pSavedEnd;` |
|        - |  424 | `	ph7_vm_func_arg *aArgs;` |
|        - |  425 | `	char zName[512];` |
|        - |  426 | `	static int iCnt = 1;` |
|        - |  427 | `	char *zDup;` |
|        - |  428 | `	SyToken *pTokKw;` |
|        - |  429 | `	sxu32 nLen;` |
|        - |  430 | `	sxu32 nLine;` |
|     1695 |  431 | `	sxi32 iFlags = 0;` |
|     1695 |  432 | `	int bStatic = 0;` |
|        - |  433 | `	sxi32 rc;` |
|        - |  434 | `	sxu32 n;` |
|      845 |  435 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|        - |  436 |  |
|     1695 |  437 | `	nLine = pGen->pIn->nLine;` |
|        - |  438 | ``	/* Attribute-sidecar key: `#[A] [static] fn` trivia is keyed to this token */`` |
|     1695 |  439 | `	pTokKw = pGen->pIn;` |
|        - |  440 | `	/* Optional 'static' prefix */` |
|     1690 |  441 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     1695 |  442 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       27 |  443 | `		bStatic = 1;` |
|       27 |  444 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|       27 |  445 | `		pGen->pIn++;` |
|       13 |  446 | `	}` |
|        - |  447 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|     1690 |  448 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|     1695 |  449 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|      ! 0 |  450 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  451 | `			"Arrow function: expected 'fn' keyword");` |
|      ! 0 |  452 | `		return SXERR_SYNTAX;` |
|        - |  453 | `	}` |
|     1695 |  454 | `	pGen->pIn++; /* Jump 'fn' */` |
|        - |  455 | `	/* Optional '&' — return by reference */` |
|     1695 |  456 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|      ! 0 |  457 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|      ! 0 |  458 | `		pGen->pIn++;` |
|      ! 0 |  459 | `	}` |
|        - |  460 | `	/* Expect '(' */` |
|     1695 |  461 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        3 |  462 | `		if( pGen->pIn < pGen->pEnd ){` |
|        4 |  463 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|        - |  464 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|        2 |  465 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        2 |  466 | `		}else{` |
|      ! 0 |  467 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - |  468 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|        - |  469 | `		}` |
|        3 |  470 | `		return SXERR_SYNTAX;` |
|        - |  471 | `	}` |
|     1693 |  472 | `	pGen->pIn++; /* Jump '(' */` |
|        - |  473 | `	/* Delimit the parameter list */` |
|     1693 |  474 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|     1693 |  475 | `	if( pSigEnd >= pGen->pEnd ){` |
|        3 |  476 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - |  477 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|        3 |  478 | `		return SXERR_SYNTAX;` |
|        - |  479 | `	}` |
|        - |  480 | `	/* Allocate the function state */` |
|     1691 |  481 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|     1691 |  482 | `	if( pFunc == 0 ){` |
|      ! 0 |  483 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  484 | `			"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  485 | `		return SXERR_ABORT;` |
|        - |  486 | `	}` |
|        - |  487 | `	/* Generate a unique lambda name */` |
|     1691 |  488 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     1693 |  489 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|        3 |  490 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|        1 |  491 | `	}` |
|     1691 |  492 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|     1691 |  493 | `	if( zDup == 0 ){` |
|      ! 0 |  494 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  495 | `			"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  496 | `		return SXERR_ABORT;` |
|        - |  497 | `	}` |
|     1691 |  498 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|        - |  499 | `	/* Reflection getStartLine(): line of the ['static'] 'fn' keyword */` |
|     1691 |  500 | `	pFunc->nLine = nLine;` |
|        - |  501 | ``	/* Expression-position attributes (`$f = #[A] fn () => …`) */`` |
|     1691 |  502 | `	if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pFunc->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  503 | `		return SXERR_ABORT;` |
|        - |  504 | `	}` |
|        - |  505 | `	/* Collect function arguments */` |
|     1691 |  506 | `	if( pGen->pIn < pSigEnd ){` |
|      269 |  507 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|      269 |  508 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  509 | `			return SXERR_ABORT;` |
|        - |  510 | `		}` |
|      132 |  511 | `	}` |
|        - |  512 | `	/* Point past ')' and parse optional return type */` |
|     1691 |  513 | `	pGen->pIn = &pSigEnd[1];` |
|     1691 |  514 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|     1691 |  515 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  516 | `		return SXERR_ABORT;` |
|     1691 |  517 | `	}else if( rc == SXERR_SYNTAX ){` |
|      ! 0 |  518 | `		return SXERR_SYNTAX;` |
|        - |  519 | `	}` |
|        - |  520 | `	/* Expect '=>' */` |
|     1691 |  521 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|        3 |  522 | `		if( pGen->pIn < pGen->pEnd ){` |
|        4 |  523 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|        - |  524 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|        2 |  525 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        2 |  526 | `		}else{` |
|      ! 0 |  527 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - |  528 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|        - |  529 | `		}` |
|        3 |  530 | `		return SXERR_SYNTAX;` |
|        - |  531 | `	}` |
|     1689 |  532 | `	pGen->pIn++; /* Jump '=>' */` |
|     1689 |  533 | `	pBodyStart = pGen->pIn;` |
|     1689 |  534 | `	pBodyEnd = pGen->pEnd;` |
|        - |  535 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|        - |  536 | `	 * recursively collect free-variable references from the body. The scan` |
|        - |  537 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|        - |  538 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|     1689 |  539 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|        - |  540 | `	{` |
|     1689 |  541 | `		SyString *aShadow = 0;` |
|     1689 |  542 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|     1689 |  543 | `		if( nShadow > 0 ){` |
|      267 |  544 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      262 |  545 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|      267 |  546 | `			if( aShadow == 0 ){` |
|      ! 0 |  547 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  548 | `					"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  549 | `				return SXERR_ABORT;` |
|        - |  550 | `			}` |
|      587 |  551 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|      325 |  552 | `				aShadow[n] = aArgs[n].sName;` |
|      165 |  553 | `			}` |
|      131 |  554 | `		}` |
|     2531 |  555 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|      842 |  556 | `			aShadow,nShadow);` |
|     1689 |  557 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  558 | `			return SXERR_ABORT;` |
|        - |  559 | `		}` |
|        - |  560 | `	}` |
|        - |  561 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|        - |  562 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|        - |  563 | `	 * captured value is silently dropped when the enclosing scope has no` |
|        - |  564 | `	 * $this. */` |
|     1689 |  565 | `	if( !bStatic ){` |
|        - |  566 | `		char *zThisDup;` |
|     1663 |  567 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|     1663 |  568 | `		if( zThisDup == 0 ){` |
|      ! 0 |  569 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  570 | `				"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  571 | `			return SXERR_ABORT;` |
|        - |  572 | `		}` |
|     1663 |  573 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|     1663 |  574 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|     1663 |  575 | `		sEnv.nIdx = SXU32_HIGH;` |
|     1663 |  576 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|     1663 |  577 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|     1663 |  578 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|      829 |  579 | `	}` |
|        - |  580 | `	/* Arrow functions are always closures; the ARROW mark tells OP_LOAD_CLOSURE` |
|        - |  581 | `	 * these captures are implicit (auto-scanned) so an undefined one stays silent` |
|        - |  582 | `	 * at creation — php only warns when the body reads it. */` |
|     1689 |  583 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE \| VM_FUNC_ARROW;` |
|        - |  584 | `	/* Compile the body expression as an implicit return */` |
|     2531 |  585 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      842 |  586 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|     1689 |  587 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  588 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  589 | `			"PH7 engine is running out-of-memory");` |
|      ! 0 |  590 | `		return SXERR_ABORT;` |
|        - |  591 | `	}` |
|     1689 |  592 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     1689 |  593 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|     1689 |  594 | `	pSavedEnd = pGen->pEnd;` |
|     1689 |  595 | `	pGen->pIn = pBodyStart;` |
|     1689 |  596 | `	pGen->pEnd = pBodyEnd;` |
|        - |  597 | ``	/* The body is an implicit `return <expr>`, which READS its operands. Compile`` |
|        - |  598 | `	 * it read-only (like echo / string interpolation) so a lone undefined variable` |
|        - |  599 | ``	 * — e.g. `fn()=>$z` for an auto-capture that was undefined at creation and so`` |
|        - |  600 | `	 * never captured (see VmExecOpLoadClosure) — raises php's "Undefined variable"` |
|        - |  601 | `	 * warning at the read instead of being loaded quietly as a plain expression` |
|        - |  602 | ``	 * statement (`$z;`, silent in both engines) would be. */`` |
|     1689 |  603 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|     1689 |  604 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  605 | `		return SXERR_ABORT;` |
|        - |  606 | `	}` |
|        - |  607 | `	/* The cursor stopped just past the body expression */` |
|     1689 |  608 | `	pFunc->nEndLine = (pGen->pIn > pBodyStart) ? pGen->pIn[-1].nLine : nLine;` |
|        - |  609 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|        - |  610 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|        - |  611 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|        - |  612 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|     1689 |  613 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     1689 |  614 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     1689 |  615 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     1689 |  616 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     1689 |  617 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - |  618 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|     1689 |  619 | `	pGen->pIn = pBodyEnd;` |
|     1689 |  620 | `	pGen->pEnd = pSavedEnd;` |
|        - |  621 | `	/* Emit the load-closure instruction */` |
|     1689 |  622 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|     1689 |  623 | `	return SXRET_OK;` |
|      850 |  624 | `}` |
|        - |  625 | `/*` |
|        - |  626 | ` * Compile a single arm's expression range into a freshly-allocated` |
|        - |  627 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|        - |  628 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|        - |  629 | ` * expression's value.` |
|        - |  630 | ` */` |
|      458 |  631 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|        - |  632 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|        4 |  633 | `{` |
|        - |  634 | `	SySet *pInstrContainer;` |
|        - |  635 | `	SyToken *pTmpIn,*pTmpEnd;` |
|        - |  636 | `	GenBlock *pArmBlock;` |
|        - |  637 | `	sxi32 rc;` |
|      462 |  638 | `	pTmpIn  = pGen->pIn;` |
|      462 |  639 | `	pTmpEnd = pGen->pEnd;` |
|      462 |  640 | `	pGen->pIn  = pStart;` |
|      462 |  641 | `	pGen->pEnd = pStop;` |
|      462 |  642 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      462 |  643 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|        - |  644 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|        - |  645 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|        - |  646 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|        - |  647 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|        - |  648 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|      691 |  649 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|      229 |  650 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|      462 |  651 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  652 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 |  653 | `		pGen->pIn  = pTmpIn;` |
|      ! 0 |  654 | `		pGen->pEnd = pTmpEnd;` |
|      ! 0 |  655 | `		return SXERR_ABORT;` |
|        - |  656 | `	}` |
|      462 |  657 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      462 |  658 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      462 |  659 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|      462 |  660 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|      462 |  661 | `	GenStateLeaveBlock(&(*pGen),0);` |
|      462 |  662 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      462 |  663 | `	pGen->pIn  = pTmpIn;` |
|      462 |  664 | `	pGen->pEnd = pTmpEnd;` |
|      462 |  665 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  666 | `		return SXERR_ABORT;` |
|        - |  667 | `	}` |
|      462 |  668 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 |  669 | `		return SXERR_EMPTY;` |
|        - |  670 | `	}` |
|      462 |  671 | `	return SXRET_OK;` |
|      233 |  672 | `}` |
|        - |  673 | `/*` |
|        - |  674 | ` * Compile a PHP 8.0 match expression:` |
|        - |  675 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|        - |  676 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|        - |  677 | ` * Strict comparison (===) is used between the subject and each condition.` |
|        - |  678 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|        - |  679 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|        - |  680 | ` */` |
|        - |  681 | `/*` |
|        - |  682 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|        - |  683 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|        - |  684 | ` * caller can bail out of the current expression.` |
|        - |  685 | ` */` |
|        2 |  686 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|        1 |  687 | `{` |
|        - |  688 | `	va_list ap;` |
|        - |  689 | `	sxi32 rc;` |
|        - |  690 | `	SyBlob sMsg;` |
|        3 |  691 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|        3 |  692 | `	va_start(ap,zFmt);` |
|        3 |  693 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|        3 |  694 | `	va_end(ap);` |
|        3 |  695 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|        3 |  696 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|        3 |  697 | `	SyBlobRelease(&sMsg);` |
|        3 |  698 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  699 | `		return SXERR_ABORT;` |
|        - |  700 | `	}` |
|        3 |  701 | `	return SXERR_SYNTAX;` |
|        2 |  702 | `}` |
|        - |  703 | `/*` |
|        - |  704 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|        - |  705 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|        - |  706 | ` * Returns the stop token pointer (or pEnd if none found).` |
|        - |  707 | ` */` |
|      460 |  708 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|        4 |  709 | `{` |
|      464 |  710 | `	SyToken *pCur = pStart;` |
|      464 |  711 | `	int iNest = 0;` |
|     1116 |  712 | `	while( pCur < pEnd ){` |
|     1050 |  713 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       34 |  714 | `			iNest++;` |
|     1034 |  715 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       34 |  716 | `			iNest--;` |
|     1002 |  717 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|      398 |  718 | `			return pCur;` |
|        - |  719 | `		}` |
|      656 |  720 | `		pCur++;` |
|        4 |  721 | `	}` |
|       70 |  722 | `	return pEnd;` |
|      234 |  723 | `}` |
|      104 |  724 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  725 | `{` |
|        - |  726 | `	ph7_match *pMatch;` |
|        - |  727 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|      109 |  728 | `	int bHasDefault = 0;` |
|        - |  729 | `	sxu32 nLine;` |
|        - |  730 | `	sxi32 rc;` |
|       52 |  731 | `	SXUNUSED(iCompileFlag);` |
|      109 |  732 | `	nLine = pGen->pIn->nLine;` |
|      109 |  733 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|        - |  734 | `	/* Expect '(' */` |
|      109 |  735 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 |  736 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  737 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|      ! 0 |  738 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|        - |  739 | `	}` |
|      109 |  740 | `	pGen->pIn++; /* Jump '(' */` |
|      109 |  741 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|      109 |  742 | `	if( pSubjEnd >= pGen->pEnd ){` |
|      ! 0 |  743 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  744 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|        - |  745 | `	}` |
|      109 |  746 | `	if( pGen->pIn >= pSubjEnd ){` |
|      ! 0 |  747 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  748 | `			"syntax error, unexpected \")\", expecting match subject");` |
|        - |  749 | `	}` |
|        - |  750 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|      109 |  751 | `	pSavedEnd = pGen->pEnd;` |
|      109 |  752 | `	pGen->pEnd = pSubjEnd;` |
|      109 |  753 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      109 |  754 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  755 | `		return SXERR_ABORT;` |
|        - |  756 | `	}` |
|      109 |  757 | `	pGen->pEnd = pSavedEnd;` |
|      109 |  758 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|        - |  759 | `	/* Expect '{' */` |
|      109 |  760 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|      ! 0 |  761 | `		return GenStateMatchError(pGen,` |
|      ! 0 |  762 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|        - |  763 | `			"syntax error, expecting \"{\" after match subject");` |
|        - |  764 | `	}` |
|      109 |  765 | `	pGen->pIn++; /* Jump '{' */` |
|      109 |  766 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|      109 |  767 | `	if( pBodyEnd >= pGen->pEnd ){` |
|      ! 0 |  768 | `		return GenStateMatchError(pGen,nLine,` |
|        - |  769 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|        - |  770 | `	}` |
|        - |  771 | `	/* Allocate ph7_match container */` |
|      109 |  772 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|      109 |  773 | `	if( pMatch == 0 ){` |
|      ! 0 |  774 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  775 | `			"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  776 | `		return SXERR_ABORT;` |
|        - |  777 | `	}` |
|      109 |  778 | `	SyZero(pMatch,sizeof(ph7_match));` |
|      109 |  779 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|        - |  780 | `	/* Iterate arms */` |
|      353 |  781 | `	while( pGen->pIn < pBodyEnd ){` |
|        - |  782 | `		ph7_match_arm sArm;` |
|        - |  783 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|      252 |  784 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|      252 |  785 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|      252 |  786 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|      252 |  787 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - |  788 | `		/* 'default' arm? */` |
|      248 |  789 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      148 |  790 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|       44 |  791 | `			if( bHasDefault ){` |
|        3 |  792 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|        - |  793 | `					"Match expressions may only contain one default arm");` |
|        4 |  794 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - |  795 | `			}` |
|       42 |  796 | `			sArm.bDefault = 1;` |
|       42 |  797 | `			bHasDefault = 1;` |
|       42 |  798 | `			pGen->pIn++;` |
|       42 |  799 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|      ! 0 |  800 | `				return GenStateMatchError(pGen,nArmLine,` |
|        - |  801 | `					"syntax error, expecting \"=>\" after 'default'");` |
|        - |  802 | `			}` |
|       42 |  803 | `			pGen->pIn++; /* Jump '=>' */` |
|       23 |  804 | `		}else{` |
|        - |  805 | `			/* Condition list: cond (',' cond)* '=>' */` |
|      212 |  806 | `			pCondStart = pGen->pIn;` |
|      212 |  807 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|        - |  808 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|      220 |  809 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|        - |  810 | `				SySet sCondBc;` |
|        9 |  811 | `				if( pCondStart >= pArrow ){` |
|      ! 0 |  812 | `					return GenStateMatchError(pGen,nArmLine,` |
|        - |  813 | `						"syntax error, empty match condition expression");` |
|        - |  814 | `				}` |
|        9 |  815 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        9 |  816 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|        9 |  817 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  818 | `					return SXERR_ABORT;` |
|        - |  819 | `				}` |
|        9 |  820 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|        9 |  821 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|        9 |  822 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|        - |  823 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|        1 |  824 | `			}` |
|      212 |  825 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|        3 |  826 | `				return GenStateMatchError(pGen,nArmLine,` |
|        - |  827 | `					"syntax error, expecting \"=>\" in match arm");` |
|        - |  828 | `			}` |
|      210 |  829 | `			if( pCondStart >= pArrow ){` |
|      ! 0 |  830 | `				return GenStateMatchError(pGen,nArmLine,` |
|        - |  831 | `					"syntax error, empty match condition expression");` |
|        - |  832 | `			}` |
|        - |  833 | `			{` |
|        - |  834 | `				SySet sCondBc;` |
|      210 |  835 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      210 |  836 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|      210 |  837 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  838 | `					return SXERR_ABORT;` |
|        - |  839 | `				}` |
|      210 |  840 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|        - |  841 | `			}` |
|      210 |  842 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|        - |  843 | `		}` |
|        - |  844 | `		/* Compile result expression: up to top-level ',' or body end */` |
|      248 |  845 | `		pResStart = pGen->pIn;` |
|      248 |  846 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|      248 |  847 | `		if( pResStart >= pResEnd ){` |
|      ! 0 |  848 | `			return GenStateMatchError(pGen,nArmLine,` |
|        - |  849 | `				"syntax error, expected expression after \"=>\"");` |
|        - |  850 | `		}` |
|      248 |  851 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|      248 |  852 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  853 | `			return SXERR_ABORT;` |
|        - |  854 | `		}` |
|      248 |  855 | `		pGen->pIn = pResEnd;` |
|      248 |  856 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|      184 |  857 | `			pGen->pIn++; /* Skip trailing ',' */` |
|       90 |  858 | `		}` |
|      248 |  859 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|        4 |  860 | `	}` |
|      105 |  861 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|      105 |  862 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|      105 |  863 | `	return SXRET_OK;` |
|       57 |  864 | `}` |
|        - |  865 | `/*` |
|        - |  866 | ` * Compile a backtick quoted string.` |
|        - |  867 | ` */` |
|        2 |  868 | `PH7_PRIVATE sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        1 |  869 | `{` |
|        1 |  870 | `	SXUNUSED(iCompileFlag);` |
|        - |  871 | `	/*` |
|        - |  872 | ``	 * The backtick (`) operator was DEPRECATED by php (shell_exec() is the replacement).`` |
|        - |  873 | `	 * PHL targets php's *non-deprecated* surface, so it is a hard parse error — never` |
|        - |  874 | `	 * compiled to a shell_exec() call.` |
|        - |  875 | `	 */` |
|        3 |  876 | `	PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|        - |  877 | ``		"syntax error, the backtick (`) operator was removed, use shell_exec() instead");`` |
|        3 |  878 | `	return SXERR_ABORT;` |
|        1 |  879 | `}` |
|        - |  880 | `/*` |
|        - |  881 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|        - |  882 | ` * construct.` |
|        - |  883 | ` */` |
|       80 |  884 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  885 | `{` |
|        - |  886 | `	SyString *pName;` |
|        - |  887 | `	sxu32 nKeyID;` |
|        - |  888 | `	sxi32 rc;` |
|        - |  889 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|       85 |  890 | `	pName = &pGen->pIn->sData;` |
|       85 |  891 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       85 |  892 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|       85 |  893 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|        6 |  894 | `		SyToken *pTmp,*pNext = 0;` |
|        - |  895 | ``		/* A STATEMENT `echo` never reaches here — it dispatches through the statement`` |
|        - |  896 | `		 * table. Arriving in expression position means source like` |
|        - |  897 | ``		 * `fopen('f','r') or echo "IO error";`, which was a Symisc extension and is a`` |
|        - |  898 | `		 * php parse error (§10: a PH7-ism that changes the meaning of valid source is a` |
|        - |  899 | ``		 * bug). The one legitimate expression-echo is the token a `<?= ... ?>` short tag`` |
|        - |  900 | `		 * synthesizes, which raises nExprEchoOk around its own compile. */` |
|        6 |  901 | `		if( pGen->nExprEchoOk < 1 ){` |
|        3 |  902 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn - 1,0);` |
|        3 |  903 | `			return SXERR_ABORT;` |
|        - |  904 | `		}` |
|        - |  905 | `		/* Compile arguments one after one */` |
|        3 |  906 | `		pTmp = pGen->pEnd;` |
|        3 |  907 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|        5 |  908 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|        3 |  909 | `			if( pGen->pIn < pNext ){` |
|        3 |  910 | `				pGen->pEnd = pNext;` |
|        3 |  911 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|        3 |  912 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  913 | `					return SXERR_ABORT;` |
|        - |  914 | `				}` |
|        3 |  915 | `				if( rc != SXERR_EMPTY ){` |
|        - |  916 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|        - |  917 | `					 * without the overhead of a function call.` |
|        - |  918 | `					 * This is a very powerful optimization that improve` |
|        - |  919 | `					 * performance greatly.` |
|        - |  920 | `					 */` |
|        3 |  921 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|        1 |  922 | `				}` |
|        1 |  923 | `			}` |
|        - |  924 | `			/* Jump trailing commas */` |
|        3 |  925 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|      ! 0 |  926 | `				pNext++;` |
|      ! 0 |  927 | `			}` |
|        3 |  928 | `			pGen->pIn = pNext;` |
|        1 |  929 | `		}` |
|        - |  930 | `		/* Restore token stream */` |
|        3 |  931 | `		pGen->pEnd = pTmp;` |
|        2 |  932 | `	}else{` |
|       81 |  933 | `		sxi32 nArg = 0;` |
|       81 |  934 | `		sxu32 nIdx = 0;` |
|        - |  935 | `		char zCanon[sizeof("include_once")-1];` |
|        - |  936 | `		SyString sCanon;` |
|       81 |  937 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|       81 |  938 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  939 | `			return SXERR_ABORT;` |
|       81 |  940 | `		}else if(rc != SXERR_EMPTY ){` |
|       81 |  941 | `			nArg = 1;` |
|       38 |  942 | `		}` |
|        - |  943 | `		/* The construct is dispatched as a CALL to the host function of the same name,` |
|        - |  944 | `		 * so the name emitted here must be the construct's canonical spelling, not the` |
|        - |  945 | ``		 * source's: php accepts `PRINT`/`Isset`/`EVAL` (keywords are case-insensitive)`` |
|        - |  946 | ``		 * where the raw text produced `Call to undefined function PRINT()`. Every`` |
|        - |  947 | `		 * construct name is lower-case ASCII, so folding IS canonicalising. */` |
|       81 |  948 | `		if( pName->nByte <= sizeof(zCanon) ){` |
|        - |  949 | `			sxu32 i;` |
|      703 |  950 | `			for( i = 0 ; i < pName->nByte ; ++i ){` |
|      627 |  951 | `				unsigned char c = (unsigned char)pName->zString[i];` |
|      627 |  952 | `				zCanon[i] = (char)((c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c);` |
|      316 |  953 | `			}` |
|       81 |  954 | `			SyStringInitFromBuf(&sCanon,zCanon,pName->nByte);` |
|       81 |  955 | `			pName = &sCanon;` |
|       38 |  956 | `		}` |
|       81 |  957 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|        - |  958 | `			ph7_value *pObj;` |
|        - |  959 | `			/* Emit the call instruction */` |
|       41 |  960 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       41 |  961 | `			if( pObj == 0 ){` |
|      ! 0 |  962 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  963 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 |  964 | `				return SXERR_ABORT;` |
|        - |  965 | `			}` |
|       41 |  966 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|        - |  967 | `			/* Install in the literal table */` |
|       41 |  968 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       18 |  969 | `		}` |
|        - |  970 | `		/* Emit the call instruction */` |
|       81 |  971 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       81 |  972 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        - |  973 | `	}` |
|        - |  974 | `	/* Node successfully compiled */` |
|       83 |  975 | `	return SXRET_OK;` |
|       45 |  976 | `}` |
|        - |  977 | `/*` |
|        - |  978 | ` * Compile a node holding a variable declaration.` |
|        - |  979 | ` * According to the PHP language reference` |
|        - |  980 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|        - |  981 | ` *  The variable name is case-sensitive.` |
|        - |  982 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|        - |  983 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|        - |  984 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|        - |  985 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|        - |  986 | ` *  Note: $this is a special variable that can't be assigned.` |
|        - |  987 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|        - |  988 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|        - |  989 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|        - |  990 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|        - |  991 | ` *  the chapter on Expressions.` |
|        - |  992 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|        - |  993 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|        - |  994 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|        - |  995 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|        - |  996 | ` *  is being assigned (the source variable).` |
|        - |  997 | ` */` |
| 25365160 |  998 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 |  999 | `{` |
| 25365165 | 1000 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|        - | 1001 | `	sxi32 iVv;` |
|        - | 1002 | `	sxi32 iP1;` |
| 25365165 | 1003 | `	sxi32 iP2 = 0; /* 1 = quiet read (isset/empty): a missing variable must not warn */` |
|        - | 1004 | `	void *p3;` |
|        - | 1005 | `	sxi32 rc;` |
| 25365165 | 1006 | `	iVv = -1; /* Variable variable counter */` |
| 50730359 | 1007 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
| 25365199 | 1008 | `		pGen->pIn++;` |
| 25365199 | 1009 | `		iVv++;` |
|        5 | 1010 | `	}` |
| 25365165 | 1011 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|        - | 1012 | `		/* Invalid variable name */` |
|      ! 0 | 1013 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable or \"{\" or \"$\"");` |
|      ! 0 | 1014 | `		if( rc == SXERR_ABORT ){` |
|        - | 1015 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1016 | `			return SXERR_ABORT;` |
|        - | 1017 | `		}` |
|      ! 0 | 1018 | `		return SXRET_OK;` |
|        - | 1019 | `	}` |
| 25365165 | 1020 | `	p3  = 0;` |
| 25365165 | 1021 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|        - | 1022 | `		/* Dynamic variable creation */` |
|       30 | 1023 | `		pGen->pIn++;  /* Jump the open curly */` |
|       30 | 1024 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|       30 | 1025 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 1026 | `			/* Empty expression */` |
|        - | 1027 | `			{` |
|        - | 1028 | `			/* php names the offending token and, for an empty "${}", stops there:` |
|        - | 1029 | `			 * the "expecting" tail only appears when something could still follow. */` |
|        - | 1030 | ``			/* `${}`: pEnd was stepped back past the trailing '}', so the token php`` |
|        - | 1031 | `			 * names sits AT pEnd. Reach for it before deciding the tail -- php stops` |
|        - | 1032 | `			 * at "unexpected token \"}\"" with no "expecting" clause, which the` |
|        - | 1033 | `			 * NULL-token path could not express because it never saw the '}'. */` |
|        3 | 1034 | `			SyToken *pBad = pGen->pIn < pGen->pEnd ? pGen->pIn : 0;` |
|        3 | 1035 | `			if( pBad == 0 && pGen->pTokenSet ){` |
|        3 | 1036 | `				SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        3 | 1037 | `				SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|        3 | 1038 | `				if( pGen->pEnd >= pBase && pGen->pEnd < pStreamEnd ){` |
|        3 | 1039 | `					pBad = pGen->pEnd;` |
|        1 | 1040 | `				}` |
|        1 | 1041 | `			}` |
|        5 | 1042 | `			PH7_GenSyntaxError(&(*pGen),pBad,` |
|        2 | 1043 | `				(pBad && (pBad->nType & PH7_TK_CCB)) ? 0 : "variable or \"{\" or \"$\"");` |
|        - | 1044 | `			}` |
|        3 | 1045 | `			return SXRET_OK;` |
|        - | 1046 | `		}` |
|        - | 1047 | `		/* Compile the expression holding the variable name. It is a pure READ, so` |
|        - | 1048 | ``		 * compile it read-only: `${$u}` warns on an undefined $u (php) instead of`` |
|        - | 1049 | ``		 * silently creating it, matching the `$$u` name-read path below. A quiet`` |
|        - | 1050 | `		 * outer (isset()/empty()) suppresses that name warning too, so carry the` |
|        - | 1051 | `		 * quiet flag into the name expression. */` |
|       27 | 1052 | `		sxi32 iNameFlags = EXPR_FLAG_RDONLY_LOAD;` |
|       27 | 1053 | `		if( iCompileFlag & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY) ){` |
|        - | 1054 | ``			/* isset()/empty() suppress the name warning; `??` (EXPR_FLAG_QUIET_VAR)`` |
|        - | 1055 | ``			 * does NOT — php warns `Undefined variable $u` for `${$u} ?? x` and only`` |
|        - | 1056 | `			 * quiets the TARGET read, so QUIET_VAR is deliberately excluded here. */` |
|        3 | 1057 | `			iNameFlags \|= EXPR_FLAG_QUIET_VAR;` |
|        1 | 1058 | `		}` |
|       27 | 1059 | `		rc = PH7_CompileExpr(&(*pGen),iNameFlags,0);` |
|       27 | 1060 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1061 | `			return SXERR_ABORT;` |
|       27 | 1062 | `		}else if( rc == SXERR_EMPTY ){` |
|        3 | 1063 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,0);` |
|        3 | 1064 | `			return SXRET_OK;` |
|        - | 1065 | `		}` |
|       14 | 1066 | `	}else{` |
|        - | 1067 | `		SyHashEntry *pEntry;` |
|        - | 1068 | `		SyString *pName;` |
| 25365139 | 1069 | `		char *zName = 0;` |
|        - | 1070 | `		/* Extract variable name */` |
| 25365139 | 1071 | `		pName = &pGen->pIn->sData;` |
|        - | 1072 | `		/* Advance the stream cursor */` |
| 25365139 | 1073 | `		pGen->pIn++;` |
| 25365139 | 1074 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
| 25365139 | 1075 | `		if( pEntry == 0 ){` |
|        - | 1076 | `			/* Duplicate name */` |
|  1502569 | 1077 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  1502569 | 1078 | `			if( zName == 0 ){` |
|      ! 0 | 1079 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1080 | `				return SXERR_ABORT;` |
|        - | 1081 | `			}` |
|        - | 1082 | `			/* Install in the hashtable */` |
|  1502569 | 1083 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|   751287 | 1084 | `		}else{` |
|        - | 1085 | `			/* Name already available */` |
| 23862575 | 1086 | `			zName = (char *)pEntry->pUserData;` |
|        - | 1087 | `		}` |
| 25365139 | 1088 | `		p3 = (void *)zName;` |
|        - | 1089 | `	}` |
| 25365161 | 1090 | `	iP1 = 0;` |
| 25365161 | 1091 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
| 24511013 | 1092 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|        - | 1093 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
| 18133275 | 1094 | `			iP1 = 1;` |
|  9066635 | 1095 | `		}` |
| 12255504 | 1096 | `	}` |
|        - | 1097 | ``	/* iP2 marks a QUIET read: `isset($x)` / `empty($x)` inspect a variable`` |
|        - | 1098 | `	 * without reading it, so an undefined one must not warn (php stays silent` |
|        - | 1099 | `	 * for both). Every other read of a missing variable warns — see OP_LOAD.` |
|        - | 1100 | `	 * The two flags are cleared before recursing into a subscript's index` |
|        - | 1101 | ``	 * expression, so `isset($a[$i])` still warns for an undefined $i, as php`` |
|        - | 1102 | ``	 * does. Contexts that VIVIFY (assignment targets, `??`, appends) already`` |
|        - | 1103 | `	 * emit iP1 = 0 and never reach the warning. */` |
| 25365161 | 1104 | `	if( iCompileFlag & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_QUIET_VAR) ){` |
|   449193 | 1105 | `		iP2 = 1;` |
| 25140567 | 1106 | `	}else if( iCompileFlag & EXPR_FLAG_RMW_LOAD ){` |
|        - | 1107 | ``		/* Warn-then-create: the read half of `$x++` / `$x .= ...` still needs a`` |
|        - | 1108 | `		 * writable slot, so it cannot use the read-only load above. */` |
|   775337 | 1109 | `		iP2 = 2;` |
| 24528307 | 1110 | `	}else if( iCompileFlag & EXPR_FLAG_DEFER_ARG ){` |
|        - | 1111 | ``		/* D1 deferred call argument. For a plain `$var` (iVv == 0, p3 holds the name)`` |
|        - | 1112 | `		 * emit the deferred load (iP1 stays 1 = no create, iP2 = 3): an undefined` |
|        - | 1113 | `		 * variable is left uncreated and silent, carrying a lazy-lvalue marker that` |
|        - | 1114 | `		 * OP_CALL resolves against the callee's by-ref flags. A variable-variable` |
|        - | 1115 | `		 * ($$x) computes its name on the stack (p3 == 0), so it cannot carry the` |
|        - | 1116 | `		 * marker — fall back to the historical eager create (iP1 = 0), which keeps` |
|        - | 1117 | `		 * its by-ref binding working exactly as before. */` |
|  5922755 | 1118 | `		if( iVv == 0 ){` |
|  5922751 | 1119 | `			iP2 = 3;` |
|  2961378 | 1120 | `		}else{` |
|        6 | 1121 | `			iP1 = 0;` |
|        - | 1122 | `		}` |
|  2961375 | 1123 | `	}` |
|        - | 1124 | `	/* Emit the load instruction(s). For a variable-variable ($$x, $$$x, ...) every` |
|        - | 1125 | `	 * load EXCEPT the final dereference resolves a NAME: a pure read that warns on an` |
|        - | 1126 | `	 * undefined name (php) and never creates it. Only the last load is the actual` |
|        - | 1127 | `	 * variable and carries the caller's write/create context (iP1). Emitting the` |
|        - | 1128 | ``	 * outer create-mode for the name loads silently invented $n in `$$n = 5` and`` |
|        - | 1129 | ``	 * skipped php's `Undefined variable $n` warning; a quiet outer (isset/empty)`` |
|        - | 1130 | `	 * still suppresses the name warning as php does. */` |
| 25365161 | 1131 | `	if( iVv > 0 ){` |
|       36 | 1132 | `		sxi32 iP2Name = (iCompileFlag & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY))` |
|        - | 1133 | ``			? 1 /* isset()/empty() suppress the name warning too; `??` (QUIET_VAR)`` |
|       16 | 1134 | `			     * does NOT — it warns the name and quiets only the target read. */ : 0;` |
|       36 | 1135 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,1/* read-only name */,iP2Name,p3,0);` |
|       38 | 1136 | `		while( iVv > 1 ){` |
|        3 | 1137 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,1/* read-only name */,iP2Name,0,0);` |
|        3 | 1138 | `			iVv--;` |
|        1 | 1139 | `		}` |
|        - | 1140 | `		/* Final dereference: the actual variable, in the caller's context. */` |
|       36 | 1141 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,iP2,0,0);` |
|       20 | 1142 | `	}else{` |
| 25365129 | 1143 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,iP2,p3,0);` |
|        - | 1144 | `	}` |
|        - | 1145 | `	/* Node successfully compiled */` |
| 25365161 | 1146 | `	return SXRET_OK;` |
| 12682585 | 1147 | `}` |
|        - | 1148 | `/*` |
|        - | 1149 | ` * Load a literal.` |
|        - | 1150 | ` */` |
| 16559640 | 1151 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|        5 | 1152 | `{` |
| 16559645 | 1153 | `	SyToken *pToken = pGen->pIn;` |
|        - | 1154 | `	ph7_value *pObj;` |
|        - | 1155 | `	SyString *pStr;` |
|        - | 1156 | `	SyString sCanon;` |
|        - | 1157 | `	sxu32 nIdx;` |
|        - | 1158 | `	/* Extract token value */` |
| 16559645 | 1159 | `	pStr = &pToken->sData;` |
|        - | 1160 | `	/* php's MAGIC constants are case-insensitive like the rest of its reserved words —` |
|        - | 1161 | ``	 * `__line__`, `__Dir__` and `__CLASS__` are one constant each — but every one of them`` |
|        - | 1162 | `	 * is recognised by a BYTE-EXACT compare: the compile-time branches just below, and` |
|        - | 1163 | ``	 * `__CLASS__` through constant.c's (deliberately case-sensitive) constant table. Fold`` |
|        - | 1164 | `	 * the spelling to the canonical upper case here, once, so both mechanisms see it; any` |
|        - | 1165 | ``	 * other spelling used to reach the plain-literal path and raise `Undefined constant`` |
|        - | 1166 | ``	 * "__line__"`. Two of the branches below also read a single byte to tell a pair apart`` |
|        - | 1167 | ``	 * (`zString[2]` for __DIR__ vs __FILE__ and __METHOD__ vs __FUNCTION__), which only`` |
|        - | 1168 | `	 * works on the canonical form.` |
|        - | 1169 | `	 *` |
|        - | 1170 | `	 * User constants stay case-SENSITIVE (php) — this fold is limited to the eight names` |
|        - | 1171 | ``	 * below, and skips a member NAME, where `C::__LINE__` is an ordinary class constant. */`` |
| 16559640 | 1172 | `	if( (pToken->nType & PH7_TK_MEMBER_NAME) == 0 && pStr->nByte > 4` |
|  8660372 | 1173 | `		&& pStr->zString[0] == '_' && pStr->zString[1] == '_' ){` |
|        - | 1174 | `		static const char * const azMagic[] = {` |
|        - | 1175 | `			"__LINE__", "__FILE__", "__DIR__", "__FUNCTION__", "__CLASS__",` |
|        - | 1176 | `			"__METHOD__", "__NAMESPACE__", "__TRAIT__"` |
|        - | 1177 | `		};` |
|        - | 1178 | `		sxu32 i;` |
| 14686447 | 1179 | `		for( i = 0 ; i < SX_ARRAYSIZE(azMagic) ; ++i ){` |
| 13056247 | 1180 | `			sxu32 nMagic = SyStrlen(azMagic[i]);` |
| 13056247 | 1181 | `			if( pStr->nByte == nMagic && SyStrnicmp(pStr->zString,azMagic[i],nMagic) == 0 ){` |
|     9313 | 1182 | `				SyStringInitFromBuf(&sCanon,azMagic[i],nMagic);` |
|     9313 | 1183 | `				pStr = &sCanon;` |
|     9313 | 1184 | `				break;` |
|        - | 1185 | `			}` |
|  6523472 | 1186 | `		}` |
|   819754 | 1187 | `	}` |
|        - | 1188 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first. A reserved` |
|        - | 1189 | `	 * word used as a member NAME (Enum::Null, C::Array, $o->list()) is a plain` |
|        - | 1190 | `	 * identifier — the parser flagged its token so the whole value-folding chain is` |
|        - | 1191 | `	 * skipped and it falls through to the ordinary string-literal emit below. */` |
| 16559645 | 1192 | `	if( pToken->nType & PH7_TK_MEMBER_NAME ){` |
|        - | 1193 | `		/* fall through to the plain-string literal path */` |
| 13194876 | 1194 | `	}else if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  2001829 | 1195 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|        - | 1196 | `			/* NULL constant are always indexed at 0 */` |
|  1373193 | 1197 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|  1373193 | 1198 | `			return SXRET_OK;` |
|   628641 | 1199 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|        - | 1200 | `			/* TRUE constant are always indexed at 1 */` |
|   422883 | 1201 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|   422883 | 1202 | `			return SXRET_OK;` |
|        5 | 1203 | `		}` |
|  8857166 | 1204 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|  1852008 | 1205 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|        - | 1206 | `			/* FALSE constant are always indexed at 2 */` |
|   929103 | 1207 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|   929103 | 1208 | `			return SXRET_OK;` |
|  7086836 | 1209 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   375302 | 1210 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|        - | 1211 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|     4549 | 1212 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     4549 | 1213 | `			if( pObj == 0 ){` |
|      ! 0 | 1214 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1215 | `				return SXERR_ABORT;` |
|        - | 1216 | `			}` |
|     4549 | 1217 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|        - | 1218 | `			/* Emit the load constant instruction */` |
|     4549 | 1219 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     4549 | 1220 | `			return SXRET_OK;` |
|  7080015 | 1221 | `	}else if( (pStr->nByte == sizeof("__FILE__") - 1 &&` |
|   629048 | 1222 | `		SyMemcmp(pStr->zString,"__FILE__",sizeof("__FILE__")-1) == 0) \|\|` |
|  7148300 | 1223 | `		(pStr->nByte == sizeof("__DIR__") - 1 &&` |
|   516570 | 1224 | `		SyMemcmp(pStr->zString,"__DIR__",sizeof("__DIR__")-1) == 0) ){` |
|        - | 1225 | `			/* __FILE__ / __DIR__ are magic constants resolved at COMPILE time to the` |
|        - | 1226 | `			 * file being compiled (where the token is written), NOT the runtime` |
|        - | 1227 | `			 * execution file. A function defined in a.php reporting __FILE__ must say` |
|        - | 1228 | `			 * a.php even when called from b.php — php semantics, and what Composer's` |
|        - | 1229 | ``			 * autoloader (loadClassLoader's `require __DIR__ . '/ClassLoader.php'`)`` |
|        - | 1230 | `			 * relies on. The runtime-constant path returned the caller's file. */` |
|     4657 | 1231 | `			int bDir = (pStr->zString[2] == 'D'); /* __DIR__ vs __FILE__ */` |
|     4657 | 1232 | `			SyString *pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     4657 | 1233 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     4657 | 1234 | `			if( pObj == 0 ){` |
|      ! 0 | 1235 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1236 | `				return SXERR_ABORT;` |
|        - | 1237 | `			}` |
|     4657 | 1238 | `			if( pFile && pFile->nByte > 0 ){` |
|      129 | 1239 | `				if( bDir ){` |
|        - | 1240 | `					const char *zDir;` |
|        - | 1241 | `					int nLen;` |
|        - | 1242 | `					SyString sDir;` |
|       67 | 1243 | `					zDir = PH7_ExtractDirName(pFile->zString,(int)pFile->nByte,&nLen);` |
|       67 | 1244 | `					SyStringInitFromBuf(&sDir,zDir,nLen);` |
|       67 | 1245 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&sDir);` |
|       36 | 1246 | `				}else{` |
|       67 | 1247 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,pFile);` |
|        - | 1248 | `				}` |
|       67 | 1249 | `			}else{` |
|        - | 1250 | `				SyString sMem;` |
|     4533 | 1251 | `				SyStringInitFromBuf(&sMem,":MEMORY:",sizeof(":MEMORY:")-1);` |
|     4533 | 1252 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sMem);` |
|        - | 1253 | `			}` |
|     4657 | 1254 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     4657 | 1255 | `			return SXRET_OK;` |
|  7021744 | 1256 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   263510 | 1257 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|        - | 1258 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|       14 | 1259 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       14 | 1260 | `			if( pObj == 0 ){` |
|      ! 0 | 1261 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1262 | `				return SXERR_ABORT;` |
|        - | 1263 | `			}` |
|       14 | 1264 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        - | 1265 | `				SyString sNs;` |
|        7 | 1266 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        7 | 1267 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|        4 | 1268 | `			}else{` |
|        7 | 1269 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|        - | 1270 | `			}` |
|       14 | 1271 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       14 | 1272 | `			return SXRET_OK;` |
|  7239957 | 1273 | `	}else if( pStr->nByte == sizeof("__TRAIT__") - 1 &&` |
|   699960 | 1274 | `		SyMemcmp(pStr->zString,"__TRAIT__",sizeof("__TRAIT__")-1) == 0 ){` |
|        - | 1275 | `			/* __TRAIT__ magic constant: the name of the trait whose SOURCE lexically` |
|        - | 1276 | `			 * encloses this token. php resolves it at compile time, and it is shared` |
|        - | 1277 | `			 * across every using class because a trait method body compiles ONCE with` |
|        - | 1278 | `			 * the trait as its owner (PH7_ClassUseTrait adopts the same method pointer).` |
|        - | 1279 | `			 * Unlike __FUNCTION__/__METHOD__ (nearest function), __TRAIT__ is LEXICAL:` |
|        - | 1280 | `			 * closures and arrow-fns are TRANSPARENT (a closure inside a trait method still` |
|        - | 1281 | `			 * yields the trait), so we skip them and keep walking outward — but a class` |
|        - | 1282 | `			 * method or a plain function is an OPAQUE lexical boundary that fixes the answer.` |
|        - | 1283 | `			 * An anonymous class defined inside a trait method is a fresh scope, so` |
|        - | 1284 | `			 * __TRAIT__ is "" there, not the enclosing trait. "" outside any trait (global` |
|        - | 1285 | `			 * scope, plain functions, non-trait methods) — php renders it the empty string,` |
|        - | 1286 | `			 * not NULL. */` |
|       54 | 1287 | `			ph7_class *pTrait = 0;` |
|       54 | 1288 | `			if( pGen->iInMemberDefault > 0 ){` |
|        - | 1289 | `				/* A property/parameter DEFAULT is a const-expression that belongs to the` |
|        - | 1290 | `				 * class whose body is being compiled (pCurClass), never to a lexically-` |
|        - | 1291 | `				 * enclosing method. Read pCurClass directly — the block chain has no func` |
|        - | 1292 | `				 * block for the default and would leak into the enclosing function (an` |
|        - | 1293 | `				 * anonymous class's default inside a trait method is the anon's scope, "").*/` |
|       13 | 1294 | `				if( pGen->pCurClass && (pGen->pCurClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        5 | 1295 | `					pTrait = pGen->pCurClass;` |
|        2 | 1296 | `				}` |
|        7 | 1297 | `			}else{` |
|       42 | 1298 | `				GenBlock *pBlock = pGen->pCurrent;` |
|      102 | 1299 | `				while( pBlock ){` |
|       98 | 1300 | `					if( pBlock->iFlags & GEN_BLOCK_FUNC ){` |
|       53 | 1301 | `						ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       53 | 1302 | `						if( pFunc == 0 ){` |
|        - | 1303 | `							/* A SYNTHETIC function block carries no ph7_vm_func — e.g. the` |
|        - | 1304 | `							 * per-arm throw-fixup block GenStateCompileMatchSubExpr enters to` |
|        - | 1305 | `							 * host a match() expression. It is not a real lexical scope` |
|        - | 1306 | `							 * boundary, so stay transparent and keep walking outward. */` |
|        6 | 1307 | `							pBlock = pBlock->pParent;` |
|        6 | 1308 | `							continue;` |
|        - | 1309 | `						}` |
|       49 | 1310 | `						if( pFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - | 1311 | `							/* A class method (of any class, including an anonymous one) is an` |
|        - | 1312 | `							 * OPAQUE scope boundary and fixes the answer: a trait method yields` |
|        - | 1313 | `							 * its trait, any other class's method yields "". Tested BEFORE the` |
|        - | 1314 | `							 * closure flags so a static method is never mistaken for transparent. */` |
|       32 | 1315 | `							if( pFunc->pUserData` |
|       35 | 1316 | `								&& (((ph7_class *)pFunc->pUserData)->iFlags & PH7_CLASS_TRAIT) ){` |
|       29 | 1317 | `								pTrait = (ph7_class *)pFunc->pUserData;` |
|       13 | 1318 | `							}` |
|       35 | 1319 | `							break;` |
|        - | 1320 | `						}` |
|       15 | 1321 | `						if( pFunc->iFlags & (VM_FUNC_CLOSURE\|VM_FUNC_ARROW\|VM_FUNC_STATIC_CL) ){` |
|        - | 1322 | `							/* Closure / arrow fn (VM_FUNC_CLOSURE is only set when the closure` |
|        - | 1323 | `							 * captures, so a capture-less static closure carries only` |
|        - | 1324 | `							 * VM_FUNC_STATIC_CL — include it). Transparent: keep walking outward. */` |
|       13 | 1325 | `							pBlock = pBlock->pParent;` |
|       13 | 1326 | `							continue;` |
|        - | 1327 | `						}` |
|        - | 1328 | `						/* A plain named function is an opaque boundary: __TRAIT__ is "". */` |
|        3 | 1329 | `						break;` |
|        - | 1330 | `					}` |
|       48 | 1331 | `					pBlock = pBlock->pParent;` |
|        4 | 1332 | `				}` |
|        - | 1333 | `			}` |
|       54 | 1334 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       54 | 1335 | `			if( pObj == 0 ){` |
|      ! 0 | 1336 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1337 | `				return SXERR_ABORT;` |
|        - | 1338 | `			}` |
|       54 | 1339 | `			if( pTrait ){` |
|       34 | 1340 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&pTrait->sName);` |
|       19 | 1341 | `			}else{` |
|       22 | 1342 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0); /* empty string */` |
|        - | 1343 | `			}` |
|       54 | 1344 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       54 | 1345 | `			return SXRET_OK;` |
|  7046828 | 1346 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   514441 | 1347 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  7090514 | 1348 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   401248 | 1349 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|       45 | 1350 | `			GenBlock *pBlock = pGen->pCurrent;` |
|        - | 1351 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|        - | 1352 | `			/* Skip SYNTHETIC function blocks (GEN_BLOCK_FUNC with no ph7_vm_func in` |
|        - | 1353 | `			 * pUserData — e.g. the per-arm throw-fixup block a match() expression enters):` |
|        - | 1354 | `			 * they are not real function scopes. Without this a __FUNCTION__/__METHOD__` |
|        - | 1355 | `			 * inside a match arm reached a NULL pUserData and dereferenced it (compile-time` |
|        - | 1356 | `			 * crash); php resolves to the enclosing real function, which the walk now finds. */` |
|      114 | 1357 | `			while( pBlock && ((pBlock->iFlags & GEN_BLOCK_FUNC) == 0 \|\| pBlock->pUserData == 0) ){` |
|        - | 1358 | `				/* Point to the upper block */` |
|       51 | 1359 | `				pBlock = pBlock->pParent;` |
|        3 | 1360 | `			}` |
|       45 | 1361 | `			if( pBlock == 0 ){` |
|        - | 1362 | `				/* Called in the global scope,load NULL */` |
|        5 | 1363 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|        3 | 1364 | `			}else{` |
|        - | 1365 | `				/* Extract the target function/method */` |
|       41 | 1366 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|       41 | 1367 | `				int bMethod = (pStr->zString[2] == 'M'); /* __METHOD__ vs __FUNCTION__ */` |
|       41 | 1368 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       41 | 1369 | `				if( pObj == 0 ){` |
|      ! 0 | 1370 | `					PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1371 | `					return SXERR_ABORT;` |
|        - | 1372 | `				}` |
|        - | 1373 | `				/*` |
|        - | 1374 | `				 * __METHOD__ is the QUALIFIED name: "C::m" inside a method, and the plain` |
|        - | 1375 | `				 * function name inside a plain function (php does not answer "" there —` |
|        - | 1376 | `				 * PH7 loaded NULL, so __METHOD__ was empty in every free function and` |
|        - | 1377 | `				 * unqualified in every method).` |
|        - | 1378 | `				 */` |
|        - | 1379 | ``				/* A property hook answers php's `$p::get`, never the method name`` |
|        - | 1380 | ``				 * PHL synthesizes for it — so __METHOD__ reads `C::$p::get`, the`` |
|        - | 1381 | `				 * same rendering the runtime diagnostics use. */` |
|        - | 1382 | `				{` |
|        - | 1383 | `					SyString sProp,sSelf;` |
|        - | 1384 | `					const char *zKind;` |
|        - | 1385 | `					SyBlob sQual;` |
|        - | 1386 | `					SyString sOut;` |
|       41 | 1387 | `					int bHook = PH7_VmHookSplitName(&pFunc->sName,&sProp,&zKind);` |
|       41 | 1388 | `					SyBlobInit(&sQual,&pGen->pVm->sAllocator);` |
|       41 | 1389 | `					if( bHook ){` |
|        7 | 1390 | `						SyBlobFormat(&sQual,"$%z::%s",&sProp,zKind);` |
|        7 | 1391 | `						SyStringInitFromBuf(&sSelf,SyBlobData(&sQual),SyBlobLength(&sQual));` |
|        4 | 1392 | `					}else{` |
|       35 | 1393 | `						sSelf = pFunc->sName;` |
|        - | 1394 | `					}` |
|       48 | 1395 | `					if( bMethod && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|       17 | 1396 | `						SyString *pCls = &((ph7_class *)pFunc->pUserData)->sName;` |
|        - | 1397 | `						SyBlob sFull;` |
|       17 | 1398 | `						SyBlobInit(&sFull,&pGen->pVm->sAllocator);` |
|       17 | 1399 | `						SyBlobFormat(&sFull,"%z::%z",pCls,&sSelf);` |
|       17 | 1400 | `						SyStringInitFromBuf(&sOut,SyBlobData(&sFull),SyBlobLength(&sFull));` |
|       17 | 1401 | `						PH7_MemObjInitFromString(pGen->pVm,pObj,&sOut);` |
|       17 | 1402 | `						SyBlobRelease(&sFull);` |
|       10 | 1403 | `					}else{` |
|       27 | 1404 | `						PH7_MemObjInitFromString(pGen->pVm,pObj,&sSelf);` |
|        - | 1405 | `					}` |
|       41 | 1406 | `					SyBlobRelease(&sQual);` |
|        - | 1407 | `				}` |
|        - | 1408 | `				/* Emit the load constant instruction */` |
|       41 | 1409 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|        - | 1410 | `			}` |
|       45 | 1411 | `			return SXRET_OK;` |
|        - | 1412 | `	}` |
|        - | 1413 | ``	/* php keywords are CASE-INSENSITIVE (`SELF::C`, `Parent::m()`, `new STATIC`,`` |
|        - | 1414 | ``	 * `ISSET($x)`), but a few of them reach the engine as THIS literal and are matched`` |
|        - | 1415 | `	 * there BYTE-EXACTLY: the scope keywords against "self"/"parent"/"static" (OP_MEMBER,` |
|        - | 1416 | `	 * OP_NEW, the FCC scope resolver, the error formatter), and isset/empty/eval as the` |
|        - | 1417 | `	 * name of the host function their call dispatches to. Emit the canonical lower-case` |
|        - | 1418 | `	 * spelling for exactly those so the source's case never reaches the match — PH7` |
|        - | 1419 | ``	 * emitted the raw text, so `SELF::C` looked for a class literally named "SELF" and`` |
|        - | 1420 | ``	 * `ISSET($x)` for a function named "ISSET".`` |
|        - | 1421 | `	 *` |
|        - | 1422 | `	 * Every OTHER keyword literal keeps its source case on purpose: it is a CONSTANT` |
|        - | 1423 | ``	 * read (`define('OBJECT',1); echo OBJECT;` — PHL's keyword table covers type names`` |
|        - | 1424 | `	 * php's lexer does not reserve), and php constants are case-SENSITIVE. So is a` |
|        - | 1425 | ``	 * keyword used as a member NAME, which the parser flags: `class C { const STATIC = 5; }`` |
|        - | 1426 | ``	 * echo C::STATIC;` names the constant "STATIC", and folding it looked for "static". */`` |
| 13825181 | 1427 | `	if( (pToken->nType & PH7_TK_KEYWORD) && (pToken->nType & PH7_TK_MEMBER_NAME) == 0 ){` |
|   562487 | 1428 | `		sxu32 nKeyID = (sxu32)SX_PTR_TO_INT(pToken->pUserData);` |
|   562487 | 1429 | `		const char *zCanon = 0;` |
|   562487 | 1430 | `		if( nKeyID == PH7_TKWRD_SELF ){` |
|   117883 | 1431 | `			zCanon = "self";` |
|   503548 | 1432 | `		}else if( nKeyID == PH7_TKWRD_PARENT ){` |
|    54397 | 1433 | `			zCanon = "parent";` |
|   417413 | 1434 | `		}else if( nKeyID == PH7_TKWRD_STATIC ){` |
|     9129 | 1435 | `			zCanon = "static";` |
|   385655 | 1436 | `		}else if( nKeyID == PH7_TKWRD_ISSET ){` |
|   380867 | 1437 | `			zCanon = "isset";` |
|   190662 | 1438 | `		}else if( nKeyID == PH7_TKWRD_EMPTY ){` |
|      149 | 1439 | `			zCanon = "empty";` |
|      159 | 1440 | `		}else if( nKeyID == PH7_TKWRD_EVAL ){` |
|       83 | 1441 | `			zCanon = "eval";` |
|       39 | 1442 | `		}` |
|   562487 | 1443 | `		if( zCanon ){` |
|   562483 | 1444 | `			SyStringInitFromBuf(&sCanon,zCanon,SyStrlen(zCanon));` |
|   562483 | 1445 | `			pStr = &sCanon;` |
|   281239 | 1446 | `		}` |
|   281241 | 1447 | `	}` |
|        - | 1448 | `	/* Query literal table */` |
| 13825181 | 1449 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|        - | 1450 | `		ph7_value *pLitObj;` |
|        - | 1451 | `		/* Unknown literal,install it in the literal table */` |
|  2771155 | 1452 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  2771155 | 1453 | `		if( pLitObj == 0 ){` |
|      ! 0 | 1454 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 | 1455 | `			return SXERR_ABORT;` |
|        - | 1456 | `		}` |
|  2771155 | 1457 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,pStr);` |
|  2771155 | 1458 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  1385575 | 1459 | `	}` |
|        - | 1460 | `	/* Emit the load constant instruction.` |
|        - | 1461 | `	 *` |
|        - | 1462 | `	 * php resolves an UNQUALIFIED constant against the namespace its SOURCE sits in,` |
|        - | 1463 | `	 * decided at COMPILE time — so a function keeps its own namespace when called from` |
|        - | 1464 | ``	 * another one — and in a fixed order: a `use const` import first (and an import has`` |
|        - | 1465 | ``	 * NO global fallback), else `current-namespace\NAME`, else the global `NAME`. The`` |
|        - | 1466 | `	 * candidate that order picks is resolved here and travels in the instruction's p3;` |
|        - | 1467 | `	 * the VM's own lookup of the bare literal is then just the global step, and the` |
|        - | 1468 | `	 * "Undefined constant" message names the candidate, as php does.` |
|        - | 1469 | `	 *` |
|        - | 1470 | `	 * Only a name that could BE a constant needs one: a keyword literal never is (the` |
|        - | 1471 | `	 * scope keywords and isset/empty/eval reach the OO and call handlers by this same` |
|        - | 1472 | `	 * literal), and a call/new site clears PH7_LOADC_EXPAND before the constant path` |
|        - | 1473 | `	 * can ever run. */` |
|        - | 1474 | `	{` |
| 13825181 | 1475 | `		sxi32 iLoadFlags = PH7_LOADC_EXPAND;` |
| 13825181 | 1476 | `		char *zCand = 0;` |
| 13825181 | 1477 | `		if( (pToken->nType & PH7_TK_KEYWORD) == 0 ){` |
| 19655906 | 1478 | `			SyHashEntry *pImport = SyHashGet(&pGen->hUseConstImports,` |
| 13103934 | 1479 | `				(const void *)pStr->zString,pStr->nByte);` |
| 13103939 | 1480 | `			if( pImport ){` |
|       31 | 1481 | `				const char *zFQN = (const char *)pImport->pUserData;` |
|       31 | 1482 | `				zCand = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFQN,SyStrlen(zFQN));` |
|       31 | 1483 | `				iLoadFlags \|= PH7_LOADC_NOGLOBAL;` |
| 13103926 | 1484 | `			}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        - | 1485 | `				SyBlob sCand;` |
|      433 | 1486 | `				SyBlobInit(&sCand,&pGen->pVm->sAllocator);` |
|      433 | 1487 | `				SyBlobAppend(&sCand,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      433 | 1488 | `				SyBlobAppend(&sCand,"\\",1);` |
|      433 | 1489 | `				SyBlobAppend(&sCand,pStr->zString,pStr->nByte);` |
|      647 | 1490 | `				zCand = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      428 | 1491 | `					(const char *)SyBlobData(&sCand),SyBlobLength(&sCand));` |
|      433 | 1492 | `				SyBlobRelease(&sCand);` |
|      214 | 1493 | `			}` |
|  6551967 | 1494 | `		}` |
| 13825181 | 1495 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,iLoadFlags,nIdx,zCand,0);` |
|        - | 1496 | `	}` |
| 13825181 | 1497 | `	return SXRET_OK;` |
|  8279825 | 1498 | `}` |
|        - | 1499 | `/*` |
|        - | 1500 | ` * Resolve a namespace path or simply load a literal.` |
|        - | 1501 | ` * If the token stream contains namespace separators (backslashes),` |
|        - | 1502 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|        - | 1503 | ` * Otherwise, load the simple literal directly.` |
|        - | 1504 | ` */` |
| 16564358 | 1505 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|        5 | 1506 | `{` |
|        - | 1507 | `	sxi32 rc;` |
| 16564363 | 1508 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 1509 | `		return SXRET_OK;` |
|        - | 1510 | `	}` |
|        - | 1511 | `	/* Check if this is a multi-token namespace path */` |
| 16564363 | 1512 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|        - | 1513 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|     4723 | 1514 | `		SyBlob *pWorker = &pGen->sWorker;` |
|     4723 | 1515 | `		int isAbsolute = 0;` |
|     4723 | 1516 | `		SyBlobReset(pWorker);` |
|        - | 1517 | `		/* Check for leading backslash (absolute path) */` |
|     4723 | 1518 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|     4631 | 1519 | `			isAbsolute = 1;` |
|     4631 | 1520 | `			pGen->pIn++; /* Skip leading backslash */` |
|     2313 | 1521 | `		}` |
|        - | 1522 | `		/* Collect the raw path (no prefix yet) so its FIRST segment can be resolved` |
|        - | 1523 | ``		 * against use-imports below — php resolves `A\B\C` by mapping the leading`` |
|        - | 1524 | ``		 * `A` through the imports (`use X\A;` makes it `X\A\B\C`), and only prepends`` |
|        - | 1525 | ``		 * the current namespace when `A` matches no import. Blindly prefixing the`` |
|        - | 1526 | ``		 * namespace here produced e.g. `Ns\Ev\Bus` for `use ...\Event as Ev; Ev\Bus`. */`` |
|        - | 1527 | `		{` |
|        - | 1528 | `			SyBlob sRaw;` |
|     4723 | 1529 | `			SyBlobInit(&sRaw,&pGen->pVm->sAllocator);` |
|        - | 1530 | ``			/* `namespace\X` spells the CURRENT namespace and is FULLY QUALIFIED from`` |
|        - | 1531 | `			 * there: no import ever applies to it, and the namespace is already in. */` |
|     4723 | 1532 | `			if( !isAbsolute && GenStateNsRelPrefix(pGen,&pGen->pIn,pGen->pEnd,&sRaw) ){` |
|       66 | 1533 | `				isAbsolute = 1;` |
|       32 | 1534 | `			}` |
|     4939 | 1535 | `			while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|     4939 | 1536 | `				if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|      113 | 1537 | `					SyBlobAppend(&sRaw,"\\",1);` |
|       59 | 1538 | `				}else{` |
|     4831 | 1539 | `					SyBlobAppend(&sRaw,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        - | 1540 | `				}` |
|     4939 | 1541 | `				if( pGen->pIn == &pGen->pEnd[-1] ){` |
|     4723 | 1542 | `					pGen->pIn++;` |
|     4723 | 1543 | `					break;` |
|        - | 1544 | `				}` |
|      221 | 1545 | `				pGen->pIn++;` |
|        5 | 1546 | `			}` |
|     4723 | 1547 | `			if( isAbsolute ){` |
|     4695 | 1548 | `				SyBlobAppend(pWorker,SyBlobData(&sRaw),SyBlobLength(&sRaw)); /* FQN as written */` |
|     2350 | 1549 | `			}else{` |
|       31 | 1550 | `				const char *zRaw = (const char *)SyBlobData(&sRaw);` |
|       31 | 1551 | `				sxu32 nRaw = SyBlobLength(&sRaw);` |
|       31 | 1552 | `				sxu32 nFirst = 0;` |
|        - | 1553 | `				SyHashEntry *pNsImp;` |
|      125 | 1554 | `				while( nFirst < nRaw && zRaw[nFirst] != '\\' ){ nFirst++; }` |
|       31 | 1555 | `				pNsImp = SyHashGet(&pGen->hUseImports,(const void *)zRaw,nFirst);` |
|       31 | 1556 | `				if( pNsImp ){` |
|        - | 1557 | `					/* Leading segment is an imported alias: substitute its FQN. */` |
|       26 | 1558 | `					const char *zFQN = (const char *)pNsImp->pUserData;` |
|       26 | 1559 | `					SyBlobAppend(pWorker,zFQN,SyStrlen(zFQN));` |
|       26 | 1560 | `					SyBlobAppend(pWorker,zRaw + nFirst,nRaw - nFirst);` |
|       18 | 1561 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        6 | 1562 | `					SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        6 | 1563 | `					SyBlobAppend(pWorker,"\\",1);` |
|        6 | 1564 | `					SyBlobAppend(pWorker,zRaw,nRaw);` |
|        4 | 1565 | `				}else{` |
|      ! 0 | 1566 | `					SyBlobAppend(pWorker,zRaw,nRaw); /* global scope, no import */` |
|        - | 1567 | `				}` |
|        - | 1568 | `			}` |
|     4723 | 1569 | `			SyBlobRelease(&sRaw);` |
|        - | 1570 | `		}` |
|     4723 | 1571 | `		if( SyBlobLength(pWorker) > 0 ){` |
|        - | 1572 | `			ph7_value *pObj;` |
|        - | 1573 | `			SyString sPath;` |
|        - | 1574 | `			sxu32 nIdx;` |
|     4723 | 1575 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|        - | 1576 | `			/* Install in the literal table */` |
|     4723 | 1577 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|     4625 | 1578 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     4625 | 1579 | `				if( pObj == 0 ){` |
|      ! 0 | 1580 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 | 1581 | `					return SXERR_ABORT;` |
|        - | 1582 | `				}` |
|     4625 | 1583 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|     4625 | 1584 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     2310 | 1585 | `			}` |
|        - | 1586 | `			/* Emit the load constant instruction.` |
|        - | 1587 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|        - | 1588 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|     7082 | 1589 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|     2359 | 1590 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|     2359 | 1591 | `				nIdx,0,0);` |
|     4723 | 1592 | `			return SXRET_OK;` |
|        - | 1593 | `		}` |
|      ! 0 | 1594 | `	}` |
|        - | 1595 | `	/* Single-token literal: load directly */` |
| 16559645 | 1596 | `	rc = GenStateLoadLiteral(&(*pGen));` |
| 16559645 | 1597 | `	return rc;` |
|  8282184 | 1598 | `}` |
|        - | 1599 | `/*` |
|        - | 1600 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|        - | 1601 | ` */` |
|        - | 1602 | `/*` |
|        - | 1603 | `` * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of`` |
|        - | 1604 | `` * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument`` |
|        - | 1605 | ``  * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...` `` |
|        - | 1606 | ` * appeared outside a call argument list — a syntax error (PHP rejects it likewise).` |
|        - | 1607 | ` */` |
|      ! 0 | 1608 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|      ! 0 | 1609 | `{` |
|      ! 0 | 1610 | `	SXUNUSED(iCompileFlag);` |
|      ! 0 | 1611 | `	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|        - | 1612 | `		"Cannot use the first-class callable syntax '...' here");` |
|      ! 0 | 1613 | `	return SXERR_SYNTAX;` |
|      ! 0 | 1614 | `}` |
| 16564358 | 1615 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|        5 | 1616 | `{` |
|        - | 1617 | `	sxi32 rc;` |
| 16564363 | 1618 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
| 16564363 | 1619 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1620 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|      ! 0 | 1621 | `		return rc;` |
|        - | 1622 | `	}` |
|        - | 1623 | `	/* Node successfully compiled */` |
| 16564363 | 1624 | `	return SXRET_OK;` |
|  8282184 | 1625 | `}` |
|        - | 1626 |  |
