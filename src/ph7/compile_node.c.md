# src/ph7/compile_node.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 888/1030 lines (86.21%)

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
|   14389 |   37 | `static void GenStateClosureName(ph7_gen_state *pGen,sxu32 nLine)` |
|       5 |   38 | `{` |
|   14394 |   39 | `	ph7_vm_func *pOuter = 0;` |
|   14394 |   40 | `	GenBlock *pBlock = pGen->pCurrent;` |
|       - |   41 | `	SyBlob sName;` |
|       - |   42 | `	char *zDup;` |
|       - |   43 | `	/* Innermost REAL function block: a synthetic one (a match() arm's throw-fixup` |
|       - |   44 | `	 * block) carries no ph7_vm_func and is not a scope. */` |
|   30209 |   45 | `	while( pBlock ){` |
|   16672 |   46 | `		if( (pBlock->iFlags & GEN_BLOCK_FUNC) && pBlock->pUserData ){` |
|     857 |   47 | `			pOuter = (ph7_vm_func *)pBlock->pUserData;` |
|     857 |   48 | `			break;` |
|       - |   49 | `		}` |
|   15820 |   50 | `		pBlock = pBlock->pParent;` |
|       5 |   51 | `	}` |
|   14394 |   52 | `	SyStringInitFromBuf(&pGen->sPendingClosureName,0,0);` |
|   14394 |   53 | `	SyStringInitFromBuf(&pGen->sPendingClosureScope,0,0);` |
|   14394 |   54 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|   14394 |   55 | `	SyBlobAppend(&sName,"{closure:",sizeof("{closure:")-1);` |
|   14394 |   56 | `	if( pOuter == 0 ){` |
|       - |   57 | `		/* Top level: php writes the compiled file's path (empty when there is none —` |
|       - |   58 | `		 * an eval()/direct-API compile — which is php's "{closure::LINE}" there). */` |
|   13542 |   59 | `		SyString *pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|   13542 |   60 | `		if( pFile && SyStringLength(pFile) > 0 ){` |
|   13542 |   61 | `			SyBlobAppend(&sName,SyStringData(pFile),SyStringLength(pFile));` |
|    6659 |   62 | `		}` |
|    7511 |   63 | `	}else if( SyStringLength(&pOuter->sClosureName) > 0 ){` |
|       - |   64 | `		/* Enclosing closure: its whole name, no parens and no class -- but php's` |
|       - |   65 | `		 * SCOPE is inherited, so a closure written inside a closure inside a method` |
|       - |   66 | `		 * still belongs to that class. */` |
|     767 |   67 | `		SyBlobAppend(&sName,SyStringData(&pOuter->sClosureName),` |
|     254 |   68 | `			SyStringLength(&pOuter->sClosureName));` |
|     513 |   69 | `		pGen->sPendingClosureScope = pOuter->sClosureScope;` |
|     259 |   70 | `	}else{` |
|     349 |   71 | `		if( (pOuter->iFlags & VM_FUNC_CLASS_METHOD) && pOuter->pUserData ){` |
|     127 |   72 | `			SyString *pCls = &((ph7_class *)pOuter->pUserData)->sName;` |
|     127 |   73 | `			SyBlobAppend(&sName,SyStringData(pCls),SyStringLength(pCls));` |
|     127 |   74 | `			SyBlobAppend(&sName,"::",2);` |
|     127 |   75 | `			pGen->sPendingClosureScope = *pCls;` |
|      61 |   76 | `		}` |
|     349 |   77 | `		SyBlobAppend(&sName,SyStringData(&pOuter->sName),SyStringLength(&pOuter->sName));` |
|     349 |   78 | `		SyBlobAppend(&sName,"()",2);` |
|       - |   79 | `	}` |
|   14394 |   80 | `	SyBlobFormat(&sName,":%u}",nLine);` |
|   21473 |   81 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|   14389 |   82 | `		(const char *)SyBlobData(&sName),(sxu32)SyBlobLength(&sName));` |
|   14394 |   83 | `	if( zDup ){` |
|   14394 |   84 | `		SyStringInitFromBuf(&pGen->sPendingClosureName,zDup,(sxu32)SyBlobLength(&sName));` |
|    7079 |   85 | `	}` |
|   14394 |   86 | `	SyBlobRelease(&sName);` |
|   14394 |   87 | `}` |
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
|    6247 |  108 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  109 | `{` |
|    6252 |  110 | `	ph7_vm_func *pAnnonFunc = 0; /* Annonymous function body */` |
|       - |  111 | `	char zName[512];         /* Unique lambda name */` |
|       - |  112 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - |  113 | `							  * one thread is allowed to compile the script.` |
|       - |  114 | `						      */` |
|       - |  115 | `	SyString sName;` |
|    6252 |  116 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `$f = #[A] function…` trivia`` |
|       - |  117 | `	                              * is keyed to this ['static'] 'function' token */` |
|       - |  118 | `	sxu32 nKwLine;` |
|    6252 |  119 | `	sxi32 iFlags = 0;` |
|       - |  120 | `	sxu32 nLen;` |
|       - |  121 | `	sxi32 rc;` |
|    3107 |  122 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  123 |  |
|    6252 |  124 | `	nKwLine = pGen->pIn->nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|    6247 |  125 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|    6252 |  126 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  127 | `		/* Static closure: no $this auto-capture, bind refused */` |
|     343 |  128 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|     343 |  129 | `		pGen->pIn++; /* Jump the 'static' keyword */` |
|     169 |  130 | `	}` |
|    6252 |  131 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|       - |  132 | ``	/* `function &(…) {…}` — a closure returns by reference exactly as a named`` |
|       - |  133 | ``	 * function does, and the `&` sits in the same place. Nothing consumed it here,`` |
|       - |  134 | ``	 * so every by-ref closure was `syntax error, unexpected token "&", expecting`` |
|       - |  135 | ``	 * "("`; the arrow form already read its own. */`` |
|    6252 |  136 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|      14 |  137 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|      14 |  138 | `		pGen->pIn++;` |
|       6 |  139 | `	}` |
|    6252 |  140 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 |  141 | `		pGen->pIn++;` |
|     ! 0 |  142 | `	}` |
|       - |  143 | `	/* Generate a unique name */` |
|    6252 |  144 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - |  145 | `	/* Make sure the generated name is unique */` |
|    6252 |  146 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  147 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 |  148 | `	}` |
|    6252 |  149 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|       - |  150 | `	/* php's visible name for this closure, built before the body compiles so a` |
|       - |  151 | `	 * __FUNCTION__ inside it resolves to the same text php reports. */` |
|    6252 |  152 | `	GenStateClosureName(&(*pGen),nKwLine);` |
|       - |  153 | `	/* Compile the lambda body */` |
|    6252 |  154 | `	rc = GenStateCompileFunc(&(*pGen),&sName,iFlags,TRUE,&pAnnonFunc);` |
|    6252 |  155 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  156 | `		return SXERR_ABORT;` |
|       - |  157 | `	}` |
|    6252 |  158 | `	if( pAnnonFunc ){` |
|    6250 |  159 | `		pAnnonFunc->nLine = nKwLine;` |
|       - |  160 | ``		/* Expression-position attributes (`$f = #[A] function () {}`): the trivia`` |
|       - |  161 | `		 * sidecar keys them to the closure's first keyword token. */` |
|    6250 |  162 | `		if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnnonFunc->aAttrs) == SXERR_ABORT ){` |
|     ! 0 |  163 | `			return SXERR_ABORT;` |
|       - |  164 | `		}` |
|    6250 |  165 | `		if( GenStateCheckAttrPlacement(&(*pGen),&pAnnonFunc->aAttrs,2,2,0,0) == SXERR_ABORT ){` |
|     ! 0 |  166 | `			return SXERR_ABORT;` |
|       - |  167 | `		}` |
|       - |  168 | `		/* A closure's own attributes arrive AFTER its body compiled, so the` |
|       - |  169 | `		 * #[\NoDiscard] rules are decided here rather than with the signature. */` |
|    6250 |  170 | `		if( GenStateApplyNoDiscard(&(*pGen),pAnnonFunc,0,0) == SXERR_ABORT ){` |
|     ! 0 |  171 | `			return SXERR_ABORT;` |
|       - |  172 | `		}` |
|    3106 |  173 | `	}` |
|       - |  174 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|       - |  175 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|       - |  176 | `	 * the handler wraps either in a Closure instance. */` |
|    6252 |  177 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|       - |  178 | `	/* Node successfully compiled */` |
|    6252 |  179 | `	return SXRET_OK;` |
|    3112 |  180 | `}` |
|       - |  181 | `/*` |
|       - |  182 | ` * Add a free variable to the arrow function's closure environment, unless` |
|       - |  183 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|       - |  184 | ` * enclosing arrow level, or has already been captured.` |
|       - |  185 | ` */` |
|    5097 |  186 | `static sxi32 GenStateArrowAddCapture(` |
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
|    5102 |  198 | `	if( nByte == 0 ){` |
|     ! 0 |  199 | `		return SXRET_OK;` |
|       - |  200 | `	}` |
|    5097 |  201 | `	if( nByte == sizeof("this")-1` |
|    2681 |  202 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|      18 |  203 | `		return SXRET_OK;` |
|       - |  204 | `	}` |
|    5086 |  205 | `	if( PH7_VmIsAutoGlobal(zName,nByte) ){` |
|       - |  206 | `		/* php never auto-captures an auto-global — it is already visible inside` |
|       - |  207 | `		 * the arrow function. Capturing one was actively destructive here: the` |
|       - |  208 | `		 * install resolves the name through hSuper and so wrote the by-value` |
|       - |  209 | `		 * SNAPSHOT over the superglobal's own slot, which for $GLOBALS froze the` |
|       - |  210 | `		 * whole symbol-table view at closure-creation time for the rest of the` |
|       - |  211 | `		 * program. */` |
|      62 |  212 | `		return SXRET_OK;` |
|       - |  213 | `	}` |
|    5302 |  214 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|    1346 |  215 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|    1298 |  216 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|    1075 |  217 | `			return SXRET_OK;` |
|       - |  218 | `		}` |
|     142 |  219 | `	}` |
|    3956 |  220 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|    3956 |  221 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|    4452 |  222 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|     869 |  223 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|     744 |  224 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|     378 |  225 | `			return SXRET_OK;` |
|       - |  226 | `		}` |
|     253 |  227 | `	}` |
|    3583 |  228 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|    3583 |  229 | `	if( zDup == 0 ){` |
|     ! 0 |  230 | `		return SXERR_ABORT;` |
|       - |  231 | `	}` |
|    3583 |  232 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|    3583 |  233 | `	sEnv.iFlags = 0;` |
|    3583 |  234 | `	sEnv.nIdx = SXU32_HIGH;` |
|    3583 |  235 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|    3583 |  236 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|    3583 |  237 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|    3583 |  238 | `	return SXRET_OK;` |
|    2540 |  239 | `}` |
|       - |  240 | `/*` |
|       - |  241 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|       - |  242 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|       - |  243 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|       - |  244 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|       - |  245 | ` */` |
|    1303 |  246 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|       - |  247 | `	ph7_gen_state *pGen,` |
|       - |  248 | `	ph7_vm_func *pFunc,` |
|       - |  249 | `	const char *zIn,` |
|       - |  250 | `	const char *zEnd,` |
|       - |  251 | `	SyString *aShadow,` |
|       - |  252 | `	sxu32 nShadow)` |
|       5 |  253 | `{` |
|       - |  254 | `	sxi32 rc;` |
|    6704 |  255 | `	while( zIn < zEnd ){` |
|    5401 |  256 | `		if( zIn[0] == '\\' ){` |
|     461 |  257 | `			zIn++;` |
|     461 |  258 | `			if( zIn < zEnd ){` |
|     461 |  259 | `				zIn++;` |
|     221 |  260 | `			}` |
|     461 |  261 | `			continue;` |
|       - |  262 | `		}` |
|    4940 |  263 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|     139 |  264 | `			&& ((unsigned char)zIn[1] >= 0x80` |
|     134 |  265 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|       - |  266 | `			/* php's label bytes, the flat set (LEX_LABEL_START in lex.c). */` |
|       - |  267 | `			const char *zName;` |
|     138 |  268 | `			zIn++; /* skip '$' */` |
|     138 |  269 | `			zName = zIn;` |
|     507 |  270 | `			while( zIn < zEnd` |
|     756 |  271 | `				&& ((unsigned char)zIn[0] >= 0x80` |
|     714 |  272 | `					\|\| SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_') ){` |
|     622 |  273 | `				zIn++;` |
|       4 |  274 | `			}` |
|     138 |  275 | `			if( zIn > zName ){` |
|     204 |  276 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|     134 |  277 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|     138 |  278 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  279 | `					return SXERR_ABORT;` |
|       - |  280 | `				}` |
|      66 |  281 | `			}` |
|     138 |  282 | `			continue;` |
|       - |  283 | `		}` |
|    4811 |  284 | `		zIn++;` |
|       5 |  285 | `	}` |
|    1308 |  286 | `	return SXRET_OK;` |
|     648 |  287 | `}` |
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
|    8298 |  299 | `static sxi32 GenStateArrowCaptureScan(` |
|       - |  300 | `	ph7_gen_state *pGen,` |
|       - |  301 | `	ph7_vm_func *pFunc,` |
|       - |  302 | `	SyToken *pStart,` |
|       - |  303 | `	SyToken *pEnd,` |
|       - |  304 | `	SyString *aShadow,` |
|       - |  305 | `	sxu32 nShadow)` |
|       5 |  306 | `{` |
|    8303 |  307 | `	SyToken *pScan = pStart;` |
|       - |  308 | `	sxi32 rc;` |
|   76786 |  309 | `	while( pScan < pEnd ){` |
|   68488 |  310 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|    1951 |  311 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|     643 |  312 | `				pScan->sData.zString,` |
|    1303 |  313 | `				pScan->sData.zString + pScan->sData.nByte,` |
|     643 |  314 | `				aShadow,nShadow);` |
|    1308 |  315 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  316 | `				return SXERR_ABORT;` |
|       - |  317 | `			}` |
|    1308 |  318 | `			pScan++;` |
|    1308 |  319 | `			continue;` |
|       - |  320 | `		}` |
|   67185 |  321 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|     533 |  322 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|     533 |  323 | `			SyToken *pFnKw = (nKw == PH7_TKWRD_STATIC) ? &pScan[1] : pScan;` |
|       - |  324 | ``			/* A NESTED arrow function, not a `$fn`/`C::fn` name that merely`` |
|       - |  325 | `			 * spells the keyword (see PH7_TokenOpensArrowFunc). */` |
|     533 |  326 | `			if( PH7_TokenOpensArrowFunc(pStart,pScan,pEnd) ){` |
|       - |  327 | `				SyToken *pInnerSigStart;` |
|       - |  328 | `				SyToken *pInnerSigEnd;` |
|       - |  329 | `				SyToken *pInnerBodyEnd;` |
|       - |  330 | `				SyString *aInnerShadow;` |
|       - |  331 | `				sxu32 nInnerShadow;` |
|       - |  332 | `				sxu32 nInnerParamMax;` |
|       - |  333 | `				SyToken *p;` |
|       - |  334 | `				int iNestInner;` |
|     157 |  335 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|     157 |  336 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  337 | `					pScan++;` |
|     ! 0 |  338 | `				}` |
|     157 |  339 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  340 | `					pScan++;` |
|     ! 0 |  341 | `					continue;` |
|       - |  342 | `				}` |
|     157 |  343 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|     157 |  344 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|       - |  345 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|     157 |  346 | `				if( pInnerSigEnd >= pEnd ){` |
|     ! 0 |  347 | `					pScan = pEnd;` |
|     ! 0 |  348 | `					continue;` |
|       - |  349 | `				}` |
|       - |  350 | `				/* Build an augmented shadow list: inherited + inner params */` |
|     157 |  351 | `				nInnerParamMax = 0;` |
|     463 |  352 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|     310 |  353 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|     140 |  354 | `						nInnerParamMax++;` |
|      68 |  355 | `					}` |
|     157 |  356 | `				}` |
|     157 |  357 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|     152 |  358 | `					&pGen->pVm->sAllocator,` |
|     152 |  359 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|     157 |  360 | `				if( aInnerShadow == 0 ){` |
|     ! 0 |  361 | `					return SXERR_ABORT;` |
|       - |  362 | `				}` |
|     157 |  363 | `				nInnerShadow = 0;` |
|     169 |  364 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|      14 |  365 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|       8 |  366 | `				}` |
|     463 |  367 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|     310 |  368 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|     174 |  369 | `						continue;` |
|       - |  370 | `					}` |
|     140 |  371 | `					if( &p[1] >= pInnerSigEnd ){` |
|     ! 0 |  372 | `						break;` |
|       - |  373 | `					}` |
|     140 |  374 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  375 | `						continue;` |
|       - |  376 | `					}` |
|     140 |  377 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|      72 |  378 | `				}` |
|     157 |  379 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|     157 |  380 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|     ! 0 |  381 | `					pScan++;` |
|     ! 0 |  382 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|     ! 0 |  383 | `						&& pScan->sData.nByte == 1` |
|     ! 0 |  384 | `						&& pScan->sData.zString[0] == '?' ){` |
|     ! 0 |  385 | `						pScan++;` |
|     ! 0 |  386 | `					}` |
|     ! 0 |  387 | `					if( pScan < pEnd` |
|     ! 0 |  388 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|     ! 0 |  389 | `						pScan++;` |
|     ! 0 |  390 | `					}` |
|     ! 0 |  391 | `				}` |
|     157 |  392 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|     157 |  393 | `					pScan++; /* past '=>' */` |
|      76 |  394 | `				}` |
|     157 |  395 | `				pInnerBodyEnd = pScan;` |
|     157 |  396 | `				iNestInner = 0;` |
|    1121 |  397 | `				while( pInnerBodyEnd < pEnd ){` |
|    1099 |  398 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|       - |  399 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|       - |  400 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|     134 |  401 | `						break;` |
|       - |  402 | `					}` |
|     969 |  403 | `					if( pInnerBodyEnd->nType &` |
|       - |  404 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     126 |  405 | `						iNestInner++;` |
|     908 |  406 | `					}else if( pInnerBodyEnd->nType &` |
|       - |  407 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     126 |  408 | `						iNestInner--;` |
|      61 |  409 | `					}` |
|     969 |  410 | `					pInnerBodyEnd++;` |
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
|     157 |  423 | `					SyToken *pArgStart = pInnerSigStart;` |
|     293 |  424 | `					while( pArgStart < pInnerSigEnd ){` |
|     140 |  425 | `						SyToken *pArgEnd = pArgStart;` |
|     140 |  426 | `						SyToken *pEq = 0;` |
|     140 |  427 | `						int iNestArg = 0;` |
|     426 |  428 | `						while( pArgEnd < pInnerSigEnd ){` |
|     306 |  429 | `							if( iNestArg == 0` |
|     310 |  430 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|      22 |  431 | `								break;` |
|       - |  432 | `							}` |
|     290 |  433 | `							if( pArgEnd->nType &` |
|       - |  434 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  435 | `								iNestArg++;` |
|     290 |  436 | `							}else if( pArgEnd->nType &` |
|       - |  437 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  438 | `								iNestArg--;` |
|     ! 0 |  439 | `							}` |
|     286 |  440 | `							if( pEq == 0 && iNestArg == 0` |
|     284 |  441 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|       7 |  442 | `								pEq = pArgEnd;` |
|       3 |  443 | `							}` |
|     290 |  444 | `							pArgEnd++;` |
|       4 |  445 | `						}` |
|     140 |  446 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|      10 |  447 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       3 |  448 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|       7 |  449 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  450 | `								return SXERR_ABORT;` |
|       - |  451 | `							}` |
|       3 |  452 | `						}` |
|     140 |  453 | `						pArgStart = pArgEnd;` |
|     136 |  454 | `						if( pArgStart < pInnerSigEnd` |
|      82 |  455 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|      22 |  456 | `							pArgStart++;` |
|      10 |  457 | `						}` |
|       4 |  458 | `					}` |
|       - |  459 | `				}` |
|     233 |  460 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|      76 |  461 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|     157 |  462 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  463 | `					return SXERR_ABORT;` |
|       - |  464 | `				}` |
|     157 |  465 | `				pScan = pInnerBodyEnd;` |
|     157 |  466 | `				continue;` |
|       - |  467 | `			}` |
|     188 |  468 | `		}` |
|   67033 |  469 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|   62070 |  470 | `			pScan++;` |
|   62070 |  471 | `			continue;` |
|       - |  472 | `		}` |
|       - |  473 | `		{` |
|       - |  474 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|    4968 |  475 | `			SyToken *pDollar = pScan;` |
|    7432 |  476 | `			while( &pDollar[1] < pEnd` |
|    4968 |  477 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|     ! 0 |  478 | `				pDollar++;` |
|     ! 0 |  479 | `			}` |
|    4968 |  480 | `			if( &pDollar[1] >= pEnd ){` |
|     ! 0 |  481 | `				break;` |
|       - |  482 | `			}` |
|    4968 |  483 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  484 | `				pScan = pDollar + 1;` |
|     ! 0 |  485 | `				continue;` |
|       - |  486 | `			}` |
|    7437 |  487 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|    4963 |  488 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|    2469 |  489 | `				aShadow,nShadow);` |
|    4968 |  490 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  491 | `				return SXERR_ABORT;` |
|       - |  492 | `			}` |
|    4968 |  493 | `			pScan = pDollar + 2;` |
|       - |  494 | `		}` |
|       5 |  495 | `	}` |
|    8303 |  496 | `	return SXRET_OK;` |
|    4055 |  497 | `}` |
|       - |  498 | `/*` |
|       - |  499 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|       - |  500 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|       - |  501 | ` * variables by value. The body is a single expression that acts as an` |
|       - |  502 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|       - |  503 | ` * $this is also made available.` |
|       - |  504 | ` */` |
|    8146 |  505 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
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
|    8151 |  522 | `	sxi32 iFlags = 0;` |
|    8151 |  523 | `	int bStatic = 0;` |
|       - |  524 | `	sxi32 rc;` |
|       - |  525 | `	sxu32 n;` |
|    3974 |  526 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  527 |  |
|    8151 |  528 | `	nLine = pGen->pIn->nLine;` |
|       - |  529 | ``	/* Attribute-sidecar key: `#[A] [static] fn` trivia is keyed to this token */`` |
|    8151 |  530 | `	pTokKw = pGen->pIn;` |
|       - |  531 | `	/* Optional 'static' prefix */` |
|    8146 |  532 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|    8151 |  533 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|      82 |  534 | `		bStatic = 1;` |
|      82 |  535 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|      82 |  536 | `		pGen->pIn++;` |
|      39 |  537 | `	}` |
|       - |  538 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|    8146 |  539 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    8151 |  540 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|     ! 0 |  541 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  542 | `			"Arrow function: expected 'fn' keyword");` |
|     ! 0 |  543 | `		return SXERR_SYNTAX;` |
|       - |  544 | `	}` |
|    8151 |  545 | `	pGen->pIn++; /* Jump 'fn' */` |
|       - |  546 | `	/* Optional '&' — return by reference */` |
|    8151 |  547 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  548 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|     ! 0 |  549 | `		pGen->pIn++;` |
|     ! 0 |  550 | `	}` |
|       - |  551 | `	/* Expect '(' */` |
|    8151 |  552 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
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
|    8149 |  563 | `	pGen->pIn++; /* Jump '(' */` |
|       - |  564 | `	/* Delimit the parameter list */` |
|    8149 |  565 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|    8149 |  566 | `	if( pSigEnd >= pGen->pEnd ){` |
|       3 |  567 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  568 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       3 |  569 | `		return SXERR_SYNTAX;` |
|       - |  570 | `	}` |
|       - |  571 | `	/* Allocate the function state */` |
|    8147 |  572 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|    8147 |  573 | `	if( pFunc == 0 ){` |
|     ! 0 |  574 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  575 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  576 | `		return SXERR_ABORT;` |
|       - |  577 | `	}` |
|       - |  578 | `	/* Generate a unique lambda name */` |
|    8147 |  579 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|    8223 |  580 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|      79 |  581 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       3 |  582 | `	}` |
|    8147 |  583 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|    8147 |  584 | `	if( zDup == 0 ){` |
|     ! 0 |  585 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  586 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  587 | `		return SXERR_ABORT;` |
|       - |  588 | `	}` |
|    8147 |  589 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|       - |  590 | `	/* Reflection getStartLine(): line of the ['static'] 'fn' keyword */` |
|    8147 |  591 | `	pFunc->nLine = nLine;` |
|       - |  592 | `	/* php's visible name — an arrow function is named exactly like a closure. This` |
|       - |  593 | `	 * compiler builds its own function state, so it consumes the pending name itself. */` |
|    8147 |  594 | `	GenStateClosureName(&(*pGen),nLine);` |
|    8147 |  595 | `	pFunc->sClosureName = pGen->sPendingClosureName;` |
|    8147 |  596 | `	pFunc->sClosureScope = pGen->sPendingClosureScope;` |
|    8147 |  597 | `	SyStringInitFromBuf(&pGen->sPendingClosureName,0,0);` |
|       - |  598 | ``	/* Expression-position attributes (`$f = #[A] fn () => …`) */`` |
|    8147 |  599 | `	if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pFunc->aAttrs) == SXERR_ABORT ){` |
|     ! 0 |  600 | `		return SXERR_ABORT;` |
|       - |  601 | `	}` |
|    8147 |  602 | `	if( GenStateCheckAttrPlacement(&(*pGen),&pFunc->aAttrs,2,2,0,0) == SXERR_ABORT ){` |
|     ! 0 |  603 | `		return SXERR_ABORT;` |
|       - |  604 | `	}` |
|       - |  605 | `	/* An arrow function is a closure: its signature is exempt from the` |
|       - |  606 | ``	 * scope-keyword screen, exactly as `function () {}`'s is (see iSigScope). */`` |
|       - |  607 | `	{` |
|    8147 |  608 | `	int iSavedSig = pGen->iSigScope;` |
|    8147 |  609 | `	pGen->iSigScope = PH7_SIGSCOPE_CLOSURE;` |
|       - |  610 | `	/* Collect function arguments */` |
|    8147 |  611 | `	if( pGen->pIn < pSigEnd ){` |
|     812 |  612 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|     812 |  613 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  614 | `			pGen->iSigScope = iSavedSig;` |
|     ! 0 |  615 | `			return SXERR_ABORT;` |
|       - |  616 | `		}` |
|     402 |  617 | `	}` |
|       - |  618 | `	/* Point past ')' and parse optional return type */` |
|    8147 |  619 | `	pGen->pIn = &pSigEnd[1];` |
|    8147 |  620 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|    8147 |  621 | `	pGen->iSigScope = iSavedSig;` |
|       - |  622 | `	}` |
|    8147 |  623 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  624 | `		return SXERR_ABORT;` |
|    8147 |  625 | `	}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  626 | `		return SXERR_SYNTAX;` |
|       - |  627 | `	}` |
|    8147 |  628 | `	if( GenStateApplyNoDiscard(&(*pGen),pFunc,0,0) == SXERR_ABORT ){` |
|     ! 0 |  629 | `		return SXERR_ABORT;` |
|       - |  630 | `	}` |
|       - |  631 | `	/* Expect '=>' */` |
|    8147 |  632 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
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
|    8145 |  643 | `	pGen->pIn++; /* Jump '=>' */` |
|    8145 |  644 | `	pBodyStart = pGen->pIn;` |
|    8145 |  645 | `	pBodyEnd = pGen->pEnd;` |
|       - |  646 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|       - |  647 | `	 * recursively collect free-variable references from the body. The scan` |
|       - |  648 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|       - |  649 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|    8145 |  650 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       - |  651 | `	{` |
|    8145 |  652 | `		SyString *aShadow = 0;` |
|    8145 |  653 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|    8145 |  654 | `		if( nShadow > 0 ){` |
|     810 |  655 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|     805 |  656 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|     810 |  657 | `			if( aShadow == 0 ){` |
|     ! 0 |  658 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  659 | `					"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  660 | `				return SXERR_ABORT;` |
|       - |  661 | `			}` |
|    1830 |  662 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|    1025 |  663 | `				aShadow[n] = aArgs[n].sName;` |
|     513 |  664 | `			}` |
|     401 |  665 | `		}` |
|   12116 |  666 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|    3971 |  667 | `			aShadow,nShadow);` |
|    8145 |  668 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  669 | `			return SXERR_ABORT;` |
|       - |  670 | `		}` |
|       - |  671 | `	}` |
|       - |  672 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|       - |  673 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|       - |  674 | `	 * captured value is silently dropped when the enclosing scope has no` |
|       - |  675 | `	 * $this. */` |
|    8145 |  676 | `	if( !bStatic ){` |
|       - |  677 | `		char *zThisDup;` |
|    8067 |  678 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|    8067 |  679 | `		if( zThisDup == 0 ){` |
|     ! 0 |  680 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  681 | `				"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  682 | `			return SXERR_ABORT;` |
|       - |  683 | `		}` |
|    8067 |  684 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|    8067 |  685 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|    8067 |  686 | `		sEnv.nIdx = SXU32_HIGH;` |
|    8067 |  687 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|    8067 |  688 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|    8067 |  689 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|    3932 |  690 | `	}` |
|       - |  691 | `	/* Arrow functions are always closures; the ARROW mark tells OP_LOAD_CLOSURE` |
|       - |  692 | `	 * these captures are implicit (auto-scanned) so an undefined one stays silent` |
|       - |  693 | `	 * at creation — php only warns when the body reads it. */` |
|    8145 |  694 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE \| VM_FUNC_ARROW;` |
|       - |  695 | `	/* Compile the body expression as an implicit return */` |
|   12116 |  696 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|    3971 |  697 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|    8145 |  698 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  699 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  700 | `			"PH7 engine is running out-of-memory");` |
|     ! 0 |  701 | `		return SXERR_ABORT;` |
|       - |  702 | `	}` |
|    8145 |  703 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    8145 |  704 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|    8145 |  705 | `	pSavedEnd = pGen->pEnd;` |
|    8145 |  706 | `	pGen->pIn = pBodyStart;` |
|    8145 |  707 | `	pGen->pEnd = pBodyEnd;` |
|       - |  708 | ``	/* The body is an implicit `return <expr>`, which READS its operands. Compile`` |
|       - |  709 | `	 * it read-only (like echo / string interpolation) so a lone undefined variable` |
|       - |  710 | ``	 * — e.g. `fn()=>$z` for an auto-capture that was undefined at creation and so`` |
|       - |  711 | `	 * never captured (see VmExecOpLoadClosure) — raises php's "Undefined variable"` |
|       - |  712 | `	 * warning at the read instead of being loaded quietly as a plain expression` |
|       - |  713 | ``	 * statement (`$z;`, silent in both engines) would be. */`` |
|    8145 |  714 | `	rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|    8145 |  715 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  716 | `		return SXERR_ABORT;` |
|       - |  717 | `	}` |
|       - |  718 | `	/* The cursor stopped just past the body expression */` |
|    8145 |  719 | `	pFunc->nEndLine = (pGen->pIn > pBodyStart) ? pGen->pIn[-1].nLine : nLine;` |
|       - |  720 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|       - |  721 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|       - |  722 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|       - |  723 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|    8145 |  724 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|    8145 |  725 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|    8145 |  726 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|    8145 |  727 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|    8145 |  728 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  729 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|    8145 |  730 | `	pGen->pIn = pBodyEnd;` |
|    8145 |  731 | `	pGen->pEnd = pSavedEnd;` |
|       - |  732 | ``	/* An arrow body is one expression, and php takes a `yield` in it: calling`` |
|       - |  733 | ``	 * `fn() => yield 7` hands back a Generator, exactly as the `function` form`` |
|       - |  734 | `	 * does. Only the closure/function path scanned for the opcode, so the arrow's` |
|       - |  735 | ``	 * yield ran with no generator frame around it and raised `Cannot use yield`` |
|       - |  736 | ``	 * outside of a generator`. Same scan, same definition-time return-type screen. */`` |
|       - |  737 | `	{` |
|    8145 |  738 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - |  739 | `		sxu32 i;` |
|   85871 |  740 | `		for( i = 0 ; i < SySetUsed(&pFunc->aByteCode) ; i++ ){` |
|   77749 |  741 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|      19 |  742 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      19 |  743 | `				break;` |
|       - |  744 | `			}` |
|   37966 |  745 | `		}` |
|       - |  746 | `	}` |
|    8145 |  747 | `	if( pFunc->iFlags & VM_FUNC_GENERATOR ){` |
|      19 |  748 | `		if( SXERR_ABORT == GenStateValidateGeneratorReturnType(&(*pGen),pFunc) ){` |
|     ! 0 |  749 | `			return SXERR_ABORT;` |
|       - |  750 | `		}` |
|       9 |  751 | `	}` |
|       - |  752 | `	/* Emit the load-closure instruction */` |
|    8145 |  753 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|    8145 |  754 | `	return SXRET_OK;` |
|    3979 |  755 | `}` |
|       - |  756 | `/*` |
|       - |  757 | ` * Compile a single arm's expression range into a freshly-allocated` |
|       - |  758 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|       - |  759 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|       - |  760 | ` * expression's value.` |
|       - |  761 | ` */` |
|     608 |  762 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|       - |  763 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|       4 |  764 | `{` |
|       - |  765 | `	SySet *pInstrContainer;` |
|       - |  766 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  767 | `	GenBlock *pArmBlock;` |
|       - |  768 | `	sxi32 rc;` |
|     612 |  769 | `	pTmpIn  = pGen->pIn;` |
|     612 |  770 | `	pTmpEnd = pGen->pEnd;` |
|     612 |  771 | `	pGen->pIn  = pStart;` |
|     612 |  772 | `	pGen->pEnd = pStop;` |
|     612 |  773 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     612 |  774 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|       - |  775 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|       - |  776 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|       - |  777 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|       - |  778 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|       - |  779 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|     916 |  780 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|     304 |  781 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|     612 |  782 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  783 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  784 | `		pGen->pIn  = pTmpIn;` |
|     ! 0 |  785 | `		pGen->pEnd = pTmpEnd;` |
|     ! 0 |  786 | `		return SXERR_ABORT;` |
|       - |  787 | `	}` |
|     612 |  788 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     612 |  789 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     612 |  790 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     612 |  791 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     612 |  792 | `	GenStateLeaveBlock(&(*pGen),0);` |
|     612 |  793 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     612 |  794 | `	pGen->pIn  = pTmpIn;` |
|     612 |  795 | `	pGen->pEnd = pTmpEnd;` |
|     612 |  796 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  797 | `		return SXERR_ABORT;` |
|       - |  798 | `	}` |
|     612 |  799 | `	if( rc == SXERR_EMPTY ){` |
|     ! 0 |  800 | `		return SXERR_EMPTY;` |
|       - |  801 | `	}` |
|     612 |  802 | `	return SXRET_OK;` |
|     308 |  803 | `}` |
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
|       2 |  817 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|       1 |  818 | `{` |
|       - |  819 | `	va_list ap;` |
|       - |  820 | `	sxi32 rc;` |
|       - |  821 | `	SyBlob sMsg;` |
|       3 |  822 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|       3 |  823 | `	va_start(ap,zFmt);` |
|       3 |  824 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|       3 |  825 | `	va_end(ap);` |
|       3 |  826 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|       3 |  827 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|       3 |  828 | `	SyBlobRelease(&sMsg);` |
|       3 |  829 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  830 | `		return SXERR_ABORT;` |
|       - |  831 | `	}` |
|       3 |  832 | `	return SXERR_SYNTAX;` |
|       2 |  833 | `}` |
|       - |  834 | `/*` |
|       - |  835 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|       - |  836 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|       - |  837 | ` * Returns the stop token pointer (or pEnd if none found).` |
|       - |  838 | ` */` |
|     610 |  839 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|       4 |  840 | `{` |
|     614 |  841 | `	SyToken *pCur = pStart;` |
|     614 |  842 | `	int iNest = 0;` |
|    1838 |  843 | `	while( pCur < pEnd ){` |
|    1748 |  844 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     107 |  845 | `			iNest++;` |
|    1696 |  846 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     107 |  847 | `			iNest--;` |
|    1592 |  848 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|     524 |  849 | `			return pCur;` |
|       - |  850 | `		}` |
|    1228 |  851 | `		pCur++;` |
|       4 |  852 | `	}` |
|      94 |  853 | `	return pEnd;` |
|     309 |  854 | `}` |
|     138 |  855 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  856 | `{` |
|       - |  857 | `	ph7_match *pMatch;` |
|       - |  858 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|     143 |  859 | `	int bHasDefault = 0;` |
|       - |  860 | `	sxu32 nLine;` |
|       - |  861 | `	sxi32 rc;` |
|      69 |  862 | `	SXUNUSED(iCompileFlag);` |
|     143 |  863 | `	nLine = pGen->pIn->nLine;` |
|     143 |  864 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|       - |  865 | `	/* Expect '(' */` |
|     143 |  866 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  867 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  868 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|     ! 0 |  869 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|       - |  870 | `	}` |
|     143 |  871 | `	pGen->pIn++; /* Jump '(' */` |
|     143 |  872 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|     143 |  873 | `	if( pSubjEnd >= pGen->pEnd ){` |
|     ! 0 |  874 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  875 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       - |  876 | `	}` |
|     143 |  877 | `	if( pGen->pIn >= pSubjEnd ){` |
|     ! 0 |  878 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  879 | `			"syntax error, unexpected \")\", expecting match subject");` |
|       - |  880 | `	}` |
|       - |  881 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|     143 |  882 | `	pSavedEnd = pGen->pEnd;` |
|     143 |  883 | `	pGen->pEnd = pSubjEnd;` |
|     143 |  884 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     143 |  885 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  886 | `		return SXERR_ABORT;` |
|       - |  887 | `	}` |
|     143 |  888 | `	pGen->pEnd = pSavedEnd;` |
|     143 |  889 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|       - |  890 | `	/* Expect '{' */` |
|     143 |  891 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  892 | `		return GenStateMatchError(pGen,` |
|     ! 0 |  893 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  894 | `			"syntax error, expecting \"{\" after match subject");` |
|       - |  895 | `	}` |
|     143 |  896 | `	pGen->pIn++; /* Jump '{' */` |
|     143 |  897 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|     143 |  898 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  899 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  900 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|       - |  901 | `	}` |
|       - |  902 | `	/* Allocate ph7_match container */` |
|     143 |  903 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|     143 |  904 | `	if( pMatch == 0 ){` |
|     ! 0 |  905 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  906 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  907 | `		return SXERR_ABORT;` |
|       - |  908 | `	}` |
|     143 |  909 | `	SyZero(pMatch,sizeof(ph7_match));` |
|     143 |  910 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|       - |  911 | `	/* Iterate arms */` |
|     467 |  912 | `	while( pGen->pIn < pBodyEnd ){` |
|       - |  913 | `		ph7_match_arm sArm;` |
|       - |  914 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|     332 |  915 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|     332 |  916 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|     332 |  917 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|     332 |  918 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  919 | `		/* 'default' arm? */` |
|     328 |  920 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     193 |  921 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|      54 |  922 | `			if( bHasDefault ){` |
|       3 |  923 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|       - |  924 | `					"Match expressions may only contain one default arm");` |
|       4 |  925 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  926 | `			}` |
|      52 |  927 | `			sArm.bDefault = 1;` |
|      52 |  928 | `			bHasDefault = 1;` |
|      52 |  929 | `			pGen->pIn++;` |
|      52 |  930 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|     ! 0 |  931 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  932 | `					"syntax error, expecting \"=>\" after 'default'");` |
|       - |  933 | `			}` |
|      52 |  934 | `			pGen->pIn++; /* Jump '=>' */` |
|      28 |  935 | `		}else{` |
|       - |  936 | `			/* Condition list: cond (',' cond)* '=>' */` |
|     282 |  937 | `			pCondStart = pGen->pIn;` |
|     282 |  938 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|       - |  939 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|     290 |  940 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|       - |  941 | `				SySet sCondBc;` |
|       9 |  942 | `				if( pCondStart >= pArrow ){` |
|     ! 0 |  943 | `					return GenStateMatchError(pGen,nArmLine,` |
|       - |  944 | `						"syntax error, empty match condition expression");` |
|       - |  945 | `				}` |
|       9 |  946 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       9 |  947 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|       9 |  948 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  949 | `					return SXERR_ABORT;` |
|       - |  950 | `				}` |
|       9 |  951 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       9 |  952 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|       9 |  953 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|       - |  954 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|       1 |  955 | `			}` |
|     282 |  956 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  957 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  958 | `					"syntax error, expecting \"=>\" in match arm");` |
|       - |  959 | `			}` |
|     280 |  960 | `			if( pCondStart >= pArrow ){` |
|     ! 0 |  961 | `				return GenStateMatchError(pGen,nArmLine,` |
|       - |  962 | `					"syntax error, empty match condition expression");` |
|       - |  963 | `			}` |
|       - |  964 | `			{` |
|       - |  965 | `				SySet sCondBc;` |
|     280 |  966 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     280 |  967 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|     280 |  968 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  969 | `					return SXERR_ABORT;` |
|       - |  970 | `				}` |
|     280 |  971 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|       - |  972 | `			}` |
|     280 |  973 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|       - |  974 | `		}` |
|       - |  975 | `		/* Compile result expression: up to top-level ',' or body end */` |
|     328 |  976 | `		pResStart = pGen->pIn;` |
|     328 |  977 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|     328 |  978 | `		if( pResStart >= pResEnd ){` |
|     ! 0 |  979 | `			return GenStateMatchError(pGen,nArmLine,` |
|       - |  980 | `				"syntax error, expected expression after \"=>\"");` |
|       - |  981 | `		}` |
|     328 |  982 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|     328 |  983 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  984 | `			return SXERR_ABORT;` |
|       - |  985 | `		}` |
|     328 |  986 | `		pGen->pIn = pResEnd;` |
|     328 |  987 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|     240 |  988 | `			pGen->pIn++; /* Skip trailing ',' */` |
|     118 |  989 | `		}` |
|     328 |  990 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|       4 |  991 | `	}` |
|     139 |  992 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|     139 |  993 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|     139 |  994 | `	return SXRET_OK;` |
|      74 |  995 | `}` |
|       - |  996 | `/*` |
|       - |  997 | ` * Compile a backtick quoted string.` |
|       - |  998 | ` */` |
|       2 |  999 | `PH7_PRIVATE sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 | 1000 | `{` |
|       1 | 1001 | `	SXUNUSED(iCompileFlag);` |
|       - | 1002 | `	/*` |
|       - | 1003 | ``	 * The backtick (`) operator was DEPRECATED by php (shell_exec() is the replacement).`` |
|       - | 1004 | `	 * PHL targets php's *non-deprecated* surface, so it is a hard parse error — never` |
|       - | 1005 | `	 * compiled to a shell_exec() call.` |
|       - | 1006 | `	 */` |
|       3 | 1007 | `	PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - | 1008 | ``		"syntax error, the backtick (`) operator was removed, use shell_exec() instead");`` |
|       3 | 1009 | `	return SXERR_ABORT;` |
|       1 | 1010 | `}` |
|       - | 1011 | `/*` |
|       - | 1012 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - | 1013 | ` * construct.` |
|       - | 1014 | ` */` |
|     292 | 1015 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 | 1016 | `{` |
|       - | 1017 | `	SyString *pName;` |
|       - | 1018 | `	sxu32 nKeyID;` |
|       - | 1019 | `	sxi32 rc;` |
|       - | 1020 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|     297 | 1021 | `	pName = &pGen->pIn->sData;` |
|     297 | 1022 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     297 | 1023 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|     297 | 1024 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       6 | 1025 | `		SyToken *pTmp,*pNext = 0;` |
|       - | 1026 | ``		/* A STATEMENT `echo` never reaches here — it dispatches through the statement`` |
|       - | 1027 | `		 * table. Arriving in expression position means source like` |
|       - | 1028 | ``		 * `fopen('f','r') or echo "IO error";`, which was a Symisc extension and is a`` |
|       - | 1029 | `		 * php parse error (§10: a PH7-ism that changes the meaning of valid source is a` |
|       - | 1030 | ``		 * bug). The one legitimate expression-echo is the token a `<?= ... ?>` short tag`` |
|       - | 1031 | `		 * synthesizes, which raises nExprEchoOk around its own compile. */` |
|       6 | 1032 | `		if( pGen->nExprEchoOk < 1 ){` |
|       3 | 1033 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn - 1,0);` |
|       3 | 1034 | `			return SXERR_ABORT;` |
|       - | 1035 | `		}` |
|       - | 1036 | `		/* Compile arguments one after one */` |
|       3 | 1037 | `		pTmp = pGen->pEnd;` |
|       3 | 1038 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1 /* Boolean true index */,0,0);` |
|       5 | 1039 | `		while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       3 | 1040 | `			if( pGen->pIn < pNext ){` |
|       3 | 1041 | `				pGen->pEnd = pNext;` |
|       3 | 1042 | `				rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|       3 | 1043 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 1044 | `					return SXERR_ABORT;` |
|       - | 1045 | `				}` |
|       3 | 1046 | `				if( rc != SXERR_EMPTY ){` |
|       - | 1047 | `					/* Ticket 1433-008: Optimization #1: Consume input directly` |
|       - | 1048 | `					 * without the overhead of a function call.` |
|       - | 1049 | `					 * This is a very powerful optimization that improve` |
|       - | 1050 | `					 * performance greatly.` |
|       - | 1051 | `					 */` |
|       3 | 1052 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|       1 | 1053 | `				}` |
|       1 | 1054 | `			}` |
|       - | 1055 | `			/* Jump trailing commas */` |
|       3 | 1056 | `			while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|     ! 0 | 1057 | `				pNext++;` |
|     ! 0 | 1058 | `			}` |
|       3 | 1059 | `			pGen->pIn = pNext;` |
|       1 | 1060 | `		}` |
|       - | 1061 | `		/* Restore token stream */` |
|       3 | 1062 | `		pGen->pEnd = pTmp;` |
|       2 | 1063 | `	}else{` |
|     293 | 1064 | `		sxi32 nArg = 0;` |
|     293 | 1065 | `		sxu32 nIdx = 0;` |
|       - | 1066 | `		char zCanon[sizeof("include_once")-1];` |
|       - | 1067 | `		SyString sCanon;` |
|     293 | 1068 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|     293 | 1069 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1070 | `			return SXERR_ABORT;` |
|     293 | 1071 | `		}else if(rc != SXERR_EMPTY ){` |
|     293 | 1072 | `			nArg = 1;` |
|     144 | 1073 | `		}` |
|       - | 1074 | `		/* The construct is dispatched as a CALL to the host function of the same name,` |
|       - | 1075 | `		 * so the name emitted here must be the construct's canonical spelling, not the` |
|       - | 1076 | ``		 * source's: php accepts `PRINT`/`Isset`/`EVAL` (keywords are case-insensitive)`` |
|       - | 1077 | ``		 * where the raw text produced `Call to undefined function PRINT()`. Every`` |
|       - | 1078 | `		 * construct name is lower-case ASCII, so folding IS canonicalising. */` |
|     293 | 1079 | `		if( pName->nByte <= sizeof(zCanon) ){` |
|       - | 1080 | `			sxu32 i;` |
|    2495 | 1081 | `			for( i = 0 ; i < pName->nByte ; ++i ){` |
|    2207 | 1082 | `				unsigned char c = (unsigned char)pName->zString[i];` |
|    2207 | 1083 | `				zCanon[i] = (char)((c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c);` |
|    1106 | 1084 | `			}` |
|     293 | 1085 | `			SyStringInitFromBuf(&sCanon,zCanon,pName->nByte);` |
|     293 | 1086 | `			pName = &sCanon;` |
|     144 | 1087 | `		}` |
|     293 | 1088 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - | 1089 | `			ph7_value *pObj;` |
|       - | 1090 | `			/* Emit the call instruction */` |
|     169 | 1091 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     169 | 1092 | `			if( pObj == 0 ){` |
|     ! 0 | 1093 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1094 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1095 | `				return SXERR_ABORT;` |
|       - | 1096 | `			}` |
|     169 | 1097 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - | 1098 | `			/* Install in the literal table */` |
|     169 | 1099 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      82 | 1100 | `		}` |
|       - | 1101 | `		/* Emit the call instruction */` |
|     293 | 1102 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     293 | 1103 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       - | 1104 | `	}` |
|       - | 1105 | `	/* Node successfully compiled */` |
|     295 | 1106 | `	return SXRET_OK;` |
|     151 | 1107 | `}` |
|       - | 1108 | `/*` |
|       - | 1109 | ` * Compile a node holding a variable declaration.` |
|       - | 1110 | ` * According to the PHP language reference` |
|       - | 1111 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - | 1112 | ` *  The variable name is case-sensitive.` |
|       - | 1113 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - | 1114 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - | 1115 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - | 1116 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - | 1117 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - | 1118 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - | 1119 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - | 1120 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - | 1121 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - | 1122 | ` *  the chapter on Expressions.` |
|       - | 1123 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - | 1124 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - | 1125 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - | 1126 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - | 1127 | ` *  is being assigned (the source variable).` |
|       - | 1128 | ` */` |
| 2926128 | 1129 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 | 1130 | `{` |
| 2926133 | 1131 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 1132 | `	sxi32 iVv;` |
|       - | 1133 | `	sxi32 iP1;` |
| 2926133 | 1134 | ``	sxi32 iP2 = 0; /* 1 = quiet read (isset/empty), 4 = quiet read (`??`): a missing`` |
|       - | 1135 | `	                * variable must not warn in either */` |
|       - | 1136 | `	void *p3;` |
|       - | 1137 | `	sxi32 rc;` |
| 2926133 | 1138 | `	iVv = -1; /* Variable variable counter */` |
| 5852303 | 1139 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
| 2926175 | 1140 | `		pGen->pIn++;` |
| 2926175 | 1141 | `		iVv++;` |
|       5 | 1142 | `	}` |
| 2926133 | 1143 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - | 1144 | `		/* Invalid variable name */` |
|     ! 0 | 1145 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable or \"{\" or \"$\"");` |
|     ! 0 | 1146 | `		if( rc == SXERR_ABORT ){` |
|       - | 1147 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1148 | `			return SXERR_ABORT;` |
|       - | 1149 | `		}` |
|     ! 0 | 1150 | `		return SXRET_OK;` |
|       - | 1151 | `	}` |
| 2926133 | 1152 | `	p3  = 0;` |
| 2926133 | 1153 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - | 1154 | `		/* Dynamic variable creation */` |
|      38 | 1155 | `		pGen->pIn++;  /* Jump the open curly */` |
|      38 | 1156 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      38 | 1157 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 1158 | `			/* Empty expression */` |
|       - | 1159 | `			{` |
|       - | 1160 | `			/* php names the offending token and, for an empty "${}", stops there:` |
|       - | 1161 | `			 * the "expecting" tail only appears when something could still follow. */` |
|       - | 1162 | ``			/* `${}`: pEnd was stepped back past the trailing '}', so the token php`` |
|       - | 1163 | `			 * names sits AT pEnd. Reach for it before deciding the tail -- php stops` |
|       - | 1164 | `			 * at "unexpected token \"}\"" with no "expecting" clause, which the` |
|       - | 1165 | `			 * NULL-token path could not express because it never saw the '}'. */` |
|       3 | 1166 | `			SyToken *pBad = pGen->pIn < pGen->pEnd ? pGen->pIn : 0;` |
|       3 | 1167 | `			if( pBad == 0 && pGen->pTokenSet ){` |
|       3 | 1168 | `				SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|       3 | 1169 | `				SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|       3 | 1170 | `				if( pGen->pEnd >= pBase && pGen->pEnd < pStreamEnd ){` |
|       3 | 1171 | `					pBad = pGen->pEnd;` |
|       1 | 1172 | `				}` |
|       1 | 1173 | `			}` |
|       5 | 1174 | `			PH7_GenSyntaxError(&(*pGen),pBad,` |
|       2 | 1175 | `				(pBad && (pBad->nType & PH7_TK_CCB)) ? 0 : "variable or \"{\" or \"$\"");` |
|       - | 1176 | `			}` |
|       3 | 1177 | `			return SXRET_OK;` |
|       - | 1178 | `		}` |
|       - | 1179 | `		/* Compile the expression holding the variable name. It is a pure READ, so` |
|       - | 1180 | ``		 * compile it read-only: `${$u}` warns on an undefined $u (php) instead of`` |
|       - | 1181 | ``		 * silently creating it, matching the `$$u` name-read path below. A quiet`` |
|       - | 1182 | `		 * outer (isset()/empty()) suppresses that name warning too, so carry the` |
|       - | 1183 | `		 * quiet flag into the name expression. */` |
|      36 | 1184 | `		sxi32 iNameFlags = EXPR_FLAG_RDONLY_LOAD;` |
|      36 | 1185 | `		if( iCompileFlag & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY) ){` |
|       - | 1186 | ``			/* isset()/empty() suppress the name warning; `??` (EXPR_FLAG_QUIET_VAR)`` |
|       - | 1187 | ``			 * does NOT — php warns `Undefined variable $u` for `${$u} ?? x` and only`` |
|       - | 1188 | `			 * quiets the TARGET read, so QUIET_VAR is deliberately excluded here. */` |
|       3 | 1189 | `			iNameFlags \|= EXPR_FLAG_QUIET_VAR;` |
|       1 | 1190 | `		}` |
|      36 | 1191 | `		rc = PH7_CompileExpr(&(*pGen),iNameFlags,0);` |
|      36 | 1192 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1193 | `			return SXERR_ABORT;` |
|      36 | 1194 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 | 1195 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,0);` |
|       3 | 1196 | `			return SXRET_OK;` |
|       - | 1197 | `		}` |
|      18 | 1198 | `	}else{` |
|       - | 1199 | `		SyHashEntry *pEntry;` |
|       - | 1200 | `		SyString *pName;` |
| 2926099 | 1201 | `		char *zName = 0;` |
|       - | 1202 | `		/* Extract variable name */` |
| 2926099 | 1203 | `		pName = &pGen->pIn->sData;` |
|       - | 1204 | `		/* Advance the stream cursor */` |
| 2926099 | 1205 | `		pGen->pIn++;` |
| 2926099 | 1206 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
| 2926099 | 1207 | `		if( pEntry == 0 ){` |
|       - | 1208 | `			/* Duplicate name */` |
|  425428 | 1209 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  425428 | 1210 | `			if( zName == 0 ){` |
|     ! 0 | 1211 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1212 | `				return SXERR_ABORT;` |
|       - | 1213 | `			}` |
|       - | 1214 | `			/* Install in the hashtable */` |
|  425428 | 1215 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|  212392 | 1216 | `		}else{` |
|       - | 1217 | `			/* Name already available */` |
| 2500676 | 1218 | `			zName = (char *)pEntry->pUserData;` |
|       - | 1219 | `		}` |
| 2926099 | 1220 | `		p3 = (void *)zName;` |
|       - | 1221 | `	}` |
| 2926129 | 1222 | `	iP1 = 0;` |
| 2926129 | 1223 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
| 2776089 | 1224 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - | 1225 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
| 1984702 | 1226 | `			iP1 = 1;` |
|  990930 | 1227 | `		}` |
| 1386075 | 1228 | `	}` |
|       - | 1229 | ``	/* iP2 marks a QUIET read: `isset($x)` / `empty($x)` inspect a variable`` |
|       - | 1230 | `	 * without reading it, so an undefined one must not warn (php stays silent` |
|       - | 1231 | `	 * for both). Every other read of a missing variable warns — see OP_LOAD.` |
|       - | 1232 | `	 * The two flags are cleared before recursing into a subscript's index` |
|       - | 1233 | ``	 * expression, so `isset($a[$i])` still warns for an undefined $i, as php`` |
|       - | 1234 | ``	 * does. Contexts that VIVIFY (assignment targets, `??`, appends) already`` |
|       - | 1235 | `	 * emit iP1 = 0 and never reach the warning. */` |
| 2926129 | 1236 | `	if( iCompileFlag & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY) ){` |
|   14633 | 1237 | `		iP2 = 1;` |
| 2918806 | 1238 | `	}else if( iCompileFlag & EXPR_FLAG_QUIET_VAR ){` |
|       - | 1239 | ``		/* `??`'s quiet read is quiet for the same WARNING and not for the same`` |
|       - | 1240 | ``		 * rule: php swallows `Undefined variable $x` under `$x ?? d` but still`` |
|       - | 1241 | ``		 * throws `Using $this when not in object context` for `$this ?? d`, where`` |
|       - | 1242 | ``		 * `isset($this)` answers false in silence. One value each, so OP_LOAD can`` |
|       - | 1243 | `		 * tell the two silences apart. */` |
|     639 | 1244 | `		iP2 = 4;` |
| 2911184 | 1245 | `	}else if( iCompileFlag & EXPR_FLAG_RMW_LOAD ){` |
|       - | 1246 | ``		/* Warn-then-create: the read half of `$x++` / `$x .= ...` still needs a`` |
|       - | 1247 | `		 * writable slot, so it cannot use the read-only load above. */` |
|   87398 | 1248 | `		iP2 = 2;` |
| 2867112 | 1249 | `	}else if( iCompileFlag & EXPR_FLAG_DEFER_ARG ){` |
|       - | 1250 | ``		/* D1 deferred call argument. For a plain `$var` (iVv == 0, p3 holds the name)`` |
|       - | 1251 | `		 * emit the deferred load (iP1 stays 1 = no create, iP2 = 3): an undefined` |
|       - | 1252 | `		 * variable is left uncreated and silent, carrying a lazy-lvalue marker that` |
|       - | 1253 | `		 * OP_CALL resolves against the callee's by-ref flags. A variable-variable` |
|       - | 1254 | `		 * ($$x) computes its name on the stack (p3 == 0), so it cannot carry the` |
|       - | 1255 | `		 * marker — fall back to the historical eager create (iP1 = 0), which keeps` |
|       - | 1256 | `		 * its by-ref binding working exactly as before. */` |
|  660681 | 1257 | `		if( iVv == 0 ){` |
|  660675 | 1258 | `			iP2 = 3;` |
|  329841 | 1259 | `		}else{` |
|       9 | 1260 | `			iP1 = 0;` |
|       - | 1261 | `		}` |
|  329839 | 1262 | `	}` |
|       - | 1263 | `	/* Emit the load instruction(s). For a variable-variable ($$x, $$$x, ...) every` |
|       - | 1264 | `	 * load EXCEPT the final dereference resolves a NAME: a pure read that warns on an` |
|       - | 1265 | `	 * undefined name (php) and never creates it. Only the last load is the actual` |
|       - | 1266 | `	 * variable and carries the caller's write/create context (iP1). Emitting the` |
|       - | 1267 | ``	 * outer create-mode for the name loads silently invented $n in `$$n = 5` and`` |
|       - | 1268 | ``	 * skipped php's `Undefined variable $n` warning; a quiet outer (isset/empty)`` |
|       - | 1269 | `	 * still suppresses the name warning as php does. */` |
| 2926129 | 1270 | `	if( iVv > 0 ){` |
|      45 | 1271 | `		sxi32 iP2Name = (iCompileFlag & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY))` |
|       - | 1272 | ``			? 1 /* isset()/empty() suppress the name warning too; `??` (QUIET_VAR)`` |
|      20 | 1273 | `			     * does NOT — it warns the name and quiets only the target read. */ : 0;` |
|      45 | 1274 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,1/* read-only name */,iP2Name,p3,0);` |
|      47 | 1275 | `		while( iVv > 1 ){` |
|       3 | 1276 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,1/* read-only name */,iP2Name,0,0);` |
|       3 | 1277 | `			iVv--;` |
|       1 | 1278 | `		}` |
|       - | 1279 | `		/* Final dereference: the actual variable, in the caller's context. */` |
|      45 | 1280 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,iP2,0,0);` |
|      25 | 1281 | `	}else{` |
| 2926089 | 1282 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,iP2,p3,0);` |
|       - | 1283 | `	}` |
|       - | 1284 | `	/* Node successfully compiled */` |
| 2926129 | 1285 | `	return SXRET_OK;` |
| 1460982 | 1286 | `}` |
|       - | 1287 | `/*` |
|       - | 1288 | ` * Load a literal.` |
|       - | 1289 | ` */` |
| 1443998 | 1290 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       5 | 1291 | `{` |
| 1444003 | 1292 | `	SyToken *pToken = pGen->pIn;` |
|       - | 1293 | `	ph7_value *pObj;` |
|       - | 1294 | `	SyString *pStr;` |
|       - | 1295 | `	SyString sCanon;` |
|       - | 1296 | `	sxu32 nIdx;` |
|       - | 1297 | `	/* Extract token value */` |
| 1444003 | 1298 | `	pStr = &pToken->sData;` |
|       - | 1299 | `	/* php's MAGIC constants are case-insensitive like the rest of its reserved words —` |
|       - | 1300 | ``	 * `__line__`, `__Dir__` and `__CLASS__` are one constant each — but every one of them`` |
|       - | 1301 | `	 * is recognised by a BYTE-EXACT compare: the compile-time branches just below, and` |
|       - | 1302 | ``	 * `__CLASS__` through constant.c's (deliberately case-sensitive) constant table. Fold`` |
|       - | 1303 | `	 * the spelling to the canonical upper case here, once, so both mechanisms see it; any` |
|       - | 1304 | ``	 * other spelling used to reach the plain-literal path and raise `Undefined constant`` |
|       - | 1305 | ``	 * "__line__"`. Two of the branches below also read a single byte to tell a pair apart`` |
|       - | 1306 | ``	 * (`zString[2]` for __DIR__ vs __FILE__ and __METHOD__ vs __FUNCTION__), which only`` |
|       - | 1307 | `	 * works on the canonical form.` |
|       - | 1308 | `	 *` |
|       - | 1309 | `	 * User constants stay case-SENSITIVE (php) — this fold is limited to the eight names` |
|       - | 1310 | ``	 * below, and skips a member NAME, where `C::__LINE__` is an ordinary class constant. */`` |
| 1443998 | 1311 | `	if( (pToken->nType & PH7_TK_MEMBER_NAME) == 0 && pStr->nByte > 4` |
| 1313425 | 1312 | `		&& pStr->zString[0] == '_' && pStr->zString[1] == '_' ){` |
|       - | 1313 | `		static const char * const azMagic[] = {` |
|       - | 1314 | `			"__LINE__", "__FILE__", "__DIR__", "__FUNCTION__", "__CLASS__",` |
|       - | 1315 | `			"__METHOD__", "__NAMESPACE__", "__TRAIT__"` |
|       - | 1316 | `		};` |
|       - | 1317 | `		sxu32 i;` |
|  123217 | 1318 | `		for( i = 0 ; i < SX_ARRAYSIZE(azMagic) ; ++i ){` |
|  109747 | 1319 | `			sxu32 nMagic = SyStrlen(azMagic[i]);` |
|  109747 | 1320 | `			if( pStr->nByte == nMagic && SyStrnicmp(pStr->zString,azMagic[i],nMagic) == 0 ){` |
|     597 | 1321 | `				SyStringInitFromBuf(&sCanon,azMagic[i],nMagic);` |
|     597 | 1322 | `				pStr = &sCanon;` |
|     597 | 1323 | `				break;` |
|       - | 1324 | `			}` |
|   54507 | 1325 | `		}` |
|    7021 | 1326 | `	}` |
|       - | 1327 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first. A reserved` |
|       - | 1328 | `	 * word used as a member NAME (Enum::Null, C::Array, $o->list()) is a plain` |
|       - | 1329 | `	 * identifier — the parser flagged its token so the whole value-folding chain is` |
|       - | 1330 | `	 * skipped and it falls through to the ordinary string-literal emit below. */` |
| 1444003 | 1331 | `	if( pToken->nType & PH7_TK_MEMBER_NAME ){` |
|       - | 1332 | `		/* fall through to the plain-string literal path */` |
| 1426728 | 1333 | `	}else if( pStr->nByte == sizeof("NULL") - 1 ){` |
|   91086 | 1334 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - | 1335 | `			/* NULL constant are always indexed at 0 */` |
|   17481 | 1336 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   17481 | 1337 | `			return SXRET_OK;` |
|   73610 | 1338 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - | 1339 | `			/* TRUE constant are always indexed at 1 */` |
|   14690 | 1340 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|   14690 | 1341 | `			return SXRET_OK;` |
|       5 | 1342 | `		}` |
| 1474881 | 1343 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|  253775 | 1344 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - | 1345 | `			/* FALSE constant are always indexed at 2 */` |
|  178594 | 1346 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|  178594 | 1347 | `			return SXRET_OK;` |
| 1139816 | 1348 | `	}else if( pStr->nByte == sizeof("__COMPILER_HALT_OFFSET__") - 1` |
|  568881 | 1349 | `		&& pGen->bHaltSeen` |
|     160 | 1350 | `		&& SyStrnicmp(pStr->zString,"__COMPILER_HALT_OFFSET__",` |
|       6 | 1351 | `			sizeof("__COMPILER_HALT_OFFSET__")-1) == 0 ){` |
|       - | 1352 | `			/*` |
|       - | 1353 | ``			 * php's `__COMPILER_HALT_OFFSET__`: the byte just past the`` |
|       - | 1354 | ``			 * `__halt_compiler();` statement's semicolon, resolved at COMPILE time`` |
|       - | 1355 | ``			 * and only in a file that HAS one. That is why `defined()` answers`` |
|       - | 1356 | `			 * false for it (php registers it under a mangled per-file name, and` |
|       - | 1357 | `			 * this emits a literal instead) and why a file with no halt reaches` |
|       - | 1358 | ``			 * the ordinary constant path and raises php's `Undefined constant`.`` |
|       - | 1359 | `			 */` |
|      13 | 1360 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      13 | 1361 | `			if( pObj == 0 ){` |
|     ! 0 | 1362 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1363 | `				return SXERR_ABORT;` |
|       - | 1364 | `			}` |
|      13 | 1365 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,(sxi64)pGen->nHaltOffset);` |
|      13 | 1366 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      13 | 1367 | `			return SXRET_OK;` |
| 1174361 | 1368 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   68963 | 1369 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - | 1370 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|      22 | 1371 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      22 | 1372 | `			if( pObj == 0 ){` |
|     ! 0 | 1373 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1374 | `				return SXERR_ABORT;` |
|       - | 1375 | `			}` |
|      22 | 1376 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - | 1377 | `			/* Emit the load constant instruction */` |
|      22 | 1378 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      22 | 1379 | `			return SXRET_OK;` |
| 1174326 | 1380 | `	}else if( (pStr->nByte == sizeof("__FILE__") - 1 &&` |
|  104822 | 1381 | `		SyMemcmp(pStr->zString,"__FILE__",sizeof("__FILE__")-1) == 0) \|\|` |
| 1175494 | 1382 | `		(pStr->nByte == sizeof("__DIR__") - 1 &&` |
|   71902 | 1383 | `		SyMemcmp(pStr->zString,"__DIR__",sizeof("__DIR__")-1) == 0) ){` |
|       - | 1384 | `			/* __FILE__ / __DIR__ are magic constants resolved at COMPILE time to the` |
|       - | 1385 | `			 * file being compiled (where the token is written), NOT the runtime` |
|       - | 1386 | `			 * execution file. A function defined in a.php reporting __FILE__ must say` |
|       - | 1387 | `			 * a.php even when called from b.php — php semantics, and what Composer's` |
|       - | 1388 | ``			 * autoloader (loadClassLoader's `require __DIR__ . '/ClassLoader.php'`)`` |
|       - | 1389 | `			 * relies on. The runtime-constant path returned the caller's file. */` |
|     441 | 1390 | `			int bDir = (pStr->zString[2] == 'D'); /* __DIR__ vs __FILE__ */` |
|     441 | 1391 | `			SyString *pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     441 | 1392 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     441 | 1393 | `			if( pObj == 0 ){` |
|     ! 0 | 1394 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1395 | `				return SXERR_ABORT;` |
|       - | 1396 | `			}` |
|     441 | 1397 | `			if( pFile && pFile->nByte > 0 ){` |
|     441 | 1398 | `				if( bDir ){` |
|       - | 1399 | `					const char *zDir;` |
|       - | 1400 | `					int nLen;` |
|       - | 1401 | `					SyString sDir;` |
|     241 | 1402 | `					zDir = PH7_ExtractDirName(pFile->zString,(int)pFile->nByte,&nLen);` |
|     241 | 1403 | `					SyStringInitFromBuf(&sDir,zDir,nLen);` |
|     241 | 1404 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&sDir);` |
|     123 | 1405 | `				}else{` |
|     205 | 1406 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,pFile);` |
|       - | 1407 | `				}` |
|     222 | 1408 | `			}else{` |
|       - | 1409 | `				SyString sMem;` |
|     ! 0 | 1410 | `				SyStringInitFromBuf(&sMem,":MEMORY:",sizeof(":MEMORY:")-1);` |
|     ! 0 | 1411 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sMem);` |
|       - | 1412 | `			}` |
|     441 | 1413 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     441 | 1414 | `			return SXRET_OK;` |
| 1161077 | 1415 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   43377 | 1416 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|       - | 1417 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|      17 | 1418 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      17 | 1419 | `			if( pObj == 0 ){` |
|     ! 0 | 1420 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1421 | `				return SXERR_ABORT;` |
|       - | 1422 | `			}` |
|      17 | 1423 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - | 1424 | `				SyString sNs;` |
|      11 | 1425 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      11 | 1426 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|       7 | 1427 | `			}else{` |
|       7 | 1428 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - | 1429 | `			}` |
|      17 | 1430 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      17 | 1431 | `			return SXRET_OK;` |
| 1166937 | 1432 | `	}else if( pStr->nByte == sizeof("__TRAIT__") - 1 &&` |
|   55104 | 1433 | `		SyMemcmp(pStr->zString,"__TRAIT__",sizeof("__TRAIT__")-1) == 0 ){` |
|       - | 1434 | `			/* __TRAIT__ magic constant: the name of the trait whose SOURCE lexically` |
|       - | 1435 | `			 * encloses this token. php resolves it at compile time, and it is shared` |
|       - | 1436 | `			 * across every using class because a trait method body compiles ONCE with` |
|       - | 1437 | `			 * the trait as its owner (PH7_ClassUseTrait adopts the same method pointer).` |
|       - | 1438 | `			 * Unlike __FUNCTION__/__METHOD__ (nearest function), __TRAIT__ is LEXICAL:` |
|       - | 1439 | `			 * closures and arrow-fns are TRANSPARENT (a closure inside a trait method still` |
|       - | 1440 | `			 * yields the trait), so we skip them and keep walking outward — but a class` |
|       - | 1441 | `			 * method or a plain function is an OPAQUE lexical boundary that fixes the answer.` |
|       - | 1442 | `			 * An anonymous class defined inside a trait method is a fresh scope, so` |
|       - | 1443 | `			 * __TRAIT__ is "" there, not the enclosing trait. "" outside any trait (global` |
|       - | 1444 | `			 * scope, plain functions, non-trait methods) — php renders it the empty string,` |
|       - | 1445 | `			 * not NULL. */` |
|      56 | 1446 | `			ph7_class *pTrait = 0;` |
|      56 | 1447 | `			if( pGen->iInMemberDefault > 0 ){` |
|       - | 1448 | `				/* A property/parameter DEFAULT is a const-expression that belongs to the` |
|       - | 1449 | `				 * class whose body is being compiled (pCurClass), never to a lexically-` |
|       - | 1450 | `				 * enclosing method. Read pCurClass directly — the block chain has no func` |
|       - | 1451 | `				 * block for the default and would leak into the enclosing function (an` |
|       - | 1452 | `				 * anonymous class's default inside a trait method is the anon's scope, "").*/` |
|      13 | 1453 | `				if( pGen->pCurClass && (pGen->pCurClass->iFlags & PH7_CLASS_TRAIT) ){` |
|       5 | 1454 | `					pTrait = pGen->pCurClass;` |
|       2 | 1455 | `				}` |
|       7 | 1456 | `			}else{` |
|      44 | 1457 | `				GenBlock *pBlock = pGen->pCurrent;` |
|     106 | 1458 | `				while( pBlock ){` |
|     102 | 1459 | `					if( pBlock->iFlags & GEN_BLOCK_FUNC ){` |
|      55 | 1460 | `						ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      55 | 1461 | `						if( pFunc == 0 ){` |
|       - | 1462 | `							/* A SYNTHETIC function block carries no ph7_vm_func — e.g. the` |
|       - | 1463 | `							 * per-arm throw-fixup block GenStateCompileMatchSubExpr enters to` |
|       - | 1464 | `							 * host a match() expression. It is not a real lexical scope` |
|       - | 1465 | `							 * boundary, so stay transparent and keep walking outward. */` |
|       6 | 1466 | `							pBlock = pBlock->pParent;` |
|       6 | 1467 | `							continue;` |
|       - | 1468 | `						}` |
|      51 | 1469 | `						if( pFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|       - | 1470 | `							/* A class method (of any class, including an anonymous one) is an` |
|       - | 1471 | `							 * OPAQUE scope boundary and fixes the answer: a trait method yields` |
|       - | 1472 | `							 * its trait, any other class's method yields "". Tested BEFORE the` |
|       - | 1473 | `							 * closure flags so a static method is never mistaken for transparent. */` |
|      34 | 1474 | `							if( pFunc->pUserData` |
|      37 | 1475 | `								&& (((ph7_class *)pFunc->pUserData)->iFlags & PH7_CLASS_TRAIT) ){` |
|      31 | 1476 | `								pTrait = (ph7_class *)pFunc->pUserData;` |
|      14 | 1477 | `							}` |
|      37 | 1478 | `							break;` |
|       - | 1479 | `						}` |
|      15 | 1480 | `						if( pFunc->iFlags & (VM_FUNC_CLOSURE\|VM_FUNC_ARROW\|VM_FUNC_STATIC_CL) ){` |
|       - | 1481 | `							/* Closure / arrow fn (VM_FUNC_CLOSURE is only set when the closure` |
|       - | 1482 | `							 * captures, so a capture-less static closure carries only` |
|       - | 1483 | `							 * VM_FUNC_STATIC_CL — include it). Transparent: keep walking outward. */` |
|      13 | 1484 | `							pBlock = pBlock->pParent;` |
|      13 | 1485 | `							continue;` |
|       - | 1486 | `						}` |
|       - | 1487 | `						/* A plain named function is an opaque boundary: __TRAIT__ is "". */` |
|       3 | 1488 | `						break;` |
|       - | 1489 | `					}` |
|      50 | 1490 | `					pBlock = pBlock->pParent;` |
|       4 | 1491 | `				}` |
|       - | 1492 | `			}` |
|      56 | 1493 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      56 | 1494 | `			if( pObj == 0 ){` |
|     ! 0 | 1495 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1496 | `				return SXERR_ABORT;` |
|       - | 1497 | `			}` |
|      56 | 1498 | `			if( pTrait ){` |
|      36 | 1499 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&pTrait->sName);` |
|      20 | 1500 | `			}else{` |
|      22 | 1501 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0); /* empty string */` |
|       - | 1502 | `			}` |
|      56 | 1503 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      56 | 1504 | `			return SXRET_OK;` |
| 1174790 | 1505 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|  132607 | 1506 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
| 1201208 | 1507 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|  123719 | 1508 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      62 | 1509 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - | 1510 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|       - | 1511 | `			/* Skip SYNTHETIC function blocks (GEN_BLOCK_FUNC with no ph7_vm_func in` |
|       - | 1512 | `			 * pUserData — e.g. the per-arm throw-fixup block a match() expression enters):` |
|       - | 1513 | `			 * they are not real function scopes. Without this a __FUNCTION__/__METHOD__` |
|       - | 1514 | `			 * inside a match arm reached a NULL pUserData and dereferenced it (compile-time` |
|       - | 1515 | `			 * crash); php resolves to the enclosing real function, which the walk now finds. */` |
|     153 | 1516 | `			while( pBlock && ((pBlock->iFlags & GEN_BLOCK_FUNC) == 0 \|\| pBlock->pUserData == 0) ){` |
|       - | 1517 | `				/* Point to the upper block */` |
|      66 | 1518 | `				pBlock = pBlock->pParent;` |
|       4 | 1519 | `			}` |
|      62 | 1520 | `			if( pBlock == 0 ){` |
|       - | 1521 | `				/* Called in the global scope,load NULL */` |
|       5 | 1522 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 | 1523 | `			}else{` |
|       - | 1524 | `				/* Extract the target function/method */` |
|      58 | 1525 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      58 | 1526 | `				int bMethod = (pStr->zString[2] == 'M'); /* __METHOD__ vs __FUNCTION__ */` |
|      58 | 1527 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      58 | 1528 | `				if( pObj == 0 ){` |
|     ! 0 | 1529 | `					PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1530 | `					return SXERR_ABORT;` |
|       - | 1531 | `				}` |
|       - | 1532 | `				/*` |
|       - | 1533 | `				 * __METHOD__ is the QUALIFIED name: "C::m" inside a method, and the plain` |
|       - | 1534 | `				 * function name inside a plain function (php does not answer "" there —` |
|       - | 1535 | `				 * PH7 loaded NULL, so __METHOD__ was empty in every free function and` |
|       - | 1536 | `				 * unqualified in every method).` |
|       - | 1537 | `				 */` |
|       - | 1538 | ``				/* A property hook answers php's `$p::get`, never the method name`` |
|       - | 1539 | ``				 * PHL synthesizes for it — so __METHOD__ reads `C::$p::get`, the`` |
|       - | 1540 | `				 * same rendering the runtime diagnostics use. */` |
|       - | 1541 | `				{` |
|       - | 1542 | `					SyString sProp,sSelf;` |
|       - | 1543 | `					const char *zKind;` |
|       - | 1544 | `					SyBlob sQual;` |
|       - | 1545 | `					SyString sOut;` |
|      58 | 1546 | `					int bHook = PH7_VmHookSplitName(&pFunc->sName,&sProp,&zKind);` |
|      58 | 1547 | `					SyBlobInit(&sQual,&pGen->pVm->sAllocator);` |
|      58 | 1548 | `					if( bHook ){` |
|       7 | 1549 | `						SyBlobFormat(&sQual,"$%z::%s",&sProp,zKind);` |
|       7 | 1550 | `						SyStringInitFromBuf(&sSelf,SyBlobData(&sQual),SyBlobLength(&sQual));` |
|      55 | 1551 | `					}else if( SyStringLength(&pFunc->sClosureName) > 0 ){` |
|       - | 1552 | ``						/* A closure answers php's `{closure:...}` name, never the`` |
|       - | 1553 | `						 * synthesized lookup key — and __METHOD__ answers the SAME text` |
|       - | 1554 | `						 * (the name already carries the declaring class), so the` |
|       - | 1555 | `						 * qualification below must not run for it. */` |
|      15 | 1556 | `						sSelf = pFunc->sClosureName;` |
|      15 | 1557 | `						bMethod = 0;` |
|       8 | 1558 | `					}else{` |
|      37 | 1559 | `						sSelf = pFunc->sName;` |
|       - | 1560 | `					}` |
|      66 | 1561 | `					if( bMethod && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|      19 | 1562 | `						SyString *pCls = &((ph7_class *)pFunc->pUserData)->sName;` |
|       - | 1563 | `						SyBlob sFull;` |
|      19 | 1564 | `						SyBlobInit(&sFull,&pGen->pVm->sAllocator);` |
|      19 | 1565 | `						SyBlobFormat(&sFull,"%z::%z",pCls,&sSelf);` |
|      19 | 1566 | `						SyStringInitFromBuf(&sOut,SyBlobData(&sFull),SyBlobLength(&sFull));` |
|      19 | 1567 | `						PH7_MemObjInitFromString(pGen->pVm,pObj,&sOut);` |
|      19 | 1568 | `						SyBlobRelease(&sFull);` |
|      11 | 1569 | `					}else{` |
|      42 | 1570 | `						PH7_MemObjInitFromString(pGen->pVm,pObj,&sSelf);` |
|       - | 1571 | `					}` |
|      58 | 1572 | `					SyBlobRelease(&sQual);` |
|       - | 1573 | `				}` |
|       - | 1574 | `				/* Emit the load constant instruction */` |
|      58 | 1575 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 1576 | `			}` |
|      62 | 1577 | `			return SXRET_OK;` |
|       - | 1578 | `	}` |
|       - | 1579 | ``	/* php keywords are CASE-INSENSITIVE (`SELF::C`, `Parent::m()`, `new STATIC`,`` |
|       - | 1580 | ``	 * `ISSET($x)`), but a few of them reach the engine as THIS literal and are matched`` |
|       - | 1581 | `	 * there BYTE-EXACTLY: the scope keywords against "self"/"parent"/"static" (OP_MEMBER,` |
|       - | 1582 | `	 * OP_NEW, the FCC scope resolver, the error formatter), and isset/empty/eval as the` |
|       - | 1583 | `	 * name of the host function their call dispatches to. Emit the canonical lower-case` |
|       - | 1584 | `	 * spelling for exactly those so the source's case never reaches the match — PH7` |
|       - | 1585 | ``	 * emitted the raw text, so `SELF::C` looked for a class literally named "SELF" and`` |
|       - | 1586 | ``	 * `ISSET($x)` for a function named "ISSET".`` |
|       - | 1587 | `	 *` |
|       - | 1588 | `	 * Every OTHER keyword literal keeps its source case on purpose: it is a CONSTANT` |
|       - | 1589 | ``	 * read (`define('OBJECT',1); echo OBJECT;` — PHL's keyword table covers type names`` |
|       - | 1590 | `	 * php's lexer does not reserve), and php constants are case-SENSITIVE. So is a` |
|       - | 1591 | ``	 * keyword used as a member NAME, which the parser flags: `class C { const STATIC = 5; }`` |
|       - | 1592 | ``	 * echo C::STATIC;` names the constant "STATIC", and folding it looked for "static". */`` |
| 1232661 | 1593 | `	if( (pToken->nType & PH7_TK_KEYWORD) && (pToken->nType & PH7_TK_MEMBER_NAME) == 0 ){` |
|   15659 | 1594 | `		sxu32 nKeyID = (sxu32)SX_PTR_TO_INT(pToken->pUserData);` |
|   15659 | 1595 | `		const char *zCanon = 0;` |
|   15659 | 1596 | `		if( nKeyID == PH7_TKWRD_SELF ){` |
|     447 | 1597 | `			zCanon = "self";` |
|   15438 | 1598 | `		}else if( nKeyID == PH7_TKWRD_PARENT ){` |
|     130 | 1599 | `			zCanon = "parent";` |
|   15154 | 1600 | `		}else if( nKeyID == PH7_TKWRD_STATIC ){` |
|     120 | 1601 | `			zCanon = "static";` |
|   15033 | 1602 | `		}else if( nKeyID == PH7_TKWRD_ISSET ){` |
|   14569 | 1603 | `			zCanon = "isset";` |
|    7684 | 1604 | `		}else if( nKeyID == PH7_TKWRD_EMPTY ){` |
|     209 | 1605 | `			zCanon = "empty";` |
|     309 | 1606 | `		}else if( nKeyID == PH7_TKWRD_EVAL ){` |
|     171 | 1607 | `			zCanon = "eval";` |
|      83 | 1608 | `		}` |
|   15659 | 1609 | `		if( zCanon ){` |
|   15623 | 1610 | `			SyStringInitFromBuf(&sCanon,zCanon,SyStrlen(zCanon));` |
|   15623 | 1611 | `			pStr = &sCanon;` |
|    7800 | 1612 | `		}` |
|    7818 | 1613 | `	}` |
|       - | 1614 | `	/* Query literal table */` |
| 1232661 | 1615 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - | 1616 | `		ph7_value *pLitObj;` |
|       - | 1617 | `		/* Unknown literal,install it in the literal table */` |
|  462739 | 1618 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  462739 | 1619 | `		if( pLitObj == 0 ){` |
|     ! 0 | 1620 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 1621 | `			return SXERR_ABORT;` |
|       - | 1622 | `		}` |
|  462739 | 1623 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,pStr);` |
|  462739 | 1624 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  230944 | 1625 | `	}` |
|       - | 1626 | `	/* Emit the load constant instruction.` |
|       - | 1627 | `	 *` |
|       - | 1628 | `	 * php resolves an UNQUALIFIED constant against the namespace its SOURCE sits in,` |
|       - | 1629 | `	 * decided at COMPILE time — so a function keeps its own namespace when called from` |
|       - | 1630 | ``	 * another one — and in a fixed order: a `use const` import first (and an import has`` |
|       - | 1631 | ``	 * NO global fallback), else `current-namespace\NAME`, else the global `NAME`. The`` |
|       - | 1632 | `	 * candidate that order picks is resolved here and travels in the instruction's p3;` |
|       - | 1633 | `	 * the VM's own lookup of the bare literal is then just the global step, and the` |
|       - | 1634 | `	 * "Undefined constant" message names the candidate, as php does.` |
|       - | 1635 | `	 *` |
|       - | 1636 | `	 * Only a name that could BE a constant needs one: a keyword literal never is (the` |
|       - | 1637 | `	 * scope keywords and isset/empty/eval reach the OO and call handlers by this same` |
|       - | 1638 | `	 * literal), and a call/new site clears PH7_LOADC_EXPAND before the constant path` |
|       - | 1639 | `	 * can ever run. */` |
|       - | 1640 | `	{` |
| 1232661 | 1641 | `		sxi32 iLoadFlags = PH7_LOADC_EXPAND;` |
| 1232661 | 1642 | `		char *zCand = 0;` |
| 1232661 | 1643 | `		if( (pToken->nType & PH7_TK_KEYWORD) == 0 ){` |
| 1823643 | 1644 | `			SyHashEntry *pImport = SyHashGet(&pGen->hUseConstImports,` |
| 1216592 | 1645 | `				(const void *)pStr->zString,pStr->nByte);` |
| 1216597 | 1646 | `			if( pImport ){` |
|      33 | 1647 | `				const char *zFQN = (const char *)pImport->pUserData;` |
|      33 | 1648 | `				zCand = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFQN,SyStrlen(zFQN));` |
|      33 | 1649 | `				iLoadFlags \|= PH7_LOADC_NOGLOBAL;` |
| 1216583 | 1650 | `			}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - | 1651 | `				SyBlob sCand;` |
|     733 | 1652 | `				SyBlobInit(&sCand,&pGen->pVm->sAllocator);` |
|     733 | 1653 | `				SyBlobAppend(&sCand,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     733 | 1654 | `				SyBlobAppend(&sCand,"\\",1);` |
|     733 | 1655 | `				SyBlobAppend(&sCand,pStr->zString,pStr->nByte);` |
|    1097 | 1656 | `				zCand = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     728 | 1657 | `					(const char *)SyBlobData(&sCand),SyBlobLength(&sCand));` |
|     733 | 1658 | `				SyBlobRelease(&sCand);` |
|     364 | 1659 | `			}` |
|  607046 | 1660 | `		}` |
| 1232661 | 1661 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,iLoadFlags,nIdx,zCand,0);` |
|       - | 1662 | `	}` |
| 1232661 | 1663 | `	return SXRET_OK;` |
|  720583 | 1664 | `}` |
|       - | 1665 | `/*` |
|       - | 1666 | ` * Resolve a namespace path or simply load a literal.` |
|       - | 1667 | ` * If the token stream contains namespace separators (backslashes),` |
|       - | 1668 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - | 1669 | ` * Otherwise, load the simple literal directly.` |
|       - | 1670 | ` */` |
| 1444541 | 1671 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       5 | 1672 | `{` |
|       - | 1673 | `	sxi32 rc;` |
| 1444546 | 1674 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 1675 | `		return SXRET_OK;` |
|       - | 1676 | `	}` |
|       - | 1677 | `	/* Check if this is a multi-token namespace path */` |
| 1444546 | 1678 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - | 1679 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|     548 | 1680 | `		SyBlob *pWorker = &pGen->sWorker;` |
|     548 | 1681 | `		int isAbsolute = 0;` |
|     548 | 1682 | `		SyBlobReset(pWorker);` |
|       - | 1683 | `		/* Check for leading backslash (absolute path) */` |
|     548 | 1684 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|     213 | 1685 | `			isAbsolute = 1;` |
|     213 | 1686 | `			pGen->pIn++; /* Skip leading backslash */` |
|     104 | 1687 | `		}` |
|       - | 1688 | `		/* Collect the raw path (no prefix yet) so its FIRST segment can be resolved` |
|       - | 1689 | ``		 * against use-imports below — php resolves `A\B\C` by mapping the leading`` |
|       - | 1690 | ``		 * `A` through the imports (`use X\A;` makes it `X\A\B\C`), and only prepends`` |
|       - | 1691 | ``		 * the current namespace when `A` matches no import. Blindly prefixing the`` |
|       - | 1692 | ``		 * namespace here produced e.g. `Ns\Ev\Bus` for `use ...\Event as Ev; Ev\Bus`. */`` |
|       - | 1693 | `		{` |
|       - | 1694 | `			SyBlob sRaw;` |
|     548 | 1695 | `			SyBlobInit(&sRaw,&pGen->pVm->sAllocator);` |
|       - | 1696 | ``			/* `namespace\X` spells the CURRENT namespace and is FULLY QUALIFIED from`` |
|       - | 1697 | `			 * there: no import ever applies to it, and the namespace is already in. */` |
|     548 | 1698 | `			if( !isAbsolute && GenStateNsRelPrefix(pGen,&pGen->pIn,pGen->pEnd,&sRaw) ){` |
|      68 | 1699 | `				isAbsolute = 1;` |
|      33 | 1700 | `			}` |
|    1434 | 1701 | `			while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|    1434 | 1702 | `				if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|     448 | 1703 | `					SyBlobAppend(&sRaw,"\\",1);` |
|     226 | 1704 | `				}else{` |
|     991 | 1705 | `					SyBlobAppend(&sRaw,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - | 1706 | `				}` |
|    1434 | 1707 | `				if( pGen->pIn == &pGen->pEnd[-1] ){` |
|     548 | 1708 | `					pGen->pIn++;` |
|     548 | 1709 | `					break;` |
|       - | 1710 | `				}` |
|     891 | 1711 | `				pGen->pIn++;` |
|       5 | 1712 | `			}` |
|     548 | 1713 | `			if( isAbsolute ){` |
|     279 | 1714 | `				SyBlobAppend(pWorker,SyBlobData(&sRaw),SyBlobLength(&sRaw)); /* FQN as written */` |
|     142 | 1715 | `			}else{` |
|     274 | 1716 | `				const char *zRaw = (const char *)SyBlobData(&sRaw);` |
|     274 | 1717 | `				sxu32 nRaw = SyBlobLength(&sRaw);` |
|     274 | 1718 | `				sxu32 nFirst = 0;` |
|       - | 1719 | `				SyHashEntry *pNsImp;` |
|    1485 | 1720 | `				while( nFirst < nRaw && zRaw[nFirst] != '\\' ){ nFirst++; }` |
|     274 | 1721 | `				pNsImp = SyHashGet(&pGen->hUseImports,(const void *)zRaw,nFirst);` |
|     274 | 1722 | `				if( pNsImp ){` |
|       - | 1723 | `					/* Leading segment is an imported alias: substitute its FQN. */` |
|      47 | 1724 | `					const char *zFQN = (const char *)pNsImp->pUserData;` |
|      47 | 1725 | `					SyBlobAppend(pWorker,zFQN,SyStrlen(zFQN));` |
|      47 | 1726 | `					SyBlobAppend(pWorker,zRaw + nFirst,nRaw - nFirst);` |
|     252 | 1727 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      22 | 1728 | `					SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      22 | 1729 | `					SyBlobAppend(pWorker,"\\",1);` |
|      22 | 1730 | `					SyBlobAppend(pWorker,zRaw,nRaw);` |
|      13 | 1731 | `				}else{` |
|     211 | 1732 | `					SyBlobAppend(pWorker,zRaw,nRaw); /* global scope, no import */` |
|       - | 1733 | `				}` |
|       - | 1734 | `			}` |
|     548 | 1735 | `			SyBlobRelease(&sRaw);` |
|       - | 1736 | `		}` |
|     548 | 1737 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - | 1738 | `			ph7_value *pObj;` |
|       - | 1739 | `			SyString sPath;` |
|       - | 1740 | `			sxu32 nIdx;` |
|     548 | 1741 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - | 1742 | `			/*` |
|       - | 1743 | ``			 * `\true`, `\false` and `\null` are php's three reserved literals`` |
|       - | 1744 | `			 * written the way a code GENERATOR writes them -- fully qualified, so` |
|       - | 1745 | ``			 * that no `use` or namespace can shadow them. php resolves each to the`` |
|       - | 1746 | `			 * literal itself; this engine looked the name up in the constant table` |
|       - | 1747 | ``			 * and answered `Undefined constant "true"`. nikic/php-parser emits`` |
|       - | 1748 | `			 * every one of its booleans that way, which is what phpunit.phar dies` |
|       - | 1749 | ``			 * on. Only the GLOBAL spelling counts: `\Ns\true` is an ordinary`` |
|       - | 1750 | ``			 * constant, and so is a bare `true` (handled by the literal path,`` |
|       - | 1751 | `			 * which folds it already).` |
|       - | 1752 | `			 */` |
|     548 | 1753 | `			if( isAbsolute && SyByteFind(sPath.zString,sPath.nByte,'\\',0) != SXRET_OK ){` |
|     146 | 1754 | `				if( sPath.nByte == sizeof("null")-1` |
|      79 | 1755 | `				 && SyStrnicmp(sPath.zString,"null",sizeof("null")-1) == 0 ){` |
|     ! 0 | 1756 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|     ! 0 | 1757 | `					return SXRET_OK;` |
|       - | 1758 | `				}` |
|     146 | 1759 | `				if( sPath.nByte == sizeof("true")-1` |
|      79 | 1760 | `				 && SyStrnicmp(sPath.zString,"true",sizeof("true")-1) == 0 ){` |
|     ! 0 | 1761 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     ! 0 | 1762 | `					return SXRET_OK;` |
|       - | 1763 | `				}` |
|     146 | 1764 | `				if( sPath.nByte == sizeof("false")-1` |
|      79 | 1765 | `				 && SyStrnicmp(sPath.zString,"false",sizeof("false")-1) == 0 ){` |
|     ! 0 | 1766 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|     ! 0 | 1767 | `					return SXRET_OK;` |
|       - | 1768 | `				}` |
|      73 | 1769 | `			}` |
|       - | 1770 | `			/* Install in the literal table */` |
|     548 | 1771 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|     170 | 1772 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     170 | 1773 | `				if( pObj == 0 ){` |
|     ! 0 | 1774 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 1775 | `					return SXERR_ABORT;` |
|       - | 1776 | `				}` |
|     170 | 1777 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|     170 | 1778 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      82 | 1779 | `			}` |
|       - | 1780 | `			/* Emit the load constant instruction.` |
|       - | 1781 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|       - | 1782 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|     819 | 1783 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|     271 | 1784 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|     271 | 1785 | `				nIdx,0,0);` |
|     548 | 1786 | `			return SXRET_OK;` |
|       - | 1787 | `		}` |
|     ! 0 | 1788 | `	}` |
|       - | 1789 | `	/* Single-token literal: load directly */` |
| 1444003 | 1790 | `	rc = GenStateLoadLiteral(&(*pGen));` |
| 1444003 | 1791 | `	return rc;` |
|  720854 | 1792 | `}` |
|       - | 1793 | `/*` |
|       - | 1794 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - | 1795 | ` */` |
|       - | 1796 | `/*` |
|       - | 1797 | `` * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of`` |
|       - | 1798 | `` * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument`` |
|       - | 1799 | ``  * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...` `` |
|       - | 1800 | ` * appeared outside a call argument list — a syntax error (PHP rejects it likewise).` |
|       - | 1801 | ` */` |
|     ! 0 | 1802 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|     ! 0 | 1803 | `{` |
|     ! 0 | 1804 | `	SXUNUSED(iCompileFlag);` |
|     ! 0 | 1805 | `	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|       - | 1806 | `		"Cannot use the first-class callable syntax '...' here");` |
|     ! 0 | 1807 | `	return SXERR_SYNTAX;` |
|     ! 0 | 1808 | `}` |
| 1444541 | 1809 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 | 1810 | `{` |
|       - | 1811 | `	sxi32 rc;` |
| 1444546 | 1812 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
| 1444546 | 1813 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1814 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1815 | `		return rc;` |
|       - | 1816 | `	}` |
|       - | 1817 | `	/* Node successfully compiled */` |
| 1444546 | 1818 | `	return SXRET_OK;` |
|  720854 | 1819 | `}` |
|       - | 1820 |  |
