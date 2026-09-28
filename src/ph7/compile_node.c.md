# src/ph7/compile_node.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 849/981 lines (86.54%)

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
|   10560 |   37 | `static void GenStateClosureName(ph7_gen_state *pGen,sxu32 nLine)` |
|       5 |   38 | `{` |
|   10565 |   39 | `	ph7_vm_func *pOuter = 0;` |
|   10565 |   40 | `	GenBlock *pBlock = pGen->pCurrent;` |
|       - |   41 | `	SyBlob sName;` |
|       - |   42 | `	char *zDup;` |
|       - |   43 | `	/* Innermost REAL function block: a synthetic one (a match() arm's throw-fixup` |
|       - |   44 | `	 * block) carries no ph7_vm_func and is not a scope. */` |
|   21965 |   45 | `	while( pBlock ){` |
|   11963 |   46 | `		if( (pBlock->iFlags & GEN_BLOCK_FUNC) && pBlock->pUserData ){` |
|     563 |   47 | `			pOuter = (ph7_vm_func *)pBlock->pUserData;` |
|     563 |   48 | `			break;` |
|       - |   49 | `		}` |
|   11405 |   50 | `		pBlock = pBlock->pParent;` |
|       5 |   51 | `	}` |
|   10565 |   52 | `	SyStringInitFromBuf(&pGen->sPendingClosureName,0,0);` |
|   10565 |   53 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|   10565 |   54 | `	SyBlobAppend(&sName,"{closure:",sizeof("{closure:")-1);` |
|   10565 |   55 | `	if( pOuter == 0 ){` |
|       - |   56 | `		/* Top level: php writes the compiled file's path (empty when there is none —` |
|       - |   57 | `		 * an eval()/direct-API compile — which is php's "{closure::LINE}" there). */` |
|   10007 |   58 | `		SyString *pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|   10007 |   59 | `		if( pFile && SyStringLength(pFile) > 0 ){` |
|   10007 |   60 | `			SyBlobAppend(&sName,SyStringData(pFile),SyStringLength(pFile));` |
|    5006 |   61 | `		}` |
|    5564 |   62 | `	}else if( SyStringLength(&pOuter->sClosureName) > 0 ){` |
|       - |   63 | `		/* Enclosing closure: its whole name, no parens and no class. */` |
|     461 |   64 | `		SyBlobAppend(&sName,SyStringData(&pOuter->sClosureName),` |
|     152 |   65 | `			SyStringLength(&pOuter->sClosureName));` |
|     157 |   66 | `	}else{` |
|     259 |   67 | `		if( (pOuter->iFlags & VM_FUNC_CLASS_METHOD) && pOuter->pUserData ){` |
|     106 |   68 | `			SyString *pCls = &((ph7_class *)pOuter->pUserData)->sName;` |
|     106 |   69 | `			SyBlobAppend(&sName,SyStringData(pCls),SyStringLength(pCls));` |
|     106 |   70 | `			SyBlobAppend(&sName,"::",2);` |
|      51 |   71 | `		}` |
|     259 |   72 | `		SyBlobAppend(&sName,SyStringData(&pOuter->sName),SyStringLength(&pOuter->sName));` |
|     259 |   73 | `		SyBlobAppend(&sName,"()",2);` |
|       - |   74 | `	}` |
|   10565 |   75 | `	SyBlobFormat(&sName,":%u}",nLine);` |
|   15845 |   76 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|   10560 |   77 | `		(const char *)SyBlobData(&sName),(sxu32)SyBlobLength(&sName));` |
|   10565 |   78 | `	if( zDup ){` |
|   10565 |   79 | `		SyStringInitFromBuf(&pGen->sPendingClosureName,zDup,(sxu32)SyBlobLength(&sName));` |
|    5280 |   80 | `	}` |
|   10565 |   81 | `	SyBlobRelease(&sName);` |
|   10565 |   82 | `}` |
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
|    4744 |  103 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  104 | `{` |
|    4749 |  105 | `	ph7_vm_func *pAnnonFunc = 0; /* Annonymous function body */` |
|       - |  106 | `	char zName[512];         /* Unique lambda name */` |
|       - |  107 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - |  108 | `							  * one thread is allowed to compile the script.` |
|       - |  109 | `						      */` |
|       - |  110 | `	SyString sName;` |
|    4749 |  111 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `$f = #[A] function…` trivia`` |
|       - |  112 | `	                              * is keyed to this ['static'] 'function' token */` |
|       - |  113 | `	sxu32 nKwLine;` |
|    4749 |  114 | `	sxi32 iFlags = 0;` |
|       - |  115 | `	sxu32 nLen;` |
|       - |  116 | `	sxi32 rc;` |
|    2372 |  117 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  118 |  |
|    4749 |  119 | `	nKwLine = pGen->pIn->nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|    4744 |  120 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|    4749 |  121 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  122 | `		/* Static closure: no $this auto-capture, bind refused */` |
|     319 |  123 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|     319 |  124 | `		pGen->pIn++; /* Jump the 'static' keyword */` |
|     157 |  125 | `	}` |
|    4749 |  126 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|    4749 |  127 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 |  128 | `		pGen->pIn++;` |
|     ! 0 |  129 | `	}` |
|       - |  130 | `	/* Generate a unique name */` |
|    4749 |  131 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - |  132 | `	/* Make sure the generated name is unique */` |
|    4749 |  133 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  134 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 |  135 | `	}` |
|    4749 |  136 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|       - |  137 | `	/* php's visible name for this closure, built before the body compiles so a` |
|       - |  138 | `	 * __FUNCTION__ inside it resolves to the same text php reports. */` |
|    4749 |  139 | `	GenStateClosureName(&(*pGen),nKwLine);` |
|       - |  140 | `	/* Compile the lambda body */` |
|    4749 |  141 | `	rc = GenStateCompileFunc(&(*pGen),&sName,iFlags,TRUE,&pAnnonFunc);` |
|    4749 |  142 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  143 | `		return SXERR_ABORT;` |
|       - |  144 | `	}` |
|    4749 |  145 | `	if( pAnnonFunc ){` |
|    4747 |  146 | `		pAnnonFunc->nLine = nKwLine;` |
|       - |  147 | ``		/* Expression-position attributes (`$f = #[A] function () {}`): the trivia`` |
|       - |  148 | `		 * sidecar keys them to the closure's first keyword token. */` |
|    4747 |  149 | `		if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnnonFunc->aAttrs) == SXERR_ABORT ){` |
|     ! 0 |  150 | `			return SXERR_ABORT;` |
|       - |  151 | `		}` |
|    4747 |  152 | `		if( GenStateCheckAttrPlacement(&(*pGen),&pAnnonFunc->aAttrs,2,2,0,0) == SXERR_ABORT ){` |
|     ! 0 |  153 | `			return SXERR_ABORT;` |
|       - |  154 | `		}` |
|       - |  155 | `		/* A closure's own attributes arrive AFTER its body compiled, so the` |
|       - |  156 | `		 * #[\NoDiscard] rules are decided here rather than with the signature. */` |
|    4747 |  157 | `		if( GenStateApplyNoDiscard(&(*pGen),pAnnonFunc,0,0) == SXERR_ABORT ){` |
|     ! 0 |  158 | `			return SXERR_ABORT;` |
|       - |  159 | `		}` |
|    2371 |  160 | `	}` |
|       - |  161 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|       - |  162 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|       - |  163 | `	 * the handler wraps either in a Closure instance. */` |
|    4749 |  164 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|       - |  165 | `	/* Node successfully compiled */` |
|    4749 |  166 | `	return SXRET_OK;` |
|    2377 |  167 | `}` |
|       - |  168 | `/*` |
|       - |  169 | ` * Add a free variable to the arrow function's closure environment, unless` |
|       - |  170 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|       - |  171 | ` * enclosing arrow level, or has already been captured.` |
|       - |  172 | ` */` |
|    3506 |  173 | `static sxi32 GenStateArrowAddCapture(` |
|       - |  174 | `	ph7_gen_state *pGen,` |
|       - |  175 | `	ph7_vm_func *pFunc,` |
|       - |  176 | `	const char *zName,` |
|       - |  177 | `	sxu32 nByte,` |
|       - |  178 | `	SyString *aShadow,` |
|       - |  179 | `	sxu32 nShadow)` |
|       5 |  180 | `{` |
|       - |  181 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  182 | `	ph7_vm_func_closure_env *aEnv;` |
|       - |  183 | `	sxu32 n, nEnv;` |
|       - |  184 | `	char *zDup;` |
|    3511 |  185 | `	if( nByte == 0 ){` |
|     ! 0 |  186 | `		return SXRET_OK;` |
|       - |  187 | `	}` |
|    3506 |  188 | `	if( nByte == sizeof("this")-1` |
|    1879 |  189 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|      15 |  190 | `		return SXRET_OK;` |
|       - |  191 | `	}` |
|    3497 |  192 | `	if( PH7_VmIsAutoGlobal(zName,nByte) ){` |
|       - |  193 | `		/* php never auto-captures an auto-global — it is already visible inside` |
|       - |  194 | `		 * the arrow function. Capturing one was actively destructive here: the` |
|       - |  195 | `		 * install resolves the name through hSuper and so wrote the by-value` |
|       - |  196 | `		 * SNAPSHOT over the superglobal's own slot, which for $GLOBALS froze the` |
|       - |  197 | `		 * whole symbol-table view at closure-creation time for the rest of the` |
|       - |  198 | `		 * program. */` |
|      60 |  199 | `		return SXRET_OK;` |
|       - |  200 | `	}` |
|    3633 |  201 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|     984 |  202 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|     962 |  203 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|     795 |  204 | `			return SXRET_OK;` |
|       - |  205 | `		}` |
|     101 |  206 | `	}` |
|    2649 |  207 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|    2649 |  208 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|    3051 |  209 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|     626 |  210 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|     529 |  211 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|     229 |  212 | `			return SXRET_OK;` |
|       - |  213 | `		}` |
|     205 |  214 | `	}` |
|    2425 |  215 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|    2425 |  216 | `	if( zDup == 0 ){` |
|     ! 0 |  217 | `		return SXERR_ABORT;` |
|       - |  218 | `	}` |
|    2425 |  219 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|    2425 |  220 | `	sEnv.iFlags = 0;` |
|    2425 |  221 | `	sEnv.nIdx = SXU32_HIGH;` |
|    2425 |  222 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|    2425 |  223 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|    2425 |  224 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|    2425 |  225 | `	return SXRET_OK;` |
|    1758 |  226 | `}` |
|       - |  227 | `/*` |
|       - |  228 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|       - |  229 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|       - |  230 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|       - |  231 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|       - |  232 | ` */` |
|    1166 |  233 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|       - |  234 | `	ph7_gen_state *pGen,` |
|       - |  235 | `	ph7_vm_func *pFunc,` |
|       - |  236 | `	const char *zIn,` |
|       - |  237 | `	const char *zEnd,` |
|       - |  238 | `	SyString *aShadow,` |
|       - |  239 | `	sxu32 nShadow)` |
|       5 |  240 | `{` |
|       - |  241 | `	sxi32 rc;` |
|    5811 |  242 | `	while( zIn < zEnd ){` |
|    4645 |  243 | `		if( zIn[0] == '\\' ){` |
|     265 |  244 | `			zIn++;` |
|     265 |  245 | `			if( zIn < zEnd ){` |
|     265 |  246 | `				zIn++;` |
|     130 |  247 | `			}` |
|     265 |  248 | `			continue;` |
|       - |  249 | `		}` |
|    4380 |  250 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|     129 |  251 | `			&& ((unsigned char)zIn[1] >= 0x80` |
|     124 |  252 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|       - |  253 | `			/* php's label bytes, the flat set (LEX_LABEL_START in lex.c). */` |
|       - |  254 | `			const char *zName;` |
|     128 |  255 | `			zIn++; /* skip '$' */` |
|     128 |  256 | `			zName = zIn;` |
|     483 |  257 | `			while( zIn < zEnd` |
|     722 |  258 | `				&& ((unsigned char)zIn[0] >= 0x80` |
|     682 |  259 | `					\|\| SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_') ){` |
|     598 |  260 | `				zIn++;` |
|       4 |  261 | `			}` |
|     128 |  262 | `			if( zIn > zName ){` |
|     190 |  263 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|     124 |  264 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|     128 |  265 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  266 | `					return SXERR_ABORT;` |
|       - |  267 | `				}` |
|      62 |  268 | `			}` |
|     128 |  269 | `			continue;` |
|       - |  270 | `		}` |
|    4261 |  271 | `		zIn++;` |
|       5 |  272 | `	}` |
|    1171 |  273 | `	return SXRET_OK;` |
|     588 |  274 | `}` |
|       - |  275 | `/*` |
|       - |  276 | ` * Scan the body token range of an arrow function for free-variable` |
|       - |  277 | ` * references and record them in pFunc's closure environment. Handles:` |
|       - |  278 | ` *   - plain $<id> pairs` |
|       - |  279 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|       - |  280 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|       - |  281 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|       - |  282 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|       - |  283 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|       - |  284 | ` *     are never mistakenly captured.` |
|       - |  285 | ` */` |
|    5940 |  286 | `static sxi32 GenStateArrowCaptureScan(` |
|       - |  287 | `	ph7_gen_state *pGen,` |
|       - |  288 | `	ph7_vm_func *pFunc,` |
|       - |  289 | `	SyToken *pStart,` |
|       - |  290 | `	SyToken *pEnd,` |
|       - |  291 | `	SyString *aShadow,` |
|       - |  292 | `	sxu32 nShadow)` |
|       5 |  293 | `{` |
|    5945 |  294 | `	SyToken *pScan = pStart;` |
|       - |  295 | `	sxi32 rc;` |
|   55827 |  296 | `	while( pScan < pEnd ){` |
|   49887 |  297 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|    1754 |  298 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|     583 |  299 | `				pScan->sData.zString,` |
|    1166 |  300 | `				pScan->sData.zString + pScan->sData.nByte,` |
|     583 |  301 | `				aShadow,nShadow);` |
|    1171 |  302 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  303 | `				return SXERR_ABORT;` |
|       - |  304 | `			}` |
|    1171 |  305 | `			pScan++;` |
|    1171 |  306 | `			continue;` |
|       - |  307 | `		}` |
|   48721 |  308 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|     357 |  309 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|     357 |  310 | `			SyToken *pFnKw = (nKw == PH7_TKWRD_STATIC) ? &pScan[1] : pScan;` |
|       - |  311 | ``			/* A NESTED arrow function, not a `$fn`/`C::fn` name that merely`` |
|       - |  312 | `			 * spells the keyword (see PH7_TokenOpensArrowFunc). */` |
|     357 |  313 | `			if( PH7_TokenOpensArrowFunc(pStart,pScan,pEnd) ){` |
|       - |  314 | `				SyToken *pInnerSigStart;` |
|       - |  315 | `				SyToken *pInnerSigEnd;` |
|       - |  316 | `				SyToken *pInnerBodyEnd;` |
|       - |  317 | `				SyString *aInnerShadow;` |
|       - |  318 | `				sxu32 nInnerShadow;` |
|       - |  319 | `				sxu32 nInnerParamMax;` |
|       - |  320 | `				SyToken *p;` |
|       - |  321 | `				int iNestInner;` |
|     125 |  322 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|     125 |  323 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  324 | `					pScan++;` |
|     ! 0 |  325 | `				}` |
|     125 |  326 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  327 | `					pScan++;` |
|     ! 0 |  328 | `					continue;` |
|       - |  329 | `				}` |
|     125 |  330 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|     125 |  331 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|       - |  332 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|     125 |  333 | `				if( pInnerSigEnd >= pEnd ){` |
|     ! 0 |  334 | `					pScan = pEnd;` |
|     ! 0 |  335 | `					continue;` |
|       - |  336 | `				}` |
|       - |  337 | `				/* Build an augmented shadow list: inherited + inner params */` |
|     125 |  338 | `				nInnerParamMax = 0;` |
|     389 |  339 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|     268 |  340 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|     120 |  341 | `						nInnerParamMax++;` |
|      58 |  342 | `					}` |
|     136 |  343 | `				}` |
|     125 |  344 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|     120 |  345 | `					&pGen->pVm->sAllocator,` |
|     120 |  346 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|     125 |  347 | `				if( aInnerShadow == 0 ){` |
|     ! 0 |  348 | `					return SXERR_ABORT;` |
|       - |  349 | `				}` |
|     125 |  350 | `				nInnerShadow = 0;` |
|     135 |  351 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|      12 |  352 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|       7 |  353 | `				}` |
|     389 |  354 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|     268 |  355 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|     152 |  356 | `						continue;` |
|       - |  357 | `					}` |
|     120 |  358 | `					if( &p[1] >= pInnerSigEnd ){` |
|     ! 0 |  359 | `						break;` |
|       - |  360 | `					}` |
|     120 |  361 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  362 | `						continue;` |
|       - |  363 | `					}` |
|     120 |  364 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|      62 |  365 | `				}` |
|     125 |  366 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|     125 |  367 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|     ! 0 |  368 | `					pScan++;` |
|     ! 0 |  369 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|     ! 0 |  370 | `						&& pScan->sData.nByte == 1` |
|     ! 0 |  371 | `						&& pScan->sData.zString[0] == '?' ){` |
|     ! 0 |  372 | `						pScan++;` |
|     ! 0 |  373 | `					}` |
|     ! 0 |  374 | `					if( pScan < pEnd` |
|     ! 0 |  375 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 |  376 | `						pScan++;` |
|     ! 0 |  377 | `					}` |
|     ! 0 |  378 | `				}` |
|     125 |  379 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|     125 |  380 | `					pScan++; /* past '=>' */` |
|      60 |  381 | `				}` |
|     125 |  382 | `				pInnerBodyEnd = pScan;` |
|     125 |  383 | `				iNestInner = 0;` |
|     961 |  384 | `				while( pInnerBodyEnd < pEnd ){` |
|     939 |  385 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|       - |  386 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|       - |  387 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|     102 |  388 | `						break;` |
|       - |  389 | `					}` |
|     841 |  390 | `					if( pInnerBodyEnd->nType &` |
|       - |  391 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     106 |  392 | `						iNestInner++;` |
|     790 |  393 | `					}else if( pInnerBodyEnd->nType &` |
|       - |  394 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     106 |  395 | `						iNestInner--;` |
|      51 |  396 | `					}` |
|     841 |  397 | `					pInnerBodyEnd++;` |
|       5 |  398 | `				}` |
|       - |  399 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|       - |  400 | `				 * the outer's body: a default value is evaluated at call time` |
|       - |  401 | `				 * in the outer frame, so any free variable it references is` |
|       - |  402 | `				 * an outer capture. We must NOT scan the parameter-name` |
|       - |  403 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|       - |  404 | `				 * or those names leak into the outer's closure environment.` |
|       - |  405 | `				 *` |
|       - |  406 | `				 * Walk the signature argument-by-argument, splitting on` |
|       - |  407 | `				 * top-level commas, and for each argument scan only the token` |
|       - |  408 | `				 * range after the '=' sign. */` |
|       - |  409 | `				{` |
|     125 |  410 | `					SyToken *pArgStart = pInnerSigStart;` |
|     241 |  411 | `					while( pArgStart < pInnerSigEnd ){` |
|     120 |  412 | `						SyToken *pArgEnd = pArgStart;` |
|     120 |  413 | `						SyToken *pEq = 0;` |
|     120 |  414 | `						int iNestArg = 0;` |
|     366 |  415 | `						while( pArgEnd < pInnerSigEnd ){` |
|     264 |  416 | `							if( iNestArg == 0` |
|     268 |  417 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|      20 |  418 | `								break;` |
|       - |  419 | `							}` |
|     250 |  420 | `							if( pArgEnd->nType &` |
|       - |  421 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  422 | `								iNestArg++;` |
|     250 |  423 | `							}else if( pArgEnd->nType &` |
|       - |  424 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  425 | `								iNestArg--;` |
|     ! 0 |  426 | `							}` |
|     246 |  427 | `							if( pEq == 0 && iNestArg == 0` |
|     244 |  428 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|       7 |  429 | `								pEq = pArgEnd;` |
|       3 |  430 | `							}` |
|     250 |  431 | `							pArgEnd++;` |
|       4 |  432 | `						}` |
|     120 |  433 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|      10 |  434 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       3 |  435 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|       7 |  436 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  437 | `								return SXERR_ABORT;` |
|       - |  438 | `							}` |
|       3 |  439 | `						}` |
|     120 |  440 | `						pArgStart = pArgEnd;` |
|     116 |  441 | `						if( pArgStart < pInnerSigEnd` |
|      71 |  442 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|      20 |  443 | `							pArgStart++;` |
|       9 |  444 | `						}` |
|       4 |  445 | `					}` |
|       - |  446 | `				}` |
|     185 |  447 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|      60 |  448 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|     125 |  449 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  450 | `					return SXERR_ABORT;` |
|       - |  451 | `				}` |
|     125 |  452 | `				pScan = pInnerBodyEnd;` |
|     125 |  453 | `				continue;` |
|       - |  454 | `			}` |
|     116 |  455 | `		}` |
|   48601 |  456 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|   45219 |  457 | `			pScan++;` |
|   45219 |  458 | `			continue;` |
|       - |  459 | `		}` |
|       - |  460 | `		{` |
|       - |  461 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|    3387 |  462 | `			SyToken *pDollar = pScan;` |
|    5073 |  463 | `			while( &pDollar[1] < pEnd` |
|    3387 |  464 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|     ! 0 |  465 | `				pDollar++;` |
|     ! 0 |  466 | `			}` |
|    3387 |  467 | `			if( &pDollar[1] >= pEnd ){` |
|     ! 0 |  468 | `				break;` |
|       - |  469 | `			}` |
|    3387 |  470 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  471 | `				pScan = pDollar + 1;` |
|     ! 0 |  472 | `				continue;` |
|       - |  473 | `			}` |
|    5078 |  474 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|    3382 |  475 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|    1691 |  476 | `				aShadow,nShadow);` |
|    3387 |  477 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  478 | `				return SXERR_ABORT;` |
|       - |  479 | `			}` |
|    3387 |  480 | `			pScan = pDollar + 2;` |
|       - |  481 | `		}` |
|       5 |  482 | `	}` |
|    5945 |  483 | `	return SXRET_OK;` |
|    2975 |  484 | `}` |
|       - |  485 | `/*` |
|       - |  486 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|       - |  487 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|       - |  488 | ` * variables by value. The body is a single expression that acts as an` |
|       - |  489 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|       - |  490 | ` * $this is also made available.` |
|       - |  491 | ` */` |
|    5820 |  492 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  493 | `{` |
|       - |  494 | `	ph7_vm_func *pFunc;` |
|       - |  495 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  496 | `	GenBlock *pBlock;` |
|       - |  497 | `	SySet *pInstrContainer;` |
|       - |  498 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|       - |  499 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|       - |  500 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|       - |  501 | `	SyToken *pSavedEnd;` |
|       - |  502 | `	ph7_vm_func_arg *aArgs;` |
|       - |  503 | `	char zName[512];` |
|       - |  504 | `	static int iCnt = 1;` |
|       - |  505 | `	char *zDup;` |
|       - |  506 | `	SyToken *pTokKw;` |
|       - |  507 | `	sxu32 nLen;` |
|       - |  508 | `	sxu32 nLine;` |
|    5825 |  509 | `	sxi32 iFlags = 0;` |
|    5825 |  510 | `	int bStatic = 0;` |
|       - |  511 | `	sxi32 rc;` |
|       - |  512 | `	sxu32 n;` |
|    2910 |  513 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  514 |  |
|    5825 |  515 | `	nLine = pGen->pIn->nLine;` |
|       - |  516 | ``	/* Attribute-sidecar key: `#[A] [static] fn` trivia is keyed to this token */`` |
|    5825 |  517 | `	pTokKw = pGen->pIn;` |
|       - |  518 | `	/* Optional 'static' prefix */` |
|    5820 |  519 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|    5825 |  520 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|      61 |  521 | `		bStatic = 1;` |
|      61 |  522 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|      61 |  523 | `		pGen->pIn++;` |
|      30 |  524 | `	}` |
|       - |  525 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|    5820 |  526 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    5825 |  527 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|     ! 0 |  528 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  529 | `			"Arrow function: expected 'fn' keyword");` |
|     ! 0 |  530 | `		return SXERR_SYNTAX;` |
|       - |  531 | `	}` |
|    5825 |  532 | `	pGen->pIn++; /* Jump 'fn' */` |
|       - |  533 | `	/* Optional '&' — return by reference */` |
|    5825 |  534 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  535 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|     ! 0 |  536 | `		pGen->pIn++;` |
|     ! 0 |  537 | `	}` |
|       - |  538 | `	/* Expect '(' */` |
|    5825 |  539 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  540 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  541 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  542 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|       2 |  543 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  544 | `		}else{` |
|     ! 0 |  545 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  546 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|       - |  547 | `		}` |
|       3 |  548 | `		return SXERR_SYNTAX;` |
|       - |  549 | `	}` |
|    5823 |  550 | `	pGen->pIn++; /* Jump '(' */` |
|       - |  551 | `	/* Delimit the parameter list */` |
|    5823 |  552 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|    5823 |  553 | `	if( pSigEnd >= pGen->pEnd ){` |
|       3 |  554 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  555 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       3 |  556 | `		return SXERR_SYNTAX;` |
|       - |  557 | `	}` |
|       - |  558 | `	/* Allocate the function state */` |
|    5821 |  559 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|    5821 |  560 | `	if( pFunc == 0 ){` |
|     ! 0 |  561 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  562 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  563 | `		return SXERR_ABORT;` |
|       - |  564 | `	}` |
|       - |  565 | `	/* Generate a unique lambda name */` |
|    5821 |  566 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|    5871 |  567 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|      52 |  568 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       2 |  569 | `	}` |
|    5821 |  570 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|    5821 |  571 | `	if( zDup == 0 ){` |
|     ! 0 |  572 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  573 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  574 | `		return SXERR_ABORT;` |
|       - |  575 | `	}` |
|    5821 |  576 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|       - |  577 | `	/* Reflection getStartLine(): line of the ['static'] 'fn' keyword */` |
|    5821 |  578 | `	pFunc->nLine = nLine;` |
|       - |  579 | `	/* php's visible name — an arrow function is named exactly like a closure. This` |
|       - |  580 | `	 * compiler builds its own function state, so it consumes the pending name itself. */` |
|    5821 |  581 | `	GenStateClosureName(&(*pGen),nLine);` |
|    5821 |  582 | `	pFunc->sClosureName = pGen->sPendingClosureName;` |
|    5821 |  583 | `	SyStringInitFromBuf(&pGen->sPendingClosureName,0,0);` |
|       - |  584 | ``	/* Expression-position attributes (`$f = #[A] fn () => …`) */`` |
|    5821 |  585 | `	if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pFunc->aAttrs) == SXERR_ABORT ){` |
|     ! 0 |  586 | `		return SXERR_ABORT;` |
|       - |  587 | `	}` |
|    5821 |  588 | `	if( GenStateCheckAttrPlacement(&(*pGen),&pFunc->aAttrs,2,2,0,0) == SXERR_ABORT ){` |
|     ! 0 |  589 | `		return SXERR_ABORT;` |
|       - |  590 | `	}` |
|       - |  591 | `	/* Collect function arguments */` |
|    5821 |  592 | `	if( pGen->pIn < pSigEnd ){` |
|     565 |  593 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|     565 |  594 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  595 | `			return SXERR_ABORT;` |
|       - |  596 | `		}` |
|     280 |  597 | `	}` |
|       - |  598 | `	/* Point past ')' and parse optional return type */` |
|    5821 |  599 | `	pGen->pIn = &pSigEnd[1];` |
|    5821 |  600 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|    5821 |  601 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  602 | `		return SXERR_ABORT;` |
|    5821 |  603 | `	}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  604 | `		return SXERR_SYNTAX;` |
|       - |  605 | `	}` |
|    5821 |  606 | `	if( GenStateApplyNoDiscard(&(*pGen),pFunc,0,0) == SXERR_ABORT ){` |
|     ! 0 |  607 | `		return SXERR_ABORT;` |
|       - |  608 | `	}` |
|       - |  609 | `	/* Expect '=>' */` |
|    5821 |  610 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  611 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  612 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  613 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|       2 |  614 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  615 | `		}else{` |
|     ! 0 |  616 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  617 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|       - |  618 | `		}` |
|       3 |  619 | `		return SXERR_SYNTAX;` |
|       - |  620 | `	}` |
|    5819 |  621 | `	pGen->pIn++; /* Jump '=>' */` |
|    5819 |  622 | `	pBodyStart = pGen->pIn;` |
|    5819 |  623 | `	pBodyEnd = pGen->pEnd;` |
|       - |  624 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|       - |  625 | `	 * recursively collect free-variable references from the body. The scan` |
|       - |  626 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|       - |  627 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|    5819 |  628 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       - |  629 | `	{` |
|    5819 |  630 | `		SyString *aShadow = 0;` |
|    5819 |  631 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|    5819 |  632 | `		if( nShadow > 0 ){` |
|     563 |  633 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|     558 |  634 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|     563 |  635 | `			if( aShadow == 0 ){` |
|     ! 0 |  636 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  637 | `					"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  638 | `				return SXERR_ABORT;` |
|       - |  639 | `			}` |
|    1281 |  640 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|     723 |  641 | `				aShadow[n] = aArgs[n].sName;` |
|     364 |  642 | `			}` |
|     279 |  643 | `		}` |
|    8726 |  644 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|    2907 |  645 | `			aShadow,nShadow);` |
|    5819 |  646 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  647 | `			return SXERR_ABORT;` |
|       - |  648 | `		}` |
|       - |  649 | `	}` |
|       - |  650 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|       - |  651 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|       - |  652 | `	 * captured value is silently dropped when the enclosing scope has no` |
|       - |  653 | `	 * $this. */` |
|    5819 |  654 | `	if( !bStatic ){` |
|       - |  655 | `		char *zThisDup;` |
|    5759 |  656 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|    5759 |  657 | `		if( zThisDup == 0 ){` |
|     ! 0 |  658 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  659 | `				"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  660 | `			return SXERR_ABORT;` |
|       - |  661 | `		}` |
|    5759 |  662 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|    5759 |  663 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|    5759 |  664 | `		sEnv.nIdx = SXU32_HIGH;` |
|    5759 |  665 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|    5759 |  666 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|    5759 |  667 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|    2877 |  668 | `	}` |
|       - |  669 | `	/* Arrow functions are always closures; the ARROW mark tells OP_LOAD_CLOSURE` |
|       - |  670 | `	 * these captures are implicit (auto-scanned) so an undefined one stays silent` |
|       - |  671 | `	 * at creation — php only warns when the body reads it. */` |
|    5819 |  672 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE \| VM_FUNC_ARROW;` |
|       - |  673 | `	/* Compile the body expression as an implicit return */` |
|    8726 |  674 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|    2907 |  675 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|    5819 |  676 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  677 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  678 | `			"PH7 engine is running out-of-memory");` |
|     ! 0 |  679 | `		return SXERR_ABORT;` |
|       - |  680 | `	}` |
|    5819 |  681 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    5819 |  682 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|    5819 |  683 | `	pSavedEnd = pGen->pEnd;` |
|    5819 |  684 | `	pGen->pIn = pBodyStart;` |
|    5819 |  685 | `	pGen->pEnd = pBodyEnd;` |
|       - |  686 | ``	/* The body is an implicit `return <expr>`, which READS its operands. Compile`` |
|       - |  687 | `	 * it read-only (like echo / string interpolation) so a lone undefined variable` |
|       - |  688 | ``	 * — e.g. `fn()=>$z` for an auto-capture that was undefined at creation and so`` |
|       - |  689 | `	 * never captured (see VmExecOpLoadClosure) — raises php's "Undefined variable"` |
|       - |  690 | `	 * warning at the read instead of being loaded quietly as a plain expression` |
|       - |  691 | ``	 * statement (`$z;`, silent in both engines) would be. */`` |
|    5819 |  692 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|    5819 |  693 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  694 | `		return SXERR_ABORT;` |
|       - |  695 | `	}` |
|       - |  696 | `	/* The cursor stopped just past the body expression */` |
|    5819 |  697 | `	pFunc->nEndLine = (pGen->pIn > pBodyStart) ? pGen->pIn[-1].nLine : nLine;` |
|       - |  698 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|       - |  699 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|       - |  700 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|       - |  701 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|    5819 |  702 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|    5819 |  703 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|    5819 |  704 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|    5819 |  705 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    5819 |  706 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  707 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|    5819 |  708 | `	pGen->pIn = pBodyEnd;` |
|    5819 |  709 | `	pGen->pEnd = pSavedEnd;` |
|       - |  710 | `	/* Emit the load-closure instruction */` |
|    5819 |  711 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|    5819 |  712 | `	return SXRET_OK;` |
|    2915 |  713 | `}` |
|       - |  714 | `/*` |
|       - |  715 | ` * Compile a single arm's expression range into a freshly-allocated` |
|       - |  716 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|       - |  717 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|       - |  718 | ` * expression's value.` |
|       - |  719 | ` */` |
|     572 |  720 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|       - |  721 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|       3 |  722 | `{` |
|       - |  723 | `	SySet *pInstrContainer;` |
|       - |  724 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  725 | `	GenBlock *pArmBlock;` |
|       - |  726 | `	sxi32 rc;` |
|     575 |  727 | `	pTmpIn  = pGen->pIn;` |
|     575 |  728 | `	pTmpEnd = pGen->pEnd;` |
|     575 |  729 | `	pGen->pIn  = pStart;` |
|     575 |  730 | `	pGen->pEnd = pStop;` |
|     575 |  731 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     575 |  732 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|       - |  733 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|       - |  734 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|       - |  735 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|       - |  736 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|       - |  737 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|     861 |  738 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|     286 |  739 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|     575 |  740 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  741 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  742 | `		pGen->pIn  = pTmpIn;` |
|     ! 0 |  743 | `		pGen->pEnd = pTmpEnd;` |
|     ! 0 |  744 | `		return SXERR_ABORT;` |
|       - |  745 | `	}` |
|     575 |  746 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     575 |  747 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     575 |  748 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     575 |  749 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     575 |  750 | `	GenStateLeaveBlock(&(*pGen),0);` |
|     575 |  751 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     575 |  752 | `	pGen->pIn  = pTmpIn;` |
|     575 |  753 | `	pGen->pEnd = pTmpEnd;` |
|     575 |  754 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  755 | `		return SXERR_ABORT;` |
|       - |  756 | `	}` |
|     575 |  757 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  758 | `		return SXERR_EMPTY;` |
|       - |  759 | `	}` |
|     575 |  760 | `	return SXRET_OK;` |
|     289 |  761 | `}` |
|       - |  762 | `/*` |
|       - |  763 | ` * Compile a PHP 8.0 match expression:` |
|       - |  764 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|       - |  765 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|       - |  766 | ` * Strict comparison (===) is used between the subject and each condition.` |
|       - |  767 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|       - |  768 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|       - |  769 | ` */` |
|       - |  770 | `/*` |
|       - |  771 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|       - |  772 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|       - |  773 | ` * caller can bail out of the current expression.` |
|       - |  774 | ` */` |
|       2 |  775 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|       1 |  776 | `{` |
|       - |  777 | `	va_list ap;` |
|       - |  778 | `	sxi32 rc;` |
|       - |  779 | `	SyBlob sMsg;` |
|       3 |  780 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|       3 |  781 | `	va_start(ap,zFmt);` |
|       3 |  782 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|       3 |  783 | `	va_end(ap);` |
|       3 |  784 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|       3 |  785 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|       3 |  786 | `	SyBlobRelease(&sMsg);` |
|       3 |  787 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  788 | `		return SXERR_ABORT;` |
|       - |  789 | `	}` |
|       3 |  790 | `	return SXERR_SYNTAX;` |
|       2 |  791 | `}` |
|       - |  792 | `/*` |
|       - |  793 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|       - |  794 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|       - |  795 | ` * Returns the stop token pointer (or pEnd if none found).` |
|       - |  796 | ` */` |
|     574 |  797 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|       4 |  798 | `{` |
|     578 |  799 | `	SyToken *pCur = pStart;` |
|     578 |  800 | `	int iNest = 0;` |
|    1726 |  801 | `	while( pCur < pEnd ){` |
|    1650 |  802 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|      94 |  803 | `			iNest++;` |
|    1604 |  804 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|      94 |  805 | `			iNest--;` |
|    1512 |  806 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|     501 |  807 | `			return pCur;` |
|       - |  808 | `		}` |
|    1152 |  809 | `		pCur++;` |
|       4 |  810 | `	}` |
|      80 |  811 | `	return pEnd;` |
|     291 |  812 | `}` |
|     124 |  813 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       4 |  814 | `{` |
|       - |  815 | `	ph7_match *pMatch;` |
|       - |  816 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|     128 |  817 | `	int bHasDefault = 0;` |
|       - |  818 | `	sxu32 nLine;` |
|       - |  819 | `	sxi32 rc;` |
|      62 |  820 | `	SXUNUSED(iCompileFlag);` |
|     128 |  821 | `	nLine = pGen->pIn->nLine;` |
|     128 |  822 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|       - |  823 | `	/* Expect '(' */` |
|     128 |  824 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  825 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  826 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|     ! 0 |  827 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|       - |  828 | `	}` |
|     128 |  829 | `	pGen->pIn++; /* Jump '(' */` |
|     128 |  830 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|     128 |  831 | `	if( pSubjEnd >= pGen->pEnd ){` |
|     ! 0 |  832 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  833 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       - |  834 | `	}` |
|     128 |  835 | `	if( pGen->pIn >= pSubjEnd ){` |
|     ! 0 |  836 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  837 | `			"syntax error, unexpected \")\", expecting match subject");` |
|       - |  838 | `	}` |
|       - |  839 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|     128 |  840 | `	pSavedEnd = pGen->pEnd;` |
|     128 |  841 | `	pGen->pEnd = pSubjEnd;` |
|     128 |  842 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     128 |  843 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  844 | `		return SXERR_ABORT;` |
|       - |  845 | `	}` |
|     128 |  846 | `	pGen->pEnd = pSavedEnd;` |
|     128 |  847 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|       - |  848 | `	/* Expect '{' */` |
|     128 |  849 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  850 | `		return GenStateMatchError(pGen,` |
|     ! 0 |  851 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  852 | `			"syntax error, expecting \"{\" after match subject");` |
|       - |  853 | `	}` |
|     128 |  854 | `	pGen->pIn++; /* Jump '{' */` |
|     128 |  855 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|     128 |  856 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  857 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  858 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|       - |  859 | `	}` |
|       - |  860 | `	/* Allocate ph7_match container */` |
|     128 |  861 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|     128 |  862 | `	if( pMatch == 0 ){` |
|     ! 0 |  863 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  864 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  865 | `		return SXERR_ABORT;` |
|       - |  866 | `	}` |
|     128 |  867 | `	SyZero(pMatch,sizeof(ph7_match));` |
|     128 |  868 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|       - |  869 | `	/* Iterate arms */` |
|     430 |  870 | `	while( pGen->pIn < pBodyEnd ){` |
|       - |  871 | `		ph7_match_arm sArm;` |
|       - |  872 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|     310 |  873 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|     310 |  874 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|     310 |  875 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|     310 |  876 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  877 | `		/* 'default' arm? */` |
|     306 |  878 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     178 |  879 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|      45 |  880 | `			if( bHasDefault ){` |
|       3 |  881 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|       - |  882 | `					"Match expressions may only contain one default arm");` |
|       4 |  883 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  884 | `			}` |
|      43 |  885 | `			sArm.bDefault = 1;` |
|      43 |  886 | `			bHasDefault = 1;` |
|      43 |  887 | `			pGen->pIn++;` |
|      43 |  888 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|     ! 0 |  889 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  890 | `					"syntax error, expecting \"=>\" after 'default'");` |
|       - |  891 | `			}` |
|      43 |  892 | `			pGen->pIn++; /* Jump '=>' */` |
|      23 |  893 | `		}else{` |
|       - |  894 | `			/* Condition list: cond (',' cond)* '=>' */` |
|     268 |  895 | `			pCondStart = pGen->pIn;` |
|     268 |  896 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|       - |  897 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|     276 |  898 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|       - |  899 | `				SySet sCondBc;` |
|       9 |  900 | `				if( pCondStart >= pArrow ){` |
|     ! 0 |  901 | `					return GenStateMatchError(pGen,nArmLine,` |
|       - |  902 | `						"syntax error, empty match condition expression");` |
|       - |  903 | `				}` |
|       9 |  904 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       9 |  905 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|       9 |  906 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  907 | `					return SXERR_ABORT;` |
|       - |  908 | `				}` |
|       9 |  909 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       9 |  910 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|       9 |  911 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|       - |  912 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|       1 |  913 | `			}` |
|     268 |  914 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  915 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  916 | `					"syntax error, expecting \"=>\" in match arm");` |
|       - |  917 | `			}` |
|     265 |  918 | `			if( pCondStart >= pArrow ){` |
|     ! 0 |  919 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  920 | `					"syntax error, empty match condition expression");` |
|       - |  921 | `			}` |
|       - |  922 | `			{` |
|       - |  923 | `				SySet sCondBc;` |
|     265 |  924 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     265 |  925 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|     265 |  926 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  927 | `					return SXERR_ABORT;` |
|       - |  928 | `				}` |
|     265 |  929 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       - |  930 | `			}` |
|     265 |  931 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|       - |  932 | `		}` |
|       - |  933 | `		/* Compile result expression: up to top-level ',' or body end */` |
|     305 |  934 | `		pResStart = pGen->pIn;` |
|     305 |  935 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|     305 |  936 | `		if( pResStart >= pResEnd ){` |
|     ! 0 |  937 | `			return GenStateMatchError(pGen,nArmLine,` |
|       - |  938 | `				"syntax error, expected expression after \"=>\"");` |
|       - |  939 | `		}` |
|     305 |  940 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|     305 |  941 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  942 | `			return SXERR_ABORT;` |
|       - |  943 | `		}` |
|     305 |  944 | `		pGen->pIn = pResEnd;` |
|     305 |  945 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|     231 |  946 | `			pGen->pIn++; /* Skip trailing ',' */` |
|     114 |  947 | `		}` |
|     305 |  948 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|       3 |  949 | `	}` |
|     123 |  950 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|     123 |  951 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|     123 |  952 | `	return SXRET_OK;` |
|      66 |  953 | `}` |
|       - |  954 | `/*` |
|       - |  955 | ` * Compile a backtick quoted string.` |
|       - |  956 | ` */` |
|       2 |  957 | `PH7_PRIVATE sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 |  958 | `{` |
|       1 |  959 | `	SXUNUSED(iCompileFlag);` |
|       - |  960 | `	/*` |
|       - |  961 | ``	 * The backtick (`) operator was DEPRECATED by php (shell_exec() is the replacement).`` |
|       - |  962 | `	 * PHL targets php's *non-deprecated* surface, so it is a hard parse error — never` |
|       - |  963 | `	 * compiled to a shell_exec() call.` |
|       - |  964 | `	 */` |
|       3 |  965 | `	PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  966 | ``		"syntax error, the backtick (`) operator was removed, use shell_exec() instead");`` |
|       3 |  967 | `	return SXERR_ABORT;` |
|       1 |  968 | `}` |
|       - |  969 | `/*` |
|       - |  970 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - |  971 | ` * construct.` |
|       - |  972 | ` */` |
|     190 |  973 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  974 | `{` |
|       - |  975 | `	SyString *pName;` |
|       - |  976 | `	sxu32 nKeyID;` |
|       - |  977 | `	sxi32 rc;` |
|       - |  978 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|     195 |  979 | `	pName = &pGen->pIn->sData;` |
|     195 |  980 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     195 |  981 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|     195 |  982 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       6 |  983 | `		SyToken *pTmp,*pNext = 0;` |
|       - |  984 | ``		/* A STATEMENT `echo` never reaches here — it dispatches through the statement`` |
|       - |  985 | `		 * table. Arriving in expression position means source like` |
|       - |  986 | ``		 * `fopen('f','r') or echo "IO error";`, which was a Symisc extension and is a`` |
|       - |  987 | `		 * php parse error (§10: a PH7-ism that changes the meaning of valid source is a` |
|       - |  988 | ``		 * bug). The one legitimate expression-echo is the token a `<?= ... ?>` short tag`` |
|       - |  989 | `		 * synthesizes, which raises nExprEchoOk around its own compile. */` |
|       6 |  990 | `		if( pGen->nExprEchoOk < 1 ){` |
|       3 |  991 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn - 1,0);` |
|       3 |  992 | `			return SXERR_ABORT;` |
|       - |  993 | `		}` |
|       - |  994 | `		/* Compile arguments one after one */` |
|       3 |  995 | `		pTmp = pGen->pEnd;` |
|       3 |  996 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|       5 |  997 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       3 |  998 | `			if( pGen->pIn < pNext ){` |
|       3 |  999 | `				pGen->pEnd = pNext;` |
|       3 | 1000 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|       3 | 1001 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1002 | `					return SXERR_ABORT;` |
|       - | 1003 | `				}` |
|       3 | 1004 | `				if( rc != SXERR_EMPTY ){` |
|       - | 1005 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|       - | 1006 | `					 * without the overhead of a function call.` |
|       - | 1007 | `					 * This is a very powerful optimization that improve` |
|       - | 1008 | `					 * performance greatly.` |
|       - | 1009 | `					 */` |
|       3 | 1010 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|       1 | 1011 | `				}` |
|       1 | 1012 | `			}` |
|       - | 1013 | `			/* Jump trailing commas */` |
|       3 | 1014 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     ! 0 | 1015 | `				pNext++;` |
|     ! 0 | 1016 | `			}` |
|       3 | 1017 | `			pGen->pIn = pNext;` |
|       1 | 1018 | `		}` |
|       - | 1019 | `		/* Restore token stream */` |
|       3 | 1020 | `		pGen->pEnd = pTmp;` |
|       2 | 1021 | `	}else{` |
|     191 | 1022 | `		sxi32 nArg = 0;` |
|     191 | 1023 | `		sxu32 nIdx = 0;` |
|       - | 1024 | `		char zCanon[sizeof("include_once")-1];` |
|       - | 1025 | `		SyString sCanon;` |
|     191 | 1026 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|     191 | 1027 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1028 | `			return SXERR_ABORT;` |
|     191 | 1029 | `		}else if(rc != SXERR_EMPTY ){` |
|     191 | 1030 | `			nArg = 1;` |
|      93 | 1031 | `		}` |
|       - | 1032 | `		/* The construct is dispatched as a CALL to the host function of the same name,` |
|       - | 1033 | `		 * so the name emitted here must be the construct's canonical spelling, not the` |
|       - | 1034 | ``		 * source's: php accepts `PRINT`/`Isset`/`EVAL` (keywords are case-insensitive)`` |
|       - | 1035 | ``		 * where the raw text produced `Call to undefined function PRINT()`. Every`` |
|       - | 1036 | `		 * construct name is lower-case ASCII, so folding IS canonicalising. */` |
|     191 | 1037 | `		if( pName->nByte <= sizeof(zCanon) ){` |
|       - | 1038 | `			sxu32 i;` |
|    1661 | 1039 | `			for( i = 0 ; i < pName->nByte ; ++i ){` |
|    1475 | 1040 | `				unsigned char c = (unsigned char)pName->zString[i];` |
|    1475 | 1041 | `				zCanon[i] = (char)((c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c);` |
|     740 | 1042 | `			}` |
|     191 | 1043 | `			SyStringInitFromBuf(&sCanon,zCanon,pName->nByte);` |
|     191 | 1044 | `			pName = &sCanon;` |
|      93 | 1045 | `		}` |
|     191 | 1046 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - | 1047 | `			ph7_value *pObj;` |
|       - | 1048 | `			/* Emit the call instruction */` |
|     111 | 1049 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     111 | 1050 | `			if( pObj == 0 ){` |
|     ! 0 | 1051 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1052 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1053 | `				return SXERR_ABORT;` |
|       - | 1054 | `			}` |
|     111 | 1055 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - | 1056 | `			/* Install in the literal table */` |
|     111 | 1057 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      53 | 1058 | `		}` |
|       - | 1059 | `		/* Emit the call instruction */` |
|     191 | 1060 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     191 | 1061 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       - | 1062 | `	}` |
|       - | 1063 | `	/* Node successfully compiled */` |
|     193 | 1064 | `	return SXRET_OK;` |
|     100 | 1065 | `}` |
|       - | 1066 | `/*` |
|       - | 1067 | ` * Compile a node holding a variable declaration.` |
|       - | 1068 | ` * According to the PHP language reference` |
|       - | 1069 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - | 1070 | ` *  The variable name is case-sensitive.` |
|       - | 1071 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - | 1072 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - | 1073 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - | 1074 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - | 1075 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - | 1076 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - | 1077 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - | 1078 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - | 1079 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - | 1080 | ` *  the chapter on Expressions.` |
|       - | 1081 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - | 1082 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - | 1083 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - | 1084 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - | 1085 | ` *  is being assigned (the source variable).` |
|       - | 1086 | ` */` |
| 3109102 | 1087 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 | 1088 | `{` |
| 3109107 | 1089 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 1090 | `	sxi32 iVv;` |
|       - | 1091 | `	sxi32 iP1;` |
| 3109107 | 1092 | `	sxi32 iP2 = 0; /* 1 = quiet read (isset/empty): a missing variable must not warn */` |
|       - | 1093 | `	void *p3;` |
|       - | 1094 | `	sxi32 rc;` |
| 3109107 | 1095 | `	iVv = -1; /* Variable variable counter */` |
| 6218247 | 1096 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
| 3109145 | 1097 | `		pGen->pIn++;` |
| 3109145 | 1098 | `		iVv++;` |
|       5 | 1099 | `	}` |
| 3109107 | 1100 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - | 1101 | `		/* Invalid variable name */` |
|     ! 0 | 1102 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable or \"{\" or \"$\"");` |
|     ! 0 | 1103 | `		if( rc == SXERR_ABORT ){` |
|       - | 1104 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1105 | `			return SXERR_ABORT;` |
|       - | 1106 | `		}` |
|     ! 0 | 1107 | `		return SXRET_OK;` |
|       - | 1108 | `	}` |
| 3109107 | 1109 | `	p3  = 0;` |
| 3109107 | 1110 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - | 1111 | `		/* Dynamic variable creation */` |
|      31 | 1112 | `		pGen->pIn++;  /* Jump the open curly */` |
|      31 | 1113 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      31 | 1114 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 1115 | `			/* Empty expression */` |
|       - | 1116 | `			{` |
|       - | 1117 | `			/* php names the offending token and, for an empty "${}", stops there:` |
|       - | 1118 | `			 * the "expecting" tail only appears when something could still follow. */` |
|       - | 1119 | ``			/* `${}`: pEnd was stepped back past the trailing '}', so the token php`` |
|       - | 1120 | `			 * names sits AT pEnd. Reach for it before deciding the tail -- php stops` |
|       - | 1121 | `			 * at "unexpected token \"}\"" with no "expecting" clause, which the` |
|       - | 1122 | `			 * NULL-token path could not express because it never saw the '}'. */` |
|       3 | 1123 | `			SyToken *pBad = pGen->pIn < pGen->pEnd ? pGen->pIn : 0;` |
|       3 | 1124 | `			if( pBad == 0 && pGen->pTokenSet ){` |
|       3 | 1125 | `				SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|       3 | 1126 | `				SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|       3 | 1127 | `				if( pGen->pEnd >= pBase && pGen->pEnd < pStreamEnd ){` |
|       3 | 1128 | `					pBad = pGen->pEnd;` |
|       1 | 1129 | `				}` |
|       1 | 1130 | `			}` |
|       5 | 1131 | `			PH7_GenSyntaxError(&(*pGen),pBad,` |
|       2 | 1132 | `				(pBad && (pBad->nType & PH7_TK_CCB)) ? 0 : "variable or \"{\" or \"$\"");` |
|       - | 1133 | `			}` |
|       3 | 1134 | `			return SXRET_OK;` |
|       - | 1135 | `		}` |
|       - | 1136 | `		/* Compile the expression holding the variable name. It is a pure READ, so` |
|       - | 1137 | ``		 * compile it read-only: `${$u}` warns on an undefined $u (php) instead of`` |
|       - | 1138 | ``		 * silently creating it, matching the `$$u` name-read path below. A quiet`` |
|       - | 1139 | `		 * outer (isset()/empty()) suppresses that name warning too, so carry the` |
|       - | 1140 | `		 * quiet flag into the name expression. */` |
|      29 | 1141 | `		sxi32 iNameFlags = EXPR_FLAG_RDONLY_LOAD;` |
|      29 | 1142 | `		if( iCompileFlag & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY) ){` |
|       - | 1143 | ``			/* isset()/empty() suppress the name warning; `??` (EXPR_FLAG_QUIET_VAR)`` |
|       - | 1144 | ``			 * does NOT — php warns `Undefined variable $u` for `${$u} ?? x` and only`` |
|       - | 1145 | `			 * quiets the TARGET read, so QUIET_VAR is deliberately excluded here. */` |
|       3 | 1146 | `			iNameFlags \|= EXPR_FLAG_QUIET_VAR;` |
|       1 | 1147 | `		}` |
|      29 | 1148 | `		rc = PH7_CompileExpr(&(*pGen),iNameFlags,0);` |
|      29 | 1149 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1150 | `			return SXERR_ABORT;` |
|      29 | 1151 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 | 1152 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,0);` |
|       3 | 1153 | `			return SXRET_OK;` |
|       - | 1154 | `		}` |
|      15 | 1155 | `	}else{` |
|       - | 1156 | `		SyHashEntry *pEntry;` |
|       - | 1157 | `		SyString *pName;` |
| 3109079 | 1158 | `		char *zName = 0;` |
|       - | 1159 | `		/* Extract variable name */` |
| 3109079 | 1160 | `		pName = &pGen->pIn->sData;` |
|       - | 1161 | `		/* Advance the stream cursor */` |
| 3109079 | 1162 | `		pGen->pIn++;` |
| 3109079 | 1163 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
| 3109079 | 1164 | `		if( pEntry == 0 ){` |
|       - | 1165 | `			/* Duplicate name */` |
|  407125 | 1166 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  407125 | 1167 | `			if( zName == 0 ){` |
|     ! 0 | 1168 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1169 | `				return SXERR_ABORT;` |
|       - | 1170 | `			}` |
|       - | 1171 | `			/* Install in the hashtable */` |
|  407125 | 1172 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|  203565 | 1173 | `		}else{` |
|       - | 1174 | `			/* Name already available */` |
| 2701959 | 1175 | `			zName = (char *)pEntry->pUserData;` |
|       - | 1176 | `		}` |
| 3109079 | 1177 | `		p3 = (void *)zName;` |
|       - | 1178 | `	}` |
| 3109103 | 1179 | `	iP1 = 0;` |
| 3109103 | 1180 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
| 2901221 | 1181 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - | 1182 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
| 2018113 | 1183 | `			iP1 = 1;` |
| 1009054 | 1184 | `		}` |
| 1450608 | 1185 | `	}` |
|       - | 1186 | ``	/* iP2 marks a QUIET read: `isset($x)` / `empty($x)` inspect a variable`` |
|       - | 1187 | `	 * without reading it, so an undefined one must not warn (php stays silent` |
|       - | 1188 | `	 * for both). Every other read of a missing variable warns — see OP_LOAD.` |
|       - | 1189 | `	 * The two flags are cleared before recursing into a subscript's index` |
|       - | 1190 | ``	 * expression, so `isset($a[$i])` still warns for an undefined $i, as php`` |
|       - | 1191 | ``	 * does. Contexts that VIVIFY (assignment targets, `??`, appends) already`` |
|       - | 1192 | `	 * emit iP1 = 0 and never reach the warning. */` |
| 3109103 | 1193 | `	if( iCompileFlag & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_QUIET_VAR) ){` |
|   12955 | 1194 | `		iP2 = 1;` |
| 3102628 | 1195 | `	}else if( iCompileFlag & EXPR_FLAG_RMW_LOAD ){` |
|       - | 1196 | ``		/* Warn-then-create: the read half of `$x++` / `$x .= ...` still needs a`` |
|       - | 1197 | `		 * writable slot, so it cannot use the read-only load above. */` |
|   75377 | 1198 | `		iP2 = 2;` |
| 3058467 | 1199 | `	}else if( iCompileFlag & EXPR_FLAG_DEFER_ARG ){` |
|       - | 1200 | ``		/* D1 deferred call argument. For a plain `$var` (iVv == 0, p3 holds the name)`` |
|       - | 1201 | `		 * emit the deferred load (iP1 stays 1 = no create, iP2 = 3): an undefined` |
|       - | 1202 | `		 * variable is left uncreated and silent, carrying a lazy-lvalue marker that` |
|       - | 1203 | `		 * OP_CALL resolves against the callee's by-ref flags. A variable-variable` |
|       - | 1204 | `		 * ($$x) computes its name on the stack (p3 == 0), so it cannot carry the` |
|       - | 1205 | `		 * marker — fall back to the historical eager create (iP1 = 0), which keeps` |
|       - | 1206 | `		 * its by-ref binding working exactly as before. */` |
|  659189 | 1207 | `		if( iVv == 0 ){` |
|  659183 | 1208 | `			iP2 = 3;` |
|  329594 | 1209 | `		}else{` |
|       9 | 1210 | `			iP1 = 0;` |
|       - | 1211 | `		}` |
|  329592 | 1212 | `	}` |
|       - | 1213 | `	/* Emit the load instruction(s). For a variable-variable ($$x, $$$x, ...) every` |
|       - | 1214 | `	 * load EXCEPT the final dereference resolves a NAME: a pure read that warns on an` |
|       - | 1215 | `	 * undefined name (php) and never creates it. Only the last load is the actual` |
|       - | 1216 | `	 * variable and carries the caller's write/create context (iP1). Emitting the` |
|       - | 1217 | ``	 * outer create-mode for the name loads silently invented $n in `$$n = 5` and`` |
|       - | 1218 | ``	 * skipped php's `Undefined variable $n` warning; a quiet outer (isset/empty)`` |
|       - | 1219 | `	 * still suppresses the name warning as php does. */` |
| 3109103 | 1220 | `	if( iVv > 0 ){` |
|      41 | 1221 | `		sxi32 iP2Name = (iCompileFlag & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY))` |
|       - | 1222 | ``			? 1 /* isset()/empty() suppress the name warning too; `??` (QUIET_VAR)`` |
|      18 | 1223 | `			     * does NOT — it warns the name and quiets only the target read. */ : 0;` |
|      41 | 1224 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,1/* read-only name */,iP2Name,p3,0);` |
|      43 | 1225 | `		while( iVv > 1 ){` |
|       3 | 1226 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,1/* read-only name */,iP2Name,0,0);` |
|       3 | 1227 | `			iVv--;` |
|       1 | 1228 | `		}` |
|       - | 1229 | `		/* Final dereference: the actual variable, in the caller's context. */` |
|      41 | 1230 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,iP2,0,0);` |
|      23 | 1231 | `	}else{` |
| 3109067 | 1232 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,iP2,p3,0);` |
|       - | 1233 | `	}` |
|       - | 1234 | `	/* Node successfully compiled */` |
| 3109103 | 1235 | `	return SXRET_OK;` |
| 1554556 | 1236 | `}` |
|       - | 1237 | `/*` |
|       - | 1238 | ` * Load a literal.` |
|       - | 1239 | ` */` |
| 1329016 | 1240 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       5 | 1241 | `{` |
| 1329021 | 1242 | `	SyToken *pToken = pGen->pIn;` |
|       - | 1243 | `	ph7_value *pObj;` |
|       - | 1244 | `	SyString *pStr;` |
|       - | 1245 | `	SyString sCanon;` |
|       - | 1246 | `	sxu32 nIdx;` |
|       - | 1247 | `	/* Extract token value */` |
| 1329021 | 1248 | `	pStr = &pToken->sData;` |
|       - | 1249 | `	/* php's MAGIC constants are case-insensitive like the rest of its reserved words —` |
|       - | 1250 | ``	 * `__line__`, `__Dir__` and `__CLASS__` are one constant each — but every one of them`` |
|       - | 1251 | `	 * is recognised by a BYTE-EXACT compare: the compile-time branches just below, and` |
|       - | 1252 | ``	 * `__CLASS__` through constant.c's (deliberately case-sensitive) constant table. Fold`` |
|       - | 1253 | `	 * the spelling to the canonical upper case here, once, so both mechanisms see it; any` |
|       - | 1254 | ``	 * other spelling used to reach the plain-literal path and raise `Undefined constant`` |
|       - | 1255 | ``	 * "__line__"`. Two of the branches below also read a single byte to tell a pair apart`` |
|       - | 1256 | ``	 * (`zString[2]` for __DIR__ vs __FILE__ and __METHOD__ vs __FUNCTION__), which only`` |
|       - | 1257 | `	 * works on the canonical form.` |
|       - | 1258 | `	 *` |
|       - | 1259 | `	 * User constants stay case-SENSITIVE (php) — this fold is limited to the eight names` |
|       - | 1260 | ``	 * below, and skips a member NAME, where `C::__LINE__` is an ordinary class constant. */`` |
| 1329016 | 1261 | `	if( (pToken->nType & PH7_TK_MEMBER_NAME) == 0 && pStr->nByte > 4` |
| 1178807 | 1262 | `		&& pStr->zString[0] == '_' && pStr->zString[1] == '_' ){` |
|       - | 1263 | `		static const char * const azMagic[] = {` |
|       - | 1264 | `			"__LINE__", "__FILE__", "__DIR__", "__FUNCTION__", "__CLASS__",` |
|       - | 1265 | `			"__METHOD__", "__NAMESPACE__", "__TRAIT__"` |
|       - | 1266 | `		};` |
|       - | 1267 | `		sxu32 i;` |
|    1659 | 1268 | `		for( i = 0 ; i < SX_ARRAYSIZE(azMagic) ; ++i ){` |
|    1645 | 1269 | `			sxu32 nMagic = SyStrlen(azMagic[i]);` |
|    1645 | 1270 | `			if( pStr->nByte == nMagic && SyStrnicmp(pStr->zString,azMagic[i],nMagic) == 0 ){` |
|     419 | 1271 | `				SyStringInitFromBuf(&sCanon,azMagic[i],nMagic);` |
|     419 | 1272 | `				pStr = &sCanon;` |
|     419 | 1273 | `				break;` |
|       - | 1274 | `			}` |
|     618 | 1275 | `		}` |
|     214 | 1276 | `	}` |
|       - | 1277 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first. A reserved` |
|       - | 1278 | `	 * word used as a member NAME (Enum::Null, C::Array, $o->list()) is a plain` |
|       - | 1279 | `	 * identifier — the parser flagged its token so the whole value-folding chain is` |
|       - | 1280 | `	 * skipped and it falls through to the ordinary string-literal emit below. */` |
| 1329021 | 1281 | `	if( pToken->nType & PH7_TK_MEMBER_NAME ){` |
|       - | 1282 | `		/* fall through to the plain-string literal path */` |
| 1305788 | 1283 | `	}else if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  122735 | 1284 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - | 1285 | `			/* NULL constant are always indexed at 0 */` |
|   43769 | 1286 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   43769 | 1287 | `			return SXRET_OK;` |
|   78971 | 1288 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - | 1289 | `			/* TRUE constant are always indexed at 1 */` |
|   29669 | 1290 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|   29669 | 1291 | `			return SXRET_OK;` |
|       5 | 1292 | `		}` |
| 1289672 | 1293 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|  210392 | 1294 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - | 1295 | `			/* FALSE constant are always indexed at 2 */` |
|  152499 | 1296 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|  152499 | 1297 | `			return SXRET_OK;` |
| 1039134 | 1298 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   63606 | 1299 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - | 1300 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|      18 | 1301 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      18 | 1302 | `			if( pObj == 0 ){` |
|     ! 0 | 1303 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1304 | `				return SXERR_ABORT;` |
|       - | 1305 | `			}` |
|      18 | 1306 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - | 1307 | `			/* Emit the load constant instruction */` |
|      18 | 1308 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      18 | 1309 | `			return SXRET_OK;` |
| 1039105 | 1310 | `	}else if( (pStr->nByte == sizeof("__FILE__") - 1 &&` |
|   94149 | 1311 | `		SyMemcmp(pStr->zString,"__FILE__",sizeof("__FILE__")-1) == 0) \|\|` |
| 1037672 | 1312 | `		(pStr->nByte == sizeof("__DIR__") - 1 &&` |
|   61108 | 1313 | `		SyMemcmp(pStr->zString,"__DIR__",sizeof("__DIR__")-1) == 0) ){` |
|       - | 1314 | `			/* __FILE__ / __DIR__ are magic constants resolved at COMPILE time to the` |
|       - | 1315 | `			 * file being compiled (where the token is written), NOT the runtime` |
|       - | 1316 | `			 * execution file. A function defined in a.php reporting __FILE__ must say` |
|       - | 1317 | `			 * a.php even when called from b.php — php semantics, and what Composer's` |
|       - | 1318 | ``			 * autoloader (loadClassLoader's `require __DIR__ . '/ClassLoader.php'`)`` |
|       - | 1319 | `			 * relies on. The runtime-constant path returned the caller's file. */` |
|     269 | 1320 | `			int bDir = (pStr->zString[2] == 'D'); /* __DIR__ vs __FILE__ */` |
|     269 | 1321 | `			SyString *pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     269 | 1322 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     269 | 1323 | `			if( pObj == 0 ){` |
|     ! 0 | 1324 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1325 | `				return SXERR_ABORT;` |
|       - | 1326 | `			}` |
|     269 | 1327 | `			if( pFile && pFile->nByte > 0 ){` |
|     269 | 1328 | `				if( bDir ){` |
|       - | 1329 | `					const char *zDir;` |
|       - | 1330 | `					int nLen;` |
|       - | 1331 | `					SyString sDir;` |
|     149 | 1332 | `					zDir = PH7_ExtractDirName(pFile->zString,(int)pFile->nByte,&nLen);` |
|     149 | 1333 | `					SyStringInitFromBuf(&sDir,zDir,nLen);` |
|     149 | 1334 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&sDir);` |
|      77 | 1335 | `				}else{` |
|     125 | 1336 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,pFile);` |
|       - | 1337 | `				}` |
|     137 | 1338 | `			}else{` |
|       - | 1339 | `				SyString sMem;` |
|     ! 0 | 1340 | `				SyStringInitFromBuf(&sMem,":MEMORY:",sizeof(":MEMORY:")-1);` |
|     ! 0 | 1341 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sMem);` |
|       - | 1342 | `			}` |
|     269 | 1343 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     269 | 1344 | `			return SXRET_OK;` |
| 1019798 | 1345 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   25494 | 1346 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|       - | 1347 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|      15 | 1348 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      15 | 1349 | `			if( pObj == 0 ){` |
|     ! 0 | 1350 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1351 | `				return SXERR_ABORT;` |
|       - | 1352 | `			}` |
|      15 | 1353 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - | 1354 | `				SyString sNs;` |
|       8 | 1355 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       8 | 1356 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|       5 | 1357 | `			}else{` |
|       7 | 1358 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - | 1359 | `			}` |
|      15 | 1360 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      15 | 1361 | `			return SXRET_OK;` |
| 1047795 | 1362 | `	}else if( pStr->nByte == sizeof("__TRAIT__") - 1 &&` |
|   81512 | 1363 | `		SyMemcmp(pStr->zString,"__TRAIT__",sizeof("__TRAIT__")-1) == 0 ){` |
|       - | 1364 | `			/* __TRAIT__ magic constant: the name of the trait whose SOURCE lexically` |
|       - | 1365 | `			 * encloses this token. php resolves it at compile time, and it is shared` |
|       - | 1366 | `			 * across every using class because a trait method body compiles ONCE with` |
|       - | 1367 | `			 * the trait as its owner (PH7_ClassUseTrait adopts the same method pointer).` |
|       - | 1368 | `			 * Unlike __FUNCTION__/__METHOD__ (nearest function), __TRAIT__ is LEXICAL:` |
|       - | 1369 | `			 * closures and arrow-fns are TRANSPARENT (a closure inside a trait method still` |
|       - | 1370 | `			 * yields the trait), so we skip them and keep walking outward — but a class` |
|       - | 1371 | `			 * method or a plain function is an OPAQUE lexical boundary that fixes the answer.` |
|       - | 1372 | `			 * An anonymous class defined inside a trait method is a fresh scope, so` |
|       - | 1373 | `			 * __TRAIT__ is "" there, not the enclosing trait. "" outside any trait (global` |
|       - | 1374 | `			 * scope, plain functions, non-trait methods) — php renders it the empty string,` |
|       - | 1375 | `			 * not NULL. */` |
|      55 | 1376 | `			ph7_class *pTrait = 0;` |
|      55 | 1377 | `			if( pGen->iInMemberDefault > 0 ){` |
|       - | 1378 | `				/* A property/parameter DEFAULT is a const-expression that belongs to the` |
|       - | 1379 | `				 * class whose body is being compiled (pCurClass), never to a lexically-` |
|       - | 1380 | `				 * enclosing method. Read pCurClass directly — the block chain has no func` |
|       - | 1381 | `				 * block for the default and would leak into the enclosing function (an` |
|       - | 1382 | `				 * anonymous class's default inside a trait method is the anon's scope, "").*/` |
|      13 | 1383 | `				if( pGen->pCurClass && (pGen->pCurClass->iFlags & PH7_CLASS_TRAIT) ){` |
|       5 | 1384 | `					pTrait = pGen->pCurClass;` |
|       2 | 1385 | `				}` |
|       7 | 1386 | `			}else{` |
|      43 | 1387 | `				GenBlock *pBlock = pGen->pCurrent;` |
|     105 | 1388 | `				while( pBlock ){` |
|     101 | 1389 | `					if( pBlock->iFlags & GEN_BLOCK_FUNC ){` |
|      54 | 1390 | `						ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      54 | 1391 | `						if( pFunc == 0 ){` |
|       - | 1392 | `							/* A SYNTHETIC function block carries no ph7_vm_func — e.g. the` |
|       - | 1393 | `							 * per-arm throw-fixup block GenStateCompileMatchSubExpr enters to` |
|       - | 1394 | `							 * host a match() expression. It is not a real lexical scope` |
|       - | 1395 | `							 * boundary, so stay transparent and keep walking outward. */` |
|       5 | 1396 | `							pBlock = pBlock->pParent;` |
|       5 | 1397 | `							continue;` |
|       - | 1398 | `						}` |
|      50 | 1399 | `						if( pFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|       - | 1400 | `							/* A class method (of any class, including an anonymous one) is an` |
|       - | 1401 | `							 * OPAQUE scope boundary and fixes the answer: a trait method yields` |
|       - | 1402 | `							 * its trait, any other class's method yields "". Tested BEFORE the` |
|       - | 1403 | `							 * closure flags so a static method is never mistaken for transparent. */` |
|      34 | 1404 | `							if( pFunc->pUserData` |
|      36 | 1405 | `								&& (((ph7_class *)pFunc->pUserData)->iFlags & PH7_CLASS_TRAIT) ){` |
|      30 | 1406 | `								pTrait = (ph7_class *)pFunc->pUserData;` |
|      14 | 1407 | `							}` |
|      36 | 1408 | `							break;` |
|       - | 1409 | `						}` |
|      15 | 1410 | `						if( pFunc->iFlags & (VM_FUNC_CLOSURE\|VM_FUNC_ARROW\|VM_FUNC_STATIC_CL) ){` |
|       - | 1411 | `							/* Closure / arrow fn (VM_FUNC_CLOSURE is only set when the closure` |
|       - | 1412 | `							 * captures, so a capture-less static closure carries only` |
|       - | 1413 | `							 * VM_FUNC_STATIC_CL — include it). Transparent: keep walking outward. */` |
|      13 | 1414 | `							pBlock = pBlock->pParent;` |
|      13 | 1415 | `							continue;` |
|       - | 1416 | `						}` |
|       - | 1417 | `						/* A plain named function is an opaque boundary: __TRAIT__ is "". */` |
|       3 | 1418 | `						break;` |
|       - | 1419 | `					}` |
|      49 | 1420 | `					pBlock = pBlock->pParent;` |
|       3 | 1421 | `				}` |
|       - | 1422 | `			}` |
|      55 | 1423 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      55 | 1424 | `			if( pObj == 0 ){` |
|     ! 0 | 1425 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1426 | `				return SXERR_ABORT;` |
|       - | 1427 | `			}` |
|      55 | 1428 | `			if( pTrait ){` |
|      35 | 1429 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&pTrait->sName);` |
|      19 | 1430 | `			}else{` |
|      22 | 1431 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0); /* empty string */` |
|       - | 1432 | `			}` |
|      55 | 1433 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      55 | 1434 | `			return SXRET_OK;` |
| 1045790 | 1435 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|  127380 | 1436 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
| 1056695 | 1437 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|   99518 | 1438 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      62 | 1439 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - | 1440 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|       - | 1441 | `			/* Skip SYNTHETIC function blocks (GEN_BLOCK_FUNC with no ph7_vm_func in` |
|       - | 1442 | `			 * pUserData — e.g. the per-arm throw-fixup block a match() expression enters):` |
|       - | 1443 | `			 * they are not real function scopes. Without this a __FUNCTION__/__METHOD__` |
|       - | 1444 | `			 * inside a match arm reached a NULL pUserData and dereferenced it (compile-time` |
|       - | 1445 | `			 * crash); php resolves to the enclosing real function, which the walk now finds. */` |
|     153 | 1446 | `			while( pBlock && ((pBlock->iFlags & GEN_BLOCK_FUNC) == 0 \|\| pBlock->pUserData == 0) ){` |
|       - | 1447 | `				/* Point to the upper block */` |
|      66 | 1448 | `				pBlock = pBlock->pParent;` |
|       4 | 1449 | `			}` |
|      62 | 1450 | `			if( pBlock == 0 ){` |
|       - | 1451 | `				/* Called in the global scope,load NULL */` |
|       5 | 1452 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 | 1453 | `			}else{` |
|       - | 1454 | `				/* Extract the target function/method */` |
|      58 | 1455 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      58 | 1456 | `				int bMethod = (pStr->zString[2] == 'M'); /* __METHOD__ vs __FUNCTION__ */` |
|      58 | 1457 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      58 | 1458 | `				if( pObj == 0 ){` |
|     ! 0 | 1459 | `					PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1460 | `					return SXERR_ABORT;` |
|       - | 1461 | `				}` |
|       - | 1462 | `				/*` |
|       - | 1463 | `				 * __METHOD__ is the QUALIFIED name: "C::m" inside a method, and the plain` |
|       - | 1464 | `				 * function name inside a plain function (php does not answer "" there —` |
|       - | 1465 | `				 * PH7 loaded NULL, so __METHOD__ was empty in every free function and` |
|       - | 1466 | `				 * unqualified in every method).` |
|       - | 1467 | `				 */` |
|       - | 1468 | ``				/* A property hook answers php's `$p::get`, never the method name`` |
|       - | 1469 | ``				 * PHL synthesizes for it — so __METHOD__ reads `C::$p::get`, the`` |
|       - | 1470 | `				 * same rendering the runtime diagnostics use. */` |
|       - | 1471 | `				{` |
|       - | 1472 | `					SyString sProp,sSelf;` |
|       - | 1473 | `					const char *zKind;` |
|       - | 1474 | `					SyBlob sQual;` |
|       - | 1475 | `					SyString sOut;` |
|      58 | 1476 | `					int bHook = PH7_VmHookSplitName(&pFunc->sName,&sProp,&zKind);` |
|      58 | 1477 | `					SyBlobInit(&sQual,&pGen->pVm->sAllocator);` |
|      58 | 1478 | `					if( bHook ){` |
|       7 | 1479 | `						SyBlobFormat(&sQual,"$%z::%s",&sProp,zKind);` |
|       7 | 1480 | `						SyStringInitFromBuf(&sSelf,SyBlobData(&sQual),SyBlobLength(&sQual));` |
|      55 | 1481 | `					}else if( SyStringLength(&pFunc->sClosureName) > 0 ){` |
|       - | 1482 | ``						/* A closure answers php's `{closure:...}` name, never the`` |
|       - | 1483 | `						 * synthesized lookup key — and __METHOD__ answers the SAME text` |
|       - | 1484 | `						 * (the name already carries the declaring class), so the` |
|       - | 1485 | `						 * qualification below must not run for it. */` |
|      15 | 1486 | `						sSelf = pFunc->sClosureName;` |
|      15 | 1487 | `						bMethod = 0;` |
|       8 | 1488 | `					}else{` |
|      37 | 1489 | `						sSelf = pFunc->sName;` |
|       - | 1490 | `					}` |
|      66 | 1491 | `					if( bMethod && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|      19 | 1492 | `						SyString *pCls = &((ph7_class *)pFunc->pUserData)->sName;` |
|       - | 1493 | `						SyBlob sFull;` |
|      19 | 1494 | `						SyBlobInit(&sFull,&pGen->pVm->sAllocator);` |
|      19 | 1495 | `						SyBlobFormat(&sFull,"%z::%z",pCls,&sSelf);` |
|      19 | 1496 | `						SyStringInitFromBuf(&sOut,SyBlobData(&sFull),SyBlobLength(&sFull));` |
|      19 | 1497 | `						PH7_MemObjInitFromString(pGen->pVm,pObj,&sOut);` |
|      19 | 1498 | `						SyBlobRelease(&sFull);` |
|      11 | 1499 | `					}else{` |
|      42 | 1500 | `						PH7_MemObjInitFromString(pGen->pVm,pObj,&sSelf);` |
|       - | 1501 | `					}` |
|      58 | 1502 | `					SyBlobRelease(&sQual);` |
|       - | 1503 | `				}` |
|       - | 1504 | `				/* Emit the load constant instruction */` |
|      58 | 1505 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 1506 | `			}` |
|      62 | 1507 | `			return SXRET_OK;` |
|       - | 1508 | `	}` |
|       - | 1509 | ``	/* php keywords are CASE-INSENSITIVE (`SELF::C`, `Parent::m()`, `new STATIC`,`` |
|       - | 1510 | ``	 * `ISSET($x)`), but a few of them reach the engine as THIS literal and are matched`` |
|       - | 1511 | `	 * there BYTE-EXACTLY: the scope keywords against "self"/"parent"/"static" (OP_MEMBER,` |
|       - | 1512 | `	 * OP_NEW, the FCC scope resolver, the error formatter), and isset/empty/eval as the` |
|       - | 1513 | `	 * name of the host function their call dispatches to. Emit the canonical lower-case` |
|       - | 1514 | `	 * spelling for exactly those so the source's case never reaches the match — PH7` |
|       - | 1515 | ``	 * emitted the raw text, so `SELF::C` looked for a class literally named "SELF" and`` |
|       - | 1516 | ``	 * `ISSET($x)` for a function named "ISSET".`` |
|       - | 1517 | `	 *` |
|       - | 1518 | `	 * Every OTHER keyword literal keeps its source case on purpose: it is a CONSTANT` |
|       - | 1519 | ``	 * read (`define('OBJECT',1); echo OBJECT;` — PHL's keyword table covers type names`` |
|       - | 1520 | `	 * php's lexer does not reserve), and php constants are case-SENSITIVE. So is a` |
|       - | 1521 | ``	 * keyword used as a member NAME, which the parser flags: `class C { const STATIC = 5; }`` |
|       - | 1522 | ``	 * echo C::STATIC;` names the constant "STATIC", and folding it looked for "static". */`` |
| 1102697 | 1523 | `	if( (pToken->nType & PH7_TK_KEYWORD) && (pToken->nType & PH7_TK_MEMBER_NAME) == 0 ){` |
|   13151 | 1524 | `		sxu32 nKeyID = (sxu32)SX_PTR_TO_INT(pToken->pUserData);` |
|   13151 | 1525 | `		const char *zCanon = 0;` |
|   13151 | 1526 | `		if( nKeyID == PH7_TKWRD_SELF ){` |
|     319 | 1527 | `			zCanon = "self";` |
|   12994 | 1528 | `		}else if( nKeyID == PH7_TKWRD_PARENT ){` |
|     114 | 1529 | `			zCanon = "parent";` |
|   12782 | 1530 | `		}else if( nKeyID == PH7_TKWRD_STATIC ){` |
|      94 | 1531 | `			zCanon = "static";` |
|   12682 | 1532 | `		}else if( nKeyID == PH7_TKWRD_ISSET ){` |
|   12321 | 1533 | `			zCanon = "isset";` |
|    6479 | 1534 | `		}else if( nKeyID == PH7_TKWRD_EMPTY ){` |
|     189 | 1535 | `			zCanon = "empty";` |
|     229 | 1536 | `		}else if( nKeyID == PH7_TKWRD_EVAL ){` |
|     123 | 1537 | `			zCanon = "eval";` |
|      59 | 1538 | `		}` |
|   13151 | 1539 | `		if( zCanon ){` |
|   13137 | 1540 | `			SyStringInitFromBuf(&sCanon,zCanon,SyStrlen(zCanon));` |
|   13137 | 1541 | `			pStr = &sCanon;` |
|    6566 | 1542 | `		}` |
|    6573 | 1543 | `	}` |
|       - | 1544 | `	/* Query literal table */` |
| 1102697 | 1545 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - | 1546 | `		ph7_value *pLitObj;` |
|       - | 1547 | `		/* Unknown literal,install it in the literal table */` |
|  438101 | 1548 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  438101 | 1549 | `		if( pLitObj == 0 ){` |
|     ! 0 | 1550 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 1551 | `			return SXERR_ABORT;` |
|       - | 1552 | `		}` |
|  438101 | 1553 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,pStr);` |
|  438101 | 1554 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  219048 | 1555 | `	}` |
|       - | 1556 | `	/* Emit the load constant instruction.` |
|       - | 1557 | `	 *` |
|       - | 1558 | `	 * php resolves an UNQUALIFIED constant against the namespace its SOURCE sits in,` |
|       - | 1559 | `	 * decided at COMPILE time — so a function keeps its own namespace when called from` |
|       - | 1560 | ``	 * another one — and in a fixed order: a `use const` import first (and an import has`` |
|       - | 1561 | ``	 * NO global fallback), else `current-namespace\NAME`, else the global `NAME`. The`` |
|       - | 1562 | `	 * candidate that order picks is resolved here and travels in the instruction's p3;` |
|       - | 1563 | `	 * the VM's own lookup of the bare literal is then just the global step, and the` |
|       - | 1564 | `	 * "Undefined constant" message names the candidate, as php does.` |
|       - | 1565 | `	 *` |
|       - | 1566 | `	 * Only a name that could BE a constant needs one: a keyword literal never is (the` |
|       - | 1567 | `	 * scope keywords and isset/empty/eval reach the OO and call handlers by this same` |
|       - | 1568 | `	 * literal), and a call/new site clears PH7_LOADC_EXPAND before the constant path` |
|       - | 1569 | `	 * can ever run. */` |
|       - | 1570 | `	{` |
| 1102697 | 1571 | `		sxi32 iLoadFlags = PH7_LOADC_EXPAND;` |
| 1102697 | 1572 | `		char *zCand = 0;` |
| 1102697 | 1573 | `		if( (pToken->nType & PH7_TK_KEYWORD) == 0 ){` |
| 1633787 | 1574 | `			SyHashEntry *pImport = SyHashGet(&pGen->hUseConstImports,` |
| 1089188 | 1575 | `				(const void *)pStr->zString,pStr->nByte);` |
| 1089193 | 1576 | `			if( pImport ){` |
|      31 | 1577 | `				const char *zFQN = (const char *)pImport->pUserData;` |
|      31 | 1578 | `				zCand = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFQN,SyStrlen(zFQN));` |
|      31 | 1579 | `				iLoadFlags \|= PH7_LOADC_NOGLOBAL;` |
| 1089180 | 1580 | `			}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - | 1581 | `				SyBlob sCand;` |
|     481 | 1582 | `				SyBlobInit(&sCand,&pGen->pVm->sAllocator);` |
|     481 | 1583 | `				SyBlobAppend(&sCand,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     481 | 1584 | `				SyBlobAppend(&sCand,"\\",1);` |
|     481 | 1585 | `				SyBlobAppend(&sCand,pStr->zString,pStr->nByte);` |
|     719 | 1586 | `				zCand = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     476 | 1587 | `					(const char *)SyBlobData(&sCand),SyBlobLength(&sCand));` |
|     481 | 1588 | `				SyBlobRelease(&sCand);` |
|     238 | 1589 | `			}` |
|  544594 | 1590 | `		}` |
| 1102697 | 1591 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,iLoadFlags,nIdx,zCand,0);` |
|       - | 1592 | `	}` |
| 1102697 | 1593 | `	return SXRET_OK;` |
|  664513 | 1594 | `}` |
|       - | 1595 | `/*` |
|       - | 1596 | ` * Resolve a namespace path or simply load a literal.` |
|       - | 1597 | ` * If the token stream contains namespace separators (backslashes),` |
|       - | 1598 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - | 1599 | ` * Otherwise, load the simple literal directly.` |
|       - | 1600 | ` */` |
| 1329444 | 1601 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       5 | 1602 | `{` |
|       - | 1603 | `	sxi32 rc;` |
| 1329449 | 1604 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 1605 | `		return SXRET_OK;` |
|       - | 1606 | `	}` |
|       - | 1607 | `	/* Check if this is a multi-token namespace path */` |
| 1329449 | 1608 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - | 1609 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|     433 | 1610 | `		SyBlob *pWorker = &pGen->sWorker;` |
|     433 | 1611 | `		int isAbsolute = 0;` |
|     433 | 1612 | `		SyBlobReset(pWorker);` |
|       - | 1613 | `		/* Check for leading backslash (absolute path) */` |
|     433 | 1614 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|     133 | 1615 | `			isAbsolute = 1;` |
|     133 | 1616 | `			pGen->pIn++; /* Skip leading backslash */` |
|      64 | 1617 | `		}` |
|       - | 1618 | `		/* Collect the raw path (no prefix yet) so its FIRST segment can be resolved` |
|       - | 1619 | ``		 * against use-imports below — php resolves `A\B\C` by mapping the leading`` |
|       - | 1620 | ``		 * `A` through the imports (`use X\A;` makes it `X\A\B\C`), and only prepends`` |
|       - | 1621 | ``		 * the current namespace when `A` matches no import. Blindly prefixing the`` |
|       - | 1622 | ``		 * namespace here produced e.g. `Ns\Ev\Bus` for `use ...\Event as Ev; Ev\Bus`. */`` |
|       - | 1623 | `		{` |
|       - | 1624 | `			SyBlob sRaw;` |
|     433 | 1625 | `			SyBlobInit(&sRaw,&pGen->pVm->sAllocator);` |
|       - | 1626 | ``			/* `namespace\X` spells the CURRENT namespace and is FULLY QUALIFIED from`` |
|       - | 1627 | `			 * there: no import ever applies to it, and the namespace is already in. */` |
|     433 | 1628 | `			if( !isAbsolute && GenStateNsRelPrefix(pGen,&pGen->pIn,pGen->pEnd,&sRaw) ){` |
|      68 | 1629 | `				isAbsolute = 1;` |
|      33 | 1630 | `			}` |
|    1213 | 1631 | `			while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|    1213 | 1632 | `				if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|     395 | 1633 | `					SyBlobAppend(&sRaw,"\\",1);` |
|     200 | 1634 | `				}else{` |
|     823 | 1635 | `					SyBlobAppend(&sRaw,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - | 1636 | `				}` |
|    1213 | 1637 | `				if( pGen->pIn == &pGen->pEnd[-1] ){` |
|     433 | 1638 | `					pGen->pIn++;` |
|     433 | 1639 | `					break;` |
|       - | 1640 | `				}` |
|     785 | 1641 | `				pGen->pIn++;` |
|       5 | 1642 | `			}` |
|     433 | 1643 | `			if( isAbsolute ){` |
|     199 | 1644 | `				SyBlobAppend(pWorker,SyBlobData(&sRaw),SyBlobLength(&sRaw)); /* FQN as written */` |
|     102 | 1645 | `			}else{` |
|     238 | 1646 | `				const char *zRaw = (const char *)SyBlobData(&sRaw);` |
|     238 | 1647 | `				sxu32 nRaw = SyBlobLength(&sRaw);` |
|     238 | 1648 | `				sxu32 nFirst = 0;` |
|       - | 1649 | `				SyHashEntry *pNsImp;` |
|    1346 | 1650 | `				while( nFirst < nRaw && zRaw[nFirst] != '\\' ){ nFirst++; }` |
|     238 | 1651 | `				pNsImp = SyHashGet(&pGen->hUseImports,(const void *)zRaw,nFirst);` |
|     238 | 1652 | `				if( pNsImp ){` |
|       - | 1653 | `					/* Leading segment is an imported alias: substitute its FQN. */` |
|      26 | 1654 | `					const char *zFQN = (const char *)pNsImp->pUserData;` |
|      26 | 1655 | `					SyBlobAppend(pWorker,zFQN,SyStrlen(zFQN));` |
|      26 | 1656 | `					SyBlobAppend(pWorker,zRaw + nFirst,nRaw - nFirst);` |
|     226 | 1657 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       6 | 1658 | `					SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       6 | 1659 | `					SyBlobAppend(pWorker,"\\",1);` |
|       6 | 1660 | `					SyBlobAppend(pWorker,zRaw,nRaw);` |
|       4 | 1661 | `				}else{` |
|     210 | 1662 | `					SyBlobAppend(pWorker,zRaw,nRaw); /* global scope, no import */` |
|       - | 1663 | `				}` |
|       - | 1664 | `			}` |
|     433 | 1665 | `			SyBlobRelease(&sRaw);` |
|       - | 1666 | `		}` |
|     433 | 1667 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - | 1668 | `			ph7_value *pObj;` |
|       - | 1669 | `			SyString sPath;` |
|       - | 1670 | `			sxu32 nIdx;` |
|     433 | 1671 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - | 1672 | `			/* Install in the literal table */` |
|     433 | 1673 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|     139 | 1674 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     139 | 1675 | `				if( pObj == 0 ){` |
|     ! 0 | 1676 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 1677 | `					return SXERR_ABORT;` |
|       - | 1678 | `				}` |
|     139 | 1679 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|     139 | 1680 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      67 | 1681 | `			}` |
|       - | 1682 | `			/* Emit the load constant instruction.` |
|       - | 1683 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|       - | 1684 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|     647 | 1685 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|     214 | 1686 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|     214 | 1687 | `				nIdx,0,0);` |
|     433 | 1688 | `			return SXRET_OK;` |
|       - | 1689 | `		}` |
|     ! 0 | 1690 | `	}` |
|       - | 1691 | `	/* Single-token literal: load directly */` |
| 1329021 | 1692 | `	rc = GenStateLoadLiteral(&(*pGen));` |
| 1329021 | 1693 | `	return rc;` |
|  664727 | 1694 | `}` |
|       - | 1695 | `/*` |
|       - | 1696 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - | 1697 | ` */` |
|       - | 1698 | `/*` |
|       - | 1699 | `` * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of`` |
|       - | 1700 | `` * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument`` |
|       - | 1701 | ``  * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...` `` |
|       - | 1702 | ` * appeared outside a call argument list — a syntax error (PHP rejects it likewise).` |
|       - | 1703 | ` */` |
|     ! 0 | 1704 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|     ! 0 | 1705 | `{` |
|     ! 0 | 1706 | `	SXUNUSED(iCompileFlag);` |
|     ! 0 | 1707 | `	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|       - | 1708 | `		"Cannot use the first-class callable syntax '...' here");` |
|     ! 0 | 1709 | `	return SXERR_SYNTAX;` |
|     ! 0 | 1710 | `}` |
| 1329444 | 1711 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 | 1712 | `{` |
|       - | 1713 | `	sxi32 rc;` |
| 1329449 | 1714 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
| 1329449 | 1715 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1716 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1717 | `		return rc;` |
|       - | 1718 | `	}` |
|       - | 1719 | `	/* Node successfully compiled */` |
| 1329449 | 1720 | `	return SXRET_OK;` |
|  664727 | 1721 | `}` |
|       - | 1722 |  |
