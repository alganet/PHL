# src/ph7/compile_node.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 902/1045 lines (86.32%)

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
|   15095 |   37 | `static void GenStateClosureName(ph7_gen_state *pGen,sxu32 nLine)` |
|       5 |   38 | `{` |
|   15100 |   39 | `	ph7_vm_func *pOuter = 0;` |
|   15100 |   40 | `	GenBlock *pBlock = pGen->pCurrent;` |
|       - |   41 | `	SyBlob sName;` |
|       - |   42 | `	char *zDup;` |
|       - |   43 | `	/* Innermost REAL function block: a synthetic one (a match() arm's throw-fixup` |
|       - |   44 | `	 * block) carries no ph7_vm_func and is not a scope. */` |
|   31671 |   45 | `	while( pBlock ){` |
|   17604 |   46 | `		if( (pBlock->iFlags & GEN_BLOCK_FUNC) && pBlock->pUserData ){` |
|    1033 |   47 | `			pOuter = (ph7_vm_func *)pBlock->pUserData;` |
|    1033 |   48 | `			break;` |
|       - |   49 | `		}` |
|   16576 |   50 | `		pBlock = pBlock->pParent;` |
|       5 |   51 | `	}` |
|   15100 |   52 | `	SyStringInitFromBuf(&pGen->sPendingClosureName,0,0);` |
|   15100 |   53 | `	SyStringInitFromBuf(&pGen->sPendingClosureScope,0,0);` |
|   15100 |   54 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|   15100 |   55 | `	SyBlobAppend(&sName,"{closure:",sizeof("{closure:")-1);` |
|   15100 |   56 | `	if( pOuter == 0 ){` |
|       - |   57 | `		/* Top level: php writes the compiled file's path (empty when there is none —` |
|       - |   58 | `		 * an eval()/direct-API compile — which is php's "{closure::LINE}" there). */` |
|   14072 |   59 | `		SyString *pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|   14072 |   60 | `		if( pFile && SyStringLength(pFile) > 0 ){` |
|   14072 |   61 | `			SyBlobAppend(&sName,SyStringData(pFile),SyStringLength(pFile));` |
|    6924 |   62 | `		}` |
|    7952 |   63 | `	}else if( SyStringLength(&pOuter->sClosureName) > 0 ){` |
|       - |   64 | `		/* Enclosing closure: its whole name, no parens and no class -- but php's` |
|       - |   65 | `		 * SCOPE is inherited, so a closure written inside a closure inside a method` |
|       - |   66 | `		 * still belongs to that class. */` |
|     860 |   67 | `		SyBlobAppend(&sName,SyStringData(&pOuter->sClosureName),` |
|     285 |   68 | `			SyStringLength(&pOuter->sClosureName));` |
|     575 |   69 | `		pGen->sPendingClosureScope = pOuter->sClosureScope;` |
|     290 |   70 | `	}else{` |
|     463 |   71 | `		if( (pOuter->iFlags & VM_FUNC_CLASS_METHOD) && pOuter->pUserData ){` |
|     137 |   72 | `			SyString *pCls = &((ph7_class *)pOuter->pUserData)->sName;` |
|     137 |   73 | `			SyBlobAppend(&sName,SyStringData(pCls),SyStringLength(pCls));` |
|     137 |   74 | `			SyBlobAppend(&sName,"::",2);` |
|     137 |   75 | `			pGen->sPendingClosureScope = *pCls;` |
|      66 |   76 | `		}` |
|     463 |   77 | `		SyBlobAppend(&sName,SyStringData(&pOuter->sName),SyStringLength(&pOuter->sName));` |
|     463 |   78 | `		SyBlobAppend(&sName,"()",2);` |
|       - |   79 | `	}` |
|   15100 |   80 | `	SyBlobFormat(&sName,":%u}",nLine);` |
|   22532 |   81 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|   15095 |   82 | `		(const char *)SyBlobData(&sName),(sxu32)SyBlobLength(&sName));` |
|   15100 |   83 | `	if( zDup ){` |
|   15100 |   84 | `		SyStringInitFromBuf(&pGen->sPendingClosureName,zDup,(sxu32)SyBlobLength(&sName));` |
|    7432 |   85 | `	}` |
|   15100 |   86 | `	SyBlobRelease(&sName);` |
|   15100 |   87 | `}` |
|       - |   88 | `/*` |
|       - |   89 | ` * Compile an annoynmous function or a closure.` |
|       - |   90 | ` * According to the PHP language reference` |
|       - |   91 | ` *  Anonymous functions, also known as closures, allow the creation of functions` |
|       - |   92 | ` *  which have no specified name. They are most useful as the value of callback` |
|       - |   93 | ` *  parameters, but they have many other uses. Closures can also be used as` |
|       - |   94 | ` *  the values of variables; Assigning a closure to a variable uses the same` |
|       - |   95 | ` *  syntax as any other assignment, including the trailing semicolon:` |
|       - |   96 | ` *  Example Anonymous function variable assignment example` |
|       - |   97 | ` * <?php` |
|       - |   98 | ` * $greet = function($name)` |
|       - |   99 | ` * {` |
|       - |  100 | ` *    printf("Hello %s\r\n", $name);` |
|       - |  101 | ` * };` |
|       - |  102 | ` * $greet('World');` |
|       - |  103 | ` * $greet('PHP');` |
|       - |  104 | ` * ?>` |
|       - |  105 | ` * Note that the implementation of annoynmous function and closure under` |
|       - |  106 | ` * PH7 is completely different from the one used by the zend engine.` |
|       - |  107 | ` */` |
|    6599 |  108 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  109 | `{` |
|    6604 |  110 | `	ph7_vm_func *pAnnonFunc = 0; /* Annonymous function body */` |
|       - |  111 | `	char zName[512];         /* Unique lambda name */` |
|       - |  112 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - |  113 | `							  * one thread is allowed to compile the script.` |
|       - |  114 | `						      */` |
|       - |  115 | `	SyString sName;` |
|    6604 |  116 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `$f = #[A] function…` trivia`` |
|       - |  117 | `	                              * is keyed to this ['static'] 'function' token */` |
|       - |  118 | `	sxu32 nKwLine;` |
|    6604 |  119 | `	sxi32 iFlags = 0;` |
|       - |  120 | `	sxu32 nLen;` |
|       - |  121 | `	sxi32 rc;` |
|    3283 |  122 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  123 |  |
|    6604 |  124 | `	nKwLine = pGen->pIn->nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|    6599 |  125 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|    6604 |  126 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  127 | `		/* Static closure: no $this auto-capture, bind refused */` |
|     401 |  128 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|     401 |  129 | `		pGen->pIn++; /* Jump the 'static' keyword */` |
|     198 |  130 | `	}` |
|    6604 |  131 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|       - |  132 | ``	/* `function &(…) {…}` — a closure returns by reference exactly as a named`` |
|       - |  133 | ``	 * function does, and the `&` sits in the same place. Nothing consumed it here,`` |
|       - |  134 | ``	 * so every by-ref closure was `syntax error, unexpected token "&", expecting`` |
|       - |  135 | ``	 * "("`; the arrow form already read its own. */`` |
|    6604 |  136 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|      14 |  137 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|      14 |  138 | `		pGen->pIn++;` |
|       6 |  139 | `	}` |
|    6604 |  140 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 |  141 | `		pGen->pIn++;` |
|     ! 0 |  142 | `	}` |
|       - |  143 | `	/* Generate a unique name */` |
|    6604 |  144 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - |  145 | `	/* Make sure the generated name is unique */` |
|    6604 |  146 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  147 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 |  148 | `	}` |
|    6604 |  149 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|       - |  150 | `	/* php's visible name for this closure, built before the body compiles so a` |
|       - |  151 | `	 * __FUNCTION__ inside it resolves to the same text php reports. */` |
|    6604 |  152 | `	GenStateClosureName(&(*pGen),nKwLine);` |
|       - |  153 | `	/* Compile the lambda body */` |
|    6604 |  154 | `	rc = GenStateCompileFunc(&(*pGen),&sName,iFlags,TRUE,&pAnnonFunc);` |
|    6604 |  155 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  156 | `		return SXERR_ABORT;` |
|       - |  157 | `	}` |
|    6604 |  158 | `	if( pAnnonFunc ){` |
|    6602 |  159 | `		pAnnonFunc->nLine = nKwLine;` |
|       - |  160 | ``		/* Expression-position attributes (`$f = #[A] function () {}`): the trivia`` |
|       - |  161 | `		 * sidecar keys them to the closure's first keyword token. */` |
|    6602 |  162 | `		if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnnonFunc->aAttrs) == SXERR_ABORT ){` |
|     ! 0 |  163 | `			return SXERR_ABORT;` |
|       - |  164 | `		}` |
|    6602 |  165 | `		if( GenStateCheckAttrPlacement(&(*pGen),&pAnnonFunc->aAttrs,2,2,0,0) == SXERR_ABORT ){` |
|     ! 0 |  166 | `			return SXERR_ABORT;` |
|       - |  167 | `		}` |
|       - |  168 | `		/* A closure's own attributes arrive AFTER its body compiled, so the` |
|       - |  169 | `		 * #[\NoDiscard] rules are decided here rather than with the signature. */` |
|    6602 |  170 | `		if( GenStateApplyNoDiscard(&(*pGen),pAnnonFunc,0,0) == SXERR_ABORT ){` |
|     ! 0 |  171 | `			return SXERR_ABORT;` |
|       - |  172 | `		}` |
|    3282 |  173 | `	}` |
|       - |  174 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|       - |  175 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|       - |  176 | `	 * the handler wraps either in a Closure instance. */` |
|    6604 |  177 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|       - |  178 | `	/* Node successfully compiled */` |
|    6604 |  179 | `	return SXRET_OK;` |
|    3288 |  180 | `}` |
|       - |  181 | `/*` |
|       - |  182 | ` * Add a free variable to the arrow function's closure environment, unless` |
|       - |  183 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|       - |  184 | ` * enclosing arrow level, or has already been captured.` |
|       - |  185 | ` */` |
|    5357 |  186 | `static sxi32 GenStateArrowAddCapture(` |
|       - |  187 | `	ph7_gen_state *pGen,` |
|       - |  188 | `	ph7_vm_func *pFunc,` |
|       - |  189 | `	const char *zName,` |
|       - |  190 | `	sxu32 nByte,` |
|       - |  191 | `	SyString *aShadow,` |
|       - |  192 | `	sxu32 nShadow)` |
|       5 |  193 | `{` |
|       - |  194 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  195 | `	ph7_vm_func_closure_env *aEnv;` |
|       - |  196 | `	sxu32 n, nEnv;` |
|       - |  197 | `	char *zDup;` |
|    5362 |  198 | `	if( nByte == 0 ){` |
|     ! 0 |  199 | `		return SXRET_OK;` |
|       - |  200 | `	}` |
|    5357 |  201 | `	if( nByte == sizeof("this")-1` |
|    2813 |  202 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|      23 |  203 | `		return SXRET_OK;` |
|       - |  204 | `	}` |
|    5342 |  205 | `	if( PH7_VmIsAutoGlobal(zName,nByte) ){` |
|       - |  206 | `		/* php never auto-captures an auto-global — it is already visible inside` |
|       - |  207 | `		 * the arrow function. Capturing one was actively destructive here: the` |
|       - |  208 | `		 * install resolves the name through hSuper and so wrote the by-value` |
|       - |  209 | `		 * SNAPSHOT over the superglobal's own slot, which for $GLOBALS froze the` |
|       - |  210 | `		 * whole symbol-table view at closure-creation time for the rest of the` |
|       - |  211 | `		 * program. */` |
|      62 |  212 | `		return SXRET_OK;` |
|       - |  213 | `	}` |
|    5600 |  214 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|    1488 |  215 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|    1440 |  216 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|    1175 |  217 | `			return SXRET_OK;` |
|       - |  218 | `		}` |
|     163 |  219 | `	}` |
|    4112 |  220 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|    4112 |  221 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|    4668 |  222 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|     941 |  223 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|     816 |  224 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|     390 |  225 | `			return SXRET_OK;` |
|       - |  226 | `		}` |
|     283 |  227 | `	}` |
|    3727 |  228 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|    3727 |  229 | `	if( zDup == 0 ){` |
|     ! 0 |  230 | `		return SXERR_ABORT;` |
|       - |  231 | `	}` |
|    3727 |  232 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|    3727 |  233 | `	sEnv.iFlags = 0;` |
|    3727 |  234 | `	sEnv.nIdx = SXU32_HIGH;` |
|    3727 |  235 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|    3727 |  236 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|    3727 |  237 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|    3727 |  238 | `	return SXRET_OK;` |
|    2670 |  239 | `}` |
|       - |  240 | `/*` |
|       - |  241 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|       - |  242 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|       - |  243 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|       - |  244 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|       - |  245 | ` */` |
|    1319 |  246 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|       - |  247 | `	ph7_gen_state *pGen,` |
|       - |  248 | `	ph7_vm_func *pFunc,` |
|       - |  249 | `	const char *zIn,` |
|       - |  250 | `	const char *zEnd,` |
|       - |  251 | `	SyString *aShadow,` |
|       - |  252 | `	sxu32 nShadow)` |
|       5 |  253 | `{` |
|       - |  254 | `	sxi32 rc;` |
|    6764 |  255 | `	while( zIn < zEnd ){` |
|    5445 |  256 | `		if( zIn[0] == '\\' ){` |
|     469 |  257 | `			zIn++;` |
|     469 |  258 | `			if( zIn < zEnd ){` |
|     469 |  259 | `				zIn++;` |
|     225 |  260 | `			}` |
|     469 |  261 | `			continue;` |
|       - |  262 | `		}` |
|    4976 |  263 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|     157 |  264 | `			&& ((unsigned char)zIn[1] >= 0x80` |
|     152 |  265 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|       - |  266 | `			/* php's label bytes, the flat set (LEX_LABEL_START in lex.c). */` |
|       - |  267 | `			const char *zName;` |
|     157 |  268 | `			zIn++; /* skip '$' */` |
|     157 |  269 | `			zName = zIn;` |
|     543 |  270 | `			while( zIn < zEnd` |
|     793 |  271 | `				&& ((unsigned char)zIn[0] >= 0x80` |
|     744 |  272 | `					\|\| SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_') ){` |
|     641 |  273 | `				zIn++;` |
|       5 |  274 | `			}` |
|     157 |  275 | `			if( zIn > zName ){` |
|     232 |  276 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|     152 |  277 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|     157 |  278 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  279 | `					return SXERR_ABORT;` |
|       - |  280 | `				}` |
|      75 |  281 | `			}` |
|     157 |  282 | `			continue;` |
|       - |  283 | `		}` |
|    4829 |  284 | `		zIn++;` |
|       5 |  285 | `	}` |
|    1324 |  286 | `	return SXRET_OK;` |
|     656 |  287 | `}` |
|       - |  288 | `/*` |
|       - |  289 | ` * Scan the body token range of an arrow function for free-variable` |
|       - |  290 | ` * references and record them in pFunc's closure environment. Handles:` |
|       - |  291 | ` *   - plain $<id> pairs` |
|       - |  292 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|       - |  293 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|       - |  294 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|       - |  295 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|       - |  296 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|       - |  297 | ` *     are never mistakenly captured.` |
|       - |  298 | ` */` |
|    8692 |  299 | `static sxi32 GenStateArrowCaptureScan(` |
|       - |  300 | `	ph7_gen_state *pGen,` |
|       - |  301 | `	ph7_vm_func *pFunc,` |
|       - |  302 | `	SyToken *pStart,` |
|       - |  303 | `	SyToken *pEnd,` |
|       - |  304 | `	SyString *aShadow,` |
|       - |  305 | `	sxu32 nShadow)` |
|       5 |  306 | `{` |
|    8697 |  307 | `	SyToken *pScan = pStart;` |
|       - |  308 | `	sxi32 rc;` |
|   79620 |  309 | `	while( pScan < pEnd ){` |
|   70928 |  310 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|    1975 |  311 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|     651 |  312 | `				pScan->sData.zString,` |
|    1319 |  313 | `				pScan->sData.zString + pScan->sData.nByte,` |
|     651 |  314 | `				aShadow,nShadow);` |
|    1324 |  315 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  316 | `				return SXERR_ABORT;` |
|       - |  317 | `			}` |
|    1324 |  318 | `			pScan++;` |
|    1324 |  319 | `			continue;` |
|       - |  320 | `		}` |
|   69609 |  321 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|     605 |  322 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|     605 |  323 | `			SyToken *pFnKw = (nKw == PH7_TKWRD_STATIC) ? &pScan[1] : pScan;` |
|       - |  324 | ``			/* A NESTED arrow function, not a `$fn`/`C::fn` name that merely`` |
|       - |  325 | `			 * spells the keyword (see PH7_TokenOpensArrowFunc). */` |
|     605 |  326 | `			if( PH7_TokenOpensArrowFunc(pStart,pScan,pEnd) ){` |
|       - |  327 | `				SyToken *pInnerSigStart;` |
|       - |  328 | `				SyToken *pInnerSigEnd;` |
|       - |  329 | `				SyToken *pInnerBodyEnd;` |
|       - |  330 | `				SyString *aInnerShadow;` |
|       - |  331 | `				sxu32 nInnerShadow;` |
|       - |  332 | `				sxu32 nInnerParamMax;` |
|       - |  333 | `				SyToken *p;` |
|       - |  334 | `				int iNestInner;` |
|     197 |  335 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|     197 |  336 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  337 | `					pScan++;` |
|     ! 0 |  338 | `				}` |
|     197 |  339 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  340 | `					pScan++;` |
|     ! 0 |  341 | `					continue;` |
|       - |  342 | `				}` |
|     197 |  343 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|     197 |  344 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|       - |  345 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|     197 |  346 | `				if( pInnerSigEnd >= pEnd ){` |
|     ! 0 |  347 | `					pScan = pEnd;` |
|     ! 0 |  348 | `					continue;` |
|       - |  349 | `				}` |
|       - |  350 | `				/* Build an augmented shadow list: inherited + inner params */` |
|     197 |  351 | `				nInnerParamMax = 0;` |
|     583 |  352 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|     391 |  353 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|     181 |  354 | `						nInnerParamMax++;` |
|      88 |  355 | `					}` |
|     198 |  356 | `				}` |
|     197 |  357 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|     192 |  358 | `					&pGen->pVm->sAllocator,` |
|     192 |  359 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|     197 |  360 | `				if( aInnerShadow == 0 ){` |
|     ! 0 |  361 | `					return SXERR_ABORT;` |
|       - |  362 | `				}` |
|     197 |  363 | `				nInnerShadow = 0;` |
|     247 |  364 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|      53 |  365 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|      28 |  366 | `				}` |
|     583 |  367 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|     391 |  368 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|     215 |  369 | `						continue;` |
|       - |  370 | `					}` |
|     181 |  371 | `					if( &p[1] >= pInnerSigEnd ){` |
|     ! 0 |  372 | `						break;` |
|       - |  373 | `					}` |
|     181 |  374 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  375 | `						continue;` |
|       - |  376 | `					}` |
|     181 |  377 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|      93 |  378 | `				}` |
|     197 |  379 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|     197 |  380 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|      35 |  381 | `					pScan++;` |
|      34 |  382 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|      19 |  383 | `						&& pScan->sData.nByte == 1` |
|       5 |  384 | `						&& pScan->sData.zString[0] == '?' ){` |
|       5 |  385 | `						pScan++;` |
|       2 |  386 | `					}` |
|      34 |  387 | `					if( pScan < pEnd` |
|      35 |  388 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|      31 |  389 | `						pScan++;` |
|      15 |  390 | `					}` |
|      17 |  391 | `				}` |
|     197 |  392 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|     191 |  393 | `					pScan++; /* past '=>' */` |
|      93 |  394 | `				}` |
|     197 |  395 | `				pInnerBodyEnd = pScan;` |
|     197 |  396 | `				iNestInner = 0;` |
|    1369 |  397 | `				while( pInnerBodyEnd < pEnd ){` |
|    1317 |  398 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|       - |  399 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|       - |  400 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|     145 |  401 | `						break;` |
|       - |  402 | `					}` |
|    1177 |  403 | `					if( pInnerBodyEnd->nType &` |
|       - |  404 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     143 |  405 | `						iNestInner++;` |
|    1108 |  406 | `					}else if( pInnerBodyEnd->nType &` |
|       - |  407 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     143 |  408 | `						iNestInner--;` |
|      69 |  409 | `					}` |
|    1177 |  410 | `					pInnerBodyEnd++;` |
|       5 |  411 | `				}` |
|       - |  412 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|       - |  413 | `				 * the outer's body: a default value is evaluated at call time` |
|       - |  414 | `				 * in the outer frame, so any free variable it references is` |
|       - |  415 | `				 * an outer capture. We must NOT scan the parameter-name` |
|       - |  416 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|       - |  417 | `				 * or those names leak into the outer's closure environment.` |
|       - |  418 | `				 *` |
|       - |  419 | `				 * Walk the signature argument-by-argument, splitting on` |
|       - |  420 | `				 * top-level commas, and for each argument scan only the token` |
|       - |  421 | `				 * range after the '=' sign. */` |
|       - |  422 | `				{` |
|     197 |  423 | `					SyToken *pArgStart = pInnerSigStart;` |
|     373 |  424 | `					while( pArgStart < pInnerSigEnd ){` |
|     181 |  425 | `						SyToken *pArgEnd = pArgStart;` |
|     181 |  426 | `						SyToken *pEq = 0;` |
|     181 |  427 | `						int iNestArg = 0;` |
|     547 |  428 | `						while( pArgEnd < pInnerSigEnd ){` |
|     386 |  429 | `							if( iNestArg == 0` |
|     391 |  430 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|      22 |  431 | `								break;` |
|       - |  432 | `							}` |
|     371 |  433 | `							if( pArgEnd->nType &` |
|       - |  434 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  435 | `								iNestArg++;` |
|     371 |  436 | `							}else if( pArgEnd->nType &` |
|       - |  437 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  438 | `								iNestArg--;` |
|     ! 0 |  439 | `							}` |
|     366 |  440 | `							if( pEq == 0 && iNestArg == 0` |
|     365 |  441 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|       7 |  442 | `								pEq = pArgEnd;` |
|       3 |  443 | `							}` |
|     371 |  444 | `							pArgEnd++;` |
|       5 |  445 | `						}` |
|     181 |  446 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|      10 |  447 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       3 |  448 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|       7 |  449 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  450 | `								return SXERR_ABORT;` |
|       - |  451 | `							}` |
|       3 |  452 | `						}` |
|     181 |  453 | `						pArgStart = pArgEnd;` |
|     176 |  454 | `						if( pArgStart < pInnerSigEnd` |
|     103 |  455 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|      22 |  456 | `							pArgStart++;` |
|      10 |  457 | `						}` |
|       5 |  458 | `					}` |
|       - |  459 | `				}` |
|     293 |  460 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|      96 |  461 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|     197 |  462 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  463 | `					return SXERR_ABORT;` |
|       - |  464 | `				}` |
|     197 |  465 | `				pScan = pInnerBodyEnd;` |
|     197 |  466 | `				continue;` |
|       - |  467 | `			}` |
|     204 |  468 | `		}` |
|   69417 |  469 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|   64212 |  470 | `			pScan++;` |
|   64212 |  471 | `			continue;` |
|       - |  472 | `		}` |
|       - |  473 | `		{` |
|       - |  474 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|    5210 |  475 | `			SyToken *pDollar = pScan;` |
|    7795 |  476 | `			while( &pDollar[1] < pEnd` |
|    5210 |  477 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|     ! 0 |  478 | `				pDollar++;` |
|     ! 0 |  479 | `			}` |
|    5210 |  480 | `			if( &pDollar[1] >= pEnd ){` |
|     ! 0 |  481 | `				break;` |
|       - |  482 | `			}` |
|    5210 |  483 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  484 | `				pScan = pDollar + 1;` |
|     ! 0 |  485 | `				continue;` |
|       - |  486 | `			}` |
|    7800 |  487 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|    5205 |  488 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|    2590 |  489 | `				aShadow,nShadow);` |
|    5210 |  490 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  491 | `				return SXERR_ABORT;` |
|       - |  492 | `			}` |
|    5210 |  493 | `			pScan = pDollar + 2;` |
|       - |  494 | `		}` |
|       5 |  495 | `	}` |
|    8697 |  496 | `	return SXRET_OK;` |
|    4252 |  497 | `}` |
|       - |  498 | `/*` |
|       - |  499 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|       - |  500 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|       - |  501 | ` * variables by value. The body is a single expression that acts as an` |
|       - |  502 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|       - |  503 | ` * $this is also made available.` |
|       - |  504 | ` */` |
|    8500 |  505 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  506 | `{` |
|       - |  507 | `	ph7_vm_func *pFunc;` |
|       - |  508 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  509 | `	GenBlock *pBlock;` |
|       - |  510 | `	SySet *pInstrContainer;` |
|       - |  511 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|       - |  512 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|       - |  513 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|       - |  514 | `	SyToken *pSavedEnd;` |
|       - |  515 | `	ph7_vm_func_arg *aArgs;` |
|       - |  516 | `	char zName[512];` |
|       - |  517 | `	static int iCnt = 1;` |
|       - |  518 | `	char *zDup;` |
|       - |  519 | `	SyToken *pTokKw;` |
|       - |  520 | `	sxu32 nLen;` |
|       - |  521 | `	sxu32 nLine;` |
|    8505 |  522 | `	sxi32 iFlags = 0;` |
|    8505 |  523 | `	int bStatic = 0;` |
|       - |  524 | `	sxi32 rc;` |
|       - |  525 | `	sxu32 n;` |
|    4151 |  526 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  527 |  |
|    8505 |  528 | `	nLine = pGen->pIn->nLine;` |
|       - |  529 | ``	/* Attribute-sidecar key: `#[A] [static] fn` trivia is keyed to this token */`` |
|    8505 |  530 | `	pTokKw = pGen->pIn;` |
|       - |  531 | `	/* Optional 'static' prefix */` |
|    8500 |  532 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|    8505 |  533 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|     155 |  534 | `		bStatic = 1;` |
|     155 |  535 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|     155 |  536 | `		pGen->pIn++;` |
|      75 |  537 | `	}` |
|       - |  538 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|    8500 |  539 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    8505 |  540 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|     ! 0 |  541 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  542 | `			"Arrow function: expected 'fn' keyword");` |
|     ! 0 |  543 | `		return SXERR_SYNTAX;` |
|       - |  544 | `	}` |
|    8505 |  545 | `	pGen->pIn++; /* Jump 'fn' */` |
|       - |  546 | `	/* Optional '&' — return by reference */` |
|    8505 |  547 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  548 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|     ! 0 |  549 | `		pGen->pIn++;` |
|     ! 0 |  550 | `	}` |
|       - |  551 | `	/* Expect '(' */` |
|    8505 |  552 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  553 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  554 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  555 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|       2 |  556 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  557 | `		}else{` |
|     ! 0 |  558 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  559 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|       - |  560 | `		}` |
|       3 |  561 | `		return SXERR_SYNTAX;` |
|       - |  562 | `	}` |
|    8503 |  563 | `	pGen->pIn++; /* Jump '(' */` |
|       - |  564 | `	/* Delimit the parameter list */` |
|    8503 |  565 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|    8503 |  566 | `	if( pSigEnd >= pGen->pEnd ){` |
|       3 |  567 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  568 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       3 |  569 | `		return SXERR_SYNTAX;` |
|       - |  570 | `	}` |
|       - |  571 | `	/* Allocate the function state */` |
|    8501 |  572 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|    8501 |  573 | `	if( pFunc == 0 ){` |
|     ! 0 |  574 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  575 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  576 | `		return SXERR_ABORT;` |
|       - |  577 | `	}` |
|       - |  578 | `	/* Generate a unique lambda name */` |
|    8501 |  579 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|    8577 |  580 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|      79 |  581 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       3 |  582 | `	}` |
|    8501 |  583 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|    8501 |  584 | `	if( zDup == 0 ){` |
|     ! 0 |  585 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  586 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  587 | `		return SXERR_ABORT;` |
|       - |  588 | `	}` |
|    8501 |  589 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|       - |  590 | `	/* Reflection getStartLine(): line of the ['static'] 'fn' keyword */` |
|    8501 |  591 | `	pFunc->nLine = nLine;` |
|       - |  592 | `	/* php's visible name — an arrow function is named exactly like a closure. This` |
|       - |  593 | `	 * compiler builds its own function state, so it consumes the pending name itself. */` |
|    8501 |  594 | `	GenStateClosureName(&(*pGen),nLine);` |
|    8501 |  595 | `	pFunc->sClosureName = pGen->sPendingClosureName;` |
|    8501 |  596 | `	pFunc->sClosureScope = pGen->sPendingClosureScope;` |
|    8501 |  597 | `	SyStringInitFromBuf(&pGen->sPendingClosureName,0,0);` |
|       - |  598 | ``	/* Expression-position attributes (`$f = #[A] fn () => …`) */`` |
|    8501 |  599 | `	if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pFunc->aAttrs) == SXERR_ABORT ){` |
|     ! 0 |  600 | `		return SXERR_ABORT;` |
|       - |  601 | `	}` |
|    8501 |  602 | `	if( GenStateCheckAttrPlacement(&(*pGen),&pFunc->aAttrs,2,2,0,0) == SXERR_ABORT ){` |
|     ! 0 |  603 | `		return SXERR_ABORT;` |
|       - |  604 | `	}` |
|       - |  605 | `	/* An arrow function is a closure: its signature is exempt from the` |
|       - |  606 | ``	 * scope-keyword screen, exactly as `function () {}`'s is (see iSigScope). */`` |
|       - |  607 | `	{` |
|    8501 |  608 | `	int iSavedSig = pGen->iSigScope;` |
|    8501 |  609 | `	pGen->iSigScope = PH7_SIGSCOPE_CLOSURE;` |
|       - |  610 | `	/* Collect function arguments */` |
|    8501 |  611 | `	if( pGen->pIn < pSigEnd ){` |
|     898 |  612 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|     898 |  613 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  614 | `			pGen->iSigScope = iSavedSig;` |
|     ! 0 |  615 | `			return SXERR_ABORT;` |
|       - |  616 | `		}` |
|     445 |  617 | `	}` |
|       - |  618 | `	/* Point past ')' and parse optional return type */` |
|    8501 |  619 | `	pGen->pIn = &pSigEnd[1];` |
|    8501 |  620 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|    8501 |  621 | `	pGen->iSigScope = iSavedSig;` |
|       - |  622 | `	}` |
|    8501 |  623 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  624 | `		return SXERR_ABORT;` |
|    8501 |  625 | `	}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  626 | `		return SXERR_SYNTAX;` |
|       - |  627 | `	}` |
|    8501 |  628 | `	if( GenStateApplyNoDiscard(&(*pGen),pFunc,0,0) == SXERR_ABORT ){` |
|     ! 0 |  629 | `		return SXERR_ABORT;` |
|       - |  630 | `	}` |
|       - |  631 | `	/* Expect '=>' */` |
|    8501 |  632 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  633 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  634 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  635 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|       2 |  636 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  637 | `		}else{` |
|     ! 0 |  638 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  639 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|       - |  640 | `		}` |
|       3 |  641 | `		return SXERR_SYNTAX;` |
|       - |  642 | `	}` |
|    8499 |  643 | `	pGen->pIn++; /* Jump '=>' */` |
|    8499 |  644 | `	pBodyStart = pGen->pIn;` |
|    8499 |  645 | `	pBodyEnd = pGen->pEnd;` |
|       - |  646 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|       - |  647 | `	 * recursively collect free-variable references from the body. The scan` |
|       - |  648 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|       - |  649 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|    8499 |  650 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       - |  651 | `	{` |
|    8499 |  652 | `		SyString *aShadow = 0;` |
|    8499 |  653 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|    8499 |  654 | `		if( nShadow > 0 ){` |
|     896 |  655 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|     891 |  656 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|     896 |  657 | `			if( aShadow == 0 ){` |
|     ! 0 |  658 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  659 | `					"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  660 | `				return SXERR_ABORT;` |
|       - |  661 | `			}` |
|    2006 |  662 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|    1115 |  663 | `				aShadow[n] = aArgs[n].sName;` |
|     558 |  664 | `			}` |
|     444 |  665 | `		}` |
|   12647 |  666 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|    4148 |  667 | `			aShadow,nShadow);` |
|    8499 |  668 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  669 | `			return SXERR_ABORT;` |
|       - |  670 | `		}` |
|       - |  671 | `	}` |
|       - |  672 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|       - |  673 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|       - |  674 | `	 * captured value is silently dropped when the enclosing scope has no` |
|       - |  675 | `	 * $this. */` |
|    8499 |  676 | `	if( !bStatic ){` |
|       - |  677 | `		char *zThisDup;` |
|    8349 |  678 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|    8349 |  679 | `		if( zThisDup == 0 ){` |
|     ! 0 |  680 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  681 | `				"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  682 | `			return SXERR_ABORT;` |
|       - |  683 | `		}` |
|    8349 |  684 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|    8349 |  685 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|    8349 |  686 | `		sEnv.nIdx = SXU32_HIGH;` |
|    8349 |  687 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|    8349 |  688 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|    8349 |  689 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|    4073 |  690 | `	}` |
|       - |  691 | `	/* Arrow functions are always closures; the ARROW mark tells OP_LOAD_CLOSURE` |
|       - |  692 | `	 * these captures are implicit (auto-scanned) so an undefined one stays silent` |
|       - |  693 | `	 * at creation — php only warns when the body reads it. */` |
|    8499 |  694 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE \| VM_FUNC_ARROW;` |
|       - |  695 | `	/* Compile the body expression as an implicit return */` |
|   12647 |  696 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|    4148 |  697 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|    8499 |  698 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  699 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  700 | `			"PH7 engine is running out-of-memory");` |
|     ! 0 |  701 | `		return SXERR_ABORT;` |
|       - |  702 | `	}` |
|    8499 |  703 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    8499 |  704 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|    8499 |  705 | `	pSavedEnd = pGen->pEnd;` |
|    8499 |  706 | `	pGen->pIn = pBodyStart;` |
|    8499 |  707 | `	pGen->pEnd = pBodyEnd;` |
|       - |  708 | ``	/* The body is an implicit `return <expr>`, which READS its operands. Compile`` |
|       - |  709 | `	 * it read-only (like echo / string interpolation) so a lone undefined variable` |
|       - |  710 | ``	 * — e.g. `fn()=>$z` for an auto-capture that was undefined at creation and so`` |
|       - |  711 | `	 * never captured (see VmExecOpLoadClosure) — raises php's "Undefined variable"` |
|       - |  712 | `	 * warning at the read instead of being loaded quietly as a plain expression` |
|       - |  713 | ``	 * statement (`$z;`, silent in both engines) would be. */`` |
|    8499 |  714 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|    8499 |  715 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  716 | `		return SXERR_ABORT;` |
|       - |  717 | `	}` |
|       - |  718 | `	/* The cursor stopped just past the body expression */` |
|    8499 |  719 | `	pFunc->nEndLine = (pGen->pIn > pBodyStart) ? pGen->pIn[-1].nLine : nLine;` |
|       - |  720 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|       - |  721 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|       - |  722 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|       - |  723 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|    8499 |  724 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|    8499 |  725 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|    8499 |  726 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|    8499 |  727 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    8499 |  728 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  729 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|    8499 |  730 | `	pGen->pIn = pBodyEnd;` |
|    8499 |  731 | `	pGen->pEnd = pSavedEnd;` |
|       - |  732 | ``	/* An arrow body is one expression, and php takes a `yield` in it: calling`` |
|       - |  733 | ``	 * `fn() => yield 7` hands back a Generator, exactly as the `function` form`` |
|       - |  734 | `	 * does. Only the closure/function path scanned for the opcode, so the arrow's` |
|       - |  735 | ``	 * yield ran with no generator frame around it and raised `Cannot use yield`` |
|       - |  736 | ``	 * outside of a generator`. Same scan, same definition-time return-type screen. */`` |
|       - |  737 | `	{` |
|    8499 |  738 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - |  739 | `		sxu32 i;` |
|   91039 |  740 | `		for( i = 0 ; i < SySetUsed(&pFunc->aByteCode) ; i++ ){` |
|   82565 |  741 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|      21 |  742 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      21 |  743 | `				break;` |
|       - |  744 | `			}` |
|   40357 |  745 | `		}` |
|       - |  746 | `	}` |
|    8499 |  747 | `	if( pFunc->iFlags & VM_FUNC_GENERATOR ){` |
|      21 |  748 | `		if( SXERR_ABORT == GenStateValidateGeneratorReturnType(&(*pGen),pFunc) ){` |
|     ! 0 |  749 | `			return SXERR_ABORT;` |
|       - |  750 | `		}` |
|      10 |  751 | `	}` |
|       - |  752 | `	/* Emit the load-closure instruction */` |
|    8499 |  753 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|    8499 |  754 | `	return SXRET_OK;` |
|    4156 |  755 | `}` |
|       - |  756 | `/*` |
|       - |  757 | ` * Compile a single arm's expression range into a freshly-allocated` |
|       - |  758 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|       - |  759 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|       - |  760 | ` * expression's value.` |
|       - |  761 | ` */` |
|     660 |  762 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|       - |  763 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|       4 |  764 | `{` |
|       - |  765 | `	SySet *pInstrContainer;` |
|       - |  766 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  767 | `	GenBlock *pArmBlock;` |
|       - |  768 | `	sxi32 rc;` |
|     664 |  769 | `	pTmpIn  = pGen->pIn;` |
|     664 |  770 | `	pTmpEnd = pGen->pEnd;` |
|     664 |  771 | `	pGen->pIn  = pStart;` |
|     664 |  772 | `	pGen->pEnd = pStop;` |
|     664 |  773 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     664 |  774 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|       - |  775 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|       - |  776 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|       - |  777 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|       - |  778 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|       - |  779 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|     994 |  780 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|     330 |  781 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|     664 |  782 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  783 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  784 | `		pGen->pIn  = pTmpIn;` |
|     ! 0 |  785 | `		pGen->pEnd = pTmpEnd;` |
|     ! 0 |  786 | `		return SXERR_ABORT;` |
|       - |  787 | `	}` |
|     664 |  788 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     664 |  789 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     664 |  790 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     664 |  791 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     664 |  792 | `	GenStateLeaveBlock(&(*pGen),0);` |
|     664 |  793 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     664 |  794 | `	pGen->pIn  = pTmpIn;` |
|     664 |  795 | `	pGen->pEnd = pTmpEnd;` |
|     664 |  796 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  797 | `		return SXERR_ABORT;` |
|       - |  798 | `	}` |
|     664 |  799 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  800 | `		return SXERR_EMPTY;` |
|       - |  801 | `	}` |
|     664 |  802 | `	return SXRET_OK;` |
|     334 |  803 | `}` |
|       - |  804 | `/*` |
|       - |  805 | ` * Compile a PHP 8.0 match expression:` |
|       - |  806 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|       - |  807 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|       - |  808 | ` * Strict comparison (===) is used between the subject and each condition.` |
|       - |  809 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|       - |  810 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|       - |  811 | ` */` |
|       - |  812 | `/*` |
|       - |  813 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|       - |  814 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|       - |  815 | ` * caller can bail out of the current expression.` |
|       - |  816 | ` */` |
|     ! 0 |  817 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|     ! 0 |  818 | `{` |
|       - |  819 | `	va_list ap;` |
|       - |  820 | `	sxi32 rc;` |
|       - |  821 | `	SyBlob sMsg;` |
|     ! 0 |  822 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|     ! 0 |  823 | `	va_start(ap,zFmt);` |
|     ! 0 |  824 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|     ! 0 |  825 | `	va_end(ap);` |
|     ! 0 |  826 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|     ! 0 |  827 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|     ! 0 |  828 | `	SyBlobRelease(&sMsg);` |
|     ! 0 |  829 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  830 | `		return SXERR_ABORT;` |
|       - |  831 | `	}` |
|     ! 0 |  832 | `	return SXERR_SYNTAX;` |
|     ! 0 |  833 | `}` |
|       - |  834 | `/*` |
|       - |  835 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|       - |  836 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|       - |  837 | ` * Returns the stop token pointer (or pEnd if none found).` |
|       - |  838 | ` */` |
|     668 |  839 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|       5 |  840 | `{` |
|     673 |  841 | `	SyToken *pCur = pStart;` |
|     673 |  842 | `	int iNest = 0;` |
|    1955 |  843 | `	while( pCur < pEnd ){` |
|    1855 |  844 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     107 |  845 | `			iNest++;` |
|    1803 |  846 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     107 |  847 | `			iNest--;` |
|    1699 |  848 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|     573 |  849 | `			return pCur;` |
|       - |  850 | `		}` |
|    1287 |  851 | `		pCur++;` |
|       5 |  852 | `	}` |
|     105 |  853 | `	return pEnd;` |
|     339 |  854 | `}` |
|     156 |  855 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  856 | `{` |
|       - |  857 | `	ph7_match *pMatch;` |
|       - |  858 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|     161 |  859 | `	int bHasDefault = 0;` |
|       - |  860 | `	sxu32 nLine;` |
|       - |  861 | `	sxi32 rc;` |
|      78 |  862 | `	SXUNUSED(iCompileFlag);` |
|     161 |  863 | `	nLine = pGen->pIn->nLine;` |
|     161 |  864 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|       - |  865 | `	/* Expect '(' */` |
|     161 |  866 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  867 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  868 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|     ! 0 |  869 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|       - |  870 | `	}` |
|     161 |  871 | `	pGen->pIn++; /* Jump '(' */` |
|     161 |  872 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|     161 |  873 | `	if( pSubjEnd >= pGen->pEnd ){` |
|     ! 0 |  874 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  875 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       - |  876 | `	}` |
|     161 |  877 | `	if( pGen->pIn >= pSubjEnd ){` |
|     ! 0 |  878 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  879 | `			"syntax error, unexpected \")\", expecting match subject");` |
|       - |  880 | `	}` |
|       - |  881 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|     161 |  882 | `	pSavedEnd = pGen->pEnd;` |
|     161 |  883 | `	pGen->pEnd = pSubjEnd;` |
|     161 |  884 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     161 |  885 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  886 | `		return SXERR_ABORT;` |
|       - |  887 | `	}` |
|     161 |  888 | `	pGen->pEnd = pSavedEnd;` |
|     161 |  889 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|       - |  890 | `	/* Expect '{' */` |
|     161 |  891 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  892 | `		return GenStateMatchError(pGen,` |
|     ! 0 |  893 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  894 | `			"syntax error, expecting \"{\" after match subject");` |
|       - |  895 | `	}` |
|     161 |  896 | `	pGen->pIn++; /* Jump '{' */` |
|     161 |  897 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|     161 |  898 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  899 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  900 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|       - |  901 | `	}` |
|       - |  902 | `	/* Allocate ph7_match container */` |
|     161 |  903 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|     161 |  904 | `	if( pMatch == 0 ){` |
|     ! 0 |  905 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  906 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  907 | `		return SXERR_ABORT;` |
|       - |  908 | `	}` |
|     161 |  909 | `	SyZero(pMatch,sizeof(ph7_match));` |
|     161 |  910 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|       - |  911 | `	/* Iterate arms */` |
|     509 |  912 | `	while( pGen->pIn < pBodyEnd ){` |
|       - |  913 | `		ph7_match_arm sArm;` |
|       - |  914 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|     363 |  915 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|     363 |  916 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|     363 |  917 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|     363 |  918 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  919 | `		/* 'default' arm? */` |
|     358 |  920 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     212 |  921 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|      60 |  922 | `			if( bHasDefault ){` |
|       3 |  923 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|       - |  924 | `					"Match expressions may only contain one default arm");` |
|       7 |  925 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  926 | `			}` |
|      58 |  927 | `			sArm.bDefault = 1;` |
|      58 |  928 | `			bHasDefault = 1;` |
|      58 |  929 | `			pGen->pIn++;` |
|       - |  930 | ``			/* php's `default possible_comma =>`: one trailing comma is allowed */`` |
|      58 |  931 | `			if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 |  932 | `				pGen->pIn++;` |
|       1 |  933 | `			}` |
|      58 |  934 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|     ! 0 |  935 | `				rc = PH7_GenSyntaxError(pGen,pGen->pIn,"\"=>\"");` |
|     ! 0 |  936 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  937 | `			}` |
|      58 |  938 | `			pGen->pIn++; /* Jump '=>' */` |
|      31 |  939 | `		}else{` |
|       - |  940 | `			/* Condition list: cond (',' cond)* [','] '=>'. The comma before the` |
|       - |  941 | ``			 * arrow is php's `possible_comma` -- `'a', 'b', => …` is how`` |
|       - |  942 | `			 * symfony/cache's RedisTrait lays a long list out one per line, and` |
|       - |  943 | `			 * it used to be refused here as an empty condition. */` |
|     307 |  944 | `			pCondStart = pGen->pIn;` |
|     307 |  945 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|       - |  946 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|     325 |  947 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|       - |  948 | `				SySet sCondBc;` |
|      42 |  949 | `				if( pCondStart >= pArrow ){` |
|       - |  950 | `					/* An arm opening on ',' is where php expects the closing` |
|       - |  951 | `					 * brace; one that runs ',' ',' is missing its arrow. */` |
|       8 |  952 | `					rc = PH7_GenSyntaxError(pGen,pArrow,` |
|       4 |  953 | `						pCondStart == pGen->pIn ? "\"}\"" : "\"=>\"");` |
|       6 |  954 | `					return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  955 | `				}` |
|      37 |  956 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      37 |  957 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|      37 |  958 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  959 | `					return SXERR_ABORT;` |
|       - |  960 | `				}` |
|      37 |  961 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|      37 |  962 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|      37 |  963 | `				if( pCondStart < pBodyEnd && (pCondStart->nType & PH7_TK_ARRAY_OP) ){` |
|      17 |  964 | ``					pArrow = pCondStart; /* the trailing comma: `cond, =>` */`` |
|      17 |  965 | `					break;` |
|       - |  966 | `				}` |
|      21 |  967 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|       - |  968 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|       3 |  969 | `			}` |
|     303 |  970 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  971 | `				rc = PH7_GenSyntaxError(pGen,pArrow,"\"=>\"");` |
|       3 |  972 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  973 | `			}` |
|     301 |  974 | `			if( pCondStart >= pArrow && SySetUsed(&sArm.aConds) == 0 ){` |
|       3 |  975 | ``				rc = PH7_GenSyntaxError(pGen,pArrow,"\"}\""); /* `{ => …`: no condition at all */`` |
|       3 |  976 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  977 | `			}` |
|     298 |  978 | `			if( pCondStart < pArrow ){` |
|       - |  979 | `				SySet sCondBc;` |
|     282 |  980 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     282 |  981 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|     282 |  982 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  983 | `					return SXERR_ABORT;` |
|       - |  984 | `				}` |
|     282 |  985 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|     139 |  986 | `			}` |
|     298 |  987 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|       - |  988 | `		}` |
|       - |  989 | `		/* Compile result expression: up to top-level ',' or body end */` |
|     352 |  990 | `		pResStart = pGen->pIn;` |
|     352 |  991 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|     352 |  992 | `		if( pResStart >= pResEnd ){` |
|       - |  993 | `			/* php names the token that stands where the result should: the ','` |
|       - |  994 | `			 * of the next arm, or the body's '}' */` |
|     ! 0 |  995 | `			rc = PH7_GenSyntaxError(pGen,pResEnd,0);` |
|     ! 0 |  996 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  997 | `		}` |
|     352 |  998 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|     352 |  999 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1000 | `			return SXERR_ABORT;` |
|       - | 1001 | `		}` |
|     352 | 1002 | `		pGen->pIn = pResEnd;` |
|     352 | 1003 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|     254 | 1004 | `			pGen->pIn++; /* Skip trailing ',' */` |
|     125 | 1005 | `		}` |
|     352 | 1006 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|       4 | 1007 | `	}` |
|     150 | 1008 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|     150 | 1009 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|     150 | 1010 | `	return SXRET_OK;` |
|      83 | 1011 | `}` |
|       - | 1012 | `/*` |
|       - | 1013 | ` * Compile a backtick quoted string.` |
|       - | 1014 | ` */` |
|       2 | 1015 | `PH7_PRIVATE sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 | 1016 | `{` |
|       1 | 1017 | `	SXUNUSED(iCompileFlag);` |
|       - | 1018 | `	/*` |
|       - | 1019 | ``	 * The backtick (`) operator was DEPRECATED by php (shell_exec() is the replacement).`` |
|       - | 1020 | `	 * PHL targets php's *non-deprecated* surface, so it is a hard parse error — never` |
|       - | 1021 | `	 * compiled to a shell_exec() call.` |
|       - | 1022 | `	 */` |
|       3 | 1023 | `	PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - | 1024 | ``		"syntax error, the backtick (`) operator was removed, use shell_exec() instead");`` |
|       3 | 1025 | `	return SXERR_ABORT;` |
|       1 | 1026 | `}` |
|       - | 1027 | `/*` |
|       - | 1028 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - | 1029 | ` * construct.` |
|       - | 1030 | ` */` |
|     370 | 1031 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 | 1032 | `{` |
|       - | 1033 | `	SyString *pName;` |
|       - | 1034 | `	sxu32 nKeyID;` |
|       - | 1035 | `	sxi32 rc;` |
|       - | 1036 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|     375 | 1037 | `	pName = &pGen->pIn->sData;` |
|     375 | 1038 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     375 | 1039 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|     375 | 1040 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       6 | 1041 | `		SyToken *pTmp,*pNext = 0;` |
|       - | 1042 | ``		/* A STATEMENT `echo` never reaches here — it dispatches through the statement`` |
|       - | 1043 | `		 * table. Arriving in expression position means source like` |
|       - | 1044 | ``		 * `fopen('f','r') or echo "IO error";`, which was a Symisc extension and is a`` |
|       - | 1045 | `		 * php parse error (the scope policy: a PH7-ism that changes the meaning of valid source is a` |
|       - | 1046 | ``		 * bug). The one legitimate expression-echo is the token a `<?= ... ?>` short tag`` |
|       - | 1047 | `		 * synthesizes, which raises nExprEchoOk around its own compile. */` |
|       6 | 1048 | `		if( pGen->nExprEchoOk < 1 ){` |
|       3 | 1049 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn - 1,0);` |
|       3 | 1050 | `			return SXERR_ABORT;` |
|       - | 1051 | `		}` |
|       - | 1052 | `		/* Compile arguments one after one */` |
|       3 | 1053 | `		pTmp = pGen->pEnd;` |
|       3 | 1054 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|       5 | 1055 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       3 | 1056 | `			if( pGen->pIn < pNext ){` |
|       3 | 1057 | `				pGen->pEnd = pNext;` |
|       3 | 1058 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|       3 | 1059 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1060 | `					return SXERR_ABORT;` |
|       - | 1061 | `				}` |
|       3 | 1062 | `				if( rc != SXERR_EMPTY ){` |
|       - | 1063 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|       - | 1064 | `					 * without the overhead of a function call.` |
|       - | 1065 | `					 * This is a very powerful optimization that improve` |
|       - | 1066 | `					 * performance greatly.` |
|       - | 1067 | `					 */` |
|       3 | 1068 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|       1 | 1069 | `				}` |
|       1 | 1070 | `			}` |
|       - | 1071 | `			/* Jump trailing commas */` |
|       3 | 1072 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     ! 0 | 1073 | `				pNext++;` |
|     ! 0 | 1074 | `			}` |
|       3 | 1075 | `			pGen->pIn = pNext;` |
|       1 | 1076 | `		}` |
|       - | 1077 | `		/* Restore token stream */` |
|       3 | 1078 | `		pGen->pEnd = pTmp;` |
|       2 | 1079 | `	}else{` |
|     371 | 1080 | `		sxi32 nArg = 0;` |
|     371 | 1081 | `		sxu32 nIdx = 0;` |
|       - | 1082 | `		char zCanon[sizeof("include_once")-1];` |
|       - | 1083 | `		SyString sCanon;` |
|     371 | 1084 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|     371 | 1085 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1086 | `			return SXERR_ABORT;` |
|     371 | 1087 | `		}else if(rc != SXERR_EMPTY ){` |
|     371 | 1088 | `			nArg = 1;` |
|     183 | 1089 | `		}` |
|       - | 1090 | `		/* The construct is dispatched as a CALL to the host function of the same name,` |
|       - | 1091 | `		 * so the name emitted here must be the construct's canonical spelling, not the` |
|       - | 1092 | ``		 * source's: php accepts `PRINT`/`Isset`/`EVAL` (keywords are case-insensitive)`` |
|       - | 1093 | ``		 * where the raw text produced `Call to undefined function PRINT()`. Every`` |
|       - | 1094 | `		 * construct name is lower-case ASCII, so folding IS canonicalising. */` |
|     371 | 1095 | `		if( pName->nByte <= sizeof(zCanon) ){` |
|       - | 1096 | `			sxu32 i;` |
|    3185 | 1097 | `			for( i = 0 ; i < pName->nByte ; ++i ){` |
|    2819 | 1098 | `				unsigned char c = (unsigned char)pName->zString[i];` |
|    2819 | 1099 | `				zCanon[i] = (char)((c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c);` |
|    1412 | 1100 | `			}` |
|     371 | 1101 | `			SyStringInitFromBuf(&sCanon,zCanon,pName->nByte);` |
|     371 | 1102 | `			pName = &sCanon;` |
|     183 | 1103 | `		}` |
|     371 | 1104 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - | 1105 | `			ph7_value *pObj;` |
|       - | 1106 | `			/* Emit the call instruction */` |
|     205 | 1107 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     205 | 1108 | `			if( pObj == 0 ){` |
|     ! 0 | 1109 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1110 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1111 | `				return SXERR_ABORT;` |
|       - | 1112 | `			}` |
|     205 | 1113 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - | 1114 | `			/* Install in the literal table */` |
|     205 | 1115 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     100 | 1116 | `		}` |
|       - | 1117 | `		/* Emit the call instruction */` |
|     371 | 1118 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 1119 | ``		/* PH7_CALL_CONSTRUCT: `print`, `include`, `include_once`, `require` and`` |
|       - | 1120 | ``		 * `require_once` are dispatched as a host function of the same name, and php has`` |
|       - | 1121 | `		 * no such function -- the mark is what lets the hidden registration answer this` |
|       - | 1122 | `		 * site and nothing a script spells (PH7_VmGetHostFunction). */` |
|     554 | 1123 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,PH7_CALL_CONSTRUCT,` |
|     183 | 1124 | `			GenStateAttachStrictFlag(pGen,0),0);` |
|       - | 1125 | `	}` |
|       - | 1126 | `	/* Node successfully compiled */` |
|     373 | 1127 | `	return SXRET_OK;` |
|     190 | 1128 | `}` |
|       - | 1129 | `/*` |
|       - | 1130 | ` * Compile a node holding a variable declaration.` |
|       - | 1131 | ` * According to the PHP language reference` |
|       - | 1132 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - | 1133 | ` *  The variable name is case-sensitive.` |
|       - | 1134 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - | 1135 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - | 1136 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - | 1137 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - | 1138 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - | 1139 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - | 1140 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - | 1141 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - | 1142 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - | 1143 | ` *  the chapter on Expressions.` |
|       - | 1144 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - | 1145 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - | 1146 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - | 1147 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - | 1148 | ` *  is being assigned (the source variable).` |
|       - | 1149 | ` */` |
| 3539476 | 1150 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 | 1151 | `{` |
| 3539481 | 1152 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 1153 | `	sxi32 iVv;` |
|       - | 1154 | `	sxi32 iP1;` |
| 3539481 | 1155 | ``	sxi32 iP2 = 0; /* 1 = quiet read (isset/empty), 4 = quiet read (`??`): a missing`` |
|       - | 1156 | `	                * variable must not warn in either */` |
|       - | 1157 | `	void *p3;` |
|       - | 1158 | `	sxi32 rc;` |
| 3539481 | 1159 | `	iVv = -1; /* Variable variable counter */` |
| 7079007 | 1160 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
| 3539531 | 1161 | `		pGen->pIn++;` |
| 3539531 | 1162 | `		iVv++;` |
|       5 | 1163 | `	}` |
| 3539481 | 1164 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - | 1165 | `		/* Invalid variable name */` |
|     ! 0 | 1166 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable or \"{\" or \"$\"");` |
|     ! 0 | 1167 | `		if( rc == SXERR_ABORT ){` |
|       - | 1168 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1169 | `			return SXERR_ABORT;` |
|       - | 1170 | `		}` |
|     ! 0 | 1171 | `		return SXRET_OK;` |
|       - | 1172 | `	}` |
| 3539481 | 1173 | `	p3  = 0;` |
| 3539481 | 1174 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - | 1175 | `		/* Dynamic variable creation */` |
|      40 | 1176 | `		pGen->pIn++;  /* Jump the open curly */` |
|      40 | 1177 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      40 | 1178 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 1179 | `			/* Empty expression */` |
|       - | 1180 | `			{` |
|       - | 1181 | `			/* php names the offending token and, for an empty "${}", stops there:` |
|       - | 1182 | `			 * the "expecting" tail only appears when something could still follow. */` |
|       - | 1183 | ``			/* `${}`: pEnd was stepped back past the trailing '}', so the token php`` |
|       - | 1184 | `			 * names sits AT pEnd. Reach for it before deciding the tail -- php stops` |
|       - | 1185 | `			 * at "unexpected token \"}\"" with no "expecting" clause, which the` |
|       - | 1186 | `			 * NULL-token path could not express because it never saw the '}'. */` |
|       3 | 1187 | `			SyToken *pBad = pGen->pIn < pGen->pEnd ? pGen->pIn : 0;` |
|       3 | 1188 | `			if( pBad == 0 && pGen->pTokenSet ){` |
|       3 | 1189 | `				SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|       3 | 1190 | `				SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|       3 | 1191 | `				if( pGen->pEnd >= pBase && pGen->pEnd < pStreamEnd ){` |
|       3 | 1192 | `					pBad = pGen->pEnd;` |
|       1 | 1193 | `				}` |
|       1 | 1194 | `			}` |
|       5 | 1195 | `			PH7_GenSyntaxError(&(*pGen),pBad,` |
|       2 | 1196 | `				(pBad && (pBad->nType & PH7_TK_CCB)) ? 0 : "variable or \"{\" or \"$\"");` |
|       - | 1197 | `			}` |
|       3 | 1198 | `			return SXRET_OK;` |
|       - | 1199 | `		}` |
|       - | 1200 | `		/* Compile the expression holding the variable name. It is a pure READ, so` |
|       - | 1201 | ``		 * compile it read-only: `${$u}` warns on an undefined $u (php) instead of`` |
|       - | 1202 | ``		 * silently creating it, matching the `$$u` name-read path below. A quiet`` |
|       - | 1203 | `		 * outer (isset()/empty()) suppresses that name warning too, so carry the` |
|       - | 1204 | `		 * quiet flag into the name expression. */` |
|      37 | 1205 | `		sxi32 iNameFlags = EXPR_FLAG_RDONLY_LOAD;` |
|      37 | 1206 | `		if( iCompileFlag & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY) ){` |
|       - | 1207 | ``			/* isset()/empty() suppress the name warning; `??` (EXPR_FLAG_QUIET_VAR)`` |
|       - | 1208 | ``			 * does NOT — php warns `Undefined variable $u` for `${$u} ?? x` and only`` |
|       - | 1209 | `			 * quiets the TARGET read, so QUIET_VAR is deliberately excluded here. */` |
|       3 | 1210 | `			iNameFlags \|= EXPR_FLAG_QUIET_VAR;` |
|       1 | 1211 | `		}` |
|      37 | 1212 | `		rc = PH7_CompileExpr(&(*pGen),iNameFlags,0);` |
|      37 | 1213 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1214 | `			return SXERR_ABORT;` |
|      37 | 1215 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 | 1216 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,0);` |
|       3 | 1217 | `			return SXRET_OK;` |
|       - | 1218 | `		}` |
|      19 | 1219 | `	}else{` |
|       - | 1220 | `		SyHashEntry *pEntry;` |
|       - | 1221 | `		SyString *pName;` |
| 3539445 | 1222 | `		char *zName = 0;` |
|       - | 1223 | `		/* Extract variable name */` |
| 3539445 | 1224 | `		pName = &pGen->pIn->sData;` |
|       - | 1225 | `		/* Advance the stream cursor */` |
| 3539445 | 1226 | `		pGen->pIn++;` |
| 3539445 | 1227 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
| 3539445 | 1228 | `		if( pEntry == 0 ){` |
|       - | 1229 | `			/* Duplicate name */` |
|  508986 | 1230 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  508986 | 1231 | `			if( zName == 0 ){` |
|     ! 0 | 1232 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1233 | `				return SXERR_ABORT;` |
|       - | 1234 | `			}` |
|       - | 1235 | `			/* Install in the hashtable */` |
|  508986 | 1236 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|  254099 | 1237 | `		}else{` |
|       - | 1238 | `			/* Name already available */` |
| 3030464 | 1239 | `			zName = (char *)pEntry->pUserData;` |
|       - | 1240 | `		}` |
| 3539445 | 1241 | `		p3 = (void *)zName;` |
|       - | 1242 | `	}` |
| 3539477 | 1243 | `	iP1 = 0;` |
| 3539477 | 1244 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
| 3346997 | 1245 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - | 1246 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
| 2402988 | 1247 | `			iP1 = 1;` |
| 1199732 | 1248 | `		}` |
| 1671057 | 1249 | `	}` |
|       - | 1250 | ``	/* iP2 marks a QUIET read: `isset($x)` / `empty($x)` inspect a variable`` |
|       - | 1251 | `	 * without reading it, so an undefined one must not warn (php stays silent` |
|       - | 1252 | `	 * for both). Every other read of a missing variable warns — see OP_LOAD.` |
|       - | 1253 | `	 * The two flags are cleared before recursing into a subscript's index` |
|       - | 1254 | ``	 * expression, so `isset($a[$i])` still warns for an undefined $i, as php`` |
|       - | 1255 | ``	 * does. Contexts that VIVIFY (assignment targets, `??`, appends) already`` |
|       - | 1256 | `	 * emit iP1 = 0 and never reach the warning. */` |
| 3539477 | 1257 | `	if( iCompileFlag & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY) ){` |
|   17139 | 1258 | `		iP2 = 1;` |
| 3530899 | 1259 | `	}else if( iCompileFlag & EXPR_FLAG_QUIET_VAR ){` |
|       - | 1260 | ``		/* `??`'s quiet read is quiet for the same WARNING and not for the same`` |
|       - | 1261 | ``		 * rule: php swallows `Undefined variable $x` under `$x ?? d` but still`` |
|       - | 1262 | ``		 * throws `Using $this when not in object context` for `$this ?? d`, where`` |
|       - | 1263 | ``		 * `isset($this)` answers false in silence. One value each, so OP_LOAD can`` |
|       - | 1264 | `		 * tell the two silences apart. */` |
|     653 | 1265 | `		iP2 = 4;` |
| 3522019 | 1266 | `	}else if( iCompileFlag & EXPR_FLAG_RMW_LOAD ){` |
|       - | 1267 | ``		/* Warn-then-create: the read half of `$x++` / `$x .= ...` still needs a`` |
|       - | 1268 | `		 * writable slot, so it cannot use the read-only load above. */` |
|  102116 | 1269 | `		iP2 = 2;` |
| 3470568 | 1270 | `	}else if( iCompileFlag & EXPR_FLAG_DEFER_ARG ){` |
|       - | 1271 | ``		/* D1 deferred call argument. For a plain `$var` (iVv == 0, p3 holds the name)`` |
|       - | 1272 | `		 * emit the deferred load (iP1 stays 1 = no create, iP2 = 3): an undefined` |
|       - | 1273 | `		 * variable is left uncreated and silent, carrying a lazy-lvalue marker that` |
|       - | 1274 | `		 * OP_CALL resolves against the callee's by-ref flags. A variable-variable` |
|       - | 1275 | `		 * ($$x) computes its name on the stack (p3 == 0), so it cannot carry the` |
|       - | 1276 | `		 * marker — fall back to the historical eager create (iP1 = 0), which keeps` |
|       - | 1277 | `		 * its by-ref binding working exactly as before. */` |
|  809159 | 1278 | `		if( iVv == 0 ){` |
|  809153 | 1279 | `			iP2 = 3;` |
|  403957 | 1280 | `		}else{` |
|       8 | 1281 | `			iP1 = 0;` |
|       - | 1282 | `		}` |
|  403955 | 1283 | `	}` |
|       - | 1284 | `	/* Emit the load instruction(s). For a variable-variable ($$x, $$$x, ...) every` |
|       - | 1285 | `	 * load EXCEPT the final dereference resolves a NAME: a pure read that warns on an` |
|       - | 1286 | `	 * undefined name (php) and never creates it. Only the last load is the actual` |
|       - | 1287 | `	 * variable and carries the caller's write/create context (iP1). Emitting the` |
|       - | 1288 | ``	 * outer create-mode for the name loads silently invented $n in `$$n = 5` and`` |
|       - | 1289 | ``	 * skipped php's `Undefined variable $n` warning; a quiet outer (isset/empty)`` |
|       - | 1290 | `	 * still suppresses the name warning as php does. */` |
| 3539477 | 1291 | `	if( iVv > 0 ){` |
|      53 | 1292 | `		sxi32 iP2Name = (iCompileFlag & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY))` |
|       - | 1293 | ``			? 1 /* isset()/empty() suppress the name warning too; `??` (QUIET_VAR)`` |
|      24 | 1294 | `			     * does NOT — it warns the name and quiets only the target read. */ : 0;` |
|      53 | 1295 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,1/* read-only name */,iP2Name,p3,0);` |
|      55 | 1296 | `		while( iVv > 1 ){` |
|       3 | 1297 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,1/* read-only name */,iP2Name,0,0);` |
|       3 | 1298 | `			iVv--;` |
|       1 | 1299 | `		}` |
|       - | 1300 | `		/* Final dereference: the actual variable, in the caller's context. */` |
|      53 | 1301 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,iP2,0,0);` |
|      29 | 1302 | `	}else{` |
| 3539429 | 1303 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,iP2,p3,0);` |
|       - | 1304 | `	}` |
|       - | 1305 | `	/* Node successfully compiled */` |
| 3539477 | 1306 | `	return SXRET_OK;` |
| 1767152 | 1307 | `}` |
|       - | 1308 | `/*` |
|       - | 1309 | ` * Load a literal.` |
|       - | 1310 | ` */` |
| 1725688 | 1311 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       5 | 1312 | `{` |
| 1725693 | 1313 | `	SyToken *pToken = pGen->pIn;` |
|       - | 1314 | `	ph7_value *pObj;` |
|       - | 1315 | `	SyString *pStr;` |
|       - | 1316 | `	SyString sCanon;` |
|       - | 1317 | `	sxu32 nIdx;` |
|       - | 1318 | `	/* Extract token value */` |
| 1725693 | 1319 | `	pStr = &pToken->sData;` |
|       - | 1320 | `	/* php's MAGIC constants are case-insensitive like the rest of its reserved words —` |
|       - | 1321 | ``	 * `__line__`, `__Dir__` and `__CLASS__` are one constant each — but every one of them`` |
|       - | 1322 | `	 * is recognised by a BYTE-EXACT compare: the compile-time branches just below, and` |
|       - | 1323 | ``	 * `__CLASS__` through constant.c's (deliberately case-sensitive) constant table. Fold`` |
|       - | 1324 | `	 * the spelling to the canonical upper case here, once, so both mechanisms see it; any` |
|       - | 1325 | ``	 * other spelling used to reach the plain-literal path and raise `Undefined constant`` |
|       - | 1326 | ``	 * "__line__"`. Two of the branches below also read a single byte to tell a pair apart`` |
|       - | 1327 | ``	 * (`zString[2]` for __DIR__ vs __FILE__ and __METHOD__ vs __FUNCTION__), which only`` |
|       - | 1328 | `	 * works on the canonical form.` |
|       - | 1329 | `	 *` |
|       - | 1330 | `	 * User constants stay case-SENSITIVE (php) — this fold is limited to the eight names` |
|       - | 1331 | ``	 * below, and skips a member NAME, where `C::__LINE__` is an ordinary class constant. */`` |
| 1725688 | 1332 | `	if( (pToken->nType & PH7_TK_MEMBER_NAME) == 0 && pStr->nByte > 4` |
| 1574232 | 1333 | `		&& pStr->zString[0] == '_' && pStr->zString[1] == '_' ){` |
|       - | 1334 | `		static const char * const azMagic[] = {` |
|       - | 1335 | `			"__LINE__", "__FILE__", "__DIR__", "__FUNCTION__", "__CLASS__",` |
|       - | 1336 | `			"__METHOD__", "__NAMESPACE__", "__TRAIT__"` |
|       - | 1337 | `		};` |
|       - | 1338 | `		sxu32 i;` |
|  145007 | 1339 | `		for( i = 0 ; i < SX_ARRAYSIZE(azMagic) ; ++i ){` |
|  129129 | 1340 | `			sxu32 nMagic = SyStrlen(azMagic[i]);` |
|  129129 | 1341 | `			if( pStr->nByte == nMagic && SyStrnicmp(pStr->zString,azMagic[i],nMagic) == 0 ){` |
|     651 | 1342 | `				SyStringInitFromBuf(&sCanon,azMagic[i],nMagic);` |
|     651 | 1343 | `				pStr = &sCanon;` |
|     651 | 1344 | `				break;` |
|       - | 1345 | `			}` |
|   64155 | 1346 | `		}` |
|    8250 | 1347 | `	}` |
|       - | 1348 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first. A reserved` |
|       - | 1349 | `	 * word used as a member NAME (Enum::Null, C::Array, $o->list()) is a plain` |
|       - | 1350 | `	 * identifier — the parser flagged its token so the whole value-folding chain is` |
|       - | 1351 | `	 * skipped and it falls through to the ordinary string-literal emit below. */` |
| 1725693 | 1352 | `	if( pToken->nType & PH7_TK_MEMBER_NAME ){` |
|       - | 1353 | `		/* fall through to the plain-string literal path */` |
| 1707992 | 1354 | `	}else if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  113589 | 1355 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - | 1356 | `			/* NULL constant are always indexed at 0 */` |
|   20273 | 1357 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   20273 | 1358 | `			return SXRET_OK;` |
|   93321 | 1359 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - | 1360 | `			/* TRUE constant are always indexed at 1 */` |
|   16262 | 1361 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|   16262 | 1362 | `			return SXRET_OK;` |
|       5 | 1363 | `		}` |
| 1768679 | 1364 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|  306497 | 1365 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - | 1366 | `			/* FALSE constant are always indexed at 2 */` |
|  218493 | 1367 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|  218493 | 1368 | `			return SXRET_OK;` |
| 1358252 | 1369 | `	}else if( pStr->nByte == sizeof("__COMPILER_HALT_OFFSET__") - 1` |
|  677926 | 1370 | `		&& pGen->bHaltSeen` |
|     174 | 1371 | `		&& SyStrnicmp(pStr->zString,"__COMPILER_HALT_OFFSET__",` |
|       6 | 1372 | `			sizeof("__COMPILER_HALT_OFFSET__")-1) == 0 ){` |
|       - | 1373 | `			/*` |
|       - | 1374 | ``			 * php's `__COMPILER_HALT_OFFSET__`: the byte just past the`` |
|       - | 1375 | ``			 * `__halt_compiler();` statement's semicolon, resolved at COMPILE time`` |
|       - | 1376 | ``			 * and only in a file that HAS one. That is why `defined()` answers`` |
|       - | 1377 | `			 * false for it (php registers it under a mangled per-file name, and` |
|       - | 1378 | `			 * this emits a literal instead) and why a file with no halt reaches` |
|       - | 1379 | ``			 * the ordinary constant path and raises php's `Undefined constant`.`` |
|       - | 1380 | `			 */` |
|      13 | 1381 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      13 | 1382 | `			if( pObj == 0 ){` |
|     ! 0 | 1383 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1384 | `				return SXERR_ABORT;` |
|       - | 1385 | `			}` |
|      13 | 1386 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,(sxi64)pGen->nHaltOffset);` |
|      13 | 1387 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      13 | 1388 | `			return SXRET_OK;` |
| 1398167 | 1389 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   79680 | 1390 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - | 1391 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|      22 | 1392 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      22 | 1393 | `			if( pObj == 0 ){` |
|     ! 0 | 1394 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1395 | `				return SXERR_ABORT;` |
|       - | 1396 | `			}` |
|      22 | 1397 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - | 1398 | `			/* Emit the load constant instruction */` |
|      22 | 1399 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      22 | 1400 | `			return SXRET_OK;` |
| 1398132 | 1401 | `	}else if( (pStr->nByte == sizeof("__FILE__") - 1 &&` |
|  121582 | 1402 | `		SyMemcmp(pStr->zString,"__FILE__",sizeof("__FILE__")-1) == 0) \|\|` |
| 1399950 | 1403 | `		(pStr->nByte == sizeof("__DIR__") - 1 &&` |
|   84014 | 1404 | `		SyMemcmp(pStr->zString,"__DIR__",sizeof("__DIR__")-1) == 0) ){` |
|       - | 1405 | `			/* __FILE__ / __DIR__ are magic constants resolved at COMPILE time to the` |
|       - | 1406 | `			 * file being compiled (where the token is written), NOT the runtime` |
|       - | 1407 | `			 * execution file. A function defined in a.php reporting __FILE__ must say` |
|       - | 1408 | `			 * a.php even when called from b.php — php semantics, and what Composer's` |
|       - | 1409 | ``			 * autoloader (loadClassLoader's `require __DIR__ . '/ClassLoader.php'`)`` |
|       - | 1410 | `			 * relies on. The runtime-constant path returned the caller's file. */` |
|     495 | 1411 | `			int bDir = (pStr->zString[2] == 'D'); /* __DIR__ vs __FILE__ */` |
|     495 | 1412 | `			SyString *pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     495 | 1413 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     495 | 1414 | `			if( pObj == 0 ){` |
|     ! 0 | 1415 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1416 | `				return SXERR_ABORT;` |
|       - | 1417 | `			}` |
|     495 | 1418 | `			if( pFile && pFile->nByte > 0 ){` |
|     495 | 1419 | `				if( bDir ){` |
|       - | 1420 | `					const char *zDir;` |
|       - | 1421 | `					int nLen;` |
|       - | 1422 | `					SyString sDir;` |
|     251 | 1423 | `					zDir = PH7_ExtractDirName(pFile->zString,(int)pFile->nByte,&nLen);` |
|     251 | 1424 | `					SyStringInitFromBuf(&sDir,zDir,nLen);` |
|     251 | 1425 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&sDir);` |
|     128 | 1426 | `				}else{` |
|     249 | 1427 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,pFile);` |
|       - | 1428 | `				}` |
|     249 | 1429 | `			}else{` |
|       - | 1430 | `				SyString sMem;` |
|     ! 0 | 1431 | `				SyStringInitFromBuf(&sMem,":MEMORY:",sizeof(":MEMORY:")-1);` |
|     ! 0 | 1432 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sMem);` |
|       - | 1433 | `			}` |
|     495 | 1434 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     495 | 1435 | `			return SXRET_OK;` |
| 1383138 | 1436 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   50723 | 1437 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|       - | 1438 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|      17 | 1439 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      17 | 1440 | `			if( pObj == 0 ){` |
|     ! 0 | 1441 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1442 | `				return SXERR_ABORT;` |
|       - | 1443 | `			}` |
|      17 | 1444 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - | 1445 | `				SyString sNs;` |
|      11 | 1446 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      11 | 1447 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|       7 | 1448 | `			}else{` |
|       7 | 1449 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - | 1450 | `			}` |
|      17 | 1451 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      17 | 1452 | `			return SXRET_OK;` |
| 1389676 | 1453 | `	}else if( pStr->nByte == sizeof("__TRAIT__") - 1 &&` |
|   63804 | 1454 | `		SyMemcmp(pStr->zString,"__TRAIT__",sizeof("__TRAIT__")-1) == 0 ){` |
|       - | 1455 | `			/* __TRAIT__ magic constant: the name of the trait whose SOURCE lexically` |
|       - | 1456 | `			 * encloses this token. php resolves it at compile time, and it is shared` |
|       - | 1457 | `			 * across every using class because a trait method body compiles ONCE with` |
|       - | 1458 | `			 * the trait as its owner (PH7_ClassUseTrait adopts the same method pointer).` |
|       - | 1459 | `			 * Unlike __FUNCTION__/__METHOD__ (nearest function), __TRAIT__ is LEXICAL:` |
|       - | 1460 | `			 * closures and arrow-fns are TRANSPARENT (a closure inside a trait method still` |
|       - | 1461 | `			 * yields the trait), so we skip them and keep walking outward — but a class` |
|       - | 1462 | `			 * method or a plain function is an OPAQUE lexical boundary that fixes the answer.` |
|       - | 1463 | `			 * An anonymous class defined inside a trait method is a fresh scope, so` |
|       - | 1464 | `			 * __TRAIT__ is "" there, not the enclosing trait. "" outside any trait (global` |
|       - | 1465 | `			 * scope, plain functions, non-trait methods) — php renders it the empty string,` |
|       - | 1466 | `			 * not NULL. */` |
|      56 | 1467 | `			ph7_class *pTrait = 0;` |
|      56 | 1468 | `			if( pGen->iInMemberDefault > 0 ){` |
|       - | 1469 | `				/* A property/parameter DEFAULT is a const-expression that belongs to the` |
|       - | 1470 | `				 * class whose body is being compiled (pCurClass), never to a lexically-` |
|       - | 1471 | `				 * enclosing method. Read pCurClass directly — the block chain has no func` |
|       - | 1472 | `				 * block for the default and would leak into the enclosing function (an` |
|       - | 1473 | `				 * anonymous class's default inside a trait method is the anon's scope, "").*/` |
|      13 | 1474 | `				if( pGen->pCurClass && (pGen->pCurClass->iFlags & PH7_CLASS_TRAIT) ){` |
|       5 | 1475 | `					pTrait = pGen->pCurClass;` |
|       2 | 1476 | `				}` |
|       7 | 1477 | `			}else{` |
|      44 | 1478 | `				GenBlock *pBlock = pGen->pCurrent;` |
|     106 | 1479 | `				while( pBlock ){` |
|     102 | 1480 | `					if( pBlock->iFlags & GEN_BLOCK_FUNC ){` |
|      55 | 1481 | `						ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      55 | 1482 | `						if( pFunc == 0 ){` |
|       - | 1483 | `							/* A SYNTHETIC function block carries no ph7_vm_func — e.g. the` |
|       - | 1484 | `							 * per-arm throw-fixup block GenStateCompileMatchSubExpr enters to` |
|       - | 1485 | `							 * host a match() expression. It is not a real lexical scope` |
|       - | 1486 | `							 * boundary, so stay transparent and keep walking outward. */` |
|       6 | 1487 | `							pBlock = pBlock->pParent;` |
|       6 | 1488 | `							continue;` |
|       - | 1489 | `						}` |
|      51 | 1490 | `						if( pFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|       - | 1491 | `							/* A class method (of any class, including an anonymous one) is an` |
|       - | 1492 | `							 * OPAQUE scope boundary and fixes the answer: a trait method yields` |
|       - | 1493 | `							 * its trait, any other class's method yields "". Tested BEFORE the` |
|       - | 1494 | `							 * closure flags so a static method is never mistaken for transparent. */` |
|      34 | 1495 | `							if( pFunc->pUserData` |
|      37 | 1496 | `								&& (((ph7_class *)pFunc->pUserData)->iFlags & PH7_CLASS_TRAIT) ){` |
|      31 | 1497 | `								pTrait = (ph7_class *)pFunc->pUserData;` |
|      14 | 1498 | `							}` |
|      37 | 1499 | `							break;` |
|       - | 1500 | `						}` |
|      15 | 1501 | `						if( pFunc->iFlags & (VM_FUNC_CLOSURE\|VM_FUNC_ARROW\|VM_FUNC_STATIC_CL) ){` |
|       - | 1502 | `							/* Closure / arrow fn (VM_FUNC_CLOSURE is only set when the closure` |
|       - | 1503 | `							 * captures, so a capture-less static closure carries only` |
|       - | 1504 | `							 * VM_FUNC_STATIC_CL — include it). Transparent: keep walking outward. */` |
|      13 | 1505 | `							pBlock = pBlock->pParent;` |
|      13 | 1506 | `							continue;` |
|       - | 1507 | `						}` |
|       - | 1508 | `						/* A plain named function is an opaque boundary: __TRAIT__ is "". */` |
|       3 | 1509 | `						break;` |
|       - | 1510 | `					}` |
|      50 | 1511 | `					pBlock = pBlock->pParent;` |
|       4 | 1512 | `				}` |
|       - | 1513 | `			}` |
|      56 | 1514 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      56 | 1515 | `			if( pObj == 0 ){` |
|     ! 0 | 1516 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1517 | `				return SXERR_ABORT;` |
|       - | 1518 | `			}` |
|      56 | 1519 | `			if( pTrait ){` |
|      36 | 1520 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&pTrait->sName);` |
|      20 | 1521 | `			}else{` |
|      22 | 1522 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0); /* empty string */` |
|       - | 1523 | `			}` |
|      56 | 1524 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      56 | 1525 | `			return SXRET_OK;` |
| 1399354 | 1526 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|  155379 | 1527 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
| 1430052 | 1528 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|  144609 | 1529 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      61 | 1530 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - | 1531 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|       - | 1532 | `			/* Skip SYNTHETIC function blocks (GEN_BLOCK_FUNC with no ph7_vm_func in` |
|       - | 1533 | `			 * pUserData — e.g. the per-arm throw-fixup block a match() expression enters):` |
|       - | 1534 | `			 * they are not real function scopes. Without this a __FUNCTION__/__METHOD__` |
|       - | 1535 | `			 * inside a match arm reached a NULL pUserData and dereferenced it (compile-time` |
|       - | 1536 | `			 * crash); php resolves to the enclosing real function, which the walk now finds. */` |
|     152 | 1537 | `			while( pBlock && ((pBlock->iFlags & GEN_BLOCK_FUNC) == 0 \|\| pBlock->pUserData == 0) ){` |
|       - | 1538 | `				/* Point to the upper block */` |
|      65 | 1539 | `				pBlock = pBlock->pParent;` |
|       3 | 1540 | `			}` |
|      61 | 1541 | `			if( pBlock == 0 ){` |
|       - | 1542 | `				/* Called in the global scope,load NULL */` |
|       5 | 1543 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 | 1544 | `			}else{` |
|       - | 1545 | `				/* Extract the target function/method */` |
|      57 | 1546 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      57 | 1547 | `				int bMethod = (pStr->zString[2] == 'M'); /* __METHOD__ vs __FUNCTION__ */` |
|      57 | 1548 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      57 | 1549 | `				if( pObj == 0 ){` |
|     ! 0 | 1550 | `					PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1551 | `					return SXERR_ABORT;` |
|       - | 1552 | `				}` |
|       - | 1553 | `				/*` |
|       - | 1554 | `				 * __METHOD__ is the QUALIFIED name: "C::m" inside a method, and the plain` |
|       - | 1555 | `				 * function name inside a plain function (php does not answer "" there —` |
|       - | 1556 | `				 * PH7 loaded NULL, so __METHOD__ was empty in every free function and` |
|       - | 1557 | `				 * unqualified in every method).` |
|       - | 1558 | `				 */` |
|       - | 1559 | ``				/* A property hook answers php's `$p::get`, never the method name`` |
|       - | 1560 | ``				 * PHL synthesizes for it — so __METHOD__ reads `C::$p::get`, the`` |
|       - | 1561 | `				 * same rendering the runtime diagnostics use. */` |
|       - | 1562 | `				{` |
|       - | 1563 | `					SyString sProp,sSelf;` |
|       - | 1564 | `					const char *zKind;` |
|       - | 1565 | `					SyBlob sQual;` |
|       - | 1566 | `					SyString sOut;` |
|      57 | 1567 | `					int bHook = PH7_VmHookSplitName(&pFunc->sName,&sProp,&zKind);` |
|      57 | 1568 | `					SyBlobInit(&sQual,&pGen->pVm->sAllocator);` |
|      57 | 1569 | `					if( bHook ){` |
|       7 | 1570 | `						SyBlobFormat(&sQual,"$%z::%s",&sProp,zKind);` |
|       7 | 1571 | `						SyStringInitFromBuf(&sSelf,SyBlobData(&sQual),SyBlobLength(&sQual));` |
|      54 | 1572 | `					}else if( SyStringLength(&pFunc->sClosureName) > 0 ){` |
|       - | 1573 | ``						/* A closure answers php's `{closure:...}` name, never the`` |
|       - | 1574 | `						 * synthesized lookup key — and __METHOD__ answers the SAME text` |
|       - | 1575 | `						 * (the name already carries the declaring class), so the` |
|       - | 1576 | `						 * qualification below must not run for it. */` |
|      15 | 1577 | `						sSelf = pFunc->sClosureName;` |
|      15 | 1578 | `						bMethod = 0;` |
|       8 | 1579 | `					}else{` |
|      37 | 1580 | `						sSelf = pFunc->sName;` |
|       - | 1581 | `					}` |
|      65 | 1582 | `					if( bMethod && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|      19 | 1583 | `						SyString *pCls = &((ph7_class *)pFunc->pUserData)->sName;` |
|       - | 1584 | `						SyBlob sFull;` |
|      19 | 1585 | `						SyBlobInit(&sFull,&pGen->pVm->sAllocator);` |
|      19 | 1586 | `						SyBlobFormat(&sFull,"%z::%z",pCls,&sSelf);` |
|      19 | 1587 | `						SyStringInitFromBuf(&sOut,SyBlobData(&sFull),SyBlobLength(&sFull));` |
|      19 | 1588 | `						PH7_MemObjInitFromString(pGen->pVm,pObj,&sOut);` |
|      19 | 1589 | `						SyBlobRelease(&sFull);` |
|      11 | 1590 | `					}else{` |
|      41 | 1591 | `						PH7_MemObjInitFromString(pGen->pVm,pObj,&sSelf);` |
|       - | 1592 | `					}` |
|      57 | 1593 | `					SyBlobRelease(&sQual);` |
|       - | 1594 | `				}` |
|       - | 1595 | `				/* Emit the load constant instruction */` |
|      57 | 1596 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 1597 | `			}` |
|      61 | 1598 | `			return SXRET_OK;` |
|       - | 1599 | `	}` |
|       - | 1600 | ``	/* php keywords are CASE-INSENSITIVE (`SELF::C`, `Parent::m()`, `new STATIC`,`` |
|       - | 1601 | ``	 * `ISSET($x)`), but a few of them reach the engine as THIS literal and are matched`` |
|       - | 1602 | `	 * there BYTE-EXACTLY: the scope keywords against "self"/"parent"/"static" (OP_MEMBER,` |
|       - | 1603 | `	 * OP_NEW, the FCC scope resolver, the error formatter), and isset/empty/eval as the` |
|       - | 1604 | `	 * name of the host function their call dispatches to. Emit the canonical lower-case` |
|       - | 1605 | `	 * spelling for exactly those so the source's case never reaches the match — PH7` |
|       - | 1606 | ``	 * emitted the raw text, so `SELF::C` looked for a class literally named "SELF" and`` |
|       - | 1607 | ``	 * `ISSET($x)` for a function named "ISSET".`` |
|       - | 1608 | `	 *` |
|       - | 1609 | `	 * Every OTHER keyword literal keeps its source case on purpose: it is a CONSTANT` |
|       - | 1610 | ``	 * read (`define('OBJECT',1); echo OBJECT;` — PHL's keyword table covers type names`` |
|       - | 1611 | `	 * php's lexer does not reserve), and php constants are case-SENSITIVE. So is a` |
|       - | 1612 | ``	 * keyword used as a member NAME, which the parser flags: `class C { const STATIC = 5; }`` |
|       - | 1613 | ``	 * echo C::STATIC;` names the constant "STATIC", and folding it looked for "static". */`` |
| 1470034 | 1614 | `	if( (pToken->nType & PH7_TK_KEYWORD) && (pToken->nType & PH7_TK_MEMBER_NAME) == 0 ){` |
|   18237 | 1615 | `		sxu32 nKeyID = (sxu32)SX_PTR_TO_INT(pToken->pUserData);` |
|   18237 | 1616 | `		const char *zCanon = 0;` |
|   18237 | 1617 | `		if( nKeyID == PH7_TKWRD_SELF ){` |
|     457 | 1618 | `			zCanon = "self";` |
|   18011 | 1619 | `		}else if( nKeyID == PH7_TKWRD_PARENT ){` |
|     134 | 1620 | `			zCanon = "parent";` |
|   17720 | 1621 | `		}else if( nKeyID == PH7_TKWRD_STATIC ){` |
|     158 | 1622 | `			zCanon = "static";` |
|   17578 | 1623 | `		}else if( nKeyID == PH7_TKWRD_ISSET ){` |
|   17067 | 1624 | `			zCanon = "isset";` |
|    8959 | 1625 | `		}else if( nKeyID == PH7_TKWRD_EMPTY ){` |
|     217 | 1626 | `			zCanon = "empty";` |
|     333 | 1627 | `		}else if( nKeyID == PH7_TKWRD_EVAL ){` |
|     191 | 1628 | `			zCanon = "eval";` |
|      93 | 1629 | `		}` |
|   18237 | 1630 | `		if( zCanon ){` |
|   18201 | 1631 | `			SyStringInitFromBuf(&sCanon,zCanon,SyStrlen(zCanon));` |
|   18201 | 1632 | `			pStr = &sCanon;` |
|    9087 | 1633 | `		}` |
|    9105 | 1634 | `	}` |
|       - | 1635 | `	/* Query literal table */` |
| 1470034 | 1636 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - | 1637 | `		ph7_value *pLitObj;` |
|       - | 1638 | `		/* Unknown literal,install it in the literal table */` |
|  544931 | 1639 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  544931 | 1640 | `		if( pLitObj == 0 ){` |
|     ! 0 | 1641 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 1642 | `			return SXERR_ABORT;` |
|       - | 1643 | `		}` |
|  544931 | 1644 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,pStr);` |
|  544931 | 1645 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  271970 | 1646 | `	}` |
|       - | 1647 | `	/* Emit the load constant instruction.` |
|       - | 1648 | `	 *` |
|       - | 1649 | `	 * php resolves an UNQUALIFIED constant against the namespace its SOURCE sits in,` |
|       - | 1650 | `	 * decided at COMPILE time — so a function keeps its own namespace when called from` |
|       - | 1651 | ``	 * another one — and in a fixed order: a `use const` import first (and an import has`` |
|       - | 1652 | ``	 * NO global fallback), else `current-namespace\NAME`, else the global `NAME`. The`` |
|       - | 1653 | `	 * candidate that order picks is resolved here and travels in the instruction's p3;` |
|       - | 1654 | `	 * the VM's own lookup of the bare literal is then just the global step, and the` |
|       - | 1655 | `	 * "Undefined constant" message names the candidate, as php does.` |
|       - | 1656 | `	 *` |
|       - | 1657 | `	 * Only a name that could BE a constant needs one: a keyword literal never is (the` |
|       - | 1658 | `	 * scope keywords and isset/empty/eval reach the OO and call handlers by this same` |
|       - | 1659 | `	 * literal), and a call/new site clears PH7_LOADC_EXPAND before the constant path` |
|       - | 1660 | `	 * can ever run. */` |
|       - | 1661 | `	{` |
| 1470034 | 1662 | `		sxi32 iLoadFlags = PH7_LOADC_EXPAND;` |
| 1470034 | 1663 | `		char *zCand = 0;` |
| 1470034 | 1664 | `		if( (pToken->nType & PH7_TK_KEYWORD) == 0 ){` |
| 2175583 | 1665 | `			SyHashEntry *pImport = SyHashGet(&pGen->hUseConstImports,` |
| 1451351 | 1666 | `				(const void *)pStr->zString,pStr->nByte);` |
| 1451356 | 1667 | `			if( pImport ){` |
|      32 | 1668 | `				const char *zFQN = (const char *)pImport->pUserData;` |
|      32 | 1669 | `				zCand = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFQN,SyStrlen(zFQN));` |
|      32 | 1670 | `				iLoadFlags \|= PH7_LOADC_NOGLOBAL;` |
| 1451342 | 1671 | `			}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - | 1672 | `				SyBlob sCand;` |
|     747 | 1673 | `				SyBlobInit(&sCand,&pGen->pVm->sAllocator);` |
|     747 | 1674 | `				SyBlobAppend(&sCand,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     747 | 1675 | `				SyBlobAppend(&sCand,"\\",1);` |
|     747 | 1676 | `				SyBlobAppend(&sCand,pStr->zString,pStr->nByte);` |
|    1118 | 1677 | `				zCand = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     742 | 1678 | `					(const char *)SyBlobData(&sCand),SyBlobLength(&sCand));` |
|     747 | 1679 | `				SyBlobRelease(&sCand);` |
|     371 | 1680 | `			}` |
|  724227 | 1681 | `		}` |
| 1470034 | 1682 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,iLoadFlags,nIdx,zCand,0);` |
|       - | 1683 | `	}` |
| 1470034 | 1684 | `	return SXRET_OK;` |
|  861191 | 1685 | `}` |
|       - | 1686 | `/*` |
|       - | 1687 | ` * Resolve a namespace path or simply load a literal.` |
|       - | 1688 | ` * If the token stream contains namespace separators (backslashes),` |
|       - | 1689 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - | 1690 | ` * Otherwise, load the simple literal directly.` |
|       - | 1691 | ` */` |
| 1726269 | 1692 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       5 | 1693 | `{` |
|       - | 1694 | `	sxi32 rc;` |
| 1726274 | 1695 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 1696 | `		return SXRET_OK;` |
|       - | 1697 | `	}` |
|       - | 1698 | `	/* Check if this is a multi-token namespace path */` |
| 1726274 | 1699 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - | 1700 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|     586 | 1701 | `		SyBlob *pWorker = &pGen->sWorker;` |
|     586 | 1702 | `		int isAbsolute = 0;` |
|     586 | 1703 | `		SyBlobReset(pWorker);` |
|       - | 1704 | `		/* Check for leading backslash (absolute path) */` |
|     586 | 1705 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|     249 | 1706 | `			isAbsolute = 1;` |
|     249 | 1707 | `			pGen->pIn++; /* Skip leading backslash */` |
|     122 | 1708 | `		}` |
|       - | 1709 | `		/* Collect the raw path (no prefix yet) so its FIRST segment can be resolved` |
|       - | 1710 | ``		 * against use-imports below — php resolves `A\B\C` by mapping the leading`` |
|       - | 1711 | ``		 * `A` through the imports (`use X\A;` makes it `X\A\B\C`), and only prepends`` |
|       - | 1712 | ``		 * the current namespace when `A` matches no import. Blindly prefixing the`` |
|       - | 1713 | ``		 * namespace here produced e.g. `Ns\Ev\Bus` for `use ...\Event as Ev; Ev\Bus`. */`` |
|       - | 1714 | `		{` |
|       - | 1715 | `			SyBlob sRaw;` |
|     586 | 1716 | `			SyBlobInit(&sRaw,&pGen->pVm->sAllocator);` |
|       - | 1717 | ``			/* `namespace\X` spells the CURRENT namespace and is FULLY QUALIFIED from`` |
|       - | 1718 | `			 * there: no import ever applies to it, and the namespace is already in. */` |
|     586 | 1719 | `			if( !isAbsolute && GenStateNsRelPrefix(pGen,&pGen->pIn,pGen->pEnd,&sRaw) ){` |
|      71 | 1720 | `				isAbsolute = 1;` |
|      34 | 1721 | `			}` |
|    1484 | 1722 | `			while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|    1484 | 1723 | `				if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|     454 | 1724 | `					SyBlobAppend(&sRaw,"\\",1);` |
|     229 | 1725 | `				}else{` |
|    1035 | 1726 | `					SyBlobAppend(&sRaw,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - | 1727 | `				}` |
|    1484 | 1728 | `				if( pGen->pIn == &pGen->pEnd[-1] ){` |
|     586 | 1729 | `					pGen->pIn++;` |
|     586 | 1730 | `					break;` |
|       - | 1731 | `				}` |
|     903 | 1732 | `				pGen->pIn++;` |
|       5 | 1733 | `			}` |
|     586 | 1734 | `			if( isAbsolute ){` |
|     317 | 1735 | `				SyBlobAppend(pWorker,SyBlobData(&sRaw),SyBlobLength(&sRaw)); /* FQN as written */` |
|     161 | 1736 | `			}else{` |
|     274 | 1737 | `				const char *zRaw = (const char *)SyBlobData(&sRaw);` |
|     274 | 1738 | `				sxu32 nRaw = SyBlobLength(&sRaw);` |
|     274 | 1739 | `				sxu32 nFirst = 0;` |
|       - | 1740 | `				SyHashEntry *pNsImp;` |
|    1485 | 1741 | `				while( nFirst < nRaw && zRaw[nFirst] != '\\' ){ nFirst++; }` |
|     274 | 1742 | `				pNsImp = SyHashGet(&pGen->hUseImports,(const void *)zRaw,nFirst);` |
|     274 | 1743 | `				if( pNsImp ){` |
|       - | 1744 | `					/* Leading segment is an imported alias: substitute its FQN. */` |
|      47 | 1745 | `					const char *zFQN = (const char *)pNsImp->pUserData;` |
|      47 | 1746 | `					SyBlobAppend(pWorker,zFQN,SyStrlen(zFQN));` |
|      47 | 1747 | `					SyBlobAppend(pWorker,zRaw + nFirst,nRaw - nFirst);` |
|     252 | 1748 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      21 | 1749 | `					SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      21 | 1750 | `					SyBlobAppend(pWorker,"\\",1);` |
|      21 | 1751 | `					SyBlobAppend(pWorker,zRaw,nRaw);` |
|      12 | 1752 | `				}else{` |
|     211 | 1753 | `					SyBlobAppend(pWorker,zRaw,nRaw); /* global scope, no import */` |
|       - | 1754 | `				}` |
|       - | 1755 | `			}` |
|     586 | 1756 | `			SyBlobRelease(&sRaw);` |
|       - | 1757 | `		}` |
|     586 | 1758 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - | 1759 | `			ph7_value *pObj;` |
|       - | 1760 | `			SyString sPath;` |
|       - | 1761 | `			sxu32 nIdx;` |
|     586 | 1762 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - | 1763 | `			/*` |
|       - | 1764 | ``			 * `\true`, `\false` and `\null` are php's three reserved literals`` |
|       - | 1765 | `			 * written the way a code GENERATOR writes them -- fully qualified, so` |
|       - | 1766 | ``			 * that no `use` or namespace can shadow them. php resolves each to the`` |
|       - | 1767 | `			 * literal itself; this engine looked the name up in the constant table` |
|       - | 1768 | ``			 * and answered `Undefined constant "true"`. nikic/php-parser emits`` |
|       - | 1769 | `			 * every one of its booleans that way, which is what phpunit.phar dies` |
|       - | 1770 | ``			 * on. Only the GLOBAL spelling counts: `\Ns\true` is an ordinary`` |
|       - | 1771 | ``			 * constant, and so is a bare `true` (handled by the literal path,`` |
|       - | 1772 | `			 * which folds it already).` |
|       - | 1773 | `			 */` |
|     586 | 1774 | `			if( isAbsolute && SyByteFind(sPath.zString,sPath.nByte,'\\',0) != SXRET_OK ){` |
|     178 | 1775 | `				if( sPath.nByte == sizeof("null")-1` |
|      95 | 1776 | `				 && SyStrnicmp(sPath.zString,"null",sizeof("null")-1) == 0 ){` |
|     ! 0 | 1777 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|     ! 0 | 1778 | `					return SXRET_OK;` |
|       - | 1779 | `				}` |
|     178 | 1780 | `				if( sPath.nByte == sizeof("true")-1` |
|      95 | 1781 | `				 && SyStrnicmp(sPath.zString,"true",sizeof("true")-1) == 0 ){` |
|     ! 0 | 1782 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     ! 0 | 1783 | `					return SXRET_OK;` |
|       - | 1784 | `				}` |
|     178 | 1785 | `				if( sPath.nByte == sizeof("false")-1` |
|      95 | 1786 | `				 && SyStrnicmp(sPath.zString,"false",sizeof("false")-1) == 0 ){` |
|     ! 0 | 1787 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|     ! 0 | 1788 | `					return SXRET_OK;` |
|       - | 1789 | `				}` |
|      89 | 1790 | `			}` |
|       - | 1791 | `			/* Install in the literal table */` |
|     586 | 1792 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|     194 | 1793 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     194 | 1794 | `				if( pObj == 0 ){` |
|     ! 0 | 1795 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 1796 | `					return SXERR_ABORT;` |
|       - | 1797 | `				}` |
|     194 | 1798 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|     194 | 1799 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      94 | 1800 | `			}` |
|       - | 1801 | `			/* Emit the load constant instruction.` |
|       - | 1802 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|       - | 1803 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|     876 | 1804 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|     290 | 1805 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|     290 | 1806 | `				nIdx,0,0);` |
|     586 | 1807 | `			return SXRET_OK;` |
|       - | 1808 | `		}` |
|     ! 0 | 1809 | `	}` |
|       - | 1810 | `	/* Single-token literal: load directly */` |
| 1725693 | 1811 | `	rc = GenStateLoadLiteral(&(*pGen));` |
| 1725693 | 1812 | `	return rc;` |
|  861481 | 1813 | `}` |
|       - | 1814 | `/*` |
|       - | 1815 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - | 1816 | ` */` |
|       - | 1817 | `/*` |
|       - | 1818 | `` * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of`` |
|       - | 1819 | `` * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument`` |
|       - | 1820 | ``  * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...` `` |
|       - | 1821 | ` * appeared outside a call argument list — a syntax error (PHP rejects it likewise).` |
|       - | 1822 | ` */` |
|     ! 0 | 1823 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|     ! 0 | 1824 | `{` |
|     ! 0 | 1825 | `	SXUNUSED(iCompileFlag);` |
|     ! 0 | 1826 | `	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|       - | 1827 | `		"Cannot use the first-class callable syntax '...' here");` |
|     ! 0 | 1828 | `	return SXERR_SYNTAX;` |
|     ! 0 | 1829 | `}` |
| 1726269 | 1830 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 | 1831 | `{` |
|       - | 1832 | `	sxi32 rc;` |
| 1726274 | 1833 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
| 1726274 | 1834 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1835 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1836 | `		return rc;` |
|       - | 1837 | `	}` |
|       - | 1838 | `	/* Node successfully compiled */` |
| 1726274 | 1839 | `	return SXRET_OK;` |
|  861481 | 1840 | `}` |
|       - | 1841 |  |
