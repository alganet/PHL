# src/ph7/compile_node.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 844/973 lines (86.74%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|       - |    7 | `#include "compile_int.h"` |
|       - |    8 | `/*` |
|       - |    9 | ` * Section:` |
|       - |   10 | ` *    Expression-node compilation: anonymous functions/closures, arrow` |
|       - |   11 | ` *    functions and their capture scan, match expressions, backtick,` |
|       - |   12 | ` *    language constructs, variables and literal/name resolution.` |
|       - |   13 | ` * Status:` |
|       - |   14 | ` *    Stable.` |
|       - |   15 | ` */` |
|       - |   16 | `/*` |
|       - |   17 | ` * Build php's VISIBLE name for the closure whose 'function'/'fn' keyword sits on nLine,` |
|       - |   18 | ` * and hand it to GenStateCompileFunc (or to the arrow-function compiler) through` |
|       - |   19 | ` * pGen->sPendingClosureName.` |
|       - |   20 | ` *` |
|       - |   21 | ` * php 8.4 stopped naming every closure "{closure}" and now writes the SCOPE it was` |
|       - |   22 | ` * declared in — zend_begin_func_decl (Zend/zend_compile.c):` |
|       - |   23 | ` *` |
|       - |   24 | ` *     "{closure:%S%S%S%s:%u}"  <- class, separator, function, parens, start line` |
|       - |   25 | ` *` |
|       - |   26 | ` * where the function part is the ENCLOSING function's name and falls back to the` |
|       - |   27 | ` * FILE at top level. Two details the format string hides, both probed against the` |
|       - |   28 | `` * oracle: a method contributes `Class::name()` — the class being COMPILED, so a`` |
|       - |   29 | ` * trait method names the TRAIT, not the class that uses it — while an enclosing` |
|       - |   30 | `` * CLOSURE contributes its own `{closure:...}` name verbatim, with no parens and no`` |
|       - |   31 | `` * class, so nesting reads `{closure:{closure:/f.php:13}:13}`.`` |
|       - |   32 | ` *` |
|       - |   33 | ` * The name is a compile-time fact (the enclosing scope is not knowable at run time),` |
|       - |   34 | ` * and it must be recorded BEFORE the body is compiled, since __FUNCTION__ inside the` |
|       - |   35 | ` * body resolves against it at compile time.` |
|       - |   36 | ` */` |
|    6122 |   37 | `static void GenStateClosureName(ph7_gen_state *pGen,sxu32 nLine)` |
|       5 |   38 | `{` |
|    6127 |   39 | `	ph7_vm_func *pOuter = 0;` |
|    6127 |   40 | `	GenBlock *pBlock = pGen->pCurrent;` |
|       - |   41 | `	SyBlob sName;` |
|       - |   42 | `	char *zDup;` |
|       - |   43 | `	/* Innermost REAL function block: a synthetic one (a match() arm's throw-fixup` |
|       - |   44 | `	 * block) carries no ph7_vm_func and is not a scope. */` |
|   12487 |   45 | `	while( pBlock ){` |
|    6671 |   46 | `		if( (pBlock->iFlags & GEN_BLOCK_FUNC) && pBlock->pUserData ){` |
|     311 |   47 | `			pOuter = (ph7_vm_func *)pBlock->pUserData;` |
|     311 |   48 | `			break;` |
|       - |   49 | `		}` |
|    6365 |   50 | `		pBlock = pBlock->pParent;` |
|       5 |   51 | `	}` |
|    6127 |   52 | `	SyStringInitFromBuf(&pGen->sPendingClosureName,0,0);` |
|    6127 |   53 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|    6127 |   54 | `	SyBlobAppend(&sName,"{closure:",sizeof("{closure:")-1);` |
|    6127 |   55 | `	if( pOuter == 0 ){` |
|       - |   56 | `		/* Top level: php writes the compiled file's path (empty when there is none —` |
|       - |   57 | `		 * an eval()/direct-API compile — which is php's "{closure::LINE}" there). */` |
|    5821 |   58 | `		SyString *pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|    5821 |   59 | `		if( pFile && SyStringLength(pFile) > 0 ){` |
|    5821 |   60 | `			SyBlobAppend(&sName,SyStringData(pFile),SyStringLength(pFile));` |
|    2913 |   61 | `		}` |
|    3219 |   62 | `	}else if( SyStringLength(&pOuter->sClosureName) > 0 ){` |
|       - |   63 | `		/* Enclosing closure: its whole name, no parens and no class. */` |
|     224 |   64 | `		SyBlobAppend(&sName,SyStringData(&pOuter->sClosureName),` |
|      73 |   65 | `			SyStringLength(&pOuter->sClosureName));` |
|      78 |   66 | `	}else{` |
|     165 |   67 | `		if( (pOuter->iFlags & VM_FUNC_CLASS_METHOD) && pOuter->pUserData ){` |
|     104 |   68 | `			SyString *pCls = &((ph7_class *)pOuter->pUserData)->sName;` |
|     104 |   69 | `			SyBlobAppend(&sName,SyStringData(pCls),SyStringLength(pCls));` |
|     104 |   70 | `			SyBlobAppend(&sName,"::",2);` |
|      50 |   71 | `		}` |
|     165 |   72 | `		SyBlobAppend(&sName,SyStringData(&pOuter->sName),SyStringLength(&pOuter->sName));` |
|     165 |   73 | `		SyBlobAppend(&sName,"()",2);` |
|       - |   74 | `	}` |
|    6127 |   75 | `	SyBlobFormat(&sName,":%u}",nLine);` |
|    9188 |   76 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    6122 |   77 | `		(const char *)SyBlobData(&sName),(sxu32)SyBlobLength(&sName));` |
|    6127 |   78 | `	if( zDup ){` |
|    6127 |   79 | `		SyStringInitFromBuf(&pGen->sPendingClosureName,zDup,(sxu32)SyBlobLength(&sName));` |
|    3061 |   80 | `	}` |
|    6127 |   81 | `	SyBlobRelease(&sName);` |
|    6127 |   82 | `}` |
|       - |   83 | `/*` |
|       - |   84 | ` * Compile an annoynmous function or a closure.` |
|       - |   85 | ` * According to the PHP language reference` |
|       - |   86 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|       - |   87 | ` *  which have no specified name. They are most useful as the value of callback` |
|       - |   88 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|       - |   89 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|       - |   90 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|       - |   91 | ` *  Example Anonymous function variable assignment example` |
|       - |   92 | ` * <?php` |
|       - |   93 | ` * $greet = function($name)` |
|       - |   94 | ` * {` |
|       - |   95 | ` *    printf("Hello %s\r\n", $name);` |
|       - |   96 | ` * };` |
|       - |   97 | ` * $greet('World');` |
|       - |   98 | ` * $greet('PHP');` |
|       - |   99 | ` * ?>` |
|       - |  100 | ` * Note that the implementation of annoynmous function and closure under` |
|       - |  101 | ` * PH7 is completely different from the one used by the zend engine.` |
|       - |  102 | ` */` |
|    2602 |  103 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  104 | `{` |
|    2607 |  105 | `	ph7_vm_func *pAnnonFunc = 0; /* Annonymous function body */` |
|       - |  106 | `	char zName[512];         /* Unique lambda name */` |
|       - |  107 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - |  108 | `							  * one thread is allowed to compile the script.` |
|       - |  109 | `						      */` |
|       - |  110 | `	SyString sName;` |
|    2607 |  111 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `$f = #[A] function…` trivia`` |
|       - |  112 | `	                              * is keyed to this ['static'] 'function' token */` |
|       - |  113 | `	sxu32 nKwLine;` |
|    2607 |  114 | `	sxi32 iFlags = 0;` |
|       - |  115 | `	sxu32 nLen;` |
|       - |  116 | `	sxi32 rc;` |
|    1301 |  117 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  118 |  |
|    2607 |  119 | `	nKwLine = pGen->pIn->nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|    2602 |  120 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|    2607 |  121 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  122 | `		/* Static closure: no $this auto-capture, bind refused */` |
|      51 |  123 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|      51 |  124 | `		pGen->pIn++; /* Jump the 'static' keyword */` |
|      23 |  125 | `	}` |
|    2607 |  126 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|    2607 |  127 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 |  128 | `		pGen->pIn++;` |
|     ! 0 |  129 | `	}` |
|       - |  130 | `	/* Generate a unique name */` |
|    2607 |  131 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - |  132 | `	/* Make sure the generated name is unique */` |
|    2607 |  133 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  134 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 |  135 | `	}` |
|    2607 |  136 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|       - |  137 | `	/* php's visible name for this closure, built before the body compiles so a` |
|       - |  138 | `	 * __FUNCTION__ inside it resolves to the same text php reports. */` |
|    2607 |  139 | `	GenStateClosureName(&(*pGen),nKwLine);` |
|       - |  140 | `	/* Compile the lambda body */` |
|    2607 |  141 | `	rc = GenStateCompileFunc(&(*pGen),&sName,iFlags,TRUE,&pAnnonFunc);` |
|    2607 |  142 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  143 | `		return SXERR_ABORT;` |
|       - |  144 | `	}` |
|    2607 |  145 | `	if( pAnnonFunc ){` |
|    2605 |  146 | `		pAnnonFunc->nLine = nKwLine;` |
|       - |  147 | ``		/* Expression-position attributes (`$f = #[A] function () {}`): the trivia`` |
|       - |  148 | `		 * sidecar keys them to the closure's first keyword token. */` |
|    2605 |  149 | `		if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnnonFunc->aAttrs) == SXERR_ABORT ){` |
|     ! 0 |  150 | `			return SXERR_ABORT;` |
|       - |  151 | `		}` |
|    1300 |  152 | `	}` |
|       - |  153 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|       - |  154 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|       - |  155 | `	 * the handler wraps either in a Closure instance. */` |
|    2607 |  156 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|       - |  157 | `	/* Node successfully compiled */` |
|    2607 |  158 | `	return SXRET_OK;` |
|    1306 |  159 | `}` |
|       - |  160 | `/*` |
|       - |  161 | ` * Add a free variable to the arrow function's closure environment, unless` |
|       - |  162 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|       - |  163 | ` * enclosing arrow level, or has already been captured.` |
|       - |  164 | ` */` |
|    1512 |  165 | `static sxi32 GenStateArrowAddCapture(` |
|       - |  166 | `	ph7_gen_state *pGen,` |
|       - |  167 | `	ph7_vm_func *pFunc,` |
|       - |  168 | `	const char *zName,` |
|       - |  169 | `	sxu32 nByte,` |
|       - |  170 | `	SyString *aShadow,` |
|       - |  171 | `	sxu32 nShadow)` |
|       5 |  172 | `{` |
|       - |  173 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  174 | `	ph7_vm_func_closure_env *aEnv;` |
|       - |  175 | `	sxu32 n, nEnv;` |
|       - |  176 | `	char *zDup;` |
|    1517 |  177 | `	if( nByte == 0 ){` |
|     ! 0 |  178 | `		return SXRET_OK;` |
|       - |  179 | `	}` |
|    1512 |  180 | `	if( nByte == sizeof("this")-1` |
|     819 |  181 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|      15 |  182 | `		return SXRET_OK;` |
|       - |  183 | `	}` |
|    1503 |  184 | `	if( PH7_VmIsAutoGlobal(zName,nByte) ){` |
|       - |  185 | `		/* php never auto-captures an auto-global — it is already visible inside` |
|       - |  186 | `		 * the arrow function. Capturing one was actively destructive here: the` |
|       - |  187 | `		 * install resolves the name through hSuper and so wrote the by-value` |
|       - |  188 | `		 * SNAPSHOT over the superglobal's own slot, which for $GLOBALS froze the` |
|       - |  189 | `		 * whole symbol-table view at closure-creation time for the rest of the` |
|       - |  190 | `		 * program. */` |
|      44 |  191 | `		return SXRET_OK;` |
|       - |  192 | `	}` |
|    1585 |  193 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|     630 |  194 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|     623 |  195 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|     511 |  196 | `			return SXRET_OK;` |
|       - |  197 | `		}` |
|      65 |  198 | `	}` |
|     955 |  199 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|     955 |  200 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|    1065 |  201 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|     192 |  202 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|     161 |  203 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|      85 |  204 | `			return SXRET_OK;` |
|       - |  205 | `		}` |
|      59 |  206 | `	}` |
|     873 |  207 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|     873 |  208 | `	if( zDup == 0 ){` |
|     ! 0 |  209 | `		return SXERR_ABORT;` |
|       - |  210 | `	}` |
|     873 |  211 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|     873 |  212 | `	sEnv.iFlags = 0;` |
|     873 |  213 | `	sEnv.nIdx = SXU32_HIGH;` |
|     873 |  214 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|     873 |  215 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|     873 |  216 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|     873 |  217 | `	return SXRET_OK;` |
|     761 |  218 | `}` |
|       - |  219 | `/*` |
|       - |  220 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|       - |  221 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|       - |  222 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|       - |  223 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|       - |  224 | ` */` |
|     890 |  225 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|       - |  226 | `	ph7_gen_state *pGen,` |
|       - |  227 | `	ph7_vm_func *pFunc,` |
|       - |  228 | `	const char *zIn,` |
|       - |  229 | `	const char *zEnd,` |
|       - |  230 | `	SyString *aShadow,` |
|       - |  231 | `	sxu32 nShadow)` |
|       5 |  232 | `{` |
|       - |  233 | `	sxi32 rc;` |
|    3213 |  234 | `	while( zIn < zEnd ){` |
|    2323 |  235 | `		if( zIn[0] == '\\' ){` |
|      97 |  236 | `			zIn++;` |
|      97 |  237 | `			if( zIn < zEnd ){` |
|      97 |  238 | `				zIn++;` |
|      46 |  239 | `			}` |
|      97 |  240 | `			continue;` |
|       - |  241 | `		}` |
|    2226 |  242 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|      65 |  243 | `			&& ((unsigned char)zIn[1] >= 0x80` |
|      60 |  244 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|       - |  245 | `			/* php's label bytes, the flat set (LEX_LABEL_START in lex.c). */` |
|       - |  246 | `			const char *zName;` |
|      62 |  247 | `			zIn++; /* skip '$' */` |
|      62 |  248 | `			zName = zIn;` |
|     216 |  249 | `			while( zIn < zEnd` |
|     314 |  250 | `				&& ((unsigned char)zIn[0] >= 0x80` |
|     306 |  251 | `					\|\| SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_') ){` |
|     254 |  252 | `				zIn++;` |
|       2 |  253 | `			}` |
|      62 |  254 | `			if( zIn > zName ){` |
|      92 |  255 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|      60 |  256 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|      62 |  257 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  258 | `					return SXERR_ABORT;` |
|       - |  259 | `				}` |
|      30 |  260 | `			}` |
|      62 |  261 | `			continue;` |
|       - |  262 | `		}` |
|    2171 |  263 | `		zIn++;` |
|       5 |  264 | `	}` |
|     895 |  265 | `	return SXRET_OK;` |
|     450 |  266 | `}` |
|       - |  267 | `/*` |
|       - |  268 | ` * Scan the body token range of an arrow function for free-variable` |
|       - |  269 | ` * references and record them in pFunc's closure environment. Handles:` |
|       - |  270 | ` *   - plain $<id> pairs` |
|       - |  271 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|       - |  272 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|       - |  273 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|       - |  274 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|       - |  275 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|       - |  276 | ` *     are never mistakenly captured.` |
|       - |  277 | ` */` |
|    3614 |  278 | `static sxi32 GenStateArrowCaptureScan(` |
|       - |  279 | `	ph7_gen_state *pGen,` |
|       - |  280 | `	ph7_vm_func *pFunc,` |
|       - |  281 | `	SyToken *pStart,` |
|       - |  282 | `	SyToken *pEnd,` |
|       - |  283 | `	SyString *aShadow,` |
|       - |  284 | `	sxu32 nShadow)` |
|       5 |  285 | `{` |
|    3619 |  286 | `	SyToken *pScan = pStart;` |
|       - |  287 | `	sxi32 rc;` |
|   33387 |  288 | `	while( pScan < pEnd ){` |
|   29773 |  289 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|    1340 |  290 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|     445 |  291 | `				pScan->sData.zString,` |
|     890 |  292 | `				pScan->sData.zString + pScan->sData.nByte,` |
|     445 |  293 | `				aShadow,nShadow);` |
|     895 |  294 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  295 | `				return SXERR_ABORT;` |
|       - |  296 | `			}` |
|     895 |  297 | `			pScan++;` |
|     895 |  298 | `			continue;` |
|       - |  299 | `		}` |
|   28883 |  300 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|     205 |  301 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|     205 |  302 | `			SyToken *pFnKw = (nKw == PH7_TKWRD_STATIC) ? &pScan[1] : pScan;` |
|       - |  303 | ``			/* A NESTED arrow function, not a `$fn`/`C::fn` name that merely`` |
|       - |  304 | `			 * spells the keyword (see PH7_TokenOpensArrowFunc). */` |
|     205 |  305 | `			if( PH7_TokenOpensArrowFunc(pStart,pScan,pEnd) ){` |
|       - |  306 | `				SyToken *pInnerSigStart;` |
|       - |  307 | `				SyToken *pInnerSigEnd;` |
|       - |  308 | `				SyToken *pInnerBodyEnd;` |
|       - |  309 | `				SyString *aInnerShadow;` |
|       - |  310 | `				sxu32 nInnerShadow;` |
|       - |  311 | `				sxu32 nInnerParamMax;` |
|       - |  312 | `				SyToken *p;` |
|       - |  313 | `				int iNestInner;` |
|      94 |  314 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|      94 |  315 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  316 | `					pScan++;` |
|     ! 0 |  317 | `				}` |
|      94 |  318 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  319 | `					pScan++;` |
|     ! 0 |  320 | `					continue;` |
|       - |  321 | `				}` |
|      94 |  322 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|      94 |  323 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|       - |  324 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|      94 |  325 | `				if( pInnerSigEnd >= pEnd ){` |
|     ! 0 |  326 | `					pScan = pEnd;` |
|     ! 0 |  327 | `					continue;` |
|       - |  328 | `				}` |
|       - |  329 | `				/* Build an augmented shadow list: inherited + inner params */` |
|      94 |  330 | `				nInnerParamMax = 0;` |
|     294 |  331 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|     203 |  332 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|      89 |  333 | `						nInnerParamMax++;` |
|      43 |  334 | `					}` |
|     103 |  335 | `				}` |
|      94 |  336 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|      90 |  337 | `					&pGen->pVm->sAllocator,` |
|      90 |  338 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|      94 |  339 | `				if( aInnerShadow == 0 ){` |
|     ! 0 |  340 | `					return SXERR_ABORT;` |
|       - |  341 | `				}` |
|      94 |  342 | `				nInnerShadow = 0;` |
|     102 |  343 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|       9 |  344 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|       5 |  345 | `				}` |
|     294 |  346 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|     203 |  347 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|     117 |  348 | `						continue;` |
|       - |  349 | `					}` |
|      89 |  350 | `					if( &p[1] >= pInnerSigEnd ){` |
|     ! 0 |  351 | `						break;` |
|       - |  352 | `					}` |
|      89 |  353 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  354 | `						continue;` |
|       - |  355 | `					}` |
|      89 |  356 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|      46 |  357 | `				}` |
|      94 |  358 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|      94 |  359 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|     ! 0 |  360 | `					pScan++;` |
|     ! 0 |  361 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|     ! 0 |  362 | `						&& pScan->sData.nByte == 1` |
|     ! 0 |  363 | `						&& pScan->sData.zString[0] == '?' ){` |
|     ! 0 |  364 | `						pScan++;` |
|     ! 0 |  365 | `					}` |
|     ! 0 |  366 | `					if( pScan < pEnd` |
|     ! 0 |  367 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 |  368 | `						pScan++;` |
|     ! 0 |  369 | `					}` |
|     ! 0 |  370 | `				}` |
|      94 |  371 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|      94 |  372 | `					pScan++; /* past '=>' */` |
|      45 |  373 | `				}` |
|      94 |  374 | `				pInnerBodyEnd = pScan;` |
|      94 |  375 | `				iNestInner = 0;` |
|     712 |  376 | `				while( pInnerBodyEnd < pEnd ){` |
|     690 |  377 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|       - |  378 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|       - |  379 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|      71 |  380 | `						break;` |
|       - |  381 | `					}` |
|     622 |  382 | `					if( pInnerBodyEnd->nType &` |
|       - |  383 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      80 |  384 | `						iNestInner++;` |
|     583 |  385 | `					}else if( pInnerBodyEnd->nType &` |
|       - |  386 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      80 |  387 | `						iNestInner--;` |
|      39 |  388 | `					}` |
|     622 |  389 | `					pInnerBodyEnd++;` |
|       4 |  390 | `				}` |
|       - |  391 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|       - |  392 | `				 * the outer's body: a default value is evaluated at call time` |
|       - |  393 | `				 * in the outer frame, so any free variable it references is` |
|       - |  394 | `				 * an outer capture. We must NOT scan the parameter-name` |
|       - |  395 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|       - |  396 | `				 * or those names leak into the outer's closure environment.` |
|       - |  397 | `				 *` |
|       - |  398 | `				 * Walk the signature argument-by-argument, splitting on` |
|       - |  399 | `				 * top-level commas, and for each argument scan only the token` |
|       - |  400 | `				 * range after the '=' sign. */` |
|       - |  401 | `				{` |
|      94 |  402 | `					SyToken *pArgStart = pInnerSigStart;` |
|     180 |  403 | `					while( pArgStart < pInnerSigEnd ){` |
|      89 |  404 | `						SyToken *pArgEnd = pArgStart;` |
|      89 |  405 | `						SyToken *pEq = 0;` |
|      89 |  406 | `						int iNestArg = 0;` |
|     275 |  407 | `						while( pArgEnd < pInnerSigEnd ){` |
|     200 |  408 | `							if( iNestArg == 0` |
|     203 |  409 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|      17 |  410 | `								break;` |
|       - |  411 | `							}` |
|     189 |  412 | `							if( pArgEnd->nType &` |
|       - |  413 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  414 | `								iNestArg++;` |
|     189 |  415 | `							}else if( pArgEnd->nType &` |
|       - |  416 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  417 | `								iNestArg--;` |
|     ! 0 |  418 | `							}` |
|     186 |  419 | `							if( pEq == 0 && iNestArg == 0` |
|     183 |  420 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|       7 |  421 | `								pEq = pArgEnd;` |
|       3 |  422 | `							}` |
|     189 |  423 | `							pArgEnd++;` |
|       3 |  424 | `						}` |
|      89 |  425 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|      10 |  426 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       3 |  427 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|       7 |  428 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  429 | `								return SXERR_ABORT;` |
|       - |  430 | `							}` |
|       3 |  431 | `						}` |
|      89 |  432 | `						pArgStart = pArgEnd;` |
|      86 |  433 | `						if( pArgStart < pInnerSigEnd` |
|      53 |  434 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|      17 |  435 | `							pArgStart++;` |
|       7 |  436 | `						}` |
|       3 |  437 | `					}` |
|       - |  438 | `				}` |
|     139 |  439 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|      45 |  440 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|      94 |  441 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  442 | `					return SXERR_ABORT;` |
|       - |  443 | `				}` |
|      94 |  444 | `				pScan = pInnerBodyEnd;` |
|      94 |  445 | `				continue;` |
|       - |  446 | `			}` |
|      55 |  447 | `		}` |
|   28793 |  448 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|   27341 |  449 | `			pScan++;` |
|   27341 |  450 | `			continue;` |
|       - |  451 | `		}` |
|       - |  452 | `		{` |
|       - |  453 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|    1457 |  454 | `			SyToken *pDollar = pScan;` |
|    2178 |  455 | `			while( &pDollar[1] < pEnd` |
|    1457 |  456 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|     ! 0 |  457 | `				pDollar++;` |
|     ! 0 |  458 | `			}` |
|    1457 |  459 | `			if( &pDollar[1] >= pEnd ){` |
|     ! 0 |  460 | `				break;` |
|       - |  461 | `			}` |
|    1457 |  462 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  463 | `				pScan = pDollar + 1;` |
|     ! 0 |  464 | `				continue;` |
|       - |  465 | `			}` |
|    2183 |  466 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|    1452 |  467 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|     726 |  468 | `				aShadow,nShadow);` |
|    1457 |  469 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  470 | `				return SXERR_ABORT;` |
|       - |  471 | `			}` |
|    1457 |  472 | `			pScan = pDollar + 2;` |
|       - |  473 | `		}` |
|       5 |  474 | `	}` |
|    3619 |  475 | `	return SXRET_OK;` |
|    1812 |  476 | `}` |
|       - |  477 | `/*` |
|       - |  478 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|       - |  479 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|       - |  480 | ` * variables by value. The body is a single expression that acts as an` |
|       - |  481 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|       - |  482 | ` * $this is also made available.` |
|       - |  483 | ` */` |
|    3524 |  484 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  485 | `{` |
|       - |  486 | `	ph7_vm_func *pFunc;` |
|       - |  487 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  488 | `	GenBlock *pBlock;` |
|       - |  489 | `	SySet *pInstrContainer;` |
|       - |  490 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|       - |  491 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|       - |  492 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|       - |  493 | `	SyToken *pSavedEnd;` |
|       - |  494 | `	ph7_vm_func_arg *aArgs;` |
|       - |  495 | `	char zName[512];` |
|       - |  496 | `	static int iCnt = 1;` |
|       - |  497 | `	char *zDup;` |
|       - |  498 | `	SyToken *pTokKw;` |
|       - |  499 | `	sxu32 nLen;` |
|       - |  500 | `	sxu32 nLine;` |
|    3529 |  501 | `	sxi32 iFlags = 0;` |
|    3529 |  502 | `	int bStatic = 0;` |
|       - |  503 | `	sxi32 rc;` |
|       - |  504 | `	sxu32 n;` |
|    1762 |  505 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  506 |  |
|    3529 |  507 | `	nLine = pGen->pIn->nLine;` |
|       - |  508 | ``	/* Attribute-sidecar key: `#[A] [static] fn` trivia is keyed to this token */`` |
|    3529 |  509 | `	pTokKw = pGen->pIn;` |
|       - |  510 | `	/* Optional 'static' prefix */` |
|    3524 |  511 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|    3529 |  512 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|      37 |  513 | `		bStatic = 1;` |
|      37 |  514 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|      37 |  515 | `		pGen->pIn++;` |
|      18 |  516 | `	}` |
|       - |  517 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|    3524 |  518 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    3529 |  519 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|     ! 0 |  520 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  521 | `			"Arrow function: expected 'fn' keyword");` |
|     ! 0 |  522 | `		return SXERR_SYNTAX;` |
|       - |  523 | `	}` |
|    3529 |  524 | `	pGen->pIn++; /* Jump 'fn' */` |
|       - |  525 | `	/* Optional '&' — return by reference */` |
|    3529 |  526 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  527 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|     ! 0 |  528 | `		pGen->pIn++;` |
|     ! 0 |  529 | `	}` |
|       - |  530 | `	/* Expect '(' */` |
|    3529 |  531 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  532 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  533 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  534 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|       2 |  535 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  536 | `		}else{` |
|     ! 0 |  537 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  538 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|       - |  539 | `		}` |
|       3 |  540 | `		return SXERR_SYNTAX;` |
|       - |  541 | `	}` |
|    3527 |  542 | `	pGen->pIn++; /* Jump '(' */` |
|       - |  543 | `	/* Delimit the parameter list */` |
|    3527 |  544 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|    3527 |  545 | `	if( pSigEnd >= pGen->pEnd ){` |
|       3 |  546 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  547 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       3 |  548 | `		return SXERR_SYNTAX;` |
|       - |  549 | `	}` |
|       - |  550 | `	/* Allocate the function state */` |
|    3525 |  551 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|    3525 |  552 | `	if( pFunc == 0 ){` |
|     ! 0 |  553 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  554 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  555 | `		return SXERR_ABORT;` |
|       - |  556 | `	}` |
|       - |  557 | `	/* Generate a unique lambda name */` |
|    3525 |  558 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|    3527 |  559 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|       3 |  560 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       1 |  561 | `	}` |
|    3525 |  562 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|    3525 |  563 | `	if( zDup == 0 ){` |
|     ! 0 |  564 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  565 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  566 | `		return SXERR_ABORT;` |
|       - |  567 | `	}` |
|    3525 |  568 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|       - |  569 | `	/* Reflection getStartLine(): line of the ['static'] 'fn' keyword */` |
|    3525 |  570 | `	pFunc->nLine = nLine;` |
|       - |  571 | `	/* php's visible name — an arrow function is named exactly like a closure. This` |
|       - |  572 | `	 * compiler builds its own function state, so it consumes the pending name itself. */` |
|    3525 |  573 | `	GenStateClosureName(&(*pGen),nLine);` |
|    3525 |  574 | `	pFunc->sClosureName = pGen->sPendingClosureName;` |
|    3525 |  575 | `	SyStringInitFromBuf(&pGen->sPendingClosureName,0,0);` |
|       - |  576 | ``	/* Expression-position attributes (`$f = #[A] fn () => …`) */`` |
|    3525 |  577 | `	if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pFunc->aAttrs) == SXERR_ABORT ){` |
|     ! 0 |  578 | `		return SXERR_ABORT;` |
|       - |  579 | `	}` |
|       - |  580 | `	/* Collect function arguments */` |
|    3525 |  581 | `	if( pGen->pIn < pSigEnd ){` |
|     369 |  582 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|     369 |  583 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  584 | `			return SXERR_ABORT;` |
|       - |  585 | `		}` |
|     182 |  586 | `	}` |
|       - |  587 | `	/* Point past ')' and parse optional return type */` |
|    3525 |  588 | `	pGen->pIn = &pSigEnd[1];` |
|    3525 |  589 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|    3525 |  590 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  591 | `		return SXERR_ABORT;` |
|    3525 |  592 | `	}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  593 | `		return SXERR_SYNTAX;` |
|       - |  594 | `	}` |
|       - |  595 | `	/* Expect '=>' */` |
|    3525 |  596 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  597 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  598 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  599 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|       2 |  600 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  601 | `		}else{` |
|     ! 0 |  602 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  603 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|       - |  604 | `		}` |
|       3 |  605 | `		return SXERR_SYNTAX;` |
|       - |  606 | `	}` |
|    3523 |  607 | `	pGen->pIn++; /* Jump '=>' */` |
|    3523 |  608 | `	pBodyStart = pGen->pIn;` |
|    3523 |  609 | `	pBodyEnd = pGen->pEnd;` |
|       - |  610 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|       - |  611 | `	 * recursively collect free-variable references from the body. The scan` |
|       - |  612 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|       - |  613 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|    3523 |  614 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       - |  615 | `	{` |
|    3523 |  616 | `		SyString *aShadow = 0;` |
|    3523 |  617 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|    3523 |  618 | `		if( nShadow > 0 ){` |
|     367 |  619 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|     362 |  620 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|     367 |  621 | `			if( aShadow == 0 ){` |
|     ! 0 |  622 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  623 | `					"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  624 | `				return SXERR_ABORT;` |
|       - |  625 | `			}` |
|     809 |  626 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|     447 |  627 | `				aShadow[n] = aArgs[n].sName;` |
|     226 |  628 | `			}` |
|     181 |  629 | `		}` |
|    5282 |  630 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|    1759 |  631 | `			aShadow,nShadow);` |
|    3523 |  632 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  633 | `			return SXERR_ABORT;` |
|       - |  634 | `		}` |
|       - |  635 | `	}` |
|       - |  636 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|       - |  637 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|       - |  638 | `	 * captured value is silently dropped when the enclosing scope has no` |
|       - |  639 | `	 * $this. */` |
|    3523 |  640 | `	if( !bStatic ){` |
|       - |  641 | `		char *zThisDup;` |
|    3487 |  642 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|    3487 |  643 | `		if( zThisDup == 0 ){` |
|     ! 0 |  644 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  645 | `				"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  646 | `			return SXERR_ABORT;` |
|       - |  647 | `		}` |
|    3487 |  648 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|    3487 |  649 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|    3487 |  650 | `		sEnv.nIdx = SXU32_HIGH;` |
|    3487 |  651 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|    3487 |  652 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|    3487 |  653 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|    1741 |  654 | `	}` |
|       - |  655 | `	/* Arrow functions are always closures; the ARROW mark tells OP_LOAD_CLOSURE` |
|       - |  656 | `	 * these captures are implicit (auto-scanned) so an undefined one stays silent` |
|       - |  657 | `	 * at creation — php only warns when the body reads it. */` |
|    3523 |  658 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE \| VM_FUNC_ARROW;` |
|       - |  659 | `	/* Compile the body expression as an implicit return */` |
|    5282 |  660 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|    1759 |  661 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|    3523 |  662 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  663 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  664 | `			"PH7 engine is running out-of-memory");` |
|     ! 0 |  665 | `		return SXERR_ABORT;` |
|       - |  666 | `	}` |
|    3523 |  667 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    3523 |  668 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|    3523 |  669 | `	pSavedEnd = pGen->pEnd;` |
|    3523 |  670 | `	pGen->pIn = pBodyStart;` |
|    3523 |  671 | `	pGen->pEnd = pBodyEnd;` |
|       - |  672 | ``	/* The body is an implicit `return <expr>`, which READS its operands. Compile`` |
|       - |  673 | `	 * it read-only (like echo / string interpolation) so a lone undefined variable` |
|       - |  674 | ``	 * — e.g. `fn()=>$z` for an auto-capture that was undefined at creation and so`` |
|       - |  675 | `	 * never captured (see VmExecOpLoadClosure) — raises php's "Undefined variable"` |
|       - |  676 | `	 * warning at the read instead of being loaded quietly as a plain expression` |
|       - |  677 | ``	 * statement (`$z;`, silent in both engines) would be. */`` |
|    3523 |  678 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|    3523 |  679 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  680 | `		return SXERR_ABORT;` |
|       - |  681 | `	}` |
|       - |  682 | `	/* The cursor stopped just past the body expression */` |
|    3523 |  683 | `	pFunc->nEndLine = (pGen->pIn > pBodyStart) ? pGen->pIn[-1].nLine : nLine;` |
|       - |  684 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|       - |  685 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|       - |  686 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|       - |  687 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|    3523 |  688 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|    3523 |  689 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|    3523 |  690 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|    3523 |  691 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    3523 |  692 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  693 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|    3523 |  694 | `	pGen->pIn = pBodyEnd;` |
|    3523 |  695 | `	pGen->pEnd = pSavedEnd;` |
|       - |  696 | `	/* Emit the load-closure instruction */` |
|    3523 |  697 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|    3523 |  698 | `	return SXRET_OK;` |
|    1767 |  699 | `}` |
|       - |  700 | `/*` |
|       - |  701 | ` * Compile a single arm's expression range into a freshly-allocated` |
|       - |  702 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|       - |  703 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|       - |  704 | ` * expression's value.` |
|       - |  705 | ` */` |
|     478 |  706 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|       - |  707 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|       4 |  708 | `{` |
|       - |  709 | `	SySet *pInstrContainer;` |
|       - |  710 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  711 | `	GenBlock *pArmBlock;` |
|       - |  712 | `	sxi32 rc;` |
|     482 |  713 | `	pTmpIn  = pGen->pIn;` |
|     482 |  714 | `	pTmpEnd = pGen->pEnd;` |
|     482 |  715 | `	pGen->pIn  = pStart;` |
|     482 |  716 | `	pGen->pEnd = pStop;` |
|     482 |  717 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     482 |  718 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|       - |  719 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|       - |  720 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|       - |  721 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|       - |  722 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|       - |  723 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|     721 |  724 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|     239 |  725 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|     482 |  726 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  727 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  728 | `		pGen->pIn  = pTmpIn;` |
|     ! 0 |  729 | `		pGen->pEnd = pTmpEnd;` |
|     ! 0 |  730 | `		return SXERR_ABORT;` |
|       - |  731 | `	}` |
|     482 |  732 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     482 |  733 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     482 |  734 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     482 |  735 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     482 |  736 | `	GenStateLeaveBlock(&(*pGen),0);` |
|     482 |  737 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     482 |  738 | `	pGen->pIn  = pTmpIn;` |
|     482 |  739 | `	pGen->pEnd = pTmpEnd;` |
|     482 |  740 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  741 | `		return SXERR_ABORT;` |
|       - |  742 | `	}` |
|     482 |  743 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  744 | `		return SXERR_EMPTY;` |
|       - |  745 | `	}` |
|     482 |  746 | `	return SXRET_OK;` |
|     243 |  747 | `}` |
|       - |  748 | `/*` |
|       - |  749 | ` * Compile a PHP 8.0 match expression:` |
|       - |  750 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|       - |  751 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|       - |  752 | ` * Strict comparison (===) is used between the subject and each condition.` |
|       - |  753 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|       - |  754 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|       - |  755 | ` */` |
|       - |  756 | `/*` |
|       - |  757 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|       - |  758 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|       - |  759 | ` * caller can bail out of the current expression.` |
|       - |  760 | ` */` |
|       2 |  761 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|       1 |  762 | `{` |
|       - |  763 | `	va_list ap;` |
|       - |  764 | `	sxi32 rc;` |
|       - |  765 | `	SyBlob sMsg;` |
|       3 |  766 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|       3 |  767 | `	va_start(ap,zFmt);` |
|       3 |  768 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|       3 |  769 | `	va_end(ap);` |
|       3 |  770 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|       3 |  771 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|       3 |  772 | `	SyBlobRelease(&sMsg);` |
|       3 |  773 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  774 | `		return SXERR_ABORT;` |
|       - |  775 | `	}` |
|       3 |  776 | `	return SXERR_SYNTAX;` |
|       2 |  777 | `}` |
|       - |  778 | `/*` |
|       - |  779 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|       - |  780 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|       - |  781 | ` * Returns the stop token pointer (or pEnd if none found).` |
|       - |  782 | ` */` |
|     480 |  783 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|       5 |  784 | `{` |
|     485 |  785 | `	SyToken *pCur = pStart;` |
|     485 |  786 | `	int iNest = 0;` |
|    1179 |  787 | `	while( pCur < pEnd ){` |
|    1107 |  788 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      38 |  789 | `			iNest++;` |
|    1089 |  790 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      38 |  791 | `			iNest--;` |
|    1053 |  792 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|     412 |  793 | `			return pCur;` |
|       - |  794 | `		}` |
|     699 |  795 | `		pCur++;` |
|       5 |  796 | `	}` |
|      77 |  797 | `	return pEnd;` |
|     245 |  798 | `}` |
|     112 |  799 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  800 | `{` |
|       - |  801 | `	ph7_match *pMatch;` |
|       - |  802 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|     117 |  803 | `	int bHasDefault = 0;` |
|       - |  804 | `	sxu32 nLine;` |
|       - |  805 | `	sxi32 rc;` |
|      56 |  806 | `	SXUNUSED(iCompileFlag);` |
|     117 |  807 | `	nLine = pGen->pIn->nLine;` |
|     117 |  808 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|       - |  809 | `	/* Expect '(' */` |
|     117 |  810 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  811 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  812 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|     ! 0 |  813 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|       - |  814 | `	}` |
|     117 |  815 | `	pGen->pIn++; /* Jump '(' */` |
|     117 |  816 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|     117 |  817 | `	if( pSubjEnd >= pGen->pEnd ){` |
|     ! 0 |  818 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  819 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       - |  820 | `	}` |
|     117 |  821 | `	if( pGen->pIn >= pSubjEnd ){` |
|     ! 0 |  822 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  823 | `			"syntax error, unexpected \")\", expecting match subject");` |
|       - |  824 | `	}` |
|       - |  825 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|     117 |  826 | `	pSavedEnd = pGen->pEnd;` |
|     117 |  827 | `	pGen->pEnd = pSubjEnd;` |
|     117 |  828 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     117 |  829 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  830 | `		return SXERR_ABORT;` |
|       - |  831 | `	}` |
|     117 |  832 | `	pGen->pEnd = pSavedEnd;` |
|     117 |  833 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|       - |  834 | `	/* Expect '{' */` |
|     117 |  835 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  836 | `		return GenStateMatchError(pGen,` |
|     ! 0 |  837 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  838 | `			"syntax error, expecting \"{\" after match subject");` |
|       - |  839 | `	}` |
|     117 |  840 | `	pGen->pIn++; /* Jump '{' */` |
|     117 |  841 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|     117 |  842 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  843 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  844 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|       - |  845 | `	}` |
|       - |  846 | `	/* Allocate ph7_match container */` |
|     117 |  847 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|     117 |  848 | `	if( pMatch == 0 ){` |
|     ! 0 |  849 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  850 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  851 | `		return SXERR_ABORT;` |
|       - |  852 | `	}` |
|     117 |  853 | `	SyZero(pMatch,sizeof(ph7_match));` |
|     117 |  854 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|       - |  855 | `	/* Iterate arms */` |
|     371 |  856 | `	while( pGen->pIn < pBodyEnd ){` |
|       - |  857 | `		ph7_match_arm sArm;` |
|       - |  858 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|     263 |  859 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|     263 |  860 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|     263 |  861 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|     263 |  862 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  863 | `		/* 'default' arm? */` |
|     258 |  864 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     154 |  865 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|      44 |  866 | `			if( bHasDefault ){` |
|       3 |  867 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|       - |  868 | `					"Match expressions may only contain one default arm");` |
|       4 |  869 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  870 | `			}` |
|      42 |  871 | `			sArm.bDefault = 1;` |
|      42 |  872 | `			bHasDefault = 1;` |
|      42 |  873 | `			pGen->pIn++;` |
|      42 |  874 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|     ! 0 |  875 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  876 | `					"syntax error, expecting \"=>\" after 'default'");` |
|       - |  877 | `			}` |
|      42 |  878 | `			pGen->pIn++; /* Jump '=>' */` |
|      23 |  879 | `		}else{` |
|       - |  880 | `			/* Condition list: cond (',' cond)* '=>' */` |
|     223 |  881 | `			pCondStart = pGen->pIn;` |
|     223 |  882 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|       - |  883 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|     231 |  884 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|       - |  885 | `				SySet sCondBc;` |
|       9 |  886 | `				if( pCondStart >= pArrow ){` |
|     ! 0 |  887 | `					return GenStateMatchError(pGen,nArmLine,` |
|       - |  888 | `						"syntax error, empty match condition expression");` |
|       - |  889 | `				}` |
|       9 |  890 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       9 |  891 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|       9 |  892 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  893 | `					return SXERR_ABORT;` |
|       - |  894 | `				}` |
|       9 |  895 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       9 |  896 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|       9 |  897 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|       - |  898 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|       1 |  899 | `			}` |
|     223 |  900 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  901 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  902 | `					"syntax error, expecting \"=>\" in match arm");` |
|       - |  903 | `			}` |
|     220 |  904 | `			if( pCondStart >= pArrow ){` |
|     ! 0 |  905 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  906 | `					"syntax error, empty match condition expression");` |
|       - |  907 | `			}` |
|       - |  908 | `			{` |
|       - |  909 | `				SySet sCondBc;` |
|     220 |  910 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     220 |  911 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|     220 |  912 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  913 | `					return SXERR_ABORT;` |
|       - |  914 | `				}` |
|     220 |  915 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       - |  916 | `			}` |
|     220 |  917 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|       - |  918 | `		}` |
|       - |  919 | `		/* Compile result expression: up to top-level ',' or body end */` |
|     258 |  920 | `		pResStart = pGen->pIn;` |
|     258 |  921 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|     258 |  922 | `		if( pResStart >= pResEnd ){` |
|     ! 0 |  923 | `			return GenStateMatchError(pGen,nArmLine,` |
|       - |  924 | `				"syntax error, expected expression after \"=>\"");` |
|       - |  925 | `		}` |
|     258 |  926 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|     258 |  927 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  928 | `			return SXERR_ABORT;` |
|       - |  929 | `		}` |
|     258 |  930 | `		pGen->pIn = pResEnd;` |
|     258 |  931 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|     188 |  932 | `			pGen->pIn++; /* Skip trailing ',' */` |
|      92 |  933 | `		}` |
|     258 |  934 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|       4 |  935 | `	}` |
|     112 |  936 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|     112 |  937 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|     112 |  938 | `	return SXRET_OK;` |
|      61 |  939 | `}` |
|       - |  940 | `/*` |
|       - |  941 | ` * Compile a backtick quoted string.` |
|       - |  942 | ` */` |
|       2 |  943 | `PH7_PRIVATE sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 |  944 | `{` |
|       1 |  945 | `	SXUNUSED(iCompileFlag);` |
|       - |  946 | `	/*` |
|       - |  947 | ``	 * The backtick (`) operator was DEPRECATED by php (shell_exec() is the replacement).`` |
|       - |  948 | `	 * PHL targets php's *non-deprecated* surface, so it is a hard parse error — never` |
|       - |  949 | `	 * compiled to a shell_exec() call.` |
|       - |  950 | `	 */` |
|       3 |  951 | `	PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  952 | ``		"syntax error, the backtick (`) operator was removed, use shell_exec() instead");`` |
|       3 |  953 | `	return SXERR_ABORT;` |
|       1 |  954 | `}` |
|       - |  955 | `/*` |
|       - |  956 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - |  957 | ` * construct.` |
|       - |  958 | ` */` |
|     118 |  959 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  960 | `{` |
|       - |  961 | `	SyString *pName;` |
|       - |  962 | `	sxu32 nKeyID;` |
|       - |  963 | `	sxi32 rc;` |
|       - |  964 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|     123 |  965 | `	pName = &pGen->pIn->sData;` |
|     123 |  966 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     123 |  967 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|     123 |  968 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       6 |  969 | `		SyToken *pTmp,*pNext = 0;` |
|       - |  970 | ``		/* A STATEMENT `echo` never reaches here — it dispatches through the statement`` |
|       - |  971 | `		 * table. Arriving in expression position means source like` |
|       - |  972 | ``		 * `fopen('f','r') or echo "IO error";`, which was a Symisc extension and is a`` |
|       - |  973 | `		 * php parse error (§10: a PH7-ism that changes the meaning of valid source is a` |
|       - |  974 | ``		 * bug). The one legitimate expression-echo is the token a `<?= ... ?>` short tag`` |
|       - |  975 | `		 * synthesizes, which raises nExprEchoOk around its own compile. */` |
|       6 |  976 | `		if( pGen->nExprEchoOk < 1 ){` |
|       3 |  977 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn - 1,0);` |
|       3 |  978 | `			return SXERR_ABORT;` |
|       - |  979 | `		}` |
|       - |  980 | `		/* Compile arguments one after one */` |
|       3 |  981 | `		pTmp = pGen->pEnd;` |
|       3 |  982 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|       5 |  983 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       3 |  984 | `			if( pGen->pIn < pNext ){` |
|       3 |  985 | `				pGen->pEnd = pNext;` |
|       3 |  986 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|       3 |  987 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  988 | `					return SXERR_ABORT;` |
|       - |  989 | `				}` |
|       3 |  990 | `				if( rc != SXERR_EMPTY ){` |
|       - |  991 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|       - |  992 | `					 * without the overhead of a function call.` |
|       - |  993 | `					 * This is a very powerful optimization that improve` |
|       - |  994 | `					 * performance greatly.` |
|       - |  995 | `					 */` |
|       3 |  996 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|       1 |  997 | `				}` |
|       1 |  998 | `			}` |
|       - |  999 | `			/* Jump trailing commas */` |
|       3 | 1000 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     ! 0 | 1001 | `				pNext++;` |
|     ! 0 | 1002 | `			}` |
|       3 | 1003 | `			pGen->pIn = pNext;` |
|       1 | 1004 | `		}` |
|       - | 1005 | `		/* Restore token stream */` |
|       3 | 1006 | `		pGen->pEnd = pTmp;` |
|       2 | 1007 | `	}else{` |
|     119 | 1008 | `		sxi32 nArg = 0;` |
|     119 | 1009 | `		sxu32 nIdx = 0;` |
|       - | 1010 | `		char zCanon[sizeof("include_once")-1];` |
|       - | 1011 | `		SyString sCanon;` |
|     119 | 1012 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|     119 | 1013 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1014 | `			return SXERR_ABORT;` |
|     119 | 1015 | `		}else if(rc != SXERR_EMPTY ){` |
|     119 | 1016 | `			nArg = 1;` |
|      57 | 1017 | `		}` |
|       - | 1018 | `		/* The construct is dispatched as a CALL to the host function of the same name,` |
|       - | 1019 | `		 * so the name emitted here must be the construct's canonical spelling, not the` |
|       - | 1020 | ``		 * source's: php accepts `PRINT`/`Isset`/`EVAL` (keywords are case-insensitive)`` |
|       - | 1021 | ``		 * where the raw text produced `Call to undefined function PRINT()`. Every`` |
|       - | 1022 | `		 * construct name is lower-case ASCII, so folding IS canonicalising. */` |
|     119 | 1023 | `		if( pName->nByte <= sizeof(zCanon) ){` |
|       - | 1024 | `			sxu32 i;` |
|    1037 | 1025 | `			for( i = 0 ; i < pName->nByte ; ++i ){` |
|     923 | 1026 | `				unsigned char c = (unsigned char)pName->zString[i];` |
|     923 | 1027 | `				zCanon[i] = (char)((c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c);` |
|     464 | 1028 | `			}` |
|     119 | 1029 | `			SyStringInitFromBuf(&sCanon,zCanon,pName->nByte);` |
|     119 | 1030 | `			pName = &sCanon;` |
|      57 | 1031 | `		}` |
|     119 | 1032 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - | 1033 | `			ph7_value *pObj;` |
|       - | 1034 | `			/* Emit the call instruction */` |
|      75 | 1035 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      75 | 1036 | `			if( pObj == 0 ){` |
|     ! 0 | 1037 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1038 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1039 | `				return SXERR_ABORT;` |
|       - | 1040 | `			}` |
|      75 | 1041 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - | 1042 | `			/* Install in the literal table */` |
|      75 | 1043 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      35 | 1044 | `		}` |
|       - | 1045 | `		/* Emit the call instruction */` |
|     119 | 1046 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     119 | 1047 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       - | 1048 | `	}` |
|       - | 1049 | `	/* Node successfully compiled */` |
|     121 | 1050 | `	return SXRET_OK;` |
|      64 | 1051 | `}` |
|       - | 1052 | `/*` |
|       - | 1053 | ` * Compile a node holding a variable declaration.` |
|       - | 1054 | ` * According to the PHP language reference` |
|       - | 1055 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - | 1056 | ` *  The variable name is case-sensitive.` |
|       - | 1057 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - | 1058 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - | 1059 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - | 1060 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - | 1061 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - | 1062 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - | 1063 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - | 1064 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - | 1065 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - | 1066 | ` *  the chapter on Expressions.` |
|       - | 1067 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - | 1068 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - | 1069 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - | 1070 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - | 1071 | ` *  is being assigned (the source variable).` |
|       - | 1072 | ` */` |
| 2356120 | 1073 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 | 1074 | `{` |
| 2356125 | 1075 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 1076 | `	sxi32 iVv;` |
|       - | 1077 | `	sxi32 iP1;` |
| 2356125 | 1078 | `	sxi32 iP2 = 0; /* 1 = quiet read (isset/empty): a missing variable must not warn */` |
|       - | 1079 | `	void *p3;` |
|       - | 1080 | `	sxi32 rc;` |
| 2356125 | 1081 | `	iVv = -1; /* Variable variable counter */` |
| 4712283 | 1082 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
| 2356163 | 1083 | `		pGen->pIn++;` |
| 2356163 | 1084 | `		iVv++;` |
|       5 | 1085 | `	}` |
| 2356125 | 1086 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - | 1087 | `		/* Invalid variable name */` |
|     ! 0 | 1088 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable or \"{\" or \"$\"");` |
|     ! 0 | 1089 | `		if( rc == SXERR_ABORT ){` |
|       - | 1090 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1091 | `			return SXERR_ABORT;` |
|       - | 1092 | `		}` |
|     ! 0 | 1093 | `		return SXRET_OK;` |
|       - | 1094 | `	}` |
| 2356125 | 1095 | `	p3  = 0;` |
| 2356125 | 1096 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - | 1097 | `		/* Dynamic variable creation */` |
|      33 | 1098 | `		pGen->pIn++;  /* Jump the open curly */` |
|      33 | 1099 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      33 | 1100 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 1101 | `			/* Empty expression */` |
|       - | 1102 | `			{` |
|       - | 1103 | `			/* php names the offending token and, for an empty "${}", stops there:` |
|       - | 1104 | `			 * the "expecting" tail only appears when something could still follow. */` |
|       - | 1105 | ``			/* `${}`: pEnd was stepped back past the trailing '}', so the token php`` |
|       - | 1106 | `			 * names sits AT pEnd. Reach for it before deciding the tail -- php stops` |
|       - | 1107 | `			 * at "unexpected token \"}\"" with no "expecting" clause, which the` |
|       - | 1108 | `			 * NULL-token path could not express because it never saw the '}'. */` |
|       3 | 1109 | `			SyToken *pBad = pGen->pIn < pGen->pEnd ? pGen->pIn : 0;` |
|       3 | 1110 | `			if( pBad == 0 && pGen->pTokenSet ){` |
|       3 | 1111 | `				SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|       3 | 1112 | `				SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|       3 | 1113 | `				if( pGen->pEnd >= pBase && pGen->pEnd < pStreamEnd ){` |
|       3 | 1114 | `					pBad = pGen->pEnd;` |
|       1 | 1115 | `				}` |
|       1 | 1116 | `			}` |
|       5 | 1117 | `			PH7_GenSyntaxError(&(*pGen),pBad,` |
|       2 | 1118 | `				(pBad && (pBad->nType & PH7_TK_CCB)) ? 0 : "variable or \"{\" or \"$\"");` |
|       - | 1119 | `			}` |
|       3 | 1120 | `			return SXRET_OK;` |
|       - | 1121 | `		}` |
|       - | 1122 | `		/* Compile the expression holding the variable name. It is a pure READ, so` |
|       - | 1123 | ``		 * compile it read-only: `${$u}` warns on an undefined $u (php) instead of`` |
|       - | 1124 | ``		 * silently creating it, matching the `$$u` name-read path below. A quiet`` |
|       - | 1125 | `		 * outer (isset()/empty()) suppresses that name warning too, so carry the` |
|       - | 1126 | `		 * quiet flag into the name expression. */` |
|      30 | 1127 | `		sxi32 iNameFlags = EXPR_FLAG_RDONLY_LOAD;` |
|      30 | 1128 | `		if( iCompileFlag & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY) ){` |
|       - | 1129 | ``			/* isset()/empty() suppress the name warning; `??` (EXPR_FLAG_QUIET_VAR)`` |
|       - | 1130 | ``			 * does NOT — php warns `Undefined variable $u` for `${$u} ?? x` and only`` |
|       - | 1131 | `			 * quiets the TARGET read, so QUIET_VAR is deliberately excluded here. */` |
|       3 | 1132 | `			iNameFlags \|= EXPR_FLAG_QUIET_VAR;` |
|       1 | 1133 | `		}` |
|      30 | 1134 | `		rc = PH7_CompileExpr(&(*pGen),iNameFlags,0);` |
|      30 | 1135 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1136 | `			return SXERR_ABORT;` |
|      30 | 1137 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 | 1138 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,0);` |
|       3 | 1139 | `			return SXRET_OK;` |
|       - | 1140 | `		}` |
|      15 | 1141 | `	}else{` |
|       - | 1142 | `		SyHashEntry *pEntry;` |
|       - | 1143 | `		SyString *pName;` |
| 2356097 | 1144 | `		char *zName = 0;` |
|       - | 1145 | `		/* Extract variable name */` |
| 2356097 | 1146 | `		pName = &pGen->pIn->sData;` |
|       - | 1147 | `		/* Advance the stream cursor */` |
| 2356097 | 1148 | `		pGen->pIn++;` |
| 2356097 | 1149 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
| 2356097 | 1150 | `		if( pEntry == 0 ){` |
|       - | 1151 | `			/* Duplicate name */` |
|  315371 | 1152 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  315371 | 1153 | `			if( zName == 0 ){` |
|     ! 0 | 1154 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1155 | `				return SXERR_ABORT;` |
|       - | 1156 | `			}` |
|       - | 1157 | `			/* Install in the hashtable */` |
|  315371 | 1158 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|  157688 | 1159 | `		}else{` |
|       - | 1160 | `			/* Name already available */` |
| 2040731 | 1161 | `			zName = (char *)pEntry->pUserData;` |
|       - | 1162 | `		}` |
| 2356097 | 1163 | `		p3 = (void *)zName;` |
|       - | 1164 | `	}` |
| 2356121 | 1165 | `	iP1 = 0;` |
| 2356121 | 1166 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
| 2202923 | 1167 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - | 1168 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
| 1524651 | 1169 | `			iP1 = 1;` |
|  762323 | 1170 | `		}` |
| 1101459 | 1171 | `	}` |
|       - | 1172 | ``	/* iP2 marks a QUIET read: `isset($x)` / `empty($x)` inspect a variable`` |
|       - | 1173 | `	 * without reading it, so an undefined one must not warn (php stays silent` |
|       - | 1174 | `	 * for both). Every other read of a missing variable warns — see OP_LOAD.` |
|       - | 1175 | `	 * The two flags are cleared before recursing into a subscript's index` |
|       - | 1176 | ``	 * expression, so `isset($a[$i])` still warns for an undefined $i, as php`` |
|       - | 1177 | ``	 * does. Contexts that VIVIFY (assignment targets, `??`, appends) already`` |
|       - | 1178 | `	 * emit iP1 = 0 and never reach the warning. */` |
| 2356121 | 1179 | `	if( iCompileFlag & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_QUIET_VAR) ){` |
|   10357 | 1180 | `		iP2 = 1;` |
| 2350945 | 1181 | `	}else if( iCompileFlag & EXPR_FLAG_RMW_LOAD ){` |
|       - | 1182 | ``		/* Warn-then-create: the read half of `$x++` / `$x .= ...` still needs a`` |
|       - | 1183 | `		 * writable slot, so it cannot use the read-only load above. */` |
|   57287 | 1184 | `		iP2 = 2;` |
| 2317128 | 1185 | `	}else if( iCompileFlag & EXPR_FLAG_DEFER_ARG ){` |
|       - | 1186 | ``		/* D1 deferred call argument. For a plain `$var` (iVv == 0, p3 holds the name)`` |
|       - | 1187 | `		 * emit the deferred load (iP1 stays 1 = no create, iP2 = 3): an undefined` |
|       - | 1188 | `		 * variable is left uncreated and silent, carrying a lazy-lvalue marker that` |
|       - | 1189 | `		 * OP_CALL resolves against the callee's by-ref flags. A variable-variable` |
|       - | 1190 | `		 * ($$x) computes its name on the stack (p3 == 0), so it cannot carry the` |
|       - | 1191 | `		 * marker — fall back to the historical eager create (iP1 = 0), which keeps` |
|       - | 1192 | `		 * its by-ref binding working exactly as before. */` |
|  456311 | 1193 | `		if( iVv == 0 ){` |
|  456305 | 1194 | `			iP2 = 3;` |
|  228155 | 1195 | `		}else{` |
|       9 | 1196 | `			iP1 = 0;` |
|       - | 1197 | `		}` |
|  228153 | 1198 | `	}` |
|       - | 1199 | `	/* Emit the load instruction(s). For a variable-variable ($$x, $$$x, ...) every` |
|       - | 1200 | `	 * load EXCEPT the final dereference resolves a NAME: a pure read that warns on an` |
|       - | 1201 | `	 * undefined name (php) and never creates it. Only the last load is the actual` |
|       - | 1202 | `	 * variable and carries the caller's write/create context (iP1). Emitting the` |
|       - | 1203 | ``	 * outer create-mode for the name loads silently invented $n in `$$n = 5` and`` |
|       - | 1204 | ``	 * skipped php's `Undefined variable $n` warning; a quiet outer (isset/empty)`` |
|       - | 1205 | `	 * still suppresses the name warning as php does. */` |
| 2356121 | 1206 | `	if( iVv > 0 ){` |
|      40 | 1207 | `		sxi32 iP2Name = (iCompileFlag & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY))` |
|       - | 1208 | ``			? 1 /* isset()/empty() suppress the name warning too; `??` (QUIET_VAR)`` |
|      18 | 1209 | `			     * does NOT — it warns the name and quiets only the target read. */ : 0;` |
|      40 | 1210 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,1/* read-only name */,iP2Name,p3,0);` |
|      42 | 1211 | `		while( iVv > 1 ){` |
|       3 | 1212 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,1/* read-only name */,iP2Name,0,0);` |
|       3 | 1213 | `			iVv--;` |
|       1 | 1214 | `		}` |
|       - | 1215 | `		/* Final dereference: the actual variable, in the caller's context. */` |
|      40 | 1216 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,iP2,0,0);` |
|      22 | 1217 | `	}else{` |
| 2356085 | 1218 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,iP2,p3,0);` |
|       - | 1219 | `	}` |
|       - | 1220 | `	/* Node successfully compiled */` |
| 2356121 | 1221 | `	return SXRET_OK;` |
| 1178065 | 1222 | `}` |
|       - | 1223 | `/*` |
|       - | 1224 | ` * Load a literal.` |
|       - | 1225 | ` */` |
|  915520 | 1226 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       5 | 1227 | `{` |
|  915525 | 1228 | `	SyToken *pToken = pGen->pIn;` |
|       - | 1229 | `	ph7_value *pObj;` |
|       - | 1230 | `	SyString *pStr;` |
|       - | 1231 | `	SyString sCanon;` |
|       - | 1232 | `	sxu32 nIdx;` |
|       - | 1233 | `	/* Extract token value */` |
|  915525 | 1234 | `	pStr = &pToken->sData;` |
|       - | 1235 | `	/* php's MAGIC constants are case-insensitive like the rest of its reserved words —` |
|       - | 1236 | ``	 * `__line__`, `__Dir__` and `__CLASS__` are one constant each — but every one of them`` |
|       - | 1237 | `	 * is recognised by a BYTE-EXACT compare: the compile-time branches just below, and` |
|       - | 1238 | ``	 * `__CLASS__` through constant.c's (deliberately case-sensitive) constant table. Fold`` |
|       - | 1239 | `	 * the spelling to the canonical upper case here, once, so both mechanisms see it; any` |
|       - | 1240 | ``	 * other spelling used to reach the plain-literal path and raise `Undefined constant`` |
|       - | 1241 | ``	 * "__line__"`. Two of the branches below also read a single byte to tell a pair apart`` |
|       - | 1242 | ``	 * (`zString[2]` for __DIR__ vs __FILE__ and __METHOD__ vs __FUNCTION__), which only`` |
|       - | 1243 | `	 * works on the canonical form.` |
|       - | 1244 | `	 *` |
|       - | 1245 | `	 * User constants stay case-SENSITIVE (php) — this fold is limited to the eight names` |
|       - | 1246 | ``	 * below, and skips a member NAME, where `C::__LINE__` is an ordinary class constant. */`` |
|  915520 | 1247 | `	if( (pToken->nType & PH7_TK_MEMBER_NAME) == 0 && pStr->nByte > 4` |
|  814082 | 1248 | `		&& pStr->zString[0] == '_' && pStr->zString[1] == '_' ){` |
|       - | 1249 | `		static const char * const azMagic[] = {` |
|       - | 1250 | `			"__LINE__", "__FILE__", "__DIR__", "__FUNCTION__", "__CLASS__",` |
|       - | 1251 | `			"__METHOD__", "__NAMESPACE__", "__TRAIT__"` |
|       - | 1252 | `		};` |
|       - | 1253 | `		sxu32 i;` |
|    1439 | 1254 | `		for( i = 0 ; i < SX_ARRAYSIZE(azMagic) ; ++i ){` |
|    1425 | 1255 | `			sxu32 nMagic = SyStrlen(azMagic[i]);` |
|    1425 | 1256 | `			if( pStr->nByte == nMagic && SyStrnicmp(pStr->zString,azMagic[i],nMagic) == 0 ){` |
|     327 | 1257 | `				SyStringInitFromBuf(&sCanon,azMagic[i],nMagic);` |
|     327 | 1258 | `				pStr = &sCanon;` |
|     327 | 1259 | `				break;` |
|       - | 1260 | `			}` |
|     554 | 1261 | `		}` |
|     168 | 1262 | `	}` |
|       - | 1263 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first. A reserved` |
|       - | 1264 | `	 * word used as a member NAME (Enum::Null, C::Array, $o->list()) is a plain` |
|       - | 1265 | `	 * identifier — the parser flagged its token so the whole value-folding chain is` |
|       - | 1266 | `	 * skipped and it falls through to the ordinary string-literal emit below. */` |
|  915525 | 1267 | `	if( pToken->nType & PH7_TK_MEMBER_NAME ){` |
|       - | 1268 | `		/* fall through to the plain-string literal path */` |
|  900957 | 1269 | `	}else if( pStr->nByte == sizeof("NULL") - 1 ){` |
|   76391 | 1270 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - | 1271 | `			/* NULL constant are always indexed at 0 */` |
|   38817 | 1272 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   38817 | 1273 | `			return SXRET_OK;` |
|   37579 | 1274 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - | 1275 | `			/* TRUE constant are always indexed at 1 */` |
|   21131 | 1276 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|   21131 | 1277 | `			return SXRET_OK;` |
|       5 | 1278 | `		}` |
|  897908 | 1279 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|  159362 | 1280 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - | 1281 | `			/* FALSE constant are always indexed at 2 */` |
|  117761 | 1282 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|  117761 | 1283 | `			return SXRET_OK;` |
|  716519 | 1284 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   48544 | 1285 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - | 1286 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|      18 | 1287 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      18 | 1288 | `			if( pObj == 0 ){` |
|     ! 0 | 1289 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1290 | `				return SXERR_ABORT;` |
|       - | 1291 | `			}` |
|      18 | 1292 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - | 1293 | `			/* Emit the load constant instruction */` |
|      18 | 1294 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      18 | 1295 | `			return SXRET_OK;` |
|  716490 | 1296 | `	}else if( (pStr->nByte == sizeof("__FILE__") - 1 &&` |
|   68381 | 1297 | `		SyMemcmp(pStr->zString,"__FILE__",sizeof("__FILE__")-1) == 0) \|\|` |
|  711956 | 1298 | `		(pStr->nByte == sizeof("__DIR__") - 1 &&` |
|   39696 | 1299 | `		SyMemcmp(pStr->zString,"__DIR__",sizeof("__DIR__")-1) == 0) ){` |
|       - | 1300 | `			/* __FILE__ / __DIR__ are magic constants resolved at COMPILE time to the` |
|       - | 1301 | `			 * file being compiled (where the token is written), NOT the runtime` |
|       - | 1302 | `			 * execution file. A function defined in a.php reporting __FILE__ must say` |
|       - | 1303 | `			 * a.php even when called from b.php — php semantics, and what Composer's` |
|       - | 1304 | ``			 * autoloader (loadClassLoader's `require __DIR__ . '/ClassLoader.php'`)`` |
|       - | 1305 | `			 * relies on. The runtime-constant path returned the caller's file. */` |
|     177 | 1306 | `			int bDir = (pStr->zString[2] == 'D'); /* __DIR__ vs __FILE__ */` |
|     177 | 1307 | `			SyString *pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     177 | 1308 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     177 | 1309 | `			if( pObj == 0 ){` |
|     ! 0 | 1310 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1311 | `				return SXERR_ABORT;` |
|       - | 1312 | `			}` |
|     177 | 1313 | `			if( pFile && pFile->nByte > 0 ){` |
|     177 | 1314 | `				if( bDir ){` |
|       - | 1315 | `					const char *zDir;` |
|       - | 1316 | `					int nLen;` |
|       - | 1317 | `					SyString sDir;` |
|     113 | 1318 | `					zDir = PH7_ExtractDirName(pFile->zString,(int)pFile->nByte,&nLen);` |
|     113 | 1319 | `					SyStringInitFromBuf(&sDir,zDir,nLen);` |
|     113 | 1320 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&sDir);` |
|      59 | 1321 | `				}else{` |
|      69 | 1322 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,pFile);` |
|       - | 1323 | `				}` |
|      91 | 1324 | `			}else{` |
|       - | 1325 | `				SyString sMem;` |
|     ! 0 | 1326 | `				SyStringInitFromBuf(&sMem,":MEMORY:",sizeof(":MEMORY:")-1);` |
|     ! 0 | 1327 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sMem);` |
|       - | 1328 | `			}` |
|     177 | 1329 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     177 | 1330 | `			return SXRET_OK;` |
|  706831 | 1331 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   29544 | 1332 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|       - | 1333 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|      14 | 1334 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      14 | 1335 | `			if( pObj == 0 ){` |
|     ! 0 | 1336 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1337 | `				return SXERR_ABORT;` |
|       - | 1338 | `			}` |
|      14 | 1339 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - | 1340 | `				SyString sNs;` |
|       7 | 1341 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       7 | 1342 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|       4 | 1343 | `			}else{` |
|       7 | 1344 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - | 1345 | `			}` |
|      14 | 1346 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      14 | 1347 | `			return SXRET_OK;` |
|  726697 | 1348 | `	}else if( pStr->nByte == sizeof("__TRAIT__") - 1 &&` |
|   69300 | 1349 | `		SyMemcmp(pStr->zString,"__TRAIT__",sizeof("__TRAIT__")-1) == 0 ){` |
|       - | 1350 | `			/* __TRAIT__ magic constant: the name of the trait whose SOURCE lexically` |
|       - | 1351 | `			 * encloses this token. php resolves it at compile time, and it is shared` |
|       - | 1352 | `			 * across every using class because a trait method body compiles ONCE with` |
|       - | 1353 | `			 * the trait as its owner (PH7_ClassUseTrait adopts the same method pointer).` |
|       - | 1354 | `			 * Unlike __FUNCTION__/__METHOD__ (nearest function), __TRAIT__ is LEXICAL:` |
|       - | 1355 | `			 * closures and arrow-fns are TRANSPARENT (a closure inside a trait method still` |
|       - | 1356 | `			 * yields the trait), so we skip them and keep walking outward — but a class` |
|       - | 1357 | `			 * method or a plain function is an OPAQUE lexical boundary that fixes the answer.` |
|       - | 1358 | `			 * An anonymous class defined inside a trait method is a fresh scope, so` |
|       - | 1359 | `			 * __TRAIT__ is "" there, not the enclosing trait. "" outside any trait (global` |
|       - | 1360 | `			 * scope, plain functions, non-trait methods) — php renders it the empty string,` |
|       - | 1361 | `			 * not NULL. */` |
|      56 | 1362 | `			ph7_class *pTrait = 0;` |
|      56 | 1363 | `			if( pGen->iInMemberDefault > 0 ){` |
|       - | 1364 | `				/* A property/parameter DEFAULT is a const-expression that belongs to the` |
|       - | 1365 | `				 * class whose body is being compiled (pCurClass), never to a lexically-` |
|       - | 1366 | `				 * enclosing method. Read pCurClass directly — the block chain has no func` |
|       - | 1367 | `				 * block for the default and would leak into the enclosing function (an` |
|       - | 1368 | `				 * anonymous class's default inside a trait method is the anon's scope, "").*/` |
|      13 | 1369 | `				if( pGen->pCurClass && (pGen->pCurClass->iFlags & PH7_CLASS_TRAIT) ){` |
|       5 | 1370 | `					pTrait = pGen->pCurClass;` |
|       2 | 1371 | `				}` |
|       7 | 1372 | `			}else{` |
|      44 | 1373 | `				GenBlock *pBlock = pGen->pCurrent;` |
|     106 | 1374 | `				while( pBlock ){` |
|     102 | 1375 | `					if( pBlock->iFlags & GEN_BLOCK_FUNC ){` |
|      55 | 1376 | `						ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      55 | 1377 | `						if( pFunc == 0 ){` |
|       - | 1378 | `							/* A SYNTHETIC function block carries no ph7_vm_func — e.g. the` |
|       - | 1379 | `							 * per-arm throw-fixup block GenStateCompileMatchSubExpr enters to` |
|       - | 1380 | `							 * host a match() expression. It is not a real lexical scope` |
|       - | 1381 | `							 * boundary, so stay transparent and keep walking outward. */` |
|       6 | 1382 | `							pBlock = pBlock->pParent;` |
|       6 | 1383 | `							continue;` |
|       - | 1384 | `						}` |
|      51 | 1385 | `						if( pFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|       - | 1386 | `							/* A class method (of any class, including an anonymous one) is an` |
|       - | 1387 | `							 * OPAQUE scope boundary and fixes the answer: a trait method yields` |
|       - | 1388 | `							 * its trait, any other class's method yields "". Tested BEFORE the` |
|       - | 1389 | `							 * closure flags so a static method is never mistaken for transparent. */` |
|      34 | 1390 | `							if( pFunc->pUserData` |
|      37 | 1391 | `								&& (((ph7_class *)pFunc->pUserData)->iFlags & PH7_CLASS_TRAIT) ){` |
|      31 | 1392 | `								pTrait = (ph7_class *)pFunc->pUserData;` |
|      14 | 1393 | `							}` |
|      37 | 1394 | `							break;` |
|       - | 1395 | `						}` |
|      15 | 1396 | `						if( pFunc->iFlags & (VM_FUNC_CLOSURE\|VM_FUNC_ARROW\|VM_FUNC_STATIC_CL) ){` |
|       - | 1397 | `							/* Closure / arrow fn (VM_FUNC_CLOSURE is only set when the closure` |
|       - | 1398 | `							 * captures, so a capture-less static closure carries only` |
|       - | 1399 | `							 * VM_FUNC_STATIC_CL — include it). Transparent: keep walking outward. */` |
|      13 | 1400 | `							pBlock = pBlock->pParent;` |
|      13 | 1401 | `							continue;` |
|       - | 1402 | `						}` |
|       - | 1403 | `						/* A plain named function is an opaque boundary: __TRAIT__ is "". */` |
|       3 | 1404 | `						break;` |
|       - | 1405 | `					}` |
|      50 | 1406 | `					pBlock = pBlock->pParent;` |
|       4 | 1407 | `				}` |
|       - | 1408 | `			}` |
|      56 | 1409 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      56 | 1410 | `			if( pObj == 0 ){` |
|     ! 0 | 1411 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1412 | `				return SXERR_ABORT;` |
|       - | 1413 | `			}` |
|      56 | 1414 | `			if( pTrait ){` |
|      36 | 1415 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&pTrait->sName);` |
|      20 | 1416 | `			}else{` |
|      22 | 1417 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0); /* empty string */` |
|       - | 1418 | `			}` |
|      56 | 1419 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      56 | 1420 | `			return SXRET_OK;` |
|  711554 | 1421 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|   80750 | 1422 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
|  733561 | 1423 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   83234 | 1424 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      61 | 1425 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - | 1426 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|       - | 1427 | `			/* Skip SYNTHETIC function blocks (GEN_BLOCK_FUNC with no ph7_vm_func in` |
|       - | 1428 | `			 * pUserData — e.g. the per-arm throw-fixup block a match() expression enters):` |
|       - | 1429 | `			 * they are not real function scopes. Without this a __FUNCTION__/__METHOD__` |
|       - | 1430 | `			 * inside a match arm reached a NULL pUserData and dereferenced it (compile-time` |
|       - | 1431 | `			 * crash); php resolves to the enclosing real function, which the walk now finds. */` |
|     152 | 1432 | `			while( pBlock && ((pBlock->iFlags & GEN_BLOCK_FUNC) == 0 \|\| pBlock->pUserData == 0) ){` |
|       - | 1433 | `				/* Point to the upper block */` |
|      65 | 1434 | `				pBlock = pBlock->pParent;` |
|       3 | 1435 | `			}` |
|      61 | 1436 | `			if( pBlock == 0 ){` |
|       - | 1437 | `				/* Called in the global scope,load NULL */` |
|       5 | 1438 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 | 1439 | `			}else{` |
|       - | 1440 | `				/* Extract the target function/method */` |
|      57 | 1441 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      57 | 1442 | `				int bMethod = (pStr->zString[2] == 'M'); /* __METHOD__ vs __FUNCTION__ */` |
|      57 | 1443 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      57 | 1444 | `				if( pObj == 0 ){` |
|     ! 0 | 1445 | `					PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1446 | `					return SXERR_ABORT;` |
|       - | 1447 | `				}` |
|       - | 1448 | `				/*` |
|       - | 1449 | `				 * __METHOD__ is the QUALIFIED name: "C::m" inside a method, and the plain` |
|       - | 1450 | `				 * function name inside a plain function (php does not answer "" there —` |
|       - | 1451 | `				 * PH7 loaded NULL, so __METHOD__ was empty in every free function and` |
|       - | 1452 | `				 * unqualified in every method).` |
|       - | 1453 | `				 */` |
|       - | 1454 | ``				/* A property hook answers php's `$p::get`, never the method name`` |
|       - | 1455 | ``				 * PHL synthesizes for it — so __METHOD__ reads `C::$p::get`, the`` |
|       - | 1456 | `				 * same rendering the runtime diagnostics use. */` |
|       - | 1457 | `				{` |
|       - | 1458 | `					SyString sProp,sSelf;` |
|       - | 1459 | `					const char *zKind;` |
|       - | 1460 | `					SyBlob sQual;` |
|       - | 1461 | `					SyString sOut;` |
|      57 | 1462 | `					int bHook = PH7_VmHookSplitName(&pFunc->sName,&sProp,&zKind);` |
|      57 | 1463 | `					SyBlobInit(&sQual,&pGen->pVm->sAllocator);` |
|      57 | 1464 | `					if( bHook ){` |
|       7 | 1465 | `						SyBlobFormat(&sQual,"$%z::%s",&sProp,zKind);` |
|       7 | 1466 | `						SyStringInitFromBuf(&sSelf,SyBlobData(&sQual),SyBlobLength(&sQual));` |
|      54 | 1467 | `					}else if( SyStringLength(&pFunc->sClosureName) > 0 ){` |
|       - | 1468 | ``						/* A closure answers php's `{closure:...}` name, never the`` |
|       - | 1469 | `						 * synthesized lookup key — and __METHOD__ answers the SAME text` |
|       - | 1470 | `						 * (the name already carries the declaring class), so the` |
|       - | 1471 | `						 * qualification below must not run for it. */` |
|      15 | 1472 | `						sSelf = pFunc->sClosureName;` |
|      15 | 1473 | `						bMethod = 0;` |
|       8 | 1474 | `					}else{` |
|      37 | 1475 | `						sSelf = pFunc->sName;` |
|       - | 1476 | `					}` |
|      65 | 1477 | `					if( bMethod && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|      19 | 1478 | `						SyString *pCls = &((ph7_class *)pFunc->pUserData)->sName;` |
|       - | 1479 | `						SyBlob sFull;` |
|      19 | 1480 | `						SyBlobInit(&sFull,&pGen->pVm->sAllocator);` |
|      19 | 1481 | `						SyBlobFormat(&sFull,"%z::%z",pCls,&sSelf);` |
|      19 | 1482 | `						SyStringInitFromBuf(&sOut,SyBlobData(&sFull),SyBlobLength(&sFull));` |
|      19 | 1483 | `						PH7_MemObjInitFromString(pGen->pVm,pObj,&sOut);` |
|      19 | 1484 | `						SyBlobRelease(&sFull);` |
|      11 | 1485 | `					}else{` |
|      41 | 1486 | `						PH7_MemObjInitFromString(pGen->pVm,pObj,&sSelf);` |
|       - | 1487 | `					}` |
|      57 | 1488 | `					SyBlobRelease(&sQual);` |
|       - | 1489 | `				}` |
|       - | 1490 | `				/* Emit the load constant instruction */` |
|      57 | 1491 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 1492 | `			}` |
|      61 | 1493 | `			return SXRET_OK;` |
|       - | 1494 | `	}` |
|       - | 1495 | ``	/* php keywords are CASE-INSENSITIVE (`SELF::C`, `Parent::m()`, `new STATIC`,`` |
|       - | 1496 | ``	 * `ISSET($x)`), but a few of them reach the engine as THIS literal and are matched`` |
|       - | 1497 | `	 * there BYTE-EXACTLY: the scope keywords against "self"/"parent"/"static" (OP_MEMBER,` |
|       - | 1498 | `	 * OP_NEW, the FCC scope resolver, the error formatter), and isset/empty/eval as the` |
|       - | 1499 | `	 * name of the host function their call dispatches to. Emit the canonical lower-case` |
|       - | 1500 | `	 * spelling for exactly those so the source's case never reaches the match — PH7` |
|       - | 1501 | ``	 * emitted the raw text, so `SELF::C` looked for a class literally named "SELF" and`` |
|       - | 1502 | ``	 * `ISSET($x)` for a function named "ISSET".`` |
|       - | 1503 | `	 *` |
|       - | 1504 | `	 * Every OTHER keyword literal keeps its source case on purpose: it is a CONSTANT` |
|       - | 1505 | ``	 * read (`define('OBJECT',1); echo OBJECT;` — PHL's keyword table covers type names`` |
|       - | 1506 | `	 * php's lexer does not reserve), and php constants are case-SENSITIVE. So is a` |
|       - | 1507 | ``	 * keyword used as a member NAME, which the parser flags: `class C { const STATIC = 5; }`` |
|       - | 1508 | ``	 * echo C::STATIC;` names the constant "STATIC", and folding it looked for "static". */`` |
|  737521 | 1509 | `	if( (pToken->nType & PH7_TK_KEYWORD) && (pToken->nType & PH7_TK_MEMBER_NAME) == 0 ){` |
|   10563 | 1510 | `		sxu32 nKeyID = (sxu32)SX_PTR_TO_INT(pToken->pUserData);` |
|   10563 | 1511 | `		const char *zCanon = 0;` |
|   10563 | 1512 | `		if( nKeyID == PH7_TKWRD_SELF ){` |
|     217 | 1513 | `			zCanon = "self";` |
|   10457 | 1514 | `		}else if( nKeyID == PH7_TKWRD_PARENT ){` |
|      93 | 1515 | `			zCanon = "parent";` |
|   10306 | 1516 | `		}else if( nKeyID == PH7_TKWRD_STATIC ){` |
|      90 | 1517 | `			zCanon = "static";` |
|   10218 | 1518 | `		}else if( nKeyID == PH7_TKWRD_ISSET ){` |
|    9911 | 1519 | `			zCanon = "isset";` |
|    5222 | 1520 | `		}else if( nKeyID == PH7_TKWRD_EMPTY ){` |
|     157 | 1521 | `			zCanon = "empty";` |
|     193 | 1522 | `		}else if( nKeyID == PH7_TKWRD_EVAL ){` |
|     103 | 1523 | `			zCanon = "eval";` |
|      49 | 1524 | `		}` |
|   10563 | 1525 | `		if( zCanon ){` |
|   10549 | 1526 | `			SyStringInitFromBuf(&sCanon,zCanon,SyStrlen(zCanon));` |
|   10549 | 1527 | `			pStr = &sCanon;` |
|    5272 | 1528 | `		}` |
|    5279 | 1529 | `	}` |
|       - | 1530 | `	/* Query literal table */` |
|  737521 | 1531 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - | 1532 | `		ph7_value *pLitObj;` |
|       - | 1533 | `		/* Unknown literal,install it in the literal table */` |
|  339999 | 1534 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  339999 | 1535 | `		if( pLitObj == 0 ){` |
|     ! 0 | 1536 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 1537 | `			return SXERR_ABORT;` |
|       - | 1538 | `		}` |
|  339999 | 1539 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,pStr);` |
|  339999 | 1540 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  169997 | 1541 | `	}` |
|       - | 1542 | `	/* Emit the load constant instruction.` |
|       - | 1543 | `	 *` |
|       - | 1544 | `	 * php resolves an UNQUALIFIED constant against the namespace its SOURCE sits in,` |
|       - | 1545 | `	 * decided at COMPILE time — so a function keeps its own namespace when called from` |
|       - | 1546 | ``	 * another one — and in a fixed order: a `use const` import first (and an import has`` |
|       - | 1547 | ``	 * NO global fallback), else `current-namespace\NAME`, else the global `NAME`. The`` |
|       - | 1548 | `	 * candidate that order picks is resolved here and travels in the instruction's p3;` |
|       - | 1549 | `	 * the VM's own lookup of the bare literal is then just the global step, and the` |
|       - | 1550 | `	 * "Undefined constant" message names the candidate, as php does.` |
|       - | 1551 | `	 *` |
|       - | 1552 | `	 * Only a name that could BE a constant needs one: a keyword literal never is (the` |
|       - | 1553 | `	 * scope keywords and isset/empty/eval reach the OO and call handlers by this same` |
|       - | 1554 | `	 * literal), and a call/new site clears PH7_LOADC_EXPAND before the constant path` |
|       - | 1555 | `	 * can ever run. */` |
|       - | 1556 | `	{` |
|  737521 | 1557 | `		sxi32 iLoadFlags = PH7_LOADC_EXPAND;` |
|  737521 | 1558 | `		char *zCand = 0;` |
|  737521 | 1559 | `		if( (pToken->nType & PH7_TK_KEYWORD) == 0 ){` |
| 1089926 | 1560 | `			SyHashEntry *pImport = SyHashGet(&pGen->hUseConstImports,` |
|  726614 | 1561 | `				(const void *)pStr->zString,pStr->nByte);` |
|  726619 | 1562 | `			if( pImport ){` |
|      31 | 1563 | `				const char *zFQN = (const char *)pImport->pUserData;` |
|      31 | 1564 | `				zCand = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFQN,SyStrlen(zFQN));` |
|      31 | 1565 | `				iLoadFlags \|= PH7_LOADC_NOGLOBAL;` |
|  726606 | 1566 | `			}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - | 1567 | `				SyBlob sCand;` |
|     481 | 1568 | `				SyBlobInit(&sCand,&pGen->pVm->sAllocator);` |
|     481 | 1569 | `				SyBlobAppend(&sCand,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     481 | 1570 | `				SyBlobAppend(&sCand,"\\",1);` |
|     481 | 1571 | `				SyBlobAppend(&sCand,pStr->zString,pStr->nByte);` |
|     719 | 1572 | `				zCand = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     476 | 1573 | `					(const char *)SyBlobData(&sCand),SyBlobLength(&sCand));` |
|     481 | 1574 | `				SyBlobRelease(&sCand);` |
|     238 | 1575 | `			}` |
|  363307 | 1576 | `		}` |
|  737521 | 1577 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,iLoadFlags,nIdx,zCand,0);` |
|       - | 1578 | `	}` |
|  737521 | 1579 | `	return SXRET_OK;` |
|  457765 | 1580 | `}` |
|       - | 1581 | `/*` |
|       - | 1582 | ` * Resolve a namespace path or simply load a literal.` |
|       - | 1583 | ` * If the token stream contains namespace separators (backslashes),` |
|       - | 1584 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - | 1585 | ` * Otherwise, load the simple literal directly.` |
|       - | 1586 | ` */` |
|  915728 | 1587 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       5 | 1588 | `{` |
|       - | 1589 | `	sxi32 rc;` |
|  915733 | 1590 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 1591 | `		return SXRET_OK;` |
|       - | 1592 | `	}` |
|       - | 1593 | `	/* Check if this is a multi-token namespace path */` |
|  915733 | 1594 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - | 1595 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|     213 | 1596 | `		SyBlob *pWorker = &pGen->sWorker;` |
|     213 | 1597 | `		int isAbsolute = 0;` |
|     213 | 1598 | `		SyBlobReset(pWorker);` |
|       - | 1599 | `		/* Check for leading backslash (absolute path) */` |
|     213 | 1600 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|     119 | 1601 | `			isAbsolute = 1;` |
|     119 | 1602 | `			pGen->pIn++; /* Skip leading backslash */` |
|      57 | 1603 | `		}` |
|       - | 1604 | `		/* Collect the raw path (no prefix yet) so its FIRST segment can be resolved` |
|       - | 1605 | ``		 * against use-imports below — php resolves `A\B\C` by mapping the leading`` |
|       - | 1606 | ``		 * `A` through the imports (`use X\A;` makes it `X\A\B\C`), and only prepends`` |
|       - | 1607 | ``		 * the current namespace when `A` matches no import. Blindly prefixing the`` |
|       - | 1608 | ``		 * namespace here produced e.g. `Ns\Ev\Bus` for `use ...\Event as Ev; Ev\Bus`. */`` |
|       - | 1609 | `		{` |
|       - | 1610 | `			SyBlob sRaw;` |
|     213 | 1611 | `			SyBlobInit(&sRaw,&pGen->pVm->sAllocator);` |
|       - | 1612 | ``			/* `namespace\X` spells the CURRENT namespace and is FULLY QUALIFIED from`` |
|       - | 1613 | `			 * there: no import ever applies to it, and the namespace is already in. */` |
|     213 | 1614 | `			if( !isAbsolute && GenStateNsRelPrefix(pGen,&pGen->pIn,pGen->pEnd,&sRaw) ){` |
|      68 | 1615 | `				isAbsolute = 1;` |
|      33 | 1616 | `			}` |
|     433 | 1617 | `			while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|     433 | 1618 | `				if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|     115 | 1619 | `					SyBlobAppend(&sRaw,"\\",1);` |
|      60 | 1620 | `				}else{` |
|     323 | 1621 | `					SyBlobAppend(&sRaw,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - | 1622 | `				}` |
|     433 | 1623 | `				if( pGen->pIn == &pGen->pEnd[-1] ){` |
|     213 | 1624 | `					pGen->pIn++;` |
|     213 | 1625 | `					break;` |
|       - | 1626 | `				}` |
|     225 | 1627 | `				pGen->pIn++;` |
|       5 | 1628 | `			}` |
|     213 | 1629 | `			if( isAbsolute ){` |
|     185 | 1630 | `				SyBlobAppend(pWorker,SyBlobData(&sRaw),SyBlobLength(&sRaw)); /* FQN as written */` |
|      95 | 1631 | `			}else{` |
|      31 | 1632 | `				const char *zRaw = (const char *)SyBlobData(&sRaw);` |
|      31 | 1633 | `				sxu32 nRaw = SyBlobLength(&sRaw);` |
|      31 | 1634 | `				sxu32 nFirst = 0;` |
|       - | 1635 | `				SyHashEntry *pNsImp;` |
|     125 | 1636 | `				while( nFirst < nRaw && zRaw[nFirst] != '\\' ){ nFirst++; }` |
|      31 | 1637 | `				pNsImp = SyHashGet(&pGen->hUseImports,(const void *)zRaw,nFirst);` |
|      31 | 1638 | `				if( pNsImp ){` |
|       - | 1639 | `					/* Leading segment is an imported alias: substitute its FQN. */` |
|      26 | 1640 | `					const char *zFQN = (const char *)pNsImp->pUserData;` |
|      26 | 1641 | `					SyBlobAppend(pWorker,zFQN,SyStrlen(zFQN));` |
|      26 | 1642 | `					SyBlobAppend(pWorker,zRaw + nFirst,nRaw - nFirst);` |
|      18 | 1643 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       6 | 1644 | `					SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       6 | 1645 | `					SyBlobAppend(pWorker,"\\",1);` |
|       6 | 1646 | `					SyBlobAppend(pWorker,zRaw,nRaw);` |
|       4 | 1647 | `				}else{` |
|     ! 0 | 1648 | `					SyBlobAppend(pWorker,zRaw,nRaw); /* global scope, no import */` |
|       - | 1649 | `				}` |
|       - | 1650 | `			}` |
|     213 | 1651 | `			SyBlobRelease(&sRaw);` |
|       - | 1652 | `		}` |
|     213 | 1653 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - | 1654 | `			ph7_value *pObj;` |
|       - | 1655 | `			SyString sPath;` |
|       - | 1656 | `			sxu32 nIdx;` |
|     213 | 1657 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - | 1658 | `			/* Install in the literal table */` |
|     213 | 1659 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|     105 | 1660 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     105 | 1661 | `				if( pObj == 0 ){` |
|     ! 0 | 1662 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 1663 | `					return SXERR_ABORT;` |
|       - | 1664 | `				}` |
|     105 | 1665 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|     105 | 1666 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      50 | 1667 | `			}` |
|       - | 1668 | `			/* Emit the load constant instruction.` |
|       - | 1669 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|       - | 1670 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|     317 | 1671 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|     104 | 1672 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|     104 | 1673 | `				nIdx,0,0);` |
|     213 | 1674 | `			return SXRET_OK;` |
|       - | 1675 | `		}` |
|     ! 0 | 1676 | `	}` |
|       - | 1677 | `	/* Single-token literal: load directly */` |
|  915525 | 1678 | `	rc = GenStateLoadLiteral(&(*pGen));` |
|  915525 | 1679 | `	return rc;` |
|  457869 | 1680 | `}` |
|       - | 1681 | `/*` |
|       - | 1682 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - | 1683 | ` */` |
|       - | 1684 | `/*` |
|       - | 1685 | `` * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of`` |
|       - | 1686 | `` * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument`` |
|       - | 1687 | ``  * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...` `` |
|       - | 1688 | ` * appeared outside a call argument list — a syntax error (PHP rejects it likewise).` |
|       - | 1689 | ` */` |
|     ! 0 | 1690 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|     ! 0 | 1691 | `{` |
|     ! 0 | 1692 | `	SXUNUSED(iCompileFlag);` |
|     ! 0 | 1693 | `	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|       - | 1694 | `		"Cannot use the first-class callable syntax '...' here");` |
|     ! 0 | 1695 | `	return SXERR_SYNTAX;` |
|     ! 0 | 1696 | `}` |
|  915728 | 1697 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 | 1698 | `{` |
|       - | 1699 | `	sxi32 rc;` |
|  915733 | 1700 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
|  915733 | 1701 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1702 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1703 | `		return rc;` |
|       - | 1704 | `	}` |
|       - | 1705 | `	/* Node successfully compiled */` |
|  915733 | 1706 | `	return SXRET_OK;` |
|  457869 | 1707 | `}` |
|       - | 1708 |  |
