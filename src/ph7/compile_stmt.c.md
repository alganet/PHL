# src/ph7/compile_stmt.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1696/2198 lines (77.16%)

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
|        - |   10 | ` *    Statement compilation: const, continue/break, labels and goto, blocks,` |
|        - |   11 | ` *    loops (while/do/for/foreach), if, global, return, yield, halt, echo,` |
|        - |   12 | ` *    static, var, namespace/use/declare, throw, try/catch/finally, switch.` |
|        - |   13 | ` * Status:` |
|        - |   14 | ` *    Stable.` |
|        - |   15 | ` */` |
|        - |   16 | `/*` |
|        - |   17 | ` * Compile the 'const' statement.` |
|        - |   18 | ` * According to the PHP language reference` |
|        - |   19 | ` *  A constant is an identifier (name) for a simple value. As the name suggests, that value` |
|        - |   20 | ` *  cannot change during the execution of the script (except for magic constants, which aren't actually constants).` |
|        - |   21 | ` *  A constant is case-sensitive by default. By convention, constant identifiers are always uppercase.` |
|        - |   22 | ` *  The name of a constant follows the same rules as any label in PHP. A valid constant name starts` |
|        - |   23 | ` *  with a letter or underscore, followed by any number of letters, numbers, or underscores.` |
|        - |   24 | ` *  As a regular expression it would be expressed thusly: [a-zA-Z_\x7f-\xff][a-zA-Z0-9_\x7f-\xff]*` |
|        - |   25 | ` *  Syntax` |
|        - |   26 | ` *  You can define a constant by using the define()-function or by using the const keyword outside` |
|        - |   27 | ` *  a class definition. Once a constant is defined, it can never be changed or undefined.` |
|        - |   28 | ` *  You can get the value of a constant by simply specifying its name. Unlike with variables` |
|        - |   29 | ` *  you should not prepend a constant with a $. You can also use the function constant() to read` |
|        - |   30 | ` *  a constant's value if you wish to obtain the constant's name dynamically. Use get_defined_constants()` |
|        - |   31 | ` *  to get a list of all defined constants.` |
|        - |   32 | ` *` |
|        - |   33 | ` * Symisc eXtension.` |
|        - |   34 | ` *  PH7 allow any complex expression to be associated with the constant while the zend engine` |
|        - |   35 | ` *  would allow only simple scalar value.` |
|        - |   36 | ` *  Example` |
|        - |   37 | ` *    const HELLO = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|        - |   38 | ` *    Refer to the official documentation for more information on this feature.` |
|        - |   39 | ` */` |
|      132 |   40 | `PH7_PRIVATE sxi32 PH7_CompileConstant(ph7_gen_state *pGen)` |
|        5 |   41 | `{` |
|        - |   42 | `	SySet *pConsCode,*pInstrContainer;` |
|        - |   43 | `	sxu32 nLineLocal;` |
|        - |   44 | `	SyString *pName;` |
|        - |   45 | `	sxi32 rc;` |
|        - |   46 | `	/* php forbids attributes on a comma-separated const list. Snapshot whether the` |
|        - |   47 | `	 * statement carries any now, before the first constant consumes them. */` |
|      137 |   48 | `	int bHadAttrs = SySetUsed(&pGen->aPendingAttrs) > 0;` |
|        - |   49 | ``	/* php attributes every const-statement compile error to the `const` keyword's`` |
|        - |   50 | ``	 * line, not the offending list element's own line (`const A=1,\nB=strlen()` blames`` |
|        - |   51 | `	 * line 1). Capture it once here, before jumping the keyword. */` |
|      137 |   52 | `	nLineLocal = pGen->pIn->nLine;` |
|      137 |   53 | `	pGen->pIn++; /* Jump the 'const' keyword */` |
|        - |   54 | ``	/* php allows a single `const` statement to declare several constants at once`` |
|        - |   55 | ``	 * (`const A = 1, B = 2;`). Loop over the comma-separated name = value pairs;`` |
|        - |   56 | `	 * PH7_CompileExpr(EXPR_FLAG_COMMA_STATEMENT) stops each value at the first` |
|        - |   57 | `	 * top-level comma so the next pair starts cleanly. */` |
|       71 |   58 | `Loop:` |
|      147 |   59 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SSTR\|PH7_TK_DSTR\|PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - |   60 | `		/* Invalid constant name */` |
|        9 |   61 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"identifier");` |
|        9 |   62 | `		if( rc == SXERR_ABORT ){` |
|        - |   63 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |   64 | `			return SXERR_ABORT;` |
|        - |   65 | `		}` |
|        9 |   66 | `		goto Synchronize;` |
|        - |   67 | `	}` |
|        - |   68 | `	/* Peek constant name */` |
|      141 |   69 | `	pName = &pGen->pIn->sData;` |
|        - |   70 | `	/* Make sure the constant name isn't reserved */` |
|      141 |   71 | `	if( GenStateIsReservedConstant(pName) ){` |
|        - |   72 | `		/* Reserved constant */` |
|       10 |   73 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Cannot redeclare constant '%z'",pName);` |
|       10 |   74 | `		if( rc == SXERR_ABORT ){` |
|        - |   75 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |   76 | `			return SXERR_ABORT;` |
|        - |   77 | `		}` |
|       10 |   78 | `		goto Synchronize;` |
|        - |   79 | `	}` |
|      133 |   80 | `	pGen->pIn++;` |
|      133 |   81 | `	if(pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_EQUAL /* '=' */) == 0 ){` |
|        - |   82 | `		/* Invalid statement*/` |
|        6 |   83 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"=\"");` |
|        6 |   84 | `		if( rc == SXERR_ABORT ){` |
|        - |   85 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |   86 | `			return SXERR_ABORT;` |
|        - |   87 | `		}` |
|        6 |   88 | `		goto Synchronize;` |
|        - |   89 | `	}` |
|      129 |   90 | `	pGen->pIn++; /*Jump the equal sign */` |
|        - |   91 | ``	/* php: a closure in a constant expression must be `static function`; a`` |
|        - |   92 | `	 * non-static closure and any arrow fn are compile-time fatals with distinct` |
|        - |   93 | `	 * messages. Checked ahead of the call scan (it skips closure bodies). */` |
|        - |   94 | `	{` |
|      129 |   95 | `		int iClo = PH7_GenStateInitClosureError(pGen);` |
|      129 |   96 | `		if( iClo ){` |
|        8 |   97 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|        2 |   98 | `				iClo == 2 ? "Closures in constant expressions must be static"` |
|        - |   99 | `				          : "Constant expression contains invalid operations");` |
|        6 |  100 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  101 | `				return SXERR_ABORT;` |
|        - |  102 | `			}` |
|        6 |  103 | `			goto Synchronize;` |
|        - |  104 | `		}` |
|        - |  105 | `	}` |
|        - |  106 | `	/* php: a constant expression may not CALL anything --` |
|        - |  107 | `	 * "Constant expression contains invalid operations". PHL used to evaluate the` |
|        - |  108 | ``	 * call happily, so `const X = strlen("ab");` defined X as 2. */`` |
|      125 |  109 | `	if( PH7_GenStateInitHasCallExpr(pGen) ){` |
|        6 |  110 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|        - |  111 | `			"Constant expression contains invalid operations");` |
|        6 |  112 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  113 | `			return SXERR_ABORT;` |
|        - |  114 | `		}` |
|        6 |  115 | `		goto Synchronize;` |
|        - |  116 | `	}` |
|        - |  117 | `	/* Allocate a new constant value container */` |
|      121 |  118 | `	pConsCode = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      121 |  119 | `	if( pConsCode == 0 ){` |
|      ! 0 |  120 | `		PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  121 | `		return SXERR_ABORT;` |
|        - |  122 | `	}` |
|      121 |  123 | `	SySetInit(pConsCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - |  124 | `	/* Swap bytecode container */` |
|      121 |  125 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      121 |  126 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pConsCode);` |
|        - |  127 | ``	/* Compile constant value. php: a stray token after `const X = EXPR` is`` |
|        - |  128 | ``	 * `... expecting "," or ";"` (const supports a comma-separated list).`` |
|        - |  129 | `	 * EXPR_FLAG_COMMA_STATEMENT stops this value at the first top-level comma so a` |
|        - |  130 | `	 * following declaration is left for the loop below. */` |
|        - |  131 | `	{` |
|      121 |  132 | `		const char *zSaveConst = pGen->zClauseCloser;` |
|      121 |  133 | `		pGen->zClauseCloser = "\",\" or \";\"";` |
|      121 |  134 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      121 |  135 | `		pGen->zClauseCloser = zSaveConst;` |
|        - |  136 | `	}` |
|        - |  137 | `	/* Emit the done instruction */` |
|      121 |  138 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      121 |  139 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      121 |  140 | `	if( rc == SXERR_ABORT ){` |
|        - |  141 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  142 | `		return SXERR_ABORT;` |
|        - |  143 | `	}` |
|        - |  144 | ``	/* php parse-errors an empty initializer (`const A = ;`, or a `,B=` list hole);`` |
|        - |  145 | `	 * the class-const path rejects it too. Reject loudly rather than silently` |
|        - |  146 | `	 * defining a NULL constant. (php's error KIND -- a parse error -- differs from` |
|        - |  147 | `	 * PHL's compile fatal, the recursive-descent-vs-bison family; both refuse.) */` |
|      121 |  148 | `	if( rc == SXERR_EMPTY && PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|        2 |  149 | `			"Empty constant '%z' value",pName) == SXERR_ABORT ){` |
|      ! 0 |  150 | `		return SXERR_ABORT;` |
|        - |  151 | `	}` |
|      121 |  152 | `	SySetSetUserData(pConsCode,pGen->pVm);` |
|        - |  153 | `	/* Register the constant with namespace-qualified name */` |
|        - |  154 | `	{` |
|        - |  155 | `		SyBlob sFQN;` |
|        - |  156 | `		SyString sFQNStr;` |
|      121 |  157 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      121 |  158 | `		GenStateBuildFQN(pGen,pName,&sFQN);` |
|      121 |  159 | `		SyStringInitFromBuf(&sFQNStr,(const char *)SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|        - |  160 | ``		/* php refuses a `const` whose name a local `use const` already took. */`` |
|      121 |  161 | `		if( GenStateGuardImportRedeclare(pGen,2,pName,&sFQNStr,nLineLocal) == SXERR_ABORT ){` |
|      ! 0 |  162 | `			SyBlobRelease(&sFQN);` |
|      ! 0 |  163 | `			return SXERR_ABORT;` |
|        - |  164 | `		}` |
|      179 |  165 | `		rc = PH7_VmRegisterConstantEx(pGen->pVm,&sFQNStr,PH7_VmExpandConstantValue,pConsCode,` |
|      116 |  166 | `			(SyString *)SySetPeek(&pGen->pVm->aFiles),nLineLocal,1);` |
|      121 |  167 | `		if( rc == SXRET_OK && SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|        - |  168 | ``			/* php 8.5: attributes on `const` statements — attach the pending`` |
|        - |  169 | `			 * groups to the registered constant record for Reflection. */` |
|       11 |  170 | `			SyHashEntry *pCEntry = SyHashGet(&pGen->pVm->hConstant,` |
|        6 |  171 | `				SyBlobData(&sFQN),SyBlobLength(&sFQN));` |
|        8 |  172 | `			if( pCEntry ){` |
|        8 |  173 | `				ph7_constant *pRegCons = (ph7_constant *)pCEntry->pUserData;` |
|        8 |  174 | `				if( GenStateConsumeAttrs(&(*pGen),&pRegCons->aAttrs) == SXERR_ABORT ){` |
|      ! 0 |  175 | `					SyBlobRelease(&sFQN);` |
|      ! 0 |  176 | `					return SXERR_ABORT;` |
|        - |  177 | `				}` |
|        3 |  178 | `			}` |
|        3 |  179 | `		}` |
|      121 |  180 | `		SyBlobRelease(&sFQN);` |
|        - |  181 | `	}` |
|      121 |  182 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  183 | `		SySetRelease(pConsCode);` |
|      ! 0 |  184 | `		SyMemBackendPoolFree(&pGen->pVm->sAllocator,pConsCode);` |
|      ! 0 |  185 | `	}` |
|        - |  186 | ``	/* Another declaration in the same statement: `const A = 1, B = 2;`. */`` |
|      121 |  187 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA /* ',' */) ){` |
|       14 |  188 | `		if( bHadAttrs ){` |
|        - |  189 | ``			/* php compile-fatals `#[Attr] const A = 1, B = 2;` outright. */`` |
|        3 |  190 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|        - |  191 | `				"Cannot apply attributes to multiple constants at once");` |
|        3 |  192 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  193 | `				return SXERR_ABORT;` |
|        - |  194 | `			}` |
|        3 |  195 | `			goto Synchronize;` |
|        - |  196 | `		}` |
|       12 |  197 | `		pGen->pIn++; /* Jump the comma */` |
|       12 |  198 | `		goto Loop;` |
|        - |  199 | `	}` |
|      109 |  200 | `	return SXRET_OK;` |
|       14 |  201 | `Synchronize:` |
|        - |  202 | `	/* Synchronize with the next top-level semicolon and avoid compiling this` |
|        - |  203 | ``	 * erroneous statement. BRACE-aware (only `{`...`}`): a rejected closure`` |
|        - |  204 | ``	 * initializer's body holds inner `;` that are not statement terminators, so`` |
|        - |  205 | ``	 * without this its `;` and `}` dangle into a spurious second error where php`` |
|        - |  206 | `	 * halts at the first fatal. Parentheses and brackets are deliberately NOT` |
|        - |  207 | ``	 * tracked -- a lone unbalanced `(`/`[` in erroneous input must not swallow the`` |
|        - |  208 | ``	 * following statements (which would drop later error reports); a `{` never`` |
|        - |  209 | `	 * appears in a valid global-const initializer except as a closure body. */` |
|        - |  210 | `	{` |
|       32 |  211 | `		int iBrace = 0;` |
|      122 |  212 | `		while( pGen->pIn < pGen->pEnd ){` |
|      122 |  213 | `			if( iBrace == 0 && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|       32 |  214 | `				break;` |
|        - |  215 | `			}` |
|       94 |  216 | `			if( pGen->pIn->nType & PH7_TK_OCB ){` |
|        3 |  217 | `				iBrace++;` |
|       93 |  218 | `			}else if( pGen->pIn->nType & PH7_TK_CCB ){` |
|        3 |  219 | `				if( iBrace > 0 ){ iBrace--; }` |
|        1 |  220 | `			}` |
|       94 |  221 | `			pGen->pIn++;` |
|        4 |  222 | `		}` |
|        - |  223 | `	}` |
|       32 |  224 | `	return SXRET_OK;` |
|       71 |  225 | `}` |
|        - |  226 | `/*` |
|        - |  227 | ` * Compile the 'continue' statement.` |
|        - |  228 | ` * According to the PHP language reference` |
|        - |  229 | ` *  continue is used within looping structures to skip the rest of the current loop iteration` |
|        - |  230 | ` *  and continue execution at the condition evaluation and then the beginning of the next` |
|        - |  231 | ` *  iteration.` |
|        - |  232 | ` *  Note: Note that in PHP the switch statement is considered a looping structure for` |
|        - |  233 | ` *  the purposes of continue.` |
|        - |  234 | ` *  continue accepts an optional numeric argument which tells it how many levels` |
|        - |  235 | ` *  of enclosing loops it should skip to the end of.` |
|        - |  236 | ` *  Note:` |
|        - |  237 | ` *   continue 0; and continue 1; is the same as running continue;.` |
|        - |  238 | ` */` |
|        - |  239 | `/*` |
|        - |  240 | `` * Pick the jump opcode for a `break`/`continue` targeting pLoop and fill in its iP1,`` |
|        - |  241 | ` * emitting the POP_EXCEPTIONs for the crossed trys this statement can resolve here and` |
|        - |  242 | `` * now. All the classification lives in GenStateJumpScope, which `goto` shares.`` |
|        - |  243 | ` */` |
|   213058 |  244 | `static sxi32 GenStateLoopJumpOp(ph7_gen_state *pGen,GenBlock *pLoop,sxi32 *piP1,` |
|        - |  245 | `	GenJumpScope *pCross)` |
|        5 |  246 | `{` |
|        - |  247 | `	/* The loop encloses the break by construction, so the walk always reaches it. Note the` |
|        - |  248 | `	 * walk EMITS the crossed trys' POP_EXCEPTIONs as it goes, before the caller can see` |
|        - |  249 | `	 * pCross->nFinally and reject: a statement about to be fatal therefore leaves a few` |
|        - |  250 | `	 * dead instructions behind. Harmless — the compile error stops the program from` |
|        - |  251 | `	 * running at all — and the alternative is walking the chain twice on every jump. */` |
|   213063 |  252 | `	GenStateJumpScope(&(*pGen),pGen->nCurScopeId,pLoop->nScopeId,TRUE,pCross);` |
|   213063 |  253 | `	return GenStateScopeJumpOp(pCross,piP1);` |
|        5 |  254 | `}` |
|        - |  255 | `/*` |
|        - |  256 | `` * php compile-rejects a `break`/`continue`/`goto` that leaves a `finally` body (a`` |
|        - |  257 | `` * `return` is fine). One wording, one place, for all three statements.`` |
|        - |  258 | ` */` |
|       10 |  259 | `PH7_PRIVATE sxi32 GenStateJumpOutOfFinally(ph7_gen_state *pGen,sxu32 nLine)` |
|        4 |  260 | `{` |
|       14 |  261 | `	return PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - |  262 | `		"jump out of a finally block is disallowed");` |
|        4 |  263 | `}` |
|   108744 |  264 | `PH7_PRIVATE sxi32 PH7_CompileContinue(ph7_gen_state *pGen)` |
|        5 |  265 | `{` |
|        - |  266 | `	GenBlock *pLoop; /* Target loop */` |
|        - |  267 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|        - |  268 | `	sxi32 iRawLevel; /* The level as WRITTEN, kept for php's diagnostics */` |
|        - |  269 | `	sxu32 nLineLocal;` |
|        - |  270 | `	sxi32 rc;` |
|   108749 |  271 | `	iRawLevel = 1;` |
|   108749 |  272 | `	nLineLocal = pGen->pIn->nLine;` |
|   108749 |  273 | `	iLevel = 0;` |
|        - |  274 | `	/* Jump the 'continue' keyword */` |
|   108749 |  275 | `	pGen->pIn++;` |
|   108749 |  276 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|        - |  277 | `		/* optional numeric argument which tells us how many levels` |
|        - |  278 | `		 * of enclosing loops we should skip to the end of.` |
|        - |  279 | `		 */` |
|        - |  280 | `		char zScratch[GEN_NUM_SCRATCH];` |
|       19 |  281 | `		char *zAlloc = 0;` |
|        - |  282 | `		SyString sNum;` |
|       19 |  283 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|       19 |  284 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  285 | `			return SXERR_ABORT;` |
|        - |  286 | `		}` |
|       19 |  287 | `		if( rc == SXRET_OK ){` |
|       23 |  288 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|       14 |  289 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|       16 |  290 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  291 | `				return SXERR_ABORT;` |
|        - |  292 | `			}` |
|       16 |  293 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|       16 |  294 | `			iRawLevel = iLevel;` |
|       16 |  295 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        7 |  296 | `		}` |
|       19 |  297 | `		if( iLevel < 2 ){` |
|        3 |  298 | `			iLevel = 0;` |
|        1 |  299 | `		}` |
|       19 |  300 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|        8 |  301 | `	}` |
|        - |  302 | `	/* php rejects a non-positive level outright, before asking where it lands. */` |
|   108749 |  303 | `	if( iRawLevel < 1 ){` |
|      ! 0 |  304 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|        - |  305 | `			"'continue' operator accepts only positive integers");` |
|      ! 0 |  306 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  307 | `			return SXERR_ABORT;` |
|        - |  308 | `		}` |
|      ! 0 |  309 | `		return SXRET_OK;` |
|        - |  310 | `	}` |
|        - |  311 | `	/* Point to the target loop */` |
|   108749 |  312 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|   108749 |  313 | `	if( pLoop == 0 ){` |
|        - |  314 | ``		/* Same split php makes for `break`: no loop at all keeps the not-in-context`` |
|        - |  315 | ``		 * wording, too FEW loops is `Cannot 'continue' N levels`. */`` |
|       12 |  316 | `		if( GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,0) != 0 ){` |
|      ! 0 |  317 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,` |
|      ! 0 |  318 | `				"Cannot 'continue' %d level%s",iRawLevel,iRawLevel == 1 ? "" : "s");` |
|      ! 0 |  319 | `		}else{` |
|       12 |  320 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLineLocal,"'continue' not in the 'loop' or 'switch' context");` |
|        - |  321 | `		}` |
|       12 |  322 | `		if( rc == SXERR_ABORT ){` |
|        - |  323 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  324 | `			return SXERR_ABORT;` |
|        - |  325 | `		}` |
|        7 |  326 | `	}else{` |
|   108739 |  327 | `		sxu32 nInstrIdx = 0;` |
|   108739 |  328 | `		sxi32 iP1 = 0;` |
|        - |  329 | `		GenJumpScope sCross;` |
|   108739 |  330 | `		sxi32 iJmpOp = GenStateLoopJumpOp(&(*pGen),pLoop,&iP1,&sCross);` |
|   108739 |  331 | `		if( sCross.nFinally > 0 ){` |
|        3 |  332 | `			if( GenStateJumpOutOfFinally(&(*pGen),nLineLocal) == SXERR_ABORT ){` |
|      ! 0 |  333 | `				return SXERR_ABORT;` |
|        1 |  334 | `			}` |
|   108738 |  335 | `		}else if( pLoop->iFlags & GEN_BLOCK_SWITCH ){` |
|        - |  336 | ``			/* `continue` inside a switch acts like `break` — which is almost never what`` |
|        - |  337 | `			 * the author meant, so php 7.3+ says so at compile time. The generated jump` |
|        - |  338 | `			 * is unchanged; only the diagnostic was missing. An explicit level` |
|        - |  339 | ``			 * (`continue 2`) targets the enclosing loop and stays silent. */`` |
|        5 |  340 | `			if( iLevel < 1 ){` |
|        5 |  341 | `				PH7_GenCompileError(&(*pGen),E_WARNING,nLineLocal,` |
|        - |  342 | `					"\"continue\" targeting switch is equivalent to \"break\"."` |
|        - |  343 | `					" Did you mean to use \"continue 2\"?");` |
|        2 |  344 | `			}` |
|        5 |  345 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,iP1,0,0,&nInstrIdx);` |
|        5 |  346 | `			if( rc == SXRET_OK ){` |
|        5 |  347 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|        2 |  348 | `			}` |
|        3 |  349 | `		}else{` |
|        - |  350 | `			/* Emit the unconditional jump to the beginning of the target loop */` |
|   108733 |  351 | `			PH7_VmEmitInstr(pGen->pVm,iJmpOp,iP1,pLoop->nFirstInstr,0,&nInstrIdx);` |
|   108733 |  352 | `			if( pLoop->bPostContinue == TRUE ){` |
|        - |  353 | `				JumpFixup sJumpFix;` |
|        - |  354 | `				/* Post-continue */` |
|    31719 |  355 | `				sJumpFix.nJumpType = PH7_OP_JMP;` |
|    31719 |  356 | `				sJumpFix.nInstrIdx = nInstrIdx;` |
|    31719 |  357 | `				sJumpFix.pContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    31719 |  358 | `				SySetPut(&pLoop->aPostContFix,(const void *)&sJumpFix);` |
|    15857 |  359 | `			}` |
|        - |  360 | `		}` |
|        - |  361 | `	}` |
|   108749 |  362 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  363 | `		/* Not so fatal,emit a warning only */` |
|      ! 0 |  364 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'continue' statement");` |
|      ! 0 |  365 | `	}` |
|        - |  366 | `	/* Statement successfully compiled */` |
|   108749 |  367 | `	return SXRET_OK;` |
|    54377 |  368 | `}` |
|        - |  369 | `/*` |
|        - |  370 | ` * Compile the 'break' statement.` |
|        - |  371 | ` * According to the PHP language reference` |
|        - |  372 | ` *  break ends execution of the current for, foreach, while, do-while or switch` |
|        - |  373 | ` *  structure.` |
|        - |  374 | ` *  break accepts an optional numeric argument which tells it how many nested` |
|        - |  375 | ` *  enclosing structures are to be broken out of.` |
|        - |  376 | ` */` |
|   104340 |  377 | `PH7_PRIVATE sxi32 PH7_CompileBreak(ph7_gen_state *pGen)` |
|        5 |  378 | `{` |
|        - |  379 | `	GenBlock *pLoop; /* Target loop */` |
|        - |  380 | `	sxi32 iLevel;    /* How many nesting loop to skip */` |
|        - |  381 | `	sxi32 iRawLevel; /* The level as WRITTEN, kept for php's diagnostics */` |
|        - |  382 | `	sxu32 nLineLocal;` |
|        - |  383 | `	sxi32 rc;` |
|   104345 |  384 | `	iLevel = 0;` |
|   104345 |  385 | `	iRawLevel = 1;` |
|   104345 |  386 | `	nLineLocal = pGen->pIn->nLine;` |
|        - |  387 | `	/* Jump the 'break' keyword */` |
|   104345 |  388 | `	pGen->pIn++;` |
|   104345 |  389 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_NUM) ){` |
|        - |  390 | `		/* optional numeric argument which tells us how many levels` |
|        - |  391 | `		 * of enclosing loops we should skip to the end of.` |
|        - |  392 | `		 */` |
|        - |  393 | `		char zScratch[GEN_NUM_SCRATCH];` |
|       22 |  394 | `		char *zAlloc = 0;` |
|        - |  395 | `		SyString sNum;` |
|       22 |  396 | `		rc = GenStateValidateNumericSeparator(pGen, pGen->pIn);` |
|       22 |  397 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  398 | `			return SXERR_ABORT;` |
|        - |  399 | `		}` |
|       22 |  400 | `		if( rc == SXRET_OK ){` |
|       27 |  401 | `			rc = GenStateStripNumericSeparators(&pGen->pVm->sAllocator,` |
|       16 |  402 | `				&pGen->pIn->sData, zScratch, sizeof(zScratch), &sNum, &zAlloc);` |
|       19 |  403 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  404 | `				return SXERR_ABORT;` |
|        - |  405 | `			}` |
|       19 |  406 | `			iLevel = (sxi32)PH7_TokenValueToInt64(&sNum);` |
|       19 |  407 | `			iRawLevel = iLevel;` |
|       19 |  408 | `			if( zAlloc ){ SyMemBackendFree(&pGen->pVm->sAllocator, zAlloc); }` |
|        8 |  409 | `		}` |
|       22 |  410 | `		if( iLevel < 2 ){` |
|        3 |  411 | `			iLevel = 0;` |
|        1 |  412 | `		}` |
|       22 |  413 | `		pGen->pIn++; /* Jump the optional numeric argument */` |
|        9 |  414 | `	}` |
|        - |  415 | `	/* php rejects a non-positive level outright, before asking where it lands. */` |
|   104345 |  416 | `	if( iRawLevel < 1 ){` |
|      ! 0 |  417 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - |  418 | `			"'break' operator accepts only positive integers");` |
|      ! 0 |  419 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  420 | `			return SXERR_ABORT;` |
|        - |  421 | `		}` |
|      ! 0 |  422 | `		goto BreakLevelDone;` |
|        - |  423 | `	}` |
|        - |  424 | `	/* Extract the target loop */` |
|   104345 |  425 | `	pLoop = GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,iLevel);` |
|   156515 |  426 | `	if( pLoop == 0 ){` |
|        - |  427 | `		/* php distinguishes "there is no loop at all here" from "there is one, but` |
|        - |  428 | `		 * not N of them": the first keeps the not-in-context wording, the second is` |
|        - |  429 | ``		 * `Cannot 'break' N levels`. */`` |
|       19 |  430 | `		if( GenStateFetchBlock(pGen->pCurrent,GEN_BLOCK_LOOP,0) != 0 ){` |
|        4 |  431 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        1 |  432 | `				"Cannot 'break' %d level%s",iRawLevel,iRawLevel == 1 ? "" : "s");` |
|        2 |  433 | `		}else{` |
|       17 |  434 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"'break' not in the 'loop' or 'switch' context");` |
|        - |  435 | `		}` |
|       19 |  436 | `		if( rc == SXERR_ABORT ){` |
|        - |  437 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  438 | `			return SXERR_ABORT;` |
|        - |  439 | `		}` |
|       11 |  440 | `	}else{` |
|        - |  441 | `		sxu32 nInstrIdx;` |
|   104329 |  442 | `		sxi32 iP1 = 0;` |
|        - |  443 | `		GenJumpScope sCross;` |
|   104329 |  444 | `		sxi32 iJmpOp = GenStateLoopJumpOp(&(*pGen),pLoop,&iP1,&sCross);` |
|   104329 |  445 | `		if( sCross.nFinally > 0 ){` |
|        9 |  446 | `			if( GenStateJumpOutOfFinally(&(*pGen),nLineLocal) == SXERR_ABORT ){` |
|      ! 0 |  447 | `				return SXERR_ABORT;` |
|        - |  448 | `			}` |
|        6 |  449 | `		}else{` |
|   104323 |  450 | `			rc = PH7_VmEmitInstr(pGen->pVm,iJmpOp,iP1,0,0,&nInstrIdx);` |
|   104323 |  451 | `			if( rc == SXRET_OK ){` |
|        - |  452 | `				/* Fix the jump later when the jump destination is resolved */` |
|   104323 |  453 | `				GenStateNewJumpFixup(pLoop,PH7_OP_JMP,nInstrIdx);` |
|    52159 |  454 | `			}` |
|        - |  455 | `		}` |
|        - |  456 | `	}` |
|    52170 |  457 | `BreakLevelDone:` |
|   104345 |  458 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - |  459 | `		/* Not so fatal,emit a warning only */` |
|      ! 0 |  460 | `		PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Expected semi-colon ';' after 'break' statement");` |
|      ! 0 |  461 | `	}` |
|        - |  462 | `	/* Statement successfully compiled */` |
|   104345 |  463 | `	return SXRET_OK;` |
|    52175 |  464 | `}` |
|        - |  465 | `/*` |
|        - |  466 | ` * The function body a goto or a label sits in, or NULL at file scope. A goto may not` |
|        - |  467 | ` * cross functions, so GenStateFixGoto pairs the two on this.` |
|        - |  468 | ` */` |
|      432 |  469 | `static ph7_vm_func * GenStateOwningFunc(ph7_gen_state *pGen)` |
|        5 |  470 | `{` |
|      437 |  471 | `	GenBlock *pBlock = pGen->pCurrent;` |
|     1105 |  472 | `	while( pBlock && (pBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|      673 |  473 | `		pBlock = pBlock->pParent;` |
|        5 |  474 | `	}` |
|      437 |  475 | `	return pBlock ? (ph7_vm_func *)pBlock->pUserData : 0;` |
|        5 |  476 | `}` |
|        - |  477 | `/*` |
|        - |  478 | ` * Compile or record a label.` |
|        - |  479 | ` *  A label is a target point that is specified by an identifier followed by a colon.` |
|        - |  480 | ` * Example` |
|        - |  481 | ` *  goto LABEL;` |
|        - |  482 | ` *   echo 'Foo';` |
|        - |  483 | ` *  LABEL:` |
|        - |  484 | ` *   echo 'Bar';` |
|        - |  485 | ` */` |
|      212 |  486 | `PH7_PRIVATE sxi32 PH7_CompileLabel(ph7_gen_state *pGen)` |
|        5 |  487 | `{` |
|        - |  488 | `	Label sLabel;` |
|        - |  489 | `	/* php places almost NO restriction on where a label may be DEFINED — inside a loop, a` |
|        - |  490 | `	 * switch or a try{} is all fine; the one rule is that a name may not be declared twice` |
|        - |  491 | `	 * in the same function (below). The rest is on the jump: you may not goto INTO a loop` |
|        - |  492 | `	 * or switch from outside it, which is checked once the labels are all known (see` |
|        - |  493 | `	 * GenStateFixJumps). Record the loop this label sits in so that check can run. */` |
|        - |  494 | `	{` |
|      217 |  495 | `		SyString *pTarget = &pGen->pIn->sData;` |
|      217 |  496 | `		ph7_vm_func *pFunc = GenStateOwningFunc(&(*pGen));` |
|        - |  497 | `		char *zDup;` |
|        - |  498 | `		/* One name, one destination: php compile-rejects a label its function already` |
|        - |  499 | ``		 * declares, wherever the two sit (`L: L:`, one per branch of an if, one in a loop`` |
|        - |  500 | `		 * and one after it). PHL used to accept the redeclaration and silently give every` |
|        - |  501 | `		 * goto the FIRST one. The owning function is part of the key, so the same name in` |
|        - |  502 | `		 * another function — or at file scope beside it — is untouched by this. On the` |
|        - |  503 | `		 * duplicate, keep the first declaration and record nothing: the compile has already` |
|        - |  504 | `		 * failed, and a second entry under the same key would only shadow it. */` |
|      217 |  505 | `		if( SXRET_OK == GenStateGetLabel(&(*pGen),pTarget,pFunc,0) ){` |
|        8 |  506 | `			if( SXERR_ABORT == PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        2 |  507 | `				"Label '%z' already defined",pTarget) ){` |
|      ! 0 |  508 | `				return SXERR_ABORT;` |
|        - |  509 | `			}` |
|        6 |  510 | `			pGen->pIn += 2; /* Jump the label name and the semi-colon */` |
|        6 |  511 | `			return SXRET_OK;` |
|        - |  512 | `		}` |
|        - |  513 | `		/* Initialize label fields */` |
|      213 |  514 | `		sLabel.nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|        - |  515 | `		/* Duplicate label name */` |
|      213 |  516 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|      213 |  517 | `		if( zDup == 0 ){` |
|      ! 0 |  518 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  519 | `			return SXERR_ABORT;` |
|        - |  520 | `		}` |
|      213 |  521 | `		SyStringInitFromBuf(&sLabel.sName,zDup,pTarget->nByte);` |
|      213 |  522 | `		sLabel.nLine = pGen->pIn->nLine;` |
|      213 |  523 | `		sLabel.nLoopId = pGen->nCurLoopId;` |
|        - |  524 | `		/* Where the label sits, so a goto from a detached catch/finally body can be told` |
|        - |  525 | `		 * what it has to cross to reach it (GenStateJumpScope). */` |
|      213 |  526 | `		sLabel.pContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      213 |  527 | `		sLabel.nScopeId = pGen->nCurScopeId;` |
|        - |  528 | `		/* The owning FUNCTION, matched against the goto's in GenStateFixGoto. This used` |
|        - |  529 | `		 * to stop at GEN_BLOCK_EXCEPTION as well, which attributed every label inside a` |
|        - |  530 | `		 * try or catch body to "no function" — so from anywhere in a function such a` |
|        - |  531 | `		 * label read as undefined, including from the very catch body declaring it.` |
|        - |  532 | `		 * Whether a label may be jumped TO is decided by its container, not by this. */` |
|      213 |  533 | `		sLabel.pFunc = pFunc;` |
|        - |  534 | `		/* Insert in label set */` |
|      213 |  535 | `		SySetPut(&pGen->aLabel,(const void *)&sLabel);` |
|        - |  536 | `	}` |
|      213 |  537 | `	pGen->pIn += 2; /* Jump the label name and the semi-colon*/` |
|      213 |  538 | `	return SXRET_OK;` |
|      111 |  539 | `}` |
|        - |  540 | `/*` |
|        - |  541 | ` * Compile the so hated 'goto' statement.` |
|        - |  542 | ` * You've probably been taught that gotos are bad, but this sort` |
|        - |  543 | ` * of rewriting  happens all the time, in fact every time you run` |
|        - |  544 | ` * a compiler it has to do this.` |
|        - |  545 | ` * According to the PHP language reference manual` |
|        - |  546 | ` *   The goto operator can be used to jump to another section in the program.` |
|        - |  547 | ` *   The target point is specified by a label followed by a colon, and the instruction` |
|        - |  548 | ` *   is given as goto followed by the desired target label. This is not a full unrestricted goto.` |
|        - |  549 | ` *   The target label must be within the same file and context, meaning that you cannot jump out` |
|        - |  550 | ` *   of a function or method, nor can you jump into one. You also cannot jump into any sort of loop` |
|        - |  551 | ` *   or switch structure. You may jump out of these, and a common use is to use a goto in place` |
|        - |  552 | ` *   of a multi-level break` |
|        - |  553 | ` */` |
|      224 |  554 | `PH7_PRIVATE sxi32 PH7_CompileGoto(ph7_gen_state *pGen)` |
|        5 |  555 | `{` |
|        - |  556 | `	JumpFixup sJump;` |
|        - |  557 | `	sxi32 rc;` |
|      229 |  558 | `	pGen->pIn++; /* Jump the 'goto' keyword */` |
|      229 |  559 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - |  560 | `		/* Missing label */` |
|      ! 0 |  561 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"goto: expecting a 'label_name'");` |
|      ! 0 |  562 | `		if( rc == SXERR_ABORT ){` |
|        - |  563 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  564 | `			return SXERR_ABORT;` |
|        - |  565 | `		}` |
|      ! 0 |  566 | `		return SXRET_OK;` |
|        - |  567 | `	}` |
|      229 |  568 | `	if( (pGen->pIn->nType & (PH7_TK_KEYWORD\|PH7_TK_ID)) == 0 ){` |
|        6 |  569 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn,"identifier");` |
|        6 |  570 | `		if( rc == SXERR_ABORT ){` |
|        - |  571 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  572 | `			return SXERR_ABORT;` |
|        - |  573 | `		}` |
|        4 |  574 | `	}else{` |
|      225 |  575 | `		SyString *pTarget = &pGen->pIn->sData;` |
|        - |  576 | `		char *zDup;` |
|        - |  577 | `		/* Prepare the jump destination */` |
|      225 |  578 | `		sJump.nJumpType = PH7_OP_JMP;` |
|      225 |  579 | `		sJump.nLine = pGen->pIn->nLine;` |
|        - |  580 | `		/* Gotos resolve at end of compilation, well after any container swap. */` |
|      225 |  581 | `		sJump.pContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      225 |  582 | `		sJump.nScopeId = pGen->nCurScopeId;` |
|        - |  583 | `		/* Duplicate label name */` |
|      225 |  584 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pTarget->zString,pTarget->nByte);` |
|      225 |  585 | `		if( zDup == 0 ){` |
|      ! 0 |  586 | `			PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 |  587 | `			return SXERR_ABORT;` |
|        - |  588 | `		}` |
|      225 |  589 | `		SyStringInitFromBuf(&sJump.sLabel,zDup,pTarget->nByte);` |
|        - |  590 | `		/* The loop/switch this goto sits in, for the "goto into a loop" check later. */` |
|      225 |  591 | `		sJump.nLoopId = pGen->nCurLoopId;` |
|        - |  592 | `		/* A goto inside a try{}/catch{} is legal php (jumping OUT of the block is fine);` |
|        - |  593 | `		 * only the owning function matters here, since a goto may not cross functions. */` |
|      225 |  594 | `		sJump.pFunc = GenStateOwningFunc(&(*pGen));` |
|        - |  595 | `		/* Emit the unconditional jump. Inside a DETACHED catch/finally body the target may` |
|        - |  596 | `		 * lie in another bytecode array, which a plain OP_JMP cannot address; enclosing` |
|        - |  597 | `		 * trys likewise need their finally run on the way out, which a plain jump would` |
|        - |  598 | `		 * skip. Emit a structure-crossing jump whenever either is possible — the label is` |
|        - |  599 | `		 * not known yet, so GenStateFixGoto picks the final opcode (and may downgrade it` |
|        - |  600 | `		 * back to OP_JMP once the counts prove to cancel). */` |
|      335 |  601 | `		if( SXRET_OK == PH7_VmEmitInstr(pGen->pVm,` |
|      220 |  602 | `			sJump.nScopeId > 0 ? PH7_OP_CATCH_JMP : PH7_OP_JMP,0,0,0,&sJump.nInstrIdx) ){` |
|      225 |  603 | `			SySetPut(&pGen->aGoto,(const void *)&sJump);` |
|      110 |  604 | `		}` |
|        - |  605 | `	}` |
|      229 |  606 | `	pGen->pIn++; /* Jump the label name */` |
|      229 |  607 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        3 |  608 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Expected semi-colon ';' after 'goto' statement");` |
|        1 |  609 | `	}` |
|        - |  610 | `	/* Statement successfully compiled */` |
|      229 |  611 | `	return SXRET_OK;` |
|      117 |  612 | `}` |
|        - |  613 | `/*` |
|        - |  614 | ` * Point to the next PHP chunk that will be processed shortly.` |
|        - |  615 | ` * Return SXRET_OK on success. Any other return value indicates` |
|        - |  616 | ` * failure.` |
|        - |  617 | ` */` |
|       20 |  618 | `static sxi32 GenStateNextChunk(ph7_gen_state *pGen)` |
|        2 |  619 | `{` |
|        - |  620 | `	ph7_value *pRawObj; /* Raw chunk [i.e: HTML,XML...] */` |
|        - |  621 | `	sxu32 nRawObj;` |
|       10 |  622 | `	sxu32 nObjIdx;` |
|        - |  623 | `	/* Consume raw chunks verbatim without any processing until we get` |
|        - |  624 | `	 * a PHP block.` |
|        - |  625 | `	 */` |
|       10 |  626 | `Consume:` |
|       22 |  627 | `	nRawObj = nObjIdx = 0;` |
|       22 |  628 | `	while( pGen->pRawIn < pGen->pRawEnd && pGen->pRawIn->nType != PH7_TOKEN_PHP ){` |
|      ! 0 |  629 | `		pRawObj = PH7_ReserveConstObj(pGen->pVm,&nObjIdx);` |
|      ! 0 |  630 | `		if( pRawObj == 0 ){` |
|      ! 0 |  631 | `			PH7_GenCompileError(pGen,E_ERROR,1,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 |  632 | `			return SXERR_ABORT;` |
|        - |  633 | `		}` |
|        - |  634 | `		/* Mark as constant and emit the load constant instruction */` |
|      ! 0 |  635 | `		PH7_MemObjInitFromString(pGen->pVm,pRawObj,&pGen->pRawIn->sData);` |
|      ! 0 |  636 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nObjIdx,0,0);` |
|      ! 0 |  637 | `		++nRawObj;` |
|      ! 0 |  638 | `		pGen->pRawIn++; /* Next chunk */` |
|      ! 0 |  639 | `	}` |
|       22 |  640 | `	if( nRawObj > 0 ){` |
|        - |  641 | `		/* Emit the consume instruction */` |
|      ! 0 |  642 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,nRawObj,0,0,0);` |
|      ! 0 |  643 | `	}` |
|       22 |  644 | `	if( pGen->pRawIn < pGen->pRawEnd ){` |
|      ! 0 |  645 | `		SySet *pTokenSet = pGen->pTokenSet;` |
|        - |  646 | `		/* Reset the token set (and its trivia sidecar) */` |
|      ! 0 |  647 | `		SySetReset(pTokenSet);` |
|      ! 0 |  648 | `		SySetReset(&pGen->aTrivia);` |
|        - |  649 | `		/* Tokenize input */` |
|      ! 0 |  650 | `		PH7_TokenizePHP(SyStringData(&pGen->pRawIn->sData),SyStringLength(&pGen->pRawIn->sData),` |
|      ! 0 |  651 | `			pGen->pRawIn->nLine,pTokenSet,&pGen->aTrivia);` |
|        - |  652 | `		/* Point to the fresh token stream */` |
|      ! 0 |  653 | `		pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|      ! 0 |  654 | `		pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|        - |  655 | `		/* Advance the stream cursor */` |
|      ! 0 |  656 | `		pGen->pRawIn++;` |
|        - |  657 | `		/* TICKET 1433-011 */` |
|      ! 0 |  658 | `		if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|        - |  659 | `			static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|        - |  660 | `			sxi32 rc;` |
|        - |  661 | `			/* Refer to TICKET 1433-009  */` |
|      ! 0 |  662 | `			pGen->pIn->nType = PH7_TK_KEYWORD;` |
|      ! 0 |  663 | `			pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|      ! 0 |  664 | `			SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|        - |  665 | `			/* Synthesized short-tag echo: legitimately an expression here. */` |
|      ! 0 |  666 | `			pGen->nExprEchoOk++;` |
|      ! 0 |  667 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|      ! 0 |  668 | `			pGen->nExprEchoOk--;` |
|      ! 0 |  669 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  670 | `				return SXERR_ABORT;` |
|      ! 0 |  671 | `			}else if( rc != SXERR_EMPTY ){` |
|      ! 0 |  672 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      ! 0 |  673 | `			}` |
|      ! 0 |  674 | `			goto Consume;` |
|        - |  675 | `		}` |
|      ! 0 |  676 | `	}else{` |
|        - |  677 | `		/* No more chunks to process */` |
|       22 |  678 | `		pGen->pIn = pGen->pEnd;` |
|       22 |  679 | `		return SXERR_EOF;` |
|        - |  680 | `	}` |
|      ! 0 |  681 | `	return SXRET_OK;` |
|       12 |  682 | `}` |
|        - |  683 | `/*` |
|        - |  684 | ` * Compile a PHP block.` |
|        - |  685 | ` * A block is simply one or more PHP statements and expressions to compile` |
|        - |  686 | ` * optionally delimited by braces {}.` |
|        - |  687 | ` * Return SXRET_OK on success. Any other return value indicates failure` |
|        - |  688 | ` * and this function takes care of generating the appropriate error` |
|        - |  689 | ` * message.` |
|        - |  690 | ` */` |
|  7985156 |  691 | `PH7_PRIVATE sxi32 PH7_CompileBlock(` |
|        - |  692 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |  693 | `	sxi32 nKeywordEnd    /* EOF-keyword [i.e: endif;endfor;...]. 0 (zero) otherwise */` |
|        - |  694 | `	)` |
|        5 |  695 | `{` |
|        - |  696 | `	sxi32 rc;` |
|        - |  697 | `	sxu32 nLine;` |
|  7985161 |  698 | `	if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|  7839279 |  699 | `		nLine = pGen->pIn->nLine;` |
|  7839279 |  700 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|  7839279 |  701 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  702 | `			return SXERR_ABORT;` |
|        - |  703 | `		}` |
|  7839279 |  704 | `		pGen->pIn++;` |
|        - |  705 | `		/* Compile until we hit the closing braces '}' */` |
| 11556854 |  706 | `		for(;;){` |
| 23113713 |  707 | `			if( pGen->pIn >= pGen->pEnd ){` |
|       22 |  708 | `				rc = GenStateNextChunk(&(*pGen));` |
|       22 |  709 | `				if (rc == SXERR_ABORT ){` |
|      ! 0 |  710 | `			 	   return SXERR_ABORT;` |
|        - |  711 | `				}` |
|       22 |  712 | `				if( rc == SXERR_EOF ){` |
|        - |  713 | `					/* No more token to process: the block was never closed. php reports` |
|        - |  714 | `					 * the line the '{' was opened on, not where the input ran out. */` |
|       22 |  715 | `					PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"Unclosed '{' on line %u",nLine);` |
|       22 |  716 | `					break;` |
|        - |  717 | `				}` |
|      ! 0 |  718 | `			}` |
| 23113693 |  719 | `			if( pGen->pIn->nType & PH7_TK_CCB/*'}'*/ ){` |
|        - |  720 | `				/* Closing braces found,break immediately*/` |
|  7839259 |  721 | `				pGen->pIn++;` |
|  7839259 |  722 | `				break;` |
|        - |  723 | `			}` |
|        - |  724 | `			/* Compile a single statement */` |
| 15274439 |  725 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
| 15274439 |  726 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  727 | `				return SXERR_ABORT;` |
|        - |  728 | `			}` |
|        5 |  729 | `		}` |
|  7839279 |  730 | `		GenStateLeaveBlock(&(*pGen),0);` |
|  4065524 |  731 | `	}else if( (pGen->pIn->nType & PH7_TK_COLON /* ':' */) && nKeywordEnd > 0 ){` |
|        9 |  732 | `		pGen->pIn++;` |
|        9 |  733 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_STD,PH7_VmInstrLength(pGen->pVm),0,0);` |
|        9 |  734 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  735 | `			return SXERR_ABORT;` |
|        - |  736 | `		}` |
|        - |  737 | `		/* Compile until we hit the EOF-keyword [i.e: endif;endfor;...] */` |
|        9 |  738 | `		for(;;){` |
|       19 |  739 | `			if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 |  740 | `				rc = GenStateNextChunk(&(*pGen));` |
|      ! 0 |  741 | `				if (rc == SXERR_ABORT ){` |
|      ! 0 |  742 | `			 	   return SXERR_ABORT;` |
|        - |  743 | `				}` |
|      ! 0 |  744 | `				if( rc == SXERR_EOF \|\| pGen->pIn >= pGen->pEnd ){` |
|        - |  745 | `					/* No more token to process */` |
|      ! 0 |  746 | `					if( rc == SXERR_EOF ){` |
|      ! 0 |  747 | `						PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pEnd[-1].nLine,` |
|        - |  748 | `							"Missing 'endfor;','endwhile;','endswitch;' or 'endforeach;' keyword");` |
|      ! 0 |  749 | `					}` |
|      ! 0 |  750 | `					break;` |
|        - |  751 | `				}` |
|      ! 0 |  752 | `			}` |
|       19 |  753 | `			if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|        - |  754 | `				sxi32 nKwrd;` |
|        - |  755 | `				/* Keyword found */` |
|       17 |  756 | `				nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       18 |  757 | `				if( nKwrd == nKeywordEnd \|\|` |
|        5 |  758 | `					(nKeywordEnd == PH7_TKWRD_ENDIF && (nKwrd == PH7_TKWRD_ELSE \|\| nKwrd == PH7_TKWRD_ELIF)) ){` |
|        - |  759 | `						/* Delimiter keyword found,break */` |
|        9 |  760 | `						if( nKwrd != PH7_TKWRD_ELSE && nKwrd != PH7_TKWRD_ELIF ){` |
|        9 |  761 | `							pGen->pIn++; /*  endif;endswitch... */` |
|        4 |  762 | `						}` |
|        9 |  763 | `						break;` |
|        - |  764 | `				}` |
|        4 |  765 | `			}` |
|        - |  766 | `			/* Compile a single statement */` |
|       11 |  767 | `			rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|       11 |  768 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  769 | `				return SXERR_ABORT;` |
|        - |  770 | `			}` |
|        1 |  771 | `		}` |
|        9 |  772 | `		GenStateLeaveBlock(&(*pGen),0);` |
|        5 |  773 | `	}else{` |
|        - |  774 | `		/* Compile a single statement */` |
|   145879 |  775 | `		rc = GenStateCompileChunk(&(*pGen),PH7_COMPILE_SINGLE_STMT);` |
|   145879 |  776 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  777 | `			return SXERR_ABORT;` |
|        - |  778 | `		}` |
|        - |  779 | `	}` |
|        - |  780 | `	/* Jump trailing semi-colons ';' */` |
|  7985169 |  781 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        9 |  782 | `		pGen->pIn++;` |
|        1 |  783 | `	}` |
|  7985161 |  784 | `	return SXRET_OK;` |
|  3992583 |  785 | `}` |
|        - |  786 | `/*` |
|        - |  787 | ` * Compile the gentle 'while' statement.` |
|        - |  788 | ` * According to the PHP language reference` |
|        - |  789 | ` *  while loops are the simplest type of loop in PHP.They behave just like their C counterparts.` |
|        - |  790 | ` *  The basic form of a while statement is:` |
|        - |  791 | ` *  while (expr)` |
|        - |  792 | ` *   statement` |
|        - |  793 | ` *  The meaning of a while statement is simple. It tells PHP to execute the nested statement(s)` |
|        - |  794 | ` *  repeatedly, as long as the while expression evaluates to TRUE. The value of the expression` |
|        - |  795 | ` *  is checked each time at the beginning of the loop, so even if this value changes during` |
|        - |  796 | ` *  the execution of the nested statement(s), execution will not stop until the end of the iteration` |
|        - |  797 | ` *  (each time PHP runs the statements in the loop is one iteration). Sometimes, if the while` |
|        - |  798 | ` *  expression evaluates to FALSE from the very beginning, the nested statement(s) won't even be run once.` |
|        - |  799 | ` *  Like with the if statement, you can group multiple statements within the same while loop by surrounding` |
|        - |  800 | ` *  a group of statements with curly braces, or by using the alternate syntax:` |
|        - |  801 | ` *  while (expr):` |
|        - |  802 | ` *    statement` |
|        - |  803 | ` *   endwhile;` |
|        - |  804 | ` */` |
|    86194 |  805 | `PH7_PRIVATE sxi32 PH7_CompileWhile(ph7_gen_state *pGen)` |
|        5 |  806 | `{` |
|    86199 |  807 | `	GenBlock *pWhileBlock = 0;` |
|    86199 |  808 | `	SyToken *pTmp,*pEnd = 0;` |
|        - |  809 | `	sxu32 nFalseJump;` |
|        - |  810 | `	sxu32 nLine;` |
|        - |  811 | `	sxi32 rc;` |
|    86199 |  812 | `	nLine = pGen->pIn->nLine;` |
|        - |  813 | `	/* Jump the 'while' keyword */` |
|    86199 |  814 | `	pGen->pIn++;` |
|    86199 |  815 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  816 | `		/* Syntax error */` |
|      ! 0 |  817 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|      ! 0 |  818 | `		if( rc == SXERR_ABORT ){` |
|        - |  819 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  820 | `			return SXERR_ABORT;` |
|        - |  821 | `		}` |
|      ! 0 |  822 | `		goto Synchronize;` |
|        - |  823 | `	}` |
|        - |  824 | `	/* Jump the left parenthesis '(' */` |
|    86199 |  825 | `	pGen->pIn++;` |
|        - |  826 | `	/* Create the loop block */` |
|    86199 |  827 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pWhileBlock);` |
|    86199 |  828 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  829 | `		return SXERR_ABORT;` |
|        - |  830 | `	}` |
|        - |  831 | `	/* Delimit the condition */` |
|    86199 |  832 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    86199 |  833 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  834 | `		/* Empty condition. php reports the token that actually stopped it -- for` |
|        - |  835 | ``		 * `while ()` that is the ')' -- not a hand-written "expected expression". */`` |
|        3 |  836 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,0);` |
|        3 |  837 | `		if( rc == SXERR_ABORT ){` |
|        - |  838 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  839 | `			return SXERR_ABORT;` |
|        - |  840 | `		}` |
|        1 |  841 | `	}` |
|        - |  842 | `	/* Swap token streams */` |
|    86199 |  843 | `	pTmp = pGen->pEnd;` |
|    86199 |  844 | `	pGen->pEnd = pEnd;` |
|        - |  845 | `	/* Compile the expression */` |
|    86199 |  846 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    86199 |  847 | `	if( rc == SXERR_ABORT ){` |
|        - |  848 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 |  849 | `		return SXERR_ABORT;` |
|        - |  850 | `	}` |
|        - |  851 | `	/* Update token stream */` |
|    86199 |  852 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 |  853 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 |  854 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  855 | `			return SXERR_ABORT;` |
|        - |  856 | `		}` |
|      ! 0 |  857 | `		pGen->pIn++;` |
|      ! 0 |  858 | `	}` |
|        - |  859 | `	/* Synchronize pointers */` |
|    86199 |  860 | `	pGen->pIn  = &pEnd[1];` |
|    86199 |  861 | `	pGen->pEnd = pTmp;` |
|        - |  862 | `	/* Emit the false jump */` |
|    86199 |  863 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|        - |  864 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|    86199 |  865 | `	GenStateNewJumpFixup(pWhileBlock,PH7_OP_JZ,nFalseJump);` |
|        - |  866 | `	/* Compile the loop body */` |
|    86199 |  867 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDWHILE);` |
|    86199 |  868 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  869 | `		return SXERR_ABORT;` |
|        - |  870 | `	}` |
|        - |  871 | `	/* Emit the unconditional jump to the start of the loop */` |
|    86199 |  872 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pWhileBlock->nFirstInstr,0,0);` |
|        - |  873 | `	/* Fix all jumps now the destination is resolved */` |
|    86199 |  874 | `	GenStateFixJumps(pWhileBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - |  875 | `	/* Release the loop block */` |
|    86199 |  876 | `	GenStateLeaveBlock(pGen,0);` |
|        - |  877 | `	/* Statement successfully compiled */` |
|    86199 |  878 | `	return SXRET_OK;` |
|      ! 0 |  879 | `Synchronize:` |
|        - |  880 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - |  881 | `	 * compiling this erroneous block.` |
|        - |  882 | `	 */` |
|      ! 0 |  883 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 |  884 | `		pGen->pIn++;` |
|      ! 0 |  885 | `	}` |
|      ! 0 |  886 | `	return SXRET_OK;` |
|    43102 |  887 | `}` |
|        - |  888 | `/*` |
|        - |  889 | ` * Compile the ugly do..while() statement.` |
|        - |  890 | ` * According to the PHP language reference` |
|        - |  891 | ` *  do-while loops are very similar to while loops, except the truth expression is checked` |
|        - |  892 | ` *  at the end of each iteration instead of in the beginning. The main difference from regular` |
|        - |  893 | ` *  while loops is that the first iteration of a do-while loop is guaranteed to run` |
|        - |  894 | ` *  (the truth expression is only checked at the end of the iteration), whereas it may not` |
|        - |  895 | ` *  necessarily run with a regular while loop (the truth expression is checked at the beginning` |
|        - |  896 | ` *  of each iteration, if it evaluates to FALSE right from the beginning, the loop execution` |
|        - |  897 | ` *  would end immediately).` |
|        - |  898 | ` *  There is just one syntax for do-while loops:` |
|        - |  899 | ` *  <?php` |
|        - |  900 | ` *  $i = 0;` |
|        - |  901 | ` *  do {` |
|        - |  902 | ` *   echo $i;` |
|        - |  903 | ` *  } while ($i > 0);` |
|        - |  904 | ` * ?>` |
|        - |  905 | ` */` |
|        8 |  906 | `PH7_PRIVATE sxi32 PH7_CompileDoWhile(ph7_gen_state *pGen)` |
|        3 |  907 | `{` |
|       11 |  908 | `	SyToken *pTmp,*pEnd = 0;` |
|       11 |  909 | `	GenBlock *pDoBlock = 0;` |
|        - |  910 | `	sxu32 nLine;` |
|        - |  911 | `	sxi32 rc;` |
|       11 |  912 | `	nLine = pGen->pIn->nLine;` |
|        - |  913 | `	/* Jump the 'do' keyword */` |
|       11 |  914 | `	pGen->pIn++;` |
|        - |  915 | `	/* Create the loop block */` |
|       11 |  916 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pDoBlock);` |
|       11 |  917 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  918 | `		return SXERR_ABORT;` |
|        - |  919 | `	}` |
|        - |  920 | `	/* Deffer 'continue;' jumps until we compile the block */` |
|       11 |  921 | `	pDoBlock->bPostContinue = TRUE;` |
|       11 |  922 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|       11 |  923 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  924 | `		return SXERR_ABORT;` |
|        - |  925 | `	}` |
|       11 |  926 | `	if( pGen->pIn < pGen->pEnd ){` |
|        9 |  927 | `		nLine = pGen->pIn->nLine;` |
|        3 |  928 | `	}` |
|       11 |  929 | `	if( pGen->pIn >= pGen->pEnd \|\| pGen->pIn->nType != PH7_TK_KEYWORD \|\|` |
|        6 |  930 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_WHILE ){` |
|        - |  931 | `			/* Missing 'while' statement */` |
|        - |  932 | ``			/* php: `do {} ;` names the ';', while `do {}` at EOF is "unexpected end of`` |
|        - |  933 | `			 * file". The do-block's own slice stops before its terminator, so look at` |
|        - |  934 | `			 * the whole CHUNK stream: a token still there is the one php names; nothing` |
|        - |  935 | `			 * left means end of file (NULL). */` |
|        - |  936 | `			{` |
|        3 |  937 | `				SyToken *pBad = pGen->pIn < pGen->pEnd ? pGen->pIn : 0;` |
|        3 |  938 | `				if( pBad == 0 && pGen->pTokenSet ){` |
|        - |  939 | `					/* The do-block consumed its terminator, so the token php names is` |
|        - |  940 | `					 * the one just behind the cursor -- but only when it is a real` |
|        - |  941 | ``					 * terminator (`do {} ;` -> the ';'). If the block simply ended on`` |
|        - |  942 | `					 * its '}' with nothing after it, php reports end of file. */` |
|        3 |  943 | `					SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        3 |  944 | `					if( pGen->pIn > pBase && (pGen->pIn[-1].nType & PH7_TK_SEMI) ){` |
|      ! 0 |  945 | `						pBad = &pGen->pIn[-1];` |
|      ! 0 |  946 | `					}` |
|        1 |  947 | `				}` |
|        3 |  948 | `				rc = PH7_GenSyntaxError(pGen,pBad,"\"while\"");` |
|        - |  949 | `			}` |
|        3 |  950 | `			if( rc == SXERR_ABORT ){` |
|        - |  951 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 |  952 | `				return SXERR_ABORT;` |
|        - |  953 | `			}` |
|        3 |  954 | `			goto Synchronize;` |
|        - |  955 | `	}` |
|        - |  956 | `	/* Jump the 'while' keyword */` |
|        9 |  957 | `	pGen->pIn++;` |
|        9 |  958 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - |  959 | `		/* Syntax error */` |
|      ! 0 |  960 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'while' keyword");` |
|      ! 0 |  961 | `		if( rc == SXERR_ABORT ){` |
|        - |  962 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  963 | `			return SXERR_ABORT;` |
|        - |  964 | `		}` |
|      ! 0 |  965 | `		goto Synchronize;` |
|        - |  966 | `	}` |
|        - |  967 | `	/* Jump the left parenthesis '(' */` |
|        9 |  968 | `	pGen->pIn++;` |
|        - |  969 | `	/* Delimit the condition */` |
|        9 |  970 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|        9 |  971 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - |  972 | `		/* Empty condition. php reports the token that actually stopped it -- for` |
|        - |  973 | ``		 * `while ()` that is the ')' -- not a hand-written "expected expression". */`` |
|      ! 0 |  974 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,0);` |
|      ! 0 |  975 | `		if( rc == SXERR_ABORT ){` |
|        - |  976 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 |  977 | `			return SXERR_ABORT;` |
|        - |  978 | `		}` |
|      ! 0 |  979 | `		goto Synchronize;` |
|        - |  980 | `	}` |
|        - |  981 | `	/* Fix post-continue jumps now the jump destination is resolved */` |
|        9 |  982 | `	if( SySetUsed(&pDoBlock->aPostContFix) > 0 ){` |
|        - |  983 | `		JumpFixup *aPost;` |
|        - |  984 | `		VmInstr *pInstr;` |
|        - |  985 | `		sxu32 nJumpDest;` |
|        - |  986 | `		sxu32 n;` |
|        3 |  987 | `		aPost = (JumpFixup *)SySetBasePtr(&pDoBlock->aPostContFix);` |
|        3 |  988 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|        5 |  989 | `		for( n = 0 ; n < SySetUsed(&pDoBlock->aPostContFix) ; ++n ){` |
|        3 |  990 | `			pInstr = GenStateFixupInstr(&aPost[n]);` |
|        3 |  991 | `			if( pInstr ){` |
|        - |  992 | `				/* Fix */` |
|        3 |  993 | `				pInstr->iP2 = nJumpDest;` |
|        1 |  994 | `			}` |
|        2 |  995 | `		}` |
|        1 |  996 | `	}` |
|        - |  997 | `	/* Swap token streams */` |
|        9 |  998 | `	pTmp = pGen->pEnd;` |
|        9 |  999 | `	pGen->pEnd = pEnd;` |
|        - | 1000 | `	/* Compile the expression */` |
|        9 | 1001 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        9 | 1002 | `	if( rc == SXERR_ABORT ){` |
|        - | 1003 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1004 | `		return SXERR_ABORT;` |
|        - | 1005 | `	}` |
|        - | 1006 | `	/* Update token stream */` |
|        9 | 1007 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 | 1008 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 | 1009 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1010 | `			return SXERR_ABORT;` |
|        - | 1011 | `		}` |
|      ! 0 | 1012 | `		pGen->pIn++;` |
|      ! 0 | 1013 | `	}` |
|        9 | 1014 | `	pGen->pIn  = &pEnd[1];` |
|        9 | 1015 | `	pGen->pEnd = pTmp;` |
|        - | 1016 | `	/* Emit the true jump to the beginning of the loop */` |
|        9 | 1017 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,0,pDoBlock->nFirstInstr,0,0);` |
|        - | 1018 | `	/* Fix all jumps now the destination is resolved */` |
|        9 | 1019 | `	GenStateFixJumps(pDoBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 1020 | `	/* Release the loop block */` |
|        9 | 1021 | `	GenStateLeaveBlock(pGen,0);` |
|        - | 1022 | `	/* Statement successfully compiled */` |
|        9 | 1023 | `	return SXRET_OK;` |
|        1 | 1024 | `Synchronize:` |
|        - | 1025 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - | 1026 | `	 * compiling this erroneous block.` |
|        - | 1027 | `	 */` |
|        3 | 1028 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 | 1029 | `		pGen->pIn++;` |
|      ! 0 | 1030 | `	}` |
|        3 | 1031 | `	return SXRET_OK;` |
|        7 | 1032 | `}` |
|        - | 1033 | `/*` |
|        - | 1034 | ` * Compile the complex and powerful 'for' statement.` |
|        - | 1035 | ` * According to the PHP language reference` |
|        - | 1036 | ` *  for loops are the most complex loops in PHP. They behave like their C counterparts.` |
|        - | 1037 | ` *  The syntax of a for loop is:` |
|        - | 1038 | ` *  for (expr1; expr2; expr3)` |
|        - | 1039 | ` *   statement` |
|        - | 1040 | ` *  The first expression (expr1) is evaluated (executed) once unconditionally at` |
|        - | 1041 | ` *  the beginning of the loop.` |
|        - | 1042 | ` *  In the beginning of each iteration, expr2 is evaluated. If it evaluates to` |
|        - | 1043 | ` *  TRUE, the loop continues and the nested statement(s) are executed. If it evaluates` |
|        - | 1044 | ` *  to FALSE, the execution of the loop ends.` |
|        - | 1045 | ` *  At the end of each iteration, expr3 is evaluated (executed).` |
|        - | 1046 | ` *  Each of the expressions can be empty or contain multiple expressions separated by commas.` |
|        - | 1047 | ` *  In expr2, all expressions separated by a comma are evaluated but the result is taken` |
|        - | 1048 | ` *  from the last part. expr2 being empty means the loop should be run indefinitely` |
|        - | 1049 | ` *  (PHP implicitly considers it as TRUE, like C). This may not be as useless as you might` |
|        - | 1050 | ` *  think, since often you'd want to end the loop using a conditional break statement instead` |
|        - | 1051 | ` *  of using the for truth expression.` |
|        - | 1052 | ` */` |
|   145094 | 1053 | `PH7_PRIVATE sxi32 PH7_CompileFor(ph7_gen_state *pGen)` |
|        5 | 1054 | `{` |
|   145099 | 1055 | `	SyToken *pTmp,*pPostStart,*pEnd = 0;` |
|   145099 | 1056 | `	GenBlock *pForBlock = 0;` |
|        - | 1057 | `	sxu32 nFalseJump;` |
|        - | 1058 | `	sxu32 nLine;` |
|        - | 1059 | `	sxi32 rc;` |
|   145099 | 1060 | `	nLine = pGen->pIn->nLine;` |
|        - | 1061 | `	/* Jump the 'for' keyword */` |
|   145099 | 1062 | `	pGen->pIn++;` |
|   145099 | 1063 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1064 | `		/* Syntax error */` |
|      ! 0 | 1065 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'for' keyword");` |
|      ! 0 | 1066 | `		if( rc == SXERR_ABORT ){` |
|        - | 1067 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1068 | `			return SXERR_ABORT;` |
|        - | 1069 | `		}` |
|      ! 0 | 1070 | `		return SXRET_OK;` |
|        - | 1071 | `	}` |
|        - | 1072 | `	/* Jump the left parenthesis '(' */` |
|   145099 | 1073 | `	pGen->pIn++;` |
|        - | 1074 | `	/* Delimit the init-expr;condition;post-expr */` |
|   145099 | 1075 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   145099 | 1076 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - | 1077 | `		/* Empty expression */` |
|      ! 0 | 1078 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"for: Invalid expression");` |
|      ! 0 | 1079 | `		if( rc == SXERR_ABORT ){` |
|        - | 1080 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1081 | `			return SXERR_ABORT;` |
|        - | 1082 | `		}` |
|        - | 1083 | `		/* Synchronize */` |
|      ! 0 | 1084 | `		pGen->pIn = pEnd;` |
|      ! 0 | 1085 | `		if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 | 1086 | `			pGen->pIn++;` |
|      ! 0 | 1087 | `		}` |
|      ! 0 | 1088 | `		return SXRET_OK;` |
|        - | 1089 | `	}` |
|        - | 1090 | `	/* Swap token streams */` |
|   145099 | 1091 | `	pTmp = pGen->pEnd;` |
|   145099 | 1092 | `	pGen->pEnd = pEnd;` |
|        - | 1093 | `	/* for() clauses are the ONLY place php's grammar allows a comma-separated` |
|        - | 1094 | `	 * expression list, so the comma operator is permitted for their duration` |
|        - | 1095 | `	 * (see GenStateTreeHasComma). A closure body nested inside a clause is` |
|        - | 1096 | `	 * compiled through this same window — recorded as a known leniency. */` |
|   145099 | 1097 | `	pGen->nCommaExprOk++;` |
|   145099 | 1098 | `	pGen->zClauseCloser = "\";\""; /* init/condition clauses close on ';' */` |
|        - | 1099 | `	/* Compile initialization expressions if available */` |
|   145099 | 1100 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 1101 | `	/* Pop operand lvalues */` |
|   145099 | 1102 | `	if( rc == SXERR_ABORT ){` |
|        - | 1103 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1104 | `		return SXERR_ABORT;` |
|   145099 | 1105 | `	}else if( rc != SXERR_EMPTY ){` |
|   131513 | 1106 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    65754 | 1107 | `	}` |
|   145099 | 1108 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - | 1109 | `		/* Syntax error */` |
|      ! 0 | 1110 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|      ! 0 | 1111 | `		if( rc == SXERR_ABORT ){` |
|        - | 1112 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1113 | `			return SXERR_ABORT;` |
|        - | 1114 | `		}` |
|      ! 0 | 1115 | `		return SXRET_OK;` |
|        - | 1116 | `	}` |
|        - | 1117 | `	/* Jump the trailing ';' */` |
|   145099 | 1118 | `	pGen->pIn++;` |
|        - | 1119 | `	/* Create the loop block */` |
|   145099 | 1120 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForBlock);` |
|   145099 | 1121 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1122 | `		return SXERR_ABORT;` |
|        - | 1123 | `	}` |
|        - | 1124 | `	/* Deffer continue jumps */` |
|   145099 | 1125 | `	pForBlock->bPostContinue = TRUE;` |
|        - | 1126 | `	/* Compile the condition */` |
|   145099 | 1127 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   145099 | 1128 | `	if( rc == SXERR_ABORT ){` |
|        - | 1129 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1130 | `		return SXERR_ABORT;` |
|   145099 | 1131 | `	}else if( rc != SXERR_EMPTY ){` |
|        - | 1132 | `		/* Emit the false jump */` |
|   131513 | 1133 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nFalseJump);` |
|        - | 1134 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   131513 | 1135 | `		GenStateNewJumpFixup(pForBlock,PH7_OP_JZ,nFalseJump);` |
|    65754 | 1136 | `	}` |
|   145099 | 1137 | `	if( (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - | 1138 | `		/* Syntax error */` |
|        6 | 1139 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|        6 | 1140 | `		if( rc == SXERR_ABORT ){` |
|        - | 1141 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1142 | `			return SXERR_ABORT;` |
|        - | 1143 | `		}` |
|        6 | 1144 | `		return SXRET_OK;` |
|        - | 1145 | `	}` |
|        - | 1146 | `	/* Jump the trailing ';' */` |
|   145095 | 1147 | `	pGen->pIn++;` |
|        - | 1148 | `	/* Save the post condition stream */` |
|   145095 | 1149 | `	pPostStart = pGen->pIn;` |
|        - | 1150 | `	/* Compile the loop body — OUTSIDE the comma window (the body is ordinary` |
|        - | 1151 | ``	 * php, so `(1, 2)` inside it is the parse error it should be). */`` |
|   145095 | 1152 | `	pGen->nCommaExprOk--;` |
|   145095 | 1153 | `	pGen->pIn  = &pEnd[1]; /* Jump the trailing parenthesis ')' */` |
|   145095 | 1154 | `	pGen->pEnd = pTmp;` |
|   145095 | 1155 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDFOR);` |
|   145095 | 1156 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 1157 | `		return SXERR_ABORT;` |
|        - | 1158 | `	}` |
|        - | 1159 | `	/* Fix post-continue jumps */` |
|   145095 | 1160 | `	if( SySetUsed(&pForBlock->aPostContFix) > 0 ){` |
|        - | 1161 | `		JumpFixup *aPost;` |
|        - | 1162 | `		VmInstr *pInstr;` |
|        - | 1163 | `		sxu32 nJumpDest;` |
|        - | 1164 | `		sxu32 n;` |
|    13605 | 1165 | `		aPost = (JumpFixup *)SySetBasePtr(&pForBlock->aPostContFix);` |
|    13605 | 1166 | `		nJumpDest = PH7_VmInstrLength(pGen->pVm);` |
|    45317 | 1167 | `		for( n = 0 ; n < SySetUsed(&pForBlock->aPostContFix) ; ++n ){` |
|    31717 | 1168 | `			pInstr = GenStateFixupInstr(&aPost[n]);` |
|    31717 | 1169 | `			if( pInstr ){` |
|        - | 1170 | `				/* Fix jump */` |
|    31717 | 1171 | `				pInstr->iP2 = nJumpDest;` |
|    15856 | 1172 | `			}` |
|    15861 | 1173 | `		}` |
|     6800 | 1174 | `	}` |
|        - | 1175 | `	/* compile the post-expressions if available */` |
|   145095 | 1176 | `	while( pPostStart < pEnd && (pPostStart->nType & PH7_TK_SEMI) ){` |
|      ! 0 | 1177 | `		pPostStart++;` |
|      ! 0 | 1178 | `	}` |
|   145095 | 1179 | `	if( pPostStart < pEnd ){` |
|        - | 1180 | `		SyToken *pTmpIn,*pTmpEnd;` |
|   131511 | 1181 | `		SWAP_DELIMITER(pGen,pPostStart,pEnd);` |
|   131511 | 1182 | `		pGen->nCommaExprOk++; /* post-expressions are a clause list again */` |
|   131511 | 1183 | `		pGen->zClauseCloser = "\")\""; /* the post clause closes on ')' */` |
|   131511 | 1184 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   131511 | 1185 | `		pGen->nCommaExprOk--;` |
|   131511 | 1186 | `		pGen->zClauseCloser = 0;` |
|   131511 | 1187 | `		if( pGen->pIn < pGen->pEnd ){` |
|        - | 1188 | `			/* php names the token and expects ')'; the post-clause runs to the ')'. */` |
|      ! 0 | 1189 | `			rc = PH7_GenSyntaxError(pGen,pGen->pIn,"\")\"");` |
|      ! 0 | 1190 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1191 | `				return SXERR_ABORT;` |
|        - | 1192 | `			}` |
|      ! 0 | 1193 | `			return SXRET_OK;` |
|        - | 1194 | `		}` |
|   131511 | 1195 | `		RE_SWAP_DELIMITER(pGen);` |
|   131511 | 1196 | `		if( rc == SXERR_ABORT ){` |
|        - | 1197 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1198 | `			return SXERR_ABORT;` |
|   131511 | 1199 | `		}else if( rc != SXERR_EMPTY){` |
|        - | 1200 | `			/* Pop operand lvalue */` |
|   131511 | 1201 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    65753 | 1202 | `		}` |
|    65753 | 1203 | `	}` |
|        - | 1204 | `	/* Emit the unconditional jump to the start of the loop */` |
|   145095 | 1205 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForBlock->nFirstInstr,0,0);` |
|        - | 1206 | `	/* Fix all jumps now the destination is resolved */` |
|   145095 | 1207 | `	GenStateFixJumps(pForBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 1208 | `	/* Release the loop block */` |
|   145095 | 1209 | `	GenStateLeaveBlock(pGen,0);` |
|        - | 1210 | `	/* Statement successfully compiled */` |
|   145095 | 1211 | `	return SXRET_OK;` |
|    72552 | 1212 | `}` |
|        - | 1213 | `/* Expression tree validator callback used by the 'foreach' statement.` |
|        - | 1214 | ` * Note that only variable expression [i.e: $x; ${'My'.'Var'}; ${$a['key]};...]` |
|        - | 1215 | ` * are allowed.` |
|        - | 1216 | ` */` |
|   531284 | 1217 | `static sxi32 GenStateForEachNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 1218 | `{` |
|   531289 | 1219 | `	sxi32 rc = SXRET_OK; /* Assume a valid expression tree */` |
|   531289 | 1220 | `	if( pRoot->xCode != PH7_CompileVariable ){` |
|        - | 1221 | `		/* Unexpected expression */` |
|      ! 0 | 1222 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pRoot->pStart? pRoot->pStart->nLine : 0,` |
|        - | 1223 | `			"foreach: Expecting a variable name");` |
|      ! 0 | 1224 | `		if( rc != SXERR_ABORT ){` |
|      ! 0 | 1225 | `			rc = SXERR_INVALID;` |
|      ! 0 | 1226 | `		}` |
|      ! 0 | 1227 | `	}` |
|   531289 | 1228 | `	return rc;` |
|        5 | 1229 | `}` |
|        - | 1230 | `/*` |
|        - | 1231 | ` * Compile the 'foreach' statement.` |
|        - | 1232 | ` * According to the PHP language reference` |
|        - | 1233 | ` *  The foreach construct simply gives an easy way to iterate over arrays. foreach works` |
|        - | 1234 | ` *  only on arrays (and objects), and will issue an error when you try to use it on a variable` |
|        - | 1235 | ` *  with a different data type or an uninitialized variable. There are two syntaxes; the second` |
|        - | 1236 | ` *  is a minor but useful extension of the first:` |
|        - | 1237 | ` *  foreach (array_expression as $value)` |
|        - | 1238 | ` *    statement` |
|        - | 1239 | ` *  foreach (array_expression as $key => $value)` |
|        - | 1240 | ` *   statement` |
|        - | 1241 | ` *  The first form loops over the array given by array_expression. On each loop, the value` |
|        - | 1242 | ` *  of the current element is assigned to $value and the internal array pointer is advanced` |
|        - | 1243 | ` *  by one (so on the next loop, you'll be looking at the next element).` |
|        - | 1244 | ` *  The second form does the same thing, except that the current element's key will be assigned` |
|        - | 1245 | ` *  to the variable $key on each loop.` |
|        - | 1246 | ` *  Note:` |
|        - | 1247 | ` *  When foreach first starts executing, the internal array pointer is automatically reset to the` |
|        - | 1248 | ` *  first element of the array. This means that you do not need to call reset() before a foreach loop.` |
|        - | 1249 | ` *  Note:` |
|        - | 1250 | ` *  Unless the array is referenced, foreach operates on a copy of the specified array and not the array` |
|        - | 1251 | ` *  itself. foreach has some side effects on the array pointer. Don't rely on the array pointer during` |
|        - | 1252 | ` *  or after the foreach without resetting it.` |
|        - | 1253 | ` *  You can easily modify array's elements by preceding $value with &. This will assign reference instead` |
|        - | 1254 | ` *  of copying the value.` |
|        - | 1255 | ` */` |
|   376988 | 1256 | `PH7_PRIVATE sxi32 PH7_CompileForeach(ph7_gen_state *pGen)` |
|        5 | 1257 | `{` |
|   376993 | 1258 | `	SyToken *pCur,*pTmp,*pEnd = 0;` |
|   376993 | 1259 | `	SyToken *pListStart = 0,*pListEnd = 0;` |
|   376993 | 1260 | `	GenBlock *pForeachBlock = 0;` |
|        - | 1261 | `	ph7_foreach_info *pInfo;` |
|        - | 1262 | `	sxu32 nFalseJump;` |
|        - | 1263 | `	VmInstr *pInstr;` |
|        - | 1264 | `	sxu32 nLine;` |
|        - | 1265 | `	sxi32 rc;` |
|   376993 | 1266 | `	nLine = pGen->pIn->nLine;` |
|        - | 1267 | `	/* Jump the 'foreach' keyword */` |
|   376993 | 1268 | `	pGen->pIn++;` |
|   376993 | 1269 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1270 | `		/* Syntax error */` |
|      ! 0 | 1271 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Expected '('");` |
|      ! 0 | 1272 | `		if( rc == SXERR_ABORT ){` |
|        - | 1273 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1274 | `			return SXERR_ABORT;` |
|        - | 1275 | `		}` |
|      ! 0 | 1276 | `		goto Synchronize;` |
|        - | 1277 | `	}` |
|        - | 1278 | `	/* Jump the left parenthesis '(' */` |
|   376993 | 1279 | `	pGen->pIn++;` |
|        - | 1280 | `	/* Create the loop block */` |
|   376993 | 1281 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP,PH7_VmInstrLength(pGen->pVm),0,&pForeachBlock);` |
|   376993 | 1282 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1283 | `		return SXERR_ABORT;` |
|        - | 1284 | `	}` |
|        - | 1285 | `	/* Delimit the expression */` |
|   376993 | 1286 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|   376993 | 1287 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - | 1288 | `		/* Empty expression */` |
|      ! 0 | 1289 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"foreach: Missing expression");` |
|      ! 0 | 1290 | `		if( rc == SXERR_ABORT ){` |
|        - | 1291 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 1292 | `			return SXERR_ABORT;` |
|        - | 1293 | `		}` |
|        - | 1294 | `		/* Synchronize */` |
|      ! 0 | 1295 | `		pGen->pIn = pEnd;` |
|      ! 0 | 1296 | `		if( pGen->pIn < pGen->pEnd ){` |
|      ! 0 | 1297 | `			pGen->pIn++;` |
|      ! 0 | 1298 | `		}` |
|      ! 0 | 1299 | `		return SXRET_OK;` |
|        - | 1300 | `	}` |
|        - | 1301 | `	/* Compile the array expression */` |
|   376993 | 1302 | `	pCur = pGen->pIn;` |
|  2150243 | 1303 | `	while( pCur < pEnd ){` |
|  2150243 | 1304 | `		if( pCur->nType & PH7_TK_KEYWORD ){` |
|   413295 | 1305 | `			sxi32 nKeywrd = SX_PTR_TO_INT(pCur->pUserData);` |
|   413295 | 1306 | `			if( nKeywrd == PH7_TKWRD_AS ){` |
|        - | 1307 | `				/* Break with the first 'as' found */` |
|   376993 | 1308 | `				break;` |
|        - | 1309 | `			}` |
|    18151 | 1310 | `		}` |
|        - | 1311 | `		/* Advance the stream cursor */` |
|  1773255 | 1312 | `		pCur++;` |
|        5 | 1313 | `	}` |
|   376993 | 1314 | `	if( pCur <= pGen->pIn ){` |
|      ! 0 | 1315 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 1316 | `			"foreach: Missing array/object expression");` |
|      ! 0 | 1317 | `		if( rc == SXERR_ABORT ){` |
|        - | 1318 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1319 | `			return SXERR_ABORT;` |
|        - | 1320 | `		}` |
|      ! 0 | 1321 | `		goto Synchronize;` |
|        - | 1322 | `	}` |
|        - | 1323 | `	/* Swap token streams */` |
|   376993 | 1324 | `	pTmp = pGen->pEnd;` |
|   376993 | 1325 | `	pGen->pEnd = pCur;` |
|   376993 | 1326 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   376993 | 1327 | `	if( rc == SXERR_ABORT ){` |
|        - | 1328 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 1329 | `		return SXERR_ABORT;` |
|        - | 1330 | `	}` |
|        - | 1331 | `	/* Update token stream */` |
|   376993 | 1332 | `	while(pGen->pIn < pCur ){` |
|      ! 0 | 1333 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 1334 | `		if( rc == SXERR_ABORT ){` |
|        - | 1335 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1336 | `			return SXERR_ABORT;` |
|        - | 1337 | `		}` |
|      ! 0 | 1338 | `		pGen->pIn++;` |
|      ! 0 | 1339 | `	}` |
|   376993 | 1340 | `	pCur++; /* Jump the 'as' keyword */` |
|   376993 | 1341 | `	pGen->pIn = pCur;` |
|   376993 | 1342 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 1343 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key => $value pair");` |
|      ! 0 | 1344 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1345 | `			return SXERR_ABORT;` |
|        - | 1346 | `		}` |
|      ! 0 | 1347 | `	}` |
|        - | 1348 | `	/* Create the foreach context */` |
|   376993 | 1349 | `	pInfo = (ph7_foreach_info *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_foreach_info));` |
|   376993 | 1350 | `	if( pInfo == 0 ){` |
|      ! 0 | 1351 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 engine is running out-of-memory");` |
|      ! 0 | 1352 | `		return SXERR_ABORT;` |
|        - | 1353 | `	}` |
|        - | 1354 | `	/* Zero the structure */` |
|   376993 | 1355 | `	SyZero(pInfo,sizeof(ph7_foreach_info));` |
|        - | 1356 | `	/* Initialize structure fields */` |
|   376993 | 1357 | `	SySetInit(&pInfo->aStep,&pGen->pVm->sAllocator,sizeof(ph7_foreach_step *));` |
|        - | 1358 | `	/* Check if we have a key field. Scan only for a top-level '=>' so a keyed` |
|        - | 1359 | `	 * value target — foreach ($x as ["k" => $v]) — is not split at its inner` |
|        - | 1360 | `	 * '=>'. */` |
|   376993 | 1361 | `	pCur = GenStateFindTopLevelArrow(pCur,pEnd);` |
|   376993 | 1362 | `	if( pCur < pEnd ){` |
|        - | 1363 | `		/* Compile the expression holding the key name */` |
|   154335 | 1364 | `		if( pGen->pIn >= pCur ){` |
|      ! 0 | 1365 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $key");` |
|      ! 0 | 1366 | `			if( rc == SXERR_ABORT ){` |
|        - | 1367 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1368 | `				return SXERR_ABORT;` |
|        - | 1369 | `			}` |
|      ! 0 | 1370 | `		}else{` |
|   154335 | 1371 | `			pGen->pEnd = pCur;` |
|   154335 | 1372 | `			rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|   154335 | 1373 | `			if( rc == SXERR_ABORT ){` |
|        - | 1374 | `				/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1375 | `				return SXERR_ABORT;` |
|        - | 1376 | `			}` |
|   154335 | 1377 | `			pInstr = PH7_VmPopInstr(pGen->pVm);` |
|   154335 | 1378 | `			if( pInstr->p3 ){` |
|        - | 1379 | `				/* Record key name */` |
|   154335 | 1380 | `				SyStringInitFromBuf(&pInfo->sKey,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|    77165 | 1381 | `			}` |
|   154335 | 1382 | `			pInfo->iFlags \|= PH7_4EACH_STEP_KEY;` |
|        - | 1383 | `		}` |
|   154335 | 1384 | `		pGen->pIn = &pCur[1]; /* Jump the arrow */` |
|    77165 | 1385 | `	}` |
|   376993 | 1386 | `	pGen->pEnd = pEnd;` |
|   376993 | 1387 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 1388 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"foreach: Missing $value");` |
|      ! 0 | 1389 | `		if( rc == SXERR_ABORT ){` |
|        - | 1390 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1391 | `			return SXERR_ABORT;` |
|        - | 1392 | `		}` |
|      ! 0 | 1393 | `		goto Synchronize;` |
|        - | 1394 | `	}` |
|   376993 | 1395 | `	if( pGen->pIn->nType & PH7_TK_AMPER /*'&'*/){` |
|       33 | 1396 | `		pGen->pIn++;` |
|        - | 1397 | `		/* Pass by reference  */` |
|       33 | 1398 | `		pInfo->iFlags \|= PH7_4EACH_STEP_REF;` |
|       15 | 1399 | `	}` |
|        - | 1400 | `	/* Check if the value target is list() */` |
|   376993 | 1401 | `	if( (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|        8 | 1402 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_LIST ){` |
|        - | 1403 | `		/* foreach ($arr as list($a, $b)) — list unpacking.` |
|        - | 1404 | `		 * Save the list() token range; we'll compile it after FOREACH_STEP.` |
|        - | 1405 | `		 */` |
|        - | 1406 | `		static int iForeachListCnt = 0;` |
|        - | 1407 | `		char zTmp[128];` |
|        - | 1408 | `		sxu32 nLen;` |
|        - | 1409 | `		char *zDup;` |
|       10 | 1410 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_list_%d__]",iForeachListCnt++);` |
|       10 | 1411 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       10 | 1412 | `		if( zDup == 0 ){` |
|      ! 0 | 1413 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1414 | `			return SXERR_ABORT;` |
|        - | 1415 | `		}` |
|       10 | 1416 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|        - | 1417 | `		/* Save list() token boundaries */` |
|       10 | 1418 | `		pListStart = pGen->pIn;` |
|        - | 1419 | `		/* Advance past list(...) — validate parentheses */` |
|       10 | 1420 | `		pGen->pIn++; /* Jump 'list' keyword */` |
|       10 | 1421 | `		if( pGen->pIn >= pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1422 | `			/* php: syntax error, unexpected variable "$x", expecting "(" */` |
|        3 | 1423 | `			rc = PH7_GenSyntaxError(pGen,pGen->pIn < pEnd ? pGen->pIn : 0,"\"(\"");` |
|        3 | 1424 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1425 | `				return SXERR_ABORT;` |
|        - | 1426 | `			}` |
|        3 | 1427 | `			goto Synchronize;` |
|        - | 1428 | `		}` |
|        7 | 1429 | `		pGen->pIn++; /* Jump '(' */` |
|        7 | 1430 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pListEnd);` |
|        7 | 1431 | `		if( pListEnd >= pEnd ){` |
|      ! 0 | 1432 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1433 | `				"foreach: Missing closing ')' after list");` |
|      ! 0 | 1434 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1435 | `				return SXERR_ABORT;` |
|        - | 1436 | `			}` |
|      ! 0 | 1437 | `			goto Synchronize;` |
|        - | 1438 | `		}` |
|        7 | 1439 | `		pGen->pIn = &pListEnd[1]; /* Past ')' */` |
|        7 | 1440 | `		pListEnd = pGen->pIn;` |
|        7 | 1441 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|   376988 | 1442 | `	}else if( pGen->pIn->nType & PH7_TK_OSB ){` |
|        - | 1443 | `		/* foreach ($arr as [$a, $b]) — short list unpacking.` |
|        - | 1444 | `		 * Save the [...] token range; we'll compile it after FOREACH_STEP.` |
|        - | 1445 | `		 */` |
|        - | 1446 | `		static int iForeachShortListCnt = 0;` |
|        - | 1447 | `		char zTmp[128];` |
|        - | 1448 | `		sxu32 nLen;` |
|        - | 1449 | `		char *zDup;` |
|       29 | 1450 | `		nLen = (sxu32)SyBufferFormat(zTmp,sizeof(zTmp),"[__foreach_slist_%d__]",iForeachShortListCnt++);` |
|       29 | 1451 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zTmp,nLen);` |
|       29 | 1452 | `		if( zDup == 0 ){` |
|      ! 0 | 1453 | `			PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 1454 | `			return SXERR_ABORT;` |
|        - | 1455 | `		}` |
|       29 | 1456 | `		SyStringInitFromBuf(&pInfo->sValue,zDup,nLen);` |
|        - | 1457 | `		/* Save [...] token boundaries */` |
|       29 | 1458 | `		pListStart = pGen->pIn;` |
|        - | 1459 | `		/* Advance past [...] */` |
|       29 | 1460 | `		pGen->pIn++; /* Jump '[' */` |
|       29 | 1461 | `		PH7_DelimitNestedTokens(pGen->pIn,pEnd,PH7_TK_OSB,PH7_TK_CSB,&pListEnd);` |
|       29 | 1462 | `		if( pListEnd >= pEnd ){` |
|      ! 0 | 1463 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 1464 | `				"foreach: Missing closing ']' after short list");` |
|      ! 0 | 1465 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1466 | `				return SXERR_ABORT;` |
|        - | 1467 | `			}` |
|      ! 0 | 1468 | `			goto Synchronize;` |
|        - | 1469 | `		}` |
|       29 | 1470 | `		pGen->pIn = &pListEnd[1]; /* Past ']' */` |
|       29 | 1471 | `		pListEnd = pGen->pIn;` |
|       29 | 1472 | `		pInfo->iFlags \|= PH7_4EACH_STEP_LIST;` |
|       16 | 1473 | `	}else{` |
|        - | 1474 | `		/* Compile the expression holding the value name */` |
|   376959 | 1475 | `		rc = PH7_CompileExpr(&(*pGen),0,GenStateForEachNodeValidator);` |
|   376959 | 1476 | `		if( rc == SXERR_ABORT ){` |
|        - | 1477 | `			/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1478 | `			return SXERR_ABORT;` |
|        - | 1479 | `		}` |
|   376959 | 1480 | `		pInstr = PH7_VmPopInstr(pGen->pVm);` |
|   376959 | 1481 | `		if( pInstr->p3 ){` |
|        - | 1482 | `			/* Record value name */` |
|   376959 | 1483 | `			SyStringInitFromBuf(&pInfo->sValue,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|   188477 | 1484 | `		}` |
|        - | 1485 | `	}` |
|        - | 1486 | `	/* Emit the 'FOREACH_INIT' instruction */` |
|   376991 | 1487 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_INIT,0,0,pInfo,&nFalseJump);` |
|        - | 1488 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   376991 | 1489 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_INIT,nFalseJump);` |
|        - | 1490 | `	/* Record the first instruction to execute */` |
|   376991 | 1491 | `	pForeachBlock->nFirstInstr = PH7_VmInstrLength(pGen->pVm);` |
|        - | 1492 | `	/* Emit the FOREACH_STEP instruction */` |
|   376991 | 1493 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_FOREACH_STEP,0,0,pInfo,&nFalseJump);` |
|        - | 1494 | `	/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   376991 | 1495 | `	GenStateNewJumpFixup(pForeachBlock,PH7_OP_FOREACH_STEP,nFalseJump);` |
|        - | 1496 | `	/* If list() unpacking, emit bytecode to destructure the temp variable */` |
|   376991 | 1497 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_LIST) && pListStart && pListEnd ){` |
|        - | 1498 | `		SyToken *pSavedIn,*pSavedEnd;` |
|        - | 1499 | `		/* Load the temporary variable holding the current value onto the stack.` |
|        - | 1500 | `		 * The LOAD_LIST handler expects the array below the variable entries.` |
|        - | 1501 | `		 */` |
|       35 | 1502 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,(void *)SyStringData(&pInfo->sValue),0);` |
|        - | 1503 | `		/* Compile list/short-list body directly — this pushes variables and emits LOAD_LIST.` |
|        - | 1504 | `		 * We position the tokens at the construct start so the appropriate compiler` |
|        - | 1505 | `		 * picks up the delimiter and the variable names inside.` |
|        - | 1506 | `		 */` |
|       35 | 1507 | `		pSavedIn = pGen->pIn;` |
|       35 | 1508 | `		pSavedEnd = pGen->pEnd;` |
|       35 | 1509 | `		pGen->pIn = pListStart;` |
|       35 | 1510 | `		pGen->pEnd = pListEnd;` |
|       35 | 1511 | `		if( pListStart->nType & PH7_TK_OSB ){` |
|       29 | 1512 | `			rc = PH7_CompileShortList(&(*pGen),0);` |
|       16 | 1513 | `		}else{` |
|        7 | 1514 | `			rc = PH7_CompileList(&(*pGen),0);` |
|        - | 1515 | `		}` |
|       35 | 1516 | `		pGen->pIn = pSavedIn;` |
|       35 | 1517 | `		pGen->pEnd = pSavedEnd;` |
|       35 | 1518 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1519 | `			return SXERR_ABORT;` |
|        - | 1520 | `		}` |
|        - | 1521 | `		/* Pop the list result (LOAD_LIST leaves the assigned values on stack) */` |
|       35 | 1522 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       16 | 1523 | `	}` |
|        - | 1524 | `	/* Compile the loop body */` |
|   376991 | 1525 | `	pGen->pIn = &pEnd[1];` |
|   376991 | 1526 | `	pGen->pEnd = pTmp;` |
|   376991 | 1527 | `	rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_END4EACH);` |
|   376991 | 1528 | `	if( rc == SXERR_ABORT ){` |
|        - | 1529 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 | 1530 | `		return SXERR_ABORT;` |
|        - | 1531 | `	}` |
|        - | 1532 | `	/* Emit the unconditional jump to the start of the loop */` |
|   376991 | 1533 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,pForeachBlock->nFirstInstr,0,0);` |
|        - | 1534 | `	/* Fix all jumps now the destination is resolved */` |
|   376991 | 1535 | `	GenStateFixJumps(pForeachBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 1536 | `	/* Release the loop block */` |
|   376991 | 1537 | `	GenStateLeaveBlock(pGen,0);` |
|        - | 1538 | `	/* Statement successfully compiled */` |
|   376991 | 1539 | `	return SXRET_OK;` |
|        1 | 1540 | `Synchronize:` |
|        - | 1541 | `	/* Synchronize with the first semi-colon ';' so we can avoid` |
|        - | 1542 | `	 * compiling this erroneous block.` |
|        - | 1543 | `	 */` |
|        3 | 1544 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 | 1545 | `		pGen->pIn++;` |
|      ! 0 | 1546 | `	}` |
|        3 | 1547 | `	return SXRET_OK;` |
|   188499 | 1548 | `}` |
|        - | 1549 | `/*` |
|        - | 1550 | ` * Compile the infamous if/elseif/else if/else statements.` |
|        - | 1551 | ` * According to the PHP language reference` |
|        - | 1552 | ` *  The if construct is one of the most important features of many languages PHP included.` |
|        - | 1553 | ` *  It allows for conditional execution of code fragments. PHP features an if structure` |
|        - | 1554 | ` *  that is similar to that of C:` |
|        - | 1555 | ` *  if (expr)` |
|        - | 1556 | ` *   statement` |
|        - | 1557 | ` *  else construct:` |
|        - | 1558 | ` *   Often you'd want to execute a statement if a certain condition is met, and a different` |
|        - | 1559 | ` *   statement if the condition is not met. This is what else is for. else extends an if statement` |
|        - | 1560 | ` *   to execute a statement in case the expression in the if statement evaluates to FALSE.` |
|        - | 1561 | ` *   For example, the following code would display a is greater than b if $a is greater than` |
|        - | 1562 | ` *   $b, and a is NOT greater than b otherwise.` |
|        - | 1563 | ` *   The else statement is only executed if the if expression evaluated to FALSE, and if there` |
|        - | 1564 | ` *   were any elseif expressions - only if they evaluated to FALSE as well` |
|        - | 1565 | ` *  elseif` |
|        - | 1566 | ` *   elseif, as its name suggests, is a combination of if and else. Like else, it extends` |
|        - | 1567 | ` *   an if statement to execute a different statement in case the original if expression evaluates` |
|        - | 1568 | ` *   to FALSE. However, unlike else, it will execute that alternative expression only if the elseif` |
|        - | 1569 | ` *   conditional expression evaluates to TRUE. For example, the following code would display a is bigger` |
|        - | 1570 | ` *   than b, a equal to b or a is smaller than b:` |
|        - | 1571 | ` *   <?php` |
|        - | 1572 | ` *    if ($a > $b) {` |
|        - | 1573 | ` *     echo "a is bigger than b";` |
|        - | 1574 | ` *    } elseif ($a == $b) {` |
|        - | 1575 | ` *     echo "a is equal to b";` |
|        - | 1576 | ` *    } else {` |
|        - | 1577 | ` *     echo "a is smaller than b";` |
|        - | 1578 | ` *    }` |
|        - | 1579 | ` *    ?>` |
|        - | 1580 | ` */` |
|  2854296 | 1581 | `PH7_PRIVATE sxi32 PH7_CompileIf(ph7_gen_state *pGen)` |
|        5 | 1582 | `{` |
|  2854301 | 1583 | `	SyToken *pToken,*pTmp,*pEnd = 0;` |
|  2854301 | 1584 | `	GenBlock *pCondBlock = 0;` |
|        - | 1585 | `	sxu32 nJumpIdx;` |
|        - | 1586 | `	sxu32 nKeyID;` |
|        - | 1587 | `	sxi32 rc;` |
|        - | 1588 | `	/* Jump the 'if' keyword */` |
|  2854301 | 1589 | `	pGen->pIn++;` |
|  2854301 | 1590 | `	pToken = pGen->pIn;` |
|        - | 1591 | `	/* Create the conditional block */` |
|  2854301 | 1592 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_COND,PH7_VmInstrLength(pGen->pVm),0,&pCondBlock);` |
|  2854301 | 1593 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 1594 | `		return SXERR_ABORT;` |
|        - | 1595 | `	}` |
|        - | 1596 | `	/* Process as many [if/else if/elseif/else] blocks as we can */` |
|  1592496 | 1597 | `	for(;;){` |
|  3184997 | 1598 | `		if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 1599 | `			/* Syntax error */` |
|      ! 0 | 1600 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 1601 | `				pToken--;` |
|      ! 0 | 1602 | `			}` |
|      ! 0 | 1603 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing '('");` |
|      ! 0 | 1604 | `			if( rc == SXERR_ABORT ){` |
|        - | 1605 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 1606 | `				return SXERR_ABORT;` |
|        - | 1607 | `			}` |
|      ! 0 | 1608 | `			goto Synchronize;` |
|        - | 1609 | `		}` |
|        - | 1610 | `		/* Jump the left parenthesis '(' */` |
|  3184997 | 1611 | `		pToken++;` |
|        - | 1612 | `		/* Delimit the condition */` |
|  3184997 | 1613 | `		PH7_DelimitNestedTokens(pToken,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|  3184997 | 1614 | `		if( pToken >= pEnd \|\| (pEnd->nType & PH7_TK_RPAREN) == 0 ){` |
|        - | 1615 | `			/* Syntax error */` |
|      ! 0 | 1616 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 1617 | `				pToken--;` |
|      ! 0 | 1618 | `			}` |
|      ! 0 | 1619 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pToken->nLine,"if/else/elseif: Missing ')'");` |
|      ! 0 | 1620 | `			if( rc == SXERR_ABORT ){` |
|        - | 1621 | `				/* Error count limit reached,abort immediately */` |
|      ! 0 | 1622 | `				return SXERR_ABORT;` |
|        - | 1623 | `			}` |
|      ! 0 | 1624 | `			goto Synchronize;` |
|        - | 1625 | `		}` |
|        - | 1626 | `		/* Swap token streams */` |
|  3184997 | 1627 | `		SWAP_TOKEN_STREAM(pGen,pToken,pEnd);` |
|        - | 1628 | `		/* Compile the condition */` |
|  3184997 | 1629 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 1630 | `		/* Update token stream */` |
|  3184997 | 1631 | `		while(pGen->pIn < pEnd ){` |
|      ! 0 | 1632 | `			PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 | 1633 | `			pGen->pIn++;` |
|      ! 0 | 1634 | `		}` |
|  3184997 | 1635 | `		pGen->pIn  = &pEnd[1];` |
|  3184997 | 1636 | `		pGen->pEnd = pTmp;` |
|  3184997 | 1637 | `		if( rc == SXERR_ABORT ){` |
|        - | 1638 | `			/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|        3 | 1639 | `			return SXERR_ABORT;` |
|        - | 1640 | `		}` |
|        - | 1641 | `		/* Emit the false jump */` |
|  3184995 | 1642 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJumpIdx);` |
|        - | 1643 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|  3184995 | 1644 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JZ,nJumpIdx);` |
|        - | 1645 | `		/* Compile the body */` |
|  3184995 | 1646 | `		rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|  3184995 | 1647 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1648 | `			return SXERR_ABORT;` |
|        - | 1649 | `		}` |
|  3184995 | 1650 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|   611729 | 1651 | `			break;` |
|        - | 1652 | `		}` |
|        - | 1653 | `		/* Ensure that the keyword ID is 'else if' or 'else' */` |
|  1961547 | 1654 | `		nKeyID = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  1961547 | 1655 | `		if( (nKeyID & (PH7_TKWRD_ELSE\|PH7_TKWRD_ELIF)) == 0 ){` |
|  1363275 | 1656 | `			break;` |
|        - | 1657 | `		}` |
|        - | 1658 | `		/* Emit the unconditional jump */` |
|   598277 | 1659 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJumpIdx);` |
|        - | 1660 | `		/* Save the instruction index so we can fix it later when the jump destination is resolved */` |
|   598277 | 1661 | `		GenStateNewJumpFixup(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|   598277 | 1662 | `		if( nKeyID & PH7_TKWRD_ELSE ){` |
|   362669 | 1663 | `			pToken = &pGen->pIn[1];` |
|   362669 | 1664 | `			if( pToken >= pGen->pEnd \|\| (pToken->nType & PH7_TK_KEYWORD) == 0 \|\|` |
|    95126 | 1665 | `				SX_PTR_TO_INT(pToken->pUserData) != PH7_TKWRD_IF ){` |
|   133793 | 1666 | `					break;` |
|        - | 1667 | `			}` |
|    95093 | 1668 | `			pGen->pIn++; /* Jump the 'else' keyword */` |
|    47544 | 1669 | `		}` |
|   330701 | 1670 | `		pGen->pIn++; /* Jump the 'elseif/if' keyword */` |
|        - | 1671 | `		/* Synchronize cursors */` |
|   330701 | 1672 | `		pToken = pGen->pIn;` |
|        - | 1673 | `		/* Fix the false jump */` |
|   330701 | 1674 | `		GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|        5 | 1675 | `	} /* For(;;) */` |
|        - | 1676 | `	/* Fix the false jump */` |
|  2854299 | 1677 | `	GenStateFixJumps(pCondBlock,PH7_OP_JZ,PH7_VmInstrLength(pGen->pVm));` |
|  2854299 | 1678 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|  1630846 | 1679 | `		(SX_PTR_TO_INT(pGen->pIn->pUserData) & PH7_TKWRD_ELSE) ){` |
|        - | 1680 | `			/* Compile the else block */` |
|   267581 | 1681 | `			pGen->pIn++;` |
|   267581 | 1682 | `			rc = PH7_CompileBlock(&(*pGen),PH7_TKWRD_ENDIF);` |
|   267581 | 1683 | `			if( rc == SXERR_ABORT ){` |
|        - | 1684 |  |
|      ! 0 | 1685 | `				return SXERR_ABORT;` |
|        - | 1686 | `			}` |
|   133788 | 1687 | `	}` |
|  2854299 | 1688 | `	nJumpIdx = PH7_VmInstrLength(pGen->pVm);` |
|        - | 1689 | `	/* Fix all unconditional jumps now the destination is resolved */` |
|  2854299 | 1690 | `	GenStateFixJumps(pCondBlock,PH7_OP_JMP,nJumpIdx);` |
|        - | 1691 | `	/* Release the conditional block */` |
|  2854299 | 1692 | `	GenStateLeaveBlock(pGen,0);` |
|        - | 1693 | `	/* Statement successfully compiled */` |
|  2854299 | 1694 | `	return SXRET_OK;` |
|      ! 0 | 1695 | `Synchronize:` |
|        - | 1696 | `	/* Synchronize with the first semi-colon ';' so we can avoid compiling this erroneous block.` |
|        - | 1697 | `	 */` |
|      ! 0 | 1698 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|      ! 0 | 1699 | `		pGen->pIn++;` |
|      ! 0 | 1700 | `	}` |
|      ! 0 | 1701 | `	return SXRET_OK;` |
|  1427153 | 1702 | `}` |
|        - | 1703 | `/*` |
|        - | 1704 | ` * Compile the global construct.` |
|        - | 1705 | ` * According to the PHP language reference` |
|        - | 1706 | ` *  In PHP global variables must be declared global inside a function if they are going` |
|        - | 1707 | ` *  to be used in that function.` |
|        - | 1708 | ` *  Example #1 Using global` |
|        - | 1709 | ` *  <?php` |
|        - | 1710 | ` *   $a = 1;` |
|        - | 1711 | ` *   $b = 2;` |
|        - | 1712 | ` *   function Sum()` |
|        - | 1713 | ` *   {` |
|        - | 1714 | ` *    global $a, $b;` |
|        - | 1715 | ` *    $b = $a + $b;` |
|        - | 1716 | ` *   }` |
|        - | 1717 | ` *   Sum();` |
|        - | 1718 | ` *   echo $b;` |
|        - | 1719 | ` *  ?>` |
|        - | 1720 | ` *  The above script will output 3. By declaring $a and $b global within the function` |
|        - | 1721 | ` *  all references to either variable will refer to the global version. There is no limit` |
|        - | 1722 | ` *  to the number of global variables that can be manipulated by a function.` |
|        - | 1723 | ` */` |
|       46 | 1724 | `PH7_PRIVATE sxi32 PH7_CompileGlobal(ph7_gen_state *pGen)` |
|        5 | 1725 | `{` |
|       51 | 1726 | `	SyToken *pTmp,*pNext = 0;` |
|        - | 1727 | `	sxi32 nExpr;` |
|        - | 1728 | `	sxi32 rc;` |
|        - | 1729 | `	/* Jump the 'global' keyword */` |
|       51 | 1730 | `	pGen->pIn++;` |
|       51 | 1731 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        - | 1732 | `		/* Nothing to process */` |
|      ! 0 | 1733 | `		return SXRET_OK;` |
|        - | 1734 | `	}` |
|       51 | 1735 | `	pTmp = pGen->pEnd;` |
|       51 | 1736 | `	nExpr = 0;` |
|      111 | 1737 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|       65 | 1738 | `		if( pGen->pIn < pNext ){` |
|       65 | 1739 | `			pGen->pEnd = pNext;` |
|       65 | 1740 | `			if( (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|      ! 0 | 1741 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"global: Expected variable name");` |
|      ! 0 | 1742 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 1743 | `					return SXERR_ABORT;` |
|        - | 1744 | `				}` |
|      ! 0 | 1745 | `			}else{` |
|       65 | 1746 | `				pGen->pIn++;` |
|       65 | 1747 | `				if( pGen->pIn >= pGen->pEnd ){` |
|        - | 1748 | `					/* Emit a warning */` |
|      ! 0 | 1749 | `					PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn[-1].nLine,"global: Empty variable name");` |
|      ! 0 | 1750 | `				}else{` |
|       65 | 1751 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       65 | 1752 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 1753 | `						return SXERR_ABORT;` |
|       65 | 1754 | `					}else if(rc != SXERR_EMPTY ){` |
|       65 | 1755 | `						VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|       65 | 1756 | `						if( pLast && pLast->iOp == PH7_OP_LOADC ){` |
|        - | 1757 | `							/* Variable name, not a constant */` |
|       55 | 1758 | `							pLast->iP1 = 0;` |
|       25 | 1759 | `						}` |
|       65 | 1760 | `						nExpr++;` |
|       30 | 1761 | `					}` |
|        - | 1762 | `				}` |
|        - | 1763 | `			}` |
|       30 | 1764 | `		}` |
|        - | 1765 | `		/* Next expression in the stream */` |
|       65 | 1766 | `		pGen->pIn = pNext;` |
|        - | 1767 | `		/* Jump trailing commas */` |
|       79 | 1768 | `		while( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       19 | 1769 | `			pGen->pIn++;` |
|        5 | 1770 | `		}` |
|        5 | 1771 | `	}` |
|        - | 1772 | `	/* Restore token stream */` |
|       51 | 1773 | `	pGen->pEnd = pTmp;` |
|       51 | 1774 | `	if( nExpr > 0 ){` |
|        - | 1775 | `		/* Emit the uplink instruction */` |
|       51 | 1776 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_UPLINK,nExpr,0,0,0);` |
|       23 | 1777 | `	}` |
|       51 | 1778 | `	return SXRET_OK;` |
|       28 | 1779 | `}` |
|        - | 1780 | `/*` |
|        - | 1781 | ` * php's NOUN for a compile-time return diagnostic is the LEXICAL scope, not the` |
|        - | 1782 | `` * kind of function the `return` sits in: zend reads CG(active_class_entry), so a`` |
|        - | 1783 | ` * closure written inside a class body reports "method" and the very same closure` |
|        - | 1784 | ` * written at file scope reports "function". pCurClass is the compiler's exact` |
|        - | 1785 | ` * counterpart (the class/interface/trait/enum whose BODY is being compiled).` |
|        - | 1786 | ` */` |
|       26 | 1787 | `static const char * GenStateReturnNoun(ph7_gen_state *pGen)` |
|        4 | 1788 | `{` |
|       30 | 1789 | `	return pGen->pCurClass ? "method" : "function";` |
|        4 | 1790 | `}` |
|        - | 1791 | `/*` |
|        - | 1792 | ` * TRUE when a declared return type ACCEPTS null — the condition under which php` |
|        - | 1793 | `` * appends its `did you mean "return null;"` hint to the missing-value error.`` |
|        - | 1794 | ``  * That is every nullable declaration (`?T`, `T\|null`, and the standalone `null` `` |
|        - | 1795 | `` * type, all of which set VM_FUNC_RETURN_NULLABLE) plus `mixed`, which includes`` |
|        - | 1796 | ` * null but is stored as a pseudo-CLASS atom rather than through the flag.` |
|        - | 1797 | ` */` |
|       10 | 1798 | `static int GenStateReturnTypeAllowsNull(ph7_vm_func *pFunc)` |
|        4 | 1799 | `{` |
|        - | 1800 | `	SyString *pCls;` |
|       14 | 1801 | `	if( pFunc->iFlags & VM_FUNC_RETURN_NULLABLE ){` |
|        3 | 1802 | `		return 1;` |
|        - | 1803 | `	}` |
|       11 | 1804 | `	pCls = &pFunc->sReturnClass;` |
|        8 | 1805 | `	if( pFunc->nReturnType == SXU32_HIGH && pCls->nByte == sizeof("mixed")-1` |
|        3 | 1806 | `	 && SyStrnicmp(pCls->zString,"mixed",sizeof("mixed")-1) == 0 ){` |
|      ! 0 | 1807 | `		return 1;` |
|        - | 1808 | `	}` |
|       11 | 1809 | `	return 0;` |
|        9 | 1810 | `}` |
|        - | 1811 | `/*` |
|        - | 1812 | ` * Compile the return statement.` |
|        - | 1813 | ` * According to the PHP language reference` |
|        - | 1814 | ` *  If called from within a function, the return() statement immediately ends execution` |
|        - | 1815 | ` *  of the current function, and returns its argument as the value of the function call.` |
|        - | 1816 | ` *  return() will also end the execution of an eval() statement or script file.` |
|        - | 1817 | ` *  If called from the global scope, then execution of the current script file is ended.` |
|        - | 1818 | ` *  If the current script file was include()ed or require()ed, then control is passed back` |
|        - | 1819 | ` *  to the calling file. Furthermore, if the current script file was include()ed, then the value` |
|        - | 1820 | ` *  given to return() will be returned as the value of the include() call. If return() is called` |
|        - | 1821 | ` *  from within the main script file, then script execution end.` |
|        - | 1822 | ` *  Note that since return() is a language construct and not a function, the parentheses` |
|        - | 1823 | ` *  surrounding its arguments are not required. It is common to leave them out, and you actually` |
|        - | 1824 | ` *  should do so as PHP has less work to do in this case.` |
|        - | 1825 | ` *  Note: If no parameter is supplied, then the parentheses must be omitted and NULL will be returned.` |
|        - | 1826 | ` */` |
|  4129148 | 1827 | `PH7_PRIVATE sxi32 PH7_CompileReturn(ph7_gen_state *pGen)` |
|        5 | 1828 | `{` |
|  4129153 | 1829 | `	sxi32 nRet = 0; /* TRUE if there is a return value */` |
|        - | 1830 | `	sxi32 rc;` |
|  4129153 | 1831 | `	sxu32 nLine = pGen->pIn->nLine;` |
|  4129153 | 1832 | `	GenBlock *pFuncBlock = pGen->pCurrent;` |
|        - | 1833 | `	ph7_vm_func *pFunc;` |
|        - | 1834 | `	sxu32 nInstrBefore;` |
|        - | 1835 | ``	/* A `never`-returning function must not contain a `return` statement at all`` |
|        - | 1836 | `	 * (PHP compile error), with or without a value. Find the enclosing function` |
|        - | 1837 | `	 * (nearest GEN_BLOCK_FUNC) and check its declared return type. The error is` |
|        - | 1838 | `	 * recorded (nErr>0 fails the whole compile); the statement is still consumed` |
|        - | 1839 | `	 * normally below so token processing stays consistent. */` |
| 10948891 | 1840 | `	while( pFuncBlock && (pFuncBlock->iFlags & GEN_BLOCK_FUNC) == 0 ){` |
|  6819743 | 1841 | `		pFuncBlock = pFuncBlock->pParent;` |
|        5 | 1842 | `	}` |
|  4129153 | 1843 | `	pFunc = pFuncBlock ? (ph7_vm_func *)pFuncBlock->pUserData : 0;` |
|  4129153 | 1844 | `	if( pFunc && pFunc->nReturnType == MEMOBJ_NEVER ){` |
|        8 | 1845 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        2 | 1846 | `			"A never-returning %s must not return", GenStateReturnNoun(pGen));` |
|        6 | 1847 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1848 | `			return SXERR_ABORT;` |
|        - | 1849 | `		}` |
|        2 | 1850 | `	}` |
|        - | 1851 | `	/* Jump the 'return' keyword */` |
|  4129153 | 1852 | `	pGen->pIn++;` |
|  4129153 | 1853 | `	nInstrBefore = PH7_VmInstrLength(pGen->pVm);` |
|  4129153 | 1854 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - | 1855 | ``		/* php: a stray token after `return EXPR` is `... expecting ";"`. */`` |
|  4011379 | 1856 | `		const char *zSave = pGen->zClauseCloser;` |
|  4011379 | 1857 | `		pGen->zClauseCloser = "\";\"";` |
|        - | 1858 | ``		/* A `return` READS its operand (the value is consumed), so compile it`` |
|        - | 1859 | ``		 * read-only: a lone undefined variable `return $z` must warn at the read`` |
|        - | 1860 | `		 * exactly like echo/interpolation, not be loaded quietly (the same quiet` |
|        - | 1861 | ``		 * load that correctly keeps a bare `$z;` statement silent). Matches the`` |
|        - | 1862 | `		 * arrow-fn implicit-return body fix. */` |
|  4011379 | 1863 | `		rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD,0);` |
|  4011379 | 1864 | `		pGen->zClauseCloser = zSave;` |
|  4011379 | 1865 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1866 | `			return SXERR_ABORT;` |
|  4011379 | 1867 | `		}else if(rc != SXERR_EMPTY ){` |
|  4011379 | 1868 | `			nRet = 1;` |
|  2005687 | 1869 | `		}` |
|  2005687 | 1870 | `	}` |
|        - | 1871 | ``	/* A bare `return;` inside a function that DECLARES a return type is a php`` |
|        - | 1872 | `	 * COMPILE error, not the runtime TypeError PHL used to raise on the way out:` |
|        - | 1873 | `` 	 * php rejects the program before it runs. `void` (which is what `return;` `` |
|        - | 1874 | ``	 * means) and `never` (handled above) are the two declarations exempt from it,`` |
|        - | 1875 | ``	 * and a GENERATOR is exempt whatever it declares — there `return;` ends the`` |
|        - | 1876 | `	 * generator, and the declared type describes the Generator object the call` |
|        - | 1877 | `	 * produced, never the returned value. */` |
|  4129148 | 1878 | `	if( nRet == 0 && pFunc && !pGen->bInGenerator && VmFuncHasReturnType(pFunc)` |
|    56632 | 1879 | `	 && pFunc->nReturnType != MEMOBJ_VOID && pFunc->nReturnType != MEMOBJ_NEVER ){` |
|       19 | 1880 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1881 | `			"A %s with return type must return a value%s",` |
|        5 | 1882 | `			GenStateReturnNoun(pGen),` |
|       10 | 1883 | `			GenStateReturnTypeAllowsNull(pFunc)` |
|        - | 1884 | `				? " (did you mean \"return null;\" instead of \"return;\"?)" : "");` |
|       14 | 1885 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1886 | `			return SXERR_ABORT;` |
|        - | 1887 | `		}` |
|        5 | 1888 | `	}` |
|        - | 1889 | ``	/* The mirror rule: a `void` function must not return a VALUE, and php stops`` |
|        - | 1890 | `	 * the program at the return statement rather than throwing on the way out.` |
|        - | 1891 | `	 * Generators keep their own diagnostic (a void generator is rejected as` |
|        - | 1892 | ``	 * `Generator return type must be a supertype of Generator`), so they are`` |
|        - | 1893 | `	 * skipped here exactly as above. php's hint fires when the operand is a` |
|        - | 1894 | ``	 * compile-time constant null; PHL folds the `null` KEYWORD (constant index 0,`` |
|        - | 1895 | `	 * emitted as a lone OP_LOADC and unchanged by any wrapping parens), which is` |
|        - | 1896 | `	 * every shape real code writes. */` |
|  4129148 | 1897 | `	if( nRet != 0 && pFunc && !pGen->bInGenerator` |
|  4011348 | 1898 | `	 && pFunc->nReturnType == MEMOBJ_VOID ){` |
|       16 | 1899 | `		VmInstr *pLast = PH7_VmPeekInstr(pGen->pVm);` |
|       22 | 1900 | `		int bNullLiteral = (PH7_VmInstrLength(pGen->pVm) == nInstrBefore + 1)` |
|       12 | 1901 | `			&& pLast && pLast->iOp == PH7_OP_LOADC` |
|       18 | 1902 | `			&& pLast->iP1 == 0 && pLast->iP2 == 0;` |
|       22 | 1903 | `		rc = PH7_GenCompileError(pGen, E_ERROR, nLine,` |
|        - | 1904 | `			"A void %s must not return a value%s",` |
|        6 | 1905 | `			GenStateReturnNoun(pGen),` |
|        6 | 1906 | `			bNullLiteral` |
|        - | 1907 | `				? " (did you mean \"return;\" instead of \"return null;\"?)" : "");` |
|       16 | 1908 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1909 | `			return SXERR_ABORT;` |
|        - | 1910 | `		}` |
|        6 | 1911 | `	}` |
|        - | 1912 | ``	/* ROOT C: inside a generator body, route `return` through OP_SET_FINALLY_RET so every`` |
|        - | 1913 | `	 * enclosing inline finally runs first (threaded at runtime via VmFinallyAdvance over the` |
|        - | 1914 | `	 * live aException stack). With no enclosing try the action materializes immediately, so` |
|        - | 1915 | `	 * this is safe for a plain top-level generator return too. Non-generators: legacy OP_DONE. */` |
|  4129153 | 1916 | `	if( pGen->bInGenerator ){` |
|     4575 | 1917 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SET_FINALLY_RET,nRet,0,0,0);` |
|     4575 | 1918 | `		return SXRET_OK;` |
|        - | 1919 | `	}` |
|        - | 1920 | ``	/* Emit the done instruction. iP2=1 marks an explicit `return`: when this`` |
|        - | 1921 | `	 * OP_DONE terminates a catch/finally mini-program (run via VmLocalExec with` |
|        - | 1922 | `	 * bReturnPropagates), the VM must return from the enclosing function rather` |
|        - | 1923 | `	 * than fall through. Terminal catch/finally DONEs keep iP2=0 (fall-through),` |
|        - | 1924 | ``	 * so the VM can tell a real `return` from the body simply ending. */`` |
|  4124583 | 1925 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,nRet,1,0,0);` |
|  4124583 | 1926 | `	return SXRET_OK;` |
|  2064579 | 1927 | `}` |
|        - | 1928 | `/*` |
|        - | 1929 | ` * Compile a yield expression.` |
|        - | 1930 | ` * Called from the expression code generator when a yield node is encountered.` |
|        - | 1931 | ` * Handles: yield, yield $value, yield $key => $value` |
|        - | 1932 | ` * The yield expression evaluates to the value passed via Generator::send().` |
|        - | 1933 | ` */` |
|    18564 | 1934 | `PH7_PRIVATE sxi32 PH7_CompileYield(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|        5 | 1935 | `{` |
|        - | 1936 | `	SyToken *pTmp, *pSplit;` |
|    18569 | 1937 | `	sxi32 iP1 = 0; /* 1 if value present */` |
|    18569 | 1938 | `	sxi32 iP2 = 0; /* 1 if key => value */` |
|        - | 1939 | `	sxi32 rc;` |
|     9282 | 1940 | `	(void)iCompileFlag;` |
|        - | 1941 | `	/* pGen->pIn points to 'yield' keyword, skip it */` |
|    18569 | 1942 | `	pGen->pIn++;` |
|        - | 1943 | `	/* Now pGen->pIn points to the first token after 'yield'` |
|        - | 1944 | `	 * pGen->pEnd points to the delimiter (;, ), ], etc.) */` |
|        - | 1945 | ``	/* `yield from <iterable>` — generator delegation (PHP 7.0). 'from' is a`` |
|        - | 1946 | `	 * contextual identifier, not a keyword; a variable named $from lexes as` |
|        - | 1947 | ``	 * PH7_TK_DOLLAR, never PH7_TK_ID, so `yield $from` cannot match here. */`` |
|    18564 | 1948 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID)` |
|     9319 | 1949 | `		&& pGen->pIn->sData.nByte == 4` |
|       75 | 1950 | `		&& SyStrnicmp(pGen->pIn->sData.zString, "from", 4) == 0 ){` |
|       69 | 1951 | `		pGen->pIn++; /* Skip 'from' */` |
|       69 | 1952 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|       69 | 1953 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1954 | `			return SXERR_ABORT;` |
|        - | 1955 | `		}` |
|       69 | 1956 | `		if( rc == SXERR_EMPTY ){` |
|      ! 0 | 1957 | `			rc = PH7_GenCompileError(pGen, E_ERROR,` |
|      ! 0 | 1958 | `				(pGen->pIn < pGen->pEnd) ? pGen->pIn->nLine : 0,` |
|        - | 1959 | `				"Missing expression after 'yield from'");` |
|      ! 0 | 1960 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 1961 | `				return SXERR_ABORT;` |
|        - | 1962 | `			}` |
|      ! 0 | 1963 | `		}` |
|       69 | 1964 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD_FROM, 0, 0, 0, 0);` |
|       69 | 1965 | `		return SXRET_OK;` |
|        - | 1966 | `	}` |
|    18505 | 1967 | `	if( pGen->pIn >= pGen->pEnd ){` |
|        - | 1968 | `		/* Bare yield — no value */` |
|        3 | 1969 | `		PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, 0, 0, 0, 0);` |
|        3 | 1970 | `		return SXRET_OK;` |
|        - | 1971 | `	}` |
|        - | 1972 | `	/* Scan for '=>' at nesting level 0 to detect key => value syntax */` |
|    18503 | 1973 | `	pSplit = 0;` |
|        - | 1974 | `	{` |
|    18503 | 1975 | `		SyToken *pCur = pGen->pIn;` |
|    18503 | 1976 | `		sxi32 nNest = 0;` |
|    55309 | 1977 | `		while( pCur < pGen->pEnd ){` |
|    54937 | 1978 | `			if( pCur->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       29 | 1979 | `				nNest++;` |
|    54924 | 1980 | `			}else if( pCur->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       29 | 1981 | `				nNest--;` |
|    54898 | 1982 | `			}else if( nNest == 0 && (pCur->nType & PH7_TK_ARRAY_OP) ){` |
|    18131 | 1983 | `				pSplit = pCur;` |
|    18131 | 1984 | `				break;` |
|        - | 1985 | `			}` |
|    36811 | 1986 | `			pCur++;` |
|        5 | 1987 | `		}` |
|        - | 1988 | `	}` |
|    18503 | 1989 | `	pTmp = pGen->pEnd;` |
|    18503 | 1990 | `	if( pSplit ){` |
|        - | 1991 | `		/* yield $key => $value */` |
|    18131 | 1992 | `		pGen->pEnd = pSplit;` |
|    18131 | 1993 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|    18131 | 1994 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|    18131 | 1995 | `		pGen->pIn = pSplit + 1; /* Skip '=>' */` |
|    18131 | 1996 | `		pGen->pEnd = pTmp;` |
|    18131 | 1997 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|    18131 | 1998 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|    18131 | 1999 | `		iP1 = 1;` |
|    18131 | 2000 | `		iP2 = 1;` |
|     9068 | 2001 | `	}else{` |
|        - | 2002 | `		/* yield $value */` |
|      377 | 2003 | `		rc = PH7_CompileExpr(pGen, 0, 0);` |
|      377 | 2004 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      377 | 2005 | `		if( rc != SXERR_EMPTY ){` |
|      377 | 2006 | `			iP1 = 1;` |
|      186 | 2007 | `		}` |
|        - | 2008 | `	}` |
|    18503 | 2009 | `	pGen->pEnd = pTmp;` |
|    18503 | 2010 | `	PH7_VmEmitInstr(pGen->pVm, PH7_OP_YIELD, iP1, iP2, 0, 0);` |
|    18503 | 2011 | `	return SXRET_OK;` |
|     9287 | 2012 | `}` |
|        - | 2013 | `/*` |
|        - | 2014 | ` * Compile the die/exit language construct.` |
|        - | 2015 | ` * The role of these constructs is to terminate execution of the script.` |
|        - | 2016 | ` * Shutdown functions will always be executed even if exit() is called.` |
|        - | 2017 | ` */` |
|       94 | 2018 | `PH7_PRIVATE sxi32 PH7_CompileHalt(ph7_gen_state *pGen)` |
|        5 | 2019 | `{` |
|       99 | 2020 | `	sxi32 nExpr = 0;` |
|        - | 2021 | `	sxi32 rc;` |
|        - | 2022 | `	/* Jump the die/exit keyword */` |
|       99 | 2023 | `	pGen->pIn++;` |
|       99 | 2024 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        - | 2025 | `		/* Compile the expression */` |
|       99 | 2026 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|       99 | 2027 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2028 | `			return SXERR_ABORT;` |
|       99 | 2029 | `		}else if(rc != SXERR_EMPTY ){` |
|       99 | 2030 | `			nExpr = 1;` |
|       47 | 2031 | `		}` |
|       47 | 2032 | `	}` |
|        - | 2033 | `	/* Emit the HALT instruction */` |
|       99 | 2034 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_HALT,nExpr,0,0,0);` |
|       99 | 2035 | `	return SXRET_OK;` |
|       52 | 2036 | `}` |
|        - | 2037 | `/*` |
|        - | 2038 | ` * Compile the 'echo' language construct.` |
|        - | 2039 | ` */` |
|    20602 | 2040 | `PH7_PRIVATE sxi32 PH7_CompileEcho(ph7_gen_state *pGen)` |
|        5 | 2041 | `{` |
|    20607 | 2042 | `	SyToken *pTmp,*pNext = 0;` |
|    20607 | 2043 | `	sxu32 nLine = pGen->pIn->nLine;` |
|    20607 | 2044 | `	int nExpr = 0;      /* expressions actually compiled */` |
|    20607 | 2045 | `	int bExpectMore = 1;/* after 'echo' or a comma an expression is REQUIRED */` |
|        - | 2046 | `	sxi32 rc;` |
|        - | 2047 | `	/* Jump the 'echo' keyword */` |
|    20607 | 2048 | `	pGen->pIn++;` |
|        - | 2049 | `	/* Compile arguments one after one. php: a stray token in an echo list is` |
|        - | 2050 | ``	 * `... expecting "," or ";"` — set the closer for each argument's compile. */`` |
|    20607 | 2051 | `	pTmp = pGen->pEnd;` |
|        - | 2052 | `	{` |
|    20607 | 2053 | `	const char *zSaveEcho = pGen->zClauseCloser;` |
|    54903 | 2054 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pTmp,&pNext) ){` |
|    34307 | 2055 | `		if( pGen->pIn < pNext ){` |
|    34307 | 2056 | `			pGen->pEnd = pNext;` |
|    34307 | 2057 | `			pGen->zClauseCloser = "\",\" or \";\"";` |
|    34307 | 2058 | `			rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_RDONLY_LOAD/* Do not create variable if inexistant */,0);` |
|    34307 | 2059 | `			pGen->zClauseCloser = zSaveEcho;` |
|    34307 | 2060 | `			if( rc == SXERR_ABORT ){` |
|        6 | 2061 | `				return SXERR_ABORT;` |
|    34303 | 2062 | `			}else if( rc != SXERR_EMPTY ){` |
|        - | 2063 | `				/* Emit the consume instruction */` |
|    34279 | 2064 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CONSUME,1,0,0,0);` |
|    34279 | 2065 | `				nExpr++;` |
|    34279 | 2066 | `				bExpectMore = 0;` |
|    17137 | 2067 | `			}` |
|    17149 | 2068 | `		}` |
|        - | 2069 | `		/* Jump trailing commas (php: exactly one between expressions; a` |
|        - | 2070 | `		 * dangling or doubled comma is a parse error, enforced below) */` |
|    48009 | 2071 | `		while( pNext < pTmp && (pNext->nType & PH7_TK_COMMA) ){` |
|    13713 | 2072 | `			if( bExpectMore ){` |
|        - | 2073 | `				/* two commas in a row */` |
|        3 | 2074 | `				rc = PH7_GenCompileError(&(*pGen),E_PARSE,pNext->nLine,` |
|        - | 2075 | `					"syntax error, unexpected token \",\"");` |
|        3 | 2076 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 2077 | `			}` |
|    13711 | 2078 | `			bExpectMore = 1;` |
|    13711 | 2079 | `			pNext++;` |
|        5 | 2080 | `		}` |
|    34301 | 2081 | `		pGen->pIn = pNext;` |
|        5 | 2082 | `	}` |
|        - | 2083 | `	}` |
|        - | 2084 | `	/* Restore token stream */` |
|    20601 | 2085 | `	pGen->pEnd = pTmp;` |
|    20601 | 2086 | `	if( nExpr == 0 \|\| bExpectMore ){` |
|        - | 2087 | ``		/* `echo ;` or `echo expr, ;` — php rejects both */`` |
|       32 | 2088 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - | 2089 | `			"syntax error, unexpected token \";\"");` |
|       32 | 2090 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|        - | 2091 | `	}` |
|    20573 | 2092 | `	return SXRET_OK;` |
|    10306 | 2093 | `}` |
|        - | 2094 | `/*` |
|        - | 2095 | ` * Compile the static statement.` |
|        - | 2096 | ` * According to the PHP language reference` |
|        - | 2097 | ` *  Another important feature of variable scoping is the static variable.` |
|        - | 2098 | ` *  A static variable exists only in a local function scope, but it does not lose its value` |
|        - | 2099 | ` *  when program execution leaves this scope.` |
|        - | 2100 | ` *  Static variables also provide one way to deal with recursive functions.` |
|        - | 2101 | ` * Symisc eXtension.` |
|        - | 2102 | ` *  PH7 allow any complex expression to be associated with the static variable while` |
|        - | 2103 | ` *  the zend engine would allow only simple scalar value.` |
|        - | 2104 | ` *  Example` |
|        - | 2105 | ` *    static $myVar = "Welcome "." guest ".rand_str(3); //Valid under PH7/Generate error using the zend engine` |
|        - | 2106 | ` *    Refer to the official documentation for more information on this feature.` |
|        - | 2107 | ` */` |
|    13598 | 2108 | `PH7_PRIVATE sxi32 PH7_CompileStatic(ph7_gen_state *pGen)` |
|        5 | 2109 | `{` |
|        - | 2110 | `	ph7_vm_func_static_var sStatic; /* Structure describing the static variable */` |
|        - | 2111 | `	ph7_vm_func *pFunc;             /* Enclosing function */` |
|        - | 2112 | `	GenBlock *pBlock;` |
|        - | 2113 | `	SyString *pName;` |
|        - | 2114 | `	char *zDup;` |
|        - | 2115 | `	sxu32 nLine;` |
|        - | 2116 | `	sxi32 rc;` |
|        - | 2117 | ``	/* `static function () {}` / `static fn () =>` at statement position is an`` |
|        - | 2118 | `	 * EXPRESSION statement (a bare static closure), not a static-variable` |
|        - | 2119 | `	 * declaration — hand it to the expression compiler (php accepts it). */` |
|    13598 | 2120 | `	if( &pGen->pIn[1] < pGen->pEnd && (pGen->pIn[1].nType & PH7_TK_KEYWORD)` |
|     6805 | 2121 | `	 && (SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FUNCTION` |
|        1 | 2122 | `	  \|\| SX_PTR_TO_INT(pGen->pIn[1].pUserData) == PH7_TKWRD_FN) ){` |
|        3 | 2123 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        3 | 2124 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2125 | `			return SXERR_ABORT;` |
|        3 | 2126 | `		}else if( rc != SXERR_EMPTY ){` |
|        3 | 2127 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        1 | 2128 | `		}` |
|        3 | 2129 | `		return SXRET_OK;` |
|        - | 2130 | `	}` |
|        - | 2131 | `	/* Jump the static keyword */` |
|    13601 | 2132 | `	nLine = pGen->pIn->nLine;` |
|    13601 | 2133 | `	pGen->pIn++;` |
|        - | 2134 | `	/* Extract the enclosing function if any */` |
|    13601 | 2135 | `	pBlock = pGen->pCurrent;` |
|    27197 | 2136 | `	while( pBlock ){` |
|    27197 | 2137 | `		if( pBlock->iFlags & GEN_BLOCK_FUNC){` |
|    13601 | 2138 | `			break;` |
|        - | 2139 | `		}` |
|        - | 2140 | `		/* Point to the upper block */` |
|    13601 | 2141 | `		pBlock = pBlock->pParent;` |
|        5 | 2142 | `	}` |
|    13601 | 2143 | `	if( pBlock == 0 ){` |
|        - | 2144 | `		/* Static statement,called outside of a function body,treat it as a simple variable. */` |
|      ! 0 | 2145 | `		if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 ){` |
|        - | 2146 | ``			/* php: `static FOO;` is a syntax error naming FOO and expecting "::"`` |
|        - | 2147 | ``			 * (the parser is still open to `static::` at that point). */`` |
|      ! 0 | 2148 | `			rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"::\"");` |
|      ! 0 | 2149 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2150 | `				return SXERR_ABORT;` |
|        - | 2151 | `			}` |
|      ! 0 | 2152 | `			goto Synchronize;` |
|        - | 2153 | `		}` |
|        - | 2154 | `		/* Compile the expression holding the variable */` |
|      ! 0 | 2155 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|      ! 0 | 2156 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2157 | `			return SXERR_ABORT;` |
|      ! 0 | 2158 | `		}else if( rc != SXERR_EMPTY ){` |
|        - | 2159 | `			/* Emit the POP instruction */` |
|      ! 0 | 2160 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      ! 0 | 2161 | `		}` |
|      ! 0 | 2162 | `		return SXRET_OK;` |
|        - | 2163 | `	}` |
|    13601 | 2164 | `	pFunc = (ph7_vm_func *)pBlock->pUserData;` |
|        - | 2165 | `	/* Make sure we are dealing with a valid statement */` |
|    13601 | 2166 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|    13594 | 2167 | `		(pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - | 2168 | ``			/* php: `static FOO;` is a syntax error naming FOO and expecting "::"`` |
|        - | 2169 | ``			 * (the parser is still open to `static::` at that point). */`` |
|        3 | 2170 | `			rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"::\"");` |
|        3 | 2171 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2172 | `				return SXERR_ABORT;` |
|        - | 2173 | `			}` |
|        3 | 2174 | `			goto Synchronize;` |
|        - | 2175 | `	}` |
|    13599 | 2176 | `	pGen->pIn++;` |
|        - | 2177 | `	/* Extract variable name */` |
|    13599 | 2178 | `	pName = &pGen->pIn->sData;` |
|    13599 | 2179 | `	pGen->pIn++; /* Jump the var name */` |
|    13599 | 2180 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_EQUAL/*'='*/)) == 0 ){` |
|      ! 0 | 2181 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"static: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 2182 | `		goto Synchronize;` |
|        - | 2183 | `	}` |
|        - | 2184 | `	/* Initialize the structure describing the static variable */` |
|    13599 | 2185 | `	SySetInit(&sStatic.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|    13599 | 2186 | `	sStatic.nIdx = SXU32_HIGH; /* Not yet created */` |
|        - | 2187 | `	/* Duplicate variable name */` |
|    13599 | 2188 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    13599 | 2189 | `	if( zDup == 0 ){` |
|      ! 0 | 2190 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 2191 | `		return SXERR_ABORT;` |
|        - | 2192 | `	}` |
|    13599 | 2193 | `	SyStringInitFromBuf(&sStatic.sName,zDup,pName->nByte);` |
|        - | 2194 | `	/* Check if we have an expression to compile */` |
|    13599 | 2195 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_EQUAL) ){` |
|        - | 2196 | `		SySet *pInstrContainer;` |
|        - | 2197 | `		/* TICKET 1433-014: Symisc extension to the PHP programming language` |
|        - | 2198 | `		 * Static variable can take any complex expression including function` |
|        - | 2199 | `		 * call as their initialization value.` |
|        - | 2200 | `		 * Example:` |
|        - | 2201 | `		 *		static $var = foo(1,4+5,bar());` |
|        - | 2202 | `		 */` |
|    13599 | 2203 | `		pGen->pIn++; /* Jump the equal '=' sign */` |
|        - | 2204 | `		/* Swap bytecode container */` |
|    13599 | 2205 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    13599 | 2206 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&sStatic.aByteCode);` |
|        - | 2207 | `		/* Compile the expression */` |
|    13599 | 2208 | `		rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 2209 | `		/* Emit the done instruction */` |
|    13599 | 2210 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|        - | 2211 | `		/* Restore default bytecode container */` |
|    13599 | 2212 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|     6797 | 2213 | `	}` |
|        - | 2214 | `	/* Finally save the compiled static variable in the appropriate container */` |
|    13599 | 2215 | `	SySetPut(&pFunc->aStatic,(const void *)&sStatic);` |
|    13599 | 2216 | `	return SXRET_OK;` |
|        1 | 2217 | `Synchronize:` |
|        - | 2218 | `	/* Synchronize with the first semi-colon ';',so we can avoid compiling this erroneous` |
|        - | 2219 | `	 * statement.` |
|        - | 2220 | `	 */` |
|        5 | 2221 | `	while(pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ==  0 ){` |
|        3 | 2222 | `		pGen->pIn++;` |
|        1 | 2223 | `	}` |
|        3 | 2224 | `	return SXRET_OK;` |
|     6804 | 2225 | `}` |
|        - | 2226 | `/*` |
|        - | 2227 | ` * Compile the var statement.` |
|        - | 2228 | ` * Symisc Extension:` |
|        - | 2229 | ` *      var statement can be used outside of a class definition.` |
|        - | 2230 | ` */` |
|        2 | 2231 | `PH7_PRIVATE sxi32 PH7_CompileVar(ph7_gen_state *pGen)` |
|        1 | 2232 | `{` |
|        - | 2233 | `` 	/* `var` is a PROPERTY modifier and nothing else. A class body's `var $x;` `` |
|        - | 2234 | `	 * is compiled by compile_class.c, never here, so reaching this statement` |
|        - | 2235 | ``	 * handler means `var` appeared outside a class — which php rejects as a`` |
|        - | 2236 | `	 * parse error. PHL used to compile it as an ordinary expression statement,` |
|        - | 2237 | ``	 * so top-level `var $x = 1;` silently ran (a Symisc extension). */`` |
|        3 | 2238 | `	PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|        3 | 2239 | `	return SXERR_ABORT;` |
|        1 | 2240 | `}` |
|        - | 2241 | `/*` |
|        - | 2242 | ` * Namespace-qualify a literal in-place for CALL/NEW instructions.` |
|        - | 2243 | ` * Resolution: use imports -> current NS prefix. The VM handles global fallback.` |
|        - | 2244 | ` * Only rewrites unqualified names (no backslash) when a namespace is active.` |
|        - | 2245 | ` */` |
|        - | 2246 | `/*` |
|        - | 2247 | ` * Namespace-qualify a name for CALL/NEW/instanceof instructions.` |
|        - | 2248 | ` * Instead of mutating the interned literal (which would corrupt the literal` |
|        - | 2249 | ` * hash and any shared references), this creates a new literal entry with the` |
|        - | 2250 | ` * qualified name and updates the instruction's operand index.` |
|        - | 2251 | ` *` |
|        - | 2252 | ` * Resolution order:` |
|        - | 2253 | ` *   1. Check the given import table (pImports) — matches even outside namespaces.` |
|        - | 2254 | ` *   2. If no import matches and a namespace is active, prepend the current NS.` |
|        - | 2255 | ` *   3. Otherwise return the original literal index unchanged.` |
|        - | 2256 | ` *` |
|        - | 2257 | ` * If pFromImport is non-NULL, *pFromImport is set to 1 when the resolution` |
|        - | 2258 | ` * came from an import (step 1) and 0 otherwise.` |
|        - | 2259 | ` * Returns the (possibly new) literal index.` |
|        - | 2260 | ` */` |
|  7544092 | 2261 | `PH7_PRIVATE sxu32 GenStateNsQualifyName(ph7_gen_state *pGen,sxu32 nOrigIdx,SyHash *pImports,int *pFromImport)` |
|        5 | 2262 | `{` |
|        - | 2263 | `	ph7_value *pLit;` |
|        - | 2264 | `	const char *zLit;` |
|        - | 2265 | `	SyString sQualified;` |
|        - | 2266 | `	sxu32 nLit;` |
|        - | 2267 | `	sxu32 k;` |
|        - | 2268 | `	sxu32 nNewIdx;` |
|        - | 2269 | `	int hasNsSep;` |
|        - | 2270 | `	SyHashEntry *pImport;` |
|        - | 2271 | `	ph7_value *pNew;` |
|  7544097 | 2272 | `	if( pFromImport ){` |
|  6143501 | 2273 | `		*pFromImport = 0;` |
|  3071748 | 2274 | `	}` |
|  7544097 | 2275 | `	pLit = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nOrigIdx);` |
|  7544097 | 2276 | `	if( !pLit \|\| !(pLit->iFlags & MEMOBJ_STRING) \|\| SyBlobLength(&pLit->sBlob) == 0 ){` |
|      ! 0 | 2277 | `		return nOrigIdx;` |
|        - | 2278 | `	}` |
|  7544097 | 2279 | `	zLit = (const char *)SyBlobData(&pLit->sBlob);` |
|  7544097 | 2280 | `	nLit = (sxu32)SyBlobLength(&pLit->sBlob);` |
|        - | 2281 | `	/* Skip if already qualified (contains backslash) */` |
|  7544097 | 2282 | `	hasNsSep = 0;` |
| 90762377 | 2283 | `	for( k = 0; k < nLit; k++ ){` |
| 83218335 | 2284 | `		if( zLit[k] == '\\' ){ hasNsSep = 1; break; }` |
| 41609145 | 2285 | `	}` |
|  7544097 | 2286 | `	if( hasNsSep ){` |
|       54 | 2287 | `		return nOrigIdx;` |
|        - | 2288 | `	}` |
|        - | 2289 | `	/* Check use imports first (works even outside namespaces) */` |
|  7544047 | 2290 | `	SyBlobReset(&pGen->sWorker);` |
|  7544047 | 2291 | `	pImport = SyHashGet(pImports,(const void *)zLit,nLit);` |
|  7544047 | 2292 | `	if( pImport ){` |
|       82 | 2293 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       82 | 2294 | `		SyBlobAppend(&pGen->sWorker,zFQN,SyStrlen(zFQN));` |
|       82 | 2295 | `		if( pFromImport ){` |
|       34 | 2296 | `			*pFromImport = 1;` |
|       15 | 2297 | `		}` |
|       43 | 2298 | `	}else{` |
|  7543969 | 2299 | `		if( SyBlobLength(&pGen->sNamespace) == 0 ){` |
|  7543777 | 2300 | `			return nOrigIdx; /* Not in a namespace and no import match */` |
|        - | 2301 | `		}` |
|        - | 2302 | `		/* Prepend current namespace */` |
|      197 | 2303 | `		SyBlobAppend(&pGen->sWorker,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      197 | 2304 | `		SyBlobAppend(&pGen->sWorker,"\\",1);` |
|      197 | 2305 | `		SyBlobAppend(&pGen->sWorker,zLit,nLit);` |
|        - | 2306 | `	}` |
|        - | 2307 | `	/* Look up or create a new literal for the qualified name */` |
|      275 | 2308 | `	SyStringInitFromBuf(&sQualified,(const char *)SyBlobData(&pGen->sWorker),SyBlobLength(&pGen->sWorker));` |
|      275 | 2309 | `	if( SXRET_OK == GenStateFindLiteral(&(*pGen),&sQualified,&nNewIdx) ){` |
|      113 | 2310 | `		return nNewIdx; /* Already interned */` |
|        - | 2311 | `	}` |
|      167 | 2312 | `	pNew = PH7_ReserveConstObj(pGen->pVm,&nNewIdx);` |
|      167 | 2313 | `	if( pNew == 0 ){` |
|      ! 0 | 2314 | `		return nOrigIdx; /* OOM, fall back to original */` |
|        - | 2315 | `	}` |
|      167 | 2316 | `	PH7_MemObjInitFromString(pGen->pVm,pNew,&sQualified);` |
|      167 | 2317 | `	GenStateInstallLiteral(&(*pGen),pNew,nNewIdx);` |
|      167 | 2318 | `	return nNewIdx;` |
|  3772051 | 2319 | `}` |
|        - | 2320 | `/*` |
|        - | 2321 | ` * Resolve a class/function name at compile time through use imports and current namespace.` |
|        - | 2322 | ` * Writes the resolved FQN into pOut. Caller must release pOut.` |
|        - | 2323 | ` */` |
|  1199144 | 2324 | `PH7_PRIVATE void GenStateResolveName(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|        5 | 2325 | `{` |
|        - | 2326 | `	SyHashEntry *pImport;` |
|  1199149 | 2327 | `	const char *zName = pName->zString;` |
|  1199149 | 2328 | `	sxu32 nName = pName->nByte;` |
|  1199149 | 2329 | `	sxu32 nFirst = 0;` |
|        - | 2330 | `	/* php resolves a name through use-imports on its LEADING segment: an` |
|        - | 2331 | ``	 * unqualified `C` maps the whole name (`use X\C;` -> X\C), while a QUALIFIED`` |
|        - | 2332 | ``	 * `A\B\C` maps just `A` (`use X\A;` -> X\A\B\C) and keeps the `\B\C` tail.`` |
|        - | 2333 | `	 * The old code looked up the whole qualified string (which never matches a` |
|        - | 2334 | `	 * single-segment import alias) and then blindly prefixed the current` |
|        - | 2335 | ``	 * namespace, so `use PHPUnit\Framework; extends Framework\TestCase` resolved`` |
|        - | 2336 | `	 * to Ns\Framework\TestCase. Mirror GenStateResolveNamespaceLiteral here. */` |
| 15398115 | 2337 | `	while( nFirst < nName && zName[nFirst] != '\\' ){ nFirst++; }` |
|  1199149 | 2338 | `	pImport = SyHashGet(&pGen->hUseImports,(const void *)zName,nFirst);` |
|  1199149 | 2339 | `	if( pImport ){` |
|       72 | 2340 | `		const char *zFQN = (const char *)pImport->pUserData;` |
|       72 | 2341 | `		SyBlobAppend(pOut,zFQN,SyStrlen(zFQN));` |
|       72 | 2342 | `		SyBlobAppend(pOut,zName + nFirst,nName - nFirst); /* the \B\C tail, if any */` |
|       72 | 2343 | `		return;` |
|        - | 2344 | `	}` |
|        - | 2345 | `	/* Prepend current namespace if active */` |
|  1199081 | 2346 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       23 | 2347 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       23 | 2348 | `		SyBlobAppend(pOut,"\\",1);` |
|       10 | 2349 | `	}` |
|  1199081 | 2350 | `	SyBlobAppend(pOut,zName,nName);` |
|   599577 | 2351 | `}` |
|        - | 2352 | `/*` |
|        - | 2353 | ` * Build a fully-qualified name by prepending the current namespace to a short name.` |
|        - | 2354 | ` * If no namespace is active, pOut receives a copy of the short name.` |
|        - | 2355 | ` * The caller must release pOut when done.` |
|        - | 2356 | ` */` |
|  1219812 | 2357 | `PH7_PRIVATE void GenStateBuildFQN(ph7_gen_state *pGen,const SyString *pName,SyBlob *pOut)` |
|        5 | 2358 | `{` |
|  1219817 | 2359 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|     9517 | 2360 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|     9517 | 2361 | `		SyBlobAppend(pOut,"\\",1);` |
|     4756 | 2362 | `	}` |
|  1219817 | 2363 | `	SyBlobAppend(pOut,pName->zString,pName->nByte);` |
|  1219817 | 2364 | `}` |
|        - | 2365 | `/*` |
|        - | 2366 | `` * php's `namespace\X` NAME OPERATOR (5.3): a leading `namespace` keyword glued to`` |
|        - | 2367 | `` * a `\` names the CURRENT namespace, and the whole name is then FULLY QUALIFIED —`` |
|        - | 2368 | `` * `namespace\X` inside `namespace B;` is `\B\X`, and plain `\X` at global scope.`` |
|        - | 2369 | `` * php's lexer matches it as one token (T_NAME_RELATIVE, `"namespace"("\\"{LABEL})+`,`` |
|        - | 2370 | `` * case-insensitively), so the `\` must be GLUED to the keyword: `namespace \X` is a`` |
|        - | 2371 | ` * php parse error, and this mirrors that by comparing source offsets.` |
|        - | 2372 | ` *` |
|        - | 2373 | ` * This predicate only RECOGNIZES the operator (it consumes nothing), which is what` |
|        - | 2374 | `` * the statement dispatcher needs to tell `namespace\X::m();` from a namespace`` |
|        - | 2375 | ` * DECLARATION; GenStateNsRelPrefix below is what the name collectors call.` |
|        - | 2376 | ` */` |
| 17991018 | 2377 | `PH7_PRIVATE int GenStateIsNsRelName(SyToken *pIn,SyToken *pEnd)` |
|        5 | 2378 | `{` |
| 17991018 | 2379 | `	if( pIn >= pEnd \|\| (pIn->nType & PH7_TK_KEYWORD) == 0` |
| 13977413 | 2380 | `	 \|\| (sxu32)SX_PTR_TO_INT(pIn->pUserData) != PH7_TKWRD_NAMESPACE ){` |
| 17986201 | 2381 | `		return 0;` |
|        - | 2382 | `	}` |
|     4827 | 2383 | `	if( &pIn[1] >= pEnd \|\| (pIn[1].nType & PH7_TK_NSSEP) == 0 ){` |
|     4735 | 2384 | `		return 0;` |
|        - | 2385 | `	}` |
|       94 | 2386 | `	if( &pIn[2] >= pEnd \|\| (pIn[2].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 2387 | ``		return 0; /* php's T_NAME_RELATIVE needs at least one segment after the `\` */`` |
|        - | 2388 | `	}` |
|        - | 2389 | `	/* Glued? The tokenizer drops whitespace, so adjacency is the source offsets. */` |
|       94 | 2390 | `	return pIn->sData.zString + pIn->sData.nByte == pIn[1].sData.zString;` |
|  8995514 | 2391 | `}` |
|        - | 2392 | `/*` |
|        - | 2393 | `` * Consume a leading `namespace\` (see GenStateIsNsRelName) at *ppIn and seed pOut`` |
|        - | 2394 | ` * with the current namespace plus its separator — nothing at global scope, where` |
|        - | 2395 | ` * the bare name already IS the FQN. Returns TRUE when it fired, and the caller` |
|        - | 2396 | `` * must then treat the name it goes on to collect as ABSOLUTE: no `use` import may`` |
|        - | 2397 | ` * apply to it, and the current namespace is already in place.` |
|        - | 2398 | ` */` |
|  1378496 | 2399 | `PH7_PRIVATE int GenStateNsRelPrefix(ph7_gen_state *pGen,SyToken **ppIn,SyToken *pEnd,SyBlob *pOut)` |
|        5 | 2400 | `{` |
|  1378501 | 2401 | `	SyToken *pIn = *ppIn;` |
|  1378501 | 2402 | `	if( !GenStateIsNsRelName(pIn,pEnd) ){` |
|  1378415 | 2403 | `		return 0;` |
|        - | 2404 | `	}` |
|       88 | 2405 | `	if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       78 | 2406 | `		SyBlobAppend(pOut,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       78 | 2407 | `		SyBlobAppend(pOut,"\\",1);` |
|       38 | 2408 | `	}` |
|       88 | 2409 | `	*ppIn = &pIn[2];` |
|       88 | 2410 | `	return 1;` |
|   689253 | 2411 | `}` |
|        - | 2412 | `/*` |
|        - | 2413 | ` * Compile a namespace statement` |
|        - | 2414 | ` * According to the PHP language reference manual` |
|        - | 2415 | ` *  What are namespaces? In the broadest definition namespaces are a way of encapsulating items.` |
|        - | 2416 | ` *  This can be seen as an abstract concept in many places. For example, in any operating system` |
|        - | 2417 | ` *  directories serve to group related files, and act as a namespace for the files within them.` |
|        - | 2418 | ` *  As a concrete example, the file foo.txt can exist in both directory /home/greg and in /home/other` |
|        - | 2419 | ` *  but two copies of foo.txt cannot co-exist in the same directory. In addition, to access the foo.txt` |
|        - | 2420 | ` *  file outside of the /home/greg directory, we must prepend the directory name to the file name using` |
|        - | 2421 | ` *  the directory separator to get /home/greg/foo.txt. This same principle extends to namespaces in the` |
|        - | 2422 | ` *  programming world.` |
|        - | 2423 | ` *  In the PHP world, namespaces are designed to solve two problems that authors of libraries and applications` |
|        - | 2424 | ` *  encounter when creating re-usable code elements such as classes or functions:` |
|        - | 2425 | ` *  Name collisions between code you create, and internal PHP classes/functions/constants or third-party` |
|        - | 2426 | ` *  classes/functions/constants.` |
|        - | 2427 | ` *  Ability to alias (or shorten) Extra_Long_Names designed to alleviate the first problem, improving` |
|        - | 2428 | ` *  readability of source code.` |
|        - | 2429 | ` *  PHP Namespaces provide a way in which to group related classes, interfaces, functions and constants.` |
|        - | 2430 | ` *  Here is an example of namespace syntax in PHP:` |
|        - | 2431 | ` *       namespace my\name; // see "Defining Namespaces" section` |
|        - | 2432 | ` *       class MyClass {}` |
|        - | 2433 | ` *       function myfunction() {}` |
|        - | 2434 | ` *       const MYCONST = 1;` |
|        - | 2435 | ` *       $a = new MyClass;` |
|        - | 2436 | ` *       $c = new \my\name\MyClass;` |
|        - | 2437 | ` *       $a = strlen('hi');` |
|        - | 2438 | ` *       $d = namespace\MYCONST;` |
|        - | 2439 | ` *       $d = __NAMESPACE__ . '\MYCONST';` |
|        - | 2440 | ` *       echo constant($d);` |
|        - | 2441 | ` * NOTE` |
|        - | 2442 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|        - | 2443 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|        - | 2444 | ` */` |
|        - | 2445 | `/*` |
|        - | 2446 | ` * Return a PHP-style type name for a token, used in parse error messages.` |
|        - | 2447 | ` */` |
|       14 | 2448 | `PH7_PRIVATE const char * TokenTypeName(sxu32 nType)` |
|        4 | 2449 | `{` |
|       18 | 2450 | `	if( nType & PH7_TK_INTEGER ){ return "integer"; }` |
|       12 | 2451 | `	if( nType & PH7_TK_REAL ){ return "float"; }` |
|       12 | 2452 | `	if( nType & (PH7_TK_DSTR\|PH7_TK_SSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){ return "string"; }` |
|       12 | 2453 | `	if( nType & PH7_TK_KEYWORD ){ return "keyword"; }` |
|       12 | 2454 | `	if( nType & PH7_TK_ID ){ return "identifier"; }` |
|       12 | 2455 | `	if( nType & PH7_TK_DOLLAR ){ return "variable"; }` |
|        3 | 2456 | `	return "token";` |
|       11 | 2457 | `}` |
|     4730 | 2458 | `PH7_PRIVATE sxi32 PH7_CompileNamespace(ph7_gen_state *pGen)` |
|        5 | 2459 | `{` |
|        - | 2460 | `	sxu32 nLine;` |
|        - | 2461 | `	sxi32 rc;` |
|     4735 | 2462 | `	nLine = pGen->pIn->nLine;` |
|     4735 | 2463 | `	pGen->pIn++; /* Jump the 'namespace' keyword */` |
|        - | 2464 | `	/* Reset namespace and clear previous use imports */` |
|     4735 | 2465 | `	SyBlobReset(&pGen->sNamespace);` |
|     4735 | 2466 | `	GenStateResetUseImports(&(*pGen),pGen->pVm);` |
|     4735 | 2467 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 2468 | `		return SXRET_OK; /* Global namespace (bare "namespace;") */` |
|        - | 2469 | `	}` |
|     4735 | 2470 | `	if( pGen->pIn->nType & PH7_TK_SEMI ){` |
|      ! 0 | 2471 | `		return SXRET_OK; /* namespace; — switch to global namespace */` |
|        - | 2472 | `	}` |
|     4735 | 2473 | `	if( pGen->pIn->nType & PH7_TK_OCB ){` |
|        8 | 2474 | `		return SXRET_OK; /* namespace { } — global namespace block */` |
|        - | 2475 | `	}` |
|        - | 2476 | `	/* Collect the namespace path: namespace Foo\Bar\Baz */` |
|     9543 | 2477 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|     4819 | 2478 | `		if( pGen->pIn->nType & PH7_TK_NSSEP ){` |
|        - | 2479 | `			/* Append backslash separator */` |
|       51 | 2480 | `			if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|       51 | 2481 | `				SyBlobAppend(&pGen->sNamespace,"\\",1);` |
|       23 | 2482 | `			}` |
|       28 | 2483 | `		}else{` |
|        - | 2484 | `			/* Append identifier */` |
|     4773 | 2485 | `			SyBlobAppend(&pGen->sNamespace,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|        - | 2486 | `		}` |
|     4819 | 2487 | `		pGen->pIn++;` |
|        5 | 2488 | `	}` |
|     4729 | 2489 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI\|PH7_TK_OCB)) == 0 ){` |
|        8 | 2490 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - | 2491 | `			"syntax error, unexpected %s \"%z\", expecting \"{\"",` |
|        4 | 2492 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        6 | 2493 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2494 | `			return SXERR_ABORT;` |
|        - | 2495 | `		}` |
|        2 | 2496 | `	}` |
|     4729 | 2497 | `	return SXRET_OK;` |
|     2370 | 2498 | `}` |
|        - | 2499 | `/*` |
|        - | 2500 | ` * Initialize the three use-import tables of a code generator.` |
|        - | 2501 | ` *` |
|        - | 2502 | ` * php resolves CLASS and FUNCTION imports case-INSENSITIVELY, like every other` |
|        - | 2503 | ``  * name in those two families: `use A\Cee;` then `CEE::K`, `use A\Cee as Alias;` `` |
|        - | 2504 | `` * then `ALIAS::K`, `use function A\eff;` then `EFF()`, and a wrong-case leading`` |
|        - | 2505 | `` * segment of an imported namespace (`use A\B;` then `b\Cee::K`) all resolve.`` |
|        - | 2506 | ` * So both tables fold through SyStrHash/SyStrnmicmp, exactly like hClass /` |
|        - | 2507 | ` * hMethod / hFunction.` |
|        - | 2508 | ` *` |
|        - | 2509 | ` * The CONST table stays BYTE-EXACT: php keeps constant names case-sensitive,` |
|        - | 2510 | `` * so `use const A\KAY;` followed by `kay` must remain an undefined constant.`` |
|        - | 2511 | ` * That asymmetry is why the three tables exist separately.` |
|        - | 2512 | ` */` |
|   122428 | 2513 | `PH7_PRIVATE void GenStateInitUseImports(ph7_gen_state *pGen,ph7_vm *pVm)` |
|        5 | 2514 | `{` |
|   122433 | 2515 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|   122433 | 2516 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|   122433 | 2517 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|   122433 | 2518 | `}` |
|        - | 2519 | `/*` |
|        - | 2520 | ` * Drop every import currently in scope and start a fresh set (a namespace` |
|        - | 2521 | ` * switch clears imports).  Keeps the case rules of GenStateInitUseImports.` |
|        - | 2522 | ` */` |
|   117896 | 2523 | `PH7_PRIVATE void GenStateResetUseImports(ph7_gen_state *pGen,ph7_vm *pVm)` |
|        5 | 2524 | `{` |
|   117901 | 2525 | `	SyHashRelease(&pGen->hUseImports);` |
|   117901 | 2526 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|   117901 | 2527 | `	SyHashRelease(&pGen->hUseConstImports);` |
|   117901 | 2528 | `	GenStateInitUseImports(&(*pGen),&(*pVm));` |
|   117901 | 2529 | `}` |
|        - | 2530 | `/*` |
|        - | 2531 | ` * The two DECLARED-name tables (classes and functions declared so far in this` |
|        - | 2532 | ` * compile unit). php refuses an import whose name a declaration already took, and` |
|        - | 2533 | ` * the check is per COMPILE UNIT and case-INSENSITIVE — a class declared by a file` |
|        - | 2534 | `` * this one later `require`s is invisible to it, because that file compiles after`` |
|        - | 2535 | ` * this one has finished. Both tables key on the FQN, so they survive a namespace` |
|        - | 2536 | ` * switch (which clears only the imports).` |
|        - | 2537 | ` */` |
|   117698 | 2538 | `PH7_PRIVATE void GenStateInitSeenSymbols(ph7_gen_state *pGen,ph7_vm *pVm)` |
|        5 | 2539 | `{` |
|   117703 | 2540 | `	SyHashInit(&pGen->hSeenClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|   117703 | 2541 | `	SyHashInit(&pGen->hSeenFunc,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|   117703 | 2542 | `}` |
|   113170 | 2543 | `PH7_PRIVATE void GenStateReleaseSeenSymbols(ph7_gen_state *pGen)` |
|        5 | 2544 | `{` |
|   113175 | 2545 | `	SyHashRelease(&pGen->hSeenClass);` |
|   113175 | 2546 | `	SyHashRelease(&pGen->hSeenFunc);` |
|   113175 | 2547 | `}` |
|   113166 | 2548 | `PH7_PRIVATE void GenStateResetSeenSymbols(ph7_gen_state *pGen,ph7_vm *pVm)` |
|        5 | 2549 | `{` |
|   113171 | 2550 | `	GenStateReleaseSeenSymbols(&(*pGen));` |
|   113171 | 2551 | `	GenStateInitSeenSymbols(&(*pGen),&(*pVm));` |
|   113171 | 2552 | `}` |
|        - | 2553 | `/*` |
|        - | 2554 | ` * Record one declared CLASS (bFunc = 0) or FUNCTION (bFunc = 1) FQN so a later` |
|        - | 2555 | `` * `use` in this compile unit can see that the name is taken.`` |
|        - | 2556 | ` */` |
|  1204888 | 2557 | `PH7_PRIVATE void GenStateRecordDeclaredName(ph7_gen_state *pGen,int bFunc,const SyString *pFqn)` |
|        5 | 2558 | `{` |
|  1204893 | 2559 | `	SyHash *pHash = bFunc ? &pGen->hSeenFunc : &pGen->hSeenClass;` |
|        - | 2560 | `	char *zDup;` |
|  1204893 | 2561 | `	if( pFqn->nByte < 1 \|\| SyHashGet(pHash,pFqn->zString,pFqn->nByte) != 0 ){` |
|       23 | 2562 | `		return;` |
|        - | 2563 | `	}` |
|  1204875 | 2564 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pFqn->zString,pFqn->nByte);` |
|  1204875 | 2565 | `	if( zDup ){` |
|        - | 2566 | `		/* The blob the caller built is released on its way out, so the table owns` |
|        - | 2567 | `		 * a pool copy (freed in bulk with the VM, like the import FQNs). */` |
|  1204875 | 2568 | `		SyHashInsert(pHash,zDup,pFqn->nByte,zDup);` |
|   602435 | 2569 | `	}` |
|   602449 | 2570 | `}` |
|        - | 2571 | `/*` |
|        - | 2572 | `` * php refuses a DECLARATION whose short name a local `use` import already took:`` |
|        - | 2573 | ` *` |
|        - | 2574 | ` *   use A\Cee;  class Cee {}    Cannot redeclare class B\Cee (previously declared as local import)` |
|        - | 2575 | ` *   use function A\eff;  function eff(){}` |
|        - | 2576 | ` *                               Cannot redeclare function B\eff() (previously declared as local import)` |
|        - | 2577 | ` *   use const A\KAY;  const KAY = 1;` |
|        - | 2578 | ` *                               Cannot declare const B\KAY because the name is already in use` |
|        - | 2579 | ` *` |
|        - | 2580 | `` * A SELF-import (`use B\Cee;` inside `namespace B;`) names this very declaration`` |
|        - | 2581 | ` * and is a no-op, so it is exempt. iKind: 0 = class family (interface/trait/enum` |
|        - | 2582 | ` * included — php says "class" for all four), 1 = function, 2 = const.` |
|        - | 2583 | ` */` |
|  1205004 | 2584 | `PH7_PRIVATE sxi32 GenStateGuardImportRedeclare(ph7_gen_state *pGen,int iKind,` |
|        - | 2585 | `	const SyString *pShort,const SyString *pFqn,sxu32 nLine)` |
|        5 | 2586 | `{` |
|        - | 2587 | `	SyHash *pImports;` |
|        - | 2588 | `	SyHashEntry *pEntry;` |
|        - | 2589 | `	const char *zImported;` |
|        - | 2590 | `	sxu32 nImported;` |
|  1205009 | 2591 | `	switch( iKind ){` |
|   595119 | 2592 | `		case 1:  pImports = &pGen->hUseFuncImports; break;` |
|      121 | 2593 | `		case 2:  pImports = &pGen->hUseConstImports; break;` |
|   609779 | 2594 | `		default: pImports = &pGen->hUseImports; break;` |
|        - | 2595 | `	}` |
|  1205009 | 2596 | `	pEntry = SyHashGet(pImports,(const void *)pShort->zString,pShort->nByte);` |
|  1205009 | 2597 | `	if( pEntry == 0 ){` |
|  1204995 | 2598 | `		return SXRET_OK;` |
|        - | 2599 | `	}` |
|       18 | 2600 | `	zImported = (const char *)pEntry->pUserData;` |
|       18 | 2601 | `	nImported = SyStrlen(zImported);` |
|       14 | 2602 | `	if( nImported == pFqn->nByte` |
|       22 | 2603 | `	 && (iKind == 2 ? SyMemcmp((const void *)zImported,(const void *)pFqn->zString,nImported) == 0` |
|        8 | 2604 | `	                : SyStrnicmp(zImported,pFqn->zString,nImported) == 0) ){` |
|        7 | 2605 | `		return SXRET_OK; /* the import IS this declaration */` |
|        - | 2606 | `	}` |
|       13 | 2607 | `	if( iKind == 2 ){` |
|        4 | 2608 | `		return PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        1 | 2609 | `			"Cannot declare const %z because the name is already in use",pFqn);` |
|        - | 2610 | `	}` |
|        8 | 2611 | `	return PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        2 | 2612 | `		iKind == 1 ? "Cannot redeclare function %z() (previously declared as local import)"` |
|        2 | 2613 | `		           : "Cannot redeclare class %z (previously declared as local import)",pFqn);` |
|   602505 | 2614 | `}` |
|        - | 2615 | `/*` |
|        - | 2616 | `` * Register one resolved `use` import: alias -> FQN, in the table its KIND owns`` |
|        - | 2617 | ` * (iUseType: 0 = class, 1 = function, 2 = const).  Shared by the plain form` |
|        - | 2618 | `` * (`use A\Cee;`) and by each member of a group (`use A\{Cee, Dee};`).`` |
|        - | 2619 | ` */` |
|      158 | 2620 | `static sxi32 GenStateAddImport(` |
|        - | 2621 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - | 2622 | `	int iUseType,         /* 0=class, 1=function, 2=const */` |
|        - | 2623 | `	SyBlob *pPath,        /* Fully qualified name being imported */` |
|        - | 2624 | `	SyString *pAlias,     /* Short name it is imported under */` |
|        - | 2625 | `	sxu32 nLine           /* Line of the 'use' keyword (for diagnostics) */` |
|        - | 2626 | `	)` |
|        5 | 2627 | `{` |
|        - | 2628 | `	SyHash *pGenHash;   /* Compile-time import table */` |
|        - | 2629 | `	const char *zKind;  /* php's kind word in the "already in use" message */` |
|        - | 2630 | `	char *zDup;` |
|        - | 2631 | `	sxi32 rc;` |
|        - | 2632 | `	/* Select the target hash table based on import type. */` |
|      163 | 2633 | `	switch( iUseType ){` |
|       36 | 2634 | `		case 1:  pGenHash = &pGen->hUseFuncImports; break;` |
|       37 | 2635 | `		case 2:  pGenHash = &pGen->hUseConstImports; break;` |
|       99 | 2636 | `		default: pGenHash = &pGen->hUseImports; break;` |
|        - | 2637 | `	}` |
|        - | 2638 | `	/* php names the KIND of a non-class import in this message: "Cannot use` |
|        - | 2639 | `	 * function A\eff as eff …" / "Cannot use const A\KAY as KAY …". */` |
|      163 | 2640 | `	zKind = iUseType == 1 ? "function " : iUseType == 2 ? "const " : "";` |
|        - | 2641 | `	/* Check for duplicate import alias (per-type) */` |
|      163 | 2642 | `	if( SyHashGet(pGenHash,pAlias->zString,pAlias->nByte) != 0 ){` |
|       12 | 2643 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 2644 | `			"Cannot use %s%.*s as %z because the name is already in use",` |
|        6 | 2645 | `			zKind,(int)SyBlobLength(pPath),(const char *)SyBlobData(pPath),pAlias);` |
|        9 | 2646 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2647 | `			return SXERR_ABORT;` |
|        - | 2648 | `		}` |
|        3 | 2649 | `	}` |
|        - | 2650 | `	/* …and refuses one whose name a DECLARATION in this compile unit already took` |
|        - | 2651 | ``	 * (`class Cee {} use A\Cee;`), unless the import names that very declaration.`` |
|        - | 2652 | `	 * The name an import occupies is the alias in the CURRENT namespace, which is` |
|        - | 2653 | `	 * what the seen tables key on. php runs this check for classes and functions` |
|        - | 2654 | ``	 * only — a `const` declaration followed by its own `use const` is accepted. */`` |
|      163 | 2655 | `	if( iUseType != 2 ){` |
|        - | 2656 | `		SyBlob sTaken;` |
|      131 | 2657 | `		SyBlobInit(&sTaken,&pGen->pVm->sAllocator);` |
|      131 | 2658 | `		GenStateBuildFQN(&(*pGen),pAlias,&sTaken);` |
|      126 | 2659 | `		if( SyHashGet(iUseType == 1 ? &pGen->hSeenFunc : &pGen->hSeenClass,` |
|      189 | 2660 | `				SyBlobData(&sTaken),SyBlobLength(&sTaken)) != 0` |
|       70 | 2661 | `		 && (SyBlobLength(&sTaken) != SyBlobLength(pPath)` |
|        4 | 2662 | `			\|\| SyStrnicmp((const char *)SyBlobData(&sTaken),(const char *)SyBlobData(pPath),` |
|        6 | 2663 | `				(sxu32)SyBlobLength(&sTaken)) != 0) ){` |
|        8 | 2664 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 2665 | `				"Cannot use %s%.*s as %z because the name is already in use",` |
|        4 | 2666 | `				zKind,(int)SyBlobLength(pPath),(const char *)SyBlobData(pPath),pAlias);` |
|        6 | 2667 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2668 | `				SyBlobRelease(&sTaken);` |
|      ! 0 | 2669 | `				return SXERR_ABORT;` |
|        - | 2670 | `			}` |
|        2 | 2671 | `		}` |
|      131 | 2672 | `		SyBlobRelease(&sTaken);` |
|       63 | 2673 | `	}` |
|        - | 2674 | `	/* Register the import: alias -> FQN.` |
|        - | 2675 | `	 * Strings are allocated from the VM pool allocator and freed` |
|        - | 2676 | `	 * when the entire VM is released. SyHashRelease does not free` |
|        - | 2677 | `	 * user-data, but pool memory is reclaimed in bulk at shutdown. */` |
|      242 | 2678 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      158 | 2679 | `		(const char *)SyBlobData(pPath),SyBlobLength(pPath));` |
|      163 | 2680 | `	if( zDup ){` |
|        - | 2681 | `		/* All three kinds resolve entirely at COMPILE time — a const import is read` |
|        - | 2682 | `		 * by the OP_LOADC candidate builder (compile_node.c), so no runtime table` |
|        - | 2683 | `		 * is needed for it either. */` |
|      163 | 2684 | `		SyHashInsert(pGenHash,pAlias->zString,pAlias->nByte,zDup);` |
|       79 | 2685 | `	}` |
|      163 | 2686 | `	return SXRET_OK;` |
|       84 | 2687 | `}` |
|        - | 2688 | `/*` |
|        - | 2689 | `` * Collect one `\`-separated name into pOut (appending to whatever it holds, with`` |
|        - | 2690 | ` * a separator when needed) and return its LAST segment token, or 0 when the` |
|        - | 2691 | ` * cursor is not on a name at all.` |
|        - | 2692 | ` */` |
|      172 | 2693 | `static SyToken * GenStateCollectNsPath(ph7_gen_state *pGen,SyBlob *pOut)` |
|        5 | 2694 | `{` |
|      177 | 2695 | `	SyToken *pLast = 0;` |
|      611 | 2696 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_NSSEP\|PH7_TK_ID)) ){` |
|      439 | 2697 | `		if( pGen->pIn->nType & PH7_TK_ID ){` |
|      301 | 2698 | `			pLast = pGen->pIn;` |
|      301 | 2699 | `			if( SyBlobLength(pOut) > 0 ){` |
|      155 | 2700 | `				SyBlobAppend(pOut,"\\",1);` |
|       75 | 2701 | `			}` |
|      301 | 2702 | `			SyBlobAppend(pOut,pGen->pIn->sData.zString,pGen->pIn->sData.nByte);` |
|      148 | 2703 | `		}` |
|      439 | 2704 | `		pGen->pIn++;` |
|        5 | 2705 | `	}` |
|      177 | 2706 | `	return pLast;` |
|        5 | 2707 | `}` |
|        - | 2708 | `/*` |
|        - | 2709 | `` * Consume the optional `as Alias` clause, leaving *pAlias untouched when absent.`` |
|        - | 2710 | ` */` |
|      158 | 2711 | `static void GenStateCollectImportAlias(ph7_gen_state *pGen,SyString *pAlias)` |
|        5 | 2712 | `{` |
|      158 | 2713 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD)` |
|      103 | 2714 | `		&& PH7_TKWRD_AS == SX_PTR_TO_INT(pGen->pIn->pUserData) ){` |
|       42 | 2715 | `		pGen->pIn++; /* Jump 'as' */` |
|       42 | 2716 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_ID) ){` |
|       42 | 2717 | `			*pAlias = pGen->pIn->sData;` |
|       42 | 2718 | `			pGen->pIn++;` |
|       19 | 2719 | `		}` |
|       19 | 2720 | `	}` |
|      163 | 2721 | `}` |
|        - | 2722 | `/*` |
|        - | 2723 | ` * Compile the members of a GROUP use declaration (php 7.0):` |
|        - | 2724 | ` *` |
|        - | 2725 | ` *      use A\{Cee, Dee as D2, Sub\Eee};` |
|        - | 2726 | ` *      use function A\{f, g as h};` |
|        - | 2727 | ` *      use A\{function f, const K, Cee};   // per-member kind, untyped group only` |
|        - | 2728 | ` *` |
|        - | 2729 | `` * pPrefix holds the path before the brace; the cursor sits on `{`.  Each member`` |
|        - | 2730 | `` * is the prefix, a `\`, and the member's own (possibly multi-segment) name.  A`` |
|        - | 2731 | ` * trailing comma is allowed, an empty group is not.` |
|        - | 2732 | ` */` |
|       10 | 2733 | `static sxi32 GenStateCompileGroupUse(ph7_gen_state *pGen,SyBlob *pPrefix,int iUseType,sxu32 nLine)` |
|        1 | 2734 | `{` |
|        - | 2735 | `	SyBlob sPath;` |
|       11 | 2736 | `	sxi32 rc = SXRET_OK;` |
|       11 | 2737 | `	pGen->pIn++; /* Jump '{' */` |
|       11 | 2738 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|       11 | 2739 | `	for(;;){` |
|       23 | 2740 | `		int iMemberType = iUseType;` |
|        - | 2741 | `		SyString sAlias;` |
|        - | 2742 | `		SyToken *pLast;` |
|        - | 2743 | ``		/* `function`/`const` may qualify a single member, but only inside a`` |
|        - | 2744 | ``		 * group that is not itself typed (php rejects `use function A\{const C}`). */`` |
|       23 | 2745 | `		if( iUseType == 0 && pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|        5 | 2746 | `			sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|        5 | 2747 | `			if( nKey == PH7_TKWRD_FUNCTION ){` |
|        3 | 2748 | `				iMemberType = 1;` |
|        3 | 2749 | `				pGen->pIn++;` |
|        4 | 2750 | `			}else if( nKey == PH7_TKWRD_CONST ){` |
|        3 | 2751 | `				iMemberType = 2;` |
|        3 | 2752 | `				pGen->pIn++;` |
|        1 | 2753 | `			}` |
|        2 | 2754 | `		}` |
|       23 | 2755 | `		SyBlobReset(&sPath);` |
|       23 | 2756 | `		SyBlobAppend(&sPath,SyBlobData(pPrefix),SyBlobLength(pPrefix));` |
|       23 | 2757 | `		pLast = GenStateCollectNsPath(pGen,&sPath);` |
|       23 | 2758 | `		if( pLast == 0 ){` |
|        - | 2759 | ``			/* No member name: `use A\{};` or a stray token.  Report once, then`` |
|        - | 2760 | `			 * skip to the end of the group so the statement does not cascade. */` |
|      ! 0 | 2761 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,` |
|        - | 2762 | `				"syntax error, unexpected %s \"%z\", expecting identifier",` |
|      ! 0 | 2763 | `				TokenTypeName(pGen->pIn < pGen->pEnd ? pGen->pIn->nType : 0),` |
|      ! 0 | 2764 | `				pGen->pIn < pGen->pEnd ? &pGen->pIn->sData : 0);` |
|      ! 0 | 2765 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_CCB\|PH7_TK_SEMI)) == 0 ){` |
|      ! 0 | 2766 | `				pGen->pIn++;` |
|      ! 0 | 2767 | `			}` |
|      ! 0 | 2768 | `			break;` |
|        - | 2769 | `		}` |
|       23 | 2770 | `		sAlias = pLast->sData; /* Default alias is the member's last component */` |
|       23 | 2771 | `		GenStateCollectImportAlias(pGen,&sAlias);` |
|       23 | 2772 | `		rc = GenStateAddImport(&(*pGen),iMemberType,&sPath,&sAlias,nLine);` |
|       23 | 2773 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2774 | `			break;` |
|        - | 2775 | `		}` |
|       23 | 2776 | `		rc = SXRET_OK;` |
|       23 | 2777 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|       15 | 2778 | `			pGen->pIn++;` |
|       15 | 2779 | `			if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) ){` |
|        3 | 2780 | `				break; /* Trailing comma before the closing brace */` |
|        - | 2781 | `			}` |
|       13 | 2782 | `			continue;` |
|        - | 2783 | `		}` |
|        9 | 2784 | `		break;` |
|      ! 0 | 2785 | `	}` |
|       11 | 2786 | `	SyBlobRelease(&sPath);` |
|       11 | 2787 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 2788 | `		return SXERR_ABORT;` |
|        - | 2789 | `	}` |
|       11 | 2790 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_CCB) ){` |
|       11 | 2791 | `		pGen->pIn++; /* Jump '}' */` |
|        5 | 2792 | `	}` |
|       11 | 2793 | `	return SXRET_OK;` |
|        6 | 2794 | `}` |
|        - | 2795 | `/*` |
|        - | 2796 | ` * Compile the 'use' statement` |
|        - | 2797 | ` * According to the PHP language reference manual` |
|        - | 2798 | ` *  The ability to refer to an external fully qualified name with an alias or importing` |
|        - | 2799 | ` *  is an important feature of namespaces. This is similar to the ability of unix-based` |
|        - | 2800 | ` *  filesystems to create symbolic links to a file or to a directory.` |
|        - | 2801 | ` *  PHP namespaces support three kinds of aliasing or importing: aliasing a class name` |
|        - | 2802 | ` *  aliasing an interface name, and aliasing a namespace name. Note that importing` |
|        - | 2803 | ` *  a function or constant is not supported.` |
|        - | 2804 | ` *  In PHP, aliasing is accomplished with the 'use' operator.` |
|        - | 2805 | ` * NOTE` |
|        - | 2806 | ` *  AS OF THIS VERSION NAMESPACE SUPPORT IS DISABLED. IF YOU NEED A WORKING VERSION THAT IMPLEMENT` |
|        - | 2807 | ` *  NAMESPACE,PLEASE CONTACT SYMISC SYSTEMS VIA contact@symisc.net.` |
|        - | 2808 | ` */` |
|      148 | 2809 | `PH7_PRIVATE sxi32 PH7_CompileUse(ph7_gen_state *pGen)` |
|        5 | 2810 | `{` |
|        - | 2811 | `	sxu32 nLine;` |
|        - | 2812 | `	sxi32 rc;` |
|        - | 2813 | `	SyBlob sPath;` |
|        - | 2814 | `	SyString sAlias;` |
|        - | 2815 | `	SyToken *pLast;` |
|        - | 2816 | `	int iUseType; /* 0=class, 1=function, 2=const */` |
|      153 | 2817 | `	nLine = pGen->pIn->nLine;` |
|      153 | 2818 | `	pGen->pIn++; /* Jump the 'use' keyword */` |
|        - | 2819 | `	/* Detect 'function' or 'const' keyword after 'use' (PHP 5.6+) */` |
|      153 | 2820 | `	iUseType = 0;` |
|      153 | 2821 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) ){` |
|       61 | 2822 | `		sxu32 nKey = (sxu32)(SX_PTR_TO_INT(pGen->pIn->pUserData));` |
|       61 | 2823 | `		if( nKey == PH7_TKWRD_FUNCTION ){` |
|       32 | 2824 | `			iUseType = 1;` |
|       32 | 2825 | `			pGen->pIn++;` |
|       47 | 2826 | `		}else if( nKey == PH7_TKWRD_CONST ){` |
|       33 | 2827 | `			iUseType = 2;` |
|       33 | 2828 | `			pGen->pIn++;` |
|       14 | 2829 | `		}` |
|       28 | 2830 | `	}` |
|      153 | 2831 | `	SyBlobInit(&sPath,&pGen->pVm->sAllocator);` |
|        - | 2832 | `	/* Process one or more use declarations separated by commas */` |
|       75 | 2833 | `	for(;;){` |
|      155 | 2834 | `		if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 2835 | `			break;` |
|        - | 2836 | `		}` |
|      155 | 2837 | `		SyBlobReset(&sPath);` |
|        - | 2838 | `		/* Collect the full namespace path */` |
|      155 | 2839 | `		pLast = GenStateCollectNsPath(pGen,&sPath);` |
|      155 | 2840 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) && SyBlobLength(&sPath) > 0 ){` |
|        - | 2841 | `			/* GROUP declaration: what was collected is the shared prefix.  php` |
|        - | 2842 | `			 * does not let a group be comma-combined with another declaration,` |
|        - | 2843 | `			 * so the members close the statement. */` |
|       11 | 2844 | `			rc = GenStateCompileGroupUse(&(*pGen),&sPath,iUseType,nLine);` |
|       11 | 2845 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2846 | `				SyBlobRelease(&sPath);` |
|      ! 0 | 2847 | `				return SXERR_ABORT;` |
|        - | 2848 | `			}` |
|       11 | 2849 | `			break;` |
|        - | 2850 | `		}` |
|      145 | 2851 | `		if( pLast == 0 ){` |
|        - | 2852 | `			/* Empty path */` |
|        6 | 2853 | `			break;` |
|        - | 2854 | `		}` |
|        - | 2855 | `		/* Default alias is the last component of the path */` |
|      141 | 2856 | `		sAlias = pLast->sData;` |
|        - | 2857 | `		/* Check for explicit alias: use Foo\Bar as Baz */` |
|      141 | 2858 | `		GenStateCollectImportAlias(pGen,&sAlias);` |
|      141 | 2859 | `		rc = GenStateAddImport(&(*pGen),iUseType,&sPath,&sAlias,nLine);` |
|      141 | 2860 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2861 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 2862 | `			return SXERR_ABORT;` |
|        - | 2863 | `		}` |
|        - | 2864 | `		/* Check for comma (multiple use declarations) */` |
|      141 | 2865 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_COMMA) ){` |
|        3 | 2866 | `			pGen->pIn++;` |
|        2 | 2867 | `		}else{` |
|       72 | 2868 | `			break;` |
|        - | 2869 | `		}` |
|        1 | 2870 | `	}` |
|      153 | 2871 | `	SyBlobRelease(&sPath);` |
|      153 | 2872 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|        4 | 2873 | `		rc = PH7_GenCompileError(&(*pGen),E_PARSE,nLine,"syntax error, unexpected %s \"%z\"",` |
|        2 | 2874 | `			TokenTypeName(pGen->pIn->nType),&pGen->pIn->sData);` |
|        3 | 2875 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2876 | `			return SXERR_ABORT;` |
|        - | 2877 | `		}` |
|        1 | 2878 | `	}` |
|      153 | 2879 | `	return SXRET_OK;` |
|       79 | 2880 | `}` |
|        - | 2881 | `/*` |
|        - | 2882 | ` * Compile the stupid 'declare' language construct.` |
|        - | 2883 | ` *` |
|        - | 2884 | ` * According to the PHP language reference manual.` |
|        - | 2885 | ` *  The declare construct is used to set execution directives for a block of code.` |
|        - | 2886 | ` *  The syntax of declare is similar to the syntax of other flow control constructs:` |
|        - | 2887 | ` *  declare (directive)` |
|        - | 2888 | ` *   statement` |
|        - | 2889 | ` * The directive section allows the behavior of the declare block to be set.` |
|        - | 2890 | ` *  Currently only two directives are recognized: the ticks directive and the encoding directive.` |
|        - | 2891 | ` * The statement part of the declare block will be executed - how it is executed and what side` |
|        - | 2892 | ` * effects occur during execution may depend on the directive set in the directive block.` |
|        - | 2893 | ` * The declare construct can also be used in the global scope, affecting all code following` |
|        - | 2894 | ` * it (however if the file with declare was included then it does not affect the parent file).` |
|        - | 2895 | ` * <?php` |
|        - | 2896 | ` * // these are the same:` |
|        - | 2897 | ` * // you can use this:` |
|        - | 2898 | ` * declare(ticks=1) {` |
|        - | 2899 | ` *   // entire script here` |
|        - | 2900 | ` * }` |
|        - | 2901 | ` * // or you can use this:` |
|        - | 2902 | ` * declare(ticks=1);` |
|        - | 2903 | ` * // entire script here` |
|        - | 2904 | ` * ?>` |
|        - | 2905 | ` *` |
|        - | 2906 | ` * Well,actually this language construct is a NO-OP in the current release of the PH7 engine.` |
|        - | 2907 | ` */` |
|        - | 2908 | `/*` |
|        - | 2909 | ` * Match a directive name against a known literal (case-insensitive).` |
|        - | 2910 | ` */` |
|       92 | 2911 | `static int DeclareNameIs(SyString *pName, const char *zWant, sxu32 nWant)` |
|        5 | 2912 | `{` |
|      138 | 2913 | `	return SyStringLength(pName) == nWant` |
|       92 | 2914 | `	    && SyStrnicmp(SyStringData(pName), zWant, nWant) == 0;` |
|        5 | 2915 | `}` |
|        - | 2916 |  |
|       50 | 2917 | `PH7_PRIVATE sxi32 PH7_CompileDeclare(ph7_gen_state *pGen)` |
|        5 | 2918 | `{` |
|       55 | 2919 | `	sxu32 nLine = pGen->pIn->nLine;` |
|       55 | 2920 | `	SyToken *pBodyEnd = 0;` |
|        - | 2921 | `	SyToken *pBodyStart;` |
|        - | 2922 | `	SyToken *pCursor;` |
|        - | 2923 | `	int bHasStrictTypes;` |
|        - | 2924 | `	int bBlockForm;` |
|        - | 2925 | `	int bPlacementOk;` |
|        - | 2926 | `	sxi32 rc;` |
|       55 | 2927 | `	pGen->pIn++; /* Jump the 'declare' keyword */` |
|       55 | 2928 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*'('*/ ){` |
|        5 | 2929 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|        5 | 2930 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2931 | `			return SXERR_ABORT;` |
|        - | 2932 | `		}` |
|        5 | 2933 | `		goto Synchro;` |
|        - | 2934 | `	}` |
|       51 | 2935 | `	pGen->pIn++; /* Jump the left parenthesis */` |
|       51 | 2936 | `	pBodyStart = pGen->pIn;` |
|        - | 2937 | `	/* Delimit the directive body (between the outer '(' and its matching ')'). */` |
|       51 | 2938 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN/*'('*/,PH7_TK_RPAREN/*')'*/,&pBodyEnd);` |
|       51 | 2939 | `	if( pBodyEnd >= pGen->pEnd ){` |
|      ! 0 | 2940 | `		rc = PH7_GenSyntaxError(pGen,pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\")\"");` |
|      ! 0 | 2941 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2942 | `			return SXERR_ABORT;` |
|        - | 2943 | `		}` |
|      ! 0 | 2944 | `		return SXRET_OK;` |
|        - | 2945 | `	}` |
|        - | 2946 | `	/* Update the cursor past the closing ')'. pBodyStart..pBodyEnd (exclusive)` |
|        - | 2947 | `	 * now delimits the comma-separated directive list. */` |
|       51 | 2948 | `	pGen->pIn = &pBodyEnd[1];` |
|       51 | 2949 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|      ! 0 | 2950 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"declare: Expecting ';' or '{' after directive");` |
|      ! 0 | 2951 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2952 | `			return SXERR_ABORT;` |
|        - | 2953 | `		}` |
|      ! 0 | 2954 | `	}` |
|       51 | 2955 | `	bBlockForm = ( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OCB) ) ? 1 : 0;` |
|       51 | 2956 | `	bPlacementOk = ( pGen->pCurrent == &pGen->sGlobal && !pGen->bStrictTypesLocked );` |
|       51 | 2957 | `	bHasStrictTypes = 0;` |
|        - | 2958 | `	/* First pass: scan directive names to detect any strict_types occurrence.` |
|        - | 2959 | `	 * PHP applies strict_types placement and block-form rules as long as the` |
|        - | 2960 | `	 * directive appears anywhere in the list, before validating values. */` |
|       51 | 2961 | `	pCursor = pBodyStart;` |
|       63 | 2962 | `	while( pCursor < pBodyEnd ){` |
|       59 | 2963 | `		if( (pCursor->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|       51 | 2964 | `			if( DeclareNameIs(&pCursor->sData, "strict_types", sizeof("strict_types")-1) ){` |
|       47 | 2965 | `				bHasStrictTypes = 1;` |
|       47 | 2966 | `				break;` |
|        - | 2967 | `			}` |
|        2 | 2968 | `		}` |
|       14 | 2969 | `		pCursor++;` |
|        2 | 2970 | `	}` |
|       51 | 2971 | `	if( bHasStrictTypes && bBlockForm ){` |
|        3 | 2972 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2973 | `			"strict_types declaration must not use block mode");` |
|        3 | 2974 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        3 | 2975 | `		return SXRET_OK;` |
|        - | 2976 | `	}` |
|       49 | 2977 | `	if( bHasStrictTypes && !bPlacementOk ){` |
|        6 | 2978 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2979 | `			"strict_types declaration must be the very first statement in the script");` |
|        6 | 2980 | `		if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        6 | 2981 | `		return SXRET_OK;` |
|        - | 2982 | `	}` |
|        - | 2983 | `	/* Second pass: iterate comma-separated directives and apply each. */` |
|       45 | 2984 | `	pCursor = pBodyStart;` |
|       85 | 2985 | `	while( pCursor < pBodyEnd ){` |
|        - | 2986 | `		SyToken *pNameTok;` |
|        - | 2987 | `		SyToken *pEqTok;` |
|        - | 2988 | `		SyToken *pValTok;` |
|        - | 2989 | `		SyString *pDirName;` |
|        - | 2990 | `		int bIsStrict;` |
|        - | 2991 | `		int iStrictValue;` |
|       47 | 2992 | `		pNameTok = pCursor;` |
|       47 | 2993 | `		if( (pNameTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 2994 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 2995 | `				"declare: Expecting a directive name");` |
|      ! 0 | 2996 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 2997 | `			return SXRET_OK;` |
|        - | 2998 | `		}` |
|       47 | 2999 | `		pEqTok = pNameTok + 1;` |
|       47 | 3000 | `		if( pEqTok >= pBodyEnd \|\| (pEqTok->nType & PH7_TK_EQUAL) == 0 ){` |
|      ! 0 | 3001 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 3002 | `				"declare: Expecting '=' after directive name");` |
|      ! 0 | 3003 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 3004 | `			return SXRET_OK;` |
|        - | 3005 | `		}` |
|       47 | 3006 | `		pValTok = pEqTok + 1;` |
|       47 | 3007 | `		if( pValTok >= pBodyEnd ){` |
|      ! 0 | 3008 | `			rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 3009 | `				"declare: Expecting value after '='");` |
|      ! 0 | 3010 | `			if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 3011 | `			return SXRET_OK;` |
|        - | 3012 | `		}` |
|       47 | 3013 | `		pDirName = &pNameTok->sData;` |
|       47 | 3014 | `		bIsStrict = DeclareNameIs(pDirName, "strict_types", sizeof("strict_types")-1);` |
|       47 | 3015 | `		if( bIsStrict ){` |
|        - | 3016 | `			/* strict_types value must be a literal 0 or 1 (integer). PHP` |
|        - | 3017 | `			 * distinguishes non-literal (bareword) from other bad values. */` |
|       43 | 3018 | `			if( (pValTok->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0 ){` |
|      ! 0 | 3019 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 3020 | `					"declare(strict_types) value must be a literal");` |
|      ! 0 | 3021 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 3022 | `				return SXRET_OK;` |
|        - | 3023 | `			}` |
|       43 | 3024 | `			iStrictValue = -1;` |
|       43 | 3025 | `			if( pValTok->nType & PH7_TK_INTEGER ){` |
|       43 | 3026 | `				const char *zv = SyStringData(&pValTok->sData);` |
|       43 | 3027 | `				sxu32 nv = SyStringLength(&pValTok->sData);` |
|       43 | 3028 | `				if( nv == 1 && zv[0] == '0' ) iStrictValue = 0;` |
|       41 | 3029 | `				else if( nv == 1 && zv[0] == '1' ) iStrictValue = 1;` |
|       19 | 3030 | `			}` |
|       43 | 3031 | `			if( iStrictValue != 0 && iStrictValue != 1 ){` |
|        3 | 3032 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 3033 | `					"strict_types declaration must have 0 or 1 as its value");` |
|        3 | 3034 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|        3 | 3035 | `				return SXRET_OK;` |
|        - | 3036 | `			}` |
|       40 | 3037 | `			pGen->bStrictTypes = (sxi8)iStrictValue;` |
|       24 | 3038 | `		}else if( DeclareNameIs(pDirName, "encoding", sizeof("encoding")-1) ){` |
|        - | 3039 | `			/* php always ignores declare(encoding=...) unless it was built with` |
|        - | 3040 | `			 * Zend multibyte, and says so in these exact words. */` |
|        3 | 3041 | `			PH7_GenCompileError(&(*pGen),E_WARNING,nLine,` |
|        - | 3042 | `				"declare(encoding=...) ignored because Zend multibyte feature is turned off by settings");` |
|        1 | 3043 | `		}else{` |
|        - | 3044 | `			/* Other directives (ticks and friends) are accepted as no-ops.` |
|        - | 3045 | `			 * This used to emit a NOTICE naming the upstream engine and its` |
|        - | 3046 | `			 * version ("the declare construct is a no-op in the current release` |
|        - | 3047 | `			 * of the PH7(2.1.4) engine") — php prints nothing at all for` |
|        - | 3048 | ``			 * `declare(ticks=1)`, and leaking the old engine's branding into`` |
|        - | 3049 | `			 * user-visible diagnostics was wrong on its own. */` |
|        - | 3050 | `		}` |
|       44 | 3051 | `		pCursor = pValTok + 1;` |
|        - | 3052 | `		/* Consume separating comma (or end). */` |
|       44 | 3053 | `		if( pCursor < pBodyEnd ){` |
|        3 | 3054 | `			if( (pCursor->nType & PH7_TK_COMMA) == 0 ){` |
|      ! 0 | 3055 | `				rc = PH7_GenCompileError(pGen,E_ERROR,nLine,` |
|        - | 3056 | `					"declare: Expecting ',' or ')' after directive value");` |
|      ! 0 | 3057 | `				if( rc == SXERR_ABORT ) return SXERR_ABORT;` |
|      ! 0 | 3058 | `				return SXRET_OK;` |
|        - | 3059 | `			}` |
|        3 | 3060 | `			pCursor++;` |
|        1 | 3061 | `		}` |
|        4 | 3062 | `	}` |
|        - | 3063 | `	/* Declares never lock the first-statement rule: PHP allows another` |
|        - | 3064 | `	 * declare(strict_types) to follow immediately, or a declare(ticks)` |
|        - | 3065 | `	 * to precede strict_types. Only non-declare statements lock. */` |
|       42 | 3066 | `	return SXRET_OK;` |
|        2 | 3067 | `Synchro:` |
|        - | 3068 | `	/* Sycnhronize with the first semi-colon ';' or curly braces '{' */` |
|       15 | 3069 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_OCB/*'{'*/)) == 0 ){` |
|       11 | 3070 | `		pGen->pIn++;` |
|        1 | 3071 | `	}` |
|        5 | 3072 | `	return SXRET_OK;` |
|       30 | 3073 | `}` |
|        - | 3074 | `/*` |
|        - | 3075 | ` * Compile a class constant.` |
|        - | 3076 | ` * According to the PHP language reference manual` |
|        - | 3077 | ` *  Class Constants` |
|        - | 3078 | ` *   It is possible to define constant values on a per-class basis remaining` |
|        - | 3079 | ` *   the same and unchangeable. Constants differ from normal variables in that` |
|        - | 3080 | ` *   you don't use the $ symbol to declare or use them.` |
|        - | 3081 | ` *   The value must be a constant expression, not (for example) a variable,` |
|        - | 3082 | ` *   a property, a result of a mathematical operation, or a function call.` |
|        - | 3083 | ` *   It's also possible for interfaces to have constants.` |
|        - | 3084 | ` * Symisc eXtension.` |
|        - | 3085 | ` *  PH7 allow any complex expression to be associated with the constant while` |
|        - | 3086 | ` *  the zend engine would allow only simple scalar value.` |
|        - | 3087 | ` *  Example:` |
|        - | 3088 | ` *   class Test{` |
|        - | 3089 | ` *        const MyConst = "Hello"."world: ".rand_str(3); //concatenation operation + Function call` |
|        - | 3090 | ` *   };` |
|        - | 3091 | ` *   var_dump(TEST::MyConst);` |
|        - | 3092 | ` *   Refer to the official documentation for more information on the powerful extension` |
|        - | 3093 | ` *   introduced by the PH7 engine to the OO subsystem.` |
|        - | 3094 | ` */` |
|        - | 3095 | `/*` |
|        - | 3096 | ` * Exception handling.` |
|        - | 3097 | ` *  According to the PHP language reference manual` |
|        - | 3098 | ` *    An exception can be thrown, and caught ("catched") within PHP. Code may be surrounded` |
|        - | 3099 | ` *    in a try block, to facilitate the catching of potential exceptions. Each try must have` |
|        - | 3100 | ` *    at least one corresponding catch block. Multiple catch blocks can be used to catch` |
|        - | 3101 | ` *    different classes of exceptions. Normal execution (when no exception is thrown within` |
|        - | 3102 | ` *    the try block, or when a catch matching the thrown exception's class is not present)` |
|        - | 3103 | ` *    will continue after that last catch block defined in sequence. Exceptions can be thrown` |
|        - | 3104 | ` *    (or re-thrown) within a catch block.` |
|        - | 3105 | ` *    When an exception is thrown, code following the statement will not be executed, and PHP` |
|        - | 3106 | ` *    will attempt to find the first matching catch block. If an exception is not caught, a PHP` |
|        - | 3107 | ` *    Fatal Error will be issued with an "Uncaught Exception ..." message, unless a handler has` |
|        - | 3108 | ` *    been defined with set_exception_handler().` |
|        - | 3109 | ` *    The thrown object must be an instance of the Exception class or a subclass of Exception.` |
|        - | 3110 | ` *    Trying to throw an object that is not will result in a PHP Fatal Error.` |
|        - | 3111 | ` */` |
|        - | 3112 | `/*` |
|        - | 3113 | ` * Expression tree validator callback associated with the 'throw' statement.` |
|        - | 3114 | ` * Return SXRET_OK if the tree form a valid expression.Any other error` |
|        - | 3115 | ` * indicates failure.` |
|        - | 3116 | ` */` |
|   657140 | 3117 | `static sxi32 GenStateThrowNodeValidator(ph7_gen_state *pGen,ph7_expr_node *pRoot)` |
|        5 | 3118 | `{` |
|        - | 3119 | ``	/* php decides throwability at RUNTIME: `throw 5` compiles and raises the`` |
|        - | 3120 | `	 * catchable Error "Can only throw objects" when it executes, and a` |
|        - | 3121 | `	 * non-Throwable object raises "Cannot throw objects that do not implement` |
|        - | 3122 | `	 * Throwable". This used to whitelist a handful of expression shapes and` |
|        - | 3123 | `	 * reject the rest at compile time as a "friendlier" error, which meant the` |
|        - | 3124 | `	 * program never ran and no catch could ever see it — and it was inconsistent` |
|        - | 3125 | ``	 * anyway, since `throw $v` (a variable holding an int) always compiled and`` |
|        - | 3126 | `	 * fell through to the same runtime path. OP_THROW now owns the whole rule,` |
|        - | 3127 | ``	 * so accept any expression here; `throw;` with no operand is still rejected`` |
|        - | 3128 | `	 * by PH7_CompileThrow's SXERR_EMPTY branch. */` |
|   328570 | 3129 | `	SXUNUSED(pGen);` |
|   328570 | 3130 | `	SXUNUSED(pRoot);` |
|   657145 | 3131 | `	return SXRET_OK;` |
|        5 | 3132 | `}` |
|        - | 3133 | `/*` |
|        - | 3134 | ` * Compile a 'throw' statement.` |
|        - | 3135 | ` * throw: This is how you trigger an exception.` |
|        - | 3136 | ` * Each "throw" block must have at least one "catch" block associated with it.` |
|        - | 3137 | ` */` |
|   657100 | 3138 | `PH7_PRIVATE sxi32 PH7_CompileThrow(ph7_gen_state *pGen)` |
|        5 | 3139 | `{` |
|   657105 | 3140 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 3141 | `	GenBlock *pBlock;` |
|        - | 3142 | `	sxu32 nIdx;` |
|        - | 3143 | `	sxi32 rc;` |
|   657105 | 3144 | `	pGen->pIn++; /* Jump the 'throw' keyword */` |
|        - | 3145 | `	/* Compile the expression */` |
|   657105 | 3146 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|   657105 | 3147 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 | 3148 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"throw: Expecting an exception class instance");` |
|      ! 0 | 3149 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3150 | `			return SXERR_ABORT;` |
|        - | 3151 | `		}` |
|      ! 0 | 3152 | `		return SXRET_OK;` |
|        - | 3153 | `	}` |
|   657105 | 3154 | `	pBlock = pGen->pCurrent;` |
|        - | 3155 | `	/* Point to the top most function or try block and emit the forward jump */` |
|  2618309 | 3156 | `	while(pBlock->pParent){` |
|  2618301 | 3157 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|   657097 | 3158 | `			break;` |
|        - | 3159 | `		}` |
|        - | 3160 | `		/* Point to the parent block */` |
|  1961209 | 3161 | `		pBlock = pBlock->pParent;` |
|        5 | 3162 | `	}` |
|        - | 3163 | `	/* Emit the throw instruction */` |
|   657105 | 3164 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|        - | 3165 | `	/* Emit the jump */` |
|   657105 | 3166 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|   657105 | 3167 | `	return SXRET_OK;` |
|   328555 | 3168 | `}` |
|        - | 3169 | `/*` |
|        - | 3170 | ` * Compile a PHP 8.0 'throw' expression.` |
|        - | 3171 | ` * Called from the expression code generator when a 'throw' keyword is` |
|        - | 3172 | `` * encountered in an expression context (e.g. `$x ?? throw new E()`).`` |
|        - | 3173 | ` * Reuses PH7_OP_THROW and the throw-statement's jump-fixup machinery;` |
|        - | 3174 | ` * the validator guarantees the operand is a valid exception target.` |
|        - | 3175 | ` */` |
|       40 | 3176 | `PH7_PRIVATE sxi32 PH7_CompileThrowExpr(ph7_gen_state *pGen, sxi32 iCompileFlag)` |
|        4 | 3177 | `{` |
|       44 | 3178 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 3179 | `	GenBlock *pBlock;` |
|        - | 3180 | `	sxu32 nIdx;` |
|        - | 3181 | `	sxi32 rc;` |
|       20 | 3182 | `	(void)iCompileFlag;` |
|       44 | 3183 | `	pGen->pIn++; /* Skip 'throw' */` |
|       44 | 3184 | `	if( pGen->pIn >= pGen->pEnd ){` |
|      ! 0 | 3185 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 3186 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 3187 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3188 | `			return SXERR_ABORT;` |
|        - | 3189 | `		}` |
|      ! 0 | 3190 | `		return SXRET_OK;` |
|        - | 3191 | `	}` |
|       44 | 3192 | `	rc = PH7_CompileExpr(&(*pGen),0,GenStateThrowNodeValidator);` |
|       44 | 3193 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 3194 | `		return SXERR_ABORT;` |
|        - | 3195 | `	}` |
|       44 | 3196 | `	if( rc == SXERR_EMPTY ){` |
|      ! 0 | 3197 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 3198 | `			"throw: Expecting an exception class instance");` |
|      ! 0 | 3199 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3200 | `			return SXERR_ABORT;` |
|        - | 3201 | `		}` |
|      ! 0 | 3202 | `		return SXRET_OK;` |
|        - | 3203 | `	}` |
|        - | 3204 | `	/* Walk up to nearest exception/function block for the jump target */` |
|       44 | 3205 | `	pBlock = pGen->pCurrent;` |
|       68 | 3206 | `	while( pBlock->pParent ){` |
|       57 | 3207 | `		if( pBlock->iFlags & (GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FUNC) ){` |
|       33 | 3208 | `			break;` |
|        - | 3209 | `		}` |
|       26 | 3210 | `		pBlock = pBlock->pParent;` |
|        2 | 3211 | `	}` |
|       44 | 3212 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_THROW,0,0,0,&nIdx);` |
|       44 | 3213 | `	GenStateNewJumpFixup(pBlock,PH7_OP_THROW,nIdx);` |
|       44 | 3214 | `	return SXRET_OK;` |
|       24 | 3215 | `}` |
|        - | 3216 | `/*` |
|        - | 3217 | `` * ROOT C: parse a single `catch (A \| B $e)` header (no body) into an`` |
|        - | 3218 | ` * ph7_exception_block. On success pGen->pIn is positioned at the catch body's` |
|        - | 3219 | ` * opening '{'. Mirrors the header parsing in PH7_CompileCatch but leaves body` |
|        - | 3220 | ` * compilation to the caller (which emits it inline). Returns SXRET_OK, or a` |
|        - | 3221 | ` * compile error propagated from the parser.` |
|        - | 3222 | ` */` |
|       66 | 3223 | `static sxi32 GenStateParseCatchHeader(ph7_gen_state *pGen, ph7_exception_block *pCatch)` |
|        5 | 3224 | `{` |
|        - | 3225 | `	SyString sClassName;` |
|        - | 3226 | `	SyToken *pToken;` |
|        - | 3227 | `	SyString *pName;` |
|        - | 3228 | `	char *zDup;` |
|        - | 3229 | `	sxi32 rc;` |
|       71 | 3230 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|       71 | 3231 | `	SyZero(pCatch,sizeof(ph7_exception_block));` |
|       71 | 3232 | `	SySetInit(&pCatch->aClasses,&pGen->pVm->sAllocator,sizeof(SyString));` |
|        - | 3233 | `	/* Inline catches compile into the function's own container; pByteCode stays NULL. */` |
|       71 | 3234 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|      ! 0 | 3235 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 3236 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 3237 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 3238 | `		return SXERR_INVALID;` |
|        - | 3239 | `	}` |
|       71 | 3240 | `	pGen->pIn++; /* '(' */` |
|       33 | 3241 | `	for(;;){` |
|        - | 3242 | `		SyBlob sResolved;` |
|       71 | 3243 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|       71 | 3244 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|      ! 0 | 3245 | `			SyBlobRelease(&sResolved);` |
|      ! 0 | 3246 | `			pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 3247 | `			PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 3248 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 3249 | `			return SXERR_INVALID;` |
|        - | 3250 | `		}` |
|      104 | 3251 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       66 | 3252 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|       71 | 3253 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|       71 | 3254 | `		SyBlobRelease(&sResolved);` |
|       71 | 3255 | `		if( zDup == 0 ){ return SXERR_ABORT; }` |
|       71 | 3256 | `		rc = SySetPut(&pCatch->aClasses,(const void *)&sClassName);` |
|       71 | 3257 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       66 | 3258 | `		if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_OP) &&` |
|        5 | 3259 | `			pGen->pIn->sData.nByte == 1 && pGen->pIn->sData.zString[0] == '\|' ){` |
|      ! 0 | 3260 | `			pGen->pIn++; continue;` |
|        - | 3261 | `		}` |
|       71 | 3262 | `		break;` |
|      ! 0 | 3263 | `	}` |
|        - | 3264 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|        - | 3265 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding. */` |
|       71 | 3266 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|        3 | 3267 | `		pGen->pIn++; /* ')' */` |
|        3 | 3268 | `		return SXRET_OK;` |
|        - | 3269 | `	}` |
|       64 | 3270 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 \|\|` |
|       69 | 3271 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|      ! 0 | 3272 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 3273 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 3274 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 3275 | `		return SXERR_INVALID;` |
|        - | 3276 | `	}` |
|       69 | 3277 | `	pGen->pIn++; /* '$' */` |
|       69 | 3278 | `	pName = &pGen->pIn->sData;` |
|       69 | 3279 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|       69 | 3280 | `	if( zDup == 0 ){ return SXERR_ABORT; }` |
|       69 | 3281 | `	SyStringInitFromBuf(&pCatch->sThis,zDup,pName->nByte);` |
|       69 | 3282 | `	pGen->pIn++;` |
|       69 | 3283 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 | 3284 | `		pToken = pGen->pIn; if( pToken >= pGen->pEnd ){ pToken--; }` |
|      ! 0 | 3285 | `		PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 3286 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 3287 | `		return SXERR_INVALID;` |
|        - | 3288 | `	}` |
|       69 | 3289 | `	pGen->pIn++; /* ')' */` |
|       69 | 3290 | `	return SXRET_OK;` |
|       38 | 3291 | `}` |
|        - | 3292 | `/*` |
|        - | 3293 | ` * ROOT C: compile try/catch/finally INLINE into the current (function) bytecode` |
|        - | 3294 | `` * container. Used only for generator bodies so a `yield` inside a catch/finally`` |
|        - | 3295 | ` * suspends correctly (the legacy path runs them via a detached VmLocalExec whose` |
|        - | 3296 | ` * pc/stack a generator resume cannot restore). Layout (see the block comment on` |
|        - | 3297 | ` * VmThrowException):` |
|        - | 3298 | ` *` |
|        - | 3299 | ` *    LOAD_EXCEPTION p3=pExc            ; push handler + transparent frame` |
|        - | 3300 | ` *    <try body>` |
|        - | 3301 | ` *    POP_EXCEPTION  p3=pExc            ; normal completion (seeds finally or pops)` |
|        - | 3302 | ` *    JMP  -> finally\|end` |
|        - | 3303 | ` *  Lh: CATCH p3=pExc iP1=k             ; throw lands here, binds $e` |
|        - | 3304 | ` *    <catch body>` |
|        - | 3305 | ` *    JMP  -> finally\|end` |
|        - | 3306 | ` *    ... more catches ...` |
|        - | 3307 | ` *  Lfin: <finally body>` |
|        - | 3308 | ` *    END_FINALLY p3=pExc               ; dispatch pending action` |
|        - | 3309 | ` *  Lend:` |
|        - | 3310 | ` */` |
|      122 | 3311 | `static sxi32 PH7_CompileTryInline(ph7_gen_state *pGen, ph7_exception *pException)` |
|        5 | 3312 | `{` |
|      127 | 3313 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 3314 | `	GenBlock *pTry;` |
|        - | 3315 | `	VmInstr *pInstr;` |
|      127 | 3316 | `	sxu32 idxLoad = 0, idxNormalJmp = 0, iLpop;` |
|        - | 3317 | `	SySet aCatchJmp;         /* instruction indices of each catch-end JMP, to fix later */` |
|        - | 3318 | `	sxi32 rc;` |
|      127 | 3319 | `	SySetInit(&aCatchJmp,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|        - | 3320 | `	/* Try block (pUserData=pException so break/continue emit POP_EXCEPTION; passed at` |
|        - | 3321 | `	 * ENTRY so GenStateEnterBlock can classify the scope with it) */` |
|      188 | 3322 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),` |
|       61 | 3323 | `		pException,&pTry);` |
|      127 | 3324 | `	if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|      127 | 3325 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&idxLoad);` |
|      127 | 3326 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|      127 | 3327 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|      127 | 3328 | `	if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|      127 | 3329 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|      127 | 3330 | `	iLpop = PH7_VmInstrLength(pGen->pVm);` |
|        - | 3331 | `	/* LOAD_EXCEPTION landing pad = post-try-body (drives inject-drain + break-pop) */` |
|      127 | 3332 | `	pInstr = PH7_VmGetInstr(pGen->pVm,idxLoad);` |
|      127 | 3333 | `	if( pInstr ){ pInstr->iP2 = iLpop; }` |
|      127 | 3334 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|      127 | 3335 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 3336 | `	/* Normal-completion jump -> finally or end (target fixed after layout) */` |
|      127 | 3337 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxNormalJmp);` |
|        - | 3338 | `	/* Catch clauses (inline) */` |
|      127 | 3339 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      122 | 3340 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|       71 | 3341 | `		sxu32 k = 0;` |
|       99 | 3342 | `		for(;;){` |
|        - | 3343 | `			ph7_exception_block sCatch;` |
|        - | 3344 | `			GenBlock *pCatchBlk;` |
|      137 | 3345 | `			sxu32 idxJmp = 0;` |
|      132 | 3346 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|      127 | 3347 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|       38 | 3348 | `				break;` |
|        - | 3349 | `			}` |
|       71 | 3350 | `			rc = GenStateParseCatchHeader(&(*pGen),&sCatch);` |
|       71 | 3351 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       71 | 3352 | `			if( rc != SXRET_OK ){ return SXERR_INVALID; }` |
|       71 | 3353 | `			sCatch.iHandlerPc = PH7_VmInstrLength(pGen->pVm);` |
|       71 | 3354 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_CATCH,(sxi32)k,0,pException,0);` |
|        - | 3355 | `			/* Tag the catch block with its try so a break/continue leaving the catch counts` |
|        - | 3356 | `			 * this try's finally (VmThrowInline keeps the handler on aException as iInCatch` |
|        - | 3357 | `			 * during the catch, so VmFinallyAdvance can run the finally then take the jump).` |
|        - | 3358 | `			 * Passed at ENTRY: GenStateEnterBlock reads it to classify the block's scope. */` |
|      104 | 3359 | `			rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),` |
|       33 | 3360 | `				pException,&pCatchBlk);` |
|       71 | 3361 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       71 | 3362 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|       71 | 3363 | `			if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       71 | 3364 | `			GenStateFixJumps(pCatchBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|       71 | 3365 | `			GenStateLeaveBlock(&(*pGen),0);` |
|        - | 3366 | `			/* Pop the handler VmThrowInline re-pushed for this catch (iInCatch) — with a` |
|        - | 3367 | `			 * finally it seeds FALLTHROUGH and keeps the frame; otherwise it tears down. */` |
|       71 | 3368 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|       71 | 3369 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&idxJmp);` |
|       71 | 3370 | `			SySetPut(&aCatchJmp,(const void *)&idxJmp);` |
|       71 | 3371 | `			rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|       71 | 3372 | `			if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       71 | 3373 | `			k++;` |
|        5 | 3374 | `		}` |
|       33 | 3375 | `	}` |
|        - | 3376 | `	/* Finally (inline) */` |
|      127 | 3377 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|      102 | 3378 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|        - | 3379 | `		GenBlock *pFinBlk;` |
|       65 | 3380 | `		pGen->pIn++; /* Jump 'finally' */` |
|       65 | 3381 | `		pException->iFinallyPc = PH7_VmInstrLength(pGen->pVm);` |
|       95 | 3382 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION\|GEN_BLOCK_FINALLY,` |
|       30 | 3383 | `			PH7_VmInstrLength(pGen->pVm),0,&pFinBlk);` |
|       65 | 3384 | `		if( rc != SXRET_OK ){ return SXERR_ABORT; }` |
|       65 | 3385 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|       65 | 3386 | `		if( rc == SXERR_ABORT ){ return SXERR_ABORT; }` |
|       65 | 3387 | `		GenStateFixJumps(pFinBlk,-1,PH7_VmInstrLength(pGen->pVm));` |
|       65 | 3388 | `		GenStateLeaveBlock(&(*pGen),0);` |
|       65 | 3389 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_END_FINALLY,0,0,pException,0);` |
|       65 | 3390 | `		pException->iHasFinally = 1;` |
|       30 | 3391 | `	}` |
|      127 | 3392 | `	pException->iEndCatchPc = PH7_VmInstrLength(pGen->pVm);` |
|      127 | 3393 | `	pException->iInlined = 1;` |
|        - | 3394 | `	/* Fix the normal-completion + catch-end jumps to finally (if any) else end */` |
|        - | 3395 | `	{` |
|      127 | 3396 | `		sxu32 iTarget = pException->iHasFinally ? pException->iFinallyPc : pException->iEndCatchPc;` |
|        - | 3397 | `		sxu32 *aJ; sxu32 n;` |
|      127 | 3398 | `		pInstr = PH7_VmGetInstr(pGen->pVm,idxNormalJmp);` |
|      127 | 3399 | `		if( pInstr ){ pInstr->iP2 = iTarget; }` |
|      127 | 3400 | `		aJ = (sxu32 *)SySetBasePtr(&aCatchJmp);` |
|      193 | 3401 | `		for( n = 0; n < SySetUsed(&aCatchJmp); ++n ){` |
|       71 | 3402 | `			pInstr = PH7_VmGetInstr(pGen->pVm,aJ[n]);` |
|       71 | 3403 | `			if( pInstr ){ pInstr->iP2 = iTarget; }` |
|       38 | 3404 | `		}` |
|        - | 3405 | `	}` |
|      127 | 3406 | `	SySetRelease(&aCatchJmp);` |
|      127 | 3407 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|      ! 0 | 3408 | `		PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Cannot use try without catch or finally");` |
|      ! 0 | 3409 | `	}` |
|      127 | 3410 | `	return SXRET_OK;` |
|       66 | 3411 | `}` |
|        - | 3412 | `/*` |
|        - | 3413 | ` * Compile a 'catch' block.` |
|        - | 3414 | ` * Catch: A "catch" block retrieves an exception and creates` |
|        - | 3415 | ` * an object containing the exception information.` |
|        - | 3416 | ` */` |
|    29694 | 3417 | `static sxi32 PH7_CompileCatch(ph7_gen_state *pGen,ph7_exception *pException)` |
|        5 | 3418 | `{` |
|    29699 | 3419 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 3420 | `	ph7_exception_block sCatch;` |
|        - | 3421 | `	SySet *pInstrContainer;` |
|        - | 3422 | `	SyString sClassName;` |
|        - | 3423 | `	GenBlock *pCatch;` |
|        - | 3424 | `	SyToken *pToken;` |
|        - | 3425 | `	SyString *pName;` |
|        - | 3426 | `	char *zDup;` |
|        - | 3427 | `	sxi32 rc;` |
|    29699 | 3428 | `	pGen->pIn++; /* Jump the 'catch' keyword */` |
|        - | 3429 | `	/* Zero the structure */` |
|    29699 | 3430 | `	SyZero(&sCatch,sizeof(ph7_exception_block));` |
|        - | 3431 | `	/* Initialize fields */` |
|    29699 | 3432 | `	SySetInit(&sCatch.aClasses,&pException->pVm->sAllocator,sizeof(SyString));` |
|        - | 3433 | `	/* The catch body gets its own bytecode array, allocated (not embedded) so its address` |
|        - | 3434 | `	 * survives both this stack frame and any later growth of pException->sEntry — a` |
|        - | 3435 | `	 * break/continue inside the body records it in its JumpFixup (see JumpFixup). */` |
|    29699 | 3436 | `	sCatch.pByteCode = (SySet *)SyMemBackendAlloc(&pException->pVm->sAllocator,sizeof(SySet));` |
|    29699 | 3437 | `	if( sCatch.pByteCode == 0 ){` |
|      ! 0 | 3438 | `		goto Mem;` |
|        - | 3439 | `	}` |
|    29699 | 3440 | `	SySetInit(sCatch.pByteCode,&pException->pVm->sAllocator,sizeof(VmInstr));` |
|    29699 | 3441 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 /*(*/ ){` |
|        - | 3442 | `			/* Unexpected token,break immediately */` |
|      ! 0 | 3443 | `			pToken = pGen->pIn;` |
|      ! 0 | 3444 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 3445 | `				pToken--;` |
|      ! 0 | 3446 | `			}` |
|      ! 0 | 3447 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 3448 | `				"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 3449 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 3450 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3451 | `				return SXERR_ABORT;` |
|        - | 3452 | `			}` |
|      ! 0 | 3453 | `			return SXERR_INVALID;` |
|        - | 3454 | `	}` |
|        - | 3455 | `	/* Extract the exception class(es) — supports multi-catch: catch (A \| B $e) */` |
|    29699 | 3456 | `	pGen->pIn++; /* Jump the left parenthesis '(' */` |
|    14864 | 3457 | `	for(;;){` |
|        - | 3458 | `		SyBlob sResolved;` |
|    29733 | 3459 | `		SyBlobInit(&sResolved,&pGen->pVm->sAllocator);` |
|    29733 | 3460 | `		if( GenStateParseClassReference(pGen,&sResolved) != SXRET_OK ){` |
|        6 | 3461 | `			SyBlobRelease(&sResolved);` |
|        6 | 3462 | `			pToken = pGen->pIn;` |
|        6 | 3463 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 3464 | `				pToken--;` |
|      ! 0 | 3465 | `			}` |
|        8 | 3466 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 3467 | `				"syntax error, unexpected %s \"%z\"",` |
|        2 | 3468 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|        6 | 3469 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3470 | `				return SXERR_ABORT;` |
|        - | 3471 | `			}` |
|        6 | 3472 | `			return SXERR_INVALID;` |
|        - | 3473 | `		}` |
|        - | 3474 | `		/* Persist the FQN beyond this function — aClasses outlives the` |
|        - | 3475 | `		 * transient SyBlob allocation. */` |
|    44591 | 3476 | `		zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|    29724 | 3477 | `			(const char *)SyBlobData(&sResolved),SyBlobLength(&sResolved));` |
|    29729 | 3478 | `		SyStringInitFromBuf(&sClassName,zDup,SyBlobLength(&sResolved));` |
|    29729 | 3479 | `		SyBlobRelease(&sResolved);` |
|    29729 | 3480 | `		if( zDup == 0 ){` |
|      ! 0 | 3481 | `			goto Mem;` |
|        - | 3482 | `		}` |
|    29729 | 3483 | `		rc = SySetPut(&sCatch.aClasses,(const void *)&sClassName);` |
|    29729 | 3484 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 3485 | `			goto Mem;` |
|        - | 3486 | `		}` |
|        - | 3487 | `		/* Check for '\|' (multi-catch separator) */` |
|    29724 | 3488 | `		if( pGen->pIn < pGen->pEnd &&` |
|    29724 | 3489 | `			(pGen->pIn->nType & PH7_TK_OP) &&` |
|       39 | 3490 | `			pGen->pIn->sData.nByte == 1 &&` |
|       34 | 3491 | `			pGen->pIn->sData.zString[0] == '\|' ){` |
|       38 | 3492 | `			pGen->pIn++; /* Consume the '\|' */` |
|       38 | 3493 | `			continue;` |
|        - | 3494 | `		}` |
|    29695 | 3495 | `		break;` |
|      ! 0 | 3496 | `	}` |
|        - | 3497 | ``	/* PHP 8.0 non-capturing catch: `catch (Type)` / `catch (A\|B)` with no`` |
|        - | 3498 | `	 * variable. sThis stays empty (SyZero'd above) so the runtime skips binding;` |
|        - | 3499 | `	 * jump straight to compiling the block below. */` |
|    29695 | 3500 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_RPAREN) /*)*/ ){` |
|        7 | 3501 | `		goto CatchBody;` |
|        - | 3502 | `	}` |
|    29684 | 3503 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_DOLLAR) == 0 /*$*/ \|\|` |
|    29689 | 3504 | `		&pGen->pIn[1] >= pGen->pEnd \|\| (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|        - | 3505 | `			/* Unexpected token,break immediately */` |
|      ! 0 | 3506 | `			pToken = pGen->pIn;` |
|      ! 0 | 3507 | `			if( pToken >= pGen->pEnd ){` |
|      ! 0 | 3508 | `				pToken--;` |
|      ! 0 | 3509 | `			}` |
|      ! 0 | 3510 | `			rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 3511 | `				"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 3512 | `				TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 3513 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3514 | `				return SXERR_ABORT;` |
|        - | 3515 | `			}` |
|      ! 0 | 3516 | `			return SXERR_INVALID;` |
|        - | 3517 | `	}` |
|    29689 | 3518 | `	pGen->pIn++; /* Jump the dollar sign */` |
|        - | 3519 | `	/* Duplicate instance name */` |
|    29689 | 3520 | `	pName = &pGen->pIn->sData;` |
|    29689 | 3521 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,pName->zString,pName->nByte);` |
|    29689 | 3522 | `	if( zDup == 0 ){` |
|      ! 0 | 3523 | `		goto Mem;` |
|        - | 3524 | `	}` |
|    29689 | 3525 | `	SyStringInitFromBuf(&sCatch.sThis,zDup,pName->nByte);` |
|    29689 | 3526 | `	pGen->pIn++;` |
|    14845 | 3527 | `CatchBody:` |
|    29695 | 3528 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 /*)*/ ){` |
|        - | 3529 | `		/* Unexpected token,break immediately */` |
|      ! 0 | 3530 | `		pToken = pGen->pIn;` |
|      ! 0 | 3531 | `		if( pToken >= pGen->pEnd ){` |
|      ! 0 | 3532 | `			pToken--;` |
|      ! 0 | 3533 | `		}` |
|      ! 0 | 3534 | `		rc = PH7_GenCompileError(pGen,E_PARSE,pToken->nLine,` |
|        - | 3535 | `			"syntax error, unexpected %s \"%z\"",` |
|      ! 0 | 3536 | `			TokenTypeName(pToken->nType),&pToken->sData);` |
|      ! 0 | 3537 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3538 | `			return SXERR_ABORT;` |
|        - | 3539 | `		}` |
|      ! 0 | 3540 | `		return SXERR_INVALID;` |
|        - | 3541 | `	}` |
|        - | 3542 | `	/* Compile the block */` |
|    29695 | 3543 | `	pGen->pIn++; /* Jump the right parenthesis */` |
|        - | 3544 | `	/* Create the catch block. GEN_BLOCK_DETACHED: the body below compiles into` |
|        - | 3545 | `	 * sCatch.pByteCode, not into the enclosing function's array. */` |
|    29695 | 3546 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION\|GEN_BLOCK_DETACHED,PH7_VmInstrLength(pGen->pVm),0,&pCatch);` |
|    29695 | 3547 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3548 | `		return SXERR_ABORT;` |
|        - | 3549 | `	}` |
|        - | 3550 | `	/* Swap bytecode container */` |
|    29695 | 3551 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|    29695 | 3552 | `	PH7_VmSetByteCodeContainer(pGen->pVm,sCatch.pByteCode);` |
|        - | 3553 | `	/* Compile the block */` |
|    29695 | 3554 | `	PH7_CompileBlock(&(*pGen),0);` |
|        - | 3555 | `	/* Fix forward jumps now the destination is resolved  */` |
|    29695 | 3556 | `	GenStateFixJumps(pCatch,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 3557 | `	/* Emit the DONE instruction */` |
|    29695 | 3558 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - | 3559 | `	/* Leave the block */` |
|    29695 | 3560 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 3561 | `	/* Restore the default container */` |
|    29695 | 3562 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - | 3563 | `	/* Install the catch block */` |
|    29695 | 3564 | `	rc = SySetPut(&pException->sEntry,(const void *)&sCatch);` |
|    29695 | 3565 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3566 | `		goto Mem;` |
|        - | 3567 | `	}` |
|    29695 | 3568 | `	return SXRET_OK;` |
|      ! 0 | 3569 | `Mem:` |
|      ! 0 | 3570 | `	PH7_GenCompileError(&(*pGen),E_ERROR,nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 3571 | `	return SXERR_ABORT;` |
|    14852 | 3572 | `}` |
|        - | 3573 | `/*` |
|        - | 3574 | ` * Compile a 'try' block.` |
|        - | 3575 | ` * A function using an exception should be in a "try" block.` |
|        - | 3576 | ` * If the exception does not trigger, the code will continue` |
|        - | 3577 | ` * as normal. However if the exception triggers, an exception` |
|        - | 3578 | ` * is "thrown".` |
|        - | 3579 | ` */` |
|    29956 | 3580 | `PH7_PRIVATE sxi32 PH7_CompileTry(ph7_gen_state *pGen)` |
|        5 | 3581 | `{` |
|        - | 3582 | `	ph7_exception *pException;` |
|    29961 | 3583 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        - | 3584 | `	GenBlock *pTry;` |
|        - | 3585 | `	sxu32 nJmpIdx;` |
|        - | 3586 | `	sxi32 rc;` |
|        - | 3587 | `	/* Create the exception container */` |
|    29961 | 3588 | `	pException = (ph7_exception *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_exception));` |
|    29961 | 3589 | `	if( pException == 0 ){` |
|      ! 0 | 3590 | `		PH7_GenCompileError(&(*pGen),E_ERROR,` |
|      ! 0 | 3591 | `			pGen->pIn->nLine,"Fatal, PH7 engine is running out of memory");` |
|      ! 0 | 3592 | `		return SXERR_ABORT;` |
|        - | 3593 | `	}` |
|        - | 3594 | `	/* Zero the structure */` |
|    29961 | 3595 | `	SyZero(pException,sizeof(ph7_exception));` |
|        - | 3596 | `	/* Initialize fields */` |
|    29961 | 3597 | `	SySetInit(&pException->sEntry,&pGen->pVm->sAllocator,sizeof(ph7_exception_block));` |
|    29961 | 3598 | `	SySetInit(&pException->sFinally,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|    29961 | 3599 | `	pException->iHasFinally = 0;` |
|    29961 | 3600 | `	pException->iFinallyDone = 0;` |
|    29961 | 3601 | `	pException->pVm = pGen->pVm;` |
|        - | 3602 | `	/* ROOT C: inside a generator body, compile the whole try/catch/finally inline so a` |
|        - | 3603 | ``	 * `yield` in a catch/finally suspends correctly. Non-generators keep the DETACHED path`` |
|        - | 3604 | `	 * below — deliberately, not pending migration: it is the proven one, and inlining was` |
|        - | 3605 | ``	 * scoped to generators so no other code path changed. `bInlineTryCatch` is 1 since the`` |
|        - | 3606 | ``	 * inline VM handlers landed, so `bInGenerator` is what actually selects here. */`` |
|    29961 | 3607 | `	if( pGen->bInGenerator && pGen->pVm->bInlineTryCatch ){` |
|      127 | 3608 | `		return PH7_CompileTryInline(&(*pGen),pException);` |
|        - | 3609 | `	}` |
|        - | 3610 | `	/* Create the try block */` |
|        - | 3611 | `	/* pUserData is the exception context, passed at ENTRY (not assigned after) because` |
|        - | 3612 | `	 * GenStateEnterBlock reads it to classify the block's try/catch scope — see aScope.` |
|        - | 3613 | `	 * It is also what a break/continue crossing this try emits its POP_EXCEPTION with. */` |
|    44756 | 3614 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION,PH7_VmInstrLength(pGen->pVm),` |
|    14917 | 3615 | `		pException,&pTry);` |
|    29839 | 3616 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3617 | `		return SXERR_ABORT;` |
|        - | 3618 | `	}` |
|        - | 3619 | `	/* Emit the 'LOAD_EXCEPTION' instruction */` |
|    29839 | 3620 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD_EXCEPTION,0,0,pException,&nJmpIdx);` |
|        - | 3621 | `	/* Fix the jump later when the destination is resolved */` |
|    29839 | 3622 | `	GenStateNewJumpFixup(pTry,PH7_OP_LOAD_EXCEPTION,nJmpIdx);` |
|    29839 | 3623 | `	pGen->pIn++; /* Jump the 'try' keyword */` |
|        - | 3624 | `	/* Compile the block */` |
|    29839 | 3625 | `	rc = PH7_CompileBlock(&(*pGen),0);` |
|    29839 | 3626 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 3627 | `		return SXERR_ABORT;` |
|        - | 3628 | `	}` |
|        - | 3629 | `	/* Fix forward jumps now the destination is resolved */` |
|    29839 | 3630 | `	GenStateFixJumps(pTry,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 3631 | `	/* Emit the 'POP_EXCEPTION' instruction */` |
|    29839 | 3632 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pException,0);` |
|        - | 3633 | `	/* Leave the block */` |
|    29839 | 3634 | `	GenStateLeaveBlock(&(*pGen),0);` |
|        - | 3635 | `	/* Compile catch block(s) — at least one catch or finally is required */` |
|    29839 | 3636 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|    29832 | 3637 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_CATCH ){` |
|        - | 3638 | `		/* Compile one or more catch blocks */` |
|    29689 | 3639 | `		for(;;){` |
|    59378 | 3640 | `			if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_KEYWORD) == 0` |
|    44882 | 3641 | `				\|\| SX_PTR_TO_INT(pGen->pIn->pUserData) != PH7_TKWRD_CATCH ){` |
|    14847 | 3642 | `					break;` |
|        - | 3643 | `			}` |
|    29699 | 3644 | `			rc = PH7_CompileCatch(&(*pGen),pException);` |
|    29699 | 3645 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3646 | `				return SXERR_ABORT;` |
|        - | 3647 | `			}` |
|        5 | 3648 | `		}` |
|    14842 | 3649 | `	}` |
|        - | 3650 | `	/* Compile optional finally block */` |
|    29839 | 3651 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_KEYWORD) &&` |
|     1448 | 3652 | `		SX_PTR_TO_INT(pGen->pIn->pUserData) == PH7_TKWRD_FINALLY ){` |
|        - | 3653 | `		SySet *pInstrContainer;` |
|        - | 3654 | `		GenBlock *pFinBlock;` |
|      235 | 3655 | `		pGen->pIn++; /* Jump the 'finally' keyword */` |
|        - | 3656 | `		/* Create the finally block for jump fixup bookkeeping (detached: the body` |
|        - | 3657 | `		 * compiles into pException->sFinally, see GEN_BLOCK_DETACHED). */` |
|      350 | 3658 | `		rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_EXCEPTION\|GEN_BLOCK_DETACHED\|GEN_BLOCK_FINALLY,` |
|      115 | 3659 | `			PH7_VmInstrLength(pGen->pVm),0,&pFinBlock);` |
|      235 | 3660 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 3661 | `			return SXERR_ABORT;` |
|        - | 3662 | `		}` |
|        - | 3663 | `		/* Swap bytecode container */` |
|      235 | 3664 | `		pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      235 | 3665 | `		PH7_VmSetByteCodeContainer(pGen->pVm,&pException->sFinally);` |
|        - | 3666 | `		/* Compile the finally body */` |
|      235 | 3667 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|      235 | 3668 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3669 | `			PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      ! 0 | 3670 | `			return SXERR_ABORT;` |
|        - | 3671 | `		}` |
|        - | 3672 | `		/* Fix forward jumps now the destination is resolved */` |
|      235 | 3673 | `		GenStateFixJumps(pFinBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 3674 | `		/* Emit DONE to terminate the finally block */` |
|      235 | 3675 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,0,0,0,0);` |
|        - | 3676 | `		/* Leave the block */` |
|      235 | 3677 | `		GenStateLeaveBlock(&(*pGen),0);` |
|        - | 3678 | `		/* Restore the default container */` |
|      235 | 3679 | `		PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      235 | 3680 | `		pException->iHasFinally = 1;` |
|      115 | 3681 | `	}` |
|        - | 3682 | `	/* Must have at least one catch or finally */` |
|    29839 | 3683 | `	if( SySetUsed(&pException->sEntry) == 0 && !pException->iHasFinally ){` |
|        9 | 3684 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|        - | 3685 | `			"Cannot use try without catch or finally");` |
|        9 | 3686 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3687 | `			return SXERR_ABORT;` |
|        - | 3688 | `		}` |
|        3 | 3689 | `	}` |
|    29839 | 3690 | `	return SXRET_OK;` |
|    14983 | 3691 | `}` |
|        - | 3692 | `/*` |
|        - | 3693 | ` * Compile a switch block.` |
|        - | 3694 | ` *  (See block-comment below for more information)` |
|        - | 3695 | ` */` |
|   158626 | 3696 | `static sxi32 GenStateCompileSwitchBlock(ph7_gen_state *pGen,sxu32 iTokenDelim,sxu32 *pBlockStart)` |
|        5 | 3697 | `{` |
|   158631 | 3698 | `	sxi32 rc = SXRET_OK;` |
|   158631 | 3699 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*':'*/)) == 0 ){` |
|        - | 3700 | `		/* Unexpected token */` |
|      ! 0 | 3701 | `		rc = PH7_GenSyntaxError(&(*pGen),pGen->pIn,0);` |
|      ! 0 | 3702 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3703 | `			return SXERR_ABORT;` |
|        - | 3704 | `		}` |
|      ! 0 | 3705 | `		pGen->pIn++;` |
|      ! 0 | 3706 | `	}` |
|   158631 | 3707 | `	pGen->pIn++;` |
|        - | 3708 | `	/* First instruction to execute in this block. */` |
|   158631 | 3709 | `	*pBlockStart = PH7_VmInstrLength(pGen->pVm);` |
|        - | 3710 | `	/* Compile the block until we hit a case/default/endswitch keyword` |
|        - | 3711 | `	 * or the '}' token */` |
|   151935 | 3712 | `	for(;;){` |
|   303875 | 3713 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 3714 | `			/* No more input to process */` |
|      ! 0 | 3715 | `			break;` |
|        - | 3716 | `		}` |
|   303875 | 3717 | `		rc = SXRET_OK;` |
|   303875 | 3718 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|    40855 | 3719 | `			if( pGen->pIn->nType & PH7_TK_CCB /*'}' */ ){` |
|    13631 | 3720 | `				if( iTokenDelim != PH7_TK_CCB ){` |
|        - | 3721 | `					/* Unexpected token */` |
|      ! 0 | 3722 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|      ! 0 | 3723 | `						&pGen->pIn->sData);` |
|      ! 0 | 3724 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3725 | `						return SXERR_ABORT;` |
|        - | 3726 | `					}` |
|        - | 3727 | `					/* FALL THROUGH */` |
|      ! 0 | 3728 | `				}` |
|    13631 | 3729 | `				rc = SXERR_EOF;` |
|    13631 | 3730 | `				break;` |
|        - | 3731 | `			}` |
|    13617 | 3732 | `		}else{` |
|        - | 3733 | `			sxi32 nKwrd;` |
|        - | 3734 | `			/* Extract the keyword */` |
|   263025 | 3735 | `			nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   263025 | 3736 | `			if( nKwrd == PH7_TKWRD_CASE \|\| nKwrd == PH7_TKWRD_DEFAULT ){` |
|    72503 | 3737 | `				break;` |
|        - | 3738 | `			}` |
|   118029 | 3739 | `			if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|        5 | 3740 | `				if( iTokenDelim != PH7_TK_KEYWORD ){` |
|        - | 3741 | `					/* Unexpected token */` |
|      ! 0 | 3742 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Unexpected token '%z'",` |
|      ! 0 | 3743 | `						&pGen->pIn->sData);` |
|      ! 0 | 3744 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3745 | `						return SXERR_ABORT;` |
|        - | 3746 | `					}` |
|        - | 3747 | `					/* FALL THROUGH */` |
|      ! 0 | 3748 | `				}` |
|        - | 3749 | `				/* Block compiled */` |
|        5 | 3750 | `				break;` |
|        - | 3751 | `			}` |
|        - | 3752 | `		}` |
|        - | 3753 | `		/* Compile block */` |
|   145249 | 3754 | `		rc = PH7_CompileBlock(&(*pGen),0);` |
|   145249 | 3755 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3756 | `			return SXERR_ABORT;` |
|        - | 3757 | `		}` |
|        5 | 3758 | `	}` |
|   158631 | 3759 | `	return rc;` |
|    79318 | 3760 | `}` |
|        - | 3761 | `/*` |
|        - | 3762 | ` * Compile a case eXpression.` |
|        - | 3763 | ` *  (See block-comment below for more information)` |
|        - | 3764 | ` */` |
|   154064 | 3765 | `static sxi32 GenStateCompileCaseExpr(ph7_gen_state *pGen,ph7_case_expr *pExpr)` |
|        5 | 3766 | `{` |
|        - | 3767 | `	SySet *pInstrContainer;` |
|        - | 3768 | `	SyToken *pEnd,*pTmp;` |
|   154069 | 3769 | `	sxi32 iNest = 0;` |
|        - | 3770 | `	sxi32 rc;` |
|        - | 3771 | `	/* Delimit the expression */` |
|   154069 | 3772 | `	pEnd = pGen->pIn;` |
|   308149 | 3773 | `	while( pEnd < pGen->pEnd ){` |
|   308149 | 3774 | `		if( pEnd->nType & PH7_TK_LPAREN /*(*/ ){` |
|        - | 3775 | `			/* Increment nesting level */` |
|        8 | 3776 | `			iNest++;` |
|   308146 | 3777 | `		}else if( pEnd->nType & PH7_TK_RPAREN /*)*/ ){` |
|        - | 3778 | `			/* Decrement nesting level */` |
|        8 | 3779 | `			iNest--;` |
|   308140 | 3780 | `		}else if( pEnd->nType & (PH7_TK_SEMI/*';'*/\|PH7_TK_COLON/*;'*/) && iNest < 1 ){` |
|   154069 | 3781 | `			break;` |
|        - | 3782 | `		}` |
|   154085 | 3783 | `		pEnd++;` |
|        5 | 3784 | `	}` |
|   154069 | 3785 | `	if( pGen->pIn >= pEnd ){` |
|      ! 0 | 3786 | `		rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,"Empty case expression");` |
|      ! 0 | 3787 | `		if( rc == SXERR_ABORT ){` |
|        - | 3788 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 3789 | `			return SXERR_ABORT;` |
|        - | 3790 | `		}` |
|      ! 0 | 3791 | `	}` |
|        - | 3792 | `	/* Swap token stream */` |
|   154069 | 3793 | `	pTmp = pGen->pEnd;` |
|   154069 | 3794 | `	pGen->pEnd = pEnd;` |
|   154069 | 3795 | `	pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|   154069 | 3796 | `	PH7_VmSetByteCodeContainer(pGen->pVm,&pExpr->aByteCode);` |
|   154069 | 3797 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|        - | 3798 | `	/* Emit the done instruction */` |
|   154069 | 3799 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|   154069 | 3800 | `	PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|        - | 3801 | `	/* Update token stream */` |
|   154069 | 3802 | `	pGen->pIn  = pEnd;` |
|   154069 | 3803 | `	pGen->pEnd = pTmp;` |
|   154069 | 3804 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 3805 | `		return SXERR_ABORT;` |
|        - | 3806 | `	}` |
|   154069 | 3807 | `	return SXRET_OK;` |
|    77037 | 3808 | `}` |
|        - | 3809 | `/*` |
|        - | 3810 | ` * Compile the smart switch statement.` |
|        - | 3811 | ` * According to the PHP language reference manual` |
|        - | 3812 | ` *  The switch statement is similar to a series of IF statements on the same expression.` |
|        - | 3813 | ` *  In many occasions, you may want to compare the same variable (or expression) with many` |
|        - | 3814 | ` *  different values, and execute a different piece of code depending on which value it equals to.` |
|        - | 3815 | ` *  This is exactly what the switch statement is for.` |
|        - | 3816 | ` *  Note: Note that unlike some other languages, the continue statement applies to switch and acts` |
|        - | 3817 | ` *  similar to break. If you have a switch inside a loop and wish to continue to the next iteration` |
|        - | 3818 | ` *  of the outer loop, use continue 2.` |
|        - | 3819 | ` *  Note that switch/case does loose comparision.` |
|        - | 3820 | ` *  It is important to understand how the switch statement is executed in order to avoid mistakes.` |
|        - | 3821 | ` *  The switch statement executes line by line (actually, statement by statement).` |
|        - | 3822 | ` *  In the beginning, no code is executed. Only when a case statement is found with a value that` |
|        - | 3823 | ` *  matches the value of the switch expression does PHP begin to execute the statements.` |
|        - | 3824 | ` *  PHP continues to execute the statements until the end of the switch block, or the first time` |
|        - | 3825 | ` *  it sees a break statement. If you don't write a break statement at the end of a case's statement list.` |
|        - | 3826 | ` *  In a switch statement, the condition is evaluated only once and the result is compared to each` |
|        - | 3827 | ` *  case statement. In an elseif statement, the condition is evaluated again. If your condition` |
|        - | 3828 | ` *  is more complicated than a simple compare and/or is in a tight loop, a switch may be faster.` |
|        - | 3829 | ` *  The statement list for a case can also be empty, which simply passes control into the statement` |
|        - | 3830 | ` *  list for the next case.` |
|        - | 3831 | ` *  The case expression may be any expression that evaluates to a simple type, that is, integer` |
|        - | 3832 | ` *  or floating-point numbers and strings.` |
|        - | 3833 | ` */` |
|    13630 | 3834 | `PH7_PRIVATE sxi32 PH7_CompileSwitch(ph7_gen_state *pGen)` |
|        5 | 3835 | `{` |
|        - | 3836 | `	GenBlock *pSwitchBlock;` |
|        - | 3837 | `	SyToken *pTmp,*pEnd;` |
|        - | 3838 | `	ph7_switch *pSwitch;` |
|        - | 3839 | `	sxu32 nToken;` |
|        - | 3840 | `	sxu32 nLine;` |
|        - | 3841 | `	sxi32 rc;` |
|    13635 | 3842 | `	nLine = pGen->pIn->nLine;` |
|        - | 3843 | `	/* Jump the 'switch' keyword */` |
|    13635 | 3844 | `	pGen->pIn++;` |
|    13635 | 3845 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|        - | 3846 | `		/* Syntax error */` |
|      ! 0 | 3847 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected '(' after 'switch' keyword");` |
|      ! 0 | 3848 | `		if( rc == SXERR_ABORT ){` |
|        - | 3849 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 3850 | `			return SXERR_ABORT;` |
|        - | 3851 | `		}` |
|      ! 0 | 3852 | `		goto Synchronize;` |
|        - | 3853 | `	}` |
|        - | 3854 | `	/* Jump the left parenthesis '(' */` |
|    13635 | 3855 | `	pGen->pIn++;` |
|    13635 | 3856 | `	pEnd = 0; /* cc warning */` |
|        - | 3857 | `	/* Create the loop block */` |
|    20450 | 3858 | `	rc = GenStateEnterBlock(&(*pGen),GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH,` |
|     6815 | 3859 | `		PH7_VmInstrLength(pGen->pVm),0,&pSwitchBlock);` |
|    13635 | 3860 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 3861 | `		return SXERR_ABORT;` |
|        - | 3862 | `	}` |
|        - | 3863 | `	/* Delimit the condition */` |
|    13635 | 3864 | `	PH7_DelimitNestedTokens(pGen->pIn,pGen->pEnd,PH7_TK_LPAREN /* '(' */,PH7_TK_RPAREN /* ')' */,&pEnd);` |
|    13635 | 3865 | `	if( pGen->pIn == pEnd \|\| pEnd >= pGen->pEnd ){` |
|        - | 3866 | `		/* Empty expression */` |
|      ! 0 | 3867 | `		rc = PH7_GenCompileError(pGen,E_ERROR,nLine,"Expected expression after 'switch' keyword");` |
|      ! 0 | 3868 | `		if( rc == SXERR_ABORT ){` |
|        - | 3869 | `			/* Error count limit reached,abort immediately */` |
|      ! 0 | 3870 | `			return SXERR_ABORT;` |
|        - | 3871 | `		}` |
|      ! 0 | 3872 | `	}` |
|        - | 3873 | `	/* Swap token streams */` |
|    13635 | 3874 | `	pTmp = pGen->pEnd;` |
|    13635 | 3875 | `	pGen->pEnd = pEnd;` |
|        - | 3876 | `	/* Compile the expression */` |
|    13635 | 3877 | `	rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    13635 | 3878 | `	if( rc == SXERR_ABORT ){` |
|        - | 3879 | `		/* Expression handler request an operation abort [i.e: Out-of-memory] */` |
|      ! 0 | 3880 | `		return SXERR_ABORT;` |
|        - | 3881 | `	}` |
|        - | 3882 | `	/* Update token stream */` |
|    13635 | 3883 | `	while(pGen->pIn < pEnd ){` |
|      ! 0 | 3884 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|      ! 0 | 3885 | `			"Switch: Unexpected token '%z'",&pGen->pIn->sData);` |
|      ! 0 | 3886 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3887 | `			return SXERR_ABORT;` |
|        - | 3888 | `		}` |
|      ! 0 | 3889 | `		pGen->pIn++;` |
|      ! 0 | 3890 | `	}` |
|    13635 | 3891 | `	pGen->pIn  = &pEnd[1];` |
|    13635 | 3892 | `	pGen->pEnd = pTmp;` |
|    13635 | 3893 | `	if( pGen->pIn >= pGen->pEnd \|\| &pGen->pIn[1] >= pGen->pEnd \|\|` |
|    13630 | 3894 | `		(pGen->pIn->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_COLON/*:*/)) == 0 ){` |
|      ! 0 | 3895 | `			pTmp = pGen->pIn;` |
|      ! 0 | 3896 | `			if( pTmp >= pGen->pEnd ){` |
|      ! 0 | 3897 | `				pTmp--;` |
|      ! 0 | 3898 | `			}` |
|        - | 3899 | `			/* Unexpected token */` |
|      ! 0 | 3900 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pTmp->nLine,"Switch: Unexpected token '%z'",&pTmp->sData);` |
|      ! 0 | 3901 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3902 | `				return SXERR_ABORT;` |
|        - | 3903 | `			}` |
|      ! 0 | 3904 | `			goto Synchronize;` |
|        - | 3905 | `	}` |
|        - | 3906 | `	/* Set the delimiter token */` |
|    13635 | 3907 | `	if( pGen->pIn->nType & PH7_TK_COLON ){` |
|        5 | 3908 | `		nToken = PH7_TK_KEYWORD;` |
|        - | 3909 | `		/* Stop compilation when the 'endswitch;' keyword is seen */` |
|        3 | 3910 | `	}else{` |
|    13631 | 3911 | `		nToken = PH7_TK_CCB; /* '}' */` |
|        - | 3912 | `	}` |
|    13635 | 3913 | `	pGen->pIn++; /* Jump the leading curly braces/colons */` |
|        - | 3914 | `	/* Create the switch blocks container */` |
|    13635 | 3915 | `	pSwitch = (ph7_switch *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(ph7_switch));` |
|    13635 | 3916 | `	if( pSwitch == 0 ){` |
|        - | 3917 | `		/* Abort compilation */` |
|      ! 0 | 3918 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Fatal, PH7 is running out of memory");` |
|      ! 0 | 3919 | `		return SXERR_ABORT;` |
|        - | 3920 | `	}` |
|        - | 3921 | `	/* Zero the structure */` |
|    13635 | 3922 | `	SyZero(pSwitch,sizeof(ph7_switch));` |
|        - | 3923 | `	/* Initialize fields */` |
|    13635 | 3924 | `	SySetInit(&pSwitch->aCaseExpr,&pGen->pVm->sAllocator,sizeof(ph7_case_expr));` |
|        - | 3925 | `	/* Emit the switch instruction */` |
|    13635 | 3926 | `	PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWITCH,0,0,pSwitch,0);` |
|        - | 3927 | `	/* Compile case blocks */` |
|   151815 | 3928 | `	for(;;){` |
|        - | 3929 | `		sxu32 nKwrd;` |
|   158635 | 3930 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 3931 | `			/* No more input to process */` |
|      ! 0 | 3932 | `			break;` |
|        - | 3933 | `		}` |
|   158635 | 3934 | `		if( (pGen->pIn->nType & PH7_TK_KEYWORD) == 0 ){` |
|      ! 0 | 3935 | `			if( nToken != PH7_TK_CCB \|\| (pGen->pIn->nType & PH7_TK_CCB /*}*/) == 0 ){` |
|        - | 3936 | `				/* Unexpected token */` |
|      ! 0 | 3937 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 3938 | `					&pGen->pIn->sData);` |
|      ! 0 | 3939 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3940 | `					return SXERR_ABORT;` |
|        - | 3941 | `				}` |
|        - | 3942 | `				/* FALL THROUGH */` |
|      ! 0 | 3943 | `			}` |
|        - | 3944 | `			/* Block compiled */` |
|      ! 0 | 3945 | `			break;` |
|        - | 3946 | `		}` |
|        - | 3947 | `		/* Extract the keyword */` |
|   158635 | 3948 | `		nKwrd = SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   158635 | 3949 | `		if( nKwrd == PH7_TKWRD_ENDSWITCH /* endswitch; */){` |
|        5 | 3950 | `			if( nToken != PH7_TK_KEYWORD ){` |
|        - | 3951 | `				/* Unexpected token */` |
|      ! 0 | 3952 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 3953 | `					&pGen->pIn->sData);` |
|      ! 0 | 3954 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3955 | `					return SXERR_ABORT;` |
|        - | 3956 | `				}` |
|        - | 3957 | `				/* FALL THROUGH */` |
|      ! 0 | 3958 | `			}` |
|        - | 3959 | `			/* Block compiled */` |
|        5 | 3960 | `			break;` |
|        - | 3961 | `		}` |
|   158631 | 3962 | `		if( nKwrd == PH7_TKWRD_DEFAULT ){` |
|        - | 3963 | `			/*` |
|        - | 3964 | `			 * Accroding to the PHP language reference manual` |
|        - | 3965 | `			 *  A special case is the default case. This case matches anything` |
|        - | 3966 | `			 *  that wasn't matched by the other cases.` |
|        - | 3967 | `			 */` |
|     4567 | 3968 | `			if( pSwitch->nDefault > 0 ){` |
|        - | 3969 | `				/* Default case already compiled */` |
|      ! 0 | 3970 | `				rc = PH7_GenCompileError(&(*pGen),E_WARNING,pGen->pIn->nLine,"Switch: 'default' case already compiled");` |
|      ! 0 | 3971 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3972 | `					return SXERR_ABORT;` |
|        - | 3973 | `				}` |
|      ! 0 | 3974 | `			}` |
|     4567 | 3975 | `			pGen->pIn++; /* Jump the 'default' keyword */` |
|        - | 3976 | `			/* Compile the default block */` |
|     4567 | 3977 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&pSwitch->nDefault);` |
|     4567 | 3978 | `			if( rc == SXERR_ABORT){` |
|      ! 0 | 3979 | `				return SXERR_ABORT;` |
|     4567 | 3980 | `			}else if( rc == SXERR_EOF ){` |
|     4565 | 3981 | `				break;` |
|        1 | 3982 | `			}` |
|   154070 | 3983 | `		}else if( nKwrd == PH7_TKWRD_CASE ){` |
|        - | 3984 | `			ph7_case_expr sCase;` |
|        - | 3985 | `			/* Standard case block */` |
|   154069 | 3986 | `			pGen->pIn++; /* Jump the 'case' keyword */` |
|        - | 3987 | `			/* initialize the structure */` |
|   154069 | 3988 | `			SySetInit(&sCase.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|        - | 3989 | `			/* Compile the case expression */` |
|   154069 | 3990 | `			rc = GenStateCompileCaseExpr(pGen,&sCase);` |
|   154069 | 3991 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3992 | `				return SXERR_ABORT;` |
|        - | 3993 | `			}` |
|        - | 3994 | `			/* Compile the case block */` |
|   154069 | 3995 | `			rc = GenStateCompileSwitchBlock(pGen,nToken,&sCase.nStart);` |
|        - | 3996 | `			/* Insert in the switch container */` |
|   154069 | 3997 | `			SySetPut(&pSwitch->aCaseExpr,(const void *)&sCase);` |
|   154069 | 3998 | `			if( rc == SXERR_ABORT){` |
|      ! 0 | 3999 | `				return SXERR_ABORT;` |
|   154069 | 4000 | `			}else if( rc == SXERR_EOF ){` |
|     9071 | 4001 | `				break;` |
|        - | 4002 | `			}` |
|    72504 | 4003 | `		}else{` |
|        - | 4004 | `			/* Unexpected token */` |
|      ! 0 | 4005 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,"Switch: Unexpected token '%z'",` |
|      ! 0 | 4006 | `				&pGen->pIn->sData);` |
|      ! 0 | 4007 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 4008 | `				return SXERR_ABORT;` |
|        - | 4009 | `			}` |
|      ! 0 | 4010 | `			break;` |
|        - | 4011 | `		}` |
|        5 | 4012 | `	}` |
|        - | 4013 | `	/* Fix all jumps now the destination is resolved */` |
|    13635 | 4014 | `	pSwitch->nOut = PH7_VmInstrLength(pGen->pVm);` |
|    13635 | 4015 | `	GenStateFixJumps(pSwitchBlock,-1,PH7_VmInstrLength(pGen->pVm));` |
|        - | 4016 | `	/* Release the loop block */` |
|    13635 | 4017 | `	GenStateLeaveBlock(pGen,0);` |
|    13635 | 4018 | `	if( pGen->pIn < pGen->pEnd ){` |
|        - | 4019 | `		/* Jump the trailing curly braces or the endswitch keyword*/` |
|    13635 | 4020 | `		pGen->pIn++;` |
|     6815 | 4021 | `	}` |
|        - | 4022 | `	/* Statement successfully compiled */` |
|    13635 | 4023 | `	return SXRET_OK;` |
|      ! 0 | 4024 | `Synchronize:` |
|        - | 4025 | `	/* Synchronize with the first semi-colon */` |
|      ! 0 | 4026 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|      ! 0 | 4027 | `		pGen->pIn++;` |
|      ! 0 | 4028 | `	}` |
|      ! 0 | 4029 | `	return SXRET_OK;` |
|     6820 | 4030 | `}` |
|        - | 4031 |  |
