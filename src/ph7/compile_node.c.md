# src/ph7/compile_node.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 907/1044 lines (86.88%)

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
|   18269 |   37 | `static void GenStateClosureName(ph7_gen_state *pGen,sxu32 nLine)` |
|       5 |   38 | `{` |
|   18274 |   39 | `	ph7_vm_func *pOuter = 0;` |
|   18274 |   40 | `	GenBlock *pBlock = pGen->pCurrent;` |
|       - |   41 | `	SyBlob sName;` |
|       - |   42 | `	char *zDup;` |
|       - |   43 | `	/* Innermost REAL function block: a synthetic one (a match() arm's throw-fixup` |
|       - |   44 | `	 * block) carries no ph7_vm_func and is not a scope. */` |
|   38475 |   45 | `	while( pBlock ){` |
|   21706 |   46 | `		if( (pBlock->iFlags & GEN_BLOCK_FUNC) && pBlock->pUserData ){` |
|    1505 |   47 | `			pOuter = (ph7_vm_func *)pBlock->pUserData;` |
|    1505 |   48 | `			break;` |
|       - |   49 | `		}` |
|   20206 |   50 | `		pBlock = pBlock->pParent;` |
|       5 |   51 | `	}` |
|   18274 |   52 | `	SyStringInitFromBuf(&pGen->sPendingClosureName,0,0);` |
|   18274 |   53 | `	SyStringInitFromBuf(&pGen->sPendingClosureScope,0,0);` |
|   18274 |   54 | `	SyBlobInit(&sName,&pGen->pVm->sAllocator);` |
|   18274 |   55 | `	SyBlobAppend(&sName,"{closure:",sizeof("{closure:")-1);` |
|   18274 |   56 | `	if( pOuter == 0 ){` |
|       - |   57 | `		/* Top level: php writes the compiled file's path (empty when there is none —` |
|       - |   58 | `		 * an eval()/direct-API compile — which is php's "{closure::LINE}" there). */` |
|   16774 |   59 | `		SyString *pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|   16774 |   60 | `		if( pFile && SyStringLength(pFile) > 0 ){` |
|   16774 |   61 | `			SyBlobAppend(&sName,SyStringData(pFile),SyStringLength(pFile));` |
|    8275 |   62 | `		}` |
|    9775 |   63 | `	}else if( SyStringLength(&pOuter->sClosureName) > 0 ){` |
|       - |   64 | `		/* Enclosing closure: its whole name, no parens and no class -- but php's` |
|       - |   65 | `		 * SCOPE is inherited, so a closure written inside a closure inside a method` |
|       - |   66 | `		 * still belongs to that class. */` |
|     956 |   67 | `		SyBlobAppend(&sName,SyStringData(&pOuter->sClosureName),` |
|     317 |   68 | `			SyStringLength(&pOuter->sClosureName));` |
|     639 |   69 | `		pGen->sPendingClosureScope = pOuter->sClosureScope;` |
|     322 |   70 | `	}else{` |
|     871 |   71 | `		if( (pOuter->iFlags & VM_FUNC_CLASS_METHOD) && pOuter->pUserData ){` |
|     475 |   72 | `			SyString *pCls = &((ph7_class *)pOuter->pUserData)->sName;` |
|     475 |   73 | `			SyBlobAppend(&sName,SyStringData(pCls),SyStringLength(pCls));` |
|     475 |   74 | `			SyBlobAppend(&sName,"::",2);` |
|     475 |   75 | `			pGen->sPendingClosureScope = *pCls;` |
|     235 |   76 | `		}` |
|     871 |   77 | `		SyBlobAppend(&sName,SyStringData(&pOuter->sName),SyStringLength(&pOuter->sName));` |
|     871 |   78 | `		SyBlobAppend(&sName,"()",2);` |
|       - |   79 | `	}` |
|   18274 |   80 | `	SyBlobFormat(&sName,":%u}",nLine);` |
|   27293 |   81 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|   18269 |   82 | `		(const char *)SyBlobData(&sName),(sxu32)SyBlobLength(&sName));` |
|   18274 |   83 | `	if( zDup ){` |
|   18274 |   84 | `		SyStringInitFromBuf(&pGen->sPendingClosureName,zDup,(sxu32)SyBlobLength(&sName));` |
|    9019 |   85 | `	}` |
|   18274 |   86 | `	SyBlobRelease(&sName);` |
|   18274 |   87 | `}` |
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
|    7743 |  108 | `PH7_PRIVATE sxi32 PH7_CompileAnnonFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  109 | `{` |
|    7748 |  110 | `	ph7_vm_func *pAnnonFunc = 0; /* Annonymous function body */` |
|       - |  111 | `	char zName[512];         /* Unique lambda name */` |
|       - |  112 | `	static int iCnt = 1;     /* There is no worry about thread-safety here,because only` |
|       - |  113 | `							  * one thread is allowed to compile the script.` |
|       - |  114 | `						      */` |
|       - |  115 | `	SyString sName;` |
|    7748 |  116 | ``	SyToken *pTokKw = pGen->pIn; /* Attribute-sidecar key: `$f = #[A] function…` trivia`` |
|       - |  117 | `	                              * is keyed to this ['static'] 'function' token */` |
|       - |  118 | `	sxu32 nKwLine;` |
|    7748 |  119 | `	sxi32 iFlags = 0;` |
|       - |  120 | `	sxu32 nLen;` |
|       - |  121 | `	sxi32 rc;` |
|    3855 |  122 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  123 |  |
|    7748 |  124 | `	nKwLine = pGen->pIn->nLine; /* Line of the 'function' keyword (Reflection getStartLine) */` |
|    7743 |  125 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|    7748 |  126 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|       - |  127 | `		/* Static closure: no $this auto-capture, bind refused */` |
|     475 |  128 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|     475 |  129 | `		pGen->pIn++; /* Jump the 'static' keyword */` |
|       - |  130 | ``		/* php's line for the declaration is the `function` keyword's, never the`` |
|       - |  131 | `		 * modifier's: getStartLine(), the closure's name and a refused attribute` |
|       - |  132 | `		 * all read it. */` |
|     475 |  133 | `		if( pGen->pIn < pGen->pEnd ){` |
|     475 |  134 | `			nKwLine = pGen->pIn->nLine;` |
|     235 |  135 | `		}` |
|     235 |  136 | `	}` |
|    7748 |  137 | `	pGen->pIn++; /* Jump the 'function' keyword */` |
|       - |  138 | ``	/* `function &(…) {…}` — a closure returns by reference exactly as a named`` |
|       - |  139 | ``	 * function does, and the `&` sits in the same place. Nothing consumed it here,`` |
|       - |  140 | ``	 * so every by-ref closure was `syntax error, unexpected token "&", expecting`` |
|       - |  141 | ``	 * "("`; the arrow form already read its own. */`` |
|    7748 |  142 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|      14 |  143 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|      14 |  144 | `		pGen->pIn++;` |
|       6 |  145 | `	}` |
|    7748 |  146 | `	if( pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD) ){` |
|     ! 0 |  147 | `		pGen->pIn++;` |
|     ! 0 |  148 | `	}` |
|       - |  149 | `	/* Generate a unique name */` |
|    7748 |  150 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       - |  151 | `	/* Make sure the generated name is unique */` |
|    7748 |  152 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|     ! 0 |  153 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|     ! 0 |  154 | `	}` |
|    7748 |  155 | `	SyStringInitFromBuf(&sName,zName,nLen);` |
|       - |  156 | `	/* php's visible name for this closure, built before the body compiles so a` |
|       - |  157 | `	 * __FUNCTION__ inside it resolves to the same text php reports. */` |
|    7748 |  158 | `	GenStateClosureName(&(*pGen),nKwLine);` |
|       - |  159 | `	/* Compile the lambda body */` |
|    7748 |  160 | `	rc = GenStateCompileFunc(&(*pGen),&sName,iFlags,TRUE,nKwLine,&pAnnonFunc);` |
|    7748 |  161 | `	if( rc == SXERR_ABORT ){` |
|       3 |  162 | `		return SXERR_ABORT;` |
|       - |  163 | `	}` |
|    7746 |  164 | `	if( pAnnonFunc ){` |
|    7744 |  165 | `		pAnnonFunc->nLine = nKwLine;` |
|       - |  166 | ``		/* Expression-position attributes (`$f = #[A] function () {}`): the trivia`` |
|       - |  167 | `		 * sidecar keys them to the closure's first keyword token. */` |
|    7744 |  168 | `		if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pAnnonFunc->aAttrs) == SXERR_ABORT ){` |
|     ! 0 |  169 | `			return SXERR_ABORT;` |
|       - |  170 | `		}` |
|    7744 |  171 | `		if( GenStateCheckAttrPlacement(&(*pGen),&pAnnonFunc->aAttrs,nKwLine,2,2,0,0) == SXERR_ABORT ){` |
|     ! 0 |  172 | `			return SXERR_ABORT;` |
|       - |  173 | `		}` |
|       - |  174 | `		/* A closure's own attributes arrive AFTER its body compiled, so the` |
|       - |  175 | `		 * #[\NoDiscard] rules are decided here rather than with the signature. */` |
|    7744 |  176 | `		if( GenStateApplyNoDiscard(&(*pGen),pAnnonFunc,0,0) == SXERR_ABORT ){` |
|     ! 0 |  177 | `			return SXERR_ABORT;` |
|       - |  178 | `		}` |
|    3853 |  179 | `	}` |
|       - |  180 | `	/* Every anonymous function is a Closure object in PHP, so emit OP_LOAD_CLOSURE for` |
|       - |  181 | `	 * both real closures (per-instantiation captured env) and plain lambdas (no captures);` |
|       - |  182 | `	 * the handler wraps either in a Closure instance. */` |
|    7746 |  183 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pAnnonFunc,0);` |
|       - |  184 | `	/* Node successfully compiled */` |
|    7746 |  185 | `	return SXRET_OK;` |
|    3860 |  186 | `}` |
|       - |  187 | `/*` |
|       - |  188 | ` * Add a free variable to the arrow function's closure environment, unless` |
|       - |  189 | ` * it is 'this' (handled separately), is shadowed by a parameter at any` |
|       - |  190 | ` * enclosing arrow level, or has already been captured.` |
|       - |  191 | ` */` |
|    6873 |  192 | `static sxi32 GenStateArrowAddCapture(` |
|       - |  193 | `	ph7_gen_state *pGen,` |
|       - |  194 | `	ph7_vm_func *pFunc,` |
|       - |  195 | `	const char *zName,` |
|       - |  196 | `	sxu32 nByte,` |
|       - |  197 | `	SyString *aShadow,` |
|       - |  198 | `	sxu32 nShadow)` |
|       5 |  199 | `{` |
|       - |  200 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  201 | `	ph7_vm_func_closure_env *aEnv;` |
|       - |  202 | `	sxu32 n, nEnv;` |
|       - |  203 | `	char *zDup;` |
|    6878 |  204 | `	if( nByte == 0 ){` |
|     ! 0 |  205 | `		return SXRET_OK;` |
|       - |  206 | `	}` |
|    6873 |  207 | `	if( nByte == sizeof("this")-1` |
|    3653 |  208 | `		&& SyMemcmp(zName,"this",sizeof("this")-1) == 0 ){` |
|      54 |  209 | `		return SXRET_OK;` |
|       - |  210 | `	}` |
|    6826 |  211 | `	if( PH7_VmIsAutoGlobal(zName,nByte) ){` |
|       - |  212 | `		/* php never auto-captures an auto-global — it is already visible inside` |
|       - |  213 | `		 * the arrow function. Capturing one was actively destructive here: the` |
|       - |  214 | `		 * install resolves the name through hSuper and so wrote the by-value` |
|       - |  215 | `		 * SNAPSHOT over the superglobal's own slot, which for $GLOBALS froze the` |
|       - |  216 | `		 * whole symbol-table view at closure-creation time for the rest of the` |
|       - |  217 | `		 * program. */` |
|      66 |  218 | `		return SXRET_OK;` |
|       - |  219 | `	}` |
|    7126 |  220 | `	for( n = 0 ; n < nShadow ; n++ ){` |
|    1706 |  221 | `		if( SyStringLength(&aShadow[n]) == nByte` |
|    1639 |  222 | `			&& SyMemcmp(SyStringData(&aShadow[n]),zName,nByte) == 0 ){` |
|    1347 |  223 | `			return SXRET_OK;` |
|       - |  224 | `		}` |
|     186 |  225 | `	}` |
|    5420 |  226 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|    5420 |  227 | `	nEnv = SySetUsed(&pFunc->aClosureEnv);` |
|    6254 |  228 | `	for( n = 0 ; n < nEnv ; n++ ){` |
|    1263 |  229 | `		if( SyStringLength(&aEnv[n].sName) == nByte` |
|    1040 |  230 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),zName,nByte) == 0 ){` |
|     434 |  231 | `			return SXRET_OK;` |
|       - |  232 | `		}` |
|     422 |  233 | `	}` |
|    4991 |  234 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nByte);` |
|    4991 |  235 | `	if( zDup == 0 ){` |
|     ! 0 |  236 | `		return SXERR_ABORT;` |
|       - |  237 | `	}` |
|    4991 |  238 | `	SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|    4991 |  239 | `	sEnv.iFlags = 0;` |
|    4991 |  240 | `	sEnv.nIdx = SXU32_HIGH;` |
|    4991 |  241 | `	PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|    4991 |  242 | `	SyStringInitFromBuf(&sEnv.sName,zDup,nByte);` |
|    4991 |  243 | `	SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|    4991 |  244 | `	return SXRET_OK;` |
|    3428 |  245 | `}` |
|       - |  246 | `/*` |
|       - |  247 | ` * Walk the raw body of a double-quoted string or heredoc, extracting every` |
|       - |  248 | ` * unescaped $<identifier> reference. The semantics mirror the "simple` |
|       - |  249 | `` * syntax" path in GenStateCompileString: `$name`, `{$name}`, `$obj->prop`,`` |
|       - |  250 | `` * `$arr[...]`, `{$arr['k']}` all capture only the leading identifier.`` |
|       - |  251 | ` */` |
|    1427 |  252 | `static sxi32 GenStateArrowScanInterpolatedString(` |
|       - |  253 | `	ph7_gen_state *pGen,` |
|       - |  254 | `	ph7_vm_func *pFunc,` |
|       - |  255 | `	const char *zIn,` |
|       - |  256 | `	const char *zEnd,` |
|       - |  257 | `	SyString *aShadow,` |
|       - |  258 | `	sxu32 nShadow)` |
|       5 |  259 | `{` |
|       - |  260 | `	sxi32 rc;` |
|    7264 |  261 | `	while( zIn < zEnd ){` |
|    5837 |  262 | `		if( zIn[0] == '\\' ){` |
|     493 |  263 | `			zIn++;` |
|     493 |  264 | `			if( zIn < zEnd ){` |
|     493 |  265 | `				zIn++;` |
|     237 |  266 | `			}` |
|     493 |  267 | `			continue;` |
|       - |  268 | `		}` |
|    5344 |  269 | `		if( zIn[0] == '$' && &zIn[1] < zEnd` |
|     187 |  270 | `			&& ((unsigned char)zIn[1] >= 0x80` |
|     182 |  271 | `				\|\| SyisAlpha(zIn[1]) \|\| zIn[1] == '_') ){` |
|       - |  272 | `			/* php's label bytes, the flat set (LEX_LABEL_START in lex.c). */` |
|       - |  273 | `			const char *zName;` |
|     186 |  274 | `			zIn++; /* skip '$' */` |
|     186 |  275 | `			zName = zIn;` |
|     680 |  276 | `			while( zIn < zEnd` |
|    1006 |  277 | `				&& ((unsigned char)zIn[0] >= 0x80` |
|     956 |  278 | `					\|\| SyisAlphaNum(zIn[0]) \|\| zIn[0] == '_') ){` |
|     824 |  279 | `				zIn++;` |
|       4 |  280 | `			}` |
|     186 |  281 | `			if( zIn > zName ){` |
|     276 |  282 | `				rc = GenStateArrowAddCapture(pGen,pFunc,zName,` |
|     182 |  283 | `					(sxu32)(zIn - zName),aShadow,nShadow);` |
|     186 |  284 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  285 | `					return SXERR_ABORT;` |
|       - |  286 | `				}` |
|      90 |  287 | `			}` |
|     186 |  288 | `			continue;` |
|       - |  289 | `		}` |
|    5167 |  290 | `		zIn++;` |
|       5 |  291 | `	}` |
|    1432 |  292 | `	return SXRET_OK;` |
|     710 |  293 | `}` |
|       - |  294 | `/*` |
|       - |  295 | ` * Scan the body token range of an arrow function for free-variable` |
|       - |  296 | ` * references and record them in pFunc's closure environment. Handles:` |
|       - |  297 | ` *   - plain $<id> pairs` |
|       - |  298 | ` *   - variables inside "..." and heredocs (via interpolation scan)` |
|       - |  299 | ` *   - nested arrow functions: descends into the inner body with the inner` |
|       - |  300 | ` *     parameters added to the shadow list, so a variable referenced by a` |
|       - |  301 | ` *     nested arrow that is not the inner's parameter is captured by the` |
|       - |  302 | ` *     OUTER (enabling transitive capture), while the inner's own params` |
|       - |  303 | ` *     are never mistakenly captured.` |
|       - |  304 | ` */` |
|   10728 |  305 | `static sxi32 GenStateArrowCaptureScan(` |
|       - |  306 | `	ph7_gen_state *pGen,` |
|       - |  307 | `	ph7_vm_func *pFunc,` |
|       - |  308 | `	SyToken *pStart,` |
|       - |  309 | `	SyToken *pEnd,` |
|       - |  310 | `	SyString *aShadow,` |
|       - |  311 | `	sxu32 nShadow)` |
|       5 |  312 | `{` |
|   10733 |  313 | `	SyToken *pScan = pStart;` |
|       - |  314 | `	sxi32 rc;` |
|   99620 |  315 | `	while( pScan < pEnd ){` |
|   88892 |  316 | `		if( pScan->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC) ){` |
|    2137 |  317 | `			rc = GenStateArrowScanInterpolatedString(pGen,pFunc,` |
|     705 |  318 | `				pScan->sData.zString,` |
|    1427 |  319 | `				pScan->sData.zString + pScan->sData.nByte,` |
|     705 |  320 | `				aShadow,nShadow);` |
|    1432 |  321 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  322 | `				return SXERR_ABORT;` |
|       - |  323 | `			}` |
|    1432 |  324 | `			pScan++;` |
|    1432 |  325 | `			continue;` |
|       - |  326 | `		}` |
|   87465 |  327 | `		if( pScan->nType & PH7_TK_KEYWORD ){` |
|     811 |  328 | `			sxu32 nKw = (sxu32)SX_PTR_TO_INT(pScan->pUserData);` |
|     811 |  329 | `			SyToken *pFnKw = (nKw == PH7_TKWRD_STATIC) ? &pScan[1] : pScan;` |
|       - |  330 | ``			/* A NESTED arrow function, not a `$fn`/`C::fn` name that merely`` |
|       - |  331 | `			 * spells the keyword (see PH7_TokenOpensArrowFunc). */` |
|     811 |  332 | `			if( PH7_TokenOpensArrowFunc(pStart,pScan,pEnd) ){` |
|       - |  333 | `				SyToken *pInnerSigStart;` |
|       - |  334 | `				SyToken *pInnerSigEnd;` |
|       - |  335 | `				SyToken *pInnerBodyEnd;` |
|       - |  336 | `				SyString *aInnerShadow;` |
|       - |  337 | `				sxu32 nInnerShadow;` |
|       - |  338 | `				sxu32 nInnerParamMax;` |
|       - |  339 | `				SyToken *p;` |
|       - |  340 | `				int iNestInner;` |
|     203 |  341 | `				pScan = pFnKw + 1; /* past 'fn' */` |
|     203 |  342 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  343 | `					pScan++;` |
|     ! 0 |  344 | `				}` |
|     203 |  345 | `				if( pScan >= pEnd \|\| (pScan->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  346 | `					pScan++;` |
|     ! 0 |  347 | `					continue;` |
|       - |  348 | `				}` |
|     203 |  349 | `				pInnerSigStart = ++pScan; /* past '(' */` |
|     203 |  350 | `				PH7_DelimitNestedTokens(pScan,pEnd,` |
|       - |  351 | `					PH7_TK_LPAREN,PH7_TK_RPAREN,&pInnerSigEnd);` |
|     203 |  352 | `				if( pInnerSigEnd >= pEnd ){` |
|     ! 0 |  353 | `					pScan = pEnd;` |
|     ! 0 |  354 | `					continue;` |
|       - |  355 | `				}` |
|       - |  356 | `				/* Build an augmented shadow list: inherited + inner params */` |
|     203 |  357 | `				nInnerParamMax = 0;` |
|     603 |  358 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|     405 |  359 | `					if( p->nType & PH7_TK_DOLLAR ){` |
|     187 |  360 | `						nInnerParamMax++;` |
|      91 |  361 | `					}` |
|     205 |  362 | `				}` |
|     203 |  363 | `				aInnerShadow = (SyString *)SyMemBackendPoolAlloc(` |
|     198 |  364 | `					&pGen->pVm->sAllocator,` |
|     198 |  365 | `					sizeof(SyString) * (nShadow + nInnerParamMax + 1));` |
|     203 |  366 | `				if( aInnerShadow == 0 ){` |
|     ! 0 |  367 | `					return SXERR_ABORT;` |
|       - |  368 | `				}` |
|     203 |  369 | `				nInnerShadow = 0;` |
|     255 |  370 | `				for( ; nInnerShadow < nShadow ; nInnerShadow++ ){` |
|      54 |  371 | `					aInnerShadow[nInnerShadow] = aShadow[nInnerShadow];` |
|      28 |  372 | `				}` |
|     603 |  373 | `				for( p = pInnerSigStart ; p < pInnerSigEnd ; p++ ){` |
|     405 |  374 | `					if( (p->nType & PH7_TK_DOLLAR) == 0 ){` |
|     223 |  375 | `						continue;` |
|       - |  376 | `					}` |
|     187 |  377 | `					if( &p[1] >= pInnerSigEnd ){` |
|     ! 0 |  378 | `						break;` |
|       - |  379 | `					}` |
|     187 |  380 | `					if( (p[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  381 | `						continue;` |
|       - |  382 | `					}` |
|     187 |  383 | `					aInnerShadow[nInnerShadow++] = p[1].sData;` |
|      96 |  384 | `				}` |
|     203 |  385 | `				pScan = &pInnerSigEnd[1]; /* past ')' */` |
|     203 |  386 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_COLON) ){` |
|      35 |  387 | `					pScan++;` |
|      34 |  388 | `					if( pScan < pEnd && (pScan->nType & PH7_TK_OP)` |
|      19 |  389 | `						&& pScan->sData.nByte == 1` |
|       5 |  390 | `						&& pScan->sData.zString[0] == '?' ){` |
|       5 |  391 | `						pScan++;` |
|       2 |  392 | `					}` |
|      34 |  393 | `					if( pScan < pEnd` |
|      35 |  394 | `						&& (pScan->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) ){` |
|      31 |  395 | `						pScan++;` |
|      15 |  396 | `					}` |
|      17 |  397 | `				}` |
|     203 |  398 | `				if( pScan < pEnd && (pScan->nType & PH7_TK_ARRAY_OP) ){` |
|     197 |  399 | `					pScan++; /* past '=>' */` |
|      96 |  400 | `				}` |
|     203 |  401 | `				pInnerBodyEnd = pScan;` |
|     203 |  402 | `				iNestInner = 0;` |
|    1405 |  403 | `				while( pInnerBodyEnd < pEnd ){` |
|    1353 |  404 | `					if( iNestInner == 0 && (pInnerBodyEnd->nType &` |
|       - |  405 | `						(PH7_TK_COMMA\|PH7_TK_SEMI\|PH7_TK_RPAREN` |
|       - |  406 | `						 \|PH7_TK_CSB\|PH7_TK_CCB)) ){` |
|     151 |  407 | `						break;` |
|       - |  408 | `					}` |
|    1207 |  409 | `					if( pInnerBodyEnd->nType &` |
|       - |  410 | `						(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     143 |  411 | `						iNestInner++;` |
|    1137 |  412 | `					}else if( pInnerBodyEnd->nType &` |
|       - |  413 | `						(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     143 |  414 | `						iNestInner--;` |
|      70 |  415 | `					}` |
|    1207 |  416 | `					pInnerBodyEnd++;` |
|       5 |  417 | `				}` |
|       - |  418 | `				/* Scan the inner arrow's default-parameter VALUES as part of` |
|       - |  419 | `				 * the outer's body: a default value is evaluated at call time` |
|       - |  420 | `				 * in the outer frame, so any free variable it references is` |
|       - |  421 | `				 * an outer capture. We must NOT scan the parameter-name` |
|       - |  422 | ``				 * declarations themselves (e.g. '$x' in `fn($x = 10) => ...`)`` |
|       - |  423 | `				 * or those names leak into the outer's closure environment.` |
|       - |  424 | `				 *` |
|       - |  425 | `				 * Walk the signature argument-by-argument, splitting on` |
|       - |  426 | `				 * top-level commas, and for each argument scan only the token` |
|       - |  427 | `				 * range after the '=' sign. */` |
|       - |  428 | `				{` |
|     203 |  429 | `					SyToken *pArgStart = pInnerSigStart;` |
|     385 |  430 | `					while( pArgStart < pInnerSigEnd ){` |
|     187 |  431 | `						SyToken *pArgEnd = pArgStart;` |
|     187 |  432 | `						SyToken *pEq = 0;` |
|     187 |  433 | `						int iNestArg = 0;` |
|     567 |  434 | `						while( pArgEnd < pInnerSigEnd ){` |
|     400 |  435 | `							if( iNestArg == 0` |
|     405 |  436 | `								&& (pArgEnd->nType & PH7_TK_COMMA) ){` |
|      23 |  437 | `								break;` |
|       - |  438 | `							}` |
|     385 |  439 | `							if( pArgEnd->nType &` |
|       - |  440 | `								(PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     ! 0 |  441 | `								iNestArg++;` |
|     385 |  442 | `							}else if( pArgEnd->nType &` |
|       - |  443 | `								(PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     ! 0 |  444 | `								iNestArg--;` |
|     ! 0 |  445 | `							}` |
|     380 |  446 | `							if( pEq == 0 && iNestArg == 0` |
|     379 |  447 | `								&& (pArgEnd->nType & PH7_TK_EQUAL) ){` |
|       7 |  448 | `								pEq = pArgEnd;` |
|       3 |  449 | `							}` |
|     385 |  450 | `							pArgEnd++;` |
|       5 |  451 | `						}` |
|     187 |  452 | `						if( pEq && (pEq + 1) < pArgEnd ){` |
|      10 |  453 | `							rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|       3 |  454 | `								pEq + 1,pArgEnd,aShadow,nShadow);` |
|       7 |  455 | `							if( rc == SXERR_ABORT ){` |
|     ! 0 |  456 | `								return SXERR_ABORT;` |
|       - |  457 | `							}` |
|       3 |  458 | `						}` |
|     187 |  459 | `						pArgStart = pArgEnd;` |
|     182 |  460 | `						if( pArgStart < pInnerSigEnd` |
|     106 |  461 | `							&& (pArgStart->nType & PH7_TK_COMMA) ){` |
|      23 |  462 | `							pArgStart++;` |
|      10 |  463 | `						}` |
|       5 |  464 | `					}` |
|       - |  465 | `				}` |
|     302 |  466 | `				rc = GenStateArrowCaptureScan(pGen,pFunc,` |
|      99 |  467 | `					pScan,pInnerBodyEnd,aInnerShadow,nInnerShadow);` |
|     203 |  468 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  469 | `					return SXERR_ABORT;` |
|       - |  470 | `				}` |
|     203 |  471 | `				pScan = pInnerBodyEnd;` |
|     203 |  472 | `				continue;` |
|       - |  473 | `			}` |
|     304 |  474 | `		}` |
|   87267 |  475 | `		if( (pScan->nType & PH7_TK_DOLLAR) == 0 ){` |
|   80576 |  476 | `			pScan++;` |
|   80576 |  477 | `			continue;` |
|       - |  478 | `		}` |
|       - |  479 | `		{` |
|       - |  480 | `			/* Walk past variable-variable chains ($$x) to the base name. */` |
|    6696 |  481 | `			SyToken *pDollar = pScan;` |
|   10024 |  482 | `			while( &pDollar[1] < pEnd` |
|    6696 |  483 | `				&& (pDollar[1].nType & PH7_TK_DOLLAR) ){` |
|     ! 0 |  484 | `				pDollar++;` |
|     ! 0 |  485 | `			}` |
|    6696 |  486 | `			if( &pDollar[1] >= pEnd ){` |
|     ! 0 |  487 | `				break;` |
|       - |  488 | `			}` |
|    6696 |  489 | `			if( (pDollar[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     ! 0 |  490 | `				pScan = pDollar + 1;` |
|     ! 0 |  491 | `				continue;` |
|       - |  492 | `			}` |
|   10029 |  493 | `			rc = GenStateArrowAddCapture(pGen,pFunc,` |
|    6691 |  494 | `				pDollar[1].sData.zString,pDollar[1].sData.nByte,` |
|    3333 |  495 | `				aShadow,nShadow);` |
|    6696 |  496 | `			if( rc == SXERR_ABORT ){` |
|     ! 0 |  497 | `				return SXERR_ABORT;` |
|       - |  498 | `			}` |
|    6696 |  499 | `			pScan = pDollar + 2;` |
|       - |  500 | `		}` |
|       5 |  501 | `	}` |
|   10733 |  502 | `	return SXRET_OK;` |
|    5270 |  503 | `}` |
|       - |  504 | `/*` |
|       - |  505 | ` * Compile a PHP 7.4 arrow function: [static] fn([params]) [: ret_type] => expr` |
|       - |  506 | ` * Arrow functions are always closures that auto-capture enclosing-scope` |
|       - |  507 | ` * variables by value. The body is a single expression that acts as an` |
|       - |  508 | ` * implicit return. Unless prefixed with 'static', the enclosing object's` |
|       - |  509 | ` * $this is also made available.` |
|       - |  510 | ` */` |
|   10530 |  511 | `PH7_PRIVATE sxi32 PH7_CompileArrowFunc(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  512 | `{` |
|       - |  513 | `	ph7_vm_func *pFunc;` |
|       - |  514 | `	ph7_vm_func_closure_env sEnv;` |
|       - |  515 | `	GenBlock *pBlock;` |
|       - |  516 | `	SySet *pInstrContainer;` |
|       - |  517 | `	SyToken *pSigEnd;      /* Token just past ')' of the parameter list */` |
|       - |  518 | `	SyToken *pBodyStart;   /* First token after '=>' */` |
|       - |  519 | `	SyToken *pBodyEnd;     /* Token just past the last body token */` |
|       - |  520 | `	SyToken *pSavedEnd;` |
|       - |  521 | `	ph7_vm_func_arg *aArgs;` |
|       - |  522 | `	char zName[512];` |
|       - |  523 | `	static int iCnt = 1;` |
|       - |  524 | `	char *zDup;` |
|       - |  525 | `	SyToken *pTokKw;` |
|       - |  526 | `	sxu32 nLen;` |
|       - |  527 | `	sxu32 nLine;` |
|   10535 |  528 | `	sxi32 iFlags = 0;` |
|   10535 |  529 | `	int bStatic = 0;` |
|       - |  530 | `	sxi32 rc;` |
|       - |  531 | `	sxu32 n;` |
|    5166 |  532 | `	SXUNUSED(iCompileFlag); /* cc warning */` |
|       - |  533 |  |
|   10535 |  534 | `	nLine = pGen->pIn->nLine;` |
|       - |  535 | ``	/* Attribute-sidecar key: `#[A] [static] fn` trivia is keyed to this token */`` |
|   10535 |  536 | `	pTokKw = pGen->pIn;` |
|       - |  537 | `	/* Optional 'static' prefix */` |
|   10530 |  538 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|   10535 |  539 | `		&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_STATIC ){` |
|     213 |  540 | `		bStatic = 1;` |
|     213 |  541 | `		iFlags \|= VM_FUNC_STATIC_CL;` |
|     213 |  542 | `		pGen->pIn++;` |
|       - |  543 | ``		/* ...and the declaration's line is the `fn` keyword's, not `static`'s. */`` |
|     213 |  544 | `		if( pGen->pIn < pGen->pEnd ){` |
|     213 |  545 | `			nLine = pGen->pIn->nLine;` |
|     104 |  546 | `		}` |
|     104 |  547 | `	}` |
|       - |  548 | `	/* 'fn' keyword (guaranteed by ExprExtractNode's dispatch) */` |
|   10530 |  549 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|   10535 |  550 | `		\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_FN ){` |
|     ! 0 |  551 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  552 | `			"Arrow function: expected 'fn' keyword");` |
|     ! 0 |  553 | `		return SXERR_SYNTAX;` |
|       - |  554 | `	}` |
|   10535 |  555 | `	pGen->pIn++; /* Jump 'fn' */` |
|       - |  556 | `	/* Optional '&' — return by reference */` |
|   10535 |  557 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_AMPER) ){` |
|     ! 0 |  558 | `		iFlags \|= VM_FUNC_REF_RETURN;` |
|     ! 0 |  559 | `		pGen->pIn++;` |
|     ! 0 |  560 | `	}` |
|       - |  561 | `	/* Expect '(' */` |
|   10535 |  562 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|       3 |  563 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  564 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  565 | `				"syntax error, unexpected %s \"%z\", expecting \"(\"",` |
|       2 |  566 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  567 | `		}else{` |
|     ! 0 |  568 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  569 | `				"syntax error, unexpected end of file, expecting \"(\"");` |
|       - |  570 | `		}` |
|       3 |  571 | `		return SXERR_SYNTAX;` |
|       - |  572 | `	}` |
|   10533 |  573 | `	pGen->pIn++; /* Jump '(' */` |
|       - |  574 | `	/* Delimit the parameter list */` |
|   10533 |  575 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSigEnd);` |
|   10533 |  576 | `	if( pSigEnd >= pGen->pEnd ){` |
|       3 |  577 | `		PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  578 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       3 |  579 | `		return SXERR_SYNTAX;` |
|       - |  580 | `	}` |
|       - |  581 | `	/* Allocate the function state */` |
|   10531 |  582 | `	pFunc = (ph7_vm_func *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(ph7_vm_func));` |
|   10531 |  583 | `	if( pFunc == 0 ){` |
|     ! 0 |  584 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  585 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  586 | `		return SXERR_ABORT;` |
|       - |  587 | `	}` |
|       - |  588 | `	/* Generate a unique lambda name */` |
|   10531 |  589 | `	nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|   10607 |  590 | `	while( SyHashGet(&pGen->pVm->hFunction,zName,nLen) != 0 && nLen < sizeof(zName) - 2 ){` |
|      79 |  591 | `		nLen = SyBufferFormat(zName,sizeof(zName),"[lambda_%d]",iCnt++);` |
|       3 |  592 | `	}` |
|   10531 |  593 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nLen);` |
|   10531 |  594 | `	if( zDup == 0 ){` |
|     ! 0 |  595 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  596 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  597 | `		return SXERR_ABORT;` |
|       - |  598 | `	}` |
|   10531 |  599 | `	PH7_VmInitFuncState(pGen->pVm,pFunc,zDup,nLen,iFlags,0);` |
|       - |  600 | `	/* Reflection getStartLine(): line of the 'fn' keyword */` |
|   10531 |  601 | `	pFunc->nLine = nLine;` |
|       - |  602 | `	/* php's visible name — an arrow function is named exactly like a closure. This` |
|       - |  603 | `	 * compiler builds its own function state, so it consumes the pending name itself. */` |
|   10531 |  604 | `	GenStateClosureName(&(*pGen),nLine);` |
|   10531 |  605 | `	pFunc->sClosureName = pGen->sPendingClosureName;` |
|   10531 |  606 | `	pFunc->sClosureScope = pGen->sPendingClosureScope;` |
|   10531 |  607 | `	SyStringInitFromBuf(&pGen->sPendingClosureName,0,0);` |
|       - |  608 | ``	/* Expression-position attributes (`$f = #[A] fn () => …`) */`` |
|   10531 |  609 | `	if( GenStateCollectParamAttrs(&(*pGen),pTokKw,&pFunc->aAttrs) == SXERR_ABORT ){` |
|     ! 0 |  610 | `		return SXERR_ABORT;` |
|       - |  611 | `	}` |
|   10531 |  612 | `	if( GenStateCheckAttrPlacement(&(*pGen),&pFunc->aAttrs,nLine,2,2,0,0) == SXERR_ABORT ){` |
|     ! 0 |  613 | `		return SXERR_ABORT;` |
|       - |  614 | `	}` |
|       - |  615 | `	/* An arrow function is a closure: its signature is exempt from the` |
|       - |  616 | ``	 * scope-keyword screen, exactly as `function () {}`'s is (see iSigScope). */`` |
|       - |  617 | `	{` |
|   10531 |  618 | `	int iSavedSig = pGen->iSigScope;` |
|   10531 |  619 | `	pGen->iSigScope = PH7_SIGSCOPE_CLOSURE;` |
|       - |  620 | `	/* Collect function arguments */` |
|   10531 |  621 | `	if( pGen->pIn < pSigEnd ){` |
|    1018 |  622 | `		rc = GenStateCollectFuncArgs(pFunc,&(*pGen),pSigEnd,0,0);` |
|    1018 |  623 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  624 | `			pGen->iSigScope = iSavedSig;` |
|     ! 0 |  625 | `			return SXERR_ABORT;` |
|       - |  626 | `		}` |
|     505 |  627 | `	}` |
|       - |  628 | `	/* Point past ')' and parse optional return type */` |
|   10531 |  629 | `	pGen->pIn = &pSigEnd[1];` |
|   10531 |  630 | `	rc = GenStateParseReturnType(pGen,pFunc);` |
|   10531 |  631 | `	pGen->iSigScope = iSavedSig;` |
|       - |  632 | `	}` |
|   10531 |  633 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  634 | `		return SXERR_ABORT;` |
|   10531 |  635 | `	}else if( rc == SXERR_SYNTAX ){` |
|     ! 0 |  636 | `		return SXERR_SYNTAX;` |
|       - |  637 | `	}` |
|   10531 |  638 | `	if( GenStateApplyNoDiscard(&(*pGen),pFunc,0,0) == SXERR_ABORT ){` |
|     ! 0 |  639 | `		return SXERR_ABORT;` |
|       - |  640 | `	}` |
|       - |  641 | `	/* Expect '=>' */` |
|   10531 |  642 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  643 | `		if( pGen->pIn < pGen->pEnd ){` |
|       4 |  644 | `			PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - |  645 | `				"syntax error, unexpected %s \"%z\", expecting \"=>\"",` |
|       2 |  646 | `				TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|       2 |  647 | `		}else{` |
|     ! 0 |  648 | `			PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|       - |  649 | `				"syntax error, unexpected end of file, expecting \"=>\"");` |
|       - |  650 | `		}` |
|       3 |  651 | `		return SXERR_SYNTAX;` |
|       - |  652 | `	}` |
|   10529 |  653 | `	pGen->pIn++; /* Jump '=>' */` |
|   10529 |  654 | `	pBodyStart = pGen->pIn;` |
|   10529 |  655 | `	pBodyEnd = pGen->pEnd;` |
|       - |  656 | `	/* Build the initial shadow list from the arrow's own parameters, then` |
|       - |  657 | `	 * recursively collect free-variable references from the body. The scan` |
|       - |  658 | `	 * handles plain $<id>, interpolated strings/heredocs, and nested arrow` |
|       - |  659 | `	 * functions with proper parameter shadowing for transitive capture. */` |
|   10529 |  660 | `	aArgs = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       - |  661 | `	{` |
|   10529 |  662 | `		SyString *aShadow = 0;` |
|   10529 |  663 | `		sxu32 nShadow = SySetUsed(&pFunc->aArgs);` |
|   10529 |  664 | `		if( nShadow > 0 ){` |
|    1016 |  665 | `			aShadow = (SyString *)SyMemBackendPoolAlloc(` |
|    1011 |  666 | `				&pGen->pVm->sAllocator,sizeof(SyString) * nShadow);` |
|    1016 |  667 | `			if( aShadow == 0 ){` |
|     ! 0 |  668 | `				PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  669 | `					"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  670 | `				return SXERR_ABORT;` |
|       - |  671 | `			}` |
|    2272 |  672 | `			for( n = 0 ; n < nShadow ; n++ ){` |
|    1261 |  673 | `				aShadow[n] = aArgs[n].sName;` |
|     631 |  674 | `			}` |
|     504 |  675 | `		}` |
|   15692 |  676 | `		rc = GenStateArrowCaptureScan(pGen,pFunc,pBodyStart,pBodyEnd,` |
|    5163 |  677 | `			aShadow,nShadow);` |
|   10529 |  678 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  679 | `			return SXERR_ABORT;` |
|       - |  680 | `		}` |
|       - |  681 | `	}` |
|       - |  682 | `	/* Unless declared static, auto-capture $this so arrow functions used` |
|       - |  683 | `	 * inside methods can reference it. Flagged VM_FUNC_ARG_IGNORE so the` |
|       - |  684 | `	 * captured value is silently dropped when the enclosing scope has no` |
|       - |  685 | `	 * $this. */` |
|   10529 |  686 | `	if( !bStatic ){` |
|       - |  687 | `		char *zThisDup;` |
|   10321 |  688 | `		zThisDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,"this",sizeof("this")-1);` |
|   10321 |  689 | `		if( zThisDup == 0 ){` |
|     ! 0 |  690 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  691 | `				"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  692 | `			return SXERR_ABORT;` |
|       - |  693 | `		}` |
|   10321 |  694 | `		SyZero(&sEnv,sizeof(ph7_vm_func_closure_env));` |
|   10321 |  695 | `		sEnv.iFlags = VM_FUNC_ARG_IGNORE;` |
|   10321 |  696 | `		sEnv.nIdx = SXU32_HIGH;` |
|   10321 |  697 | `		PH7_MemObjInit(pGen->pVm,&sEnv.sValue);` |
|   10321 |  698 | `		SyStringInitFromBuf(&sEnv.sName,zThisDup,sizeof("this")-1);` |
|   10321 |  699 | `		SySetPut(&pFunc->aClosureEnv,(const void *)&sEnv);` |
|    5059 |  700 | `	}` |
|       - |  701 | `	/* Arrow functions are always closures; the ARROW mark tells OP_LOAD_CLOSURE` |
|       - |  702 | `	 * these captures are implicit (auto-scanned) so an undefined one stays silent` |
|       - |  703 | `	 * at creation — php only warns when the body reads it. */` |
|   10529 |  704 | `	pFunc->iFlags \|= VM_FUNC_CLOSURE \| VM_FUNC_ARROW;` |
|       - |  705 | `	/* Compile the body expression as an implicit return */` |
|   15692 |  706 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|    5163 |  707 | `		PH7_VmInstrLength(pGen->pVm),pFunc,&pBlock);` |
|   10529 |  708 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  709 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  710 | `			"PH7 engine is running out-of-memory");` |
|     ! 0 |  711 | `		return SXERR_ABORT;` |
|       - |  712 | `	}` |
|   10529 |  713 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   10529 |  714 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pFunc->aByteCode);` |
|   10529 |  715 | `	pSavedEnd = pGen->pEnd;` |
|   10529 |  716 | `	pGen->pIn = pBodyStart;` |
|   10529 |  717 | `	pGen->pEnd = pBodyEnd;` |
|       - |  718 | ``	/* The body is an implicit `return <expr>`, which READS its operands. Compile`` |
|       - |  719 | `	 * it read-only (like echo / string interpolation) so a lone undefined variable` |
|       - |  720 | ``	 * — e.g. `fn()=>$z` for an auto-capture that was undefined at creation and so`` |
|       - |  721 | `	 * never captured (see VmExecOpLoadClosure) — raises php's "Undefined variable"` |
|       - |  722 | `	 * warning at the read instead of being loaded quietly as a plain expression` |
|       - |  723 | ``	 * statement (`$z;`, silent in both engines) would be. */`` |
|       - |  724 | `	{` |
|       - |  725 | `		/* A body of its own: the frameless-arguments mark does not reach into it. */` |
|   10529 |  726 | `		int bSavedFl = pGen->bInFramelessNsArgs;` |
|   10529 |  727 | `		pGen->bInFramelessNsArgs = 0;` |
|   10529 |  728 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|   10529 |  729 | `		pGen->bInFramelessNsArgs = bSavedFl;` |
|       - |  730 | `	}` |
|   10529 |  731 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  732 | `		return SXERR_ABORT;` |
|       - |  733 | `	}` |
|       - |  734 | `	/* The cursor stopped just past the body expression */` |
|   10529 |  735 | `	pFunc->nEndLine = (pGen->pIn > pBodyStart) ? pGen->pIn[-1].nLine : nLine;` |
|       - |  736 | `	/* Emit implicit return: OP_DONE with p1=1 means 'value on stack'.` |
|       - |  737 | `	 * Any throw-expression inside the body needs a valid jump target and a` |
|       - |  738 | `	 * stack-balanced exit path — point its fixup at a separate OP_DONE with` |
|       - |  739 | `	 * p1=0 emitted below, which does not pop the (absent) return value. */` |
|   10529 |  740 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   10529 |  741 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|   10529 |  742 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|   10529 |  743 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|   10529 |  744 | `	GenStateLeaveBlock(&(*pGen),0);` |
|       - |  745 | `	/* Restore cursors; caller will re-synchronize via the node's pEnd */` |
|   10529 |  746 | `	pGen->pIn = pBodyEnd;` |
|   10529 |  747 | `	pGen->pEnd = pSavedEnd;` |
|       - |  748 | ``	/* An arrow body is one expression, and php takes a `yield` in it: calling`` |
|       - |  749 | ``	 * `fn() => yield 7` hands back a Generator, exactly as the `function` form`` |
|       - |  750 | `	 * does. Only the closure/function path scanned for the opcode, so the arrow's` |
|       - |  751 | ``	 * yield ran with no generator frame around it and raised `Cannot use yield`` |
|       - |  752 | ``	 * outside of a generator`. Same scan, same definition-time return-type screen. */`` |
|       - |  753 | `	{` |
|   10529 |  754 | `		VmInstr *aInstr = (VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|       - |  755 | `		sxu32 i;` |
|  113361 |  756 | `		for( i = 0 ; i < SySetUsed(&pFunc->aByteCode) ; i++ ){` |
|  102857 |  757 | `			if( aInstr[i].iOp == PH7_OP_YIELD \|\| aInstr[i].iOp == PH7_OP_YIELD_FROM ){` |
|      21 |  758 | `				pFunc->iFlags \|= VM_FUNC_GENERATOR;` |
|      21 |  759 | `				break;` |
|       - |  760 | `			}` |
|   50503 |  761 | `		}` |
|       - |  762 | `	}` |
|   10529 |  763 | `	if( pFunc->iFlags & VM_FUNC_GENERATOR ){` |
|      21 |  764 | `		if( SXERR_ABORT == GenStateValidateGeneratorReturnType(&(*pGen),pFunc) ){` |
|     ! 0 |  765 | `			return SXERR_ABORT;` |
|       - |  766 | `		}` |
|      10 |  767 | `	}` |
|       - |  768 | `	/* Emit the load-closure instruction */` |
|   10529 |  769 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_CLOSURE,0,0,pFunc,0);` |
|   10529 |  770 | `	return SXRET_OK;` |
|    5171 |  771 | `}` |
|       - |  772 | `/*` |
|       - |  773 | ` * Compile a single arm's expression range into a freshly-allocated` |
|       - |  774 | ` * sub-bytecode container. The caller supplies the token range [pStart, pEnd).` |
|       - |  775 | ` * The sub-bytecode is terminated with OP_DONE so VmLocalExec returns the` |
|       - |  776 | ` * expression's value.` |
|       - |  777 | ` */` |
|     708 |  778 | `static sxi32 GenStateCompileMatchSubExpr(ph7_gen_state *pGen,` |
|       - |  779 | `	SyToken *pStart,SyToken *pStop,SySet *pOut)` |
|       4 |  780 | `{` |
|       - |  781 | `	SySet *pInstrContainer;` |
|       - |  782 | `	SyToken *pTmpIn,*pTmpEnd;` |
|       - |  783 | `	GenBlock *pArmBlock;` |
|       - |  784 | `	sxi32 rc;` |
|     712 |  785 | `	pTmpIn  = pGen->pIn;` |
|     712 |  786 | `	pTmpEnd = pGen->pEnd;` |
|     712 |  787 | `	pGen->pIn  = pStart;` |
|     712 |  788 | `	pGen->pEnd = pStop;` |
|     712 |  789 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|     712 |  790 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pOut);` |
|       - |  791 | `	/* Enter a local FUNC block so any throw-expression fixups register on it` |
|       - |  792 | `	 * (and not on an outer try/catch whose instruction indices live in a` |
|       - |  793 | `	 * different bytecode container). We resolve those fixups to a trailing` |
|       - |  794 | `	 * OP_DONE p1=0 below so a throw inside a match arm cleanly terminates` |
|       - |  795 | `	 * the sub-bytecode while leaving VM_FRAME_THROW set for propagation. */` |
|    1066 |  796 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC,` |
|     354 |  797 | `		PH7_VmInstrLength(pGen->pVm),0,&pArmBlock);` |
|     712 |  798 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  799 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     ! 0 |  800 | `		pGen->pIn  = pTmpIn;` |
|     ! 0 |  801 | `		pGen->pEnd = pTmpEnd;` |
|     ! 0 |  802 | `		return SXERR_ABORT;` |
|       - |  803 | `	}` |
|     712 |  804 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     712 |  805 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|     712 |  806 | `	GenStateFixJumps(pArmBlock,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|     712 |  807 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|     712 |  808 | `	GenStateLeaveBlock(&(*pGen),0);` |
|     712 |  809 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     712 |  810 | `	pGen->pIn  = pTmpIn;` |
|     712 |  811 | `	pGen->pEnd = pTmpEnd;` |
|     712 |  812 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  813 | `		return SXERR_ABORT;` |
|       - |  814 | `	}` |
|     712 |  815 | `	if( rc == SXERR_EMPTY ){` |
|       3 |  816 | `		return SXERR_EMPTY;` |
|       - |  817 | `	}` |
|     710 |  818 | `	return SXRET_OK;` |
|     358 |  819 | `}` |
|       - |  820 | `/*` |
|       - |  821 | ` * Compile a PHP 8.0 match expression:` |
|       - |  822 | ` *     match(subject){ cond_list => result, ..., default => result }` |
|       - |  823 | ` * Match is an expression — on exit the match result is on top of the stack.` |
|       - |  824 | ` * Strict comparison (===) is used between the subject and each condition.` |
|       - |  825 | ` * No fallthrough. If no arm matches and no default is present, a fatal` |
|       - |  826 | ` * Uncaught UnhandledMatchError is raised at runtime.` |
|       - |  827 | ` */` |
|       - |  828 | `/*` |
|       - |  829 | ` * Emit a parse error for match and propagate SXERR_ABORT if the error` |
|       - |  830 | ` * count limit has been reached. Otherwise returns SXERR_SYNTAX so the` |
|       - |  831 | ` * caller can bail out of the current expression.` |
|       - |  832 | ` */` |
|     ! 0 |  833 | `static sxi32 GenStateMatchError(ph7_gen_state *pGen,sxu32 nLine,const char *zFmt,...)` |
|     ! 0 |  834 | `{` |
|       - |  835 | `	va_list ap;` |
|       - |  836 | `	sxi32 rc;` |
|       - |  837 | `	SyBlob sMsg;` |
|     ! 0 |  838 | `	SyBlobInit(&sMsg,&pGen->pVm->sAllocator);` |
|     ! 0 |  839 | `	va_start(ap,zFmt);` |
|     ! 0 |  840 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|     ! 0 |  841 | `	va_end(ap);` |
|     ! 0 |  842 | `	SyBlobAppend(&sMsg,"",1); /* NUL-terminate */` |
|     ! 0 |  843 | `	rc = PH7_GenCompileError(pGen,E_PARSE,nLine,"%s",(const char *)SyBlobData(&sMsg));` |
|     ! 0 |  844 | `	SyBlobRelease(&sMsg);` |
|     ! 0 |  845 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  846 | `		return SXERR_ABORT;` |
|       - |  847 | `	}` |
|     ! 0 |  848 | `	return SXERR_SYNTAX;` |
|     ! 0 |  849 | `}` |
|       - |  850 | `/*` |
|       - |  851 | ` * Scan a top-level token range inside a match body, stopping at the first` |
|       - |  852 | ` * token whose type is in stopMask (not counting nested parens/brackets/braces).` |
|       - |  853 | ` * Returns the stop token pointer (or pEnd if none found).` |
|       - |  854 | ` */` |
|     716 |  855 | `static SyToken * GenStateMatchScanTopLevel(SyToken *pStart,SyToken *pEnd,sxu32 stopMask)` |
|       5 |  856 | `{` |
|     721 |  857 | `	SyToken *pCur = pStart;` |
|     721 |  858 | `	int iNest = 0;` |
|    2115 |  859 | `	while( pCur < pEnd ){` |
|    2003 |  860 | `		if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     120 |  861 | `			iNest++;` |
|    1945 |  862 | `		}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     122 |  863 | `			iNest--;` |
|    1828 |  864 | `		}else if( iNest == 0 && (pCur->nType & stopMask) ){` |
|     609 |  865 | `			return pCur;` |
|       - |  866 | `		}` |
|    1398 |  867 | `		pCur++;` |
|       4 |  868 | `	}` |
|     116 |  869 | `	return pEnd;` |
|     363 |  870 | `}` |
|     170 |  871 | `PH7_PRIVATE sxi32 PH7_CompileMatch(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 |  872 | `{` |
|       - |  873 | `	ph7_match *pMatch;` |
|       - |  874 | `	SyToken *pSubjEnd,*pBodyEnd,*pSavedEnd;` |
|     175 |  875 | `	int bHasDefault = 0;` |
|       - |  876 | `	sxu32 nLine;` |
|       - |  877 | `	sxi32 rc;` |
|      85 |  878 | `	SXUNUSED(iCompileFlag);` |
|     175 |  879 | `	nLine = pGen->pIn->nLine;` |
|     175 |  880 | `	pGen->pIn++; /* Jump 'match' (dispatch in ExprExtractNode guarantees this token) */` |
|       - |  881 | `	/* Expect '(' */` |
|     175 |  882 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|     ! 0 |  883 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  884 | `			"syntax error, unexpected %s, expecting \"(\"",` |
|     ! 0 |  885 | `			pGen->pIn < pGen->pEnd ? "token" : "end of file");` |
|       - |  886 | `	}` |
|     175 |  887 | `	pGen->pIn++; /* Jump '(' */` |
|     175 |  888 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pSubjEnd);` |
|     175 |  889 | `	if( pSubjEnd >= pGen->pEnd ){` |
|     ! 0 |  890 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  891 | `			"syntax error, unexpected end of file, expecting \")\"");` |
|       - |  892 | `	}` |
|     175 |  893 | `	if( pGen->pIn >= pSubjEnd ){` |
|     ! 0 |  894 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  895 | `			"syntax error, unexpected \")\", expecting match subject");` |
|       - |  896 | `	}` |
|       - |  897 | `	/* Compile subject inline — result stays on the caller's operand stack */` |
|     175 |  898 | `	pSavedEnd = pGen->pEnd;` |
|     175 |  899 | `	pGen->pEnd = pSubjEnd;` |
|     175 |  900 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|     175 |  901 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  902 | `		return SXERR_ABORT;` |
|       - |  903 | `	}` |
|     175 |  904 | `	pGen->pEnd = pSavedEnd;` |
|     175 |  905 | `	pGen->pIn = &pSubjEnd[1]; /* Jump ')' */` |
|       - |  906 | `	/* Expect '{' */` |
|     175 |  907 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_OCB) == 0 ){` |
|     ! 0 |  908 | `		return GenStateMatchError(pGen,` |
|     ! 0 |  909 | `			pGen->pIn < pGen->pEnd ? pGen->pIn->nLine : nLine,` |
|       - |  910 | `			"syntax error, expecting \"{\" after match subject");` |
|       - |  911 | `	}` |
|     175 |  912 | `	pGen->pIn++; /* Jump '{' */` |
|     175 |  913 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_OCB,PH7_TK_CCB,&pBodyEnd);` |
|     175 |  914 | `	if( pBodyEnd >= pGen->pEnd ){` |
|     ! 0 |  915 | `		return GenStateMatchError(pGen,nLine,` |
|       - |  916 | `			"syntax error, unexpected end of file, expecting \"}\"");` |
|       - |  917 | `	}` |
|       - |  918 | `	/* Allocate ph7_match container */` |
|     175 |  919 | `	pMatch = (ph7_match *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_match));` |
|     175 |  920 | `	if( pMatch == 0 ){` |
|     ! 0 |  921 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|       - |  922 | `			"Fatal, PH7 engine is running out of memory");` |
|     ! 0 |  923 | `		return SXERR_ABORT;` |
|       - |  924 | `	}` |
|     175 |  925 | `	SyZero(pMatch,sizeof(ph7_match));` |
|     175 |  926 | `	SySetInit(&pMatch->aArms,&pGen->pVm->sAllocator,sizeof(ph7_match_arm));` |
|       - |  927 | `	/* Iterate arms */` |
|     547 |  928 | `	while( pGen->pIn < pBodyEnd ){` |
|       - |  929 | `		ph7_match_arm sArm;` |
|       - |  930 | `		SyToken *pArrow,*pCondStart,*pResStart,*pResEnd;` |
|     387 |  931 | `		sxu32 nArmLine = pGen->pIn->nLine;` |
|     387 |  932 | `		SyZero(&sArm,sizeof(ph7_match_arm));` |
|     387 |  933 | `		SySetInit(&sArm.aConds,&pGen->pVm->sAllocator,sizeof(SySet));` |
|     387 |  934 | `		SySetInit(&sArm.aResult,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       - |  935 | `		/* 'default' arm? */` |
|     382 |  936 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|     224 |  937 | `			&& SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_DEFAULT ){` |
|      60 |  938 | `			if( bHasDefault ){` |
|       3 |  939 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nArmLine,` |
|       - |  940 | `					"Match expressions may only contain one default arm");` |
|       7 |  941 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  942 | `			}` |
|      58 |  943 | `			sArm.bDefault = 1;` |
|      58 |  944 | `			bHasDefault = 1;` |
|      58 |  945 | `			pGen->pIn++;` |
|       - |  946 | ``			/* php's `default possible_comma =>`: one trailing comma is allowed */`` |
|      58 |  947 | `			if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       3 |  948 | `				pGen->pIn++;` |
|       1 |  949 | `			}` |
|      58 |  950 | `			if( pGen->pIn >= pBodyEnd \|\| (pGen->pIn->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|     ! 0 |  951 | `				rc = PH7_GenSyntaxError(pGen,pGen->pIn,"\"=>\"");` |
|     ! 0 |  952 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  953 | `			}` |
|      58 |  954 | `			pGen->pIn++; /* Jump '=>' */` |
|      31 |  955 | `		}else{` |
|       - |  956 | `			/* Condition list: cond (',' cond)* [','] '=>'. The comma before the` |
|       - |  957 | ``			 * arrow is php's `possible_comma` -- `'a', 'b', => …` is how`` |
|       - |  958 | `			 * symfony/cache's RedisTrait lays a long list out one per line, and` |
|       - |  959 | `			 * it used to be refused here as an empty condition. */` |
|     331 |  960 | `			pCondStart = pGen->pIn;` |
|     331 |  961 | `			pArrow = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,` |
|       - |  962 | `				PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|     349 |  963 | `			while( pArrow < pBodyEnd && (pArrow->nType & PH7_TK_COMMA) ){` |
|       - |  964 | `				SySet sCondBc;` |
|      42 |  965 | `				if( pCondStart >= pArrow ){` |
|       - |  966 | `					/* An arm opening on ',' is where php expects the closing` |
|       - |  967 | `					 * brace; one that runs ',' ',' is missing its arrow. */` |
|       8 |  968 | `					rc = PH7_GenSyntaxError(pGen,pArrow,` |
|       4 |  969 | `						pCondStart == pGen->pIn ? "\"}\"" : "\"=>\"");` |
|       6 |  970 | `					return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  971 | `				}` |
|      37 |  972 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      37 |  973 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|      37 |  974 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  975 | `					return SXERR_ABORT;` |
|       - |  976 | `				}` |
|      37 |  977 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|      37 |  978 | `				pCondStart = &pArrow[1]; /* Skip ',' */` |
|      37 |  979 | `				if( pCondStart < pBodyEnd && (pCondStart->nType & PH7_TK_ARRAY_OP) ){` |
|      17 |  980 | ``					pArrow = pCondStart; /* the trailing comma: `cond, =>` */`` |
|      17 |  981 | `					break;` |
|       - |  982 | `				}` |
|      21 |  983 | `				pArrow = GenStateMatchScanTopLevel(pCondStart,pBodyEnd,` |
|       - |  984 | `					PH7_TK_ARRAY_OP\|PH7_TK_COMMA);` |
|       3 |  985 | `			}` |
|     327 |  986 | `			if( pArrow >= pBodyEnd \|\| (pArrow->nType & PH7_TK_ARRAY_OP) == 0 ){` |
|       3 |  987 | `				rc = PH7_GenSyntaxError(pGen,pArrow,"\"=>\"");` |
|       3 |  988 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  989 | `			}` |
|     325 |  990 | `			if( pCondStart >= pArrow && SySetUsed(&sArm.aConds) == 0 ){` |
|       3 |  991 | ``				rc = PH7_GenSyntaxError(pGen,pArrow,"\"}\""); /* `{ => …`: no condition at all */`` |
|       3 |  992 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - |  993 | `			}` |
|     322 |  994 | `			if( pCondStart < pArrow ){` |
|       - |  995 | `				SySet sCondBc;` |
|     306 |  996 | `				SySetInit(&sCondBc,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|     306 |  997 | `				rc = GenStateCompileMatchSubExpr(pGen,pCondStart,pArrow,&sCondBc);` |
|     306 |  998 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  999 | `					return SXERR_ABORT;` |
|       - | 1000 | `				}` |
|     306 | 1001 | `				SySetPut(&sArm.aConds,(const void *)&sCondBc);` |
|     151 | 1002 | `			}` |
|     322 | 1003 | `			pGen->pIn = &pArrow[1]; /* Jump '=>' */` |
|       - | 1004 | `		}` |
|       - | 1005 | `		/* Compile result expression: up to top-level ',' or body end */` |
|     376 | 1006 | `		pResStart = pGen->pIn;` |
|     376 | 1007 | `		pResEnd = GenStateMatchScanTopLevel(pGen->pIn,pBodyEnd,PH7_TK_COMMA);` |
|     376 | 1008 | `		if( pResStart >= pResEnd ){` |
|       - | 1009 | `			/* php names the token that stands where the result should: the ','` |
|       - | 1010 | `			 * of the next arm, or the body's '}' */` |
|     ! 0 | 1011 | `			rc = PH7_GenSyntaxError(pGen,pResEnd,0);` |
|     ! 0 | 1012 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|       - | 1013 | `		}` |
|     376 | 1014 | `		rc = GenStateCompileMatchSubExpr(pGen,pResStart,pResEnd,&sArm.aResult);` |
|     376 | 1015 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1016 | `			return SXERR_ABORT;` |
|       - | 1017 | `		}` |
|     376 | 1018 | `		pGen->pIn = pResEnd;` |
|     376 | 1019 | `		if( pGen->pIn < pBodyEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|     266 | 1020 | `			pGen->pIn++; /* Skip trailing ',' */` |
|     131 | 1021 | `		}` |
|     376 | 1022 | `		SySetPut(&pMatch->aArms,(const void *)&sArm);` |
|       4 | 1023 | `	}` |
|     165 | 1024 | `	pGen->pIn = &pBodyEnd[1]; /* Jump '}' */` |
|     165 | 1025 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_MATCH,0,0,pMatch,0);` |
|     165 | 1026 | `	return SXRET_OK;` |
|      90 | 1027 | `}` |
|       - | 1028 | `/*` |
|       - | 1029 | ` * Compile a backtick quoted string.` |
|       - | 1030 | ` */` |
|       2 | 1031 | `PH7_PRIVATE sxi32 PH7_CompileBacktic(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       1 | 1032 | `{` |
|       1 | 1033 | `	SXUNUSED(iCompileFlag);` |
|       - | 1034 | `	/*` |
|       - | 1035 | ``	 * The backtick (`) operator was DEPRECATED by php (shell_exec() is the replacement).`` |
|       - | 1036 | `	 * PHL targets php's *non-deprecated* surface, so it is a hard parse error — never` |
|       - | 1037 | `	 * compiled to a shell_exec() call.` |
|       - | 1038 | `	 */` |
|       3 | 1039 | `	PH7_GenCompileError(&(*pGen),E_PARSE,pGen->pIn->nLine,` |
|       - | 1040 | ``		"syntax error, the backtick (`) operator was removed, use shell_exec() instead");`` |
|       3 | 1041 | `	return SXERR_ABORT;` |
|       1 | 1042 | `}` |
|       - | 1043 | `/*` |
|       - | 1044 | ` * Compile a function [i.e: die(),exit(),include(),...] which is a langauge` |
|       - | 1045 | ` * construct.` |
|       - | 1046 | ` */` |
|     384 | 1047 | `PH7_PRIVATE sxi32 PH7_CompileLangConstruct(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 | 1048 | `{` |
|       - | 1049 | `	SyString *pName;` |
|       - | 1050 | `	sxu32 nKeyID;` |
|       - | 1051 | `	sxi32 rc;` |
|       - | 1052 | `	/* Name of the language construct [i.e: echo,die...]*/` |
|     389 | 1053 | `	pName = &pGen->pIn->sData;` |
|     389 | 1054 | `	nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|     389 | 1055 | `	pGen->pIn++; /* Jump the language construct keyword */` |
|     389 | 1056 | `	if( nKeyID == PH7_TKWRD_ECHO ){` |
|       - | 1057 | ``		/* A STATEMENT `echo` never reaches here — it dispatches through the statement`` |
|       - | 1058 | ``		 * table, and so does the one a `<?=` short tag opens. Arriving in expression`` |
|       - | 1059 | ``		 * position means source like `fopen('f','r') or echo "IO error";`, which was a`` |
|       - | 1060 | `		 * Symisc extension and is a php parse error (the scope policy: a PH7-ism that` |
|       - | 1061 | `		 * changes the meaning of valid source is a bug). */` |
|       3 | 1062 | `		PH7_GenSyntaxError(&(*pGen),pGen->pIn - 1,0);` |
|       3 | 1063 | `		return SXERR_ABORT;` |
|     ! 0 | 1064 | `	}else{` |
|     387 | 1065 | `		sxi32 nArg = 0;` |
|     387 | 1066 | `		sxu32 nIdx = 0;` |
|       - | 1067 | `		char zCanon[sizeof("include_once")-1];` |
|       - | 1068 | `		SyString sCanon;` |
|     387 | 1069 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|     387 | 1070 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1071 | `			return SXERR_ABORT;` |
|     387 | 1072 | `		}else if(rc != SXERR_EMPTY ){` |
|     387 | 1073 | `			nArg = 1;` |
|     191 | 1074 | `		}` |
|       - | 1075 | `		/* The construct is dispatched as a CALL to the host function of the same name,` |
|       - | 1076 | `		 * so the name emitted here must be the construct's canonical spelling, not the` |
|       - | 1077 | ``		 * source's: php accepts `PRINT`/`Isset`/`EVAL` (keywords are case-insensitive)`` |
|       - | 1078 | ``		 * where the raw text produced `Call to undefined function PRINT()`. Every`` |
|       - | 1079 | `		 * construct name is lower-case ASCII, so folding IS canonicalising. */` |
|     387 | 1080 | `		if( pName->nByte <= sizeof(zCanon) ){` |
|       - | 1081 | `			sxu32 i;` |
|    3305 | 1082 | `			for( i = 0 ; i < pName->nByte ; ++i ){` |
|    2923 | 1083 | `				unsigned char c = (unsigned char)pName->zString[i];` |
|    2923 | 1084 | `				zCanon[i] = (char)((c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c);` |
|    1464 | 1085 | `			}` |
|     387 | 1086 | `			SyStringInitFromBuf(&sCanon,zCanon,pName->nByte);` |
|     387 | 1087 | `			pName = &sCanon;` |
|     191 | 1088 | `		}` |
|     387 | 1089 | `		if( SXRET_OK != GenStateFindLiteral(&(*pGen),pName,&nIdx) ){` |
|       - | 1090 | `			ph7_value *pObj;` |
|       - | 1091 | `			/* Emit the call instruction */` |
|     219 | 1092 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     219 | 1093 | `			if( pObj == 0 ){` |
|     ! 0 | 1094 | `				PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1095 | `				SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1096 | `				return SXERR_ABORT;` |
|       - | 1097 | `			}` |
|     219 | 1098 | `			PH7_MemObjInitFromString(pGen->pVm,pObj,pName);` |
|       - | 1099 | `			/* Install in the literal table */` |
|     219 | 1100 | `			GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     107 | 1101 | `		}` |
|       - | 1102 | `		/* Emit the call instruction */` |
|     387 | 1103 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 1104 | ``		/* PH7_CALL_CONSTRUCT: `print`, `include`, `include_once`, `require` and`` |
|       - | 1105 | ``		 * `require_once` are dispatched as a host function of the same name, and php has`` |
|       - | 1106 | `		 * no such function -- the mark is what lets the hidden registration answer this` |
|       - | 1107 | `		 * site and nothing a script spells (PH7_VmGetHostFunction). */` |
|     578 | 1108 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,nArg,PH7_CALL_CONSTRUCT,` |
|     191 | 1109 | `			GenStateAttachStrictFlag(pGen,0),0);` |
|       - | 1110 | `	}` |
|       - | 1111 | `	/* Node successfully compiled */` |
|     387 | 1112 | `	return SXRET_OK;` |
|     197 | 1113 | `}` |
|       - | 1114 | `/*` |
|       - | 1115 | ` * Compile a node holding a variable declaration.` |
|       - | 1116 | ` * According to the PHP language reference` |
|       - | 1117 | ` *  Variables in PHP are represented by a dollar sign followed by the name of the variable.` |
|       - | 1118 | ` *  The variable name is case-sensitive.` |
|       - | 1119 | ` *  Variable names follow the same rules as other labels in PHP. A valid variable name starts` |
|       - | 1120 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|       - | 1121 | ` *  As a regular expression, it would be expressed thus: '[a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*'` |
|       - | 1122 | ` *  Note: For our purposes here, a letter is a-z, A-Z, and the bytes from 127 through 255 (0x7f-0xff).` |
|       - | 1123 | ` *  Note: $this is a special variable that can't be assigned.` |
|       - | 1124 | ` *  By default, variables are always assigned by value. That is to say, when you assign an expression` |
|       - | 1125 | ` *  to a variable, the entire value of the original expression is copied into the destination variable.` |
|       - | 1126 | ` *  This means, for instance, that after assigning one variable's value to another, changing one of those` |
|       - | 1127 | ` *  variables will have no effect on the other. For more information on this kind of assignment, see` |
|       - | 1128 | ` *  the chapter on Expressions.` |
|       - | 1129 | ` *  PHP also offers another way to assign values to variables: assign by reference. This means that` |
|       - | 1130 | ` *  the new variable simply references (in other words, "becomes an alias for" or "points to") the original` |
|       - | 1131 | ` *  variable. Changes to the new variable affect the original, and vice versa.` |
|       - | 1132 | ` *  To assign by reference, simply prepend an ampersand (&) to the beginning of the variable which` |
|       - | 1133 | ` *  is being assigned (the source variable).` |
|       - | 1134 | ` */` |
| 3771887 | 1135 | `PH7_PRIVATE sxi32 PH7_CompileVariable(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 | 1136 | `{` |
| 3771892 | 1137 | `	sxu32 nLineLocal = pGen->pIn->nLine;` |
|       - | 1138 | `	sxi32 iVv;` |
|       - | 1139 | `	sxi32 iP1;` |
| 3771892 | 1140 | ``	sxi32 iP2 = 0; /* 1 = quiet read (isset/empty), 4 = quiet read (`??`): a missing`` |
|       - | 1141 | `	                * variable must not warn in either */` |
|       - | 1142 | `	void *p3;` |
|       - | 1143 | `	sxi32 rc;` |
| 3771892 | 1144 | `	iVv = -1; /* Variable variable counter */` |
| 7543833 | 1145 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_DOLLAR) ){` |
| 3771946 | 1146 | `		pGen->pIn++;` |
| 3771946 | 1147 | `		iVv++;` |
|       5 | 1148 | `	}` |
| 3771892 | 1149 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       - | 1150 | `		/* Invalid variable name */` |
|     ! 0 | 1151 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"variable or \"{\" or \"$\"");` |
|     ! 0 | 1152 | `		if( rc == SXERR_ABORT ){` |
|       - | 1153 | `			/* Error count limit reached,abort immediately */` |
|     ! 0 | 1154 | `			return SXERR_ABORT;` |
|       - | 1155 | `		}` |
|     ! 0 | 1156 | `		return SXRET_OK;` |
|       - | 1157 | `	}` |
| 3771892 | 1158 | `	p3  = 0;` |
| 3771892 | 1159 | `	if( pGen->pIn->nType & PH7_TK_OCB/*'{'*/ ){` |
|       - | 1160 | `		/* Dynamic variable creation */` |
|      44 | 1161 | `		pGen->pIn++;  /* Jump the open curly */` |
|      44 | 1162 | `		pGen->pEnd--; /* Ignore the trailing curly */` |
|      44 | 1163 | `		if( pGen->pIn >= pGen->pEnd ){` |
|       - | 1164 | `			/* Empty expression */` |
|       - | 1165 | `			{` |
|       - | 1166 | `			/* php names the offending token and, for an empty "${}", stops there:` |
|       - | 1167 | `			 * the "expecting" tail only appears when something could still follow. */` |
|       - | 1168 | ``			/* `${}`: pEnd was stepped back past the trailing '}', so the token php`` |
|       - | 1169 | `			 * names sits AT pEnd. Reach for it before deciding the tail -- php stops` |
|       - | 1170 | `			 * at "unexpected token \"}\"" with no "expecting" clause, which the` |
|       - | 1171 | `			 * NULL-token path could not express because it never saw the '}'. */` |
|       3 | 1172 | `			SyToken *pBad = pGen->pIn < pGen->pEnd ? pGen->pIn : 0;` |
|       3 | 1173 | `			if( pBad == 0 && pGen->pTokenSet ){` |
|       3 | 1174 | `				SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|       3 | 1175 | `				SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|       3 | 1176 | `				if( pGen->pEnd >= pBase && pGen->pEnd < pStreamEnd ){` |
|       3 | 1177 | `					pBad = pGen->pEnd;` |
|       1 | 1178 | `				}` |
|       1 | 1179 | `			}` |
|       5 | 1180 | `			PH7_GenSyntaxError(&(*pGen),pBad,` |
|       2 | 1181 | `				(pBad && (pBad->nType & PH7_TK_CCB)) ? 0 : "variable or \"{\" or \"$\"");` |
|       - | 1182 | `			}` |
|       3 | 1183 | `			return SXRET_OK;` |
|       - | 1184 | `		}` |
|       - | 1185 | `		/* Compile the expression holding the variable name. It is a pure READ, so` |
|       - | 1186 | ``		 * compile it read-only: `${$u}` warns on an undefined $u (php) instead of`` |
|       - | 1187 | ``		 * silently creating it, matching the `$$u` name-read path below. A quiet`` |
|       - | 1188 | `		 * outer (isset()/empty()) suppresses that name warning too, so carry the` |
|       - | 1189 | `		 * quiet flag into the name expression. */` |
|      42 | 1190 | `		sxi32 iNameFlags = EXPR_FLAG_RDONLY_LOAD;` |
|      42 | 1191 | `		if( iCompileFlag & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY) ){` |
|       - | 1192 | ``			/* isset()/empty() suppress the name warning; `??` (EXPR_FLAG_QUIET_VAR)`` |
|       - | 1193 | ``			 * does NOT — php warns `Undefined variable $u` for `${$u} ?? x` and only`` |
|       - | 1194 | `			 * quiets the TARGET read, so QUIET_VAR is deliberately excluded here. */` |
|       3 | 1195 | `			iNameFlags \|= EXPR_FLAG_QUIET_VAR;` |
|       1 | 1196 | `		}` |
|      42 | 1197 | `		rc = PH7_CompileExpr(&(*pGen),iNameFlags,0);` |
|      42 | 1198 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 | 1199 | `			return SXERR_ABORT;` |
|      42 | 1200 | `		}else if( rc == SXERR_EMPTY ){` |
|       3 | 1201 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,0);` |
|       3 | 1202 | `			return SXRET_OK;` |
|       - | 1203 | `		}` |
|      22 | 1204 | `	}else{` |
|       - | 1205 | `		SyHashEntry *pEntry;` |
|       - | 1206 | `		SyString *pName;` |
| 3771852 | 1207 | `		char *zName = 0;` |
|       - | 1208 | `		/* Extract variable name */` |
| 3771852 | 1209 | `		pName = &pGen->pIn->sData;` |
|       - | 1210 | `		/* Advance the stream cursor */` |
| 3771852 | 1211 | `		pGen->pIn++;` |
| 3771852 | 1212 | `		pEntry = SyHashGet(&pGen->hVar,(const void *)pName->zString,pName->nByte);` |
| 3771852 | 1213 | `		if( pEntry == 0 ){` |
|       - | 1214 | `			/* Duplicate name */` |
|  543544 | 1215 | `			zName = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|  543544 | 1216 | `			if( zName == 0 ){` |
|     ! 0 | 1217 | `				PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1218 | `				return SXERR_ABORT;` |
|       - | 1219 | `			}` |
|       - | 1220 | `			/* Install in the hashtable */` |
|  543544 | 1221 | `			SyHashInsert(&pGen->hVar,zName,pName->nByte,zName);` |
|  271378 | 1222 | `		}else{` |
|       - | 1223 | `			/* Name already available */` |
| 3228313 | 1224 | `			zName = (char *)pEntry->pUserData;` |
|       - | 1225 | `		}` |
| 3771852 | 1226 | `		p3 = (void *)zName;` |
| 3771847 | 1227 | `		if( iVv == 0 && pName->nByte == sizeof("this")-1` |
| 2184839 | 1228 | `			&& SyMemcmp((const void *)pName->zString,"this",sizeof("this")-1) == 0 ){` |
|       - | 1229 | ``			/* php marks the INNERMOST function that names `$this` (ZEND_ACC_USES_THIS);`` |
|       - | 1230 | `			 * a closure wrapping that one is not marked by it. */` |
|    2249 | 1231 | `			GenBlock *pBlock = pGen->pCurrent;` |
|    5487 | 1232 | `			while( pBlock && ((pBlock->iFlags & GEN_BLOCK_FUNC) == 0 \|\| pBlock->pUserData == 0) ){` |
|    2121 | 1233 | `				pBlock = pBlock->pParent;` |
|       5 | 1234 | `			}` |
|    2249 | 1235 | `			if( pBlock ){` |
|    2249 | 1236 | `				((ph7_vm_func *)pBlock->pUserData)->iFlags \|= VM_FUNC_USES_THIS;` |
|    1122 | 1237 | `			}` |
|    1122 | 1238 | `		}` |
|       - | 1239 | `	}` |
| 3771888 | 1240 | `	iP1 = 0;` |
| 3771888 | 1241 | `	if( iCompileFlag & EXPR_FLAG_RDONLY_LOAD ){` |
| 3566034 | 1242 | `		if( (iCompileFlag & EXPR_FLAG_LOAD_IDX_STORE) == 0 ){` |
|       - | 1243 | `			/* Read-only load.In other words do not create the variable if inexistant */` |
| 2559391 | 1244 | `			iP1 = 1;` |
| 1277939 | 1245 | `		}` |
| 1780581 | 1246 | `	}` |
|       - | 1247 | ``	/* iP2 marks a QUIET read: `isset($x)` / `empty($x)` inspect a variable`` |
|       - | 1248 | `	 * without reading it, so an undefined one must not warn (php stays silent` |
|       - | 1249 | `	 * for both). Every other read of a missing variable warns — see OP_LOAD.` |
|       - | 1250 | `	 * The two flags are cleared before recursing into a subscript's index` |
|       - | 1251 | ``	 * expression, so `isset($a[$i])` still warns for an undefined $i, as php`` |
|       - | 1252 | ``	 * does. Contexts that VIVIFY (assignment targets, `??`, appends) already`` |
|       - | 1253 | `	 * emit iP1 = 0 and never reach the warning. */` |
| 3771888 | 1254 | `	if( iCompileFlag & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY) ){` |
|   18283 | 1255 | `		iP2 = 1;` |
| 3762738 | 1256 | `	}else if( iCompileFlag & EXPR_FLAG_QUIET_VAR ){` |
|       - | 1257 | ``		/* `??`'s quiet read is quiet for the same WARNING and not for the same`` |
|       - | 1258 | ``		 * rule: php swallows `Undefined variable $x` under `$x ?? d` but still`` |
|       - | 1259 | ``		 * throws `Using $this when not in object context` for `$this ?? d`, where`` |
|       - | 1260 | ``		 * `isset($this)` answers false in silence. One value each, so OP_LOAD can`` |
|       - | 1261 | `		 * tell the two silences apart. */` |
|     749 | 1262 | `		iP2 = 4;` |
| 3753238 | 1263 | `	}else if( iCompileFlag & EXPR_FLAG_RMW_LOAD ){` |
|       - | 1264 | ``		/* Warn-then-create: the read half of `$x++` / `$x .= ...` still needs a`` |
|       - | 1265 | `		 * writable slot, so it cannot use the read-only load above. */` |
|  108564 | 1266 | `		iP2 = 2;` |
| 3698515 | 1267 | `	}else if( iCompileFlag & EXPR_FLAG_DEFER_ARG ){` |
|       - | 1268 | ``		/* D1 deferred call argument. For a plain `$var` (iVv == 0, p3 holds the name)`` |
|       - | 1269 | `		 * emit the deferred load (iP1 stays 1 = no create, iP2 = 3): an undefined` |
|       - | 1270 | `		 * variable is left uncreated and silent, carrying a lazy-lvalue marker that` |
|       - | 1271 | `		 * OP_CALL resolves against the callee's by-ref flags. A variable-variable` |
|       - | 1272 | `		 * ($$x) computes its name on the stack (p3 == 0), so it cannot carry the` |
|       - | 1273 | `		 * marker — fall back to the historical eager create (iP1 = 0), which keeps` |
|       - | 1274 | `		 * its by-ref binding working exactly as before. */` |
|  864307 | 1275 | `		if( iVv == 0 ){` |
|  864301 | 1276 | `			iP2 = 3;` |
|  431531 | 1277 | `		}else{` |
|       8 | 1278 | `			iP1 = 0;` |
|       - | 1279 | `		}` |
|  431529 | 1280 | `	}` |
|       - | 1281 | `	/* Emit the load instruction(s). For a variable-variable ($$x, $$$x, ...) every` |
|       - | 1282 | `	 * load EXCEPT the final dereference resolves a NAME: a pure read that warns on an` |
|       - | 1283 | `	 * undefined name (php) and never creates it. Only the last load is the actual` |
|       - | 1284 | `	 * variable and carries the caller's write/create context (iP1). Emitting the` |
|       - | 1285 | ``	 * outer create-mode for the name loads silently invented $n in `$$n = 5` and`` |
|       - | 1286 | ``	 * skipped php's `Undefined variable $n` warning; a quiet outer (isset/empty)`` |
|       - | 1287 | `	 * still suppresses the name warning as php does. */` |
| 3771888 | 1288 | `	if( iVv > 0 ){` |
|      56 | 1289 | `		sxi32 iP2Name = (iCompileFlag & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY))` |
|       - | 1290 | ``			? 1 /* isset()/empty() suppress the name warning too; `??` (QUIET_VAR)`` |
|      26 | 1291 | `			     * does NOT — it warns the name and quiets only the target read. */ : 0;` |
|      56 | 1292 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,1/* read-only name */,iP2Name,p3,0);` |
|      58 | 1293 | `		while( iVv > 1 ){` |
|       3 | 1294 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,1/* read-only name */,iP2Name,0,0);` |
|       3 | 1295 | `			iVv--;` |
|       1 | 1296 | `		}` |
|       - | 1297 | `		/* Final dereference: the actual variable, in the caller's context. */` |
|      56 | 1298 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,iP2,0,0);` |
|      30 | 1299 | `	}else{` |
| 3771836 | 1300 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,iP1,iP2,p3,0);` |
|       - | 1301 | `	}` |
|       - | 1302 | `	/* Node successfully compiled */` |
| 3771888 | 1303 | `	return SXRET_OK;` |
| 1883363 | 1304 | `}` |
|       - | 1305 | `/*` |
|       - | 1306 | ` * Load a literal.` |
|       - | 1307 | ` */` |
| 1849928 | 1308 | `static sxi32 GenStateLoadLiteral(ph7_gen_state *pGen)` |
|       5 | 1309 | `{` |
| 1849933 | 1310 | `	SyToken *pToken = pGen->pIn;` |
|       - | 1311 | `	ph7_value *pObj;` |
|       - | 1312 | `	SyString *pStr;` |
|       - | 1313 | `	SyString sCanon;` |
|       - | 1314 | `	sxu32 nIdx;` |
|       - | 1315 | `	/* Extract token value */` |
| 1849933 | 1316 | `	pStr = &pToken->sData;` |
|       - | 1317 | `	/* php's MAGIC constants are case-insensitive like the rest of its reserved words —` |
|       - | 1318 | ``	 * `__line__`, `__Dir__` and `__CLASS__` are one constant each — but every one of them`` |
|       - | 1319 | `	 * is recognised by a BYTE-EXACT compare: the compile-time branches just below, and` |
|       - | 1320 | ``	 * `__CLASS__` through constant.c's (deliberately case-sensitive) constant table. Fold`` |
|       - | 1321 | `	 * the spelling to the canonical upper case here, once, so both mechanisms see it; any` |
|       - | 1322 | ``	 * other spelling used to reach the plain-literal path and raise `Undefined constant`` |
|       - | 1323 | ``	 * "__line__"`. Two of the branches below also read a single byte to tell a pair apart`` |
|       - | 1324 | ``	 * (`zString[2]` for __DIR__ vs __FILE__ and __METHOD__ vs __FUNCTION__), which only`` |
|       - | 1325 | `	 * works on the canonical form.` |
|       - | 1326 | `	 *` |
|       - | 1327 | `	 * User constants stay case-SENSITIVE (php) — this fold is limited to the eight names` |
|       - | 1328 | ``	 * below, and skips a member NAME, where `C::__LINE__` is an ordinary class constant. */`` |
| 1849928 | 1329 | `	if( (pToken->nType & PH7_TK_MEMBER_NAME) == 0 && pStr->nByte > 4` |
| 1680646 | 1330 | `		&& pStr->zString[0] == '_' && pStr->zString[1] == '_' ){` |
|       - | 1331 | `		static const char * const azMagic[] = {` |
|       - | 1332 | `			"__LINE__", "__FILE__", "__DIR__", "__FUNCTION__", "__CLASS__",` |
|       - | 1333 | `			"__METHOD__", "__NAMESPACE__", "__TRAIT__"` |
|       - | 1334 | `		};` |
|       - | 1335 | `		sxu32 i;` |
|  154549 | 1336 | `		for( i = 0 ; i < SX_ARRAYSIZE(azMagic) ; ++i ){` |
|  137631 | 1337 | `			sxu32 nMagic = SyStrlen(azMagic[i]);` |
|  137631 | 1338 | `			if( pStr->nByte == nMagic && SyStrnicmp(pStr->zString,azMagic[i],nMagic) == 0 ){` |
|     729 | 1339 | `				SyStringInitFromBuf(&sCanon,azMagic[i],nMagic);` |
|     729 | 1340 | `				pStr = &sCanon;` |
|     729 | 1341 | `				break;` |
|       - | 1342 | `			}` |
|   68367 | 1343 | `		}` |
|    8809 | 1344 | `	}` |
|       - | 1345 | `	/* Deal with the reserved literals [i.e: null,false,true,...] first. A reserved` |
|       - | 1346 | `	 * word used as a member NAME (Enum::Null, C::Array, $o->list()) is a plain` |
|       - | 1347 | `	 * identifier — the parser flagged its token so the whole value-folding chain is` |
|       - | 1348 | `	 * skipped and it falls through to the ordinary string-literal emit below. */` |
| 1849933 | 1349 | `	if( pToken->nType & PH7_TK_MEMBER_NAME ){` |
|       - | 1350 | `		/* fall through to the plain-string literal path */` |
| 1827667 | 1351 | `	}else if( pStr->nByte == sizeof("NULL") - 1 ){` |
|  121979 | 1352 | `		if( SyStrnicmp(pStr->zString,"null",sizeof("NULL")-1) == 0 ){` |
|       - | 1353 | `			/* NULL constant are always indexed at 0 */` |
|   21819 | 1354 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|   21819 | 1355 | `			return SXRET_OK;` |
|  100165 | 1356 | `		}else if( SyStrnicmp(pStr->zString,"true",sizeof("TRUE")-1) == 0 ){` |
|       - | 1357 | `			/* TRUE constant are always indexed at 1 */` |
|   17572 | 1358 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|   17572 | 1359 | `			return SXRET_OK;` |
|       5 | 1360 | `		}` |
| 1888340 | 1361 | `	}else if (pStr->nByte == sizeof("FALSE") - 1 &&` |
|  326845 | 1362 | `		SyStrnicmp(pStr->zString,"false",sizeof("FALSE")-1) == 0 ){` |
|       - | 1363 | `			/* FALSE constant are always indexed at 2 */` |
|  232743 | 1364 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|  232743 | 1365 | `			return SXRET_OK;` |
| 1450722 | 1366 | `	}else if( pStr->nByte == sizeof("__COMPILER_HALT_OFFSET__") - 1` |
|  724162 | 1367 | `		&& pGen->bHaltSeen` |
|     175 | 1368 | `		&& SyStrnicmp(pStr->zString,"__COMPILER_HALT_OFFSET__",` |
|       6 | 1369 | `			sizeof("__COMPILER_HALT_OFFSET__")-1) == 0 ){` |
|       - | 1370 | `			/*` |
|       - | 1371 | ``			 * php's `__COMPILER_HALT_OFFSET__`: the byte just past the`` |
|       - | 1372 | ``			 * `__halt_compiler();` statement's semicolon, resolved at COMPILE time`` |
|       - | 1373 | ``			 * and only in a file that HAS one. That is why `defined()` answers`` |
|       - | 1374 | `			 * false for it (php registers it under a mangled per-file name, and` |
|       - | 1375 | `			 * this emits a literal instead) and why a file with no halt reaches` |
|       - | 1376 | ``			 * the ordinary constant path and raises php's `Undefined constant`.`` |
|       - | 1377 | `			 */` |
|      13 | 1378 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      13 | 1379 | `			if( pObj == 0 ){` |
|     ! 0 | 1380 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1381 | `				return SXERR_ABORT;` |
|       - | 1382 | `			}` |
|      13 | 1383 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,(sxi64)pGen->nHaltOffset);` |
|      13 | 1384 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      13 | 1385 | `			return SXRET_OK;` |
| 1493439 | 1386 | `	}else if(pStr->nByte == sizeof("__LINE__") - 1 &&` |
|   85284 | 1387 | `		SyMemcmp(pStr->zString,"__LINE__",sizeof("__LINE__")-1) == 0 ){` |
|       - | 1388 | `			/* TICKET 1433-004: __LINE__ constant must be resolved at compile time,not run time */` |
|      28 | 1389 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      28 | 1390 | `			if( pObj == 0 ){` |
|     ! 0 | 1391 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1392 | `				return SXERR_ABORT;` |
|       - | 1393 | `			}` |
|      28 | 1394 | `			PH7_MemObjInitFromInt(pGen->pVm,pObj,pToken->nLine);` |
|       - | 1395 | `			/* Emit the load constant instruction */` |
|      28 | 1396 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      28 | 1397 | `			return SXRET_OK;` |
| 1493398 | 1398 | `	}else if( (pStr->nByte == sizeof("__FILE__") - 1 &&` |
|  130090 | 1399 | `		SyMemcmp(pStr->zString,"__FILE__",sizeof("__FILE__")-1) == 0) \|\|` |
| 1495259 | 1400 | `		(pStr->nByte == sizeof("__DIR__") - 1 &&` |
|   89830 | 1401 | `		SyMemcmp(pStr->zString,"__DIR__",sizeof("__DIR__")-1) == 0) ){` |
|       - | 1402 | `			/* __FILE__ / __DIR__ are magic constants resolved at COMPILE time to the` |
|       - | 1403 | `			 * file being compiled (where the token is written), NOT the runtime` |
|       - | 1404 | `			 * execution file. A function defined in a.php reporting __FILE__ must say` |
|       - | 1405 | `			 * a.php even when called from b.php — php semantics, and what Composer's` |
|       - | 1406 | ``			 * autoloader (loadClassLoader's `require __DIR__ . '/ClassLoader.php'`)`` |
|       - | 1407 | `			 * relies on. The runtime-constant path returned the caller's file. */` |
|     563 | 1408 | `			int bDir = (pStr->zString[2] == 'D'); /* __DIR__ vs __FILE__ */` |
|     563 | 1409 | `			SyString *pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     563 | 1410 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     563 | 1411 | `			if( pObj == 0 ){` |
|     ! 0 | 1412 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1413 | `				return SXERR_ABORT;` |
|       - | 1414 | `			}` |
|     563 | 1415 | `			if( pFile && pFile->nByte > 0 ){` |
|     563 | 1416 | `				if( bDir ){` |
|       - | 1417 | `					const char *zDir;` |
|       - | 1418 | `					int nLen;` |
|       - | 1419 | `					SyString sDir;` |
|     257 | 1420 | `					zDir = PH7_ExtractDirName(pFile->zString,(int)pFile->nByte,&nLen);` |
|     257 | 1421 | `					SyStringInitFromBuf(&sDir,zDir,nLen);` |
|     257 | 1422 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,&sDir);` |
|     131 | 1423 | `				}else{` |
|     311 | 1424 | `					PH7_MemObjInitFromString(pGen->pVm,pObj,pFile);` |
|       - | 1425 | `				}` |
|     283 | 1426 | `			}else{` |
|       - | 1427 | `				SyString sMem;` |
|     ! 0 | 1428 | `				SyStringInitFromBuf(&sMem,":MEMORY:",sizeof(":MEMORY:")-1);` |
|     ! 0 | 1429 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sMem);` |
|       - | 1430 | `			}` |
|     563 | 1431 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     563 | 1432 | `			return SXRET_OK;` |
| 1477144 | 1433 | `	}else if( pStr->nByte == sizeof("__NAMESPACE__") - 1 &&` |
|   53939 | 1434 | `		SyMemcmp(pStr->zString,"__NAMESPACE__",sizeof("__NAMESPACE__")-1) == 0 ){` |
|       - | 1435 | `			/* __NAMESPACE__ magic constant: resolved at compile time */` |
|      20 | 1436 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      20 | 1437 | `			if( pObj == 0 ){` |
|     ! 0 | 1438 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1439 | `				return SXERR_ABORT;` |
|       - | 1440 | `			}` |
|      20 | 1441 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - | 1442 | `				SyString sNs;` |
|      14 | 1443 | `				SyStringInitFromBuf(&sNs,(const char *)SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      14 | 1444 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sNs);` |
|       8 | 1445 | `			}else{` |
|       7 | 1446 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0);` |
|       - | 1447 | `			}` |
|      20 | 1448 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      20 | 1449 | `			return SXRET_OK;` |
| 1484361 | 1450 | `	}else if( pStr->nByte == sizeof("__TRAIT__") - 1 &&` |
|   68386 | 1451 | `		SyMemcmp(pStr->zString,"__TRAIT__",sizeof("__TRAIT__")-1) == 0 ){` |
|       - | 1452 | `			/* __TRAIT__ magic constant: the name of the trait whose SOURCE lexically` |
|       - | 1453 | `			 * encloses this token. php resolves it at compile time, and it is shared` |
|       - | 1454 | `			 * across every using class because a trait method body compiles ONCE with` |
|       - | 1455 | `			 * the trait as its owner (PH7_ClassUseTrait adopts the same method pointer).` |
|       - | 1456 | `			 * Unlike __FUNCTION__/__METHOD__ (nearest function), __TRAIT__ is LEXICAL:` |
|       - | 1457 | `			 * closures and arrow-fns are TRANSPARENT (a closure inside a trait method still` |
|       - | 1458 | `			 * yields the trait), so we skip them and keep walking outward — but a class` |
|       - | 1459 | `			 * method or a plain function is an OPAQUE lexical boundary that fixes the answer.` |
|       - | 1460 | `			 * An anonymous class defined inside a trait method is a fresh scope, so` |
|       - | 1461 | `			 * __TRAIT__ is "" there, not the enclosing trait. "" outside any trait (global` |
|       - | 1462 | `			 * scope, plain functions, non-trait methods) — php renders it the empty string,` |
|       - | 1463 | `			 * not NULL. */` |
|      55 | 1464 | `			ph7_class *pTrait = 0;` |
|      55 | 1465 | `			if( pGen->iInMemberDefault > 0 ){` |
|       - | 1466 | `				/* A property/parameter DEFAULT is a const-expression that belongs to the` |
|       - | 1467 | `				 * class whose body is being compiled (pCurClass), never to a lexically-` |
|       - | 1468 | `				 * enclosing method. Read pCurClass directly — the block chain has no func` |
|       - | 1469 | `				 * block for the default and would leak into the enclosing function (an` |
|       - | 1470 | `				 * anonymous class's default inside a trait method is the anon's scope, "").*/` |
|      13 | 1471 | `				if( pGen->pCurClass && (pGen->pCurClass->iFlags & PH7_CLASS_TRAIT) ){` |
|       5 | 1472 | `					pTrait = pGen->pCurClass;` |
|       2 | 1473 | `				}` |
|       7 | 1474 | `			}else{` |
|      43 | 1475 | `				GenBlock *pBlock = pGen->pCurrent;` |
|     105 | 1476 | `				while( pBlock ){` |
|     101 | 1477 | `					if( pBlock->iFlags & GEN_BLOCK_FUNC ){` |
|      54 | 1478 | `						ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      54 | 1479 | `						if( pFunc == 0 ){` |
|       - | 1480 | `							/* A SYNTHETIC function block carries no ph7_vm_func — e.g. the` |
|       - | 1481 | `							 * per-arm throw-fixup block GenStateCompileMatchSubExpr enters to` |
|       - | 1482 | `							 * host a match() expression. It is not a real lexical scope` |
|       - | 1483 | `							 * boundary, so stay transparent and keep walking outward. */` |
|       5 | 1484 | `							pBlock = pBlock->pParent;` |
|       5 | 1485 | `							continue;` |
|       - | 1486 | `						}` |
|      50 | 1487 | `						if( pFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|       - | 1488 | `							/* A class method (of any class, including an anonymous one) is an` |
|       - | 1489 | `							 * OPAQUE scope boundary and fixes the answer: a trait method yields` |
|       - | 1490 | `							 * its trait, any other class's method yields "". Tested BEFORE the` |
|       - | 1491 | `							 * closure flags so a static method is never mistaken for transparent. */` |
|      34 | 1492 | `							if( pFunc->pUserData` |
|      36 | 1493 | `								&& (((ph7_class *)pFunc->pUserData)->iFlags & PH7_CLASS_TRAIT) ){` |
|      30 | 1494 | `								pTrait = (ph7_class *)pFunc->pUserData;` |
|      14 | 1495 | `							}` |
|      36 | 1496 | `							break;` |
|       - | 1497 | `						}` |
|      15 | 1498 | `						if( pFunc->iFlags & (VM_FUNC_CLOSURE\|VM_FUNC_ARROW\|VM_FUNC_STATIC_CL) ){` |
|       - | 1499 | `							/* Closure / arrow fn (VM_FUNC_CLOSURE is only set when the closure` |
|       - | 1500 | `							 * captures, so a capture-less static closure carries only` |
|       - | 1501 | `							 * VM_FUNC_STATIC_CL — include it). Transparent: keep walking outward. */` |
|      13 | 1502 | `							pBlock = pBlock->pParent;` |
|      13 | 1503 | `							continue;` |
|       - | 1504 | `						}` |
|       - | 1505 | `						/* A plain named function is an opaque boundary: __TRAIT__ is "". */` |
|       3 | 1506 | `						break;` |
|       - | 1507 | `					}` |
|      49 | 1508 | `					pBlock = pBlock->pParent;` |
|       3 | 1509 | `				}` |
|       - | 1510 | `			}` |
|      55 | 1511 | `			pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      55 | 1512 | `			if( pObj == 0 ){` |
|     ! 0 | 1513 | `				PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1514 | `				return SXERR_ABORT;` |
|       - | 1515 | `			}` |
|      55 | 1516 | `			if( pTrait ){` |
|      35 | 1517 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&pTrait->sName);` |
|      19 | 1518 | `			}else{` |
|      22 | 1519 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,0); /* empty string */` |
|       - | 1520 | `			}` |
|      55 | 1521 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      55 | 1522 | `			return SXRET_OK;` |
| 1494539 | 1523 | `	}else if( (pStr->nByte == sizeof("__FUNCTION__") - 1 &&` |
|  165736 | 1524 | `		SyMemcmp(pStr->zString,"__FUNCTION__",sizeof("__FUNCTION__")-1) == 0) \|\|` |
| 1527219 | 1525 | `		(pStr->nByte == sizeof("__METHOD__") - 1 &&` |
|  154159 | 1526 | `		SyMemcmp(pStr->zString,"__METHOD__",sizeof("__METHOD__")-1) == 0) ){` |
|      64 | 1527 | `			GenBlock *pBlock = pGen->pCurrent;` |
|       - | 1528 | `			/* TICKET 1433-004: __FUNCTION__/__METHOD__ constants must be resolved at compile time,not run time */` |
|       - | 1529 | `			/* Skip SYNTHETIC function blocks (GEN_BLOCK_FUNC with no ph7_vm_func in` |
|       - | 1530 | `			 * pUserData — e.g. the per-arm throw-fixup block a match() expression enters):` |
|       - | 1531 | `			 * they are not real function scopes. Without this a __FUNCTION__/__METHOD__` |
|       - | 1532 | `			 * inside a match arm reached a NULL pUserData and dereferenced it (compile-time` |
|       - | 1533 | `			 * crash); php resolves to the enclosing real function, which the walk now finds. */` |
|     158 | 1534 | `			while( pBlock && ((pBlock->iFlags & GEN_BLOCK_FUNC) == 0 \|\| pBlock->pUserData == 0) ){` |
|       - | 1535 | `				/* Point to the upper block */` |
|      68 | 1536 | `				pBlock = pBlock->pParent;` |
|       4 | 1537 | `			}` |
|      64 | 1538 | `			if( pBlock == 0 ){` |
|       - | 1539 | `				/* Called in the global scope,load NULL */` |
|       5 | 1540 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 | 1541 | `			}else{` |
|       - | 1542 | `				/* Extract the target function/method */` |
|      60 | 1543 | `				ph7_vm_func *pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|      60 | 1544 | `				int bMethod = (pStr->zString[2] == 'M'); /* __METHOD__ vs __FUNCTION__ */` |
|      60 | 1545 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      60 | 1546 | `				if( pObj == 0 ){` |
|     ! 0 | 1547 | `					PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"Fatal, PH7 engine is running out of memory");` |
|     ! 0 | 1548 | `					return SXERR_ABORT;` |
|       - | 1549 | `				}` |
|       - | 1550 | `				/*` |
|       - | 1551 | `				 * __METHOD__ is the QUALIFIED name: "C::m" inside a method, and the plain` |
|       - | 1552 | `				 * function name inside a plain function (php does not answer "" there —` |
|       - | 1553 | `				 * PH7 loaded NULL, so __METHOD__ was empty in every free function and` |
|       - | 1554 | `				 * unqualified in every method).` |
|       - | 1555 | `				 */` |
|       - | 1556 | ``				/* A property hook answers php's `$p::get`, never the method name`` |
|       - | 1557 | ``				 * PHL synthesizes for it — so __METHOD__ reads `C::$p::get`, the`` |
|       - | 1558 | `				 * same rendering the runtime diagnostics use. */` |
|       - | 1559 | `				{` |
|       - | 1560 | `					SyString sProp,sSelf;` |
|       - | 1561 | `					const char *zKind;` |
|       - | 1562 | `					SyBlob sQual;` |
|       - | 1563 | `					SyString sOut;` |
|      60 | 1564 | `					int bHook = PH7_VmHookSplitName(&pFunc->sName,&sProp,&zKind);` |
|      60 | 1565 | `					SyBlobInit(&sQual,&pGen->pVm->sAllocator);` |
|      60 | 1566 | `					if( bHook ){` |
|       7 | 1567 | `						SyBlobFormat(&sQual,"$%z::%s",&sProp,zKind);` |
|       7 | 1568 | `						SyStringInitFromBuf(&sSelf,SyBlobData(&sQual),SyBlobLength(&sQual));` |
|      57 | 1569 | `					}else if( SyStringLength(&pFunc->sClosureName) > 0 ){` |
|       - | 1570 | ``						/* A closure answers php's `{closure:...}` name, never the`` |
|       - | 1571 | `						 * synthesized lookup key — and __METHOD__ answers the SAME text` |
|       - | 1572 | `						 * (the name already carries the declaring class), so the` |
|       - | 1573 | `						 * qualification below must not run for it. */` |
|      18 | 1574 | `						sSelf = pFunc->sClosureName;` |
|      18 | 1575 | `						bMethod = 0;` |
|      10 | 1576 | `					}else{` |
|      37 | 1577 | `						sSelf = pFunc->sName;` |
|       - | 1578 | `					}` |
|      68 | 1579 | `					if( bMethod && (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|      19 | 1580 | `						SyString *pCls = &((ph7_class *)pFunc->pUserData)->sName;` |
|       - | 1581 | `						SyBlob sFull;` |
|      19 | 1582 | `						SyBlobInit(&sFull,&pGen->pVm->sAllocator);` |
|      19 | 1583 | `						SyBlobFormat(&sFull,"%z::%z",pCls,&sSelf);` |
|      19 | 1584 | `						SyStringInitFromBuf(&sOut,SyBlobData(&sFull),SyBlobLength(&sFull));` |
|      19 | 1585 | `						PH7_MemObjInitFromString(pGen->pVm,pObj,&sOut);` |
|      19 | 1586 | `						SyBlobRelease(&sFull);` |
|      11 | 1587 | `					}else{` |
|      44 | 1588 | `						PH7_MemObjInitFromString(pGen->pVm,pObj,&sSelf);` |
|       - | 1589 | `					}` |
|      60 | 1590 | `					SyBlobRelease(&sQual);` |
|       - | 1591 | `				}` |
|       - | 1592 | `				/* Emit the load constant instruction */` |
|      60 | 1593 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       - | 1594 | `			}` |
|      64 | 1595 | `			return SXRET_OK;` |
|       - | 1596 | `	}` |
|       - | 1597 | ``	/* php keywords are CASE-INSENSITIVE (`SELF::C`, `Parent::m()`, `new STATIC`,`` |
|       - | 1598 | ``	 * `ISSET($x)`), but a few of them reach the engine as THIS literal and are matched`` |
|       - | 1599 | `	 * there BYTE-EXACTLY: the scope keywords against "self"/"parent"/"static" (OP_MEMBER,` |
|       - | 1600 | `	 * OP_NEW, the FCC scope resolver, the error formatter), and isset/empty/eval as the` |
|       - | 1601 | `	 * name of the host function their call dispatches to. Emit the canonical lower-case` |
|       - | 1602 | `	 * spelling for exactly those so the source's case never reaches the match — PH7` |
|       - | 1603 | ``	 * emitted the raw text, so `SELF::C` looked for a class literally named "SELF" and`` |
|       - | 1604 | ``	 * `ISSET($x)` for a function named "ISSET".`` |
|       - | 1605 | `	 *` |
|       - | 1606 | `	 * Every OTHER keyword literal keeps its source case on purpose: it is a CONSTANT` |
|       - | 1607 | ``	 * read (`define('OBJECT',1); echo OBJECT;` — PHL's keyword table covers type names`` |
|       - | 1608 | `	 * php's lexer does not reserve), and php constants are case-SENSITIVE. So is a` |
|       - | 1609 | ``	 * keyword used as a member NAME, which the parser flags: `class C { const STATIC = 5; }`` |
|       - | 1610 | ``	 * echo C::STATIC;` names the constant "STATIC", and folding it looked for "static". */`` |
| 1577090 | 1611 | `	if( (pToken->nType & PH7_TK_KEYWORD) && (pToken->nType & PH7_TK_MEMBER_NAME) == 0 ){` |
|   19885 | 1612 | `		sxu32 nKeyID = (sxu32)SX_PTR_TO_INT(pToken->pUserData);` |
|   19885 | 1613 | `		const char *zCanon = 0;` |
|   19885 | 1614 | `		if( nKeyID == PH7_TKWRD_SELF ){` |
|     591 | 1615 | `			zCanon = "self";` |
|   19592 | 1616 | `		}else if( nKeyID == PH7_TKWRD_PARENT ){` |
|     251 | 1617 | `			zCanon = "parent";` |
|   19176 | 1618 | `		}else if( nKeyID == PH7_TKWRD_STATIC ){` |
|     287 | 1619 | `			zCanon = "static";` |
|   18912 | 1620 | `		}else if( nKeyID == PH7_TKWRD_ISSET ){` |
|   18215 | 1621 | `			zCanon = "isset";` |
|    9655 | 1622 | `		}else if( nKeyID == PH7_TKWRD_EMPTY ){` |
|     219 | 1623 | `			zCanon = "empty";` |
|     454 | 1624 | `		}else if( nKeyID == PH7_TKWRD_EVAL ){` |
|     311 | 1625 | `			zCanon = "eval";` |
|     153 | 1626 | `		}` |
|   19885 | 1627 | `		if( zCanon ){` |
|   19849 | 1628 | `			SyStringInitFromBuf(&sCanon,zCanon,SyStrlen(zCanon));` |
|   19849 | 1629 | `			pStr = &sCanon;` |
|    9911 | 1630 | `		}` |
|    9929 | 1631 | `	}` |
|       - | 1632 | `	/* Query literal table */` |
| 1577090 | 1633 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),pStr,&nIdx) ){` |
|       - | 1634 | `		ph7_value *pLitObj;` |
|       - | 1635 | `		/* Unknown literal,install it in the literal table */` |
|  582933 | 1636 | `		pLitObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|  582933 | 1637 | `		if( pLitObj == 0 ){` |
|     ! 0 | 1638 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 1639 | `			return SXERR_ABORT;` |
|       - | 1640 | `		}` |
|  582933 | 1641 | `		PH7_MemObjInitFromString(pGen->pVm,pLitObj,pStr);` |
|  582933 | 1642 | `		GenStateInstallLiteral(&(*pGen),pLitObj,nIdx);` |
|  290971 | 1643 | `	}` |
|       - | 1644 | `	/* Emit the load constant instruction.` |
|       - | 1645 | `	 *` |
|       - | 1646 | `	 * php resolves an UNQUALIFIED constant against the namespace its SOURCE sits in,` |
|       - | 1647 | `	 * decided at COMPILE time — so a function keeps its own namespace when called from` |
|       - | 1648 | ``	 * another one — and in a fixed order: a `use const` import first (and an import has`` |
|       - | 1649 | ``	 * NO global fallback), else `current-namespace\NAME`, else the global `NAME`. The`` |
|       - | 1650 | `	 * candidate that order picks is resolved here and travels in the instruction's p3;` |
|       - | 1651 | `	 * the VM's own lookup of the bare literal is then just the global step, and the` |
|       - | 1652 | `	 * "Undefined constant" message names the candidate, as php does.` |
|       - | 1653 | `	 *` |
|       - | 1654 | `	 * Only a name that could BE a constant needs one: a keyword literal never is (the` |
|       - | 1655 | `	 * scope keywords and isset/empty/eval reach the OO and call handlers by this same` |
|       - | 1656 | `	 * literal), and a call/new site clears PH7_LOADC_EXPAND before the constant path` |
|       - | 1657 | `	 * can ever run. */` |
|       - | 1658 | `	{` |
| 1577090 | 1659 | `		sxi32 iLoadFlags = PH7_LOADC_EXPAND;` |
| 1577090 | 1660 | `		char *zCand = 0;` |
| 1577090 | 1661 | `		if( (pToken->nType & PH7_TK_KEYWORD) == 0 ){` |
| 2333461 | 1662 | `			SyHashEntry *pImport = SyHashGet(&pGen->hUseConstImports,` |
| 1556603 | 1663 | `				(const void *)pStr->zString,pStr->nByte);` |
| 1556608 | 1664 | `			if( pImport ){` |
|      34 | 1665 | `				const char *zFQN = (const char *)pImport->pUserData;` |
|      34 | 1666 | `				zCand = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFQN,SyStrlen(zFQN));` |
|      34 | 1667 | `				iLoadFlags \|= PH7_LOADC_NOGLOBAL;` |
| 1556593 | 1668 | `			}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       - | 1669 | `				SyBlob sCand;` |
|    1607 | 1670 | `				SyBlobInit(&sCand,&pGen->pVm->sAllocator);` |
|    1607 | 1671 | `				SyBlobAppend(&sCand,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|    1607 | 1672 | `				SyBlobAppend(&sCand,"\\",1);` |
|    1607 | 1673 | `				SyBlobAppend(&sCand,pStr->zString,pStr->nByte);` |
|    2408 | 1674 | `				zCand = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    1602 | 1675 | `					(const char *)SyBlobData(&sCand),SyBlobLength(&sCand));` |
|    1607 | 1676 | `				SyBlobRelease(&sCand);` |
|     801 | 1677 | `			}` |
|  776853 | 1678 | `		}` |
| 1577090 | 1679 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,iLoadFlags,nIdx,zCand,0);` |
|       - | 1680 | `	}` |
| 1577090 | 1681 | `	return SXRET_OK;` |
|  923311 | 1682 | `}` |
|       - | 1683 | `/*` |
|       - | 1684 | ` * Resolve a namespace path or simply load a literal.` |
|       - | 1685 | ` * If the token stream contains namespace separators (backslashes),` |
|       - | 1686 | ` * assemble them into a single literal string (e.g. "Foo\Bar\Baz").` |
|       - | 1687 | ` * Otherwise, load the simple literal directly.` |
|       - | 1688 | ` */` |
| 1851345 | 1689 | `static sxi32 GenStateResolveNamespaceLiteral(ph7_gen_state *pGen)` |
|       5 | 1690 | `{` |
|       - | 1691 | `	sxi32 rc;` |
| 1851350 | 1692 | `	if( pGen->pIn >= pGen->pEnd ){` |
|     ! 0 | 1693 | `		return SXRET_OK;` |
|       - | 1694 | `	}` |
|       - | 1695 | `	/* Check if this is a multi-token namespace path */` |
| 1851350 | 1696 | `	if( pGen->pIn < &pGen->pEnd[-1] ){` |
|       - | 1697 | `		/* Multiple tokens: assemble the full path into sWorker */` |
|    1422 | 1698 | `		SyBlob *pWorker = &pGen->sWorker;` |
|    1422 | 1699 | `		int isAbsolute = 0;` |
|    1422 | 1700 | `		SyBlobReset(pWorker);` |
|       - | 1701 | `		/* Check for leading backslash (absolute path) */` |
|    1422 | 1702 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|     533 | 1703 | `			isAbsolute = 1;` |
|     533 | 1704 | `			pGen->pIn++; /* Skip leading backslash */` |
|     264 | 1705 | `		}` |
|       - | 1706 | `		/* Collect the raw path (no prefix yet) so its FIRST segment can be resolved` |
|       - | 1707 | ``		 * against use-imports below — php resolves `A\B\C` by mapping the leading`` |
|       - | 1708 | ``		 * `A` through the imports (`use X\A;` makes it `X\A\B\C`), and only prepends`` |
|       - | 1709 | ``		 * the current namespace when `A` matches no import. Blindly prefixing the`` |
|       - | 1710 | ``		 * namespace here produced e.g. `Ns\Ev\Bus` for `use ...\Event as Ev; Ev\Bus`. */`` |
|       - | 1711 | `		{` |
|       - | 1712 | `			SyBlob sRaw;` |
|    1422 | 1713 | `			SyBlobInit(&sRaw,&pGen->pVm->sAllocator);` |
|       - | 1714 | ``			/* `namespace\X` spells the CURRENT namespace and is FULLY QUALIFIED from`` |
|       - | 1715 | `			 * there: no import ever applies to it, and the namespace is already in. */` |
|    1422 | 1716 | `			if( !isAbsolute && GenStateNsRelPrefix(pGen,&pGen->pIn,pGen->pEnd,&sRaw) ){` |
|      74 | 1717 | `				isAbsolute = 1;` |
|      35 | 1718 | `			}` |
|    3560 | 1719 | `			while( pGen->pIn <= &pGen->pEnd[-1] ){` |
|    3560 | 1720 | `				if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|    1074 | 1721 | `					SyBlobAppend(&sRaw,"\\",1);` |
|     539 | 1722 | `				}else{` |
|    2491 | 1723 | `					SyBlobAppend(&sRaw,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|       - | 1724 | `				}` |
|    3560 | 1725 | `				if( pGen->pIn == &pGen->pEnd[-1] ){` |
|    1422 | 1726 | `					pGen->pIn++;` |
|    1422 | 1727 | `					break;` |
|       - | 1728 | `				}` |
|    2143 | 1729 | `				pGen->pIn++;` |
|       5 | 1730 | `			}` |
|    1422 | 1731 | `			if( isAbsolute ){` |
|     603 | 1732 | `				SyBlobAppend(pWorker,SyBlobData(&sRaw),SyBlobLength(&sRaw)); /* FQN as written */` |
|     304 | 1733 | `			}else{` |
|     824 | 1734 | `				const char *zRaw = (const char *)SyBlobData(&sRaw);` |
|     824 | 1735 | `				sxu32 nRaw = SyBlobLength(&sRaw);` |
|     824 | 1736 | `				sxu32 nFirst = 0;` |
|       - | 1737 | `				SyHashEntry *pNsImp;` |
|    3669 | 1738 | `				while( nFirst < nRaw && zRaw[nFirst] != '\\' ){ nFirst++; }` |
|     824 | 1739 | `				pNsImp = SyHashGet(&pGen->hUseImports,(const void *)zRaw,nFirst);` |
|     824 | 1740 | `				if( pNsImp ){` |
|       - | 1741 | `					/* Leading segment is an imported alias: substitute its FQN. */` |
|      55 | 1742 | `					const char *zFQN = (const char *)pNsImp->pUserData;` |
|      55 | 1743 | `					SyBlobAppend(pWorker,zFQN,SyStrlen(zFQN));` |
|      55 | 1744 | `					SyBlobAppend(pWorker,zRaw + nFirst,nRaw - nFirst);` |
|     798 | 1745 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|      22 | 1746 | `					SyBlobAppend(pWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      22 | 1747 | `					SyBlobAppend(pWorker,"\\",1);` |
|      22 | 1748 | `					SyBlobAppend(pWorker,zRaw,nRaw);` |
|      13 | 1749 | `				}else{` |
|     754 | 1750 | `					SyBlobAppend(pWorker,zRaw,nRaw); /* global scope, no import */` |
|       - | 1751 | `				}` |
|       - | 1752 | `			}` |
|    1422 | 1753 | `			SyBlobRelease(&sRaw);` |
|       - | 1754 | `		}` |
|    1422 | 1755 | `		if( SyBlobLength(pWorker) > 0 ){` |
|       - | 1756 | `			ph7_value *pObj;` |
|       - | 1757 | `			SyString sPath;` |
|       - | 1758 | `			sxu32 nIdx;` |
|    1422 | 1759 | `			SyStringInitFromBuf(&sPath,(const char *)SyBlobData(pWorker),SyBlobLength(pWorker));` |
|       - | 1760 | `			/*` |
|       - | 1761 | ``			 * `\true`, `\false` and `\null` are php's three reserved literals`` |
|       - | 1762 | `			 * written the way a code GENERATOR writes them -- fully qualified, so` |
|       - | 1763 | ``			 * that no `use` or namespace can shadow them. php resolves each to the`` |
|       - | 1764 | `			 * literal itself; this engine looked the name up in the constant table` |
|       - | 1765 | ``			 * and answered `Undefined constant "true"`. nikic/php-parser emits`` |
|       - | 1766 | `			 * every one of its booleans that way, which is what phpunit.phar dies` |
|       - | 1767 | ``			 * on. Only the GLOBAL spelling counts: `\Ns\true` is an ordinary`` |
|       - | 1768 | ``			 * constant, and so is a bare `true` (handled by the literal path,`` |
|       - | 1769 | `			 * which folds it already).` |
|       - | 1770 | `			 */` |
|    1422 | 1771 | `			if( isAbsolute && SyByteFind(sPath.zString,sPath.nByte,'\\',0) != SXRET_OK ){` |
|     398 | 1772 | `				if( sPath.nByte == sizeof("null")-1` |
|     206 | 1773 | `				 && SyStrnicmp(sPath.zString,"null",sizeof("null")-1) == 0 ){` |
|       3 | 1774 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,0,0,0);` |
|       3 | 1775 | `					return SXRET_OK;` |
|       - | 1776 | `				}` |
|     396 | 1777 | `				if( sPath.nByte == sizeof("true")-1` |
|     204 | 1778 | `				 && SyStrnicmp(sPath.zString,"true",sizeof("true")-1) == 0 ){` |
|     ! 0 | 1779 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,1,0,0);` |
|     ! 0 | 1780 | `					return SXRET_OK;` |
|       - | 1781 | `				}` |
|     396 | 1782 | `				if( sPath.nByte == sizeof("false")-1` |
|     209 | 1783 | `				 && SyStrnicmp(sPath.zString,"false",sizeof("false")-1) == 0 ){` |
|     ! 0 | 1784 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,2,0,0);` |
|     ! 0 | 1785 | `					return SXRET_OK;` |
|       - | 1786 | `				}` |
|     198 | 1787 | `			}` |
|       - | 1788 | `			/* Install in the literal table */` |
|    1420 | 1789 | `			if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sPath,&nIdx) ){` |
|     328 | 1790 | `				pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|     328 | 1791 | `				if( pObj == 0 ){` |
|     ! 0 | 1792 | `					PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|     ! 0 | 1793 | `					return SXERR_ABORT;` |
|       - | 1794 | `				}` |
|     328 | 1795 | `				PH7_MemObjInitFromString(pGen->pVm,pObj,&sPath);` |
|     328 | 1796 | `				GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|     161 | 1797 | `			}` |
|       - | 1798 | `			/* Emit the load constant instruction.` |
|       - | 1799 | `			 * iP1 bit 0 (PH7_LOADC_EXPAND): candidate for constant/function/class expansion.` |
|       - | 1800 | `			 * iP1 bit 1 (PH7_LOADC_ABSOLUTE): fully-qualified; skip namespace prefixing. */` |
|    2127 | 1801 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,` |
|     707 | 1802 | `				isAbsolute ? (PH7_LOADC_EXPAND\|PH7_LOADC_ABSOLUTE) : PH7_LOADC_EXPAND,` |
|     707 | 1803 | `				nIdx,0,0);` |
|    1420 | 1804 | `			return SXRET_OK;` |
|       - | 1805 | `		}` |
|     ! 0 | 1806 | `	}` |
|       - | 1807 | `	/* Single-token literal: load directly */` |
| 1849933 | 1808 | `	rc = GenStateLoadLiteral(&(*pGen));` |
| 1849933 | 1809 | `	return rc;` |
|  924019 | 1810 | `}` |
|       - | 1811 | `/*` |
|       - | 1812 | ` * Compile a literal which is an identifier(name) for a simple value.` |
|       - | 1813 | ` */` |
|       - | 1814 | `/*` |
|       - | 1815 | `` * Compile a first-class-callable marker node `...` (the lone-ellipsis argument list of`` |
|       - | 1816 | `` * `f(...)`). The function-call code generator detects EXPR_NODE_FCC on its single argument`` |
|       - | 1817 | ``  * and emits OP_LOAD_FCC instead of compiling this node, so reaching here means the `...` `` |
|       - | 1818 | ` * appeared outside a call argument list — a syntax error (PHP rejects it likewise).` |
|       - | 1819 | ` */` |
|     ! 0 | 1820 | `PH7_PRIVATE sxi32 PH7_CompileFccMarker(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|     ! 0 | 1821 | `{` |
|     ! 0 | 1822 | `	SXUNUSED(iCompileFlag);` |
|     ! 0 | 1823 | `	PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn ? pGen->pIn->nLine : 0,` |
|       - | 1824 | `		"Cannot use the first-class callable syntax '...' here");` |
|     ! 0 | 1825 | `	return SXERR_SYNTAX;` |
|     ! 0 | 1826 | `}` |
| 1851345 | 1827 | `PH7_PRIVATE sxi32 PH7_CompileLiteral(ph7_gen_state *pGen,sxi32 iCompileFlag)` |
|       5 | 1828 | `{` |
|       - | 1829 | `	sxi32 rc;` |
| 1851350 | 1830 | `	rc = GenStateResolveNamespaceLiteral(&(*pGen));` |
| 1851350 | 1831 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 1832 | `		SXUNUSED(iCompileFlag); /* cc warning */` |
|     ! 0 | 1833 | `		return rc;` |
|       - | 1834 | `	}` |
|       - | 1835 | `	/* Node successfully compiled */` |
| 1851350 | 1836 | `	return SXRET_OK;` |
|  924019 | 1837 | `}` |
|       - | 1838 |  |
