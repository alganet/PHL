# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2582/2747 lines (93.99%)

[Root index](../../index.md) | [Directory index](index.md)

|      Hits | Line | Source |
| --------: | ---: | :--- |
|         - |    1 | `/**` |
|         - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|         - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|         - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|         - |    5 | ` */` |
|         - |    6 | `#include "ph7int.h"` |
|         - |    7 | `#include "compile_int.h"` |
|         - |    8 | `/*` |
|         - |    9 | ` * This file implement a thread-safe and full-reentrant compiler for the PH7 engine.` |
|         - |   10 | ` * That is, routines defined in this file takes a stream of tokens and output` |
|         - |   11 | ` * PH7 bytecode instructions.` |
|         - |   12 | ` */` |
|         - |   13 | `/* Forward declaration */` |
|         - |   14 | `/*` |
|         - |   15 | ` * Local utility routines used in the code generation phase.` |
|         - |   16 | ` */` |
|         - |   17 | `/*` |
|         - |   18 | ` * Check if the given name refer to a valid label declared in the given function` |
|         - |   19 | ` * (NULL = file scope).` |
|         - |   20 | ` * Return SXRET_OK and write a pointer to that label on success.` |
|         - |   21 | ` * Any other return value indicates no such label.` |
|         - |   22 | ` *` |
|         - |   23 | ` * Labels are scoped PER FUNCTION in php, so the owning function is part of the key:` |
|         - |   24 | ` * the same name may be declared in as many functions as one likes, and each goto sees` |
|         - |   25 | ` * only its own. Matching on the name alone made the first declaration win everywhere,` |
|         - |   26 | `` * which rejected `function a(){ done: } function b(){ goto done; done: }` — ordinary`` |
|         - |   27 | ` * php — as a jump to an undefined label.` |
|         - |   28 | ` *` |
|         - |   29 | ` * Also serves PH7_CompileLabel, which asks the same question at DECLARATION time to reject` |
|         - |   30 | ` * a name its function already declared.` |
|         - |   31 | ` */` |
|       446 |   32 | `PH7_PRIVATE sxi32 GenStateGetLabel(ph7_gen_state *pGen,SyString *pName,ph7_vm_func *pFunc,Label **ppOut)` |
|         5 |   33 | `{` |
|         - |   34 | `	Label *aLabel;` |
|         - |   35 | `	sxu32 n;` |
|         - |   36 | `	/* Perform a linear scan on the label table */` |
|       451 |   37 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|      1615 |   38 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|      1333 |   39 | `		if( aLabel[n].pFunc == pFunc && SyStringCmp(&aLabel[n].sName,pName,SyMemcmp) == 0 ){` |
|         - |   40 | `			/* Jump destination found */` |
|       169 |   41 | `			if( ppOut ){` |
|       165 |   42 | `				*ppOut = &aLabel[n];` |
|        80 |   43 | `			}` |
|       169 |   44 | `			return SXRET_OK;` |
|         - |   45 | `		}` |
|       587 |   46 | `	}` |
|         - |   47 | `	/* No such destination */` |
|       287 |   48 | `	return SXERR_NOTFOUND;` |
|       228 |   49 | `}` |
|         - |   50 | `/*` |
|         - |   51 | ` * Fetch a block that correspond to the given criteria from the stack of` |
|         - |   52 | ` * compiled blocks.` |
|         - |   53 | ` * Return a pointer to that block on success. NULL otherwise.` |
|         - |   54 | ` */` |
|         - |   55 | `/*` |
|         - |   56 | ` * Is the declaration the generator is standing on CONDITIONAL -- php's "not early` |
|         - |   57 | `` * bound"? A `function` or `class` written at the top level of a unit is bound when`` |
|         - |   58 | `` * the unit compiles, and one written anywhere else -- inside an `if`, a loop, a`` |
|         - |   59 | `` * `try`, another function's body -- is bound when execution REACHES it, and not`` |
|         - |   60 | ` * before.` |
|         - |   61 | ` *` |
|         - |   62 | `` * The difference is not academic: `if (!function_exists('mb_convert_encoding')) {`` |
|         - |   63 | `` * function mb_convert_encoding(...) {...} }` is how every symfony/polyfill-* package`` |
|         - |   64 | ` * is written, and binding that body unconditionally REPLACED the engine's own` |
|         - |   65 | ` * builtin with the polyfill -- in a tree that has one, which is nearly every real` |
|         - |   66 | `` * project. `if (false) { function f(){} }` declared `f` too.`` |
|         - |   67 | ` */` |
|    157674 |   68 | `PH7_PRIVATE int GenStateDeclIsConditional(ph7_gen_state *pGen)` |
|         5 |   69 | `{` |
|    157679 |   70 | `	GenBlock *pBlock = pGen->pCurrent;` |
|    157679 |   71 | `	return pBlock != 0 && (pBlock->iFlags & GEN_BLOCK_GLOBAL) == 0;` |
|         5 |   72 | `}` |
|         - |   73 |  |
|     47671 |   74 | `PH7_PRIVATE GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|         5 |   75 | `{` |
|     47676 |   76 | `	GenBlock *pBlock = pCurrent;` |
|    108404 |   77 | `	for(;;){` |
|    217101 |   78 | `		if( pBlock->iFlags & iBlockType ){` |
|     47676 |   79 | `			iCount--; /* Decrement nesting level */` |
|     47676 |   80 | `			if( iCount < 1 ){` |
|         - |   81 | `				/* Block meet with the desired criteria */` |
|     47624 |   82 | `				return pBlock;` |
|         - |   83 | `			}` |
|        26 |   84 | `		}` |
|         - |   85 | `		/* Point to the upper block */` |
|    169482 |   86 | `		pBlock = pBlock->pParent;` |
|    169482 |   87 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|         - |   88 | `			/* Forbidden */` |
|        29 |   89 | `			break;` |
|         - |   90 | `		}` |
|         5 |   91 | `	}` |
|         - |   92 | `	/* No such block */` |
|        55 |   93 | `	return 0;` |
|     23809 |   94 | `}` |
|         - |   95 | `/*` |
|         - |   96 | ` * Initialize a freshly allocated block instance.` |
|         - |   97 | ` */` |
|   1635496 |   98 | `static void GenStateInitBlock(` |
|         - |   99 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  100 | `	GenBlock *pBlock,    /* Target block */` |
|         - |  101 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |  102 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|         - |  103 | `	void *pUserData      /* Upper layer private data */` |
|         - |  104 | `	)` |
|         5 |  105 | `{` |
|         - |  106 | `	/* Initialize block fields */` |
|   1635501 |  107 | `	pBlock->nFirstInstr = nFirstInstr;` |
|   1635501 |  108 | `	pBlock->pUserData   = pUserData;` |
|   1635501 |  109 | `	pBlock->pGen        = pGen;` |
|   1635501 |  110 | `	pBlock->iFlags      = iType;` |
|   1635501 |  111 | `	pBlock->pParent     = 0;` |
|   1635501 |  112 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|   1635501 |  113 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|   1635501 |  114 | `}` |
|         - |  115 | `/*` |
|         - |  116 | ` * Allocate a new block instance.` |
|         - |  117 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|         - |  118 | ` * on success.Otherwise generate a compile-time error and abort` |
|         - |  119 | ` * processing on failure.` |
|         - |  120 | ` */` |
|   1628771 |  121 | `PH7_PRIVATE sxi32 GenStateEnterBlock(` |
|         - |  122 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |  123 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |  124 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|         - |  125 | `	void *pUserData,      /* Upper layer private data */` |
|         - |  126 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|         - |  127 | `	)` |
|         5 |  128 | `{` |
|         - |  129 | `	GenBlock *pBlock;` |
|         - |  130 | `	/* Allocate a new block instance */` |
|   1628776 |  131 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|   1628776 |  132 | `	if( pBlock == 0 ){` |
|         - |  133 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |  134 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - |  135 | `		 */` |
|       ! 0 |  136 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|         - |  137 | `		/* Abort processing immediately */` |
|       ! 0 |  138 | `		return SXERR_ABORT;` |
|         - |  139 | `	}` |
|         - |  140 | `	/* Zero the structure */` |
|   1628776 |  141 | `	SyZero(pBlock,sizeof(GenBlock));` |
|   1628776 |  142 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|         - |  143 | `	/* Link to the parent block */` |
|   1628776 |  144 | `	pBlock->pParent = pGen->pCurrent;` |
|         - |  145 | `	/* A loop or switch gets an id, and remembers the loop it nests inside, so a goto's` |
|         - |  146 | `	 * and a label's positions can be compared after compilation (see aLoopParent). */` |
|   1628776 |  147 | `	if( iType & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    126109 |  148 | `		sxu32 nParent = pGen->nCurLoopId;` |
|    126109 |  149 | `		pGen->nLoopId++;` |
|    126109 |  150 | `		SySetPut(&pGen->aLoopParent,(const void *)&nParent);` |
|    126109 |  151 | `		pBlock->nLoopId = pGen->nLoopId;` |
|    126109 |  152 | `		pBlock->nOuterLoopId = nParent;` |
|    126109 |  153 | `		pGen->nCurLoopId = pGen->nLoopId;` |
|     62959 |  154 | `	}` |
|         - |  155 | `	/* A try/catch/finally block gets a scope id, and remembers the scope it nests inside,` |
|         - |  156 | `	 * so the chain between any two points can be walked after compilation (aScope). Every` |
|         - |  157 | `	 * other block simply inherits the scope in effect. */` |
|   1628776 |  158 | `	pBlock->nOuterScopeId = pGen->nCurScopeId;` |
|   1628776 |  159 | `	pBlock->nScopeId = pGen->nCurScopeId;` |
|   1628776 |  160 | `	if( iType & GEN_BLOCK_EXCEPTION ){` |
|         - |  161 | `		GenScope sScope;` |
|     10089 |  162 | `		sScope.nParent = pGen->nCurScopeId;` |
|     10089 |  163 | `		sScope.pUserData = pUserData;` |
|     10089 |  164 | `		if( iType & GEN_BLOCK_FINALLY ){` |
|       323 |  165 | `			sScope.iKind = GEN_SCOPE_FINALLY;` |
|      9930 |  166 | `		}else if( iType & GEN_BLOCK_DETACHED ){` |
|      4703 |  167 | `			sScope.iKind = GEN_SCOPE_DETACHED;` |
|      2350 |  168 | `		}else{` |
|         - |  169 | `			/* A try. pUserData is its ph7_exception, which every try-block site passes at` |
|         - |  170 | `			 * ENTRY precisely so this can classify it. */` |
|      5073 |  171 | `			sScope.iKind = GenStateInlineTryCatch(pGen) ? GEN_SCOPE_TRY_INLINE : GEN_SCOPE_TRY;` |
|         - |  172 | `		}` |
|     10089 |  173 | `		if( SySetPut(&pGen->aScope,(const void *)&sScope) == SXRET_OK ){` |
|     10089 |  174 | `			pBlock->nScopeId = SySetUsed(&pGen->aScope);` |
|     10089 |  175 | `			pGen->nCurScopeId = pBlock->nScopeId;` |
|      5034 |  176 | `		}` |
|      5034 |  177 | `	}` |
|         - |  178 | `	/* Mark as the current block */` |
|   1628776 |  179 | `	pGen->pCurrent = pBlock;` |
|   1628776 |  180 | `	if( ppBlock ){` |
|         - |  181 | `		/* Write a pointer to the new instance */` |
|    768379 |  182 | `		*ppBlock = pBlock;` |
|    383558 |  183 | `	}` |
|   1628776 |  184 | `	return SXRET_OK;` |
|    813164 |  185 | `}` |
|         - |  186 | `/*` |
|         - |  187 | ` * Release block fields without freeing the whole instance.` |
|         - |  188 | ` */` |
|   1628757 |  189 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|         5 |  190 | `{` |
|   1628762 |  191 | `	SySetRelease(&pBlock->aPostContFix);` |
|   1628762 |  192 | `	SySetRelease(&pBlock->aJumpFix);` |
|   1628762 |  193 | `}` |
|         - |  194 | `/*` |
|         - |  195 | ` * Release a block.` |
|         - |  196 | ` */` |
|   1628753 |  197 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|         5 |  198 | `{` |
|   1628758 |  199 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|   1628758 |  200 | `	GenStateReleaseBlock(&(*pBlock));` |
|         - |  201 | `	/* Free the instance */` |
|   1628758 |  202 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|   1628758 |  203 | `}` |
|         - |  204 | `/*` |
|         - |  205 | ` * POP and release a block from the stack of compiled blocks.` |
|         - |  206 | ` */` |
|   1628753 |  207 | `PH7_PRIVATE sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|         5 |  208 | `{` |
|   1628758 |  209 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   1628758 |  210 | `	if( pBlock == 0 ){` |
|         - |  211 | `		/* No more block to pop */` |
|       ! 0 |  212 | `		return SXERR_EMPTY;` |
|         - |  213 | `	}` |
|   1628758 |  214 | `	if( pBlock->iFlags & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    126099 |  215 | `		pGen->nCurLoopId = pBlock->nOuterLoopId;` |
|     62954 |  216 | `	}` |
|   1628758 |  217 | `	if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|     10089 |  218 | `		pGen->nCurScopeId = pBlock->nOuterScopeId;` |
|      5034 |  219 | `	}` |
|         - |  220 | `	/* Point to the upper block */` |
|   1628758 |  221 | `	pGen->pCurrent = pBlock->pParent;` |
|   1628758 |  222 | `	if( ppBlock ){` |
|         - |  223 | `		/* Write a pointer to the popped block */` |
|       ! 0 |  224 | `		*ppBlock = pBlock;` |
|       ! 0 |  225 | `	}else{` |
|         - |  226 | `		/* Safely release the block */` |
|   1628758 |  227 | `		GenStateFreeBlock(&(*pBlock));` |
|         - |  228 | `	}` |
|   1628758 |  229 | `	return SXRET_OK;` |
|    813155 |  230 | `}` |
|         - |  231 | `/*` |
|         - |  232 | ` * PHP-parity redeclaration guard.` |
|         - |  233 | ` *` |
|         - |  234 | ` * PHP raises a fatal "Cannot redeclare ..." when a class/interface/trait/enum` |
|         - |  235 | ` * or a function is declared a second time. PHL hoists every declaration into` |
|         - |  236 | `` * the VM at compile time (so `if(false){class C{}}` already makes C exist), and`` |
|         - |  237 | ` * historically it silently *overwrote* duplicates. We reproduce PHP for the` |
|         - |  238 | ` * case that matters and that real code hits: a declaration that is` |
|         - |  239 | ` * UNCONDITIONAL and at file top level, whose name is already bound by another` |
|         - |  240 | ` * unconditional top-level declaration (or by a builtin). Conditional` |
|         - |  241 | ` * declarations (inside if/loops/switch/try or nested in a function) are left` |
|         - |  242 | `` * hoisting as before, so the `if(!class_exists('C')){class C{}}` and`` |
|         - |  243 | `` * `if(false){class C{}} class C{}` guard idioms keep working.`` |
|         - |  244 | ` *` |
|         - |  245 | ` * Included files compile at include time (i.e. at run time relative to the main` |
|         - |  246 | ` * script), so this compile-time check surfaces the fatal at the same moment PHP` |
|         - |  247 | ` * does for the cross-include case too.` |
|         - |  248 | ` */` |
|    157376 |  249 | `PH7_PRIVATE int GenStateUnconditionalTopLevel(ph7_gen_state *pGen)` |
|         5 |  250 | `{` |
|    157381 |  251 | `	GenBlock *pBlock = pGen->pCurrent;` |
|    157443 |  252 | `	while( pBlock ){` |
|    157443 |  253 | `		if( pBlock->iFlags & (GEN_BLOCK_COND\|GEN_BLOCK_LOOP\|GEN_BLOCK_FUNC\|GEN_BLOCK_SWITCH\|GEN_BLOCK_EXCEPTION) ){` |
|       102 |  254 | `			return 0; /* conditional / nested */` |
|         - |  255 | `		}` |
|    157345 |  256 | `		if( pBlock->iFlags & GEN_BLOCK_GLOBAL ){` |
|    157283 |  257 | `			return 1; /* reached the global block with no conditional ancestor */` |
|         - |  258 | `		}` |
|        66 |  259 | `		pBlock = pBlock->pParent;` |
|         4 |  260 | `	}` |
|       ! 0 |  261 | `	return 1;` |
|     78589 |  262 | `}` |
|         - |  263 | `/*` |
|         - |  264 | ` * Guard a top-level function about to be installed. Same contract as the class` |
|         - |  265 | ` * guard above.` |
|         - |  266 | ` */` |
|    151842 |  267 | `PH7_PRIVATE sxi32 GenStateGuardFuncRedeclaration(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|         5 |  268 | `{` |
|         - |  269 | `	SyHashEntry *pEntry;` |
|    151847 |  270 | `	if( !GenStateUnconditionalTopLevel(pGen) ){` |
|       ! 0 |  271 | `		return SXRET_OK;` |
|         - |  272 | `	}` |
|    151847 |  273 | `	pFunc->iFlags \|= VM_FUNC_BOUND;` |
|    151847 |  274 | `	if( pGen->pVm->bCompilingBuiltin ){` |
|    147867 |  275 | `		return SXRET_OK;` |
|         - |  276 | `	}` |
|         - |  277 | `	/* Host functions present AT COMPILE TIME are guarded here, and they have to be` |
|         - |  278 | `	 * for the migration of the builtin library into C to be behaviour-preserving: a` |
|         - |  279 | `	 * function moving from an embedded PHP chunk to a C routine moves from hFunction` |
|         - |  280 | `	 * to hHostFunction, and would otherwise silently LOSE the redeclaration guard it` |
|         - |  281 | ``	 * had. `function ini_get(){}` really did win over the builtin for the rest of`` |
|         - |  282 | `	 * the program once ini_get became C.` |
|         - |  283 | `	 *` |
|         - |  284 | `	 * Which builtins that covers depends on WHERE they register. The subsystems` |
|         - |  285 | `	 * installed inside PH7_VmInit's bCompilingBuiltin window (INI, libxml, ...) are` |
|         - |  286 | `	 * in hHostFunction before any user code compiles, so they are caught. The ~650` |
|         - |  287 | `	 * core builtins (strlen, ...) register later, in PH7_VmMakeReady, which runs` |
|         - |  288 | `	 * AFTER compilation — hHostFunction has no entry for them yet, so shadowing one` |
|         - |  289 | `	 * remains the known divergence it has always been (php fatals; §7.2). Nothing` |
|         - |  290 | `	 * about their behaviour changes here.` |
|         - |  291 | `	 *` |
|         - |  292 | `	 * The bCompilingBuiltin early-return above keeps the prelude itself exempt. */` |
|      3985 |  293 | `	if( SyHashGet(&pGen->pVm->hHostFunction,(const void *)pFunc->sName.zString,pFunc->sName.nByte) ){` |
|         8 |  294 | `		PH7_GenCompileError(pGen,E_ERROR,pFunc->nLine,` |
|         2 |  295 | `			"Cannot redeclare function %z()",&pFunc->sName);` |
|         6 |  296 | `		return SXERR_ABORT;` |
|         - |  297 | `	}` |
|      3981 |  298 | `	pEntry = SyHashGet(&pGen->pVm->hFunction,(const void *)pFunc->sName.zString,pFunc->sName.nByte);` |
|      3981 |  299 | `	if( pEntry ){` |
|         8 |  300 | `		ph7_vm_func *pPrev = (ph7_vm_func *)pEntry->pUserData;` |
|         8 |  301 | `		while( pPrev ){` |
|         8 |  302 | `			if( pPrev->iFlags & VM_FUNC_BOUND ){` |
|         8 |  303 | `				if( pPrev->sFile.nByte > 0 ){` |
|        11 |  304 | `					PH7_GenCompileError(pGen,E_ERROR,pFunc->nLine,` |
|         - |  305 | `						"Cannot redeclare function %z() (previously declared in %.*s:%u)",` |
|         3 |  306 | `						&pFunc->sName,pPrev->sFile.nByte,pPrev->sFile.zString,pPrev->nLine);` |
|         5 |  307 | `				}else{` |
|       ! 0 |  308 | `					PH7_GenCompileError(pGen,E_ERROR,pFunc->nLine,` |
|       ! 0 |  309 | `						"Cannot redeclare function %z()",&pFunc->sName);` |
|         - |  310 | `				}` |
|         8 |  311 | `				return SXERR_ABORT;` |
|         - |  312 | `			}` |
|       ! 0 |  313 | `			pPrev = pPrev->pNextName;` |
|       ! 0 |  314 | `		}` |
|       ! 0 |  315 | `	}` |
|      3975 |  316 | `	return SXRET_OK;` |
|     75822 |  317 | `}` |
|         - |  318 | `/*` |
|         - |  319 | ` * Emit a forward jump.` |
|         - |  320 | ` * Notes on forward jumps` |
|         - |  321 | ` *  Compilation of some PHP constructs such as if,for,while and the logical or` |
|         - |  322 | ` *  (\|\|) and logical and (&&) operators in expressions requires the` |
|         - |  323 | ` *  generation of forward jumps.` |
|         - |  324 | ` *  Since the destination PC target of these jumps isn't known when the jumps` |
|         - |  325 | ` *  are emitted, we record each forward jump in an instance of the following` |
|         - |  326 | ` *  structure. Those jumps are fixed later when the jump destination is resolved.` |
|         - |  327 | ` */` |
|    920446 |  328 | `PH7_PRIVATE sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|         5 |  329 | `{` |
|         - |  330 | `	JumpFixup sJumpFix;` |
|         - |  331 | `	sxi32 rc;` |
|         - |  332 | `	/* Init the JumpFixup structure */` |
|    920451 |  333 | `	sJumpFix.nJumpType = nJumpType;` |
|    920451 |  334 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|         - |  335 | `	/* Remember which bytecode array the emitted instruction lives in: the block whose` |
|         - |  336 | `	 * table this lands in may be resolved after a container swap (see JumpFixup). */` |
|    920451 |  337 | `	sJumpFix.pContainer = PH7_VmGetByteCodeContainer(pBlock->pGen->pVm);` |
|         - |  338 | `	/* Insert in the jump fixup table */` |
|    920451 |  339 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|    920451 |  340 | `	return rc;` |
|         5 |  341 | `}` |
|         - |  342 | `/*` |
|         - |  343 | ` * TRUE when the body being compiled has its try/catch/finally compiled INLINE` |
|         - |  344 | ` * (ROOT C, generator bodies) rather than into detached mini-programs.` |
|         - |  345 | ` */` |
|      5068 |  346 | `PH7_PRIVATE int GenStateInlineTryCatch(ph7_gen_state *pGen)` |
|         5 |  347 | `{` |
|      5073 |  348 | `	return pGen->bInGenerator && pGen->pVm->bInlineTryCatch;` |
|         5 |  349 | `}` |
|         - |  350 | `/*` |
|         - |  351 | ` * Walk the scope chain from nFrom (where a jump is) out to nTo (where it lands) and` |
|         - |  352 | `` * describe what it crosses. One walk serves `break`, `continue` and `goto` alike,`` |
|         - |  353 | ` * because they all ask the same question of the same chain — only the two endpoints` |
|         - |  354 | ` * differ, and for a goto they are not both known until compilation ends.` |
|         - |  355 | ` *` |
|         - |  356 | ` * Returns TRUE when nTo was actually reached, i.e. the target's scope ENCLOSES the` |
|         - |  357 | ` * jump. FALSE means the target sits inside a try/catch the jump is not in — jumping` |
|         - |  358 | ` * into one, which PHL cannot express (its handler is pushed by the try's` |
|         - |  359 | ` * OP_LOAD_EXCEPTION, and a catch body is a mini-program entered at instruction 0).` |
|         - |  360 | ` * Depth counting cannot answer this: two sibling trys have the same depth.` |
|         - |  361 | ` *` |
|         - |  362 | ` * What is counted, for the opcode the caller then picks:` |
|         - |  363 | ` *  nDet    — DETACHED catch/finally bodies left. Each is its own bytecode array, so a` |
|         - |  364 | ` *            jump out of one cannot be a plain OP_JMP: it parks and travels out through` |
|         - |  365 | ` *            one OP_POP_EXCEPTION landing pad per boundary (OP_CATCH_JMP);` |
|         - |  366 | ` *  nTry    — legacy trys left whose OP_POP_EXCEPTION the jump SKIPS, so nothing else` |
|         - |  367 | ` *            would run their finally. Trys BELOW the first boundary do not qualify: a` |
|         - |  368 | ` *            break/continue emits their OP_POP_EXCEPTION right here (bEmitPops), and a` |
|         - |  369 | ` *            goto drains them where it parks — hence the reset when one is reached;` |
|         - |  370 | ` *  nInline — ROOT C inline trys left. Their finallys are driven by VmFinallyAdvance,` |
|         - |  371 | ` *            not by the aException drain, so they are crossed with OP_SET_FINALLY_JMP;` |
|         - |  372 | `` *  nFinally — `finally` bodies left, which php forbids outright. When this is non-zero the`` |
|         - |  373 | ` *            three above are NOT computed: callers must test it first and reject.` |
|         - |  374 | ` */` |
|     47775 |  375 | `PH7_PRIVATE int GenStateJumpScope(ph7_gen_state *pGen,sxu32 nFrom,sxu32 nTo,int bEmitPops,` |
|         - |  376 | `	GenJumpScope *pScope)` |
|         5 |  377 | `{` |
|     47780 |  378 | `	GenScope *aScope = (GenScope *)SySetBasePtr(&pGen->aScope);` |
|     47780 |  379 | `	sxu32 nUsed = SySetUsed(&pGen->aScope);` |
|     47780 |  380 | `	sxu32 nCur = nFrom;` |
|     47780 |  381 | `	SyZero(pScope,sizeof(*pScope));` |
|     47894 |  382 | `	while( nCur != nTo ){` |
|         - |  383 | `		GenScope *pScopeEnt;` |
|       123 |  384 | `		if( nCur == 0 \|\| nCur > nUsed ){` |
|         6 |  385 | `			return FALSE; /* ran off the top without meeting nTo */` |
|         - |  386 | `		}` |
|       119 |  387 | `		pScopeEnt = &aScope[nCur - 1];` |
|       119 |  388 | `		if( pScopeEnt->iKind == GEN_SCOPE_FINALLY ){` |
|         - |  389 | ``			/* php: `jump out of a finally block is disallowed`. Counted rather than`` |
|         - |  390 | `			 * rejected here because the caller owns the diagnostic and its line — but` |
|         - |  391 | `			 * ONLY counted: the jump is illegal, so the other three fields are left as` |
|         - |  392 | `			 * they are rather than pretending to describe a crossing that will never be` |
|         - |  393 | `			 * emitted. (They could not be right anyway: this kind covers both the legacy` |
|         - |  394 | `			 * detached finally and the generator's INLINE one, which is not a separate` |
|         - |  395 | `			 * bytecode container.) Every caller tests nFinally first. A jump that stays` |
|         - |  396 | `			 * INSIDE the finally never reaches this scope, so it stays legal. */` |
|        14 |  397 | `			pScope->nFinally++;` |
|       114 |  398 | `		}else if( pScopeEnt->iKind == GEN_SCOPE_DETACHED ){` |
|        74 |  399 | `			if( pScope->nDet == 0 ){` |
|        70 |  400 | `				pScope->nTry = 0;    /* below the first boundary: not the landing pad's */` |
|        70 |  401 | `				pScope->nInline = 0;` |
|        33 |  402 | `			}` |
|        74 |  403 | `			pScope->nDet++;` |
|        74 |  404 | `		}else if( pScopeEnt->iKind == GEN_SCOPE_TRY_INLINE ){` |
|        10 |  405 | `			pScope->nInline++;` |
|        35 |  406 | `		}else if( pScope->nDet == 0 && bEmitPops ){` |
|         3 |  407 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pScopeEnt->pUserData,0);` |
|         2 |  408 | `		}else{` |
|        28 |  409 | `			pScope->nTry++;` |
|         - |  410 | `		}` |
|       119 |  411 | `		nCur = pScopeEnt->nParent;` |
|         5 |  412 | `	}` |
|     47776 |  413 | `	return TRUE;` |
|     23861 |  414 | `}` |
|         - |  415 | `/*` |
|         - |  416 | ` * Pick the jump opcode for a crossing described by GenStateJumpScope, and its iP1.` |
|         - |  417 | ` * Shared by break/continue (which know their target at emit time) and goto (which` |
|         - |  418 | ` * settles this in GenStateFixGoto, once the label fixes the counts).` |
|         - |  419 | ` */` |
|     47659 |  420 | `PH7_PRIVATE sxi32 GenStateScopeJumpOp(const GenJumpScope *pCross,sxi32 *piP1)` |
|         5 |  421 | `{` |
|     47664 |  422 | `	if( pCross->nDet > 0 \|\| pCross->nTry > 0 ){` |
|        84 |  423 | `		*piP1 = PH7_CATCH_JMP_P1(pCross->nDet,pCross->nTry);` |
|        84 |  424 | `		return PH7_OP_CATCH_JMP;` |
|         - |  425 | `	}` |
|     47584 |  426 | `	if( pCross->nInline > 0 ){` |
|        10 |  427 | `		*piP1 = (sxi32)pCross->nInline;` |
|        10 |  428 | `		return PH7_OP_SET_FINALLY_JMP;` |
|         - |  429 | `	}` |
|     47576 |  430 | `	*piP1 = 0;` |
|     47576 |  431 | `	return PH7_OP_JMP;` |
|     23803 |  432 | `}` |
|         - |  433 | `/*` |
|         - |  434 | ` * Resolve a recorded fixup to its VM instruction, in the container it was emitted` |
|         - |  435 | ` * into (see JumpFixup.pContainer) rather than whichever one is current now.` |
|         - |  436 | ` */` |
|    934060 |  437 | `PH7_PRIVATE VmInstr * GenStateFixupInstr(const JumpFixup *pFix)` |
|         5 |  438 | `{` |
|    934065 |  439 | `	return (VmInstr *)SySetAt(pFix->pContainer,pFix->nInstrIdx);` |
|         5 |  440 | `}` |
|         - |  441 | `/*` |
|         - |  442 | ` * Fix a forward jump now the jump destination is resolved.` |
|         - |  443 | ` * Return the total number of fixed jumps.` |
|         - |  444 | ` * Notes on forward jumps:` |
|         - |  445 | ` *  Compilation of some PHP constructs such as if,for,while and the logical or` |
|         - |  446 | ` *  (\|\|) and logical and (&&) operators in expressions requires the` |
|         - |  447 | ` *  generation of forward jumps.` |
|         - |  448 | ` *  Since the destination PC target of these jumps isn't known when the jumps` |
|         - |  449 | ` *  are emitted, we record each forward jump in an instance of the following` |
|         - |  450 | ` *  structure.Those jumps are fixed later when the jump destination is resolved.` |
|         - |  451 | ` */` |
|   1313865 |  452 | `PH7_PRIVATE sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|         5 |  453 | `{` |
|         - |  454 | `	JumpFixup *aFix;` |
|         - |  455 | `	VmInstr *pInstr;` |
|         - |  456 | `	sxu32 nFixed;` |
|         - |  457 | `	sxu32 n;` |
|         - |  458 | `	/* Point to the jump fixup table */` |
|   1313870 |  459 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|         - |  460 | `	/* Fix the desired jumps */` |
|   2999990 |  461 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|   1686125 |  462 | `		if( aFix[n].nJumpType < 0 ){` |
|         - |  463 | `			/* Already fixed */` |
|    596428 |  464 | `			continue;` |
|         - |  465 | `		}` |
|   1089702 |  466 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|         - |  467 | `			/* Not of our interest */` |
|    169260 |  468 | `			continue;` |
|         - |  469 | `		}` |
|         - |  470 | `		/* Point to the instruction to fix */` |
|    920447 |  471 | `		pInstr = GenStateFixupInstr(&aFix[n]);` |
|    920447 |  472 | `		if( pInstr ){` |
|    920447 |  473 | `			pInstr->iP2 = nJumpDest;` |
|    920447 |  474 | `			nFixed++;` |
|         - |  475 | `			/* Mark as fixed */` |
|    920447 |  476 | `			aFix[n].nJumpType = -1;` |
|    459587 |  477 | `		}` |
|    459592 |  478 | `	}` |
|         - |  479 | `	/* Total number of fixed jumps */` |
|   1313870 |  480 | `	return nFixed;` |
|         5 |  481 | `}` |
|         - |  482 | `/*` |
|         - |  483 | ` * Fix a 'goto' now the jump destination is resolved.` |
|         - |  484 | ` * The goto statement can be used to jump to another section` |
|         - |  485 | ` * in the program.` |
|         - |  486 | ` * Refer to the routine responsible of compiling the goto` |
|         - |  487 | ` * statement for more information.` |
|         - |  488 | ` */` |
|    193569 |  489 | `PH7_PRIVATE sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|         5 |  490 | `{` |
|         - |  491 | `	JumpFixup *pJump,*aJumps;` |
|         - |  492 | `	GenJumpScope sCross;` |
|         - |  493 | `	Label *pLabel;` |
|         - |  494 | `	VmInstr *pInstr;` |
|         - |  495 | `	sxi32 rc;` |
|         - |  496 | `	sxu32 n;` |
|         - |  497 | `	/* Point to the goto table */` |
|    193574 |  498 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|         - |  499 | `	/* Fix */` |
|    193798 |  500 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
|       231 |  501 | `		pJump = &aJumps[n];` |
|         - |  502 | `		/* Extract the target label */` |
|         - |  503 | `		/* A label declared in ANOTHER function is not a destination: the lookup is keyed` |
|         - |  504 | `		 * on the goto's own function, so a same-named label elsewhere simply does not` |
|         - |  505 | `		 * answer and this reports php's undefined-label fatal. */` |
|       231 |  506 | `		rc = GenStateGetLabel(&(*pGen),&pJump->sLabel,pJump->pFunc,&pLabel);` |
|       231 |  507 | `		if( rc != SXRET_OK ){` |
|         - |  508 | `			/* No such label */` |
|        70 |  509 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"'goto' to undefined label '%z'",&pJump->sLabel);` |
|        70 |  510 | `			if( rc == SXERR_ABORT ){` |
|         3 |  511 | `				return SXERR_ABORT;` |
|         - |  512 | `			}` |
|        68 |  513 | `			continue;` |
|         - |  514 | `		}` |
|         - |  515 | `		/* php's one goto restriction: you may not jump INTO a loop or a switch. The label` |
|         - |  516 | `		 * is inside one exactly when it carries a loop id; that is legal only if the same` |
|         - |  517 | `		 * loop also encloses the goto, i.e. the label's loop is the goto's loop or one of` |
|         - |  518 | `		 * its ancestors. Walk up from the goto's loop looking for the label's. */` |
|       165 |  519 | `		if( pLabel->nLoopId != 0 ){` |
|         5 |  520 | `			sxu32 *aParent = (sxu32 *)SySetBasePtr(&pGen->aLoopParent);` |
|         5 |  521 | `			sxu32 nCur = pJump->nLoopId;` |
|         5 |  522 | `			int bInside = 0;` |
|         5 |  523 | `			while( nCur != 0 ){` |
|         5 |  524 | `				if( nCur == pLabel->nLoopId ){` |
|         5 |  525 | `					bInside = 1;` |
|         5 |  526 | `					break;` |
|         - |  527 | `				}` |
|       ! 0 |  528 | `				nCur = (nCur <= SySetUsed(&pGen->aLoopParent)) ? aParent[nCur - 1] : 0;` |
|       ! 0 |  529 | `			}` |
|         5 |  530 | `			if( !bInside ){` |
|       ! 0 |  531 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,` |
|         - |  532 | `					"'goto' into loop or switch statement is disallowed");` |
|       ! 0 |  533 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  534 | `					return SXERR_ABORT;` |
|         - |  535 | `				}` |
|       ! 0 |  536 | `				continue;` |
|         - |  537 | `			}` |
|         2 |  538 | `		}` |
|         - |  539 | `		/* What the jump crosses, and whether it is legal at all: the label's scope must` |
|         - |  540 | `		 * ENCLOSE the goto. Jumping INTO a try/catch/finally is fine in php (its handlers` |
|         - |  541 | `		 * are instruction RANGES, so landing anywhere in the body is being in the try),` |
|         - |  542 | `		 * but PHL pushes a handler at the try's OP_LOAD_EXCEPTION and runs a catch body` |
|         - |  543 | `		 * as a mini-program entered at its first instruction — there is no way to arrive` |
|         - |  544 | `		 * mid-body with the handler live. Say so rather than jump nowhere in silence,` |
|         - |  545 | `		 * skip a finally, or land in a foreign array. */` |
|       165 |  546 | `		if( !GenStateJumpScope(&(*pGen),pJump->nScopeId,pLabel->nScopeId,FALSE,&sCross) ){` |
|         6 |  547 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,` |
|         - |  548 | `				"'goto' into a try, catch or finally block is disallowed");` |
|         6 |  549 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  550 | `				return SXERR_ABORT;` |
|         - |  551 | `			}` |
|         6 |  552 | `			continue;` |
|         - |  553 | `		}` |
|       161 |  554 | `		if( sCross.nFinally > 0 ){` |
|         - |  555 | `			/* php's other structural rule, shared with break/continue. Tested AFTER the` |
|         - |  556 | `			 * reach test above, so a goto that both leaves a finally and lands somewhere` |
|         - |  557 | `			 * that does not enclose it reports the into-a-try wording instead of this` |
|         - |  558 | `			 * one. Both are fatal on the same line, and the two cannot be told apart` |
|         - |  559 | `			 * without a second walk outward from the LABEL — php accepts one of them` |
|         - |  560 | ``			 * (`finally { goto L; try { L: … } }`), which is the §7.2 divergence, and`` |
|         - |  561 | `			 * rejects the other. Not worth a second walk for a message on input that is` |
|         - |  562 | `			 * rejected either way. */` |
|         3 |  563 | `			if( GenStateJumpOutOfFinally(&(*pGen),pJump->nLine) == SXERR_ABORT ){` |
|       ! 0 |  564 | `				return SXERR_ABORT;` |
|         - |  565 | `			}` |
|         3 |  566 | `			continue;` |
|         - |  567 | `		}` |
|         - |  568 | `		/* Fix the jump now the destination is resolved — in the container the goto was` |
|         - |  569 | `		 * emitted into, which for a goto inside a catch/finally body is not the one` |
|         - |  570 | `		 * current here (gotos resolve at end of compilation, after every swap back). */` |
|       159 |  571 | `		pInstr = GenStateFixupInstr(pJump);` |
|       159 |  572 | `		if( pInstr ){` |
|       159 |  573 | `			pInstr->iP2 = pLabel->nJumpDest;` |
|       159 |  574 | `			if( pInstr->iOp == PH7_OP_CATCH_JMP ){` |
|         - |  575 | `				/* Emitted as a structure-crossing jump because the goto sits inside a` |
|         - |  576 | `				 * try or a detached body. Now that the crossing is known it may well` |
|         - |  577 | `				 * turn out to leave nothing, and downgrade to a plain OP_JMP. */` |
|        48 |  578 | `				sxi32 iP1 = 0;` |
|        48 |  579 | `				pInstr->iOp = (sxu8)GenStateScopeJumpOp(&sCross,&iP1);` |
|        48 |  580 | `				pInstr->iP1 = iP1;` |
|        22 |  581 | `			}` |
|        77 |  582 | `		}` |
|        82 |  583 | `	}` |
|         - |  584 | `	/* php says nothing about a label nobody jumps to — the old "defined but not` |
|         - |  585 | `	 * referenced" warning was a PH7-ism with no counterpart in the oracle. */` |
|    193572 |  586 | `	return SXRET_OK;` |
|     96660 |  587 | `}` |
|         - |  588 | `/*` |
|         - |  589 | ` * Check if a given token value is installed in the literal table.` |
|         - |  590 | ` */` |
|   1763724 |  591 | `PH7_PRIVATE sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|         5 |  592 | `{` |
|         - |  593 | `	SyHashEntry *pEntry;` |
|   1763729 |  594 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|   1763729 |  595 | `	if( pEntry == 0 ){` |
|    742208 |  596 | `		return SXERR_NOTFOUND;` |
|         - |  597 | `	}` |
|   1021526 |  598 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|   1021526 |  599 | `	return SXRET_OK;` |
|    879966 |  600 | `}` |
|         - |  601 | `/*` |
|         - |  602 | ` * Install a given constant index in the literal table.` |
|         - |  603 | ` * In order to be installed, the ph7_value must be of type string.` |
|         - |  604 | ` *` |
|         - |  605 | ` * NOTE: empty strings are deliberately omitted here.  The VM reserves a` |
|         - |  606 | ` * single shared constant for "" during initialization (pVm->nEmptyStringIdx)` |
|         - |  607 | ` * and the compiler emits a LOADC referencing that slot whenever an empty` |
|         - |  608 | ` * literal is encountered.  This keeps the literal hash from growing when` |
|         - |  609 | ` * many "" literals appear in user code.` |
|         - |  610 | ` */` |
|    742203 |  611 | `PH7_PRIVATE sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|         5 |  612 | `{` |
|    742208 |  613 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|    742208 |  614 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|    370272 |  615 | `	}` |
|    742208 |  616 | `	return SXRET_OK;` |
|         5 |  617 | `}` |
|         - |  618 | `/*` |
|         - |  619 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|         - |  620 | ` * in the constant table.` |
|         - |  621 | ` */` |
|    756615 |  622 | `PH7_PRIVATE ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|         5 |  623 | `{` |
|         - |  624 | `	ph7_value *pObj;` |
|    756620 |  625 | `	sxu32 nIdx = 0; /* cc warning */` |
|         - |  626 | `	/* Reserve a new constant */` |
|    756620 |  627 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    756620 |  628 | `	if( pObj == 0 ){` |
|       ! 0 |  629 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  630 | `		return 0;` |
|         - |  631 | `	}` |
|    756620 |  632 | `	*pIdx = nIdx;` |
|         - |  633 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|         - |  634 | `	 * the constant string iterals table [optimization purposes].` |
|         - |  635 | `	 */` |
|    756620 |  636 | `	return pObj;` |
|    377777 |  637 | `}` |
|         - |  638 | `/*` |
|         - |  639 | ` * Implementation of the PHP language constructs.` |
|         - |  640 | ` */` |
|         - |  641 | `/*` |
|         - |  642 | ` * Ensure the about-to-be-emitted CALL/NEW opcode carries a VmCallArgMap` |
|         - |  643 | ` * that reflects the caller file's strict_types mode. Returns the (possibly` |
|         - |  644 | ` * newly allocated and zero-initialized) map pointer. In weak-mode files` |
|         - |  645 | ` * this is a no-op and the caller's p3 is returned unchanged.` |
|         - |  646 | ` *` |
|         - |  647 | ` * NOTE: on allocation failure the call reverts to weak semantics rather` |
|         - |  648 | ` * than aborting compilation — out-of-memory during a map allocation is` |
|         - |  649 | ` * vanishingly unlikely and silently dropping to weak mode matches the` |
|         - |  650 | ` * surrounding callsites' zero-check fallback pattern.` |
|         - |  651 | ` */` |
|   1034783 |  652 | `PH7_PRIVATE void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|         5 |  653 | `{` |
|         - |  654 | `	VmCallArgMap *pMap;` |
|   1034788 |  655 | `	if( !pGen->bStrictTypes ) return p3;` |
|       521 |  656 | `	if( p3 == 0 ){` |
|        54 |  657 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|        54 |  658 | `		if( pMap == 0 ) return 0;` |
|        54 |  659 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|        54 |  660 | `		p3 = (void *)pMap;` |
|        25 |  661 | `	}` |
|       521 |  662 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|       521 |  663 | `	return p3;` |
|    516325 |  664 | `}` |
|         - |  665 | `/* Forward declaration */` |
|         - |  666 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut);` |
|         - |  667 | `/* Forward declarations */` |
|         - |  668 | `/*` |
|         - |  669 | ` * Recover from a compile-time error. In other words synchronize` |
|         - |  670 | ` * the token stream cursor with the first semi-colon seen.` |
|         - |  671 | ` */` |
|         8 |  672 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|         1 |  673 | `{` |
|         - |  674 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|        17 |  675 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|         9 |  676 | `		pGen->pIn++;` |
|         1 |  677 | `	}` |
|         9 |  678 | `	return SXRET_OK;` |
|         1 |  679 | `}` |
|         - |  680 | `/*` |
|         - |  681 | ` * Check if the given identifier name is reserved or not.` |
|         - |  682 | ` * Return TRUE if reserved.FALSE otherwise.` |
|         - |  683 | ` */` |
|       202 |  684 | `PH7_PRIVATE int GenStateIsReservedConstant(SyString *pName)` |
|         5 |  685 | `{` |
|       207 |  686 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|        21 |  687 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|         3 |  688 | `			return TRUE;` |
|        19 |  689 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|         5 |  690 | `			return TRUE;` |
|         3 |  691 | `		}` |
|       195 |  692 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|        17 |  693 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|         3 |  694 | `			return TRUE;` |
|         - |  695 | `		}` |
|         6 |  696 | `	}` |
|         - |  697 | `	/* Not a reserved constant */` |
|       199 |  698 | `	return FALSE;` |
|       106 |  699 | `}` |
|         - |  700 | `/*` |
|         - |  701 | ` * Chain operators participate in a postfix member-access chain.` |
|         - |  702 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|         - |  703 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|         - |  704 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|         - |  705 | ` */` |
|         - |  706 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|         - |  707 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|         - |  708 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|         - |  709 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|         - |  710 | `/*` |
|         - |  711 | ` * The chain operators that ACCESS a container -- the same four minus the call,` |
|         - |  712 | ` * whose result is an ordinary value however the chain around it is read. Used` |
|         - |  713 | ` * to spot an INTERMEDIATE link of an isset()/empty() chain, which php reads for` |
|         - |  714 | ` * its value rather than for a truth.` |
|         - |  715 | ` */` |
|         - |  716 | `#define GEN_IS_ACCESS_OP(iOp) \` |
|         - |  717 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|         - |  718 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT)` |
|         - |  719 |  |
|         - |  720 | `/*` |
|         - |  721 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|         - |  722 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|         - |  723 | ` * patched entries from the pending set.` |
|         - |  724 | ` */` |
|   8351348 |  725 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|         5 |  726 | `{` |
|   8351353 |  727 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|         - |  728 | `	sxu32 nTarget;` |
|         - |  729 | `	sxu32 *aIdx;` |
|         - |  730 | `	sxu32 i;` |
|   8351353 |  731 | `	if( nCur <= nBaseline ){` |
|   8351201 |  732 | `		return;` |
|         - |  733 | `	}` |
|       156 |  734 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|       156 |  735 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|       316 |  736 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|       164 |  737 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|       164 |  738 | `		if( pInstr ){` |
|       164 |  739 | `			pInstr->iP2 = (sxi32)nTarget;` |
|        80 |  740 | `		}` |
|        84 |  741 | `	}` |
|       156 |  742 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
|   4168930 |  743 | `}` |
|         - |  744 |  |
|         - |  745 | `/*` |
|         - |  746 | `` * Does this call-argument node reach its target THROUGH a property? `$o->p`,`` |
|         - |  747 | ``  * `$this->m['k']`, `$o->a->b` all do; `$a['k']`, `$a[$i][$j]` and a plain `$var` `` |
|         - |  748 | ` * do not.` |
|         - |  749 | ` *` |
|         - |  750 | ` * Only the SUBSCRIPT spine is walked, because that is the only operator whose` |
|         - |  751 | `` * base is still part of the same lvalue: everything else (a call, a cast, `::`,`` |
|         - |  752 | `` * `?->`) either ends the path or is not writable through at all.`` |
|         - |  753 | ` */` |
|      1542 |  754 | `static int GenStateArgHasPropertyStep(ph7_expr_node *pNode)` |
|         5 |  755 | `{` |
|      1589 |  756 | `	while( pNode && pNode->pOp ){` |
|       129 |  757 | `		if( pNode->pOp->iOp == EXPR_OP_ARROW ){` |
|        80 |  758 | `			return 1;` |
|         - |  759 | `		}` |
|        51 |  760 | `		if( pNode->pOp->iOp != EXPR_OP_SUBSCRIPT ){` |
|         8 |  761 | `			return 0;` |
|         - |  762 | `		}` |
|        44 |  763 | `		pNode = pNode->pLeft;` |
|         2 |  764 | `	}` |
|      1463 |  765 | `	return 0;` |
|       769 |  766 | `}` |
|         - |  767 | `/*` |
|         - |  768 | ` * By-reference out-parameters of builtin functions.` |
|         - |  769 | ` *` |
|         - |  770 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|         - |  771 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|         - |  772 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|         - |  773 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|         - |  774 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|         - |  775 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|         - |  776 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|         - |  777 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|         - |  778 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|         - |  779 | ` * creates it" behaviour).` |
|         - |  780 | ` *` |
|         - |  781 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|         - |  782 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|         - |  783 | ` */` |
|    934014 |  784 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|         5 |  785 | `{` |
|         - |  786 | `	static const struct {` |
|         - |  787 | `		const char *zName;` |
|         - |  788 | `		sxu32 nByte;` |
|         - |  789 | `		sxu32 mask;` |
|         - |  790 | `	} aByRef[] = {` |
|         - |  791 | `		{ "parse_str",              9, 1u<<1 },  /* &$result (apArg[1]) */` |
|         - |  792 | `		{ "settype",                7, 1u<<0 },  /* &$var    (apArg[0]) */` |
|         - |  793 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|         - |  794 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|         - |  795 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|         - |  796 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|         - |  797 | `		{ "flock",                  5, 1u<<2 },  /* &$would_block (apArg[2]) */` |
|         - |  798 | `		{ "getopt",                 6, 1u<<2 },  /* &$rest_index (apArg[2]) */` |
|         - |  799 | `		{ "is_callable",           11, 1u<<2 },  /* &$callable_name (apArg[2]) */` |
|         - |  800 | `		{ "similar_text",          12, 1u<<2 },  /* &$percent (apArg[2]) */` |
|         - |  801 | `		{ "headers_sent",         12, (1u<<0)\|(1u<<1) },  /* &$filename, &$line */` |
|         - |  802 | `		{ "str_replace",           11, 1u<<3 },  /* &$count  (apArg[3]) */` |
|         - |  803 | `		{ "str_ireplace",          12, 1u<<3 },  /* &$count  (apArg[3]) */` |
|         - |  804 | `		{ "fsockopen",              9, (1u<<2)\|(1u<<3) },  /* &$error_code, &$error_message */` |
|         - |  805 | `		{ "pfsockopen",            10, (1u<<2)\|(1u<<3) },  /* same */` |
|         - |  806 | `		{ "stream_socket_client",  20, (1u<<1)\|(1u<<2) },  /* &$error_code, &$error_message */` |
|         - |  807 | `		{ "stream_socket_server",  20, (1u<<1)\|(1u<<2) },  /* same pair */` |
|         - |  808 | `		{ "stream_socket_accept",  20, 1u<<2 },            /* &$peer_name (apArg[2]) */` |
|         - |  809 | `		{ "stream_select",         13, (1u<<0)\|(1u<<1)\|(1u<<2) }, /* &$read, &$write, &$except */` |
|         - |  810 | `		{ "stream_socket_recvfrom",22, 1u<<3 },            /* &$address (apArg[3]) */` |
|         - |  811 | `		{ "proc_open",              9, 1u<<2 },  /* &$pipes (apArg[2]) */` |
|         - |  812 | `		{ "exec",                   4, (1u<<1)\|(1u<<2) },  /* &$output, &$result_code */` |
|         - |  813 | `		{ "system",                 6, 1u<<1 },  /* &$result_code (apArg[1]) */` |
|         - |  814 | `		{ "passthru",               8, 1u<<1 },  /* &$result_code (apArg[1]) */` |
|         - |  815 | `		/* A by-ref VARIADIC tail: every actual from the third on is one of` |
|         - |  816 | ``		 * sscanf()'s `&...$vars`, so each is created rather than read. */`` |
|         - |  817 | ``		/* ext/openssl's out-params. Each is the argument php declares `&$x`;`` |
|         - |  818 | `		 * the certificate half adds its own rows beside these. */` |
|         - |  819 | `		{ "openssl_encrypt",             15, 1u<<5 },  /* &$tag (apArg[5]) */` |
|         - |  820 | `		{ "openssl_random_pseudo_bytes", 27, 1u<<1 },  /* &$strong_result */` |
|         - |  821 | `		{ "openssl_pkey_export",         19, 1u<<1 },  /* &$output */` |
|         - |  822 | `		{ "openssl_sign",                12, 1u<<1 },  /* &$signature */` |
|         - |  823 | `		{ "openssl_private_encrypt",     23, 1u<<1 },  /* &$encrypted_data */` |
|         - |  824 | `		{ "openssl_private_decrypt",     23, 1u<<1 },  /* &$decrypted_data */` |
|         - |  825 | `		{ "openssl_public_encrypt",      22, 1u<<1 },  /* &$encrypted_data */` |
|         - |  826 | `		{ "openssl_public_decrypt",      22, 1u<<1 },  /* &$decrypted_data */` |
|         - |  827 | `		{ "openssl_seal",                12, (1u<<1)\|(1u<<2)\|(1u<<5) },` |
|         - |  828 | `		{ "openssl_open",                12, 1u<<1 },  /* &$output */` |
|         - |  829 | `		{ "openssl_x509_export",         19, 1u<<1 },  /* &$output */` |
|         - |  830 | `		{ "openssl_csr_export",          18, 1u<<1 },  /* &$output */` |
|         - |  831 | `		{ "openssl_csr_new",             15, 1u<<1 },  /* &$private_key */` |
|         - |  832 | `		{ "openssl_pkcs12_export",       21, 1u<<1 },  /* &$output */` |
|         - |  833 | `		{ "openssl_pkcs12_read",         19, 1u<<1 },  /* &$certificates */` |
|         - |  834 | `		{ "openssl_pkcs7_read",          18, 1u<<1 },  /* &$certificates */` |
|         - |  835 | `		{ "openssl_cms_read",            16, 1u<<1 },  /* &$certificates */` |
|         - |  836 | ``		/* ext/pcntl's out-params. `pcntl_signal_dispatch` has none; every`` |
|         - |  837 | `		 * other by-reference argument in the extension is one of these. */` |
|         - |  838 | `		{ "pcntl_waitpid",         13, (1u<<1)\|(1u<<3) }, /* &$status, &$resource_usage */` |
|         - |  839 | `		{ "pcntl_wait",            10, (1u<<0)\|(1u<<2) }, /* &$status, &$resource_usage */` |
|         - |  840 | `		{ "pcntl_waitid",          12, (1u<<2)\|(1u<<4) }, /* &$info,   &$resource_usage */` |
|         - |  841 | `		{ "pcntl_sigprocmask",     17, 1u<<2 },           /* &$old_signals */` |
|         - |  842 | `		{ "pcntl_sigwaitinfo",     17, 1u<<1 },           /* &$info */` |
|         - |  843 | `		{ "pcntl_sigtimedwait",    18, 1u<<1 },           /* &$info */` |
|         - |  844 | `		{ "sscanf",                 6, ~((1u<<2) - 1u) },` |
|         - |  845 | `		{ "fscanf",                 6, ~((1u<<2) - 1u) },` |
|         - |  846 | `	};` |
|         - |  847 | `	sxu32 i;` |
|    934019 |  848 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|     28301 |  849 | `		return 0;` |
|         - |  850 | `	}` |
|  44868774 |  851 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
|  43972470 |  852 | `		if( pName->nByte == aByRef[i].nByte` |
|  23036121 |  853 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|      9424 |  854 | `			return aByRef[i].mask;` |
|         - |  855 | `		}` |
|  21934103 |  856 | `	}` |
|    896304 |  857 | `	return 0;` |
|    466009 |  858 | `}` |
|         - |  859 | `/*` |
|         - |  860 | ` * What may be passed by REFERENCE is decided from the argument's SHAPE, at compile` |
|         - |  861 | ` * time, exactly as php decides it (zend_compile_args -> zend_is_variable).` |
|         - |  862 | ` *` |
|         - |  863 | ` * php sorts every actual argument into three buckets:` |
|         - |  864 | ` *` |
|         - |  865 | ` *   GEN_ARG_LVALUE   a variable, an element, a property, a static property. It has a` |
|         - |  866 | ` *                    slot, so a by-ref parameter aliases it.` |
|         - |  867 | `` *   GEN_ARG_TEMPCALL the result of a call or of `new`. It has no slot, but php cannot`` |
|         - |  868 | ` *                    know at compile time whether the callee returns a reference, so it` |
|         - |  869 | ` *                    defers: E_NOTICE "Only variables should be passed by reference",` |
|         - |  870 | ` *                    then it operates on the temporary.` |
|         - |  871 | ` *   GEN_ARG_NONE     everything else — a literal, an operator/cast result, a class` |
|         - |  872 | `` *                    constant, `@$x`, `$o?->p`, an assignment. Binding one to a by-ref`` |
|         - |  873 | ` *                    parameter is a catchable Error at the CALL.` |
|         - |  874 | ` *` |
|         - |  875 | ` * Deciding it from the argument's runtime memobj instead does not work and was silently` |
|         - |  876 | ` * wrong in both directions: an arithmetic or concatenation result keeps its LEFT operand's` |
|         - |  877 | `` * slot index, so `f($i + 1)` with `function f(&$x)` aliased and overwrote `$i`; and a`` |
|         - |  878 | ` * builtin's by-ref row saw only "no slot", which a call result has too.` |
|         - |  879 | ` */` |
|         - |  880 | `#define GEN_ARG_LVALUE   0` |
|         - |  881 | `#define GEN_ARG_TEMPCALL 1` |
|         - |  882 | `#define GEN_ARG_NONE     2` |
|   1314537 |  883 | `static int GenStateArgShape(ph7_expr_node *pNode)` |
|         5 |  884 | `{` |
|   1314542 |  885 | `	if( pNode == 0 ){` |
|       ! 0 |  886 | `		return GEN_ARG_NONE;` |
|         - |  887 | `	}` |
|   1314542 |  888 | `	if( pNode->pOp == 0 ){` |
|         - |  889 | ``		/* A leaf: only the `$…` family is a variable. Everything else the parser`` |
|         - |  890 | ``		 * files here — a literal, an array/list constructor, a closure, `match`,`` |
|         - |  891 | ``		 * `clone` — is a temporary. */`` |
|   1078239 |  892 | `		return pNode->xCode == PH7_CompileVariable ? GEN_ARG_LVALUE : GEN_ARG_NONE;` |
|         - |  893 | `	}` |
|    236308 |  894 | `	switch( pNode->pOp->iOp ){` |
|     16023 |  895 | `	case EXPR_OP_SUBSCRIPT: /* $a[k], and any base: php accepts g()[0] and C::m()[0] */` |
|         - |  896 | `	case EXPR_OP_ARROW:     /* $o->p */` |
|     32012 |  897 | `		return GEN_ARG_LVALUE;` |
|       538 |  898 | `	case EXPR_OP_DC:` |
|         - |  899 | ``		/* `C::$s` is a static property (an lvalue); `C::K` is a class constant and`` |
|         - |  900 | ``		 * `C::CASE` an enum case, neither of which php will bind. The right operand`` |
|         - |  901 | `		 * tells them apart. */` |
|      1619 |  902 | `		return ( pNode->pRight && pNode->pRight->pOp == 0` |
|      1076 |  903 | `		      && pNode->pRight->xCode == PH7_CompileVariable )` |
|      1076 |  904 | `			? GEN_ARG_LVALUE : GEN_ARG_NONE;` |
|     34305 |  905 | `	case EXPR_OP_FUNC_CALL:` |
|         - |  906 | `	case EXPR_OP_NEW:` |
|     68438 |  907 | `		return GEN_ARG_TEMPCALL;` |
|         2 |  908 | `	case EXPR_OP_REF:` |
|         - |  909 | ``		/* `take($q = &$p)`: a reference ASSIGNMENT hands back the reference it`` |
|         - |  910 | `		 * made, so php passes it on to a by-ref parameter and all three names end` |
|         - |  911 | ``		 * up aliasing one slot. A plain `$q = $p` does not -- php's ASSIGN yields`` |
|         - |  912 | `		 * a temporary where ASSIGN_REF yields the VAR -- which is why only this` |
|         - |  913 | `		 * one arm moves. */` |
|         5 |  914 | `		return GEN_ARG_LVALUE;` |
|     67494 |  915 | `	default:` |
|         - |  916 | ``		/* Includes `?->` (php: "Cannot use nullsafe operator in write context"),`` |
|         - |  917 | ``		 * `@$x`, `$q = …`, `clone $o` and every arithmetic/logical operator. */`` |
|    134788 |  918 | `		return GEN_ARG_NONE;` |
|         - |  919 | `	}` |
|    655784 |  920 | `}` |
|         - |  921 | `/*` |
|         - |  922 | ` * Recover the bare global-builtin name from a call's callee node.` |
|         - |  923 | ` *` |
|         - |  924 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|         - |  925 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|         - |  926 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|         - |  927 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|         - |  928 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|         - |  929 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|         - |  930 | ` */` |
|   1830815 |  931 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|         5 |  932 | `{` |
|         - |  933 | `	SyToken *p, *pEnd;` |
|   1830820 |  934 | `	pOut->zString = 0;` |
|   1830820 |  935 | `	pOut->nByte = 0;` |
|   1830820 |  936 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|       ! 0 |  937 | `		return;` |
|         - |  938 | `	}` |
|   1830820 |  939 | `	p = pLeft->pStart;` |
|   1830820 |  940 | `	pEnd = pLeft->pEnd;` |
|         - |  941 | `	/* Optional single leading namespace separator (absolute path). */` |
|   1830820 |  942 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|       211 |  943 | `		p++;` |
|       103 |  944 | `	}` |
|   1830820 |  945 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     40890 |  946 | `		return;` |
|         - |  947 | `	}` |
|         - |  948 | `	/* Must be a single component: nothing follows the name token. */` |
|   1789935 |  949 | `	if( p + 1 != pEnd ){` |
|       514 |  950 | `		return;` |
|         - |  951 | `	}` |
|   1789426 |  952 | `	*pOut = p->sData;` |
|    913480 |  953 | `}` |
|         - |  954 | `/*` |
|         - |  955 | `` * Is this expression node the bare variable `$this`?`` |
|         - |  956 | ` */` |
|    879007 |  957 | `PH7_PRIVATE int PH7_ExprNodeIsThis(ph7_expr_node *pNode)` |
|         5 |  958 | `{` |
|         - |  959 | `	SyToken *pTok;` |
|    879012 |  960 | `	if( pNode == 0 \|\| pNode->pOp != 0 \|\| pNode->xCode != PH7_CompileVariable ){` |
|    161322 |  961 | `		return 0;` |
|         - |  962 | `	}` |
|    717695 |  963 | `	pTok = pNode->pStart;` |
|    717695 |  964 | `	if( pTok == 0 \|\| pNode->pEnd == 0 \|\| pNode->pEnd < &pTok[2] ){` |
|       ! 0 |  965 | `		return 0;` |
|         - |  966 | `	}` |
|   1076031 |  967 | `	return (pTok[0].nType & PH7_TK_DOLLAR) != 0` |
|    717690 |  968 | `		&& (pTok[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|    717678 |  969 | `		&& pTok[1].sData.nByte == sizeof("this")-1` |
|   1077044 |  970 | `		&& SyMemcmp((const void *)pTok[1].sData.zString,(const void *)"this",sizeof("this")-1) == 0;` |
|    438891 |  971 | `}` |
|         - |  972 | `/*` |
|         - |  973 | ` * TRUE when codegen is inside a real FUNCTION body — php's` |
|         - |  974 | `` * `CG(active_op_array)->function_name`. A synthetic block (a match() arm's`` |
|         - |  975 | ` * throw-fixup) carries no ph7_vm_func and is not a scope.` |
|         - |  976 | ` */` |
|         2 |  977 | `static int GenStateInFunction(ph7_gen_state *pGen)` |
|         1 |  978 | `{` |
|         3 |  979 | `	GenBlock *pBlock = pGen->pCurrent;` |
|         9 |  980 | `	while( pBlock ){` |
|         7 |  981 | `		if( (pBlock->iFlags & GEN_BLOCK_FUNC) && pBlock->pUserData ){` |
|       ! 0 |  982 | `			return 1;` |
|         - |  983 | `		}` |
|         7 |  984 | `		pBlock = pBlock->pParent;` |
|         1 |  985 | `	}` |
|         3 |  986 | `	return 0;` |
|         2 |  987 | `}` |
|         - |  988 | `/*` |
|         - |  989 | `` * php's SPECIALIZED builtins — the list behind `Cannot use result of built-in`` |
|         - |  990 | `` * function in write context`.`` |
|         - |  991 | ` *` |
|         - |  992 | ` * The wording says "built-in function" but the rule is not about builtins: php` |
|         - |  993 | ` * refuses the write when the call was compiled to an OPCODE of its own rather` |
|         - |  994 | ` * than a real call, because a specialized opcode leaves a TMP where a call` |
|         - |  995 | `` * leaves a VAR (`zend_separate_if_call_and_write`). So `strlen("x")[0] = 1` and`` |
|         - |  996 | `` * `count([1])[0] = 1` are compile fatals while `array_values([1])[0] = 2`,`` |
|         - |  997 | `` * `str_split("ab")[0] = "z"` and `get_object_vars($o)["k"] = 2` all RUN — the`` |
|         - |  998 | `` * difference being php's `zend_try_compile_special_func_ex` table, reproduced`` |
|         - |  999 | ` * here name for name with the ARITY each entry demands.` |
|         - | 1000 | ` *` |
|         - | 1001 | ` * Six of php's names are deliberately absent, and only the last pair is a` |
|         - | 1002 | ` * simplification — the other four are not refusals of php's at all:` |
|         - | 1003 | `` *   `chr`/`ord` gate on BP_VAR_R, so they are never special in a WRITE context;`` |
|         - | 1004 | `` *   `call_user_func`/`call_user_func_array` emit a REAL call, so their result is`` |
|         - | 1005 | ` *     a VAR and php does not refuse a write through it either;` |
|         - | 1006 | `` *   `in_array` and `array_slice` gate on the CONTENTS of a literal array`` |
|         - | 1007 | `` *     argument and on a `func_get_args()`-shaped first argument — value-dependent`` |
|         - | 1008 | ` *     shapes no program writes through, left out under §10. Leaving them out` |
|         - | 1009 | ` *     ACCEPTS where php refuses, which is the direction that keeps running a` |
|         - | 1010 | ` *     program php runs.` |
|         - | 1011 | ` * Verified by sweeping every internal function of both engines at arities 0-3:` |
|         - | 1012 | ` * the two specialized sets are identical, 27 names at the same arities.` |
|         - | 1013 | ` */` |
|         - | 1014 | `#define SPECFN_LITERAL_ARG0 0x01 /* php gives up unless argument #1 is a literal */` |
|         - | 1015 | ``#define SPECFN_ANY_ARGS     0x02 /* …and `assert` is decided BEFORE php's unpack/named`` |
|         - | 1016 | ``                                  * bail, so it stays special even for `assert(...$a)` */`` |
|         - | 1017 | `#define SPECFN_IN_FUNC      0x04 /* php's gate reads CG(active_op_array)->function_name:` |
|         - | 1018 | `                                  * at GLOBAL scope it emits a real call, whose runtime` |
|         - | 1019 | `                                  * Error ("cannot be called from the global scope") is` |
|         - | 1020 | `                                  * what the program actually gets */` |
|         - | 1021 | ``#define SPECFN_FORMAT_ARG0  0x08 /* …and `sprintf` also needs php's format arithmetic`` |
|         - | 1022 | `                                  * (implies SPECFN_LITERAL_ARG0) */` |
|         - | 1023 | `static const struct {` |
|         - | 1024 | `	const char *zName;` |
|         - | 1025 | `	int nMinArg;   /* inclusive */` |
|         - | 1026 | `	int nMaxArg;   /* inclusive; -1 = variadic */` |
|         - | 1027 | `	int iFlags;` |
|         - | 1028 | `} aSpecialFunc[] = {` |
|         - | 1029 | `	{ "strlen",           1,  1, 0 },` |
|         - | 1030 | `	{ "is_null",          1,  1, 0 },  { "is_bool",          1,  1, 0 },` |
|         - | 1031 | `	{ "is_long",          1,  1, 0 },  { "is_int",           1,  1, 0 },` |
|         - | 1032 | `	{ "is_integer",       1,  1, 0 },  { "is_float",         1,  1, 0 },` |
|         - | 1033 | `	{ "is_double",        1,  1, 0 },  { "is_string",        1,  1, 0 },` |
|         - | 1034 | `	{ "is_array",         1,  1, 0 },  { "is_object",        1,  1, 0 },` |
|         - | 1035 | `	{ "is_resource",      1,  1, 0 },  { "is_scalar",        1,  1, 0 },` |
|         - | 1036 | `	{ "boolval",          1,  1, 0 },  { "intval",           1,  1, 0 },` |
|         - | 1037 | `	{ "floatval",         1,  1, 0 },  { "doubleval",        1,  1, 0 },` |
|         - | 1038 | `	{ "strval",           1,  1, 0 },` |
|         - | 1039 | `	{ "count",            1,  1, 0 },  { "sizeof",           1,  1, 0 },` |
|         - | 1040 | `	{ "get_class",        0,  1, 0 },  { "get_called_class", 0,  0, 0 },` |
|         - | 1041 | `	{ "gettype",          1,  1, 0 },` |
|         - | 1042 | `	{ "func_num_args",    0,  0, SPECFN_IN_FUNC },` |
|         - | 1043 | `	{ "func_get_args",    0,  0, SPECFN_IN_FUNC },` |
|         - | 1044 | `	{ "array_key_exists", 2,  2, 0 },` |
|         - | 1045 | `	{ "defined",          1,  1, SPECFN_LITERAL_ARG0 },` |
|         - | 1046 | `	{ "sprintf",          1, -1, SPECFN_LITERAL_ARG0\|SPECFN_FORMAT_ARG0 },` |
|         - | 1047 | `	/* php compiles assert() to its own opcode pair "independently of compiler` |
|         - | 1048 | `	 * flags", in zend_compile_call BEFORE the special-func table is consulted —` |
|         - | 1049 | `	 * so every arity counts and an unpacked argument does not exempt it. */` |
|         - | 1050 | `	{ "assert",           0, -1, SPECFN_ANY_ARGS },` |
|         - | 1051 | `};` |
|         - | 1052 | `/*` |
|         - | 1053 | ` * TRUE when this call node is one php compiles to an opcode of its own, so a` |
|         - | 1054 | ` * write THROUGH its result is php's built-in-function refusal. pName is the` |
|         - | 1055 | ` * callee's bare global name, already resolved by GenStateCallBuiltinName.` |
|         - | 1056 | ` */` |
|       146 | 1057 | `static int GenStateCallIsSpecialized(ph7_gen_state *pGen,ph7_expr_node *pCall,SyString *pName)` |
|         4 | 1058 | `{` |
|         - | 1059 | `	ph7_expr_node **apArg;` |
|         - | 1060 | `	sxu32 nArg, n;` |
|         - | 1061 | `	sxu32 i;` |
|       150 | 1062 | `	if( pName->nByte < 1 ){` |
|        46 | 1063 | `		return 0;` |
|         - | 1064 | `	}` |
|       106 | 1065 | `	apArg = (ph7_expr_node **)SySetBasePtr(&pCall->aNodeArgs);` |
|       106 | 1066 | `	nArg = SySetUsed(&pCall->aNodeArgs);` |
|      2818 | 1067 | `	for( i = 0 ; i < SX_ARRAYSIZE(aSpecialFunc) ; ++i ){` |
|         - | 1068 | `		SyString sEntry;` |
|      2728 | 1069 | `		SyStringInitFromBuf(&sEntry,aSpecialFunc[i].zName,SyStrlen(aSpecialFunc[i].zName));` |
|      2724 | 1070 | `		if( sEntry.nByte != pName->nByte` |
|      1564 | 1071 | `		 \|\| SyStrnicmp(sEntry.zString,pName->zString,pName->nByte) != 0 ){` |
|      2714 | 1072 | `			continue;` |
|         - | 1073 | `		}` |
|        12 | 1074 | `		if( (int)nArg < aSpecialFunc[i].nMinArg` |
|        16 | 1075 | `		 \|\| (aSpecialFunc[i].nMaxArg >= 0 && (int)nArg > aSpecialFunc[i].nMaxArg) ){` |
|        10 | 1076 | `			return 0;` |
|         - | 1077 | `		}` |
|         - | 1078 | `		/* php bails out of the whole table when any argument unpacks or is named` |
|         - | 1079 | ``		 * (`zend_args_contain_unpack_or_named`), so `strlen(...$a)[0] = 1` RUNS. */`` |
|        11 | 1080 | `		if( (aSpecialFunc[i].iFlags & SPECFN_ANY_ARGS) == 0 ){` |
|        17 | 1081 | `			for( n = 0 ; n < nArg ; ++n ){` |
|        11 | 1082 | `				if( apArg[n] && (apArg[n]->iFlags & (EXPR_NODE_SPREAD\|EXPR_NODE_NAMED_ARG)) ){` |
|         3 | 1083 | `					return 0;` |
|         - | 1084 | `				}` |
|         5 | 1085 | `			}` |
|         3 | 1086 | `		}` |
|         9 | 1087 | `		if( (aSpecialFunc[i].iFlags & SPECFN_IN_FUNC) && !GenStateInFunction(pGen) ){` |
|         3 | 1088 | `			return 0;` |
|         - | 1089 | `		}` |
|         - | 1090 | ``		/* `defined` and `sprintf` specialize only over a LITERAL first argument;`` |
|         - | 1091 | `		 * php gives up on a computed one and emits an ordinary call. */` |
|         6 | 1092 | `		if( aSpecialFunc[i].iFlags & SPECFN_LITERAL_ARG0 ){` |
|         2 | 1093 | `			if( nArg < 1 \|\| apArg[0] == 0 \|\| apArg[0]->pOp != 0` |
|         2 | 1094 | `			 \|\| apArg[0]->pStart == 0` |
|         3 | 1095 | `			 \|\| (apArg[0]->pStart->nType & (PH7_TK_SSTR\|PH7_TK_DSTR)) == 0 ){` |
|       ! 0 | 1096 | `				return 0;` |
|         - | 1097 | `			}` |
|         1 | 1098 | `		}` |
|         6 | 1099 | `		if( (aSpecialFunc[i].iFlags & SPECFN_FORMAT_ARG0) && nArg >= 1 && apArg[0] ){` |
|         - | 1100 | `			/* php's own sprintf gate, and it is arithmetic: a format under 256` |
|         - | 1101 | ``			 * bytes carrying nothing but `%s`, `%d` and `%%`, with exactly one`` |
|         - | 1102 | ``			 * VALUE per placeholder. `sprintf("a","b")` fails it (no placeholder,`` |
|         - | 1103 | `			 * one value) and compiles to an ordinary call, which is why the write` |
|         - | 1104 | `			 * through it RUNS. */` |
|         3 | 1105 | `			const SyString *pFmt = &apArg[0]->pStart->sData;` |
|         3 | 1106 | `			sxu32 nPlace = 0, k;` |
|         3 | 1107 | `			if( pFmt->nByte >= 256 ){` |
|       ! 0 | 1108 | `				return 0;` |
|         - | 1109 | `			}` |
|         - | 1110 | `			/* An escape or an interpolation makes php's argument something other` |
|         - | 1111 | `			 * than a plain literal; leave those to the ordinary call. */` |
|         7 | 1112 | `			for( k = 0 ; k < pFmt->nByte ; ++k ){` |
|         4 | 1113 | `				if( pFmt->zString[k] == '\\'` |
|         7 | 1114 | `				 \|\| ((apArg[0]->pStart->nType & PH7_TK_DSTR)` |
|         4 | 1115 | `				  && (pFmt->zString[k] == '$' \|\| pFmt->zString[k] == '{')) ){` |
|       ! 0 | 1116 | `					return 0;` |
|         - | 1117 | `				}` |
|         3 | 1118 | `			}` |
|         5 | 1119 | `			for( k = 0 ; k < pFmt->nByte ; ++k ){` |
|         3 | 1120 | `				if( pFmt->zString[k] != '%' ){` |
|       ! 0 | 1121 | `					continue;` |
|         - | 1122 | `				}` |
|         3 | 1123 | `				if( k + 1 >= pFmt->nByte ){` |
|       ! 0 | 1124 | `					return 0; /* a trailing '%' */` |
|         - | 1125 | `				}` |
|         3 | 1126 | `				k++;` |
|         3 | 1127 | `				if( pFmt->zString[k] == 's' \|\| pFmt->zString[k] == 'd' ){` |
|         3 | 1128 | `					nPlace++;` |
|         1 | 1129 | `				}else if( pFmt->zString[k] != '%' ){` |
|       ! 0 | 1130 | `					return 0; /* any other conversion */` |
|         - | 1131 | `				}` |
|         2 | 1132 | `			}` |
|         3 | 1133 | `			if( nPlace != nArg - 1 ){` |
|       ! 0 | 1134 | `				return 0;` |
|         - | 1135 | `			}` |
|         1 | 1136 | `		}` |
|         6 | 1137 | `		return 1;` |
|       ! 0 | 1138 | `	}` |
|        91 | 1139 | `	return 0;` |
|        77 | 1140 | `}` |
|         - | 1141 | `/*` |
|         - | 1142 | ` * The two write-target rules php decides at COMPILE time, in one place because` |
|         - | 1143 | ` * every write site has to make both of them.` |
|         - | 1144 | ` *` |
|         - | 1145 | `` * **`$this`** is not a variable a program may re-point: php refuses the`` |
|         - | 1146 | `` * assignment, the reference bind, a foreach/list target and `unset()` where they`` |
|         - | 1147 | `` * are WRITTEN. PHL performed all of them, so `$this = 5;` inside a method`` |
|         - | 1148 | ` * replaced the receiver with an int for the rest of the call and every later` |
|         - | 1149 | `` * `$this->x` failed somewhere else entirely.`` |
|         - | 1150 | ` *` |
|         - | 1151 | ``  * **A temporary** cannot be written THROUGH: `(new A)->p = 1` and `"s"->p = 1` `` |
|         - | 1152 | ` * modify an object/value that no longer exists after the statement, so php` |
|         - | 1153 | `` * refuses the whole chain — every write kind, including `+=`, `++`, `=&` and`` |
|         - | 1154 | `` * `unset()`. The base of the access chain decides: a variable and a userland`` |
|         - | 1155 | `` * CALL are writable (`f()->p = 1` is php-legal), a `new`, a literal and any`` |
|         - | 1156 | ` * other computed value are not, and an internal function's result gets php's own` |
|         - | 1157 | `` * separate wording — which is what `(clone $o)->p = 1` is, `clone` being a`` |
|         - | 1158 | ` * function in php 8.5.` |
|         - | 1159 | ` *` |
|         - | 1160 | ` * **A call is writable THROUGH but not writable INTO.** The distinction is` |
|         - | 1161 | `` * php's, and it is made in two different places: `zend_compile_var_inner` lets`` |
|         - | 1162 | `` * a call be the base of a chain, while `zend_ensure_writable_variable` refuses`` |
|         - | 1163 | ` * the call when it is the target ITSELF, with a wording that says which kind of` |
|         - | 1164 | `` * call it was. So `f()[0] = 5` compiles and `f() = 5` does not. The one write`` |
|         - | 1165 | `` * site that does not ask the second question is the SOURCE of `=&`, which is`` |
|         - | 1166 | `` * why `$r =& f()` is a runtime notice rather than a compile error.`` |
|         - | 1167 | ` */` |
|    878589 | 1168 | `PH7_PRIVATE sxi32 GenStateWriteTargetCheck(ph7_gen_state *pGen,ph7_expr_node *pTarget,int iCtx)` |
|         5 | 1169 | `{` |
|    878594 | 1170 | `	ph7_expr_node *pBase = pTarget;` |
|    878594 | 1171 | `	const char *zMsg = 0;` |
|         - | 1172 | `	sxi32 rc;` |
|    878594 | 1173 | `	if( pTarget == 0 ){` |
|       ! 0 | 1174 | `		return SXRET_OK;` |
|         - | 1175 | `	}` |
|    878594 | 1176 | `	if( PH7_ExprNodeIsThis(pTarget) && (iCtx & (PH7_WTC_REFSRC\|PH7_WTC_RMW)) == 0 ){` |
|         - | 1177 | ``		/* Only as the TARGET. php refuses `$this = …`, `$this =& …`, a`` |
|         - | 1178 | ``		 * foreach/list target and `unset($this)` -- but the SOURCE of a `=&` is`` |
|         - | 1179 | `		 * compiled in write context WITHOUT zend_ensure_writable_variable, and` |
|         - | 1180 | ``		 * that is the function that holds the $this rule. So `$t =& $this` binds`` |
|         - | 1181 | ``		 * the receiver, and so do `$a[] =& $this`, `$this->p =& $this` and`` |
|         - | 1182 | ``		 * `self::$s =& $this`; a $this that has no object behind it is the`` |
|         - | 1183 | `		 * ordinary RUNTIME "Using $this when not in object context". Refusing the` |
|         - | 1184 | ``		 * source here cost react/promise's `$target =& $this` -- Composer's whole`` |
|         - | 1185 | `		 * async download layer.` |
|         - | 1186 | `		 *` |
|         - | 1187 | `		 * And only for an ASSIGNMENT. php makes this rule in the assignment` |
|         - | 1188 | ``		 * compiler, so a READ-MODIFY-WRITE (`$this += 1`, `$this .= "x"`,`` |
|         - | 1189 | ``		 * `$this++`) compiles and raises the ordinary operand error at run time`` |
|         - | 1190 | ``		 * (`Unsupported operand types: C + int`, `Cannot increment C`) -- which`` |
|         - | 1191 | `		 * this engine already words for any other object. */` |
|        16 | 1192 | `		zMsg = (iCtx & PH7_WTC_UNSET) ? "Cannot unset $this" : "Cannot re-assign $this";` |
|    878587 | 1193 | `	}else if( pTarget->pOp && pTarget->pOp->iOp == EXPR_OP_FUNC_CALL` |
|     80279 | 1194 | `	       && (iCtx & PH7_WTC_REFSRC) == 0 ){` |
|         - | 1195 | ``		/* The target is the call itself (`f() = 5`, `f()++`, `unset(f())`,`` |
|         - | 1196 | ``		 * `foreach (… as f())`). php names the kind of call: a METHOD callee —`` |
|         - | 1197 | ``		 * `$o->m()`, `C::m()` — reports "method", everything else "function".`` |
|         - | 1198 | `		 * A PARENTHESISED member callee is php's variable-invocation` |
|         - | 1199 | ``		 * (`($o->p)()` calls the property's VALUE), which is an ordinary`` |
|         - | 1200 | `		 * function call, exactly the distinction the OP_CALL codegen makes. */` |
|        23 | 1201 | `		int bMethod = pTarget->pLeft` |
|        10 | 1202 | `			&& pTarget->pLeft->pOp` |
|         8 | 1203 | `			&& (pTarget->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|         5 | 1204 | `			 \|\| pTarget->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|         3 | 1205 | `			 \|\| pTarget->pLeft->pOp->iOp == EXPR_OP_DC)` |
|        15 | 1206 | `			&& (pTarget->pLeft->iFlags & EXPR_NODE_PARENS) == 0;` |
|        13 | 1207 | `		zMsg = bMethod` |
|         - | 1208 | `			? "Can't use method return value in write context"` |
|         5 | 1209 | `			: "Can't use function return value in write context";` |
|    878577 | 1210 | `	}else if( PH7_ExprContainsNullsafe(pTarget) ){` |
|         - | 1211 | ``		/* php asks this AFTER the call question (`$o?->m()++` is a method return`` |
|         - | 1212 | `` 		 * value, not a nullsafe chain) and BEFORE the base one (`(new A)?->p = 1` `` |
|         - | 1213 | `		 * is the nullsafe refusal, not the temporary). A reference SOURCE has its` |
|         - | 1214 | ``		 * own sentence for it. The `=`/`+=`/`unset()`/foreach paths screened this`` |
|         - | 1215 | ``		 * themselves; `++`/`--`, `??=` and `array(&…)` did not, so `$o?->p++` ran. */`` |
|         6 | 1216 | `		zMsg = (iCtx & PH7_WTC_REFSRC)` |
|         - | 1217 | `			? "Cannot take reference of a nullsafe chain"` |
|         2 | 1218 | `			: "Can't use nullsafe operator in write context";` |
|         4 | 1219 | `	}else{` |
|         - | 1220 | `		/* Walk to the base of the access chain; the links themselves are writable. */` |
|   1039573 | 1221 | `		while( pBase && pBase->pOp ){` |
|    161460 | 1222 | `			if( pBase->pOp->iOp == EXPR_OP_DC && !PH7_ExprNodeIsClassConst(pBase) ){` |
|         - | 1223 | `` 				/* A `::` left operand is a CLASS reference, not a value — `C::$s = 1` `` |
|         - | 1224 | ``				 * and even `(new C)::$s = 1` write class-level storage that outlives`` |
|         - | 1225 | `` 				 * any temporary, so the chain stops being about a base here. A `::` `` |
|         - | 1226 | ``				 * naming a CONSTANT is not storage, though: `A::K[0] = 5` subscripts`` |
|         - | 1227 | `				 * a COPY, so it falls through to the computed-base verdict below —` |
|         - | 1228 | `				 * php's "Cannot use temporary expression in write context", where` |
|         - | 1229 | `				 * PHL wrote into the copy and answered nothing. */` |
|       243 | 1230 | `				return SXRET_OK;` |
|         - | 1231 | `			}` |
|    161217 | 1232 | `			if( pBase->pOp->iOp != EXPR_OP_ARROW && pBase->pOp->iOp != EXPR_OP_NULLSAFE_ARROW` |
|    158366 | 1233 | `			 && pBase->pOp->iOp != EXPR_OP_SUBSCRIPT ){` |
|       217 | 1234 | `				break;` |
|         - | 1235 | `			}` |
|    161010 | 1236 | `			pBase = pBase->pLeft;` |
|         5 | 1237 | `		}` |
|    878330 | 1238 | `		if( pBase == 0 \|\| pBase == pTarget ){` |
|         - | 1239 | `			/* No chain: a non-variable target of its own is the caller's business` |
|         - | 1240 | `			 * (php reports its parse error / "Assignments can only happen to` |
|         - | 1241 | `			 * writable values" there, and so does PHL). */` |
|    717929 | 1242 | `			return SXRET_OK;` |
|         - | 1243 | `		}` |
|    160406 | 1244 | `		if( pBase->pOp == 0 ){` |
|    160252 | 1245 | `			if( pBase->xCode != PH7_CompileVariable ){` |
|       ! 0 | 1246 | `				zMsg = "Cannot use temporary expression in write context";` |
|         5 | 1247 | `			}` |
|     80174 | 1248 | `		}else if( pBase->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|         - | 1249 | `			/* php refuses a write through the result of a call it SPECIALIZED into` |
|         - | 1250 | `			 * an opcode — see aSpecialFunc above. The old test asked whether the` |
|         - | 1251 | `			 * name was a host function AT ALL, which would have refused every` |
|         - | 1252 | `			 * builtin (php specializes 28 of them), and asked it of the CALL node` |
|         - | 1253 | `			 * where GenStateCallBuiltinName wants the CALLEE node — so it never` |
|         - | 1254 | ``			 * matched anything and `clone` below was the only arm that ever fired.`` |
|         - | 1255 | `			 * The name table IS the resolution here: php looks the callee up in a` |
|         - | 1256 | `			 * function table that is fully populated at compile time, and PHL's is` |
|         - | 1257 | `			 * not — the ~650 core builtins register in PH7_VmMakeReady, which runs` |
|         - | 1258 | `			 * AFTER compilation (see the redeclaration guard near the top of this` |
|         - | 1259 | ``			 * file), so hHostFunction has no `strlen` to find. */`` |
|         - | 1260 | `			SyString sName;` |
|       150 | 1261 | `			GenStateCallBuiltinName(pBase->pLeft,&sName);` |
|       150 | 1262 | `			if( GenStateCallIsSpecialized(&(*pGen),pBase,&sName) ){` |
|         6 | 1263 | `				zMsg = "Cannot use result of built-in function in write context";` |
|         6 | 1264 | `			}` |
|        83 | 1265 | `		}else if( pBase->pOp->iOp == EXPR_OP_CLONE ){` |
|         - | 1266 | ``			/* php 8.5 implements `clone` AS a function, so a write through its result`` |
|         - | 1267 | `			 * takes the internal-function wording rather than the temporary one. */` |
|         3 | 1268 | `			zMsg = "Cannot use result of built-in function in write context";` |
|         2 | 1269 | `		}else{` |
|         - | 1270 | ``			/* `new`, and every other computed base. */`` |
|         8 | 1271 | `			zMsg = "Cannot use temporary expression in write context";` |
|         - | 1272 | `		}` |
|         - | 1273 | `	}` |
|    160432 | 1274 | `	if( zMsg == 0 ){` |
|    160394 | 1275 | `		return SXRET_OK;` |
|         - | 1276 | `	}` |
|        42 | 1277 | `	rc = PH7_GenCompileError(&(*pGen),E_ERROR,` |
|        38 | 1278 | `		pTarget->pStart ? pTarget->pStart->nLine : 0,"%s",zMsg);` |
|        42 | 1279 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_INVALID;` |
|    438682 | 1280 | `}` |
|         - | 1281 | `/*` |
|         - | 1282 | ` * What emitting a call's ARGUMENT LIST decided, handed back to the CALL codegen.` |
|         - | 1283 | ` * The arguments are emitted from their own routine because php evaluates them` |
|         - | 1284 | ` * AFTER the callee has been resolved, so this runs between the callee's emission` |
|         - | 1285 | ` * and the OP_CALL — see GenStateEmitCallArgs.` |
|         - | 1286 | ` */` |
|         - | 1287 | `typedef struct GenCallArgs GenCallArgs;` |
|         - | 1288 | `struct GenCallArgs {` |
|         - | 1289 | `	sxi32 iP1;      /* OP_CALL.iP1: the compile-time argument count */` |
|         - | 1290 | ``	sxu32 iP2;      /* OP_CALL.iP2: 1 if any argument unpacks (`...$a`) */`` |
|         - | 1291 | `	void *p3;       /* OP_CALL.p3: the VmCallArgMap, which may ALREADY carry the callee's` |
|         - | 1292 | `	                 * namespace qualification — the callee is emitted first now */` |
|         - | 1293 | ``	int bFcc;       /* First-class callable `f(...)`: no arguments, OP_LOAD_FCC follows */`` |
|         - | 1294 | ``	int bAnySpread; /* Any `...` argument (iP2 says the same; kept for the shape masks) */`` |
|         - | 1295 | `};` |
|         - | 1296 | `static sxi32 GenStateEmitCallArgs(ph7_gen_state *pGen,ph7_expr_node *pNode,sxi32 iFlags,` |
|         - | 1297 | `	GenCallArgs *pArgs);` |
|         - | 1298 | `/*` |
|         - | 1299 | `` * TRUE when the `instanceof` SUBJECT that just compiled into the instruction`` |
|         - | 1300 | ` * stream starting at nFirst is what zend calls IS_CONST -- the shape whose` |
|         - | 1301 | ` * whole expression its compiler folds to FALSE, without ever compiling the` |
|         - | 1302 | ` * class operand.` |
|         - | 1303 | ` *` |
|         - | 1304 | `` * php decides this from its own constant FOLDER: `zend_compile_expr` on the`` |
|         - | 1305 | `` * subject comes back IS_CONST for a literal, for `null`/`true`/`false`, for an`` |
|         - | 1306 | ` * engine constant, for an array literal, and for arithmetic or concatenation` |
|         - | 1307 | `` * over any of those -- and `5 instanceof $x` is then false whatever $x holds,`` |
|         - | 1308 | `` * while `$v instanceof $x` with the same 5 in $v reaches the runtime opcode and`` |
|         - | 1309 | ` * is refused when $x is neither an object nor a string. The two spellings really` |
|         - | 1310 | ` * do answer differently, so OP_IS_A's screen has to be told which one it is.` |
|         - | 1311 | ` *` |
|         - | 1312 | ` * PHL has no constant folder, so the question is asked of the INSTRUCTIONS the` |
|         - | 1313 | ` * subject compiled to: a run built only from LITERAL loads and pure value` |
|         - | 1314 | ` * operators is a constant expression, and anything that reads a variable, names` |
|         - | 1315 | ` * a constant, calls something or touches an object is not. php's own folder` |
|         - | 1316 | `` * reaches two shapes further -- an ENGINE constant (`PHP_EOL`) and a builtin`` |
|         - | 1317 | `` * call it ct-evaluates (`strlen("a")`) -- where php answers false and this`` |
|         - | 1318 | ` * refuses; PLAN.md §7.2 records the pair under the constant-folding family.` |
|         - | 1319 | ` */` |
|     14078 | 1320 | `static int GenStateInstanceofFoldsLhs(ph7_gen_state *pGen,sxu32 nFirst)` |
|         5 | 1321 | `{` |
|         - | 1322 | `	sxu32 n;` |
|     14083 | 1323 | `	sxu32 nLen = PH7_VmInstrLength(pGen->pVm);` |
|     14373 | 1324 | `	for( n = nFirst ; n < nLen ; ++n ){` |
|     14357 | 1325 | `		VmInstr *pIn = PH7_VmGetInstr(pGen->pVm,n);` |
|     14357 | 1326 | `		if( pIn == 0 ){` |
|       ! 0 | 1327 | `			return 0;` |
|         - | 1328 | `		}` |
|     14357 | 1329 | `		switch( pIn->iOp ){` |
|       144 | 1330 | `		case PH7_OP_LOADC:` |
|         - | 1331 | `			/* A LOADC that still carries EXPAND is a NAME the runtime resolves --` |
|         - | 1332 | `			 * a constant -- and that is exactly what php does NOT fold: its` |
|         - | 1333 | `			 * compiler substitutes only the engine's own persistent constants, so` |
|         - | 1334 | ``			 * a userland `const OBJ = new C();` reaches the runtime opcode and`` |
|         - | 1335 | ``			 * `OBJ instanceof C` is a real question. Folding it answered FALSE for`` |
|         - | 1336 | `			 * every constant that holds an object. */` |
|       293 | 1337 | `			if( pIn->iP1 & PH7_LOADC_EXPAND ){` |
|         5 | 1338 | `				return 0;` |
|         - | 1339 | `			}` |
|       289 | 1340 | `			break;` |
|         3 | 1341 | `		case PH7_OP_LOAD_MAP:` |
|         - | 1342 | `		case PH7_OP_CAT:` |
|         - | 1343 | `		case PH7_OP_CVT_INT: case PH7_OP_CVT_STR: case PH7_OP_CVT_REAL:` |
|         - | 1344 | `		case PH7_OP_CVT_BOOL: case PH7_OP_CVT_NUMC: case PH7_OP_CVT_NULL:` |
|         - | 1345 | `		case PH7_OP_UMINUS: case PH7_OP_UPLUS: case PH7_OP_BITNOT: case PH7_OP_LNOT:` |
|         - | 1346 | `		case PH7_OP_MUL: case PH7_OP_DIV: case PH7_OP_MOD: case PH7_OP_POW:` |
|         - | 1347 | `		case PH7_OP_ADD: case PH7_OP_SUB: case PH7_OP_SHL: case PH7_OP_SHR:` |
|         - | 1348 | `		case PH7_OP_LT: case PH7_OP_LE: case PH7_OP_GT: case PH7_OP_GE:` |
|         - | 1349 | `		case PH7_OP_SPACESHIP: case PH7_OP_EQ: case PH7_OP_NEQ:` |
|         - | 1350 | `		case PH7_OP_TEQ: case PH7_OP_TNE:` |
|         - | 1351 | `		case PH7_OP_BAND: case PH7_OP_BXOR: case PH7_OP_BOR:` |
|         7 | 1352 | `			break;` |
|      7038 | 1353 | `		default:` |
|     14063 | 1354 | `			return 0;` |
|         - | 1355 | `		}` |
|       150 | 1356 | `	}` |
|        17 | 1357 | `	return nLen > nFirst;` |
|      7035 | 1358 | `}` |
|         - | 1359 | `/*` |
|         - | 1360 | ` * Generate bytecode for a given expression tree.` |
|         - | 1361 | ` * If something goes wrong while generating bytecode` |
|         - | 1362 | ` * for the expression tree (A very unlikely scenario)` |
|         - | 1363 | ` * this function takes care of generating the appropriate` |
|         - | 1364 | ` * error message.` |
|         - | 1365 | ` */` |
|         - | 1366 | `/*` |
|         - | 1367 | `` * php's `zend_is_variable_or_call`: what may sit on the right of a destructuring`` |
|         - | 1368 | ` * assignment whose target list binds BY REFERENCE. A variable, a property, a` |
|         - | 1369 | ` * static property, a subscript and a CALL can each hand a slot over; an array` |
|         - | 1370 | `` * literal, a string, `new`, and any computed value cannot, and php refuses those`` |
|         - | 1371 | ` * at compile time rather than binding to a temporary.` |
|         - | 1372 | ` */` |
|       344 | 1373 | `static int GenStateNodeIsRefSource(ph7_expr_node *pNode)` |
|         5 | 1374 | `{` |
|       349 | 1375 | `	if( pNode == 0 ){` |
|       ! 0 | 1376 | `		return 0;` |
|         - | 1377 | `	}` |
|       349 | 1378 | `	if( pNode->pOp ){` |
|       196 | 1379 | `		return pNode->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|       127 | 1380 | `		    \|\| pNode->pOp->iOp == EXPR_OP_ARROW` |
|       121 | 1381 | `		    \|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|       116 | 1382 | `		    \|\| pNode->pOp->iOp == EXPR_OP_DC` |
|       191 | 1383 | `		    \|\| pNode->pOp->iOp == EXPR_OP_FUNC_CALL;` |
|         - | 1384 | `	}` |
|       220 | 1385 | `	return pNode->xCode == PH7_CompileVariable;` |
|       177 | 1386 | `}` |
|         - | 1387 | `/*` |
|         - | 1388 | `` * Is this call node's callee the `isset` KEYWORD itself?`` |
|         - | 1389 | ` *` |
|         - | 1390 | ` * isset() is a language construct, not a name: it reaches the call path as a` |
|         - | 1391 | ` * keyword token whose literal the compiler canonicalizes to "isset" (see` |
|         - | 1392 | ` * compile_node.c). A keyword used as a MEMBER name is excluded here for the same` |
|         - | 1393 | `` * reason it is excluded there — `$o->isset(...)` names a method — and so is any`` |
|         - | 1394 | ` * callee that is an operator node rather than a bare literal.` |
|         - | 1395 | ` */` |
|    354164 | 1396 | `static int GenStateCalleeIsIsset(ph7_expr_node *pCallee)` |
|         5 | 1397 | `{` |
|         - | 1398 | `	SyString *pName;` |
|    354169 | 1399 | `	if( pCallee == 0 \|\| pCallee->pOp != 0 \|\| pCallee->pStart == 0 ){` |
|      1937 | 1400 | `		return 0;` |
|         - | 1401 | `	}` |
|    352232 | 1402 | `	if( (pCallee->pStart->nType & PH7_TK_KEYWORD) == 0` |
|    175689 | 1403 | `	 \|\| (pCallee->pStart->nType & PH7_TK_MEMBER_NAME) ){` |
|    352157 | 1404 | `		return 0;` |
|         - | 1405 | `	}` |
|        82 | 1406 | `	pName = &pCallee->pStart->sData;` |
|       120 | 1407 | `	return pName->nByte == sizeof("isset")-1` |
|        80 | 1408 | `		&& SyStrnicmp(pName->zString,"isset",sizeof("isset")-1) == 0;` |
|    176612 | 1409 | `}` |
|   9622923 | 1410 | `static sxi32 GenStateEmitExprCode(` |
|         - | 1411 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 1412 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|         - | 1413 | `	sxi32 iFlags /* Control flags */` |
|         - | 1414 | `	)` |
|         5 | 1415 | `{` |
|         - | 1416 | `	VmInstr *pInstr;` |
|         - | 1417 | `	sxu32 nJmpIdx;` |
|   9622928 | 1418 | `	sxi32 iP1 = 0;` |
|   9622928 | 1419 | `	sxu32 iP2 = 0;` |
|   9622928 | 1420 | `	void *p3  = 0;` |
|         - | 1421 | `	sxi32 iVmOp;` |
|         - | 1422 | `	sxi32 rc;` |
|   9622928 | 1423 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
|   9622928 | 1424 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
|   9622928 | 1425 | `	sxu32 nRhsNsBase = 0;` |
|   9622928 | 1426 | `	sxi32 iRhsFlags = 0; /* control flags the RIGHT operand is compiled under */` |
|   9622928 | 1427 | `	sxu32 nLhsFirst = 0; /* instruction index the LEFT operand starts at */` |
|         - | 1428 | ``	/* Consumed here so it describes THIS node only — the direct operand of a `new` —`` |
|         - | 1429 | `	 * and never travels down into the operand's own sub-expressions. */` |
|   9622928 | 1430 | `	int bNewCallee = (iFlags & EXPR_FLAG_NEW_CALLEE) != 0;` |
|   9622928 | 1431 | `	iFlags &= ~EXPR_FLAG_NEW_CALLEE;` |
|   9622928 | 1432 | `	if( pNode->xCode ){` |
|         - | 1433 | `		SyToken *pTmpIn,*pTmpEnd;` |
|         - | 1434 | `		/* Compile node */` |
|   5959319 | 1435 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
|   5959319 | 1436 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
|   5959319 | 1437 | `		RE_SWAP_DELIMITER(pGen);` |
|   5959319 | 1438 | `		return rc;` |
|         - | 1439 | `	}` |
|   3663614 | 1440 | `	if( pNode->pOp == 0 ){` |
|       ! 0 | 1441 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 1442 | `			"Invalid expression node,PH7 is aborting compilation");` |
|       ! 0 | 1443 | `		return SXERR_ABORT;` |
|         - | 1444 | `	}` |
|   3663614 | 1445 | `	iVmOp = pNode->pOp->iVmOp;` |
|   3663614 | 1446 | `	if( iVmOp == PH7_OP_STORE_REF && pNode->pLeft && PH7_ExprNodeIsThis(pNode->pLeft) ){` |
|         - | 1447 | ``		/* `$t =& $this` is a VALUE assignment. php's `$this` is not a slot a`` |
|         - | 1448 | `		 * reference can name -- the receiver lives in the frame's own field, not` |
|         - | 1449 | `		 * in a variable -- so the bind quietly degrades to a copy of the object` |
|         - | 1450 | ``		 * HANDLE: `$t` gets a slot of its own, `$t->v = 9` still reaches the same`` |
|         - | 1451 | `` 		 * object (that is identity, not reference), and `$t = 5` leaves `$this` `` |
|         - | 1452 | `		 * an object. Binding the slot instead let a write through the alias` |
|         - | 1453 | `		 * REPLACE the receiver for the rest of the call. The operands were` |
|         - | 1454 | `		 * swapped in parse.c, so the source is pLeft. */` |
|        17 | 1455 | `		iVmOp = PH7_OP_STORE;` |
|         8 | 1456 | `	}` |
|   3663614 | 1457 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|         - | 1458 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|         - | 1459 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|         - | 1460 | `		 * and later errors are still reported. */` |
|         3 | 1461 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 1462 | `			"The (unset) cast is no longer supported");` |
|         3 | 1463 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 1464 | `			return SXERR_ABORT;` |
|         - | 1465 | `		}` |
|         1 | 1466 | `	}` |
|   3663614 | 1467 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|       190 | 1468 | `		sxu32 nJmp = 0;` |
|         - | 1469 | `		sxu32 nNcNsBase;` |
|         - | 1470 | `		VmInstr *pInstrFix;` |
|         - | 1471 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|         - | 1472 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|         - | 1473 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|         - | 1474 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|         - | 1475 | `		 * stack slot carries a writable nIdx. */` |
|       190 | 1476 | `		if( pNode->pRight ){` |
|         - | 1477 | ``			/* `$a[] ??= v` READS its target before deciding to write, so php refuses the`` |
|         - | 1478 | ``			 * append form anywhere in that target's chain (`$a[][0] ??= v` too), even`` |
|         - | 1479 | ``			 * though the same tag makes every other `[]` on this path a legal write`` |
|         - | 1480 | ``			 * target. Only the container chain is walked — a `[]` inside an INDEX`` |
|         - | 1481 | `			 * expression is an ordinary read and the subscript codegen refuses it. */` |
|       190 | 1482 | `			ph7_expr_node *pTgt = pNode->pRight;` |
|       437 | 1483 | `			while( pTgt && pTgt->pOp && (pTgt->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|       112 | 1484 | `			      \|\| pTgt->pOp->iOp == EXPR_OP_ARROW \|\| pTgt->pOp->iOp == EXPR_OP_DC) ){` |
|       168 | 1485 | `				if( pTgt->pOp->iOp == EXPR_OP_SUBSCRIPT && SySetUsed(&pTgt->aNodeArgs) < 1 ){` |
|       ! 0 | 1486 | `					break;` |
|         - | 1487 | `				}` |
|       168 | 1488 | `				pTgt = pTgt->pLeft;` |
|         4 | 1489 | `			}` |
|       186 | 1490 | `			if( pTgt && pTgt->pOp && pTgt->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|         5 | 1491 | `			 && SySetUsed(&pTgt->aNodeArgs) < 1 ){` |
|       ! 0 | 1492 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,` |
|       ! 0 | 1493 | `					pNode->pRight->pStart ? pNode->pRight->pStart->nLine : 0,` |
|         - | 1494 | `					"Cannot use [] for reading");` |
|       ! 0 | 1495 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 1496 | `			}` |
|         - | 1497 | `` 			/* …and only THEN the write-target rules, php's order: `strval(1)[] ??= 3` `` |
|         - | 1498 | ``			 * is the append refusal, not the specialized-builtin one. `??=` compiles`` |
|         - | 1499 | `			 * its own way and so never reached this check at all, which is why` |
|         - | 1500 | ``			 * `(new A)->p ??= 3` and `"lit"->p->q ??= 3` used to run. */`` |
|       190 | 1501 | `			rc = GenStateWriteTargetCheck(&(*pGen),pNode->pRight,0);` |
|       190 | 1502 | `			if( rc != SXRET_OK ){` |
|         3 | 1503 | `				return rc;` |
|         - | 1504 | `			}` |
|       187 | 1505 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       187 | 1506 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|       187 | 1507 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1508 | `				return rc;` |
|         - | 1509 | `			}` |
|       187 | 1510 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|         - | 1511 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|         - | 1512 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|         - | 1513 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|         - | 1514 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|         - | 1515 | `			 * the store, so the parent array does not need to be copied at` |
|         - | 1516 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|         - | 1517 | `			 * cascade for the actual write path stays correct. */` |
|       187 | 1518 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|       187 | 1519 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|       101 | 1520 | `				pInstrFix->iP2 = 3;` |
|        49 | 1521 | `			}` |
|        92 | 1522 | `		}` |
|         - | 1523 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|       187 | 1524 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|         - | 1525 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|       187 | 1526 | `		if( pNode->pLeft ){` |
|       187 | 1527 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       187 | 1528 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|       187 | 1529 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1530 | `				return rc;` |
|         - | 1531 | `			}` |
|       187 | 1532 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|        92 | 1533 | `		}` |
|         - | 1534 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|       187 | 1535 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|         - | 1536 | `		/* Patch the short-circuit jump to land after the store. */` |
|       187 | 1537 | `		if( nJmp > 0 ){` |
|       187 | 1538 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|       187 | 1539 | `			if( pInstrFix ){` |
|       187 | 1540 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|        92 | 1541 | `			}` |
|        92 | 1542 | `		}` |
|       187 | 1543 | `		return SXRET_OK;` |
|         - | 1544 | `	}` |
|   3663428 | 1545 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|         - | 1546 | `		sxu32 nJz,nJmp;` |
|         - | 1547 | `		sxu32 nTernaryNsBase;` |
|         - | 1548 | `		/* Ternary operator require special handling */` |
|         - | 1549 | `		/* Phase#1: Compile the condition */` |
|     31306 | 1550 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|     31306 | 1551 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|     31306 | 1552 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1553 | `			return rc;` |
|         - | 1554 | `		}` |
|         - | 1555 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|         - | 1556 | `		 * compiling the condition must short-circuit to the end of the` |
|         - | 1557 | `		 * condition expression, not leak past the ternary. */` |
|     31306 | 1558 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|     31306 | 1559 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|     31306 | 1560 | `		if( pNode->pLeft ){` |
|         - | 1561 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|         - | 1562 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|     31152 | 1563 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - | 1564 | `			/* Phase#3: Compile the 'then' expression  */` |
|     31152 | 1565 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|     31152 | 1566 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|     31152 | 1567 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1568 | `				return rc;` |
|         - | 1569 | `			}` |
|     31152 | 1570 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|     15558 | 1571 | `		}else{` |
|         - | 1572 | `			/* Elvis operator: (expr) ?: (else)` |
|         - | 1573 | `			 * Duplicate condition so original value is the 'then' result.` |
|         - | 1574 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|       159 | 1575 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       159 | 1576 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - | 1577 | `		}` |
|         - | 1578 | `		/* Phase#4: Emit the unconditional jump */` |
|     31306 | 1579 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|         - | 1580 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|     31306 | 1581 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|     31306 | 1582 | `		if( pInstr ){` |
|     31306 | 1583 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     15630 | 1584 | `		}` |
|     31306 | 1585 | `		if( !pNode->pLeft ){` |
|         - | 1586 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|       159 | 1587 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        77 | 1588 | `		}` |
|         - | 1589 | `		/* Phase#6: Compile the 'else' expression */` |
|     31306 | 1590 | `		if( pNode->pRight ){` |
|     31306 | 1591 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|     31306 | 1592 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|     31306 | 1593 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1594 | `				return rc;` |
|         - | 1595 | `			}` |
|     31306 | 1596 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|     15630 | 1597 | `		}` |
|     31306 | 1598 | `		if( nJmp > 0 ){` |
|         - | 1599 | `			/* Phase#7: Fix the unconditional jump */` |
|     31306 | 1600 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|     31306 | 1601 | `			if( pInstr ){` |
|     31306 | 1602 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     15630 | 1603 | `			}` |
|     15630 | 1604 | `		}` |
|         - | 1605 | `		/* All done */` |
|     31306 | 1606 | `		return SXRET_OK;` |
|         - | 1607 | `	}` |
|   3632127 | 1608 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
|         - | 1609 | ``		/* PHP 8.5 pipe: `$lhs \|> $rhs` invokes the RHS callable with the LHS`` |
|         - | 1610 | ``		 * value as its sole argument [i.e. `$rhs($lhs)`]. Evaluate the LHS (the`` |
|         - | 1611 | `		 * argument) first, then the RHS callable, then emit a one-argument` |
|         - | 1612 | `		 * OP_CALL — the same stack shape the function-call path builds (the` |
|         - | 1613 | `		 * argument sits below the callee). The RHS is any callable expression:` |
|         - | 1614 | ``		 * an FCC `f(...)` (an OP_LOAD_FCC Closure), a closure variable, an`` |
|         - | 1615 | ``		 * `[obj,method]` pair, or a callable string. */`` |
|         - | 1616 | `		sxu32 nPipeNsBase;` |
|        27 | 1617 | `		sxi32 iOperandFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE` |
|         - | 1618 | `			\|EXPR_FLAG_MEMBER_REFSRC\|EXPR_FLAG_RDONLY_LOAD);` |
|        27 | 1619 | `		if( pNode->pLeft == 0 \|\| pNode->pRight == 0 ){` |
|       ! 0 | 1620 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 1621 | `				"'\|>': Missing operand");` |
|       ! 0 | 1622 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 1623 | `		}` |
|         - | 1624 | `		/* Argument: the LHS value. */` |
|        27 | 1625 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 | 1626 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iOperandFlags);` |
|        27 | 1627 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1628 | `			return rc;` |
|         - | 1629 | `		}` |
|        27 | 1630 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - | 1631 | `		/* Callable: the RHS. */` |
|        27 | 1632 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 | 1633 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iOperandFlags);` |
|        27 | 1634 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1635 | `			return rc;` |
|         - | 1636 | `		}` |
|        27 | 1637 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - | 1638 | `		/* Invoke the callable with the single piped argument. */` |
|        27 | 1639 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        27 | 1640 | `		return SXRET_OK;` |
|         - | 1641 | `	}` |
|         - | 1642 | `	/*` |
|         - | 1643 | ``	 * php compiles the multi-operand `isset($a, $b, ...)` as a short-circuit CHAIN --`` |
|         - | 1644 | ``	 * `isset($a) && isset($b) && ...` -- so nothing after the first operand that is not`` |
|         - | 1645 | `	 * set is ever evaluated. isset() is a host function here, and a call evaluates every` |
|         - | 1646 | `	 * argument before dispatching, so the later operands ran for real: the ordinary` |
|         - | 1647 | ``	 * `isset($info['k'], $data[$info['k']])` answered false through an `Undefined array`` |
|         - | 1648 | ``	 * key` warning and a null-offset deprecation php never raises, and`` |
|         - | 1649 | ``	 * `isset($a['no'], $b[side()])` CALLED side(). Doctrine's hydrator guards its`` |
|         - | 1650 | `	 * discriminator lookup in exactly that shape, so every hydrated row of every query` |
|         - | 1651 | `	 * carried two diagnostics php does not.` |
|         - | 1652 | `	 *` |
|         - | 1653 | `	 * Emit the chain the compiler owes: one SINGLE-operand isset() per argument, joined` |
|         - | 1654 | `	 * by a keep-the-value JZ to the end (the false it left IS the answer) and a POP on` |
|         - | 1655 | `	 * the fall-through. Each link is the ordinary call path below, re-entered with the` |
|         - | 1656 | `	 * argument set narrowed to one node, so every operand keeps the exact isset context` |
|         - | 1657 | `	 * it already had -- LOAD_IDX iP2=4, the quiet intermediates of an access chain,` |
|         - | 1658 | `	 * ArrayAccess::offsetExists -- and only the ORDER changes. A spread or named operand` |
|         - | 1659 | `	 * opts out: php refuses both in this position, and neither maps to one link.` |
|         - | 1660 | `	 */` |
|   3632096 | 1661 | `	if( iVmOp == PH7_OP_CALL && SySetUsed(&pNode->aNodeArgs) > 1` |
|    643983 | 1662 | `	 && GenStateCalleeIsIsset(pNode->pLeft) ){` |
|        78 | 1663 | `		ph7_expr_node **apIsset = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|        78 | 1664 | `		sxu32 nIsset = SySetUsed(&pNode->aNodeArgs);` |
|        78 | 1665 | `		SySet sSaved = pNode->aNodeArgs;` |
|         - | 1666 | `		SySet sJz;` |
|         - | 1667 | `		sxu32 n;` |
|        78 | 1668 | `		int bPlain = 1;` |
|       288 | 1669 | `		for( n = 0 ; n < nIsset ; ++n ){` |
|       210 | 1670 | `			if( apIsset[n] == 0` |
|       212 | 1671 | `			 \|\| (apIsset[n]->iFlags & (EXPR_NODE_SPREAD\|EXPR_NODE_NAMED_ARG)) ){` |
|       ! 0 | 1672 | `				bPlain = 0;` |
|       ! 0 | 1673 | `				break;` |
|         - | 1674 | `			}` |
|       107 | 1675 | `		}` |
|        78 | 1676 | `		if( bPlain ){` |
|        78 | 1677 | `			SySetInit(&sJz,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|        78 | 1678 | `			rc = SXRET_OK;` |
|       288 | 1679 | `			for( n = 0 ; n < nIsset ; ++n ){` |
|       212 | 1680 | `				pNode->aNodeArgs.pBase = (void *)&apIsset[n];` |
|       212 | 1681 | `				pNode->aNodeArgs.nUsed = 1;` |
|       212 | 1682 | `				pNode->aNodeArgs.nSize = 1;` |
|       212 | 1683 | `				pNode->aNodeArgs.nCursor = 0;` |
|       212 | 1684 | `				rc = GenStateEmitExprCode(&(*pGen),pNode,iFlags);` |
|       212 | 1685 | `				pNode->aNodeArgs = sSaved;` |
|       212 | 1686 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 1687 | `					break;` |
|         - | 1688 | `				}` |
|       212 | 1689 | `				if( n + 1 < nIsset ){` |
|       136 | 1690 | `					sxu32 nJz = 0;` |
|       136 | 1691 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,` |
|         - | 1692 | `						1 /* keep the false on the stack: it is the answer */,0,0,&nJz);` |
|       136 | 1693 | `					SySetPut(&sJz,(const void *)&nJz);` |
|         - | 1694 | `					/* Truthy link: drop it and ask the next operand. */` |
|       136 | 1695 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        67 | 1696 | `				}` |
|       107 | 1697 | `			}` |
|        78 | 1698 | `			if( rc == SXRET_OK ){` |
|        78 | 1699 | `				sxu32 *aJz = (sxu32 *)SySetBasePtr(&sJz);` |
|        78 | 1700 | `				sxu32 nEnd = PH7_VmInstrLength(pGen->pVm);` |
|       212 | 1701 | `				for( n = 0 ; n < SySetUsed(&sJz) ; ++n ){` |
|       136 | 1702 | `					VmInstr *pFix = PH7_VmGetInstr(pGen->pVm,aJz[n]);` |
|       136 | 1703 | `					if( pFix ){` |
|       136 | 1704 | `						pFix->iP2 = nEnd;` |
|        67 | 1705 | `					}` |
|        69 | 1706 | `				}` |
|        38 | 1707 | `			}` |
|        78 | 1708 | `			SySetRelease(&sJz);` |
|        78 | 1709 | `			return rc;` |
|         - | 1710 | `		}` |
|       ! 0 | 1711 | `	}` |
|   3632025 | 1712 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|   3632025 | 1713 | `	nLhsFirst = PH7_VmInstrLength(pGen->pVm);` |
|         - | 1714 | `	/* Generate code for the left tree */` |
|   3632025 | 1715 | `	if( pNode->pLeft ){` |
|   3632025 | 1716 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|         - | 1717 | `		GenCallArgs sArgs;` |
|   3632025 | 1718 | `		int bArgsEmitted = 0;` |
|   3632025 | 1719 | ``		sxu32 nNewClassInstr = 0; /* index+1 of a `new` operand's class-name push */`` |
|   3632025 | 1720 | `		SyZero(&sArgs,sizeof(sArgs));` |
|         - | 1721 | `		{` |
|         - | 1722 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|         - | 1723 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|         - | 1724 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|         - | 1725 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|         - | 1726 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|         - | 1727 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|         - | 1728 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|         - | 1729 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
|   3632025 | 1730 | `			sxi32 iLeftFlags = iFlags;` |
|         - | 1731 | `			/* The LHS chain whose subscript reads must be QUIET (LOAD_IDX iP2=8):` |
|         - | 1732 | ``			 * `??`'s left operand, and an isset()/empty() chain's intermediate`` |
|         - | 1733 | `			 * links -- php reads both silently and for the value. */` |
|   3632025 | 1734 | `			sxu32 nQuietLhsFirst = PH7_VmInstrLength(pGen->pVm);` |
|   3632025 | 1735 | `			int bQuietLhs = 0;` |
|         - | 1736 | `			/* D1 commit 2: a deferred element/property call arg records its lvalue chain, but` |
|         - | 1737 | `			 * that chain must be CONTIGUOUS. Only propagate DEFER_ARG to the base when the base` |
|         - | 1738 | `			 * is itself a continuable lvalue — a plain variable (an undefined base auto-defers),` |
|         - | 1739 | ``			 * another subscript, or a `->` member. If the base is anything else (most importantly`` |
|         - | 1740 | ``			 * a method CALL, e.g. `$o->items()->prop` or `$r->attributes->item(0)->nodeName`),`` |
|         - | 1741 | `			 * strip DEFER so that intermediate read is a NORMAL read, not a record-mode carrier. */` |
|   3632025 | 1742 | `			if( iLeftFlags & EXPR_FLAG_DEFER_ARG ){` |
|     35793 | 1743 | `				int bContinuable = pNode->pLeft` |
|     27243 | 1744 | `					&& ( (pNode->pLeft->pOp == 0 && pNode->pLeft->xCode == PH7_CompileVariable)` |
|      9743 | 1745 | `					  \|\| (pNode->pLeft->pOp != 0 && (pNode->pLeft->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|       731 | 1746 | `					                              \|\| pNode->pLeft->pOp->iOp == EXPR_OP_ARROW)) );` |
|     17899 | 1747 | `				if( !bContinuable ){` |
|       399 | 1748 | `					iLeftFlags &= ~EXPR_FLAG_DEFER_ARG;` |
|       196 | 1749 | `				}` |
|      8936 | 1750 | `			}` |
|         - | 1751 | `			/*` |
|         - | 1752 | `			 * An isset()/empty() CHAIN reads its intermediate links for their` |
|         - | 1753 | ``			 * VALUE, not for a truth. php walks `isset($o->a->b)` by fetching`` |
|         - | 1754 | ``			 * `$o->a` in BP_VAR_IS mode -- silent, but a real read that runs`` |
|         - | 1755 | `			 * __isset AND THEN __get (or offsetExists and then offsetGet) --` |
|         - | 1756 | `			 * and only the LAST link answers the isset question. PHL gave every` |
|         - | 1757 | `			 * link the terminal context, so the intermediate pushed a bool and` |
|         - | 1758 | `` 			 * the final `->b` was a property of `true`: `isset($model->rel->id)` `` |
|         - | 1759 | `			 * was FALSE for every class with accessors, and so was` |
|         - | 1760 | ``			 * `isset($container['k']['j'])` over ArrayAccess -- a silently wrong`` |
|         - | 1761 | `			 * guard, not a diagnostic.` |
|         - | 1762 | `			 *` |
|         - | 1763 | ``			 * The intermediate context is `??`'s (PH7_MEMBER_COALESCE for a`` |
|         - | 1764 | `			 * member, LOAD_IDX iP2=8 for a subscript, patched over the emitted` |
|         - | 1765 | `			 * range below), which is exactly "silent, and the value": EMPTY's` |
|         - | 1766 | `			 * would read a shade differently, since a class declaring __get with` |
|         - | 1767 | `			 * no __isset is read through __get for an intermediate link and is` |
|         - | 1768 | `			 * NOT for a terminal isset()/empty().` |
|         - | 1769 | `			 */` |
|   3632020 | 1770 | `			if( (iLeftFlags & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY))` |
|   1820452 | 1771 | `				&& pNode->pOp && pNode->pLeft && pNode->pLeft->pOp` |
|      7388 | 1772 | `				&& GEN_IS_ACCESS_OP(pNode->pOp->iOp)` |
|       182 | 1773 | `				&& GEN_IS_ACCESS_OP(pNode->pLeft->pOp->iOp) ){` |
|       127 | 1774 | `				iLeftFlags &= ~(EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY);` |
|       127 | 1775 | `				iLeftFlags \|= EXPR_FLAG_MEMBER_COALESCE\|EXPR_FLAG_QUIET_VAR;` |
|       127 | 1776 | `				bQuietLhs = 1;` |
|        61 | 1777 | `			}` |
|   3632020 | 1778 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
|   2960791 | 1779 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|   1147669 | 1780 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|   1135755 | 1781 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|     25797 | 1782 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|   3619110 | 1783 | `			}else if( iLeftFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|         - | 1784 | ``				/* A SUBSCRIPT intermediate of an unset chain (`unset($a['k']['n'])`) keeps`` |
|         - | 1785 | `				 * the unset context — it must COW-separate the parent and must NOT vivify a` |
|         - | 1786 | `				 * missing key — but it is a READ of the container, not an unset of it. The` |
|         - | 1787 | ``				 * UNSET_BASE context says exactly that: `$a['k']` is loaded, where the`` |
|         - | 1788 | `				 * plain unset context would have removed the ELEMENT (and, for an` |
|         - | 1789 | `				 * ArrayAccess base, called offsetUnset() on the intermediate key). */` |
|       369 | 1790 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|       369 | 1791 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_UNSET_BASE;` |
|       182 | 1792 | `			}` |
|         - | 1793 | `			/* Only the OUTERMOST access of a reference SOURCE is the reference fetch;` |
|         - | 1794 | `` 			 * every container under it is php's ordinary write base (`$r =& $o->arr['k']` `` |
|         - | 1795 | ``			 * creates `arr` the way `$o->arr['k'] = v` does). So the flag never travels`` |
|         - | 1796 | `			 * down as itself -- it decays to the write-lvalue flag, which the strip just` |
|         - | 1797 | ``			 * below then applies its own `->`-intermediate rule to. */`` |
|   3632025 | 1798 | `			if( iLeftFlags & EXPR_FLAG_MEMBER_REFSRC ){` |
|       297 | 1799 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_REFSRC;` |
|       297 | 1800 | `				iLeftFlags \|= EXPR_FLAG_MEMBER_WRITE;` |
|       146 | 1801 | `			}` |
|         - | 1802 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|         - | 1803 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|         - | 1804 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|         - | 1805 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|         - | 1806 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|         - | 1807 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|         - | 1808 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
|   3632020 | 1809 | `			if( pNode->pOp` |
|   5429573 | 1810 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
|   3616480 | 1811 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|   3600899 | 1812 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|     35605 | 1813 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|     17781 | 1814 | `			}` |
|         - | 1815 | ``			/* `++`/`--` mutate their operand in place — the operand is a write`` |
|         - | 1816 | ``			 * lvalue exactly like a compound assign's (`$o->m[0]++` must tag the`` |
|         - | 1817 | ``			 * member base PH7_MEMBER_WRITE the way `$o->m[0] += 1` does: hooked`` |
|         - | 1818 | `			 * properties throw php's Indirect-modification Error, missing ones` |
|         - | 1819 | `			 * auto-vivify). The prec-18 site below handles the assign family;` |
|         - | 1820 | ``			 * `++`/`--` are unary, their operand is pLeft. */`` |
|   3632020 | 1821 | `			if( pNode->pOp` |
|   3632025 | 1822 | `				&& (pNode->pOp->iVmOp == PH7_OP_INCR \|\| pNode->pOp->iVmOp == PH7_OP_DECR) ){` |
|         - | 1823 | ``				/* `(new A)->p++` writes through a temporary exactly as `= 1` does --`` |
|         - | 1824 | ``				 * but a `$this++` is a read-modify-write php leaves to run time. */`` |
|     61573 | 1825 | `				rc = GenStateWriteTargetCheck(&(*pGen),pNode->pLeft,PH7_WTC_RMW);` |
|     61573 | 1826 | `				if( rc != SXRET_OK ){` |
|        43 | 1827 | `					return rc;` |
|         - | 1828 | `				}` |
|     61567 | 1829 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE` |
|         - | 1830 | ``					\| EXPR_FLAG_RMW_LOAD /* php warns before seeding `$undef++` */;`` |
|     30739 | 1831 | `			}` |
|         - | 1832 | ``			/* The SOURCE of a `=&` (pLeft, the operands having been swapped in`` |
|         - | 1833 | `			 * parse.c) is compiled in WRITE context by php too —` |
|         - | 1834 | ``			 * `zend_compile_var(source, BP_VAR_W, 1)` — which is what makes`` |
|         - | 1835 | ``			 * `$r =& $undef` and `$r =& $a[5]` CREATE the thing they bind to,`` |
|         - | 1836 | `			 * silently. PHL READ it, so both warned about what was missing and then` |
|         - | 1837 | `			 * refused the bind outright, leaving $r undefined as well. No` |
|         - | 1838 | `			 * RMW_LOAD: a bind does not read the source's value, and no` |
|         - | 1839 | `			 * MEMBER_WRITE: a handler-backed native property has no pointer for` |
|         - | 1840 | ``			 * php to hand out either, so `$r =& $iv->s` must keep taking the`` |
|         - | 1841 | `			 * read COPY it takes in php. */` |
|   3632019 | 1842 | `			if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_REF ){` |
|       423 | 1843 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_REFSRC;` |
|       209 | 1844 | `			}` |
|         - | 1845 | `			/* A destructuring target list that binds BY REFERENCE reads its SOURCE` |
|         - | 1846 | `			 * in write context for the same reason: the bind must reach the thing` |
|         - | 1847 | ``			 * the source NAMES. That is what keeps `[&$t] = $undef;` from warning`` |
|         - | 1848 | `			 * about what it is on the point of creating. */` |
|   3632014 | 1849 | `			if( iVmOp == PH7_OP_STORE && pNode->pRight && pNode->pRight->pStart` |
|    703304 | 1850 | `			 && (pNode->pRight->xCode == PH7_CompileList` |
|    703277 | 1851 | `			  \|\| pNode->pRight->xCode == PH7_CompileShortList)` |
|    351339 | 1852 | `			 && PH7_GenStateListSpanHasRef(pNode->pRight->pStart,pNode->pRight->pEnd) ){` |
|        45 | 1853 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_REFSRC;` |
|        22 | 1854 | `			}` |
|         - | 1855 | ``			/* `??` reads its LEFT operand in isset-context: an undefined or`` |
|         - | 1856 | `			 * UNINITIALIZED typed PROPERTY must yield the default rather than a` |
|         - | 1857 | `			 * warning/Error (php). Tag a member-access LHS so its OP_MEMBER takes` |
|         - | 1858 | `			 * the silent-lookup path (iP2 = ISSET), which still loads a present` |
|         - | 1859 | `			 * value. A SUBSCRIPT LHS is left untagged: LOAD_IDX's ISSET mode means` |
|         - | 1860 | ``			 * offsetExists (a bool), but `$o[$k] ?? d` needs the offsetGet value —`` |
|         - | 1861 | `			 * that path is already handled correctly by OP_NULLC. */` |
|   3632009 | 1862 | `			if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC && pNode->pLeft ){` |
|         - | 1863 | ``				/* php reads the ENTIRE left operand of `??` in isset-context: no`` |
|         - | 1864 | ``				 * "Undefined variable" for `$x ?? d` OR for the base of`` |
|         - | 1865 | ``				 * `$x['k'] ?? d`. QUIET_VAR silences the variable read wherever it`` |
|         - | 1866 | `				 * sits in the chain. */` |
|       547 | 1867 | `				iLeftFlags \|= EXPR_FLAG_QUIET_VAR;` |
|       547 | 1868 | `				bQuietLhs = 1;` |
|       542 | 1869 | `				if( pNode->pLeft->pOp` |
|       651 | 1870 | `					&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|       387 | 1871 | `						\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|       312 | 1872 | `						\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|         - | 1873 | `					/* A member-access LHS additionally takes OP_MEMBER's SILENT` |
|         - | 1874 | `					 * lookup so an uninitialized typed property yields the default` |
|         - | 1875 | `					 * instead of an Error. It used to borrow isset()'s context for` |
|         - | 1876 | `					 * that, which is the same mistake the comment below records for` |
|         - | 1877 | `					 * subscripts: silence is shared, but isset() context makes every` |
|         - | 1878 | ``					 * ACCESSOR answer a truth, and `$o->p ?? d` needs the accessor's`` |
|         - | 1879 | ``					 * VALUE — so `??` has its own member context. A SUBSCRIPT LHS`` |
|         - | 1880 | `					 * still takes neither: LOAD_IDX's ISSET mode means offsetExists` |
|         - | 1881 | ``					 * (a bool), while `$o[$k] ?? d` needs the offsetGet value —`` |
|         - | 1882 | `					 * OP_NULLC already handles that path. */` |
|       164 | 1883 | `					iLeftFlags \|= EXPR_FLAG_MEMBER_COALESCE;` |
|        80 | 1884 | `				}` |
|       271 | 1885 | `			}` |
|   3632009 | 1886 | `			if( iVmOp == PH7_OP_ERR_CTRL ){` |
|         - | 1887 | `				/* '@' must suppress the diagnostics raised WHILE its operand runs, so` |
|         - | 1888 | `				 * open the window here; the trailing emit below closes it (iP1 = 0). */` |
|     21856 | 1889 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_ERR_CTRL,1,0,0,0);` |
|     10905 | 1890 | `			}` |
|   3632009 | 1891 | `			if( iVmOp == PH7_OP_NEW ){` |
|         - | 1892 | ``				/* Mark the direct operand so a call node under it (`new C($a)`) keeps the`` |
|         - | 1893 | `				 * emission order OP_NEW is assembled from — see the bNewCallee branch above. */` |
|     99567 | 1894 | `				iLeftFlags \|= EXPR_FLAG_NEW_CALLEE;` |
|     49713 | 1895 | `			}` |
|   3632009 | 1896 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags\|EXPR_FLAG_RDONLY_LOAD);` |
|   3632009 | 1897 | `			if( rc == SXRET_OK && bQuietLhs ){` |
|         - | 1898 | `				/* Mark EVERY subscript read in the quiet left chain (iP2=8).` |
|         - | 1899 | `				 * Peeking at the next instruction only catches the OUTERMOST` |
|         - | 1900 | ``				 * access, so `$d['x']['y'] ?? $v` still warned for the inner one;`` |
|         - | 1901 | `				 * and a forward scan would be unsound, since an unrelated sibling` |
|         - | 1902 | ``				 * access can sit immediately before a coalesce (`f($a['k'],`` |
|         - | 1903 | ``				 * $b['j'] ?? 1)`). The compiler knows the real extent, so it marks`` |
|         - | 1904 | `				 * the range it just emitted. Write-context codes (1/3/5) belong to` |
|         - | 1905 | ``				 * `??=` and keep their meaning. */`` |
|       669 | 1906 | `				sxu32 nEnd = PH7_VmInstrLength(pGen->pVm);` |
|         - | 1907 | `				sxu32 nAt;` |
|      2815 | 1908 | `				for( nAt = nQuietLhsFirst ; nAt < nEnd ; ++nAt ){` |
|      2151 | 1909 | `					VmInstr *pFix = PH7_VmGetInstr(pGen->pVm,nAt);` |
|      2151 | 1910 | `					if( pFix && pFix->iOp == PH7_OP_LOAD_IDX && pFix->iP2 == 0 ){` |
|       303 | 1911 | `						pFix->iP2 = 8;` |
|       149 | 1912 | `					}` |
|      1078 | 1913 | `				}` |
|       332 | 1914 | `			}` |
|         - | 1915 | `		}` |
|   3632009 | 1916 | `		if( rc != SXRET_OK ){` |
|        65 | 1917 | `			return rc;` |
|         - | 1918 | `		}` |
|   3631949 | 1919 | `		if( !bIsChainOp ){` |
|         - | 1920 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|         - | 1921 | `			 * target the end of that LHS chain, which is right here. */` |
|   2396110 | 1922 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|   1196382 | 1923 | `		}` |
|   3631949 | 1924 | `		if( iVmOp == PH7_OP_CALL ){` |
|    934778 | 1925 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    934778 | 1926 | `			if( pInstr ){` |
|    934778 | 1927 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|    906546 | 1928 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|         - | 1929 | `					sxu32 nQual;` |
|    906546 | 1930 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 1931 | `					/* Prevent constant expansion but preserve the absolute flag` |
|         - | 1932 | `					 * so the later NEW handler (if any) can see it. */` |
|    906546 | 1933 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|         - | 1934 | `					/* Namespace-qualify the function name for CALL, unless the` |
|         - | 1935 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|         - | 1936 | `					 * imports — class imports must NOT affect function` |
|         - | 1937 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|         - | 1938 | `					 * before NEW; we store the original literal index in the` |
|         - | 1939 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|         - | 1940 | `					 * the unqualified name and re-qualify with class imports. */` |
|    906546 | 1941 | `					if( bAbsolute ){` |
|       141 | 1942 | `						pInstr->iP2 = (sxi32)nOrig;` |
|        73 | 1943 | `					}else{` |
|    906410 | 1944 | `						int fromImport = 0;` |
|    906410 | 1945 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|    906410 | 1946 | `						pInstr->iP2 = (sxi32)nQual;` |
|    906410 | 1947 | `						if( nQual != nOrig ){` |
|         - | 1948 | `							/* Record the original literal index in the arg map` |
|         - | 1949 | `							 * (NOT in the CALL's iP2 — that is the hasSpread` |
|         - | 1950 | `							 * flag) so the NEW handler can recover the` |
|         - | 1951 | `							 * unqualified name and re-qualify with CLASS` |
|         - | 1952 | `							 * imports. */` |
|       289 | 1953 | `							if( p3 == 0 ){` |
|       289 | 1954 | `								VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|       284 | 1955 | `									&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|       289 | 1956 | `								if( pMap ){` |
|       289 | 1957 | `									SyZero(pMap, sizeof(VmCallArgMap));` |
|       289 | 1958 | `									p3 = (void *)pMap;` |
|       142 | 1959 | `								}` |
|       142 | 1960 | `							}` |
|       289 | 1961 | `							if( p3 ){` |
|       289 | 1962 | `								((VmCallArgMap *)p3)->nOrigNameLit = nOrig + 1;` |
|       289 | 1963 | `								if( !fromImport ){` |
|         - | 1964 | `									/* Mark as namespace-qualified */` |
|       259 | 1965 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|       127 | 1966 | `								}` |
|       142 | 1967 | `							}` |
|       142 | 1968 | `						}` |
|         - | 1969 | `					}` |
|    480531 | 1970 | `				}else if( (pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */` |
|     25188 | 1971 | `						&& !bNewCallee` |
|     22153 | 1972 | `						&& !(pNode->pLeft && (pNode->pLeft->iFlags & EXPR_NODE_PARENS)))` |
|     17164 | 1973 | `					\|\| pInstr->iOp == PH7_OP_NEW ){` |
|         - | 1974 | `					/* Method call,flag that. But NOT when the callee was an explicitly` |
|         - | 1975 | ``					 * PARENTHESISED member access: `($o->p)(...)` / `($o::$p)(...)` invokes`` |
|         - | 1976 | `					 * the VALUE of the property (php's variable-invocation), so the OP_MEMBER` |
|         - | 1977 | `					 * must stay a plain property READ (leaving the callable on the stack for` |
|         - | 1978 | `					 * OP_CALL to invoke) rather than being rewritten into a method-name` |
|         - | 1979 | ``					 * resolution — the parens are exactly what distinguishes `($o->p)()` from`` |
|         - | 1980 | ``					 * the method call `$o->p()`. */`` |
|     22121 | 1981 | `					pInstr->iP2 = 1;` |
|         - | 1982 | ``					/* …and a `new`'s operand is the third shape that is NOT a method`` |
|         - | 1983 | ``					 * call: after `new`, php's class expression is a `new_variable`,`` |
|         - | 1984 | `					 * which has no call in it, so the parentheses that follow are` |
|         - | 1985 | ``					 * always the CONSTRUCTOR's. `new $config->defType()` -- how`` |
|         - | 1986 | `					 * nette/di spells every service it builds -- read the property as` |
|         - | 1987 | ``					 * a METHOD and died on `Call to undefined method`` |
|         - | 1988 | ``					 * stdClass::defType()`. Same exception, same reason, as the`` |
|         - | 1989 | `					 * parenthesised member above: the OP_MEMBER stays a property READ` |
|         - | 1990 | `					 * and leaves the class NAME for OP_NEW. */` |
|         - | 1991 | `					/* This OP_MEMBER is where php SCREENS a method call -- an undefined` |
|         - | 1992 | `					 * or inaccessible method, a class that is not there -- and php` |
|         - | 1993 | `					 * reports that refusal at the line the CALL BEGINS on. The emitter` |
|         - | 1994 | `					 * stamped it with the token the generator was standing on, which` |
|         - | 1995 | `					 * for a call spanning several lines is its closing ')'. Same rule,` |
|         - | 1996 | `					 * same source, as the OP_CALL/OP_NEW stamp further down. */` |
|     22121 | 1997 | `					if( pInstr->iOp == PH7_OP_MEMBER && pNode->pStart ){` |
|     22113 | 1998 | `						pInstr->nLine = pNode->pStart->nLine;` |
|     11035 | 1999 | `					}` |
|         - | 2000 | ``					/* A static call with a DYNAMIC method name (`C::$m(...)`): the`` |
|         - | 2001 | ``					 * static-`::` codegen folded the variable NAME into OP_MEMBER->p3`` |
|         - | 2002 | ``					 * as if it were a static-PROPERTY read (`C::$m`), but in a CALL the`` |
|         - | 2003 | `					 * method name is the variable's VALUE. Rebuild the sequence` |
|         - | 2004 | `					 * [class, OP_LOAD $m -> value, OP_MEMBER(static, method)] so the` |
|         - | 2005 | `					 * dynamic name is read off the stack, matching the instance` |
|         - | 2006 | ``					 * (`$o->$m()`) path. iP1==1 marks a static member. */`` |
|     22121 | 2007 | `					if( pInstr->iOp == PH7_OP_MEMBER && pInstr->iP1 == 1 && pInstr->p3 ){` |
|        21 | 2008 | `						void *pDynName = pInstr->p3;` |
|        21 | 2009 | `						(void)PH7_VmPopInstr(pGen->pVm);` |
|        21 | 2010 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,pDynName,0);` |
|        21 | 2011 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_MEMBER,1,PH7_MEMBER_METHOD,0,0);` |
|        10 | 2012 | `					}` |
|     11039 | 2013 | `				}` |
|    466383 | 2014 | `			}` |
|         - | 2015 | `			/* The callee is resolved; NOW emit the arguments. php's order — the callee` |
|         - | 2016 | `			 * first, at its INIT_FCALL / INIT_METHOD_CALL, and the arguments only after` |
|         - | 2017 | ``			 * it — is what makes `(new C)->priv(boom())` report php's `Call to private`` |
|         - | 2018 | `` 			 * method` instead of whatever the argument threw, and what keeps `$o?->m(f())` `` |
|         - | 2019 | ``			 * from running `f()` on a null receiver. It also puts the callee in reach of`` |
|         - | 2020 | `			 * the argument ops one opcode EARLIER than OP_CALL.` |
|         - | 2021 | `			 *` |
|         - | 2022 | `			 * The stack that leaves here is therefore [callee][args…] — the mirror of the` |
|         - | 2023 | `			 * layout OP_CALL's whole dispatch is written against (the method-name pair` |
|         - | 2024 | `			 * below the arguments, the spread runs counted down from the top, the` |
|         - | 2025 | `			 * deferred-argument re-walk). OP_ROT_CALLEE turns the region back over just` |
|         - | 2026 | `			 * before the call, so nothing downstream of it changes. */` |
|    934778 | 2027 | `			if( !bArgsEmitted ){` |
|         - | 2028 | `				int bTwoSlot;` |
|    934778 | 2029 | `				sArgs.p3 = p3; /* the namespace map built just above, if any */` |
|         - | 2030 | `				/* A METHOD callee leaves TWO slots — [receiver][method name] — which` |
|         - | 2031 | `				 * OP_CALL reads as one callee (the receiver answers $this and the` |
|         - | 2032 | `				 * late-static-binding class); anything else leaves one. The instruction` |
|         - | 2033 | `				 * just emitted is what decides it. */` |
|    934778 | 2034 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    945839 | 2035 | `				bTwoSlot = pInstr && pInstr->iOp == PH7_OP_MEMBER` |
|    477482 | 2036 | `					&& pInstr->iP2 == PH7_MEMBER_METHOD` |
|         - | 2037 | `					/* …unless the member NAME was folded into p3 rather than pushed:` |
|         - | 2038 | `					 * that shape pushes the target alone, so the op leaves one slot,` |
|         - | 2039 | `					 * which is the same distinction vm_ops_oo.c makes before popping. */` |
|   1403163 | 2040 | `					&& pInstr->p3 == 0;` |
|         - | 2041 | `				/* Screen the callee HERE, where php screens it: an undefined function, a` |
|         - | 2042 | `				 * callable string/array naming nothing, a value that is not callable at` |
|         - | 2043 | `				 * all. A METHOD callee needs none — its OP_MEMBER just did it, against` |
|         - | 2044 | `				 * the entry it chose. An FCC needs none either: OP_LOAD_FCC runs the same` |
|         - | 2045 | `				 * screen, and nothing runs before it. And with NO arguments the call` |
|         - | 2046 | `				 * itself is already the first thing to happen, so there is nothing to` |
|         - | 2047 | `				 * order and no reason to pay for a second resolution. */` |
|         - | 2048 | `				{` |
|    934778 | 2049 | `					ph7_expr_node **apCallArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|    934778 | 2050 | `					sxi32 nCallArg = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|   1009515 | 2051 | `					int bNodeFcc = nCallArg == 1 && apCallArg[0]` |
|   1206791 | 2052 | `						&& (apCallArg[0]->iFlags & EXPR_NODE_FCC);` |
|    934778 | 2053 | `					if( bNewCallee ){` |
|         - | 2054 | ``						/* A `new`'s operand: the screen is OP_NEW itself, run with no`` |
|         - | 2055 | `						 * arguments on the stack (iP1 = -1). It asks every refusal the` |
|         - | 2056 | `						 * real pass asks and leaves the class name standing, so the two` |
|         - | 2057 | `						 * cannot disagree. Record where that push is — the NEW codegen` |
|         - | 2058 | `						 * used to find it one instruction behind the trailing OP_CALL,` |
|         - | 2059 | `						 * and the argument list now sits in between. */` |
|     96599 | 2060 | `						nNewClassInstr = PH7_VmInstrLength(pGen->pVm);` |
|     96599 | 2061 | `						if( nCallArg > 0 ){` |
|     93787 | 2062 | `							PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,-1,0,0,0);` |
|     46830 | 2063 | `						}` |
|    886415 | 2064 | `					}else if( nCallArg > 0 && !bTwoSlot && !bNodeFcc ){` |
|         - | 2065 | ``						/* This screen is where a `Call to undefined function` is`` |
|         - | 2066 | `						 * raised for a call that HAS arguments, and php reports such a` |
|         - | 2067 | `						 * call at the line the CALLEE is written on -- not at the` |
|         - | 2068 | `						 * closing parenthesis, which is where the emitter's token` |
|         - | 2069 | `						 * cursor has reached by now. The callee's own load is the` |
|         - | 2070 | ``						 * instruction immediately behind (`pInstr`, peeked just above`` |
|         - | 2071 | `						 * for bTwoSlot), so its line is the one to carry. A call with` |
|         - | 2072 | `						 * no arguments needs nothing: OP_CALL itself is then the first` |
|         - | 2073 | `						 * thing to run and already reports the callee's line. */` |
|    794239 | 2074 | `						sxu32 nInitIdx = PH7_VmInstrLength(pGen->pVm);` |
|   1192214 | 2075 | `						sxu32 nCalleeLine = pNode->pStart` |
|    794234 | 2076 | `							? pNode->pStart->nLine : (pInstr ? pInstr->nLine : 0);` |
|    794303 | 2077 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL_INIT,0,` |
|    794234 | 2078 | `							(p3 && ((VmCallArgMap *)p3)->bIsNamespaced) ? 1 : 0,0,0);` |
|    794239 | 2079 | `						if( nCalleeLine ){` |
|    794239 | 2080 | `							VmInstr *pInitInstr = PH7_VmGetInstr(pGen->pVm,nInitIdx);` |
|    794239 | 2081 | `							if( pInitInstr ){` |
|    794239 | 2082 | `								pInitInstr->nLine = nCalleeLine;` |
|    396259 | 2083 | `							}` |
|    396259 | 2084 | `						}` |
|    396259 | 2085 | `					}` |
|         - | 2086 | `				}` |
|    934778 | 2087 | `				rc = GenStateEmitCallArgs(&(*pGen),pNode,iFlags,&sArgs);` |
|    934778 | 2088 | `				if( rc != SXRET_OK ){` |
|        13 | 2089 | `					return rc;` |
|         - | 2090 | `				}` |
|    934768 | 2091 | `				iP1 = sArgs.iP1;` |
|    934768 | 2092 | `				iP2 = sArgs.iP2;` |
|    934768 | 2093 | `				p3  = sArgs.p3;` |
|    934768 | 2094 | `				bFcc = sArgs.bFcc;` |
|    934768 | 2095 | `				if( iP1 > 0 \|\| iP2 ){` |
|   1344496 | 2096 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_ROT_CALLEE,iP1,` |
|    896947 | 2097 | `						(iP2 ? PH7_ROT_SPREAD : 0) \| (bTwoSlot ? PH7_ROT_TWOSLOT : 0),0,0);` |
|    447544 | 2098 | `				}` |
|    934768 | 2099 | `				if( bNewCallee && nNewClassInstr > 0 ){` |
|     96599 | 2100 | `					if( p3 == 0 ){` |
|      2775 | 2101 | `						VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      2770 | 2102 | `							&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|      2775 | 2103 | `						if( pMap ){` |
|      2775 | 2104 | `							SyZero(pMap,sizeof(VmCallArgMap));` |
|      2775 | 2105 | `							p3 = (void *)pMap;` |
|      1385 | 2106 | `						}` |
|      1385 | 2107 | `					}` |
|     96599 | 2108 | `					if( p3 ){` |
|     96599 | 2109 | `						((VmCallArgMap *)p3)->nNewClassInstr = nNewClassInstr;` |
|     48231 | 2110 | `					}` |
|     48231 | 2111 | `				}` |
|    466383 | 2112 | `			}` |
|   3163554 | 2113 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|         - | 2114 | `			ph7_expr_node **apNode;` |
|         - | 2115 | `			sxi32 n;` |
|    265471 | 2116 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|         - | 2117 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|         - | 2118 | `				\|EXPR_FLAG_LOAD_IDX_UNSET_BASE` |
|         - | 2119 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE` |
|         - | 2120 | `				\|EXPR_FLAG_MEMBER_COALESCE` |
|         - | 2121 | `				\|EXPR_FLAG_QUIET_VAR\|EXPR_FLAG_RMW_LOAD\|EXPR_FLAG_DEFER_ARG);` |
|         - | 2122 | `			/* Recurse and generate bytecodes for array index */` |
|    265471 | 2123 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|    462210 | 2124 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|    196744 | 2125 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    196744 | 2126 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|    196744 | 2127 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 2128 | `					return rc;` |
|         - | 2129 | `				}` |
|         - | 2130 | `				/* Each subscript index is an independent nullsafe scope. */` |
|    196744 | 2131 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|     98240 | 2132 | `			}` |
|    265471 | 2133 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|    196744 | 2134 | `				iP1 = 1; /* Node have an index associated with it */` |
|     98240 | 2135 | `			}else{` |
|         - | 2136 | ``				/* `[]` names the element a WRITE is about to create, so php allows it`` |
|         - | 2137 | `				 * only where a write lands: an assignment target (plain, compound,` |
|         - | 2138 | ``				 * `=&`, a list()/foreach target) and a by-reference argument. Every`` |
|         - | 2139 | `				 * other placement is a COMPILE error there — PHL accepted them all and` |
|         - | 2140 | ``				 * answered NULL after a PH7-worded notice, so `$x = $a[];`,`` |
|         - | 2141 | ``				 * `isset($a[])` and `unset($a[][0])` were silent no-ops on source php`` |
|         - | 2142 | `				 * refuses to run. A call ARGUMENT is the one shape php also leaves to` |
|         - | 2143 | `				 * runtime (it cannot know the parameter's by-ref-ness at compile time),` |
|         - | 2144 | `				 * which is what DEFER_ARG marks. */` |
|     68732 | 2145 | `				if( iFlags & (EXPR_FLAG_LOAD_IDX_UNSET\|EXPR_FLAG_LOAD_IDX_UNSET_BASE) ){` |
|       ! 0 | 2146 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,` |
|       ! 0 | 2147 | `						pNode->pStart ? pNode->pStart->nLine : 0,` |
|         - | 2148 | `						"Cannot use [] for unsetting");` |
|       ! 0 | 2149 | `					return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 2150 | `				}` |
|     68732 | 2151 | `				if( (iFlags & (EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_DEFER_ARG)) == 0 ){` |
|       ! 0 | 2152 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,` |
|       ! 0 | 2153 | `						pNode->pStart ? pNode->pStart->nLine : 0,` |
|         - | 2154 | `						"Cannot use [] for reading");` |
|       ! 0 | 2155 | `					return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 2156 | `				}` |
|         - | 2157 | `			}` |
|    265471 | 2158 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|         - | 2159 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|     14127 | 2160 | `				iP2 = 4;` |
|    258401 | 2161 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|         - | 2162 | `				/* offsetUnset for ArrayAccess; for an array, remove the ELEMENT. */` |
|       247 | 2163 | `				iP2 = 5;` |
|    251228 | 2164 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET_BASE ){` |
|         - | 2165 | `				/* An unset chain's intermediate container: read it, but with the` |
|         - | 2166 | `				 * unset context's COW-separate and no-vivify rules. */` |
|        26 | 2167 | `				iP2 = 10;` |
|    251095 | 2168 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|         - | 2169 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|         - | 2170 | `				 * short-circuit on missing keys without invoking offsetGet` |
|         - | 2171 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|        57 | 2172 | `				iP2 = 6;` |
|    251057 | 2173 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|         - | 2174 | `				/* Create an empty entry when the desired index is not found.` |
|         - | 2175 | ``				 * A read-modify-write target (`$a[k] += v`, `$a[k]++`) creates it`` |
|         - | 2176 | `				 * the same way but READS it first, so php warns about the missing` |
|         - | 2177 | `				 * key before seeding it — the RMW context says which of the two` |
|         - | 2178 | `				 * this is (VM_IDX_CTX_RMW). The flag rides the whole LHS chain, so` |
|         - | 2179 | `				 * an intermediate level gets it too, as php's BP_VAR_RW fetch does. */` |
|    157922 | 2180 | `				iP2 = (iFlags & EXPR_FLAG_RMW_LOAD) ? VM_IDX_CTX_RMW : 1;` |
|    171964 | 2181 | `			}else if( iFlags & EXPR_FLAG_DEFER_ARG ){` |
|         - | 2182 | `				/* D1 commit 2: deferred by-ref/by-value element arg. Behaves as a read but,` |
|         - | 2183 | `				 * on a lookup miss, records the lvalue path instead of warning; OP_CALL` |
|         - | 2184 | `				 * re-walks it in vivify (by-ref) or read+warn (by-value) mode. */` |
|     15243 | 2185 | `				iP2 = 9;` |
|      7613 | 2186 | `			}` |
|   2564260 | 2187 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         - | 2188 | `			/* POP the left node — its answer is dropped exactly as a statement's is,` |
|         - | 2189 | `			 * so a #[\NoDiscard] callee warns for it too (php warns for every` |
|         - | 2190 | ``			 * element of a `for` clause list, not just the last). */`` |
|        14 | 2191 | `			GenStateMarkDiscardedCall(&(*pGen));` |
|        14 | 2192 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         6 | 2193 | `		}` |
|   1813091 | 2194 | `	}` |
|   3631939 | 2195 | `	rc = SXRET_OK;` |
|   3631939 | 2196 | `	nJmpIdx = 0;` |
|         - | 2197 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|         - | 2198 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|         - | 2199 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|   3631939 | 2200 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|      4414 | 2201 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      4414 | 2202 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|      4360 | 2203 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|      4360 | 2204 | `			int isSpecial = 0;` |
|      4360 | 2205 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|      4360 | 2206 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|      4360 | 2207 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|      4355 | 2208 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|      3990 | 2209 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      2005 | 2210 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|       605 | 2211 | `					isSpecial = 1;` |
|       300 | 2212 | `				}` |
|      2173 | 2213 | `			}` |
|      4360 | 2214 | `			pInstr->iP1 = 0;` |
|         - | 2215 | ``			/* A leading `\` (NSSEP) makes the class name ABSOLUTE: it names the`` |
|         - | 2216 | `			 * global class, so it must NOT be re-qualified with the current namespace.` |
|         - | 2217 | ``			 * `\Closure::bind(...)` inside `namespace X` is Closure, not X\Closure.`` |
|         - | 2218 | ``			 * (Multi-component `\A\B::m` already resolves absolutely because its`` |
|         - | 2219 | ``			 * literal keeps a backslash; the single-component `\Closure` lost it.) */`` |
|         - | 2220 | `			{` |
|      6533 | 2221 | `				int bAbsolute = (pNode->pLeft && pNode->pLeft->pStart` |
|      6537 | 2222 | `					&& (pNode->pLeft->pStart->nType & PH7_TK_NSSEP));` |
|      4360 | 2223 | `				if( !isSpecial && !bAbsolute ){` |
|      3712 | 2224 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      1849 | 2225 | `				}` |
|         - | 2226 | `			}` |
|         - | 2227 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|         - | 2228 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|      4360 | 2229 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|      3760 | 2230 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|      3760 | 2231 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|       240 | 2232 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|       151 | 2233 | `					return SXRET_OK;` |
|         - | 2234 | `				}` |
|      1800 | 2235 | `			}` |
|      2100 | 2236 | `		}` |
|      2127 | 2237 | `	}` |
|   3631788 | 2238 | `	if( iVmOp == PH7_OP_IS_A && nLhsFirst < PH7_VmInstrLength(pGen->pVm)` |
|     14083 | 2239 | `	 && GenStateInstanceofFoldsLhs(&(*pGen),nLhsFirst) ){` |
|         - | 2240 | `` 		/* php never even compiles the class operand when the SUBJECT of `instanceof` `` |
|         - | 2241 | `		 * is a compile-time constant: zend_compile_instanceof folds the whole` |
|         - | 2242 | `		 * expression to FALSE the moment its left operand comes back IS_CONST, so` |
|         - | 2243 | `` 		 * `5 instanceof $x` is false whatever $x holds -- while `$v instanceof $x` `` |
|         - | 2244 | `		 * with the same 5 in $v reaches the runtime opcode and is refused when $x is` |
|         - | 2245 | `		 * neither an object nor a string. The two spellings really do answer` |
|         - | 2246 | `		 * differently, so the screen added to OP_IS_A has to be told which one this` |
|         - | 2247 | `		 * is, and GenStateInstanceofFoldsLhs reads it off the subject's own` |
|         - | 2248 | `		 * instructions. */` |
|         - | 2249 | `		ph7_value *pFalse;` |
|         - | 2250 | `		sxu32 nFalseIdx;` |
|         - | 2251 | `		/* The subject's own instructions go with it: php frees the folded operand` |
|         - | 2252 | `		 * (zend_do_free) rather than leaving it to be computed and dropped. */` |
|        45 | 2253 | `		while( PH7_VmInstrLength(pGen->pVm) > nLhsFirst ){` |
|        29 | 2254 | `			(void)PH7_VmPopInstr(pGen->pVm);` |
|         1 | 2255 | `		}` |
|        17 | 2256 | `		pFalse = PH7_ReserveConstObj(pGen->pVm,&nFalseIdx);` |
|        17 | 2257 | `		if( pFalse == 0 ){` |
|       ! 0 | 2258 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 | 2259 | `			return SXERR_ABORT;` |
|         - | 2260 | `		}` |
|        17 | 2261 | `		PH7_MemObjInitFromBool(pGen->pVm,pFalse,0);` |
|        17 | 2262 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,(sxi32)nFalseIdx,0,0);` |
|        17 | 2263 | `		return SXRET_OK;` |
|         - | 2264 | `	}` |
|         - | 2265 | `	/* Generate code for the right tree */` |
|   3631777 | 2266 | `	if( pNode->pRight ){` |
|   2080931 | 2267 | `		if( iVmOp == PH7_OP_LAND ){` |
|         - | 2268 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    115038 | 2269 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|   2023335 | 2270 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|         - | 2271 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|     81052 | 2272 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|   1925320 | 2273 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|         - | 2274 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|       547 | 2275 | `			iVmOp = 0; /* No binary operator to emit */` |
|       547 | 2276 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|   1884660 | 2277 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|         - | 2278 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|         - | 2279 | `			 * the entire containing postfix chain to null. The jump target is` |
|         - | 2280 | `			 * patched later by the innermost non-chain ancestor (or by` |
|         - | 2281 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|         - | 2282 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|         - | 2283 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|       164 | 2284 | `			sxu32 nNsJmp = 0;` |
|       164 | 2285 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|       164 | 2286 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|   1884228 | 2287 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */` |
|   1519096 | 2288 | ``			\|\| pNode->pOp->iOp == EXPR_OP_REF /* `=&` reference bind */ ){`` |
|         - | 2289 | `` 			/* The lvalue is the RIGHT operand (prec-18 ops are right-associative; `=&` `` |
|         - | 2290 | `			 * swaps its operands in parse.c so its target is pRight too). Mark it a write` |
|         - | 2291 | `			 * target so a missing base (the container of a subscript-write, or a bare` |
|         - | 2292 | `` 			 * `$o->p`) is auto-created — PHP auto-vivifies on a plain write AND on a `=&` `` |
|         - | 2293 | ``			 * bind (`$a[0] =& $x` creates $a as [0 => &$x], it does not warn). */`` |
|         - | 2294 | `			/* php's compile-time write-target rules first ($this, a temporary base,` |
|         - | 2295 | `			 * the call that is the target itself). A COMPOUND assignment is a` |
|         - | 2296 | `			 * read-modify-write: php's $this rule does not reach it. */` |
|    755777 | 2297 | `			rc = GenStateWriteTargetCheck(&(*pGen),pNode->pRight,` |
|    377396 | 2298 | `				(iVmOp == PH7_OP_STORE \|\| pNode->pOp->iOp == EXPR_OP_REF)` |
|         - | 2299 | `					? 0 : PH7_WTC_RMW);` |
|    729516 | 2300 | `			if( rc != SXRET_OK ){` |
|        26 | 2301 | `				return rc;` |
|         - | 2302 | `			}` |
|    729494 | 2303 | `			if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|         - | 2304 | `				/* php compiles a reference SOURCE in write context too` |
|         - | 2305 | `` 				 * (`zend_compile_var(source, BP_VAR_W, 1)`), so `$r =& (new A)->p` `` |
|         - | 2306 | ``				 * and `$r =& (clone $o)->p` are the same two compile refusals a`` |
|         - | 2307 | `				 * write to them would be. Only the call-as-target question is not` |
|         - | 2308 | ``				 * asked here: `$r =& f()` is legal. The operands were swapped in`` |
|         - | 2309 | `				 * parse.c, so the source is pLeft. */` |
|       419 | 2310 | `				rc = GenStateWriteTargetCheck(&(*pGen),pNode->pLeft,PH7_WTC_REFSRC);` |
|       419 | 2311 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 2312 | `					return rc;` |
|         - | 2313 | `				}` |
|       207 | 2314 | `			}` |
|    729494 | 2315 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|    729494 | 2316 | `			if( iVmOp != PH7_OP_STORE && pNode->pOp->iOp != EXPR_OP_REF ){` |
|         - | 2317 | ``				/* COMPOUND assignment (`.=`, `+=`, ...) READS the target first, so`` |
|         - | 2318 | ``				 * php warns when it is undefined and then seeds it; a plain `=` and a`` |
|         - | 2319 | ``				 * `=&` rebind write without reading the target and stay silent. */`` |
|     25864 | 2320 | `				iFlags \|= EXPR_FLAG_RMW_LOAD;` |
|     12913 | 2321 | `			}` |
|    364238 | 2322 | `		}` |
|   2080909 | 2323 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|         - | 2324 | `		/* The RIGHT operand is never the reference source: for an assignment it is the` |
|         - | 2325 | `		 * TARGET (the operands were swapped), and for a member access it is the property` |
|         - | 2326 | ``		 * NAME -- and a name written as an expression (`$r =& $o->{$a->b}`) would`` |
|         - | 2327 | `		 * otherwise be compiled as a write-context fetch of its own. */` |
|   2080909 | 2328 | `		iRhsFlags = iFlags & ~EXPR_FLAG_MEMBER_REFSRC;` |
|   2080904 | 2329 | `		if( iVmOp == PH7_OP_STORE && pNode->pRight` |
|    703237 | 2330 | `		 && (pNode->pRight->xCode == PH7_CompileList` |
|    703205 | 2331 | `		  \|\| pNode->pRight->xCode == PH7_CompileShortList) ){` |
|         - | 2332 | ``			/* A destructuring target list may bind BY REFERENCE (`[&$t] = $src`),`` |
|         - | 2333 | `			 * and php asks at COMPILE time whether the source can hold one --` |
|         - | 2334 | ``			 * `zend_is_variable_or_call`, which takes a variable, a property, a`` |
|         - | 2335 | `			 * static property, a subscript and a CALL, and refuses everything else` |
|         - | 2336 | ``			 * with `Cannot assign reference to non referenceable value`. The list`` |
|         - | 2337 | `			 * body cannot ask: by the time it emits a bind the source is an` |
|         - | 2338 | `			 * anonymous value on the stack. Carry the answer to it. */` |
|       349 | 2339 | `			sxi8 bSavedSrcRef = pGen->bListSrcNotRef;` |
|       349 | 2340 | `			pGen->bListSrcNotRef = (sxi8)!GenStateNodeIsRefSource(pNode->pLeft);` |
|       349 | 2341 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iRhsFlags\|EXPR_FLAG_RDONLY_LOAD);` |
|       349 | 2342 | `			pGen->bListSrcNotRef = bSavedSrcRef;` |
|       177 | 2343 | `		}else{` |
|   2080565 | 2344 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iRhsFlags\|EXPR_FLAG_RDONLY_LOAD);` |
|         - | 2345 | `		}` |
|   2080909 | 2346 | `		if( !bIsChainOp ){` |
|         - | 2347 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|         - | 2348 | `			 * operator instruction is emitted. */` |
|   2045455 | 2349 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|   1021304 | 2350 | `		}` |
|   2080909 | 2351 | `		if( iVmOp == PH7_OP_STORE ){` |
|    703237 | 2352 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|    703178 | 2353 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|         - | 2354 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|         - | 2355 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|         - | 2356 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|         - | 2357 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|         - | 2358 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|         - | 2359 | `				 */` |
|       349 | 2360 | `				iVmOp = 0;` |
|    703065 | 2361 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|    702893 | 2362 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 2363 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|      1913 | 2364 | `					iP2 = 1;` |
|       959 | 2365 | `				}else{` |
|    700985 | 2366 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 2367 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|    157218 | 2368 | `						iVmOp = PH7_OP_STORE_IDX;` |
|    157218 | 2369 | `						iP1 = pInstr->iP1;` |
|     78503 | 2370 | `					}else{` |
|    543772 | 2371 | `						p3 = pInstr->p3;` |
|         - | 2372 | `					}` |
|         - | 2373 | `					/* POP the last dynamic load instruction */` |
|    700985 | 2374 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         - | 2375 | `				}` |
|    350959 | 2376 | `			}` |
|   1728803 | 2377 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|         - | 2378 | `			/* php records at COMPILE time whether the reference SOURCE was written` |
|         - | 2379 | ``			 * as a CALL (ZEND_RETURNS_FUNCTION), so the bind can raise `Only`` |
|         - | 2380 | ``			 * variables should be assigned by reference` when the callee turns out`` |
|         - | 2381 | `` 			 * not to return by reference. It is the direct call only: `$r =& f()` `` |
|         - | 2382 | ``			 * warns where `$r =& f()[0]` and `$r =& f()->p` are silent. The operands`` |
|         - | 2383 | `			 * were swapped in parse.c, so the source is pLeft. */` |
|       398 | 2384 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
|       331 | 2385 | `			 && pNode->pLeft->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|        48 | 2386 | `				iP1 \|= PH7_STOREREF_CALLSRC;` |
|        22 | 2387 | `			}` |
|         - | 2388 | `			/* Peek first: a member LHS ($o->p =& $x, C::$s =& $x) keeps its` |
|         - | 2389 | `			 * OP_MEMBER in place (it resolves + stashes the target slot at` |
|         - | 2390 | `			 * runtime), unlike the variable/array shapes which fold their load` |
|         - | 2391 | `			 * away. Mirrors the normal member-store fold above (iP2 kept-MEMBER). */` |
|       403 | 2392 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|       403 | 2393 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 2394 | `				/* Tag the member as a reference target and flag STORE_REF (iP2=1)` |
|         - | 2395 | `				 * to take the member-rebind path in the VM. */` |
|        69 | 2396 | `				pInstr->iP2 = PH7_MEMBER_REF_TARGET;` |
|        69 | 2397 | `				iP2 = 1;` |
|        36 | 2398 | `			}else{` |
|       337 | 2399 | `				pInstr = PH7_VmPopInstr(pGen->pVm);` |
|       337 | 2400 | `				if( pInstr ){` |
|       337 | 2401 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 2402 | `						/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|         - | 2403 | `						 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|         - | 2404 | `						 */` |
|        73 | 2405 | `						iVmOp = PH7_OP_STORE_IDX_REF;` |
|        73 | 2406 | `						iP1 = pInstr->iP1 \| (iP1 & PH7_STOREREF_CALLSRC);` |
|        73 | 2407 | `						iP2 = pInstr->iP2;` |
|        73 | 2408 | `						p3  = pInstr->p3;` |
|        38 | 2409 | `					}else{` |
|       267 | 2410 | `						p3 = pInstr->p3;` |
|         - | 2411 | `					}` |
|       166 | 2412 | `				}` |
|         - | 2413 | `			}` |
|       199 | 2414 | `		}` |
|   1039012 | 2415 | `	}` |
|   3631750 | 2416 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|     51198 | 2417 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|         - | 2418 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|         - | 2419 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|       142 | 2420 | `		iVmOp = 0;` |
|        69 | 2421 | `	}` |
|   3631755 | 2422 | `	if( iVmOp > 0 ){` |
|   3630719 | 2423 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|     61567 | 2424 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|         - | 2425 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|      6792 | 2426 | `				iP1 = 1;` |
|      3394 | 2427 | `			}` |
|   3599896 | 2428 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|         - | 2429 | `			/* Namespace-qualify the class name for NEW */ {` |
|     99425 | 2430 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|     99425 | 2431 | `				VmInstr *pCallInstr = 0;` |
|     99425 | 2432 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|     96599 | 2433 | `					VmCallArgMap *pNewMap = (VmCallArgMap *)pPeek->p3;` |
|     96599 | 2434 | `					pCallInstr = pPeek;` |
|         - | 2435 | `` 					/* The class-name push sits one instruction back only when this `new` `` |
|         - | 2436 | `					 * takes no arguments; with an argument list the reorder puts the whole` |
|         - | 2437 | `					 * list (and its screen and rotation) in between, so the call node` |
|         - | 2438 | `					 * recorded where the push is. */` |
|    144962 | 2439 | `					pPeek = (pNewMap && pNewMap->nNewClassInstr > 0)` |
|     96594 | 2440 | `						? PH7_VmGetInstr(pGen->pVm,pNewMap->nNewClassInstr - 1)` |
|     48363 | 2441 | `						: PH7_VmPeekNextInstr(pGen->pVm);` |
|     48231 | 2442 | `				}` |
|     99425 | 2443 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|     99301 | 2444 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 2445 | `					sxu32 nLitForClass;` |
|     99301 | 2446 | `					VmCallArgMap *pCallNsMap = pCallInstr ? (VmCallArgMap *)pCallInstr->p3 : 0;` |
|         - | 2447 | `					/* If the CALL handler qualified the name with FUNCTION` |
|         - | 2448 | `					 * imports, recover the original literal (recorded in the` |
|         - | 2449 | `					 * arg map — OP_CALL's iP2 is the hasSpread flag, and` |
|         - | 2450 | `` 					 * misreading it as a literal index made `new C(...$args)` `` |
|         - | 2451 | `					 * fatal with "Class ' ' is not defined") and re-qualify` |
|         - | 2452 | `					 * with class imports. */` |
|     99301 | 2453 | `					if( pCallNsMap && pCallNsMap->nOrigNameLit > 0 ){` |
|       102 | 2454 | `						nLitForClass = pCallNsMap->nOrigNameLit - 1;` |
|        53 | 2455 | `					}else{` |
|     99203 | 2456 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|         - | 2457 | `					}` |
|     99301 | 2458 | `					pPeek->iP1 = 0;` |
|     99301 | 2459 | `					if( !bAbsolute ){` |
|         - | 2460 | `						/* self/static/parent are resolved at runtime against the` |
|         - | 2461 | `						 * current class — never namespace-qualify them (else` |
|         - | 2462 | ``						 * `new self` in namespace N becomes "N\self"). Mirrors the`` |
|         - | 2463 | `						 * instanceof (IS_A) guard below. */` |
|     99217 | 2464 | `						ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nLitForClass);` |
|     99217 | 2465 | `						int isSpecialNew = 0;` |
|     99217 | 2466 | `						if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     99217 | 2467 | `							const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     99217 | 2468 | `							sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     99212 | 2469 | `							if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     99453 | 2470 | `								(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     49786 | 2471 | `								(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|        56 | 2472 | `								isSpecialNew = 1;` |
|        26 | 2473 | `							}` |
|     49538 | 2474 | `						}` |
|     99217 | 2475 | `						if( isSpecialNew ){` |
|        56 | 2476 | `							pPeek->iP2 = (sxi32)nLitForClass;` |
|        30 | 2477 | `						}else{` |
|     99165 | 2478 | `							pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|         - | 2479 | `						}` |
|     49543 | 2480 | `					}else{` |
|        89 | 2481 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|         - | 2482 | `					}` |
|     49580 | 2483 | `				}` |
|         - | 2484 | `			}` |
|     99425 | 2485 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     99425 | 2486 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|         - | 2487 | `				VmInstr *pPrev;` |
|         - | 2488 | `				int bPrevMember;` |
|     96599 | 2489 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|         - | 2490 | `				/* "Was the callee a MEMBER access?" — which, once the reorder puts a` |
|         - | 2491 | `				 * rotation between the callee and its call, is the question the rotation` |
|         - | 2492 | `				 * already answers (a method callee is the two-slot one). */` |
|    146368 | 2493 | `				bPrevMember = pPrev && (pPrev->iOp == PH7_OP_ROT_CALLEE` |
|     93782 | 2494 | `					? (pPrev->iP2 & PH7_ROT_TWOSLOT) != 0` |
|         - | 2495 | ``					/* …and only a METHOD member is one. A `new`'s class expression may`` |
|         - | 2496 | ``					 * BE a property read (`new $config->defType()`), which leaves an`` |
|         - | 2497 | `					 * OP_MEMBER in PH7_MEMBER_READ mode with the class NAME on the` |
|         - | 2498 | `					 * stack -- the trailing OP_CALL is the constructor's and must be` |
|         - | 2499 | `					 * folded away like any other. Reading the opcode alone kept it, so` |
|         - | 2500 | `					 * the property's VALUE was then called as a function. */` |
|      2812 | 2501 | `					: (pPrev->iOp == PH7_OP_MEMBER && pPrev->iP2 == PH7_MEMBER_METHOD));` |
|     96599 | 2502 | `				if( !bPrevMember ){` |
|         - | 2503 | `					/* Pop the call instruction, preserve named-arg map and` |
|         - | 2504 | `					 * the hasSpread flag (OP_NEW consumes the spread` |
|         - | 2505 | `					 * accumulator exactly like OP_CALL would have). */` |
|     96599 | 2506 | `					iP1 = pInstr->iP1;` |
|     96599 | 2507 | `					iP2 = pInstr->iP2;` |
|     96599 | 2508 | `					if( pInstr->p3 ){` |
|     96599 | 2509 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|     48231 | 2510 | `					}` |
|     96599 | 2511 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|     48231 | 2512 | `				}` |
|     48236 | 2513 | `			}` |
|   3519379 | 2514 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|         - | 2515 | `			/* instanceof: right operand is a class name, not a constant.` |
|         - | 2516 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     14067 | 2517 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     14067 | 2518 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     14059 | 2519 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     14059 | 2520 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     14059 | 2521 | `				int isSpecialIs = 0;` |
|     14059 | 2522 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     14059 | 2523 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     14059 | 2524 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     14054 | 2525 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     14050 | 2526 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      7015 | 2527 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|        25 | 2528 | `						isSpecialIs = 1;` |
|        11 | 2529 | `					}` |
|      7018 | 2530 | `				}` |
|     14059 | 2531 | `				pInstr->iP1 = 0;` |
|     14059 | 2532 | `				if( !isSpecialIs && !bAbsolute ){` |
|     14009 | 2533 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      6993 | 2534 | `				}` |
|      7023 | 2535 | `			}` |
|   3462697 | 2536 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|         - | 2537 | `			/* Prevent constant expansion for member/property names.` |
|         - | 2538 | `			 * The right child (member name) was just compiled — its LOADC` |
|         - | 2539 | `			 * should not trigger constant lookup. */` |
|     35459 | 2540 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     35459 | 2541 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     34649 | 2542 | `				pInstr->iP1 = 0;` |
|     17303 | 2543 | `			}` |
|     35459 | 2544 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|         - | 2545 | `				/* Static member access,remember that */` |
|      4268 | 2546 | `				iP1 = 1;` |
|      4268 | 2547 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      4268 | 2548 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|       615 | 2549 | `					p3 = pInstr->p3;` |
|         - | 2550 | ``					/* A `$`-form (`C::$s`, `C::$$x`, `C::${$e}`) is a STATIC PROPERTY`` |
|         - | 2551 | `					 * access, never a constant. A LITERAL name folds into p3 (non-zero)` |
|         - | 2552 | ``					 * and the exec side reads it there; a DYNAMIC name (`$$x`/`${$e}`)`` |
|         - | 2553 | `					 * leaves p3==0 with the computed name on the stack — the SAME shape` |
|         - | 2554 | ``					 * as a bareword constant `C::C`. Mark iP1=2 so exec still routes it`` |
|         - | 2555 | `					 * to the property table (hAttr), not the constant table (hConst). */` |
|       615 | 2556 | `					if( p3 == 0 ){` |
|        15 | 2557 | `						iP1 = 2;` |
|         6 | 2558 | `					}` |
|       615 | 2559 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       305 | 2560 | `				}` |
|      2127 | 2561 | `			}` |
|         - | 2562 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|         - | 2563 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|         - | 2564 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|         - | 2565 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|     35459 | 2566 | `			if( iP2 == PH7_MEMBER_READ ){` |
|     35459 | 2567 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|       168 | 2568 | `					iP2 = PH7_MEMBER_UNSET;` |
|     35377 | 2569 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|       313 | 2570 | `					iP2 = PH7_MEMBER_ISSET;` |
|     35141 | 2571 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|        61 | 2572 | `					iP2 = PH7_MEMBER_EMPTY;` |
|     34958 | 2573 | `				}else if( iFlags & EXPR_FLAG_MEMBER_COALESCE ){` |
|       267 | 2574 | `					iP2 = PH7_MEMBER_COALESCE;` |
|     34798 | 2575 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|         - | 2576 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|      2617 | 2577 | `					iP2 = PH7_MEMBER_WRITE;` |
|     33361 | 2578 | `				}else if( iFlags & EXPR_FLAG_DEFER_ARG ){` |
|         - | 2579 | `					/* D1 commit 2: deferred by-ref/by-value property arg ($o->p). */` |
|      2661 | 2580 | `					iP2 = PH7_MEMBER_DEFPATH;` |
|      1328 | 2581 | `				}` |
|     17708 | 2582 | `			}` |
|     17708 | 2583 | `		}` |
|         - | 2584 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|         - | 2585 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|         - | 2586 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|         - | 2587 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|         - | 2588 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
|   3630719 | 2589 | `		if( bFcc ){` |
|       267 | 2590 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|         - | 2591 | `			/* php's global fallback applies to a first-class callable exactly as it does` |
|         - | 2592 | ``			 * to the call it stands for: inside a namespace, `strlen(...)` is the global`` |
|         - | 2593 | `			 * function when the current namespace has none. The callee's literal was` |
|         - | 2594 | `			 * namespace-qualified above and the arg map that records it is dropped here` |
|         - | 2595 | `			 * (an FCC has no arguments), so carry the one bit the resolution needs in the` |
|         - | 2596 | ``			 * instruction itself — without it `strlen(...)` in a namespaced file was`` |
|         - | 2597 | ``			 * `Call to undefined function A\B\strlen()`, a fatal on ordinary php. */`` |
|       267 | 2598 | `			iP2 = (p3 && ((VmCallArgMap *)p3)->bIsNamespaced) ? 1 : 0;` |
|       267 | 2599 | `			p3 = 0;` |
|       267 | 2600 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|       267 | 2601 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER && pInstr->iP2 == PH7_MEMBER_METHOD ){` |
|         - | 2602 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|         - | 2603 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|         - | 2604 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|         - | 2605 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|       151 | 2606 | `				void *pMemberName = pInstr->p3;` |
|       151 | 2607 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|       151 | 2608 | `				if( pMemberName ){` |
|       ! 0 | 2609 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|       ! 0 | 2610 | `				}` |
|       151 | 2611 | `				iP1 = 2;` |
|        78 | 2612 | `			}else{` |
|         - | 2613 | `				/* Only a METHOD member is the callee's NAME. A parenthesised PROPERTY read` |
|         - | 2614 | ``				 * (`($o->cb)(...)`, `(C::$cb)(...)`) is php's variable-invocation: the member`` |
|         - | 2615 | `				 * op stays, its VALUE is the callable, and this is the iP1=1 wrap. Dropping it` |
|         - | 2616 | `				 * here read the property NAME as a method name and answered` |
|         - | 2617 | ``				 * `Call to undefined method H::cb()` for a closure the object was holding —`` |
|         - | 2618 | `				 * the CALL codegen above already made the distinction (it leaves the member a` |
|         - | 2619 | `				 * plain read for a parenthesised callee) and this branch undid it. */` |
|       120 | 2620 | `				iP1 = 1;` |
|         - | 2621 | `			}` |
|       131 | 2622 | `		}` |
|         - | 2623 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|         - | 2624 | `		 * This is the primary emit path for user-visible calls. */` |
|   3630719 | 2625 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|   1033926 | 2626 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|    515889 | 2627 | `		}` |
|         - | 2628 | `		/* Finally,emit the VM instruction associated with this operator */` |
|   3630719 | 2629 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|   3630714 | 2630 | `		if( iVmOp == PH7_OP_MEMBER && iP2 == PH7_MEMBER_READ` |
|     32429 | 2631 | `		 && (iFlags & EXPR_FLAG_MEMBER_REFSRC) ){` |
|         - | 2632 | `			/* The reference SOURCE keeps its READ mode -- php hands back a copy for a` |
|         - | 2633 | `			 * handler-backed property, and dispatches __get for an overloaded one -- and` |
|         - | 2634 | `			 * carries the write-context marker beside it (see VmInstr::bRefSrc). */` |
|       141 | 2635 | `			VmInstr *pRefSrc = PH7_VmPeekInstr(pGen->pVm);` |
|       141 | 2636 | `			if( pRefSrc ){` |
|       141 | 2637 | `				pRefSrc->bRefSrc = 1;` |
|        69 | 2638 | `			}` |
|        69 | 2639 | `		}` |
|   3630719 | 2640 | `		if( (iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW) && pNode->pStart ){` |
|         - | 2641 | `			/* A call's own line is where it BEGINS, not where its argument list` |
|         - | 2642 | `			 * closes. The emitter stamps every instruction with the token the` |
|         - | 2643 | `			 * generator is standing on, which for a call is the ')' -- so a call` |
|         - | 2644 | `			 * written across several lines went into the backtrace at its LAST one` |
|         - | 2645 | `			 * and php records its first. */` |
|   1033926 | 2646 | `			VmInstr *pCallInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   1033926 | 2647 | `			if( pCallInstr ){` |
|   1033926 | 2648 | `				pCallInstr->nLine = pNode->pStart->nLine;` |
|    515889 | 2649 | `			}` |
|    515889 | 2650 | `		}` |
|   1812481 | 2651 | `	}` |
|   3631755 | 2652 | `	if( nJmpIdx > 0 ){` |
|         - | 2653 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|    196627 | 2654 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|    196627 | 2655 | `		if( pInstr ){` |
|    196627 | 2656 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     98177 | 2657 | `		}` |
|     98177 | 2658 | `	}` |
|   3631755 | 2659 | `	return rc;` |
|   4803488 | 2660 | `}` |
|         - | 2661 | `/*` |
|         - | 2662 | ` * Emit a call's ARGUMENT LIST, and decide everything about it the OP_CALL then carries:` |
|         - | 2663 | ` * the count, the unpack flag, the named-argument / assert-source / argument-shape map.` |
|         - | 2664 | ` *` |
|         - | 2665 | ` * Split out of GenStateEmitExprCode because php resolves a callee BEFORE it evaluates` |
|         - | 2666 | ` * the arguments, so this now runs AFTER the callee sub-tree has been emitted (the one` |
|         - | 2667 | `` * exception is a `new`'s constructor list — see EXPR_FLAG_NEW_CALLEE). It is otherwise`` |
|         - | 2668 | ` * the same code, and reads only the node: nothing here inspects the instructions the` |
|         - | 2669 | ` * callee left behind.` |
|         - | 2670 | ` *` |
|         - | 2671 | ` * pArgs->p3 may arrive non-NULL — the callee's own namespace qualification builds the` |
|         - | 2672 | ` * VmCallArgMap first now — and every allocation site below reuses it.` |
|         - | 2673 | ` */` |
|    934773 | 2674 | `static sxi32 GenStateEmitCallArgs(` |
|         - | 2675 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 2676 | `	ph7_expr_node *pNode, /* The call node */` |
|         - | 2677 | `	sxi32 iFlags,         /* Control flags of the call site */` |
|         - | 2678 | `	GenCallArgs *pArgs    /* OUT: what the OP_CALL needs */` |
|         - | 2679 | `	)` |
|         5 | 2680 | `{` |
|    934778 | 2681 | `	void *p3 = pArgs->p3;` |
|    934778 | 2682 | `	sxi32 iP1 = 0;` |
|    934778 | 2683 | `	sxu32 iP2 = 0;` |
|    934778 | 2684 | `	int bFcc = 0;` |
|         - | 2685 | `	sxi32 rc;` |
|         - | 2686 | `	ph7_expr_node **apNode;` |
|    934778 | 2687 | `	int hasSpread = 0;` |
|    934778 | 2688 | `	int hasNamed = 0;` |
|    934778 | 2689 | `	sxu32 byRefMask = 0;` |
|         - | 2690 | `	sxi32 nArgs;` |
|         - | 2691 | `	sxi32 n;` |
|    934778 | 2692 | `	int bAnySpread = 0;` |
|         - | 2693 | `	/* Recurse and generate bytecodes for function arguments */` |
|    934778 | 2694 | `	apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|    934778 | 2695 | `	nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|         - | 2696 | ``	/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|         - | 2697 | `	 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|         - | 2698 | `	 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|    934778 | 2699 | `	if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|       267 | 2700 | `		bFcc = 1;` |
|       267 | 2701 | `		nArgs = 0;` |
|       131 | 2702 | `	}` |
|         - | 2703 | `	/* Validate argument order like php: no positional argument after a` |
|         - | 2704 | ``	 * named one OR after unpacking, and `name: ...$x` is a parse error. */`` |
|         - | 2705 | `	{` |
|    934778 | 2706 | `		int seenNamed = 0;` |
|    934778 | 2707 | `		int seenSpread = 0;` |
|   2249793 | 2708 | `		for( n = 0; n < nArgs; ++n ){` |
|   1315024 | 2709 | `			if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       372 | 2710 | `				bAnySpread = 1;` |
|       372 | 2711 | `				seenSpread = 1;` |
|       372 | 2712 | `				if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       ! 0 | 2713 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[n]->pStart->nLine,` |
|         - | 2714 | `						"syntax error, unexpected token \"...\"");` |
|       ! 0 | 2715 | `					return SXERR_SYNTAX;` |
|         - | 2716 | `				}` |
|       372 | 2717 | `				if( seenNamed ){` |
|         - | 2718 | `					/* The mirror of the positional-after-named rule: php refuses the` |
|         - | 2719 | ``					 * UNPACK too, and at compile time. Without it `f(x: 1, ...$a)` ran`` |
|         - | 2720 | `					 * and reported whatever the runtime binder made of the flattened` |
|         - | 2721 | `					 * list -- a different sentence, raised too late, on a program php` |
|         - | 2722 | `					 * never starts. */` |
|         3 | 2723 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 2724 | `						"Cannot use argument unpacking after named arguments");` |
|         3 | 2725 | `					return SXERR_SYNTAX;` |
|         5 | 2726 | `				}` |
|   1314839 | 2727 | `			}else if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       689 | 2728 | `				seenNamed = 1;` |
|       689 | 2729 | `				hasNamed = 1;` |
|   1314315 | 2730 | `			}else if( seenNamed ){` |
|         3 | 2731 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 2732 | `					"Cannot use positional argument after named argument");` |
|         3 | 2733 | `				return SXERR_SYNTAX;` |
|   1313971 | 2734 | `			}else if( seenSpread ){` |
|       ! 0 | 2735 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 2736 | `					"Cannot use positional argument after argument unpacking");` |
|       ! 0 | 2737 | `				return SXERR_SYNTAX;` |
|         - | 2738 | `			}` |
|    656022 | 2739 | `		}` |
|         - | 2740 | `	}` |
|         - | 2741 | `	/* Read-only load */` |
|    934774 | 2742 | `	iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|         - | 2743 | `	/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|         - | 2744 | ``	 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|         - | 2745 | `	 * objects dispatch to the right method (offsetExists for both;` |
|         - | 2746 | `	 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|    934774 | 2747 | `	if( pNode->pLeft && pNode->pLeft->pStart ){` |
|    934774 | 2748 | `		SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|   1440469 | 2749 | `		int bIsset = pCallName->nByte == 5` |
|    934769 | 2750 | `			&& SyStrnicmp(pCallName->zString,"isset",5) == 0;` |
|   1440469 | 2751 | `		int bEmpty = pCallName->nByte == 5` |
|    934769 | 2752 | `			&& SyStrnicmp(pCallName->zString,"empty",5) == 0;` |
|         - | 2753 | `		/* isset()/empty() are language CONSTRUCTS, not functions: php parses` |
|         - | 2754 | `		 * their argument list in the grammar and a missing operand is a parse` |
|         - | 2755 | `		 * error on the ')'. They compile through this ordinary call loop, which` |
|         - | 2756 | ``		 * never checked arity, so `empty()` quietly evaluated to true and`` |
|         - | 2757 | ``		 * `isset()` to false. (empty() also takes exactly one operand in php,`` |
|         - | 2758 | `		 * unlike isset(), which is variadic.) */` |
|    934774 | 2759 | `		if( (bIsset \|\| bEmpty) && nArgs < 1 ){` |
|         - | 2760 | `			/* php names the ')' itself as the unexpected token, so point at the` |
|         - | 2761 | `			 * node's last token rather than pGen->pIn (which has already moved` |
|         - | 2762 | `			 * past the call to the statement's ';'). */` |
|         6 | 2763 | `			SyToken *pTok = pNode->pEnd;` |
|         6 | 2764 | `			if( pTok && pTok > pNode->pStart && (pTok->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 | 2765 | `				pTok--;` |
|       ! 0 | 2766 | `			}` |
|         6 | 2767 | `			PH7_GenSyntaxError(&(*pGen),pTok,0);` |
|         6 | 2768 | `			return SXERR_ABORT;` |
|         - | 2769 | `		}` |
|    934770 | 2770 | `		if( bIsset ){` |
|     14567 | 2771 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|    927480 | 2772 | `		}else if( bEmpty ){` |
|       207 | 2773 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|       101 | 2774 | `		}` |
|         - | 2775 | `		/* Auto-vivify by-reference out-params of known builtins so an` |
|         - | 2776 | `		 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|         - | 2777 | `		 * $m never assigned) gets a real memobj slot for the builtin to` |
|         - | 2778 | `		 * write back through. Skipped when spread/named args are present:` |
|         - | 2779 | `		 * the compile-time positional index no longer maps to the` |
|         - | 2780 | `		 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|    934770 | 2781 | `		if( !bAnySpread && !hasNamed ){` |
|         - | 2782 | `			SyString sBuiltin;` |
|    934019 | 2783 | `			GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|    934019 | 2784 | `			byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|    466004 | 2785 | `		}` |
|    466379 | 2786 | `	}` |
|   2249779 | 2787 | `	for( n = 0 ; n < nArgs ; ++n ){` |
|   1315016 | 2788 | `		sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   1315016 | 2789 | `		sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE` |
|         - | 2790 | `			\|EXPR_FLAG_MEMBER_REFSRC);` |
|         - | 2791 | `		/* For a by-ref argument position, drop the read-only flag so the` |
|         - | 2792 | `		 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|         - | 2793 | `		 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|         - | 2794 | `		 * auto-vivifies its element and exposes a writable memobj slot for the` |
|         - | 2795 | `		 * builtin to write back through. A plain $var target is unaffected` |
|         - | 2796 | `		 * (iP1=0 either way).` |
|         - | 2797 | `		 *` |
|         - | 2798 | `		 * A PROPERTY target is the one shape this eager path cannot express, so it` |
|         - | 2799 | ``		 * is left to the deferred one below: what php's `FETCH_OBJ_W` does to`` |
|         - | 2800 | ``		 * `$o->p` is not what an ASSIGNMENT does to it — a missing property is`` |
|         - | 2801 | ``		 * CREATED, an overloaded one takes `Indirect modification of overloaded`` |
|         - | 2802 | ``		 * property` and is passed by VALUE (rather than reaching `__set`), and a`` |
|         - | 2803 | ``		 * non-object base is the catchable `Attempt to modify property`. The`` |
|         - | 2804 | `		 * deferred resolver already encodes all of that (VmBindPropByRef) and` |
|         - | 2805 | `		 * already reads a host function's by-ref mask, so routing the property` |
|         - | 2806 | ``		 * shapes through it is what makes `preg_match($p, $s, $this->matches)` —`` |
|         - | 2807 | `		 * the ordinary spelling — write anything at all. */` |
|   1315011 | 2808 | `		if( n < 31 && (byRefMask & (1u<<n))` |
|    656798 | 2809 | `		 && !GenStateArgHasPropertyStep(apNode[n]) ){` |
|      1469 | 2810 | `			iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|      1469 | 2811 | `			iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|       725 | 2812 | `		}` |
|         - | 2813 | ``		/* D1: a plain `$var` argument may bind to a by-ref parameter whose signature`` |
|         - | 2814 | `		 * is unknown at compile time (forward reference, dynamic call, or method` |
|         - | 2815 | ``		 * dispatch — e.g. PHPUnit's `willReturnReference($undef)`). We used to clear`` |
|         - | 2816 | `		 * the read-only flag here so an undefined variable vivified a real slot the` |
|         - | 2817 | `		 * by-ref write-back could reach — but that also invented the variable as NULL` |
|         - | 2818 | `		 * in the caller when the parameter turned out by-VALUE, and suppressed php's` |
|         - | 2819 | ``		 * `Undefined variable $x` warning. Instead mark it DEFERRED: OP_LOAD leaves an`` |
|         - | 2820 | `		 * undefined variable uncreated and carries a lazy-lvalue marker, and OP_CALL` |
|         - | 2821 | `		 * materializes it ONLY for a by-ref parameter once the callee is resolved` |
|         - | 2822 | `		 * (VmResolveDeferredArgs). Excludes isset()/empty()/unset(), which compile` |
|         - | 2823 | `		 * through this same call loop but must NEVER create their operand, and` |
|         - | 2824 | `		 * spread args (whose elements have no positional index of their own). A NAMED` |
|         - | 2825 | `		 * arg defers too: it binds to the formal its NAME picks, which the resolver` |
|         - | 2826 | `		 * looks up through the call's own argument map — excluding it left` |
|         - | 2827 | ``		 * `r(x: $a['new'])` warning `Undefined array key` and passing NULL where php`` |
|         - | 2828 | ``		 * creates the element for the by-ref parameter `$x`.`` |
|         - | 2829 | `		 *` |
|         - | 2830 | `		 * D1 commit 2: the same reasoning extends to an array-element ($a["k"]) or` |
|         - | 2831 | `		 * property ($o->p) argument — a by-ref user-function parameter must vivify the` |
|         - | 2832 | `		 * element/property, a by-value one must warn and NOT vivify. Those nodes carry a` |
|         - | 2833 | `		 * subscript/arrow operator (pOp != 0). The DEFER flag rides down to the base LOAD` |
|         - | 2834 | `		 * (undefined base auto-defers via commit 1) and to the LOAD_IDX/MEMBER, which` |
|         - | 2835 | ``		 * record the lvalue path on a lookup miss. Static `::` and nullsafe `?->` stay`` |
|         - | 2836 | `		 * eager. */` |
|   1315011 | 2837 | `		if( (iFlags & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_LOAD_IDX_UNSET` |
|    656015 | 2838 | `		               \|EXPR_FLAG_MEMBER_COALESCE)) == 0` |
|   1307610 | 2839 | `		 && (iArgFlags & EXPR_FLAG_RDONLY_LOAD) /* not a known builtin by-ref slot (kept eager above) */` |
|   1299488 | 2840 | `		 && (apNode[n]->iFlags & EXPR_NODE_SPREAD) == 0` |
|   1401712 | 2841 | `		 && ( (apNode[n]->pOp == 0 && apNode[n]->xCode == PH7_CompileVariable)` |
|    976116 | 2842 | `		   \|\| (apNode[n]->pOp != 0 && (apNode[n]->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|    214186 | 2843 | `		                            \|\| apNode[n]->pOp->iOp == EXPR_OP_ARROW)) ) ){` |
|    661055 | 2844 | `			iArgFlags \|= EXPR_FLAG_DEFER_ARG;` |
|    330025 | 2845 | `		}` |
|   1315016 | 2846 | `		rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|   1315016 | 2847 | `		if( rc != SXRET_OK ){` |
|         3 | 2848 | `			return rc;` |
|         - | 2849 | `		}` |
|         - | 2850 | `		/* Each argument is an independent nullsafe scope. */` |
|   1315014 | 2851 | `		GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|   1315014 | 2852 | `		if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|         - | 2853 | `			/* Emit spread opcode to unpack this array argument. iP1 marks a` |
|         - | 2854 | ``			 * source php will unpack BY REFERENCE: only a plain `$var` (php`` |
|         - | 2855 | ``			 * fetches every other shape — `$a[0]`, `$o->p`, `C::$s`, a cast, a`` |
|         - | 2856 | `			 * call — as an R-value, so a by-ref parameter binds its elements in` |
|         - | 2857 | `			 * a temporary and the write-back is invisible). The expander needs` |
|         - | 2858 | `			 * the distinction because it carries each element's slot for the` |
|         - | 2859 | ``			 * by-ref binder; without it `r(...$a[0])` wrote through to the real`` |
|         - | 2860 | `			 * element, which php leaves alone. */` |
|       505 | 2861 | `			PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD,` |
|       365 | 2862 | `				(apNode[n]->pOp == 0 && apNode[n]->xCode == PH7_CompileVariable) ? 1 : 0,` |
|         - | 2863 | `				0, 0, 0);` |
|       370 | 2864 | `			hasSpread = 1;` |
|       182 | 2865 | `		}` |
|    656019 | 2866 | `	}` |
|         - | 2867 | `	/* Total number of given arguments */` |
|    934768 | 2868 | `	iP1 = nArgs;` |
|    934768 | 2869 | `	iP2 = hasSpread;` |
|         - | 2870 | `	/* Build VmCallArgMap if named arguments are present.` |
|         - | 2871 | `	 * Deep-copy name strings so they survive token stream cleanup. */` |
|    934768 | 2872 | `	if( hasNamed ){` |
|       441 | 2873 | `		sxu32 nStrBytes = 0;` |
|         - | 2874 | `		char *zBuf;` |
|      1271 | 2875 | `		for( n = 0; n < nArgs; ++n ){` |
|       835 | 2876 | `			if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       685 | 2877 | `				nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|       340 | 2878 | `			}` |
|       420 | 2879 | `		}` |
|         - | 2880 | `		{` |
|       441 | 2881 | `		sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|       441 | 2882 | `		VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|       436 | 2883 | `			&pGen->pVm->sAllocator, mapSize);` |
|       441 | 2884 | `		if( pMap ){` |
|       441 | 2885 | `			SyZero(pMap, mapSize);` |
|         - | 2886 | `			/* The names need their own contiguous allocation, so this map REPLACES` |
|         - | 2887 | `			 * whatever the callee's namespace qualification built -- and it has to` |
|         - | 2888 | `			 * carry that map's findings across. Dropping them lost the ORIGINAL` |
|         - | 2889 | ``			 * name literal, which is the only thing the `new` codegen can`` |
|         - | 2890 | ``			 * re-qualify with CLASS imports: `new Imported(x: 1)` then resolved`` |
|         - | 2891 | `			 * against the current namespace and the class was not found, while` |
|         - | 2892 | ``			 * the same `new` with positional arguments worked. */`` |
|       441 | 2893 | `			if( p3 ){` |
|        25 | 2894 | `				VmCallArgMap *pPrior = (VmCallArgMap *)p3;` |
|        25 | 2895 | `				pMap->nOrigNameLit = pPrior->nOrigNameLit;` |
|        25 | 2896 | `				pMap->bIsNamespaced = pPrior->bIsNamespaced;` |
|        25 | 2897 | `				pMap->nNewClassInstr = pPrior->nNewClassInstr;` |
|        25 | 2898 | `				pMap->bStrict = pPrior->bStrict;` |
|         - | 2899 | `				/* Nothing else holds it: it is attached to no instruction yet. */` |
|        25 | 2900 | `				SyMemBackendFree(&pGen->pVm->sAllocator,pPrior);` |
|        12 | 2901 | `			}` |
|       441 | 2902 | `			pMap->bHasNamed = 1;` |
|       441 | 2903 | `			pMap->nTotal = (sxu32)nArgs;` |
|       441 | 2904 | `			pMap->aNames = (SyString *)&pMap[1];` |
|       441 | 2905 | `			zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|      1271 | 2906 | `			for( n = 0; n < nArgs; ++n ){` |
|       835 | 2907 | `				if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       685 | 2908 | `					sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|       685 | 2909 | `					SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|       685 | 2910 | `					SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|       685 | 2911 | `					zBuf += nb;` |
|       340 | 2912 | `				}` |
|         - | 2913 | `				/* else: aNames[n] remains {NULL, 0} for positional */` |
|       420 | 2914 | `			}` |
|       441 | 2915 | `			p3 = (void *)pMap;` |
|       218 | 2916 | `		}` |
|         - | 2917 | `		}` |
|       218 | 2918 | `	}` |
|         - | 2919 | `	/* assert(): php's compiler keeps a copy of the assertion's AST and a` |
|         - | 2920 | ``	 * failing assert reports its rendered SOURCE (`assert(1 == 2)`), not the`` |
|         - | 2921 | `	 * evaluated value. Render the first argument's token span` |
|         - | 2922 | `	 * into the call map so vm_builtin_assert can echo it. Only a DIRECT` |
|         - | 2923 | `	 * unqualified/absolute call qualifies — matching php, an indirect call` |
|         - | 2924 | `	 * (call_user_func, a callable variable) has no source text and its` |
|         - | 2925 | `	 * AssertionError carries an empty message. A spread first argument is` |
|         - | 2926 | `	 * skipped (its span is the unpacked array, not the assertion). */` |
|    934763 | 2927 | `	if( nArgs >= 1 && !bFcc` |
|    896952 | 2928 | `	 && (apNode[0]->iFlags & EXPR_NODE_SPREAD) == 0 ){` |
|         - | 2929 | `		SyString sCallee;` |
|    896660 | 2930 | `		GenStateCallBuiltinName(pNode->pLeft,&sCallee);` |
|    896655 | 2931 | `		if( sCallee.nByte == sizeof("assert")-1` |
|    583279 | 2932 | `		 && SyStrnicmp(sCallee.zString,"assert",sizeof("assert")-1) == 0 ){` |
|         - | 2933 | `			/* An operator root's pStart/pEnd name only the operator token` |
|         - | 2934 | ``			 * (`1 == 2` roots at `==`); the subtree walk recovers the whole`` |
|         - | 2935 | `			 * raw extent, re-adding parens the grouping pass consumed. */` |
|        67 | 2936 | `			SyToken *pSpanIn = 0;` |
|        67 | 2937 | `			SyToken *pSpanEnd = 0;` |
|         - | 2938 | `			SyBlob sSrc;` |
|        67 | 2939 | `			PH7_ExprSubtreeSpan(apNode[0],&pSpanIn,&pSpanEnd);` |
|        67 | 2940 | `			SyBlobInit(&sSrc,&pGen->pVm->sAllocator);` |
|        67 | 2941 | `			if( pSpanIn && pSpanEnd && pSpanIn < pSpanEnd ){` |
|        67 | 2942 | `				if( apNode[0]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|         - | 2943 | ``					/* php renders the name too: `assert(assertion: 1 == 2)`. */`` |
|         3 | 2944 | `					SyBlobAppend(&sSrc,apNode[0]->sArgName.zString,apNode[0]->sArgName.nByte);` |
|         3 | 2945 | `					SyBlobAppend(&sSrc,": ",2);` |
|         1 | 2946 | `				}` |
|        67 | 2947 | `				PH7_GenRenderAssertSpan(pGen,pSpanIn,pSpanEnd,&sSrc);` |
|        31 | 2948 | `			}` |
|        67 | 2949 | `			if( SyBlobLength(&sSrc) > 0 ){` |
|        98 | 2950 | `				char *zDup = (char *)SyMemBackendDup(&pGen->pVm->sAllocator,` |
|        62 | 2951 | `					SyBlobData(&sSrc),SyBlobLength(&sSrc));` |
|        67 | 2952 | `				if( zDup ){` |
|        67 | 2953 | `					if( p3 == 0 ){` |
|        65 | 2954 | `						VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|        60 | 2955 | `							&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|        65 | 2956 | `						if( pMap ){` |
|        65 | 2957 | `							SyZero(pMap,sizeof(VmCallArgMap));` |
|        65 | 2958 | `							p3 = (void *)pMap;` |
|        30 | 2959 | `						}` |
|        30 | 2960 | `					}` |
|        67 | 2961 | `					if( p3 ){` |
|        67 | 2962 | `						SyStringInitFromBuf(&((VmCallArgMap *)p3)->sAssertSrc,` |
|         - | 2963 | `							zDup,SyBlobLength(&sSrc));` |
|        31 | 2964 | `					}` |
|        31 | 2965 | `				}` |
|        31 | 2966 | `			}` |
|        67 | 2967 | `			SyBlobRelease(&sSrc);` |
|        31 | 2968 | `		}` |
|    447398 | 2969 | `	}` |
|         - | 2970 | `	/* Record each argument's compile-time SHAPE so the by-ref binders can` |
|         - | 2971 | `	 * refuse a non-variable where php refuses it — at the CALL, before the` |
|         - | 2972 | `	 * callee's ZPP runs. Skipped when the call SPREADS (one compile-time` |
|         - | 2973 | `	 * argument becomes N runtime slots, so the positions no longer line up)` |
|         - | 2974 | `	 * or when it carries more arguments than the masks can hold; a call` |
|         - | 2975 | `	 * without the flag keeps the old runtime nIdx test. Named arguments are` |
|         - | 2976 | `	 * fine: they change which FORMAL a slot binds to, not the slot's index. */` |
|    934768 | 2977 | `	if( !bAnySpread && nArgs > 0 && nArgs <= 31 && !bFcc ){` |
|    896603 | 2978 | `		sxu32 nNonLval = 0;` |
|    896603 | 2979 | `		sxu32 nTempCall = 0;` |
|   2211140 | 2980 | `		for( n = 0 ; n < nArgs ; ++n ){` |
|   1314542 | 2981 | `			int iShape = GenStateArgShape(apNode[n]);` |
|   1314542 | 2982 | `			if( iShape == GEN_ARG_NONE ){` |
|    568767 | 2983 | `				nNonLval \|= (1u << n);` |
|   1029276 | 2984 | `			}else if( iShape == GEN_ARG_TEMPCALL ){` |
|     68438 | 2985 | `				nTempCall \|= (1u << n);` |
|     34128 | 2986 | `			}` |
|    655784 | 2987 | `		}` |
|    896603 | 2988 | `		if( p3 == 0 ){` |
|    896003 | 2989 | `			VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|    895998 | 2990 | `				&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|    896003 | 2991 | `			if( pMap ){` |
|    896003 | 2992 | `				SyZero(pMap,sizeof(VmCallArgMap));` |
|    896003 | 2993 | `				p3 = (void *)pMap;` |
|    447070 | 2994 | `			}` |
|    447070 | 2995 | `		}` |
|    896603 | 2996 | `		if( p3 ){` |
|    896603 | 2997 | `			((VmCallArgMap *)p3)->bArgShapes = 1;` |
|    896603 | 2998 | `			((VmCallArgMap *)p3)->nNonLvalMask = nNonLval;` |
|    896603 | 2999 | `			((VmCallArgMap *)p3)->nTempCallMask = nTempCall;` |
|    447370 | 3000 | `		}` |
|    447370 | 3001 | `	}` |
|    934768 | 3002 | `	pArgs->iP1 = iP1;` |
|    934768 | 3003 | `	pArgs->iP2 = iP2;` |
|    934768 | 3004 | `	pArgs->p3  = p3;` |
|    934768 | 3005 | `	pArgs->bFcc = bFcc;` |
|    934768 | 3006 | `	pArgs->bAnySpread = bAnySpread;` |
|    934768 | 3007 | `	return SXRET_OK;` |
|    466388 | 3008 | `}` |
|         - | 3009 | `/*` |
|         - | 3010 | ` * Compile a PHP expression.` |
|         - | 3011 | ` * According to the PHP language reference manual:` |
|         - | 3012 | ` *  Expressions are the most important building stones of PHP.` |
|         - | 3013 | ` *  In PHP, almost anything you write is an expression.` |
|         - | 3014 | ` *  The simplest yet most accurate way to define an expression` |
|         - | 3015 | ` *  is "anything that has a value".` |
|         - | 3016 | ` * If something goes wrong while compiling the expression,this` |
|         - | 3017 | ` * function takes care of generating the appropriate error` |
|         - | 3018 | ` * message.` |
|         - | 3019 | ` */` |
|         - | 3020 | `/*` |
|         - | 3021 | ` * Does this expression tree contain a comma OPERATOR node?` |
|         - | 3022 | ` *` |
|         - | 3023 | `` * PH7 shipped `,` as a lowest-precedence binary operator (IMP-0139-COMMA), so`` |
|         - | 3024 | `` * `(1, 2)` and `$x = (f(), $y)` compile and evaluate to the right operand.`` |
|         - | 3025 | ` * php 8 has no comma operator: its grammar only allows comma-separated` |
|         - | 3026 | ` * expression LISTS inside for(...) clauses (call arguments, array literals and` |
|         - | 3027 | ` * list() are split by the parser, never by this node). Accepting it changes the` |
|         - | 3028 | ` * meaning of source php rejects, which §10 classes as a bug — so every context` |
|         - | 3029 | ` * except for() now reports php's parse error.` |
|         - | 3030 | ` */` |
|  31340906 | 3031 | `static int GenStateTreeHasComma(ph7_expr_node *pNode)` |
|         5 | 3032 | `{` |
|         - | 3033 | `	ph7_expr_node **apArg;` |
|         - | 3034 | `	sxu32 n;` |
|  31340911 | 3035 | `	if( pNode == 0 ){` |
|  22118369 | 3036 | `		return 0;` |
|         - | 3037 | `	}` |
|   9222547 | 3038 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         6 | 3039 | `		return 1;` |
|         - | 3040 | `	}` |
|   9222538 | 3041 | `	if( GenStateTreeHasComma(pNode->pLeft) \|\| GenStateTreeHasComma(pNode->pRight)` |
|   9222539 | 3042 | `	 \|\| GenStateTreeHasComma(pNode->pCond) ){` |
|         6 | 3043 | `		return 1;` |
|         - | 3044 | `	}` |
|   9222539 | 3045 | `	apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  10714396 | 3046 | `	for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; n++ ){` |
|   1491862 | 3047 | `		if( GenStateTreeHasComma(apArg[n]) ){` |
|       ! 0 | 3048 | `			return 1;` |
|         - | 3049 | `		}` |
|    744322 | 3050 | `	}` |
|   9222539 | 3051 | `	return 0;` |
|  15643846 | 3052 | `}` |
|   2304206 | 3053 | `PH7_PRIVATE sxi32 PH7_CompileExpr(` |
|         - | 3054 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 3055 | `	sxi32 iFlags,        /* Control flags */` |
|         - | 3056 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|         - | 3057 | `	)` |
|         5 | 3058 | `{` |
|         - | 3059 | `	ph7_expr_node *pRoot;` |
|         - | 3060 | `	SySet sExprNode;` |
|         - | 3061 | `	SyToken *pEnd;` |
|         - | 3062 | `	sxi32 nExpr;` |
|         - | 3063 | `	sxi32 iNest;` |
|         - | 3064 | `	sxi32 rc;` |
|         - | 3065 | `	sxu32 nNullsafeBase;` |
|         - | 3066 | `	/* Initialize worker variables */` |
|   2304211 | 3067 | `	nExpr = 0;` |
|   2304211 | 3068 | `	pRoot = 0;` |
|         - | 3069 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|         - | 3070 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|   2304211 | 3071 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   2304211 | 3072 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|   2304211 | 3073 | `	SySetAlloc(&sExprNode,0x10);` |
|   2304211 | 3074 | `	rc = SXRET_OK;` |
|         - | 3075 | `	/* Delimit the expression */` |
|   2304211 | 3076 | `	pEnd = pGen->pIn;` |
|   2304211 | 3077 | `	iNest = 0;` |
|  17706440 | 3078 | `	while( pEnd < pGen->pEnd ){` |
|  16708331 | 3079 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 3080 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|      8618 | 3081 | `			iNest++;` |
|  16704006 | 3082 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|      8628 | 3083 | `			iNest--;` |
|  16695388 | 3084 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|   1321706 | 3085 | `			if( iNest <= 0 ){` |
|   1306102 | 3086 | `				break;` |
|         - | 3087 | `			}` |
|      7761 | 3088 | `		}` |
|  15402234 | 3089 | `		pEnd++;` |
|         5 | 3090 | `	}` |
|   2304211 | 3091 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|      3147 | 3092 | `		SyToken *pEnd2 = pGen->pIn;` |
|      3147 | 3093 | `		iNest = 0;` |
|         - | 3094 | `		/* Stop at the first comma */` |
|     21253 | 3095 | `		while( pEnd2 < pEnd ){` |
|     18133 | 3096 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|       509 | 3097 | `				iNest++;` |
|     17881 | 3098 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|       509 | 3099 | `				iNest--;` |
|     17377 | 3100 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      6269 | 3101 | `				if( iNest <= 0 ){` |
|        25 | 3102 | `					break;` |
|         - | 3103 | `				}` |
|      3121 | 3104 | `			}` |
|     18111 | 3105 | `			pEnd2++;` |
|         5 | 3106 | `		}` |
|      3147 | 3107 | `		if( pEnd2 <pEnd ){` |
|        25 | 3108 | `			pEnd = pEnd2;` |
|        11 | 3109 | `		}` |
|      1571 | 3110 | `	}` |
|   2304211 | 3111 | `	if( pEnd > pGen->pIn ){` |
|   2304193 | 3112 | `		SyToken *pTmp = pGen->pEnd;` |
|         - | 3113 | `		/* Swap delimiter */` |
|   2304193 | 3114 | `		pGen->pEnd = pEnd;` |
|         - | 3115 | `		/* Try to get an expression tree */` |
|   2304193 | 3116 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|   2304188 | 3117 | `		if( rc == SXRET_OK && pRoot && pGen->nCommaExprOk < 1` |
|   2242584 | 3118 | `		 && GenStateTreeHasComma(pRoot) ){` |
|         - | 3119 | `			/* php has no comma operator outside a for() clause */` |
|         6 | 3120 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pRoot->pStart->nLine,` |
|         - | 3121 | `				"syntax error, unexpected token \",\"");` |
|         6 | 3122 | `			pGen->pEnd = pTmp;` |
|         - | 3123 | `			/* This refusal leaves by its own door, so it owes the release the` |
|         - | 3124 | `			 * ordinary path makes below -- the set owns every node the` |
|         - | 3125 | `			 * expression produced, and a bare SySetRelease drops the pointers` |
|         - | 3126 | `			 * without freeing what they point at. */` |
|         6 | 3127 | `			PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|         6 | 3128 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 3129 | `				SySetRelease(&sExprNode);` |
|       ! 0 | 3130 | `				return SXERR_ABORT;` |
|         - | 3131 | `			}` |
|         6 | 3132 | `			pGen->pIn = pEnd;` |
|         6 | 3133 | `			SySetRelease(&sExprNode);` |
|         6 | 3134 | `			SySetTruncate(&pGen->aNullsafeJmp,nNullsafeBase);` |
|         6 | 3135 | `			return SXRET_OK;` |
|         - | 3136 | `		}` |
|   2304189 | 3137 | `		if( rc == SXRET_OK && pRoot ){` |
|   2303881 | 3138 | `			rc = SXRET_OK;` |
|   2303881 | 3139 | `			if( xTreeValidator ){` |
|         - | 3140 | `				/* Call the upper layer validator callback */` |
|    175068 | 3141 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|     87403 | 3142 | `			}` |
|   2303881 | 3143 | `			if( rc != SXERR_ABORT ){` |
|         - | 3144 | `				/* Generate code for the given tree */` |
|   2303881 | 3145 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|         - | 3146 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|         - | 3147 | `				 * expression so they short-circuit to its end. */` |
|   2303881 | 3148 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|   1149967 | 3149 | `			}` |
|   2303881 | 3150 | `			nExpr = 1;` |
|   1149967 | 3151 | `		}` |
|         - | 3152 | `		/* Release the whole tree */` |
|   2304189 | 3153 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|         - | 3154 | `		/* Synchronize token stream */` |
|   2304189 | 3155 | `		pGen->pEnd = pTmp;` |
|   2304189 | 3156 | `		pGen->pIn  = pEnd;` |
|   2304189 | 3157 | `		if( rc == SXERR_ABORT ){` |
|        59 | 3158 | `			SySetRelease(&sExprNode);` |
|        59 | 3159 | `			return SXERR_ABORT;` |
|         - | 3160 | `		}` |
|   1150094 | 3161 | `	}` |
|   2304153 | 3162 | `	SySetRelease(&sExprNode);` |
|   2304153 | 3163 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|   1150137 | 3164 | `}` |
|         - | 3165 | `/*` |
|         - | 3166 | ` * Return a pointer to the node construct handler associated` |
|         - | 3167 | ` * with a given node type [i.e: string,integer,float,...].` |
|         - | 3168 | ` */` |
|   1472159 | 3169 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|         5 | 3170 | `{` |
|   1472164 | 3171 | `	if( nNodeType & PH7_TK_NUM ){` |
|         - | 3172 | `		/* Numeric literal: Either real or integer */` |
|    772561 | 3173 | `		return PH7_CompileNumLiteral;` |
|    699608 | 3174 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|         - | 3175 | `		/* Double quoted string */` |
|     75929 | 3176 | `		return PH7_CompileString;` |
|    623684 | 3177 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|         - | 3178 | `		/* Single quoted string */` |
|    623528 | 3179 | `		return PH7_CompileSimpleString;` |
|       161 | 3180 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|         - | 3181 | `		/* Heredoc */` |
|        91 | 3182 | `		return PH7_CompileHereDoc;` |
|        75 | 3183 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|         - | 3184 | `		/* Nowdoc */` |
|        63 | 3185 | `		return PH7_CompileNowDoc;` |
|        14 | 3186 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|         - | 3187 | `		/* Backtick quoted string */` |
|         3 | 3188 | `		return PH7_CompileBacktic;` |
|         - | 3189 | `	}` |
|        12 | 3190 | `	return 0;` |
|    734733 | 3191 | `}` |
|         - | 3192 | `/*` |
|         - | 3193 | ` * Tree validator for unset() arguments — php's write-target rules, then its` |
|         - | 3194 | ``  * "Can't use nullsafe operator in write context", then the grammar: `unset()` `` |
|         - | 3195 | `` * takes a `variable`, so `unset(GK)`, `unset("s")` and `unset(A::K)` are php`` |
|         - | 3196 | ` * PARSE errors where PHL let them reach the VM and answer with a PH7-ism.` |
|         - | 3197 | ` */` |
|       410 | 3198 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|         5 | 3199 | `{` |
|         - | 3200 | `	sxi32 rc;` |
|       415 | 3201 | `	rc = GenStateWriteTargetCheck(&(*pGen),pNode,PH7_WTC_UNSET);` |
|       415 | 3202 | `	if( rc != SXRET_OK ){` |
|         3 | 3203 | `		return rc;` |
|         - | 3204 | `	}` |
|       413 | 3205 | `	if( PH7_ExprContainsNullsafe(pNode) ){` |
|       ! 0 | 3206 | `		rc = PH7_GenCompileError(pGen,E_ERROR,` |
|       ! 0 | 3207 | `			pNode ? pNode->pStart->nLine : 1,` |
|         - | 3208 | `			"Can't use nullsafe operator in write context");` |
|       ! 0 | 3209 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 3210 | `	}` |
|       413 | 3211 | `	if( pNode && PH7_ExprIsModifiableValue(pNode) == FALSE ){` |
|         6 | 3212 | `		return PH7_ExprOperandNotAVariable(pGen,pNode);` |
|         - | 3213 | `	}` |
|       409 | 3214 | `	return SXRET_OK;` |
|       210 | 3215 | `}` |
|         - | 3216 | `/*` |
|         - | 3217 | ` * Compile an unset() statement.` |
|         - | 3218 | ` * unset($var, $arr[$key], ...);` |
|         - | 3219 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|         - | 3220 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|         - | 3221 | ` * parent array before extracting the element to unset.` |
|         - | 3222 | ` */` |
|      3580 | 3223 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|         5 | 3224 | `{` |
|      3585 | 3225 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|      3585 | 3226 | `	sxu32 nIdx = 0;` |
|         - | 3227 | `	SyString sName;` |
|         - | 3228 | `	sxi32 rc;` |
|         - | 3229 | `	/* Jump the 'unset' keyword */` |
|      3585 | 3230 | `	pGen->pIn++;` |
|         - | 3231 | `	/* Save delimiter */` |
|      3585 | 3232 | `	pTmp = pGen->pEnd;` |
|         - | 3233 | `	/* Skip optional opening parenthesis and find the matching close */` |
|      3585 | 3234 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|      3585 | 3235 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 3236 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|         - | 3237 | `		SyToken *pClose;` |
|      3585 | 3238 | `		pGen->pIn++;   /* Skip '(' */` |
|      3585 | 3239 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|      3585 | 3240 | `		pEnd = pClose; /* Stop at ')' */` |
|      1789 | 3241 | `	}` |
|      3585 | 3242 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|         - | 3243 | `	/* Resolve the 'unset' builtin name once */` |
|      3585 | 3244 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|       586 | 3245 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       586 | 3246 | `		if( pObj == 0 ){` |
|       ! 0 | 3247 | `			return SXERR_ABORT;` |
|         - | 3248 | `		}` |
|       586 | 3249 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       586 | 3250 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       290 | 3251 | `	}` |
|         - | 3252 | `	/* Compile each comma-separated argument */` |
|     12435 | 3253 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|      8859 | 3254 | `		if( pGen->pIn < pNext ){` |
|         - | 3255 | `			/*` |
|         - | 3256 | ``			 * A PLAIN variable (`unset($x)`, exactly two tokens: '$' and the name) drops a`` |
|         - | 3257 | `			 * single NAME binding, which the unset() builtin cannot express — all it ever` |
|         - | 3258 | `			 * receives is the value's slot index, and unsetting the slot destroys whatever` |
|         - | 3259 | `			 * else aliases it. Emit OP_UNSET_VAR with the name instead. Subscripts and` |
|         - | 3260 | ``			 * properties (`unset($a[k])`, `unset($o->p)`) keep the existing path, which`` |
|         - | 3261 | `			 * already removes just the element/property.` |
|         - | 3262 | `			 */` |
|      8854 | 3263 | `			if( &pGen->pIn[2] == pNext` |
|      8649 | 3264 | `				&& (pGen->pIn[0].nType & PH7_TK_DOLLAR)` |
|      8449 | 3265 | `				&& (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         - | 3266 | `				SyString *pVarName;` |
|         - | 3267 | ``				/* php refuses `unset($this)` where it is written. The tree validator`` |
|         - | 3268 | `				 * cannot see it — this fast path never builds a tree. */` |
|      8442 | 3269 | `				if( pGen->pIn[1].sData.nByte == sizeof("this")-1` |
|      4533 | 3270 | `				 && SyMemcmp((const void *)pGen->pIn[1].sData.zString,` |
|       308 | 3271 | `				             (const void *)"this",sizeof("this")-1) == 0 ){` |
|         5 | 3272 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - | 3273 | `						"Cannot unset $this");` |
|         5 | 3274 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 3275 | `						return SXERR_ABORT;` |
|         - | 3276 | `					}` |
|         - | 3277 | `					/* php stops compiling at its own fatal; this generator carries` |
|         - | 3278 | `					 * on to a budget of fifteen, so leave the cursor PAST the whole` |
|         - | 3279 | ``					 * `unset(...)` rather than on the operand it refused. Resuming`` |
|         - | 3280 | `					 * there re-read the closing ')' as a statement of its own and` |
|         - | 3281 | `					 * printed an "Unmatched ')'" under the fatal that php never` |
|         - | 3282 | `					 * reaches. */` |
|         5 | 3283 | `					pGen->pIn = pEnd;` |
|         5 | 3284 | `					if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|         5 | 3285 | `						pGen->pIn++;` |
|         2 | 3286 | `					}` |
|         5 | 3287 | `					pGen->pEnd = pTmp;` |
|         5 | 3288 | `					return SXERR_SYNTAX;` |
|         - | 3289 | `				}` |
|     12661 | 3290 | `				char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      8438 | 3291 | `					pGen->pIn[1].sData.zString,pGen->pIn[1].sData.nByte);` |
|      8443 | 3292 | `				pVarName = (SyString *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(SyString));` |
|      8443 | 3293 | `				if( zDup == 0 \|\| pVarName == 0 ){` |
|       ! 0 | 3294 | `					PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - | 3295 | `						"Fatal, PH7 is running out of memory");` |
|       ! 0 | 3296 | `					return SXERR_ABORT;` |
|         - | 3297 | `				}` |
|      8443 | 3298 | `				SyStringInitFromBuf(pVarName,zDup,pGen->pIn[1].sData.nByte);` |
|      8443 | 3299 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_UNSET_VAR,0,0,pVarName,0);` |
|      8443 | 3300 | `				pGen->pIn = pNext;` |
|      8443 | 3301 | `				if( pGen->pIn < pEnd ){` |
|      5243 | 3302 | `					pGen->pIn++; /* Jump the trailing comma */` |
|      2619 | 3303 | `				}` |
|      8443 | 3304 | `				continue;` |
|         - | 3305 | `			}` |
|       417 | 3306 | `			pGen->pEnd = pNext;` |
|       417 | 3307 | `			rc = PH7_CompileExpr(&(*pGen),` |
|         - | 3308 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|         - | 3309 | `				GenStateUnsetValidator);` |
|       417 | 3310 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 3311 | `				return SXERR_ABORT;` |
|         - | 3312 | `			}` |
|       417 | 3313 | `			if( rc != SXERR_EMPTY ){` |
|         - | 3314 | `				/* Emit call for this single argument */` |
|       415 | 3315 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       415 | 3316 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       415 | 3317 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       205 | 3318 | `			}` |
|       206 | 3319 | `		}` |
|         - | 3320 | `		/* Jump trailing commas */` |
|       455 | 3321 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|        41 | 3322 | `			pNext++;` |
|         3 | 3323 | `		}` |
|       417 | 3324 | `		pGen->pIn = pNext;` |
|         5 | 3325 | `	}` |
|         - | 3326 | `	/* Skip past the closing ')' if present */` |
|      3581 | 3327 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|      3581 | 3328 | `		pGen->pIn++;` |
|      1787 | 3329 | `	}` |
|         - | 3330 | `	/* Restore token stream */` |
|      3581 | 3331 | `	pGen->pEnd = pTmp;` |
|      3581 | 3332 | `	return SXRET_OK;` |
|      1794 | 3333 | `}` |
|         - | 3334 | `/*` |
|         - | 3335 | ` * PHP Language construct table.` |
|         - | 3336 | ` */` |
|         - | 3337 | `static const LangConstruct aLangConstruct[] = {` |
|         - | 3338 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|         - | 3339 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|         - | 3340 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|         - | 3341 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|         - | 3342 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|         - | 3343 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|         - | 3344 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|         - | 3345 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|         - | 3346 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|         - | 3347 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|         - | 3348 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|         - | 3349 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|         - | 3350 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|         - | 3351 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|         - | 3352 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|         - | 3353 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|         - | 3354 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|         - | 3355 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|         - | 3356 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|         - | 3357 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|         - | 3358 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|         - | 3359 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|         - | 3360 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|         - | 3361 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|         - | 3362 | `};` |
|         - | 3363 | `/*` |
|         - | 3364 | ` * Return a pointer to the statement handler routine associated` |
|         - | 3365 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|         - | 3366 | ` */` |
|   1217216 | 3367 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|         - | 3368 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|         - | 3369 | `	SyToken *pLookahed  /* Look-ahead token */` |
|         - | 3370 | `	)` |
|         5 | 3371 | `{` |
|   1217221 | 3372 | `	sxu32 n = 0;` |
|   3624697 | 3373 | `	for(;;){` |
|   7259011 | 3374 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|      6365 | 3375 | `			break;` |
|         - | 3376 | `		}` |
|   7252651 | 3377 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|   1210861 | 3378 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|       ! 0 | 3379 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|       ! 0 | 3380 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|         - | 3381 | `					/* 'static' (class context),return null */` |
|       ! 0 | 3382 | `					return 0;` |
|         - | 3383 | `				}` |
|       ! 0 | 3384 | `			}` |
|   1210856 | 3385 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|        84 | 3386 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|        51 | 3387 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|         - | 3388 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|         5 | 3389 | `				return 0;` |
|         - | 3390 | `			}` |
|         - | 3391 | `			/* Return a pointer to the handler.` |
|         - | 3392 | `			*/` |
|   1210857 | 3393 | `			return aLangConstruct[n].xConstruct;` |
|         - | 3394 | `		}` |
|   6041795 | 3395 | `		n++;` |
|         5 | 3396 | `	}` |
|      6365 | 3397 | `	if( pLookahed ){` |
|      6365 | 3398 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && PH7_IsClassNameToken(pLookahed) ){` |
|       337 | 3399 | `			return PH7_CompileClassInterface;` |
|      6033 | 3400 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && PH7_IsClassNameToken(pLookahed) ){` |
|      4663 | 3401 | `			return PH7_CompileClass;` |
|      1375 | 3402 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && PH7_IsClassNameToken(pLookahed) ){` |
|       337 | 3403 | `			return PH7_CompileTrait;` |
|         - | 3404 | `		}` |
|         - | 3405 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|         - | 3406 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|         - | 3407 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|         - | 3408 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|       519 | 3409 | `	}` |
|         - | 3410 | `	/* Not a language construct */` |
|      1043 | 3411 | `	return 0;` |
|    607774 | 3412 | `}` |
|         - | 3413 | `/*` |
|         - | 3414 | ` * Which words may NAME a class, an interface, a trait or an enum, and which may` |
|         - | 3415 | ` * not — php has two separate answers and this file used to have one.` |
|         - | 3416 | ` *` |
|         - | 3417 | ` * php's SCANNER decides the first: a reserved keyword is not an identifier, so` |
|         - | 3418 | `` * `class list {}` and `class callable {}` are parse errors. Its COMPILER decides`` |
|         - | 3419 | ` * the second, for a handful of words the scanner does hand over as identifiers:` |
|         - | 3420 | ``  * `zend_is_reserved_class_name` refuses `int`, `bool`, `void`, `null`, `self` `` |
|         - | 3421 | `` * and their neighbours with `Cannot use "X" as a class name as it is reserved`.`` |
|         - | 3422 | ` *` |
|         - | 3423 | ` * PHL's keyword set is not php's, which is where the divergence came from in both` |
|         - | 3424 | `` * directions. `integer` and `boolean` are CAST words here and identifiers in php,`` |
|         - | 3425 | `` * so `class Integer extends Base {}` — phpseclib writes exactly that, three times`` |
|         - | 3426 | ``  * — did not compile at all. And `void`, `never`, `null`, `false`, `true`, `mixed` `` |
|         - | 3427 | `` * and `iterable` arrive as plain identifiers here, so declaring a class with one`` |
|         - | 3428 | ` * of those names SUCCEEDED where php refuses.` |
|         - | 3429 | ` */` |
|    104838 | 3430 | `static int GenStateNameIs(const SyString *pName,const char *zWord)` |
|         5 | 3431 | `{` |
|    104843 | 3432 | `	sxu32 n = (sxu32)SyStrlen(zWord);` |
|         - | 3433 | `	/* Length FIRST: the token's bytes point into the source and are not` |
|         - | 3434 | `	 * NUL-terminated, so a shorter name must never be compared over its end. */` |
|    104843 | 3435 | `	return pName->nByte == n && SyStrnicmp(pName->zString,zWord,n) == 0;` |
|         5 | 3436 | `}` |
|        24 | 3437 | `static int GenStateClassNameKeywordOk(const SyString *pName)` |
|         1 | 3438 | `{` |
|         - | 3439 | `	/* The only two words PHL lexes as keywords that php lets name a class. */` |
|        25 | 3440 | `	return GenStateNameIs(pName,"integer") \|\| GenStateNameIs(pName,"boolean");` |
|         1 | 3441 | `}` |
|     15804 | 3442 | `PH7_PRIVATE int PH7_IsClassNameToken(const SyToken *pTok)` |
|         5 | 3443 | `{` |
|     15809 | 3444 | `	if( pTok == 0 ){` |
|       ! 0 | 3445 | `		return 0;` |
|         - | 3446 | `	}` |
|     15809 | 3447 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|        25 | 3448 | `		return GenStateClassNameKeywordOk(&pTok->sData);` |
|         - | 3449 | `	}` |
|     15785 | 3450 | `	if( (pTok->nType & PH7_TK_ID) == 0 ){` |
|         5 | 3451 | `		return 0;` |
|         - | 3452 | `	}` |
|     15781 | 3453 | `	if( pTok->nType & PH7_TK_OP ){` |
|         - | 3454 | ``		/* php's alpha-stream operators — `and`, `or`, `xor`, `new`, `clone`,`` |
|         - | 3455 | ``		 * `instanceof` — are keywords in its scanner and cannot be identifiers.`` |
|         - | 3456 | `		 * They reach here carrying both flags, and were accepted as names. */` |
|       ! 0 | 3457 | `		return 0;` |
|         - | 3458 | `	}` |
|         - | 3459 | `	{` |
|         - | 3460 | `		/* Two more php keywords that PHL treats as context-sensitive identifiers. */` |
|         - | 3461 | `		static const char *const azNo[] = { "callable", "readonly" };` |
|         - | 3462 | `		sxu32 i;` |
|     47333 | 3463 | `		for( i = 0 ; i < SX_ARRAYSIZE(azNo) ; ++i ){` |
|     31557 | 3464 | `			if( GenStateNameIs(&pTok->sData,azNo[i]) ){` |
|       ! 0 | 3465 | `				return 0;` |
|         - | 3466 | `			}` |
|     15781 | 3467 | `		}` |
|         - | 3468 | `	}` |
|     15781 | 3469 | `	return 1;` |
|      7907 | 3470 | `}` |
|         - | 3471 | `/*` |
|         - | 3472 | ` * php's zend_is_reserved_class_name: a word its scanner DOES hand over as an` |
|         - | 3473 | ` * identifier but its compiler refuses to name a class with. The check is` |
|         - | 3474 | ` * case-insensitive and the refusal quotes the name as WRITTEN.` |
|         - | 3475 | ` */` |
|      4884 | 3476 | `PH7_PRIVATE int PH7_IsReservedClassName(const SyString *pName)` |
|         5 | 3477 | `{` |
|         - | 3478 | `	static const char *const azReserved[] = {` |
|         - | 3479 | `		"bool", "int", "float", "string", "null", "false", "true", "void",` |
|         - | 3480 | `		"never", "iterable", "object", "mixed", "self", "parent", "static"` |
|         - | 3481 | `	};` |
|         - | 3482 | `	sxu32 i;` |
|     78137 | 3483 | `	for( i = 0 ; i < SX_ARRAYSIZE(azReserved) ; ++i ){` |
|     73255 | 3484 | `		if( GenStateNameIs(pName,azReserved[i]) ){` |
|         3 | 3485 | `			return 1;` |
|         - | 3486 | `		}` |
|     36629 | 3487 | `	}` |
|      4887 | 3488 | `	return 0;` |
|      2447 | 3489 | `}` |
|         - | 3490 | `/*` |
|         - | 3491 | ` * Check if the given keyword is in fact a PHP language construct.` |
|         - | 3492 | ` * Return TRUE on success. FALSE otheriwse.` |
|         - | 3493 | ` */` |
|      1042 | 3494 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|         5 | 3495 | `{` |
|         - | 3496 | `	int rc;` |
|      1047 | 3497 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|      1047 | 3498 | `	if( rc == FALSE ){` |
|       694 | 3499 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|       554 | 3500 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|         - | 3501 | ``			/* `match` is an EXPRESSION, and php takes an expression statement made of`` |
|         - | 3502 | ``			 * one: `match (true) { ... };` is how a dispatch table is written when the`` |
|         - | 3503 | `			 * answer is not wanted. Without this the statement dispatcher refused the` |
|         - | 3504 | `			 * keyword outright, and Doctrine's DQL parser -- which dispatches its tree` |
|         - | 3505 | `			 * walkers exactly that way -- did not compile. */` |
|       288 | 3506 | `			\|\| nKeyword == PH7_TKWRD_MATCH` |
|         - | 3507 | ``			/* php reserves NONE of `int`/`integer`/`bool`/`boolean`/`float`/`` |
|         - | 3508 | ``			 * `string`/`object`: its scanner hands every one of them back as a`` |
|         - | 3509 | `			 * plain T_STRING, and only a TYPE position gives them a meaning. A` |
|         - | 3510 | `			 * statement that begins with one is therefore an ordinary expression` |
|         - | 3511 | ``			 * -- `Integer::setModulo($id, $m);`, which is how phpseclib's`` |
|         - | 3512 | `			 * BinaryField spells the class it imported under that name, and which` |
|         - | 3513 | ``			 * this dispatcher answered `Unexpected keyword 'Integer'` for. The`` |
|         - | 3514 | ``			 * same word after `$x = ` already compiled, so only the STATEMENT head`` |
|         - | 3515 | `			 * was refusing it. */` |
|        19 | 3516 | `			\|\| nKeyword == PH7_TKWRD_INT \|\| nKeyword == PH7_TKWRD_BOOL` |
|         9 | 3517 | `			\|\| nKeyword == PH7_TKWRD_FLOAT \|\| nKeyword == PH7_TKWRD_STRING` |
|        13 | 3518 | `			\|\| nKeyword == PH7_TKWRD_OBJECT` |
|         - | 3519 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|         - | 3520 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|         - | 3521 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|         - | 3522 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|         - | 3523 | `			*/` |
|         - | 3524 | `			){` |
|       695 | 3525 | `				rc = TRUE;` |
|       345 | 3526 | `		}` |
|       345 | 3527 | `	}` |
|      1047 | 3528 | `	return rc;` |
|         5 | 3529 | `}` |
|         - | 3530 | `/*` |
|         - | 3531 | ` * Compile a PHP chunk.` |
|         - | 3532 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 3533 | ` * takes care of generating the appropriate error message.` |
|         - | 3534 | ` */` |
|         - | 3535 | `/*` |
|         - | 3536 | ` * Update pGen->sPendingDoc for the statement whose first token is` |
|         - | 3537 | ` * pGen->pIn: when a docblock trivia is keyed to that token's index in` |
|         - | 3538 | ` * the chunk token set it becomes the pending docblock. An existing` |
|         - | 3539 | ` * pending docblock is LEFT in place otherwise: Zend keeps the last-seen` |
|         - | 3540 | ` * doc comment until a declaration consumes it, so a docblock survives` |
|         - | 3541 | ` * intervening non-declaration statements.` |
|         - | 3542 | ` */` |
|   2065084 | 3543 | `PH7_PRIVATE void GenStateSetPendingDoc(ph7_gen_state *pGen)` |
|         5 | 3544 | `{` |
|   2065089 | 3545 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|   2065089 | 3546 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|   2065089 | 3547 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 3548 | `	sxu32 nIdx, n;` |
|   2065084 | 3549 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|     11475 | 3550 | `	 \|\| pGen->pIn < pBase \|\| pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|         - | 3551 | `		/* Re-tokenized substream (string interpolation, synthesized code):` |
|         - | 3552 | `		 * indexes do not map to the sidecar */` |
|   2053619 | 3553 | `		return;` |
|         - | 3554 | `	}` |
|     11475 | 3555 | `	nIdx = (sxu32)(pGen->pIn - pBase);` |
|         - | 3556 | `	/* Attributes must be adjacent to their declaration (unlike docblocks):` |
|         - | 3557 | `	 * reset at every boundary, then collect the groups keyed to this token. */` |
|     11475 | 3558 | `	SySetReset(&pGen->aPendingAttrs);` |
|     60861 | 3559 | `	for( n = 0 ; n < nT ; n++ ){` |
|     49391 | 3560 | `		if( aT[n].nTokIdx != nIdx ){` |
|     48425 | 3561 | `			continue;` |
|         - | 3562 | `		}` |
|       971 | 3563 | `		if( aT[n].iKind == PH7_TRIVIA_DOC ){` |
|       335 | 3564 | `			pGen->sPendingDoc = aT[n].sText;` |
|       806 | 3565 | `		}else if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|       641 | 3566 | `			SySetPut(&pGen->aPendingAttrs,(const void *)&aT[n]);` |
|       318 | 3567 | `		}` |
|       488 | 3568 | `	}` |
|   1030934 | 3569 | `}` |
|         - | 3570 | `/*` |
|         - | 3571 | ` * Hand the pending docblock (if any) to a declaration: duplicate it into` |
|         - | 3572 | ` * the VM allocator (the raw script buffer dies after compilation) and` |
|         - | 3573 | ` * clear the pending slot so sibling declarations do not inherit it.` |
|         - | 3574 | ` */` |
|    171945 | 3575 | `PH7_PRIVATE void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut)` |
|         5 | 3576 | `{` |
|         - | 3577 | `	char *zDup;` |
|    171950 | 3578 | `	if( SyStringLength(&pGen->sPendingDoc) < 1 ){` |
|    171656 | 3579 | `		return;` |
|         - | 3580 | `	}` |
|       446 | 3581 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       147 | 3582 | `		SyStringData(&pGen->sPendingDoc),SyStringLength(&pGen->sPendingDoc));` |
|       299 | 3583 | `	if( zDup ){` |
|       299 | 3584 | `		SyStringInitFromBuf(pOut,zDup,SyStringLength(&pGen->sPendingDoc));` |
|       147 | 3585 | `	}` |
|       299 | 3586 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|     85857 | 3587 | `}` |
|         - | 3588 | `/*` |
|         - | 3589 | ` * Compile one recorded #[...] attribute group (the span between the group` |
|         - | 3590 | ` * delimiters) into ph7_attribute records appended to pOut. The span is` |
|         - | 3591 | ` * duplicated into the VM allocator FIRST (compiled bytecode and interned` |
|         - | 3592 | ` * names may point into the token text, which must outlive the raw script` |
|         - | 3593 | ` * buffer), then re-tokenized on its own. Each argument expression compiles` |
|         - | 3594 | ` * with the container-swap idiom into its own OP_DONE-terminated set,` |
|         - | 3595 | ` * evaluated lazily at ReflectionAttribute time (PHP semantics).` |
|         - | 3596 | ` */` |
|       662 | 3597 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut)` |
|         5 | 3598 | `{` |
|         - | 3599 | `	SySet *pToken;` |
|         - | 3600 | `	SyToken *pIn, *pEnd, *pSavedIn, *pSavedEnd;` |
|         - | 3601 | `	char *zSpan;` |
|       667 | 3602 | `	sxi32 rc = SXRET_OK;` |
|       667 | 3603 | `	if( SyStringLength(&pTrivia->sText) < 1 ){` |
|       ! 0 | 3604 | `		return SXRET_OK;` |
|         - | 3605 | `	}` |
|       998 | 3606 | `	zSpan = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       331 | 3607 | `		SyStringData(&pTrivia->sText),SyStringLength(&pTrivia->sText));` |
|       667 | 3608 | `	if( zSpan == 0 ){` |
|       ! 0 | 3609 | `		return SXRET_OK;` |
|         - | 3610 | `	}` |
|         - | 3611 | `	/* The token set must outlive compilation too: interned operands may` |
|         - | 3612 | `	 * reference token payloads. Pool-allocated, never released — bounded by` |
|         - | 3613 | `	 * the number of attribute declarations in the program. */` |
|       667 | 3614 | `	pToken = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|       667 | 3615 | `	if( pToken == 0 ){` |
|       ! 0 | 3616 | `		return SXRET_OK;` |
|         - | 3617 | `	}` |
|       667 | 3618 | `	SySetInit(pToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       667 | 3619 | `	PH7_TokenizePHP(zSpan,SyStringLength(&pTrivia->sText),pTrivia->nLine,pToken,0);` |
|       667 | 3620 | `	pIn = (SyToken *)SySetBasePtr(pToken);` |
|       667 | 3621 | `	pEnd = &pIn[SySetUsed(pToken)];` |
|       667 | 3622 | `	pSavedIn = pGen->pIn;` |
|       667 | 3623 | `	pSavedEnd = pGen->pEnd;` |
|       671 | 3624 | `	while( pIn < pEnd ){` |
|         - | 3625 | `		ph7_attribute sAttr;` |
|         - | 3626 | `		SyBlob sFQN;` |
|       671 | 3627 | `		int bAbsolute = 0;` |
|       671 | 3628 | `		SyZero(&sAttr,sizeof(sAttr));` |
|       671 | 3629 | `		SySetInit(&sAttr.aArgs,&pGen->pVm->sAllocator,sizeof(ph7_attr_arg));` |
|       671 | 3630 | `		sAttr.nLine = pIn->nLine;` |
|       671 | 3631 | `		if( pIn->nType & PH7_TK_NSSEP ){` |
|       479 | 3632 | `			bAbsolute = 1;` |
|       479 | 3633 | `			pIn++;` |
|       237 | 3634 | `		}` |
|       671 | 3635 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|         - | 3636 | ``		/* `#[namespace\Attr]` — the current namespace, absolute from there. */`` |
|       671 | 3637 | `		if( !bAbsolute && GenStateNsRelPrefix(pGen,&pIn,pEnd,&sFQN) ){` |
|       ! 0 | 3638 | `			bAbsolute = 1;` |
|       ! 0 | 3639 | `		}` |
|       681 | 3640 | `		while( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       681 | 3641 | `			SyBlobAppend(&sFQN,pIn->sData.zString,pIn->sData.nByte);` |
|       681 | 3642 | `			pIn++;` |
|       681 | 3643 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|        11 | 3644 | `				SyBlobAppend(&sFQN,"\\",1);` |
|        11 | 3645 | `				pIn++;` |
|        11 | 3646 | `				continue;` |
|         - | 3647 | `			}` |
|       671 | 3648 | `			break;` |
|       ! 0 | 3649 | `		}` |
|       671 | 3650 | `		if( SyBlobLength(&sFQN) < 1 ){` |
|         - | 3651 | `			/* Malformed group: stop quietly (the group was inert trivia before` |
|         - | 3652 | `			 * this feature; never turn it into a new fatal) */` |
|       ! 0 | 3653 | `			SyBlobRelease(&sFQN);` |
|       ! 0 | 3654 | `			break;` |
|         - | 3655 | `		}` |
|         - | 3656 | `		/* Resolve to an FQN: absolute names verbatim; else use-import alias,` |
|         - | 3657 | `		 * else current-namespace prefix (PHP attribute name resolution) */` |
|         - | 3658 | `		{` |
|       671 | 3659 | `			const char *zName = (const char *)SyBlobData(&sFQN);` |
|       671 | 3660 | `			sxu32 nName = SyBlobLength(&sFQN);` |
|       671 | 3661 | `			char *zDup = 0;` |
|       671 | 3662 | `			if( !bAbsolute ){` |
|         - | 3663 | `				/* An attribute name resolves exactly the way every other class name` |
|         - | 3664 | `				 * does -- the LEADING segment through the use imports, else the` |
|         - | 3665 | `				 * current-namespace prefix. This looked the WHOLE qualified string up` |
|         - | 3666 | `				 * in the import table, which can never match a single-segment alias,` |
|         - | 3667 | ``				 * and then prefixed the namespace anyway: `use Vv as Rule;` with`` |
|         - | 3668 | ``				 * `#[Rule\\A]` asked for `App\\Rule\\A` and got`` |
|         - | 3669 | ``				 * `Attribute class ... not found`. It is the same mistake`` |
|         - | 3670 | `				 * GenStateResolveName was written to fix for the other name positions,` |
|         - | 3671 | `				 * so it is that function's job here too. */` |
|         - | 3672 | `				SyBlob sTmp;` |
|         - | 3673 | `				SyString sRaw;` |
|       196 | 3674 | `				SyStringInitFromBuf(&sRaw,zName,nName);` |
|       196 | 3675 | `				SyBlobInit(&sTmp,&pGen->pVm->sAllocator);` |
|       196 | 3676 | `				GenStateResolveName(&(*pGen),&sRaw,&sTmp);` |
|       196 | 3677 | `				if( SyBlobLength(&sTmp) > 0 ){` |
|       292 | 3678 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       192 | 3679 | `						(const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|       196 | 3680 | `					if( zDup ){` |
|       196 | 3681 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyBlobLength(&sTmp));` |
|        96 | 3682 | `					}` |
|        96 | 3683 | `				}` |
|       196 | 3684 | `				SyBlobRelease(&sTmp);` |
|        96 | 3685 | `			}` |
|       671 | 3686 | `			if( SyStringLength(&sAttr.sName) < 1 ){` |
|       479 | 3687 | `				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|       479 | 3688 | `				if( zDup ){` |
|       479 | 3689 | `					SyStringInitFromBuf(&sAttr.sName,zDup,nName);` |
|       237 | 3690 | `				}` |
|       237 | 3691 | `			}` |
|         - | 3692 | `		}` |
|       671 | 3693 | `		SyBlobRelease(&sFQN);` |
|       671 | 3694 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 3695 | `			SyToken *pArgsEnd;` |
|       137 | 3696 | `			pIn++;` |
|       137 | 3697 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pArgsEnd);` |
|       349 | 3698 | `			while( pIn < pArgsEnd ){` |
|       217 | 3699 | `				SyToken *pArgStart = pIn, *pArgStop = pIn;` |
|       217 | 3700 | `				sxi32 iDepth = 0;` |
|         - | 3701 | `				ph7_attr_arg sArgRec;` |
|       835 | 3702 | `				while( pArgStop < pArgsEnd ){` |
|       703 | 3703 | `					if( pArgStop->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        45 | 3704 | `						iDepth++;` |
|       682 | 3705 | `					}else if( pArgStop->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        45 | 3706 | `						iDepth--;` |
|       640 | 3707 | `					}else if( (pArgStop->nType & PH7_TK_COMMA) && iDepth == 0 ){` |
|        84 | 3708 | `						break;` |
|         - | 3709 | `					}` |
|       621 | 3710 | `					pArgStop++;` |
|         3 | 3711 | `				}` |
|       217 | 3712 | `				SyZero(&sArgRec,sizeof(sArgRec));` |
|       217 | 3713 | `				SySetInit(&sArgRec.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       214 | 3714 | `				if( pArgStart < pArgStop && (pArgStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|       147 | 3715 | `				 && &pArgStart[1] < pArgStop && (pArgStart[1].nType & PH7_TK_COLON) ){` |
|        41 | 3716 | `					char *zN = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        13 | 3717 | `						pArgStart->sData.zString,pArgStart->sData.nByte);` |
|        28 | 3718 | `					if( zN ){` |
|        28 | 3719 | `						SyStringInitFromBuf(&sArgRec.sName,zN,pArgStart->sData.nByte);` |
|        13 | 3720 | `					}` |
|        28 | 3721 | `					pArgStart += 2;` |
|        13 | 3722 | `				}` |
|       217 | 3723 | `				if( pArgStart < pArgStop ){` |
|         - | 3724 | `					SySet *pInstrContainer;` |
|         - | 3725 | `					const char *zCErr;` |
|       217 | 3726 | `					pGen->pIn = pArgStart;` |
|       217 | 3727 | `					pGen->pEnd = pArgStop;` |
|         - | 3728 | `					/* An attribute argument is a constant expression -- php applies the` |
|         - | 3729 | ``					 * same rules it applies to a class constant, `new` excepted (an`` |
|         - | 3730 | `					 * attribute argument takes one). This is the argument's own rule,` |
|         - | 3731 | `					 * not the malformed-group case a few lines up, so it IS a fatal. */` |
|       217 | 3732 | `					zCErr = PH7_GenStateConstExprError(pGen,1);` |
|       217 | 3733 | `					if( zCErr ){` |
|         3 | 3734 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pArgStart->nLine,"%s",zCErr);` |
|         3 | 3735 | `						pGen->pIn = pSavedIn;` |
|         3 | 3736 | `						pGen->pEnd = pSavedEnd;` |
|         3 | 3737 | `						return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - | 3738 | `					}` |
|       215 | 3739 | `					pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       215 | 3740 | `					PH7_VmSetByteCodeContainer(pGen->pVm,&sArgRec.aByteCode);` |
|       215 | 3741 | `					rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|       215 | 3742 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|       215 | 3743 | `					PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       215 | 3744 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 3745 | `						pGen->pIn = pSavedIn;` |
|       ! 0 | 3746 | `						pGen->pEnd = pSavedEnd;` |
|       ! 0 | 3747 | `						return SXERR_ABORT;` |
|         - | 3748 | `					}` |
|       215 | 3749 | `					SySetPut(&sAttr.aArgs,(const void *)&sArgRec);` |
|       106 | 3750 | `				}` |
|       215 | 3751 | `				pIn = pArgStop;` |
|       215 | 3752 | `				if( pIn < pArgsEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        84 | 3753 | `					pIn++;` |
|        41 | 3754 | `				}` |
|         3 | 3755 | `			}` |
|       135 | 3756 | `			pIn = (pArgsEnd < pEnd) ? &pArgsEnd[1] : pEnd;` |
|        66 | 3757 | `		}` |
|       669 | 3758 | `		SySetPut(pOut,(const void *)&sAttr);` |
|       669 | 3759 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|         5 | 3760 | `			pIn++;` |
|         5 | 3761 | `			continue;` |
|         - | 3762 | `		}` |
|       665 | 3763 | `		break;` |
|       ! 0 | 3764 | `	}` |
|       665 | 3765 | `	pGen->pIn = pSavedIn;` |
|       665 | 3766 | `	pGen->pEnd = pSavedEnd;` |
|       665 | 3767 | `	return SXRET_OK;` |
|       336 | 3768 | `}` |
|         - | 3769 | `/*` |
|         - | 3770 | ` * Hand the pending attribute groups (if any) to a declaration: compile` |
|         - | 3771 | ` * every recorded group into pOut and clear the pending list.` |
|         - | 3772 | ` */` |
|    171953 | 3773 | `PH7_PRIVATE sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut)` |
|         5 | 3774 | `{` |
|    171958 | 3775 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|         - | 3776 | `	sxu32 n;` |
|         - | 3777 | `	sxi32 rc;` |
|    172574 | 3778 | `	for( n = 0 ; n < SySetUsed(&pGen->aPendingAttrs) ; n++ ){` |
|       621 | 3779 | `		rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|       621 | 3780 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 3781 | `			return SXERR_ABORT;` |
|         - | 3782 | `		}` |
|       313 | 3783 | `	}` |
|    171958 | 3784 | `	SySetReset(&pGen->aPendingAttrs);` |
|    171958 | 3785 | `	return SXRET_OK;` |
|     85861 | 3786 | `}` |
|         - | 3787 | `/*` |
|         - | 3788 | ` * Compile the attribute groups keyed to the given token (a parameter's` |
|         - | 3789 | ` * first token inside a signature) into pOut. Parameters are parsed from` |
|         - | 3790 | ` * the main token stream, so the sidecar indexes map directly.` |
|         - | 3791 | ` */` |
|    239986 | 3792 | `PH7_PRIVATE sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut)` |
|         5 | 3793 | `{` |
|    239991 | 3794 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|    239991 | 3795 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|    239991 | 3796 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 3797 | `	sxu32 nIdx, n;` |
|         - | 3798 | `	sxi32 rc;` |
|    239986 | 3799 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|      1729 | 3800 | `	 \|\| pTok < pBase \|\| pTok >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|    238267 | 3801 | `		return SXRET_OK;` |
|         - | 3802 | `	}` |
|      1729 | 3803 | `	nIdx = (sxu32)(pTok - pBase);` |
|      8623 | 3804 | `	for( n = 0 ; n < nT ; n++ ){` |
|      6899 | 3805 | `		if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|        50 | 3806 | `			rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|        50 | 3807 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 3808 | `				return SXERR_ABORT;` |
|         - | 3809 | `			}` |
|        23 | 3810 | `		}` |
|      3452 | 3811 | `	}` |
|      1729 | 3812 | `	return SXRET_OK;` |
|    119722 | 3813 | `}` |
|         - | 3814 | `/*` |
|         - | 3815 | ` * ---------------------------------------------------------------------------` |
|         - | 3816 | ` * Where php's OWN attributes may be written.` |
|         - | 3817 | ` *` |
|         - | 3818 | ` * php's seven internal attribute classes each carry a target mask and a` |
|         - | 3819 | ` * validator, and the engine runs them where the declaration COMPILES: a` |
|         - | 3820 | `` * misplaced `#[\Attribute]`, `#[\Override]` or `#[\NoDiscard]` is a fatal at`` |
|         - | 3821 | ` * the line it sits on, before anything else in the file runs. A USERLAND` |
|         - | 3822 | ` * attribute is different — php checks its mask only when someone asks for it,` |
|         - | 3823 | `` * at `newInstance()` — so this table is closed on purpose and unknown names go`` |
|         - | 3824 | ` * unchecked, which is php's behaviour and not an omission.` |
|         - | 3825 | ` *` |
|         - | 3826 | ` * The masks are the same seven the classes declare (see VmInstallAttributes);` |
|         - | 3827 | ` * they are repeated here because the compiler runs before any class exists.` |
|         - | 3828 | ` * None of the seven is IS_REPEATABLE, so a second one is php's own refusal.` |
|         - | 3829 | ` * ---------------------------------------------------------------------------` |
|         - | 3830 | ` */` |
|         - | 3831 | `/* php's Attribute::TARGET_* names, in bit order (TARGET_CLASS is bit 0). */` |
|         - | 3832 | `static const char *const azGenAttrTarget[] = {` |
|         - | 3833 | `	"class","function","method","property","class constant","parameter","constant"` |
|         - | 3834 | `};` |
|         - | 3835 | `static const struct {` |
|         - | 3836 | `	const char *zName;` |
|         - | 3837 | `	int iMask;` |
|         - | 3838 | `} aGenInternalAttr[] = {` |
|         - | 3839 | `	{ "Attribute",              1  },` |
|         - | 3840 | `	{ "Deprecated",             87 },` |
|         - | 3841 | `	{ "AllowDynamicProperties", 1  },` |
|         - | 3842 | `	{ "SensitiveParameter",     32 },` |
|         - | 3843 | `	{ "ReturnTypeWillChange",   4  },` |
|         - | 3844 | `	{ "Override",               12 },` |
|         - | 3845 | `	{ "NoDiscard",              6  },` |
|         - | 3846 | `};` |
|         - | 3847 | `/*` |
|         - | 3848 | ` * The extra validator php gives three of them, asked only once the target is` |
|         - | 3849 | ` * known to be a CLASS: the mask says "a class" and these say WHICH kinds.` |
|         - | 3850 | ` * Answers php's noun for the refused kind, or 0 when the class is acceptable.` |
|         - | 3851 | ` */` |
|        98 | 3852 | `static const char * GenStateAttrClassRefusal(const char *zAttr,sxi32 iFlags)` |
|         5 | 3853 | `{` |
|         - | 3854 | `	/* zAttr is a row of aGenInternalAttr, so an exact compare is the whole test. */` |
|       103 | 3855 | `	int bAttr = SyStrncmp(zAttr,"Attribute",sizeof("Attribute")) == 0;` |
|       103 | 3856 | `	int bDyn  = SyStrncmp(zAttr,"AllowDynamicProperties",sizeof("AllowDynamicProperties")) == 0;` |
|       103 | 3857 | `	int bDep  = SyStrncmp(zAttr,"Deprecated",sizeof("Deprecated")) == 0;` |
|       103 | 3858 | `	if( !bAttr && !bDyn && !bDep ){` |
|       ! 0 | 3859 | `		return 0;` |
|         - | 3860 | `	}` |
|       103 | 3861 | `	if( iFlags & PH7_CLASS_INTERFACE ){ return "interface"; }` |
|        97 | 3862 | `	if( iFlags & PH7_CLASS_ENUM ){ return "enum"; }` |
|        91 | 3863 | `	if( iFlags & PH7_CLASS_TRAIT ){` |
|         - | 3864 | `		/* php 8.5 DOES mark a deprecated trait; the other two refuse one. */` |
|         7 | 3865 | `		return bDep ? 0 : "trait";` |
|         - | 3866 | `	}` |
|        85 | 3867 | `	if( bAttr ){` |
|         - | 3868 | `		/* An attribute class must be instantiable. */` |
|        61 | 3869 | `		return (iFlags & PH7_CLASS_ABSTRACT) ? "abstract class" : 0;` |
|         - | 3870 | `	}` |
|        26 | 3871 | `	if( bDyn ){` |
|         - | 3872 | `		/* A readonly class has no dynamic property to allow. */` |
|        22 | 3873 | `		return (iFlags & PH7_CLASS_READONLY) ? "readonly class" : 0;` |
|         - | 3874 | `	}` |
|         5 | 3875 | `	return "class";   /* #[\Deprecated] on any other class kind */` |
|        54 | 3876 | `}` |
|         - | 3877 | `/*` |
|         - | 3878 | ` * Validate one declaration's attribute set against php's placement rules.` |
|         - | 3879 | ` *` |
|         - | 3880 | ` * iTarget is the single Attribute::TARGET_* bit php NAMES for this declaration` |
|         - | 3881 | ` * and iAccept the mask it accepts, which differ in exactly one place: a PROMOTED` |
|         - | 3882 | ` * constructor parameter is a parameter and a property both, so it takes either` |
|         - | 3883 | ` * bit while still reporting "parameter". pClassName/iClassFlags describe the` |
|         - | 3884 | ` * subject when the target is a class (0 and 0 otherwise).` |
|         - | 3885 | ` */` |
|    411771 | 3886 | `PH7_PRIVATE sxi32 GenStateCheckAttrPlacement(ph7_gen_state *pGen,SySet *pAttrs,` |
|         - | 3887 | `	int iTarget,int iAccept,const SyString *pClassName,sxi32 iClassFlags)` |
|         5 | 3888 | `{` |
|    411776 | 3889 | `	ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|         - | 3890 | `	sxu32 n,k;` |
|    412358 | 3891 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; ++n ){` |
|       663 | 3892 | `		SyString *pName = &aAttr[n].sName;` |
|         - | 3893 | `		sxu32 iRow;` |
|      3671 | 3894 | `		for( iRow = 0 ; iRow < SX_ARRAYSIZE(aGenInternalAttr) ; ++iRow ){` |
|      3524 | 3895 | `			if( pName->nByte == (sxu32)SyStrlen(aGenInternalAttr[iRow].zName)` |
|      2132 | 3896 | `			 && SyStrnicmp(pName->zString,aGenInternalAttr[iRow].zName,pName->nByte) == 0 ){` |
|       521 | 3897 | `				break;` |
|         - | 3898 | `			}` |
|      1509 | 3899 | `		}` |
|       663 | 3900 | `		if( iRow >= SX_ARRAYSIZE(aGenInternalAttr) ){` |
|       146 | 3901 | `			continue;   /* a userland attribute: judged at newInstance(), not here */` |
|         - | 3902 | `		}` |
|       521 | 3903 | `		if( (aGenInternalAttr[iRow].iMask & iAccept) == 0 ){` |
|         - | 3904 | `			SyBlob sAllowed;` |
|         - | 3905 | `			int iBit;` |
|         - | 3906 | `			sxi32 rc;` |
|        47 | 3907 | `			SyBlobInit(&sAllowed,&pGen->pVm->sAllocator);` |
|       369 | 3908 | `			for( iBit = 0 ; iBit < (int)SX_ARRAYSIZE(azGenAttrTarget) ; iBit++ ){` |
|       323 | 3909 | `				if( (aGenInternalAttr[iRow].iMask & (1 << iBit)) == 0 ){` |
|       239 | 3910 | `					continue;` |
|         - | 3911 | `				}` |
|        85 | 3912 | `				if( SyBlobLength(&sAllowed) > 0 ){` |
|        39 | 3913 | `					SyBlobAppend(&sAllowed,", ",sizeof(", ")-1);` |
|        19 | 3914 | `				}` |
|       127 | 3915 | `				SyBlobAppend(&sAllowed,azGenAttrTarget[iBit],` |
|        84 | 3916 | `					(sxu32)SyStrlen(azGenAttrTarget[iBit]));` |
|        43 | 3917 | `			}` |
|        47 | 3918 | `			SyBlobAppend(&sAllowed,"",sizeof(char));   /* NUL for the %s below */` |
|       171 | 3919 | `			for( iBit = 0 ; iBit < (int)SX_ARRAYSIZE(azGenAttrTarget) ; iBit++ ){` |
|       171 | 3920 | `				if( iTarget == (1 << iBit) ){` |
|        47 | 3921 | `					break;` |
|         - | 3922 | `				}` |
|        63 | 3923 | `			}` |
|        70 | 3924 | `			rc = PH7_GenCompileError(pGen,E_ERROR,aAttr[n].nLine,` |
|        23 | 3925 | `				"Attribute \"%z\" cannot target %s (allowed targets: %s)",pName,` |
|        23 | 3926 | `				iBit < (int)SX_ARRAYSIZE(azGenAttrTarget) ? azGenAttrTarget[iBit] : "",` |
|        23 | 3927 | `				SyBlobData(&sAllowed));` |
|        47 | 3928 | `			SyBlobRelease(&sAllowed);` |
|        47 | 3929 | `			return rc;` |
|         - | 3930 | `		}` |
|         - | 3931 | `		/* ...then repetition, which is what php checks second: the FIRST of a` |
|         - | 3932 | `		 * misplaced pair reports its target instead. */` |
|       475 | 3933 | `		for( k = 0 ; k < n ; ++k ){` |
|         6 | 3934 | `			if( aAttr[k].sName.nByte == pName->nByte` |
|         7 | 3935 | `			 && SyStrnicmp(aAttr[k].sName.zString,pName->zString,pName->nByte) == 0 ){` |
|        10 | 3936 | `				return PH7_GenCompileError(pGen,E_ERROR,aAttr[n].nLine,` |
|         3 | 3937 | `					"Attribute \"%z\" must not be repeated",pName);` |
|         - | 3938 | `			}` |
|       ! 0 | 3939 | `		}` |
|       469 | 3940 | `		if( iTarget == 1 && pClassName ){` |
|       152 | 3941 | `			const char *zRefused = GenStateAttrClassRefusal(aGenInternalAttr[iRow].zName,` |
|        49 | 3942 | `				iClassFlags);` |
|       103 | 3943 | `			if( zRefused ){` |
|        37 | 3944 | `				return PH7_GenCompileError(pGen,E_ERROR,aAttr[n].nLine,` |
|        24 | 3945 | `					"Cannot apply #[\\%s] to %s %z",aGenInternalAttr[iRow].zName,` |
|        12 | 3946 | `					zRefused,pClassName);` |
|         - | 3947 | `			}` |
|        37 | 3948 | `		}` |
|       225 | 3949 | `	}` |
|    411700 | 3950 | `	return SXRET_OK;` |
|    205494 | 3951 | `}` |
|         - | 3952 | `/*` |
|         - | 3953 | `` * php 8.5's `(void)` cast is a STATEMENT prefix, not an expression operator:`` |
|         - | 3954 | `` * `$x = (void) f();` and `return (void) f();` are parse errors there too, and`` |
|         - | 3955 | ` * the only thing it does is say that dropping the answer is DELIBERATE, which` |
|         - | 3956 | ` * silences a #[\NoDiscard] callee. The lexer already assembled the three tokens` |
|         - | 3957 | ` * into one (PH7_TK_VOID_CAST); this consumes it and answers 1.` |
|         - | 3958 | ` */` |
|    839488 | 3959 | `PH7_PRIVATE int GenStateTakeVoidCast(ph7_gen_state *pGen)` |
|         5 | 3960 | `{` |
|    839493 | 3961 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_VOID_CAST) ){` |
|        19 | 3962 | `		pGen->pIn++;` |
|        19 | 3963 | `		return 1;` |
|         - | 3964 | `	}` |
|    839475 | 3965 | `	return 0;` |
|    418975 | 3966 | `}` |
|         - | 3967 | `/*` |
|         - | 3968 | `` * php's grammar takes a `(void)` cast at the head of an expression STATEMENT and`` |
|         - | 3969 | `` * at the head of each element of a `for` clause list — `for ((void) f(), $i = 0;`` |
|         - | 3970 | `` * $i < 1; $i++, (void) g())` is all valid, while `for ($i = (void) f();;)` is`` |
|         - | 3971 | ` * not. The statement head is consumed by GenStateTakeVoidCast; a clause is one` |
|         - | 3972 | ` * expression with comma operators in it, so its element heads are marked HERE,` |
|         - | 3973 | ` * before it compiles: the token becomes the no-op cast operator parse.c declares,` |
|         - | 3974 | `` * and every other `(void)` in the clause stays unrecognized, which is php's own`` |
|         - | 3975 | ` * refusal. Nothing is moved or removed — the token stream is shared with the` |
|         - | 3976 | ` * rest of the file.` |
|         - | 3977 | ` */` |
|    122443 | 3978 | `PH7_PRIVATE int GenStateEnableClauseVoidCasts(ph7_gen_state *pGen,int bLastToo)` |
|         5 | 3979 | `{` |
|    122448 | 3980 | `	SyToken *pTok = pGen->pIn,*pLastMark = 0;` |
|    122448 | 3981 | `	int iDepth = 0,bHead = 1,bCommaAfter = 0;` |
|    699486 | 3982 | `	for( ; pTok < pGen->pEnd ; pTok++ ){` |
|    658673 | 3983 | `		if( pTok->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     20208 | 3984 | `			iDepth++;` |
|    648558 | 3985 | `		}else if( pTok->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     20208 | 3986 | `			iDepth--;` |
|    628355 | 3987 | `		}else if( iDepth == 0 && (pTok->nType & PH7_TK_SEMI) ){` |
|         - | 3988 | `			/* The three clauses share one token range (only the post one is` |
|         - | 3989 | `			 * delimited), so this scan stops where its own clause does. */` |
|     40765 | 3990 | `			break;` |
|    536637 | 3991 | `		}else if( iDepth == 0 && (pTok->nType & PH7_TK_COMMA) ){` |
|        14 | 3992 | `			bHead = 1;` |
|        14 | 3993 | `			bCommaAfter = 1;` |
|        14 | 3994 | `			continue;` |
|    536625 | 3995 | `		}else if( iDepth == 0 && bHead && (pTok->nType & PH7_TK_VOID_CAST) ){` |
|         9 | 3996 | `			pTok->nType \|= PH7_TK_OP;` |
|         9 | 3997 | `			pTok->pUserData = (void *)PH7_ExprExtractOperator(&pTok->sData,0);` |
|         9 | 3998 | `			pLastMark = pTok;` |
|         9 | 3999 | `			bCommaAfter = 0;` |
|         4 | 4000 | `		}` |
|    577031 | 4001 | `		bHead = 0;` |
|    288130 | 4002 | `	}` |
|         - | 4003 | `	/* The CONDITION clause's last element is the condition VALUE, so php refuses a` |
|         - | 4004 | ``	 * `(void)` on that one and only that one: `for (;(void) f();)` is a parse error`` |
|         - | 4005 | ``	 * where `for (;(void) f(), $i < 1;)` is fine. */`` |
|    122448 | 4006 | `	if( !bLastToo && pLastMark && !bCommaAfter ){` |
|         3 | 4007 | `		pLastMark->nType &= ~(sxu32)PH7_TK_OP;` |
|         3 | 4008 | `		pLastMark->pUserData = 0;` |
|         3 | 4009 | `		return 1;` |
|         - | 4010 | `	}` |
|    122446 | 4011 | `	return 0;` |
|     61144 | 4012 | `}` |
|         - | 4013 | `/*` |
|         - | 4014 | ` * The statement is about to throw its expression's value away. When that value` |
|         - | 4015 | ` * came straight out of a CALL, mark the call: php's !RETURN_VALUE_USED, which is` |
|         - | 4016 | `` * what a #[\NoDiscard] callee reads. `f() + 1;` drops the ADD's result, not the`` |
|         - | 4017 | ` * call's, so only the last instruction is looked at.` |
|         - | 4018 | ` */` |
|    920870 | 4019 | `PH7_PRIVATE void GenStateMarkDiscardedCall(ph7_gen_state *pGen)` |
|         5 | 4020 | `{` |
|    920875 | 4021 | `	VmInstr *pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    920875 | 4022 | `	if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|    134965 | 4023 | `		pInstr->bDiscard = 1;` |
|     67200 | 4024 | `	}` |
|    920875 | 4025 | `}` |
|         - | 4026 | `/* TRUE when the cursor has run past the LAST token of a chunk that met the end of` |
|         - | 4027 | ``  * the file. A statement slice can end early (a single statement inside a `for` `` |
|         - | 4028 | `` * header), so `pIn >= pEnd` alone is not the question. */`` |
|       120 | 4029 | `static int GenStateAtChunkEof(ph7_gen_state *pGen)` |
|         4 | 4030 | `{` |
|         - | 4031 | `	SyToken *pBase;` |
|       124 | 4032 | `	if( !pGen->bChunkAtEof \|\| pGen->pTokenSet == 0 ){` |
|        66 | 4033 | `		return 0;` |
|         - | 4034 | `	}` |
|        60 | 4035 | `	pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        60 | 4036 | `	return pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)];` |
|        64 | 4037 | `}` |
|         - | 4038 | `/*` |
|         - | 4039 | ` * php's grammar wants a TERMINATOR after a statement, and the end of the file is` |
|         - | 4040 | `` * not one: `<?php echo "a"` is `syntax error, unexpected end of file, expecting`` |
|         - | 4041 | `` * "," or ";"` there, while PHL ran it and exited 0. (A `?>` IS a terminator, which`` |
|         - | 4042 | `` * is why `<?php echo "a" ?>` is legal in both engines and why the check only`` |
|         - | 4043 | ` * applies to a chunk that met the end of the FILE.)` |
|         - | 4044 | ` *` |
|         - | 4045 | `` * A statement that ends in `}` -- a block, a declaration, a braced control`` |
|         - | 4046 | `` * structure -- needs nothing, and neither does one whose `;` the loop just`` |
|         - | 4047 | ` * stepped over; everything else was left unfinished.` |
|         - | 4048 | ` *` |
|         - | 4049 | ` * Answers the "expecting" clause php names for the statement that ran out, or 0` |
|         - | 4050 | ` * when php names none. php reports the set its parser was in, which for a` |
|         - | 4051 | ` * statement is decided by the KEYWORD it opened with -- the comma-list statements` |
|         - | 4052 | `` * may take another element, `return`/`break`/`continue`/`goto`/`unset` and a`` |
|         - | 4053 | `` * do-while may not, `namespace` still wants its block -- except when the last`` |
|         - | 4054 | `` * token consumed was an alternative-syntax `end*`, whose own `;` is what is`` |
|         - | 4055 | ` * missing.` |
|         - | 4056 | ` */` |
|        50 | 4057 | `static const char * GenStateEofExpecting(SyToken *pStmt,SyToken *pLast)` |
|         1 | 4058 | `{` |
|         - | 4059 | `	sxu32 nKw;` |
|        51 | 4060 | `	if( pLast && (pLast->nType & PH7_TK_KEYWORD) ){` |
|         6 | 4061 | `		nKw = (sxu32)SX_PTR_TO_INT(pLast->pUserData);` |
|         6 | 4062 | `		if( nKw == PH7_TKWRD_ENDIF \|\| nKw == PH7_TKWRD_ENDWHILE \|\| nKw == PH7_TKWRD_ENDFOR` |
|         2 | 4063 | `		 \|\| nKw == PH7_TKWRD_END4EACH \|\| nKw == PH7_TKWRD_ENDSWITCH ){` |
|         4 | 4064 | `			return "\";\"";` |
|         - | 4065 | `		}` |
|         1 | 4066 | `	}` |
|        47 | 4067 | `	if( pStmt == 0 \|\| (pStmt->nType & PH7_TK_KEYWORD) == 0 ){` |
|        13 | 4068 | `		return 0;` |
|         - | 4069 | `	}` |
|        34 | 4070 | `	nKw = (sxu32)SX_PTR_TO_INT(pStmt->pUserData);` |
|        34 | 4071 | `	if( nKw == PH7_TKWRD_ECHO \|\| nKw == PH7_TKWRD_GLOBAL \|\| nKw == PH7_TKWRD_STATIC` |
|        23 | 4072 | `	 \|\| nKw == PH7_TKWRD_CONST \|\| nKw == PH7_TKWRD_USE ){` |
|        16 | 4073 | `		return "\",\" or \";\"";` |
|         - | 4074 | `	}` |
|        18 | 4075 | `	if( nKw == PH7_TKWRD_RETURN \|\| nKw == PH7_TKWRD_BREAK \|\| nKw == PH7_TKWRD_CONTINUE` |
|        14 | 4076 | `	 \|\| nKw == PH7_TKWRD_GOTO \|\| nKw == PH7_TKWRD_UNSET \|\| nKw == PH7_TKWRD_DO ){` |
|        10 | 4077 | `		return "\";\"";` |
|         - | 4078 | `	}` |
|         8 | 4079 | `	if( nKw == PH7_TKWRD_NAMESPACE ){` |
|         2 | 4080 | `		return "\"{\"";` |
|         - | 4081 | `	}` |
|         6 | 4082 | `	return 0;` |
|        26 | 4083 | `}` |
|         - | 4084 | ``/* Does this script spell `__halt_compiler` at all, in any case? A cheap scan`` |
|         - | 4085 | ` * that keeps the token pass below off every ordinary file. */` |
|     34872 | 4086 | `static int GenStateMentionsHalt(const char *zIn,sxu32 nIn)` |
|         5 | 4087 | `{` |
|         - | 4088 | `	static const char zWord[] = "__halt_compiler";` |
|     34877 | 4089 | `	sxu32 nWord = (sxu32)sizeof(zWord)-1;` |
|         - | 4090 | `	sxu32 i;` |
| 122832934 | 4091 | `	for( i = 0 ; i + nWord <= nIn ; ++i ){` |
| 122798103 | 4092 | `		if( zIn[i] != '_' ){` |
| 121694121 | 4093 | `			continue;` |
|         - | 4094 | `		}` |
|   1103987 | 4095 | `		if( SyStrnicmp(&zIn[i],zWord,nWord) == 0 ){` |
|        43 | 4096 | `			return 1;` |
|         - | 4097 | `		}` |
|    550974 | 4098 | `	}` |
|     34836 | 4099 | `	return 0;` |
|     17432 | 4100 | `}` |
|         - | 4101 | ``/* Is this token the `__halt_compiler` identifier? It is not a keyword in this`` |
|         - | 4102 | ` * lexer (the generated table takes nothing longer than twelve bytes), so it` |
|         - | 4103 | ` * arrives as an ordinary identifier and is recognised by NAME -- case` |
|         - | 4104 | ` * insensitively, as php's own scanner does. */` |
|   2063651 | 4105 | `static int GenStateIsHaltCompiler(SyToken *pTok,SyToken *pEnd)` |
|         5 | 4106 | `{` |
|   2059455 | 4107 | `	return pTok < pEnd` |
|   2063651 | 4108 | `	    && (pTok->nType & PH7_TK_ID)` |
|   1095658 | 4109 | `	    && pTok->sData.nByte == sizeof("__halt_compiler")-1` |
|   3097577 | 4110 | `	    && SyStrnicmp(pTok->sData.zString,"__halt_compiler",sizeof("__halt_compiler")-1) == 0;` |
|         5 | 4111 | `}` |
|         - | 4112 | `/*` |
|         - | 4113 | `` * The pre-scan behind `__COMPILER_HALT_OFFSET__`: find the halt statement in`` |
|         - | 4114 | `` * whichever PHP chunk holds it and remember the byte just past its `;`. The`` |
|         - | 4115 | ` * chunks are tokenized a second time here -- the compile below tokenizes each` |
|         - | 4116 | ` * one as it reaches it -- because the constant's value has to be known before` |
|         - | 4117 | ` * the first statement compiles. Only a file that spells the identifier gets` |
|         - | 4118 | ` * here at all.` |
|         - | 4119 | ` *` |
|         - | 4120 | `` * A `__halt_compiler` in a scope php refuses is still found: the statement`` |
|         - | 4121 | ` * compiler raises php's fatal when it reaches it, and the offset is never read.` |
|         - | 4122 | ` */` |
|        41 | 4123 | `static void GenStateScanHaltOffset(ph7_gen_state *pGen,SySet *pRawToken,const char *zFileBase)` |
|         2 | 4124 | `{` |
|        43 | 4125 | `	SyToken *pRaw = (SyToken *)SySetBasePtr(pRawToken);` |
|        43 | 4126 | `	SyToken *pRawEnd = &pRaw[SySetUsed(pRawToken)];` |
|         - | 4127 | `	SySet aTok,aTriv;` |
|        43 | 4128 | `	SySetInit(&aTok,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        43 | 4129 | `	SySetInit(&aTriv,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       129 | 4130 | `	for( ; pRaw < pRawEnd && pGen->bHaltSeen == 0 ; pRaw++ ){` |
|         - | 4131 | `		SyToken *pTok,*pEnd;` |
|        88 | 4132 | `		if( (pRaw->nType & PH7_TOKEN_PHP) == 0 ){` |
|        45 | 4133 | `			continue;` |
|         - | 4134 | `		}` |
|        45 | 4135 | `		SySetReset(&aTok);` |
|        45 | 4136 | `		SySetReset(&aTriv);` |
|        66 | 4137 | `		PH7_TokenizePHP(SyStringData(&pRaw->sData),SyStringLength(&pRaw->sData),` |
|        21 | 4138 | `			pRaw->nLine,&aTok,&aTriv);` |
|        45 | 4139 | `		pTok = (SyToken *)SySetBasePtr(&aTok);` |
|        45 | 4140 | `		pEnd = &pTok[SySetUsed(&aTok)];` |
|      7252 | 4141 | `		for( ; pTok < pEnd ; pTok++ ){` |
|      7241 | 4142 | `			if( !GenStateIsHaltCompiler(pTok,pEnd) ){` |
|      7209 | 4143 | `				continue;` |
|         - | 4144 | `			}` |
|        32 | 4145 | `			if( pTok + 3 < pEnd` |
|        30 | 4146 | `			 && (pTok[1].nType & PH7_TK_LPAREN)` |
|        28 | 4147 | `			 && (pTok[2].nType & PH7_TK_RPAREN)` |
|        29 | 4148 | `			 && (pTok[3].nType & PH7_TK_SEMI) ){` |
|        29 | 4149 | `				const char *zSemi = SyStringData(&pTok[3].sData);` |
|        29 | 4150 | `				if( zSemi > zFileBase ){` |
|        29 | 4151 | `					pGen->nHaltOffset = (sxu32)((zSemi - zFileBase) + 1);` |
|        29 | 4152 | `					pGen->bHaltSeen = 1;` |
|        14 | 4153 | `				}` |
|        14 | 4154 | `			}` |
|        33 | 4155 | `			break;` |
|       ! 0 | 4156 | `		}` |
|        23 | 4157 | `	}` |
|        43 | 4158 | `	SySetRelease(&aTok);` |
|        43 | 4159 | `	SySetRelease(&aTriv);` |
|        43 | 4160 | `}` |
|         - | 4161 | `/*` |
|         - | 4162 | ` * ---------------------------------------------------------------------------` |
|         - | 4163 | ``  * `__halt_compiler();` `` |
|         - | 4164 | ` *` |
|         - | 4165 | ` * php's scanner STOPS at it: the rest of the file is not code and is never` |
|         - | 4166 | ` * output either, which is what lets a .phar carry a binary archive in the bytes` |
|         - | 4167 | ` * behind its stub. Three rules come with it, all php's:` |
|         - | 4168 | ` *` |
|         - | 4169 | ` *   - it is only legal at the OUTERMOST scope -- inside a function, a class or` |
|         - | 4170 | `` *     even a plain `if` block it is a compile-time fatal, not a parse error;`` |
|         - | 4171 | ` *   - the parentheses and the semicolon are part of the construct, and php's` |
|         - | 4172 | ` *     parser names what it wanted when one is missing;` |
|         - | 4173 | `` *   - `__COMPILER_HALT_OFFSET__` expands to the byte just past that `;`.`` |
|         - | 4174 | ` *` |
|         - | 4175 | ` * It is NOT a keyword in this lexer (the generated table takes nothing longer` |
|         - | 4176 | ` * than twelve bytes), so it arrives as an ordinary identifier in statement` |
|         - | 4177 | ` * position and is recognised by name -- case-insensitively, as php does.` |
|         - | 4178 | ` * ---------------------------------------------------------------------------` |
|         - | 4179 | ` */` |
|        94 | 4180 | `static sxi32 GenStateCompileHaltCompiler(ph7_gen_state *pGen)` |
|         1 | 4181 | `{` |
|        95 | 4182 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        95 | 4183 | `	if( pGen->pCurrent != &pGen->sGlobal ){` |
|         - | 4184 | `		/* php's own sentence: a FATAL rather than a parse error, and one its` |
|         - | 4185 | `		 * PARSER makes -- so it prints no stack trace under it. */` |
|        67 | 4186 | `		pGen->iFatalTrace = PH7_FATAL_TRACE_NONE;` |
|        67 | 4187 | `		return PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 4188 | `			"__HALT_COMPILER() can only be used from the outermost scope");` |
|         - | 4189 | `	}` |
|        29 | 4190 | `	pGen->pIn++;` |
|        29 | 4191 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         3 | 4192 | `		return PH7_GenSyntaxError(&(*pGen),` |
|         2 | 4193 | `			pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         - | 4194 | `	}` |
|         - | 4195 | ``	/* php's scanner ran out INSIDE the parentheses: it names the `(` it never`` |
|         - | 4196 | `	 * closed rather than the token it wanted, and reports it where the INPUT` |
|         - | 4197 | ``	 * ends rather than where the `(` is. */`` |
|         - | 4198 | `#define PHL_HALT_EOF_LINE (pGen->bChunkAtEof && pGen->nChunkEofLine > nLine \` |
|         - | 4199 | `	? pGen->nChunkEofLine : nLine)` |
|        27 | 4200 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 | 4201 | `		return PH7_GenCompileError(&(*pGen),E_PARSE,PHL_HALT_EOF_LINE,` |
|       ! 0 | 4202 | `			"Unclosed '(' on line %u",nLine);` |
|         - | 4203 | `	}` |
|        27 | 4204 | `	pGen->pIn++;` |
|        27 | 4205 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|         4 | 4206 | `		return pGen->pIn >= pGen->pEnd` |
|         2 | 4207 | `			? PH7_GenCompileError(&(*pGen),E_PARSE,PHL_HALT_EOF_LINE,` |
|         1 | 4208 | `				"Unclosed '(' on line %u",nLine)` |
|         2 | 4209 | `			: PH7_GenSyntaxError(&(*pGen),pGen->pIn,"\")\"");` |
|         - | 4210 | `	}` |
|        25 | 4211 | `	pGen->pIn++;` |
|        25 | 4212 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       ! 0 | 4213 | `		return PH7_GenSyntaxError(&(*pGen),` |
|       ! 0 | 4214 | `			pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|         - | 4215 | `	}` |
|        25 | 4216 | `	pGen->pIn++;` |
|         - | 4217 | `	/* Nothing after it is code, in this chunk or in any that follows. */` |
|        25 | 4218 | `	pGen->pIn = pGen->pEnd;` |
|        25 | 4219 | `	pGen->bHalted = 1;` |
|         - | 4220 | `#undef PHL_HALT_EOF_LINE` |
|        25 | 4221 | `	return SXRET_OK;` |
|        48 | 4222 | `}` |
|   1826712 | 4223 | `PH7_PRIVATE sxi32 GenStateCompileChunk(` |
|         - | 4224 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 4225 | `	sxi32 iFlags         /* Compile flags */` |
|         - | 4226 | `	)` |
|         5 | 4227 | `{` |
|         - | 4228 | `	ProcLangConstruct xCons;` |
|         - | 4229 | `	sxi32 rc;` |
|   1826717 | 4230 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|   1172522 | 4231 | `	for(;;){` |
|   2087155 | 4232 | `		int bStmtIsDeclare = 0;` |
|         - | 4233 | `		/* Whether php's grammar wants a TERMINATOR after this statement: a block, a` |
|         - | 4234 | `		 * declaration and a LABEL end themselves, everything else -- an expression` |
|         - | 4235 | ``		 * statement included, even one that ends in the `}` of a closure or a match`` |
|         - | 4236 | `		 * -- has to be closed. */` |
|   2087155 | 4237 | `		int bStmtWantsSemi = 1;` |
|         - | 4238 | `		SyToken *pStmtStart;` |
|   2087155 | 4239 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 4240 | `			/* No more input to process */` |
|     30743 | 4241 | `			break;` |
|         - | 4242 | `		}` |
|   2056417 | 4243 | `		pStmtStart = pGen->pIn; /* The keyword this statement opened with, for the` |
|         - | 4244 | `		                         * end-of-input check below */` |
|         - | 4245 | `		/* Bind a directly-preceding docblock to this statement */` |
|   2056417 | 4246 | `		GenStateSetPendingDoc(&(*pGen));` |
|   2056417 | 4247 | `		if( SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|         - | 4248 | `			/* php: a statement-position attribute group must be followed by a` |
|         - | 4249 | ``			 * declaration (function/class-like/const) — `#[A] $x = 1;` is a`` |
|         - | 4250 | `` 			 * parse error, never a silent discard. `static`/`fn`/`function` `` |
|         - | 4251 | ``			 * cover bare closure-expression statements; `readonly`/`enum` are`` |
|         - | 4252 | `			 * context-sensitive IDs handled by the modified-class/enum scans. */` |
|       389 | 4253 | `			int bAttrTarget = 0;` |
|       384 | 4254 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd)` |
|       384 | 4255 | `			 \|\| GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|        17 | 4256 | `				bAttrTarget = 1;` |
|       381 | 4257 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       373 | 4258 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       368 | 4259 | `				if( nKw == PH7_TKWRD_FUNCTION \|\| nKw == PH7_TKWRD_CLASS` |
|        94 | 4260 | `				 \|\| nKw == PH7_TKWRD_INTERFACE \|\| nKw == PH7_TKWRD_TRAIT` |
|        11 | 4261 | `				 \|\| nKw == PH7_TKWRD_ABSTRACT \|\| nKw == PH7_TKWRD_FINAL` |
|         8 | 4262 | `				 \|\| nKw == PH7_TKWRD_CONST \|\| nKw == PH7_TKWRD_STATIC` |
|         5 | 4263 | `				 \|\| nKw == PH7_TKWRD_FN ){` |
|       373 | 4264 | `					bAttrTarget = 1;` |
|       184 | 4265 | `				}` |
|       184 | 4266 | `			}` |
|       389 | 4267 | `			if( !bAttrTarget ){` |
|       ! 0 | 4268 | `				rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|         - | 4269 | `					"syntax error, unexpected token \"%z\" after attribute group; expecting a declaration",` |
|       ! 0 | 4270 | `					&pGen->pIn->sData);` |
|       ! 0 | 4271 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 4272 | `					break;` |
|         - | 4273 | `				}` |
|       ! 0 | 4274 | `				SySetReset(&pGen->aPendingAttrs);` |
|       ! 0 | 4275 | `			}` |
|       192 | 4276 | `		}` |
|         - | 4277 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|         - | 4278 | `		 * below doesn't fire before the directive has a chance to run. */` |
|   2056417 | 4279 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   1217415 | 4280 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   1217415 | 4281 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|        63 | 4282 | `				bStmtIsDeclare = 1;` |
|        29 | 4283 | `			}` |
|    607866 | 4284 | `		}` |
|   2056417 | 4285 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|         - | 4286 | `			/* Any non-declare top-level statement locks the strict_types` |
|         - | 4287 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|    260495 | 4288 | `			pGen->bStrictTypesLocked = 1;` |
|    129895 | 4289 | `		}` |
|   2056417 | 4290 | `		if( GenStateIsHaltCompiler(pGen->pIn,pGen->pEnd) ){` |
|         - | 4291 | `			/* Everything from here on is DATA. php's own scanner stops in exactly` |
|         - | 4292 | `			 * the same place, which is what lets a .phar carry its archive in the` |
|         - | 4293 | `			 * bytes after its stub. */` |
|        95 | 4294 | `			rc = GenStateCompileHaltCompiler(&(*pGen));` |
|        95 | 4295 | `			break;` |
|         - | 4296 | `		}` |
|   2056323 | 4297 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 4298 | `			/* Compile block */` |
|        89 | 4299 | `			bStmtWantsSemi = 0;` |
|        89 | 4300 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|        89 | 4301 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 4302 | `				break;` |
|         - | 4303 | `			}` |
|        47 | 4304 | `		}else{` |
|   2056239 | 4305 | `			xCons = 0;` |
|   2056239 | 4306 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|         - | 4307 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|         - | 4308 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|         - | 4309 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|       213 | 4310 | `				xCons = PH7_CompileClassModifiers;` |
|   2056135 | 4311 | `			}else if( GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|         - | 4312 | ``				/* `enum Name …` (PHP 8.1) — `enum` is a context-sensitive ID,`` |
|         - | 4313 | `				 * so it is detected here rather than the keyword dispatcher. */` |
|       141 | 4314 | `				xCons = PH7_CompileEnum;` |
|   2055963 | 4315 | `			}else if( GenStateStartsClosureExpr(pGen->pIn,pGen->pEnd) ){` |
|         - | 4316 | ``				/* `function () {…};` / `fn (…) => …;` at STATEMENT position is an`` |
|         - | 4317 | ``				 * expression statement in php, not a declaration — the `(` where a`` |
|         - | 4318 | `				 * named function has its name is what says so. */` |
|        16 | 4319 | `				xCons = 0;` |
|   2055888 | 4320 | `			}else if( GenStateIsNsRelName(pGen->pIn,pGen->pEnd) ){` |
|         - | 4321 | ``				/* A statement that STARTS with php's `namespace\X` name operator`` |
|         - | 4322 | ``				 * (`namespace\Cee::m();`) is an expression, not a namespace`` |
|         - | 4323 | ``				 * DECLARATION — the glued `\` is what tells the two apart. */`` |
|         7 | 4324 | `				xCons = 0;` |
|   2055878 | 4325 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   1217221 | 4326 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         - | 4327 | `				/* Try to extract a language construct handler */` |
|   1217221 | 4328 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|   1217221 | 4329 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|        13 | 4330 | `					rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|         - | 4331 | `						"Syntax error: Unexpected keyword '%z'",` |
|         8 | 4332 | `						&pGen->pIn->sData);` |
|         9 | 4333 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 4334 | `						break;` |
|         - | 4335 | `					}` |
|         - | 4336 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|         - | 4337 | `					 * this erroneous statement.` |
|         - | 4338 | `					 */` |
|         9 | 4339 | `					xCons = PH7_ErrorRecover;` |
|         4 | 4340 | `				}` |
|   1446428 | 4341 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|    129205 | 4342 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|         - | 4343 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|       225 | 4344 | `				xCons = PH7_CompileLabel;` |
|       110 | 4345 | `			}` |
|   2056239 | 4346 | `			if( xCons == 0 ){` |
|         - | 4347 | `				/* Assume an expression an try to compile it. A leading php 8.5` |
|         - | 4348 | ``				 * `(void)` cast is consumed here — statement head is one of the two`` |
|         - | 4349 | `				 * places its grammar takes one — and says the answer is dropped` |
|         - | 4350 | `				 * DELIBERATELY, so the call below is not marked. */` |
|    839493 | 4351 | `				int bVoid = GenStateTakeVoidCast(&(*pGen));` |
|    839493 | 4352 | `				if( bVoid && pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|         - | 4353 | `` 					/* php's grammar wants an expression after the cast: `(void);` `` |
|         - | 4354 | ``					 * is `syntax error, unexpected token ";"` there. */`` |
|         3 | 4355 | `					rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|         - | 4356 | `						"syntax error, unexpected token \";\"");` |
|         2 | 4357 | `				}else{` |
|    839491 | 4358 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    839491 | 4359 | `					if( rc != SXERR_EMPTY ){` |
|    839255 | 4360 | `						if( !bVoid ){` |
|    839241 | 4361 | `							GenStateMarkDiscardedCall(&(*pGen));` |
|    418844 | 4362 | `						}` |
|         - | 4363 | `						/* Pop l-value */` |
|    839255 | 4364 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    418851 | 4365 | `					}` |
|         - | 4366 | `				}` |
|    418975 | 4367 | `			}else{` |
|         - | 4368 | `				/* Go compile the sucker */` |
|   1216751 | 4369 | `				rc = xCons(&(*pGen));` |
|   1216746 | 4370 | `				if( xCons == PH7_CompileLabel` |
|   1824063 | 4371 | `				 \|\| ( pGen->pTokenSet` |
|   1216526 | 4372 | `				   && pGen->pIn > (SyToken *)SySetBasePtr(pGen->pTokenSet)` |
|   1216525 | 4373 | `				   && (pGen->pIn[-1].nType & PH7_TK_CCB/*'}'*/) ) ){` |
|         - | 4374 | `					/* A label, and any construct that ends with its own block, close` |
|         - | 4375 | `					 * themselves. (An EXPRESSION statement never does, which is why` |
|         - | 4376 | ``					 * this asks the construct and not just the last token: the `}` of`` |
|         - | 4377 | ``					 * `$f = function () {}` is not a terminator.) */`` |
|    748860 | 4378 | `					bStmtWantsSemi = 0;` |
|    373918 | 4379 | `				}` |
|         - | 4380 | `			}` |
|   2056239 | 4381 | `			if( rc == SXERR_ABORT ){` |
|         - | 4382 | `				/* Request to abort compilation */` |
|        91 | 4383 | `				break;` |
|         - | 4384 | `			}` |
|         - | 4385 | `		}` |
|         - | 4386 | `		/* Ignore trailing semi-colons ';' */` |
|         - | 4387 | `		{` |
|         - | 4388 | ``			/* Terminated when a `;` is sitting there for the loop to step over, or`` |
|         - | 4389 | `			 * when the construct consumed its own (the alternative-syntax bodies` |
|         - | 4390 | ``			 * take the `;` after their `endif`/`endwhile`/… themselves). */`` |
|   2056237 | 4391 | `			int bTerminated = (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI)) != 0;` |
|   2056232 | 4392 | `			if( !bTerminated && pGen->pTokenSet` |
|    749595 | 4393 | `			 && pGen->pIn > (SyToken *)SySetBasePtr(pGen->pTokenSet)` |
|    749600 | 4394 | `			 && (pGen->pIn[-1].nType & PH7_TK_SEMI) ){` |
|       601 | 4395 | `				bTerminated = 1;` |
|       298 | 4396 | `			}` |
|   3362874 | 4397 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|   1306642 | 4398 | `				pGen->pIn++;` |
|         5 | 4399 | `			}` |
|   2056232 | 4400 | `			if( !bTerminated && bStmtWantsSemi && pGen->nErr < 1 && GenStateAtChunkEof(&(*pGen))` |
|        90 | 4401 | `			 && pGen->pIn > (SyToken *)SySetBasePtr(pGen->pTokenSet) ){` |
|         - | 4402 | `				/* (GenStateAtChunkEof already answered no for a NULL token set.) */` |
|         - | 4403 | `				/* Ran out of input with the statement still open. */` |
|        51 | 4404 | `				rc = PH7_GenSyntaxError(&(*pGen),0,GenStateEofExpecting(pStmtStart,&pGen->pIn[-1]));` |
|        51 | 4405 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 4406 | `					break;` |
|         - | 4407 | `				}` |
|        25 | 4408 | `			}` |
|         - | 4409 | `		}` |
|   2056237 | 4410 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|         - | 4411 | `			/* Compile a single statement and return */` |
|   1795799 | 4412 | `			break;` |
|         - | 4413 | `		}` |
|         - | 4414 | `		/* LOOP ONE */` |
|         - | 4415 | `		/* LOOP TWO */` |
|         - | 4416 | `		/* LOOP THREE */` |
|         - | 4417 | `		/* LOOP FOUR */` |
|         5 | 4418 | `	}` |
|         - | 4419 | `	/* Return compilation status */` |
|   1826717 | 4420 | `	return rc;` |
|         5 | 4421 | `}` |
|         - | 4422 | `/*` |
|         - | 4423 | `` * TRUE when the SOURCE bytes of a double-quoted string interpolate -- `$name`,`` |
|         - | 4424 | `` * `${`, or `{$` -- which is what decides whether php's scanner produced ONE`` |
|         - | 4425 | ` * string token for it or an opening quote followed by parts. A backslash escapes` |
|         - | 4426 | `` * whatever follows it, so `"\\$b"` does not interpolate.`` |
|         - | 4427 | ` */` |
|        24 | 4428 | `static int GenStateDqInterpolates(SyString *pStr)` |
|         1 | 4429 | `{` |
|        25 | 4430 | `	const unsigned char *z = (const unsigned char *)pStr->zString;` |
|        25 | 4431 | `	const unsigned char *zEnd = &z[pStr->nByte];` |
|        55 | 4432 | `	while( z < zEnd ){` |
|        41 | 4433 | `		if( z[0] == '\\' ){` |
|         5 | 4434 | `			z += 2;` |
|         5 | 4435 | `			continue;` |
|         - | 4436 | `		}` |
|        36 | 4437 | `		if( z[0] == '$' && &z[1] < zEnd` |
|         8 | 4438 | `		 && (z[1] == '{' \|\| z[1] >= 0x80 \|\| SyisAlpha(z[1]) \|\| z[1] == '_') ){` |
|         7 | 4439 | `			return 1;` |
|         - | 4440 | `		}` |
|        31 | 4441 | `		if( z[0] == '{' && &z[1] < zEnd && z[1] == '$' ){` |
|         5 | 4442 | `			return 1;` |
|         - | 4443 | `		}` |
|        27 | 4444 | `		z++;` |
|         1 | 4445 | `	}` |
|        15 | 4446 | `	return 0;` |
|        13 | 4447 | `}` |
|         - | 4448 | `/*` |
|         - | 4449 | ` * Compile a Raw PHP chunk.` |
|         - | 4450 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 4451 | ` * takes care of generating the appropriate error message.` |
|         - | 4452 | ` */` |
|     30884 | 4453 | `static sxi32 PH7_CompilePHP(` |
|         - | 4454 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 4455 | `	SySet *pTokenSet,     /* Token set */` |
|         - | 4456 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|         - | 4457 | `	)` |
|         5 | 4458 | `{` |
|     30889 | 4459 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|         - | 4460 | `	sxi32 rc;` |
|         - | 4461 | `	/* Reset the token set (and its trivia sidecar) */` |
|     30889 | 4462 | `	SySetReset(&(*pTokenSet));` |
|     30889 | 4463 | `	SySetReset(&pGen->aTrivia);` |
|         - | 4464 | `	/* Mark as the default token set */` |
|     30889 | 4465 | `	pGen->pTokenSet = &(*pTokenSet);` |
|         - | 4466 | `	/* Advance the stream cursor */` |
|     30889 | 4467 | `	pGen->pRawIn++;` |
|         - | 4468 | `	/* Tokenize the PHP chunk first */` |
|     30889 | 4469 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet),&pGen->aTrivia);` |
|         - | 4470 | ``	/* The raw tokenizer marked whether this chunk was closed by a `?>`; only one`` |
|         - | 4471 | `	 * that met the end of the FILE can leave a statement unterminated. */` |
|     30889 | 4472 | `	pGen->bChunkAtEof = (sxi8)(SX_PTR_TO_INT(pScript->pUserData) == 0);` |
|         - | 4473 | `	/* Point to the head and tail of the token stream. */` |
|     30889 | 4474 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     30889 | 4475 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|         - | 4476 | ``	/* php REMOVED `(real)` in its SCANNER, so the refusal belongs to the chunk and`` |
|         - | 4477 | ``	 * not to the expression the cast sits in: `strlen(real)` -- where the token is`` |
|         - | 4478 | `	 * never an operator at all -- reports the same sentence, and reports it as a` |
|         - | 4479 | ``	 * PARSE error rather than the fatal `(unset)` gets from the compiler. */`` |
|         - | 4480 | `	{` |
|         - | 4481 | `		SyToken *pTok;` |
|     30889 | 4482 | `		sxi32 nBraceOpen = 0;` |
|  22515222 | 4483 | `		for( pTok = pGen->pIn ; pTok < pGen->pEnd ; pTok++ ){` |
|  22484338 | 4484 | `			if( pTok->nType & PH7_TK_OCB ){` |
|    867016 | 4485 | `				nBraceOpen++;` |
|  22050235 | 4486 | `			}else if( pTok->nType & PH7_TK_CCB ){` |
|    866998 | 4487 | `				nBraceOpen--;` |
|    432899 | 4488 | `			}` |
|  11224081 | 4489 | `		}` |
|     30889 | 4490 | `		if( nBraceOpen > 0 ){` |
|         - | 4491 | ``			/* A `{` this chunk never closes: php's parser reports THAT at the end of`` |
|         - | 4492 | `			 * the file, ahead of any statement it left open and ahead of a string or` |
|         - | 4493 | `			 * heredoc the scanner was still inside. Stand the end-of-input questions` |
|         - | 4494 | ``			 * down and let the block compiler say its own `Unclosed '{'`. */`` |
|        18 | 4495 | `			pGen->bChunkAtEof = 0;` |
|         7 | 4496 | `		}` |
|  22515186 | 4497 | `		for( pTok = pGen->pIn ; pTok < pGen->pEnd ; pTok++ ){` |
|  22484325 | 4498 | `			if( (pTok->nType & PH7_TK_OP) && pTok->sData.nByte == sizeof("(real)")-1` |
|   1731471 | 4499 | `			 && SyMemcmp((const void *)pTok->sData.zString,(const void *)"(real)",sizeof("(real)")-1) == 0 ){` |
|         5 | 4500 | `				return PH7_GenCompileError(pGen,E_PARSE,pTok->nLine,` |
|         - | 4501 | `					"The (real) cast has been removed, use (float) instead");` |
|         - | 4502 | `			}` |
|  22484321 | 4503 | `			if( (pTok->nType & PH7_TK_UNTERM)` |
|  11224083 | 4504 | `			 && (pTok->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC))` |
|        27 | 4505 | `			 && pGen->bChunkAtEof == 0 ){` |
|         - | 4506 | `				/* php's parser reaches the end of file with the string still open and` |
|         - | 4507 | `				 * reports the UNCLOSED BRACE first -- the scanner is mid-interpolation` |
|         - | 4508 | `				 * there and has nothing of its own to say. (A single-quoted string and` |
|         - | 4509 | `				 * a block comment do: their sentences win over the brace, which is why` |
|         - | 4510 | `				 * only these three yield.) Leave it to the compile below. */` |
|         3 | 4511 | `				continue;` |
|         - | 4512 | `			}` |
|  22484324 | 4513 | `			if( pTok->nType & PH7_TK_UNTERM ){` |
|         - | 4514 | `				/* A quote, heredoc or block comment the input ran out under. php` |
|         - | 4515 | `				 * refuses the file for each; this used to take the rest of it as the` |
|         - | 4516 | ``				 * lexeme's body and RUN the program (`<?php echo 'a` printed `a`).`` |
|         - | 4517 | `				 * The wording is php's own per shape -- its scanner reports what it` |
|         - | 4518 | `				 * was still waiting for. */` |
|        24 | 4519 | `				if( pTok->nType & PH7_TK_SSTR ){` |
|         - | 4520 | `					/* php's single-quoted scanner hands the parser the CONTENT it` |
|         - | 4521 | `					 * had read, and the parser names that. */` |
|         6 | 4522 | `					return PH7_GenCompileError(pGen,E_PARSE,pTok->nLine,` |
|         2 | 4523 | `						"syntax error, unexpected string content \"%z\"",&pTok->sData);` |
|         - | 4524 | `				}` |
|        20 | 4525 | `				if( pTok->nType & PH7_TK_DSTR ){` |
|        12 | 4526 | `					const char *zExp = pTok->sData.nByte < 1` |
|         - | 4527 | `						? "variable or string content or \"${\" or \"{$\""` |
|         7 | 4528 | `						: (GenStateDqInterpolates(&pTok->sData) ? 0` |
|         - | 4529 | `						                                       : "variable or \"${\" or \"{$\"");` |
|         4 | 4530 | `					return zExp` |
|         6 | 4531 | `						? PH7_GenCompileError(pGen,E_PARSE,pTok->nLine,` |
|         2 | 4532 | `							"syntax error, unexpected end of file, expecting %s",zExp)` |
|         8 | 4533 | `						: PH7_GenCompileError(pGen,E_PARSE,pTok->nLine,` |
|         - | 4534 | `							"syntax error, unexpected end of file");` |
|         - | 4535 | `				}` |
|        12 | 4536 | `				if( pTok->nType & (PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){` |
|         - | 4537 | `					/* An EMPTY body, or one that interpolates, leaves php's parser` |
|         - | 4538 | `					 * with nothing to expect but the end it just met. */` |
|        15 | 4539 | `					int bSet = pTok->sData.nByte > 0` |
|         8 | 4540 | `						&& !((pTok->nType & PH7_TK_HEREDOC) && GenStateDqInterpolates(&pTok->sData));` |
|         4 | 4541 | `					return bSet` |
|         4 | 4542 | `						? PH7_GenCompileError(pGen,E_PARSE,pTok->nLine,` |
|         - | 4543 | `							"syntax error, unexpected end of file, "` |
|         - | 4544 | `							"expecting variable or heredoc end or \"${\" or \"{$\"")` |
|         8 | 4545 | `						: PH7_GenCompileError(pGen,E_PARSE,pTok->nLine,` |
|         - | 4546 | `							"syntax error, unexpected end of file");` |
|         - | 4547 | `				}` |
|         - | 4548 | `				/* A block comment, whose sentence names where it began. */` |
|         6 | 4549 | `				return PH7_GenCompileError(pGen,E_PARSE,pTok->nLine,` |
|         2 | 4550 | `					"Unterminated comment starting line %u",pTok->nLine);` |
|         - | 4551 | `			}` |
|  11224062 | 4552 | `		}` |
|         - | 4553 | `	}` |
|     30861 | 4554 | `	if( is_expr ){` |
|       ! 0 | 4555 | `		rc = SXERR_EMPTY;` |
|       ! 0 | 4556 | `		if( pGen->pIn < pGen->pEnd ){` |
|         - | 4557 | `			/* A simple expression,compile it */` |
|       ! 0 | 4558 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|       ! 0 | 4559 | `		}` |
|         - | 4560 | `		/* Emit the DONE instruction */` |
|       ! 0 | 4561 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       ! 0 | 4562 | `		return SXRET_OK;` |
|         - | 4563 | `	}` |
|     30861 | 4564 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|         - | 4565 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|         - | 4566 | `		/*` |
|         - | 4567 | `		 * Shortcut syntax for the 'echo' language construct.` |
|         - | 4568 | `		 * According to the PHP reference manual:` |
|         - | 4569 | `		 *  echo() also has a shortcut syntax, where you can` |
|         - | 4570 | `		 *  immediately follow` |
|         - | 4571 | `		 *  the opening tag with an equals sign as follows:` |
|         - | 4572 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|         - | 4573 | `		 * Symisc extension:` |
|         - | 4574 | `		 *   This short syntax works with all PHP opening` |
|         - | 4575 | `		 *   tags unlike the default PHP engine that handle` |
|         - | 4576 | `		 *   only short tag.` |
|         - | 4577 | `		 */` |
|         - | 4578 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|         3 | 4579 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|         3 | 4580 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|         3 | 4581 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|         - | 4582 | `		/* This synthesized echo is compiled as an EXPRESSION, which is otherwise a` |
|         - | 4583 | `		 * parse error; allow it for the duration of this one compile. */` |
|         3 | 4584 | `		pGen->nExprEchoOk++;` |
|         3 | 4585 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|         3 | 4586 | `		pGen->nExprEchoOk--;` |
|         3 | 4587 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 4588 | `			return SXERR_ABORT;` |
|         - | 4589 | `		}` |
|         3 | 4590 | `		if( rc != SXERR_EMPTY ){` |
|         3 | 4591 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         1 | 4592 | `		}` |
|         3 | 4593 | `		return SXRET_OK;` |
|         - | 4594 | `	}` |
|         - | 4595 | `	/* Compile the PHP chunk */` |
|     30859 | 4596 | `	rc = GenStateCompileChunk(pGen,0);` |
|         - | 4597 | `	/* Fix exceptions jumps */` |
|     30859 | 4598 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - | 4599 | `	/* Fix gotos now, the jump destination is resolved */` |
|     30859 | 4600 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|         3 | 4601 | `		rc = SXERR_ABORT;` |
|         1 | 4602 | `	}` |
|         - | 4603 | `	/* Reset container */` |
|     30859 | 4604 | `	SySetReset(&pGen->aGoto);` |
|     30859 | 4605 | `	SySetReset(&pGen->aLabel);` |
|     30859 | 4606 | `	SySetReset(&pGen->aNullsafeJmp);` |
|         - | 4607 | `	/* Compilation result */` |
|     30859 | 4608 | `	return rc;` |
|     15438 | 4609 | `}` |
|         - | 4610 | `/*` |
|         - | 4611 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|         - | 4612 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|         - | 4613 | ` * This is the only compile interface exported from this file.` |
|         - | 4614 | ` */` |
|     34882 | 4615 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|         - | 4616 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|         - | 4617 | `	SyString *pScript,  /* Script to compile */` |
|         - | 4618 | `	sxi32 iFlags        /* Compile flags */` |
|         - | 4619 | `	)` |
|         5 | 4620 | `{` |
|         - | 4621 | `	SySet aPhpToken,aRawToken;` |
|         - | 4622 | `	ph7_gen_state *pCodeGen;` |
|         - | 4623 | `	ph7_value *pRawObj;` |
|         - | 4624 | `	sxu32 nObjIdx;` |
|         - | 4625 | `	sxi32 nRawObj;` |
|         - | 4626 | `	int is_expr;` |
|         - | 4627 | `	sxi8 bSavedStrict;` |
|         - | 4628 | `	sxi8 bSavedStrictLocked;` |
|         - | 4629 | `	sxi8 bSavedHalted,bSavedHaltSeen;` |
|         - | 4630 | `	sxu32 nSavedHaltOffset;` |
|         - | 4631 | `	const char *zSavedScriptBase;` |
|         - | 4632 | `	const char *zFileBase;` |
|         - | 4633 | `	SyToken *pSavedIn,*pSavedEnd;` |
|         - | 4634 | `	sxi32 rc;` |
|     34887 | 4635 | `	sxu32 nBaseLine = 1;` |
|     34887 | 4636 | `	if( pScript->nByte < 1 ){` |
|         - | 4637 | `		/* Nothing to compile */` |
|        11 | 4638 | `		return PH7_OK;` |
|         - | 4639 | `	}` |
|         - | 4640 | `	/* Kept before the shebang skip below: php counts __COMPILER_HALT_OFFSET__` |
|         - | 4641 | `	 * from the first byte on DISK, shebang line included. */` |
|     34877 | 4642 | `	zFileBase = pScript->zString;` |
|         - | 4643 | `	/* php skips a "#!" shebang on the first line of a CLI script: consume it` |
|         - | 4644 | `	 * (including its newline) so it is not echoed as inline text, and bump the` |
|         - | 4645 | `	 * base line to 2 so the code below still reports php-matching line numbers. */` |
|     34877 | 4646 | `	if( pScript->nByte >= 2 && pScript->zString[0]=='#' && pScript->zString[1]=='!' ){` |
|         6 | 4647 | `		const char *z = pScript->zString;` |
|         6 | 4648 | `		const char *zEnd = &z[pScript->nByte];` |
|        78 | 4649 | `		while( z < zEnd && z[0] != '\n' ){ z++; }` |
|         6 | 4650 | `		if( z < zEnd ){ z++; } /* consume the newline too */` |
|         6 | 4651 | `		pScript->nByte -= (sxu32)(z - pScript->zString);` |
|         6 | 4652 | `		pScript->zString = z;` |
|         6 | 4653 | `		nBaseLine = 2;` |
|         6 | 4654 | `		if( pScript->nByte < 1 ){` |
|       ! 0 | 4655 | `			return PH7_OK;` |
|         - | 4656 | `		}` |
|         2 | 4657 | `	}` |
|         - | 4658 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|         - | 4659 | `	 * file's flags so include/require restore them on return. */` |
|     34877 | 4660 | `	pCodeGen = &pVm->sCodeGen;` |
|         - | 4661 | ``	/* aPhpToken below is a LOCAL token set: once it is released at `cleanup`, any`` |
|         - | 4662 | `	 * pGen->pIn/pEnd still pointing into it DANGLE. PH7_VmEmitInstr reads pIn to stamp` |
|         - | 4663 | `	 * each instruction's source line, and instructions are still emitted after this` |
|         - | 4664 | `	 * function returns (VmEvalChunk's trailing OP_DONE) -- a use-after-free ASan caught` |
|         - | 4665 | `	 * immediately. Save the caller's cursor and restore it on the way out, so an` |
|         - | 4666 | `	 * enclosing compile keeps its (live) tokens and the top level goes back to NULL. */` |
|     34877 | 4667 | `	pSavedIn = pCodeGen->pIn;` |
|     34877 | 4668 | `	pSavedEnd = pCodeGen->pEnd;` |
|     34877 | 4669 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|     34877 | 4670 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|     34877 | 4671 | `	pCodeGen->bStrictTypes = 0;` |
|     34877 | 4672 | `	pCodeGen->bStrictTypesLocked = 0;` |
|         - | 4673 | `	/* The halt is per-FILE too, and an include compiles inside its includer. */` |
|     34877 | 4674 | `	bSavedHalted = pCodeGen->bHalted;` |
|     34877 | 4675 | `	bSavedHaltSeen = pCodeGen->bHaltSeen;` |
|     34877 | 4676 | `	nSavedHaltOffset = pCodeGen->nHaltOffset;` |
|     34877 | 4677 | `	zSavedScriptBase = pCodeGen->zScriptBase;` |
|     34877 | 4678 | `	pCodeGen->bHalted = 0;` |
|     34877 | 4679 | `	pCodeGen->bHaltSeen = 0;` |
|     34877 | 4680 | `	pCodeGen->nHaltOffset = 0;` |
|     34877 | 4681 | `	pCodeGen->zScriptBase = zFileBase;` |
|         - | 4682 | `	/* Initialize the tokens containers */` |
|     34877 | 4683 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|     34877 | 4684 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|     34877 | 4685 | `	SySetAlloc(&aPhpToken,0xc0);` |
|     34877 | 4686 | `	is_expr = 0;` |
|     34877 | 4687 | `	if( iFlags & PH7_PHP_ONLY ){` |
|         - | 4688 | `		SyToken sTmp;` |
|         - | 4689 | `		/* PHP only: -*/` |
|      6896 | 4690 | `		sTmp.nLine = 1;` |
|      6896 | 4691 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|      6896 | 4692 | `		sTmp.pUserData = 0;` |
|      6896 | 4693 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|      6896 | 4694 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|      6896 | 4695 | `		if( iFlags & PH7_PHP_EXPR ){` |
|         - | 4696 | `			/* A simple PHP expression */` |
|       ! 0 | 4697 | `			is_expr = 1;` |
|       ! 0 | 4698 | `		}` |
|      3446 | 4699 | `	}else{` |
|         - | 4700 | `		/* Tokenize raw text */` |
|     27986 | 4701 | `		SySetAlloc(&aRawToken,32);` |
|     27986 | 4702 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken,nBaseLine);` |
|         - | 4703 | `	}` |
|         - | 4704 | ``	/* Where the end of INPUT sits. php reports `unexpected end of file` at the line`` |
|         - | 4705 | `	 * the file ENDS on, which is past the last token whenever anything follows it --` |
|         - | 4706 | `	 * a trailing newline always does -- and the chunk a statement was cut off in` |
|         - | 4707 | `	 * does not carry that: the raw splitter hands it the source up to its last byte` |
|         - | 4708 | `	 * of code, newline excluded. Counted here, where the whole script is still in` |
|         - | 4709 | `	 * hand, and read back by PH7_GenSyntaxError's end-of-file branch. */` |
|         - | 4710 | `	{` |
|         - | 4711 | `		sxu32 i;` |
|     34877 | 4712 | `		pCodeGen->nChunkEofLine = nBaseLine;` |
| 123312394 | 4713 | `		for( i = 0 ; i < pScript->nByte ; ++i ){` |
| 123277522 | 4714 | `			if( pScript->zString[i] == '\n' ){` |
|    214286 | 4715 | `				pCodeGen->nChunkEofLine++;` |
|    106737 | 4716 | `			}` |
|  61539603 | 4717 | `		}` |
|         - | 4718 | `	}` |
|         - | 4719 | `	/* Process high-level tokens */` |
|     34877 | 4720 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|     34877 | 4721 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|         - | 4722 | `	/*` |
|         - | 4723 | ``	 * Where `__halt_compiler();` sits, decided BEFORE anything compiles: the`` |
|         - | 4724 | `	 * constant it defines may be read ahead of the statement that sets it (and` |
|         - | 4725 | `	 * in an earlier chunk than the one holding it), exactly as it may under php,` |
|         - | 4726 | `	 * whose compiler registers the constant for the whole file. The scan costs a` |
|         - | 4727 | `	 * second tokenization of every PHP chunk, so it only runs when the file` |
|         - | 4728 | `	 * contains the identifier at all -- which no ordinary program does.` |
|         - | 4729 | `	 */` |
|     34877 | 4730 | `	if( GenStateMentionsHalt(pScript->zString,pScript->nByte) ){` |
|        43 | 4731 | `		GenStateScanHaltOffset(pCodeGen,&aRawToken,zFileBase);` |
|        20 | 4732 | `	}` |
|     34877 | 4733 | `	rc = PH7_OK;` |
|     34877 | 4734 | `	if( is_expr ){` |
|         - | 4735 | `		/* Compile the expression */` |
|       ! 0 | 4736 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|       ! 0 | 4737 | `		goto cleanup;` |
|         - | 4738 | `	}` |
|     34877 | 4739 | `	nObjIdx = 0;` |
|         - | 4740 | `	/* Start the compilation process */` |
|     31423 | 4741 | `	for(;;){` |
|     93648 | 4742 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|     34763 | 4743 | `			break; /* No more tokens to process */` |
|         - | 4744 | `		}` |
|     58890 | 4745 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|         - | 4746 | `			/* Compile the PHP chunk */` |
|     30889 | 4747 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|     30889 | 4748 | `			if( rc == SXERR_ABORT ){` |
|        95 | 4749 | `				break;` |
|         - | 4750 | `			}` |
|     30799 | 4751 | `			if( pCodeGen->bHalted ){` |
|         - | 4752 | ``				/* `__halt_compiler();` -- the rest of the FILE is data, inline`` |
|         - | 4753 | `				 * text between later chunks included. */` |
|        25 | 4754 | `				break;` |
|         - | 4755 | `			}` |
|     30775 | 4756 | `			continue;` |
|         - | 4757 | `		}` |
|         - | 4758 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|     28006 | 4759 | `		nRawObj = 0;` |
|     56023 | 4760 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|         - | 4761 | `			/* Consume the raw chunk without any processing */` |
|     28022 | 4762 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|     28022 | 4763 | `			if( pRawObj == 0 ){` |
|       ! 0 | 4764 | `				rc = SXERR_MEM;` |
|       ! 0 | 4765 | `				break;` |
|         - | 4766 | `			}` |
|         - | 4767 | `			/* Mark as constant and emit the load constant instruction */` |
|     28022 | 4768 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|     28022 | 4769 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     28022 | 4770 | `			++nRawObj;` |
|     28022 | 4771 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|         5 | 4772 | `		}` |
|     28006 | 4773 | `		if( nRawObj > 0 ){` |
|         - | 4774 | `			/* Emit the consume instruction */` |
|     28006 | 4775 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     13996 | 4776 | `		}` |
|     17432 | 4777 | `	}` |
|     17445 | 4778 | `cleanup:` |
|         - | 4779 | `	/* Drop the cursor into the token set BEFORE the set is freed (see above). */` |
|     34877 | 4780 | `	pCodeGen->pIn = pSavedIn;` |
|     34877 | 4781 | `	pCodeGen->pEnd = pSavedEnd;` |
|     34877 | 4782 | `	SySetRelease(&aRawToken);` |
|     34877 | 4783 | `	SySetRelease(&aPhpToken);` |
|         - | 4784 | `	/* Restore outer file's strict_types scope */` |
|     34877 | 4785 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|     34877 | 4786 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|         - | 4787 | `	/* ...and its halt state. */` |
|     34877 | 4788 | `	pCodeGen->bHalted = bSavedHalted;` |
|     34877 | 4789 | `	pCodeGen->bHaltSeen = bSavedHaltSeen;` |
|     34877 | 4790 | `	pCodeGen->nHaltOffset = nSavedHaltOffset;` |
|     34877 | 4791 | `	pCodeGen->zScriptBase = zSavedScriptBase;` |
|     34877 | 4792 | `	return rc;` |
|     17437 | 4793 | `}` |
|         - | 4794 | `/*` |
|         - | 4795 | ` * Utility routines.Initialize the code generator.` |
|         - | 4796 | ` */` |
|      6721 | 4797 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|         - | 4798 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 4799 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 4800 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 4801 | `	)` |
|         5 | 4802 | `{` |
|      6726 | 4803 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 4804 | `	/* Zero the structure */` |
|      6726 | 4805 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|         - | 4806 | `	/* Initial state */` |
|      6726 | 4807 | `	pGen->pVm  = &(*pVm);` |
|      6726 | 4808 | `	pGen->xErr = xErr;` |
|      6726 | 4809 | `	pGen->pErrData = pErrData;` |
|      6726 | 4810 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|      6726 | 4811 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|      6726 | 4812 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|      6726 | 4813 | `	SySetInit(&pGen->aScope,&pVm->sAllocator,sizeof(GenScope));` |
|      6726 | 4814 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|      6726 | 4815 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|      6726 | 4816 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      6726 | 4817 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      6726 | 4818 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|      6726 | 4819 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|         - | 4820 | `	/* Error log buffer */` |
|      6726 | 4821 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|      6726 | 4822 | `	SyBlobInit(&pGen->sFirstErr,&pVm->sAllocator);` |
|         - | 4823 | `	/* General purpose working buffer */` |
|      6726 | 4824 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|         - | 4825 | `	/* Namespace state */` |
|      6726 | 4826 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|      6726 | 4827 | `	GenStateInitUseImports(&(*pGen),&(*pVm));` |
|      6726 | 4828 | `	GenStateInitSeenSymbols(&(*pGen),&(*pVm));` |
|         - | 4829 | `	/* Create the global scope */` |
|      6726 | 4830 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|         - | 4831 | `	/* Point to the global scope */` |
|      6726 | 4832 | `	pGen->pCurrent = &pGen->sGlobal;` |
|      6726 | 4833 | `	return SXRET_OK;` |
|         5 | 4834 | `}` |
|         - | 4835 | `/*` |
|         - | 4836 | ` * Utility routines. Reset the code generator to it's initial state.` |
|         - | 4837 | ` */` |
|     40497 | 4838 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|         - | 4839 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 4840 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 4841 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 4842 | `	)` |
|         5 | 4843 | `{` |
|     40502 | 4844 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 4845 | `	GenBlock *pBlock,*pParent;` |
|         - | 4846 | `	/* Reset state */` |
|     40502 | 4847 | `	SySetReset(&pGen->aLabel);` |
|     40502 | 4848 | `	SySetReset(&pGen->aGoto);` |
|     40502 | 4849 | `	SySetReset(&pGen->aNullsafeJmp);` |
|     40502 | 4850 | `	SySetReset(&pGen->aTrivia);` |
|     40502 | 4851 | `	SySetReset(&pGen->aPendingAttrs);` |
|     40502 | 4852 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|     40502 | 4853 | `	SyBlobRelease(&pGen->sErrBuf);` |
|     40502 | 4854 | `	SyBlobRelease(&pGen->sFirstErr);` |
|     40502 | 4855 | `	SyBlobRelease(&pGen->sWorker);` |
|     40502 | 4856 | `	SyBlobRelease(&pGen->sNamespace);` |
|     40502 | 4857 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|     40502 | 4858 | `	GenStateResetUseImports(&(*pGen),&(*pVm));` |
|         - | 4859 | `	/* A fresh compile unit has declared nothing yet. */` |
|     40502 | 4860 | `	GenStateResetSeenSymbols(&(*pGen),&(*pVm));` |
|         - | 4861 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|         - | 4862 | `	 * They intern variable names and literal strings that are referenced by` |
|         - | 4863 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|         - | 4864 | `	 * Releasing them would either leak the interned strings or require freeing` |
|         - | 4865 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|         - | 4866 | `	 * number of unique names, which is acceptable. */` |
|         - | 4867 | `	/* Point to the global scope */` |
|     40502 | 4868 | `	pBlock = pGen->pCurrent;` |
|     40502 | 4869 | `	while( pBlock->pParent != 0 ){` |
|       ! 0 | 4870 | `		pParent = pBlock->pParent;` |
|       ! 0 | 4871 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 4872 | `		pBlock = pParent;` |
|       ! 0 | 4873 | `	}` |
|     40502 | 4874 | `	pGen->xErr = xErr;` |
|     40502 | 4875 | `	pGen->pErrData = pErrData;` |
|     40502 | 4876 | `	pGen->pCurrent = &pGen->sGlobal;` |
|     40502 | 4877 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|     40502 | 4878 | `	pGen->pIn = pGen->pEnd = 0;` |
|     40502 | 4879 | `	pGen->nErr = 0;` |
|     40502 | 4880 | `	pGen->nFatal = 0;` |
|     40502 | 4881 | `	pGen->nFirstErrLine = 0;` |
|     40502 | 4882 | `	pGen->bParseThrows = 0;` |
|     40502 | 4883 | `	pGen->iFatalTrace = PH7_FATAL_TRACE_COMPILE;` |
|         - | 4884 | `	/* Clear the class-body context (a prior compile aborted mid-class-body would` |
|         - | 4885 | `	 * otherwise leave these live for the next eval/include on this VM). */` |
|     40502 | 4886 | `	pGen->pCurClass = 0;` |
|     40502 | 4887 | `	pGen->iInMemberDefault = 0;` |
|     40502 | 4888 | `	return SXRET_OK;` |
|         5 | 4889 | `}` |
|         - | 4890 | `/*` |
|         - | 4891 | ` * Save the code generator's compile-position state and hand the live generator a` |
|         - | 4892 | ` * fresh, empty one for a NESTED compilation unit.` |
|         - | 4893 | ` *` |
|         - | 4894 | ` * A require/include normally runs at execution time, when no compile is in flight,` |
|         - | 4895 | ` * so VmEvalChunk can call PH7_ResetCodeGenerator to wipe the generator. But an` |
|         - | 4896 | `` * autoload can fire in the MIDDLE of compiling a class: resolving `class Child`` |
|         - | 4897 | `` * extends Base` calls PH7_VmExtractClass, which triggers the autoloader, whose`` |
|         - | 4898 | `` * body `require`s Base's file — a nested compile while the outer one is mid-token.`` |
|         - | 4899 | ` * PH7_ResetCodeGenerator would then blow away the outer compile's cursor` |
|         - | 4900 | ` * (pIn/pEnd), current block, use-imports, labels, etc.; the outer parse resumes` |
|         - | 4901 | ` * with pIn == NULL and reports a bogus "Expected '{' after class 'Child'". (Common` |
|         - | 4902 | ` * in every real framework: Composer autoloads parent classes on demand.)` |
|         - | 4903 | ` *` |
|         - | 4904 | ` * This snapshots the position/scope fields into *pSaved (a caller-stack` |
|         - | 4905 | ` * ph7_gen_state used purely as storage) and re-initializes the live generator's` |
|         - | 4906 | ` * position containers to FRESH, empty ones WITHOUT releasing the outer's (the` |
|         - | 4907 | ` * snapshot now owns those). hLiteral/hNumLiteral/hVar are intentionally left LIVE` |
|         - | 4908 | ` * and shared — compiled bytecode interns names/literals into them and they grow` |
|         - | 4909 | ` * monotonically (exactly why PH7_ResetCodeGenerator keeps them); restoring an old` |
|         - | 4910 | ` * copy of those headers after a nested grow would use-after-free the bucket array.` |
|         - | 4911 | ` */` |
|         4 | 4912 | `PH7_PRIVATE void PH7_CompilerSaveState(ph7_vm *pVm,ph7_gen_state *pSaved,ProcConsumer xErr,void *pErrData)` |
|         1 | 4913 | `{` |
|         5 | 4914 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 4915 | `	/* Shallow-copy every field; the position containers below are then replaced` |
|         - | 4916 | `	 * with fresh ones on the live struct, so *pSaved keeps the outer's memory. */` |
|         5 | 4917 | `	*pSaved = *pGen;` |
|         5 | 4918 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|         5 | 4919 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|         5 | 4920 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|         5 | 4921 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|         5 | 4922 | `	SySetInit(&pGen->aScope,&pVm->sAllocator,sizeof(GenScope));` |
|         5 | 4923 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|         5 | 4924 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|         5 | 4925 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|         5 | 4926 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|         5 | 4927 | `	SyBlobInit(&pGen->sFirstErr,&pVm->sAllocator);` |
|         5 | 4928 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|         5 | 4929 | `	GenStateInitUseImports(&(*pGen),&(*pVm));` |
|         5 | 4930 | `	GenStateInitSeenSymbols(&(*pGen),&(*pVm));` |
|         - | 4931 | `	/* Fresh global scope for the nested unit (address of the embedded sGlobal is` |
|         - | 4932 | `	 * stable, so any outer block still parented to it stays valid across restore). */` |
|         5 | 4933 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(pVm),0);` |
|         5 | 4934 | `	pGen->pCurrent = &pGen->sGlobal;` |
|         5 | 4935 | `	pGen->pIn = pGen->pEnd = 0;` |
|         5 | 4936 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|         5 | 4937 | `	pGen->pTokenSet = 0;` |
|         5 | 4938 | `	pGen->nErr = 0;` |
|         5 | 4939 | `	pGen->nFatal = 0;` |
|         5 | 4940 | `	pGen->nFirstErrLine = 0;` |
|         5 | 4941 | `	pGen->bParseThrows = 0;` |
|         5 | 4942 | `	pGen->iFatalTrace = PH7_FATAL_TRACE_COMPILE;` |
|         5 | 4943 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|         5 | 4944 | `	pGen->nCommaExprOk = 0;` |
|         5 | 4945 | `	pGen->zClauseCloser = 0;` |
|         5 | 4946 | `	pGen->bInGenerator = 0;` |
|         5 | 4947 | `	pGen->bStrictTypes = 0;` |
|         5 | 4948 | `	pGen->bStrictTypesLocked = 0;` |
|         - | 4949 | `	/* The nested unit is a fresh top-level compile: it is not lexically inside the` |
|         - | 4950 | `	 * outer's class body nor its member default, so a __TRAIT__ in the nested file` |
|         - | 4951 | `	 * must not inherit the outer's trait. (Restore below carries the outer's values` |
|         - | 4952 | `	 * back, so only the nested unit sees these zeros.) */` |
|         5 | 4953 | `	pGen->pCurClass = 0;` |
|         5 | 4954 | `	pGen->iInMemberDefault = 0;` |
|         5 | 4955 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|         5 | 4956 | `	pGen->xErr = xErr;` |
|         5 | 4957 | `	pGen->pErrData = pErrData;` |
|         5 | 4958 | `}` |
|         - | 4959 | `/*` |
|         - | 4960 | ` * Restore the outer compile-position state saved by PH7_CompilerSaveState,` |
|         - | 4961 | ` * releasing the nested unit's position containers first. The shared` |
|         - | 4962 | ` * hLiteral/hNumLiteral/hVar tables (grown by the nested compile) are carried` |
|         - | 4963 | ` * forward, NOT rolled back to the snapshot's stale headers.` |
|         - | 4964 | ` */` |
|         4 | 4965 | `PH7_PRIVATE void PH7_CompilerRestoreState(ph7_vm *pVm,ph7_gen_state *pSaved)` |
|         1 | 4966 | `{` |
|         5 | 4967 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 4968 | `	GenBlock *pBlock,*pParent;` |
|         - | 4969 | `	SyHash hVar,hLiteral,hNumLiteral;` |
|         - | 4970 | `	/* Free any nested blocks left open (e.g. an aborted nested compile), then the` |
|         - | 4971 | `	 * nested global block's own fixup sets. */` |
|         5 | 4972 | `	pBlock = pGen->pCurrent;` |
|         5 | 4973 | `	while( pBlock && pBlock->pParent != 0 ){` |
|       ! 0 | 4974 | `		pParent = pBlock->pParent;` |
|       ! 0 | 4975 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 4976 | `		pBlock = pParent;` |
|       ! 0 | 4977 | `	}` |
|         5 | 4978 | `	GenStateReleaseBlock(&pGen->sGlobal);` |
|         - | 4979 | `	/* Release the nested unit's position containers. */` |
|         5 | 4980 | `	SySetRelease(&pGen->aLabel);` |
|         5 | 4981 | `	SySetRelease(&pGen->aGoto);` |
|         5 | 4982 | `	SySetRelease(&pGen->aNullsafeJmp);` |
|         5 | 4983 | `	SySetRelease(&pGen->aLoopParent);` |
|         5 | 4984 | `	SySetRelease(&pGen->aScope);` |
|         5 | 4985 | `	SySetRelease(&pGen->aTrivia);` |
|         5 | 4986 | `	SySetRelease(&pGen->aPendingAttrs);` |
|         5 | 4987 | `	SyBlobRelease(&pGen->sWorker);` |
|         5 | 4988 | `	SyBlobRelease(&pGen->sErrBuf);` |
|         5 | 4989 | `	SyBlobRelease(&pGen->sFirstErr);` |
|         5 | 4990 | `	SyBlobRelease(&pGen->sNamespace);` |
|         5 | 4991 | `	SyHashRelease(&pGen->hUseImports);` |
|         5 | 4992 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|         5 | 4993 | `	SyHashRelease(&pGen->hUseConstImports);` |
|         5 | 4994 | `	GenStateReleaseSeenSymbols(&(*pGen));` |
|         - | 4995 | `	/* Preserve the (possibly grown) shared intern tables across the restore. */` |
|         5 | 4996 | `	hVar = pGen->hVar;` |
|         5 | 4997 | `	hLiteral = pGen->hLiteral;` |
|         5 | 4998 | `	hNumLiteral = pGen->hNumLiteral;` |
|         5 | 4999 | `	*pGen = *pSaved;` |
|         5 | 5000 | `	pGen->hVar = hVar;` |
|         5 | 5001 | `	pGen->hLiteral = hLiteral;` |
|         5 | 5002 | `	pGen->hNumLiteral = hNumLiteral;` |
|         5 | 5003 | `}` |
|         - | 5004 | `/*` |
|         - | 5005 | ` * Raise php's parse error for an unexpected token: E_PARSE with the exact text` |
|         - | 5006 | ` * php's parser prints, e.g.` |
|         - | 5007 | ` *` |
|         - | 5008 | ` *   syntax error, unexpected token ";", expecting "{"` |
|         - | 5009 | ` *   syntax error, unexpected identifier "invalid", expecting "("` |
|         - | 5010 | ` *   syntax error, unexpected end of file` |
|         - | 5011 | ` *` |
|         - | 5012 | ` * php names the token by CLASS, not just by text: an identifier, a variable and` |
|         - | 5013 | ` * a number each get their own noun, while everything else (keywords, operators,` |
|         - | 5014 | ` * punctuation) is a "token". zExpecting is the optional ", expecting ..." tail` |
|         - | 5015 | ` * — pass NULL when the site cannot say what it wanted (php often can't either).` |
|         - | 5016 | ` * pTok == NULL means the input ran out: "unexpected end of file".` |
|         - | 5017 | ` *` |
|         - | 5018 | ` * PHL's hand-written recursive-descent parser has no bison expectation sets, so` |
|         - | 5019 | ` * a site can only claim an "expecting" clause it genuinely knows; every clause` |
|         - | 5020 | ` * emitted here was verified against php 8.5.7 for the construct in question.` |
|         - | 5021 | ` */` |
|         - | 5022 | `/*` |
|         - | 5023 | `` * Rebuild the `<<<LABEL` marker of a heredoc/nowdoc token from the source the`` |
|         - | 5024 | ` * token's BODY points into: the header always sits immediately above it. Answers` |
|         - | 5025 | ` * FALSE when no marker is found within reach, in which case the caller falls back` |
|         - | 5026 | ` * to the generic noun rather than guessing.` |
|         - | 5027 | ` */` |
|         8 | 5028 | `static int GenStateHeredocMarker(SyString *pBody,SyString *pOut)` |
|         1 | 5029 | `{` |
|         9 | 5030 | `	const unsigned char *z = (const unsigned char *)pBody->zString;` |
|         - | 5031 | `	const unsigned char *zLabelEnd;` |
|         - | 5032 | `	/* Walk the header BACKWARDS from the body, which begins one byte past the` |
|         - | 5033 | `	 * terminator of the marker's own line: line terminator, trailing blanks, the` |
|         - | 5034 | ``	 * closing quote, the LABEL, the opening quote, leading blanks, `<<<`. Every`` |
|         - | 5035 | `	 * step stops on a byte the next step owns, so the walk cannot leave the` |
|         - | 5036 | ``	 * header -- `<` is not a label byte and a label is what sits above the body. */`` |
|         9 | 5037 | `	z--;` |
|         9 | 5038 | `	if( z[0] == '\n' ){` |
|         9 | 5039 | `		z--;` |
|         9 | 5040 | `		if( z[0] == '\r' ){` |
|       ! 0 | 5041 | `			z--;` |
|       ! 0 | 5042 | `		}` |
|         4 | 5043 | `	}` |
|         9 | 5044 | `	while( z[0] == ' ' \|\| z[0] == '\t' ){` |
|       ! 0 | 5045 | `		z--;` |
|       ! 0 | 5046 | `	}` |
|         9 | 5047 | `	if( z[0] == '"' \|\| z[0] == '\'' ){` |
|         5 | 5048 | `		z--;` |
|         2 | 5049 | `	}` |
|         9 | 5050 | `	zLabelEnd = &z[1];` |
|        33 | 5051 | `	while( z[0] >= 0x80 \|\| SyisAlphaNum(z[0]) \|\| z[0] == '_' ){` |
|        25 | 5052 | `		z--;` |
|         1 | 5053 | `	}` |
|         9 | 5054 | `	if( zLabelEnd == &z[1] ){` |
|       ! 0 | 5055 | `		return 0; /* No label: not a header this routine can read back */` |
|         - | 5056 | `	}` |
|         9 | 5057 | `	if( z[0] == '"' \|\| z[0] == '\'' ){` |
|         5 | 5058 | `		z--;` |
|         2 | 5059 | `	}` |
|        15 | 5060 | `	while( z[0] == ' ' \|\| z[0] == '\t' ){` |
|         7 | 5061 | `		z--;` |
|         1 | 5062 | `	}` |
|         9 | 5063 | `	if( !(z[0] == '<' && z[-1] == '<' && z[-2] == '<') ){` |
|       ! 0 | 5064 | `		return 0;` |
|         - | 5065 | `	}` |
|         - | 5066 | ``	/* php's token text runs from `<<<` to the end of the LABEL -- the opening`` |
|         - | 5067 | `	 * quote of a nowdoc is inside it, the closing one is not. */` |
|         9 | 5068 | `	SyStringInitFromBuf(pOut,&z[-2],(sxu32)(zLabelEnd - &z[-2]));` |
|         9 | 5069 | `	return 1;` |
|         5 | 5070 | `}` |
|       408 | 5071 | `PH7_PRIVATE sxi32 PH7_GenSyntaxError(` |
|         - | 5072 | `	ph7_gen_state *pGen,   /* Code generator state */` |
|         - | 5073 | `	SyToken *pTok,         /* Offending token, or NULL for end of file */` |
|         - | 5074 | `	const char *zExpecting /* ", expecting <this>" tail, or NULL */` |
|         - | 5075 | `	)` |
|         5 | 5076 | `{` |
|         - | 5077 | ``	/* php's noun for the offending token. An ALPHA-stream operator (`and`, `or`,`` |
|         - | 5078 | ``	 * `xor`, `new`, `clone`, `instanceof`) is lexed ID\|OP here but php calls it a`` |
|         - | 5079 | `	 * TOKEN, like every other reserved word — only a real identifier gets the` |
|         - | 5080 | `	 * "identifier" noun. */` |
|       413 | 5081 | `	const char *zNoun = "token";` |
|         - | 5082 | `	sxu32 nLine;` |
|       413 | 5083 | `	if( pTok == 0 && pGen->pTokenSet ){` |
|         - | 5084 | `		/* The caller ran out of tokens inside its own slice — but a statement's slice stops` |
|         - | 5085 | `		 * BEFORE its terminator, so the token php actually names (typically the ';') is the` |
|         - | 5086 | `		 * one sitting just past the slice, still inside the chunk's token stream. Reach for` |
|         - | 5087 | `		 * it before concluding "end of file". */` |
|       179 | 5088 | `		SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|       179 | 5089 | `		SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|       179 | 5090 | `		if( pGen->pEnd >= pBase && pGen->pEnd < pStreamEnd ){` |
|       116 | 5091 | `			pTok = pGen->pEnd;` |
|        56 | 5092 | `		}` |
|        87 | 5093 | `	}` |
|       413 | 5094 | `	nLine = pTok ? pTok->nLine : (pGen->pIn > (SyToken *)SySetBasePtr(pGen->pTokenSet) ? pGen->pIn[-1].nLine : 1);` |
|       413 | 5095 | `	if( pTok == 0 && pGen->bChunkAtEof && pGen->nChunkEofLine > nLine ){` |
|         - | 5096 | `		/* End of INPUT is reported where it sits, not where the last token ended. */` |
|         8 | 5097 | `		nLine = pGen->nChunkEofLine;` |
|         3 | 5098 | `	}` |
|       413 | 5099 | `	if( pTok == 0 ){` |
|        97 | 5100 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        31 | 5101 | `			zExpecting ? "syntax error, unexpected end of file, expecting %s"` |
|         - | 5102 | `			           : "syntax error, unexpected end of file",` |
|        31 | 5103 | `			zExpecting);` |
|         - | 5104 | `	}` |
|       351 | 5105 | `	if( (pTok->nType & PH7_TK_ID) && (pTok->nType & PH7_TK_OP) == 0 ){` |
|        47 | 5106 | `		zNoun = "identifier";` |
|       330 | 5107 | `	}else if( pTok->nType & PH7_TK_DOLLAR ){` |
|        12 | 5108 | `		zNoun = "variable";` |
|         - | 5109 | `		/* The '$' is its own token and carries only "$" as text; the NAME is the` |
|         - | 5110 | ``		 * token after it. php names the whole variable, so `$x` was being reported`` |
|         - | 5111 | ``		 * as the nameless `variable "$"`. Stitch the two back together. */`` |
|        12 | 5112 | `		if( pGen->pTokenSet ){` |
|        12 | 5113 | `			SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        12 | 5114 | `			SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|        12 | 5115 | `			SyToken *pName = &pTok[1];` |
|        10 | 5116 | `			if( pTok >= pBase && pName < pStreamEnd` |
|        10 | 5117 | `				&& (pName->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|        12 | 5118 | `				&& pName->sData.nByte > 0 ){` |
|        12 | 5119 | `				SyBlobReset(&pGen->sWorker);` |
|        12 | 5120 | `				SyBlobAppend(&pGen->sWorker,"$",sizeof(char));` |
|        12 | 5121 | `				SyBlobAppend(&pGen->sWorker,pName->sData.zString,pName->sData.nByte);` |
|         - | 5122 | `				{` |
|         - | 5123 | `					SyString sVar;` |
|        12 | 5124 | `					SyStringInitFromBuf(&sVar,SyBlobData(&pGen->sWorker),` |
|         - | 5125 | `						SyBlobLength(&pGen->sWorker));` |
|        12 | 5126 | `					if( zExpecting ){` |
|        14 | 5127 | `						return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|         - | 5128 | `							"syntax error, unexpected %s \"%z\", expecting %s",` |
|         4 | 5129 | `							zNoun,&sVar,zExpecting);` |
|         - | 5130 | `					}` |
|         4 | 5131 | `					return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|         1 | 5132 | `						"syntax error, unexpected %s \"%z\"",zNoun,&sVar);` |
|         - | 5133 | `				}` |
|         - | 5134 | `			}` |
|       ! 0 | 5135 | `		}` |
|       299 | 5136 | `	}else if( pTok->nType & PH7_TK_INTEGER ){` |
|        32 | 5137 | `		zNoun = "integer";` |
|       285 | 5138 | `	}else if( pTok->nType & PH7_TK_REAL ){` |
|         - | 5139 | ``		/* php's noun, which is not the type name: `float` is what the CAST is`` |
|         - | 5140 | ``		 * called, `floating-point number` what a stray literal is called. */`` |
|         7 | 5141 | `		zNoun = "floating-point number";` |
|       268 | 5142 | `	}else if( pTok->nType & (PH7_TK_SSTR\|PH7_TK_DSTR) ){` |
|         - | 5143 | `		/* php names a string literal by the QUOTE it was written with, and prints` |
|         - | 5144 | `		 * the SOURCE bytes between the quotes -- escapes unresolved, which is what` |
|         - | 5145 | `		 * the token already holds here. A double-quoted string that INTERPOLATES is` |
|         - | 5146 | `		 * not one token in php at all: its scanner emits the opening quote on its` |
|         - | 5147 | `		 * own, so the parser has nothing to quote and the noun stands alone. */` |
|        19 | 5148 | `		if( (pTok->nType & PH7_TK_DSTR) && GenStateDqInterpolates(&pTok->sData) ){` |
|         5 | 5149 | `			if( zExpecting ){` |
|         7 | 5150 | `				return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|         2 | 5151 | `					"syntax error, unexpected double-quote mark, expecting %s",zExpecting);` |
|         - | 5152 | `			}` |
|       ! 0 | 5153 | `			return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|         - | 5154 | `				"syntax error, unexpected double-quote mark");` |
|         - | 5155 | `		}` |
|        15 | 5156 | `		zNoun = (pTok->nType & PH7_TK_SSTR) ? "single-quoted string" : "double-quoted string";` |
|       254 | 5157 | `	}else if( pTok->nType & (PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){` |
|         - | 5158 | ``		/* php names the OPENING marker -- `<<<EOT`, or `<<<'EOT` for a nowdoc, the`` |
|         - | 5159 | `		 * closing quote dropped because the token text ends at the label -- and` |
|         - | 5160 | `		 * reports it on the line AFTER the marker's, its scanner having consumed` |
|         - | 5161 | `		 * that line's terminator before the token is handed over. The token here` |
|         - | 5162 | `		 * carries the BODY, so the marker is read back off the source it points` |
|         - | 5163 | `		 * into. */` |
|         - | 5164 | `		SyString sMark;` |
|         9 | 5165 | `		if( GenStateHeredocMarker(&pTok->sData,&sMark) ){` |
|         9 | 5166 | `			if( zExpecting ){` |
|        13 | 5167 | `				return PH7_GenCompileError(pGen,E_PARSE,nLine + 1,` |
|         4 | 5168 | `					"syntax error, unexpected heredoc start \"%z\", expecting %s",&sMark,zExpecting);` |
|         - | 5169 | `			}` |
|       ! 0 | 5170 | `			return PH7_GenCompileError(pGen,E_PARSE,nLine + 1,` |
|         - | 5171 | `				"syntax error, unexpected heredoc start \"%z\"",&sMark);` |
|         - | 5172 | `		}` |
|       ! 0 | 5173 | `	}` |
|       329 | 5174 | `	if( zExpecting ){` |
|       233 | 5175 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        76 | 5176 | `			"syntax error, unexpected %s \"%z\", expecting %s",zNoun,&pTok->sData,zExpecting);` |
|         - | 5177 | `	}` |
|       263 | 5178 | `	return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        86 | 5179 | `		"syntax error, unexpected %s \"%z\"",zNoun,&pTok->sData);` |
|       209 | 5180 | `}` |
|         - | 5181 | `/*` |
|         - | 5182 | ` * Generate a compile-time error message.` |
|         - | 5183 | ` * If the error count limit is reached (usually 15 error message)` |
|         - | 5184 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|         - | 5185 | ` * abort compilation immediately.` |
|         - | 5186 | ` */` |
|         - | 5187 | `/*` |
|         - | 5188 | `` * php's `Stack trace:` block under a compile-time FATAL: the activations that are`` |
|         - | 5189 | ` * live at the refusal -- the enclosing functions, and the include/require/eval that` |
|         - | 5190 | `` * loaded the unit being compiled -- then the `#N {main}` marker. A parse error gets`` |
|         - | 5191 | ` * none: that one is the parser's own refusal and php reports it as an E_PARSE.` |
|         - | 5192 | ` *` |
|         - | 5193 | ` * A refusal raised while the VM is still INITIALIZING is the main script's own` |
|         - | 5194 | ` * compile: there is no runtime state to walk (and no object pool to build the array` |
|         - | 5195 | ` * in), and php's answer there is the bare bottom marker.` |
|         - | 5196 | ` */` |
|       696 | 5197 | `PH7_PRIVATE void PH7_GenAppendFatalTrace(ph7_vm *pVm,SyBlob *pOut,int iTraceKind)` |
|         4 | 5198 | `{` |
|         - | 5199 | `	ph7_value *pTrace;` |
|       700 | 5200 | `	if( pVm == 0 \|\| iTraceKind == PH7_FATAL_TRACE_NONE ){` |
|        40 | 5201 | `		return;` |
|         - | 5202 | `	}` |
|       664 | 5203 | `	SyBlobAppend(pOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|       664 | 5204 | `	if( pVm->nMagic == PH7_VM_INIT ){` |
|       650 | 5205 | `		SyBlobAppend(pOut,"#0 {main}",sizeof("#0 {main}")-1);` |
|       650 | 5206 | `		return;` |
|         - | 5207 | `	}` |
|        18 | 5208 | `	pTrace = ph7_new_array(&(*pVm));` |
|        18 | 5209 | `	if( pTrace == 0 ){` |
|       ! 0 | 5210 | `		SyBlobAppend(pOut,"#0 {main}",sizeof("#0 {main}")-1);` |
|       ! 0 | 5211 | `		return;` |
|         - | 5212 | `	}` |
|         - | 5213 | `	/* php's fatal trace carries no argument list. Nor, unless this is one of the` |
|         - | 5214 | `	 * refusals php makes at RUN time, the include/require/eval that loaded the unit` |
|         - | 5215 | `	 * being compiled: php raises a compile error before it pushes that activation. */` |
|        18 | 5216 | `	VmBuildBacktrace(&(*pVm),0x2 \| (iTraceKind == PH7_FATAL_TRACE_RUNTIME ? 0 : 0x4),0,pTrace);` |
|        18 | 5217 | `	PH7_VmTraceToString(&(*pVm),pTrace,TRUE,pOut);` |
|        18 | 5218 | `	ph7_release_value(&(*pVm),pTrace);` |
|       352 | 5219 | `}` |
|      1572 | 5220 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|         5 | 5221 | `{` |
|      1577 | 5222 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|      1577 | 5223 | `	const char *zErr = "Error";` |
|         - | 5224 | `	SyString *pFile;` |
|         - | 5225 | `	va_list ap;` |
|         - | 5226 | `	sxi32 rc;` |
|         - | 5227 | `	/* Reset the working buffer. NOT when nobody is logging: there the buffer is a` |
|         - | 5228 | `	 * one-message store eval() reads its ParseError text out of, and php stops at` |
|         - | 5229 | `	 * the FIRST error where this generator carries on to a budget of fifteen -- so` |
|         - | 5230 | ``	 * resetting handed eval the LAST message. `eval('echo 1 foo;')` reported`` |
|         - | 5231 | ``	 * `unexpected token ";"`, the synchronizer's own complaint, where php names the`` |
|         - | 5232 | ``	 * `identifier "foo"` it choked on. */`` |
|      1577 | 5233 | `	if( pGen->xErr ){` |
|      1441 | 5234 | `		SyBlobReset(pWorker);` |
|       718 | 5235 | `	}` |
|         - | 5236 | `	/* Peek the processed file path if available */` |
|      1577 | 5237 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|      1577 | 5238 | `	if( nErrType == E_ERROR \|\| nErrType == E_PARSE ){` |
|         - | 5239 | `		/* Increment the error counter. A PARSE error is every bit as fatal as an` |
|         - | 5240 | `		 * E_ERROR one: php compiles nothing, runs nothing and exits 255. Counting` |
|         - | 5241 | `		 * only E_ERROR let a parse error print its diagnostic and then fall through` |
|         - | 5242 | `		 * into execution with a 0 exit status. */` |
|      1535 | 5243 | `		pGen->nErr++;` |
|      1535 | 5244 | `		if( nErrType == E_ERROR ){` |
|         - | 5245 | `			/* php's E_COMPILE_ERROR: an uncatchable fatal, where a parse error is a` |
|         - | 5246 | `			 * catchable ParseError. The include path reads this to tell them apart. */` |
|       832 | 5247 | `			pGen->nFatal++;` |
|       414 | 5248 | `		}` |
|      1535 | 5249 | `		if( pGen->nErr == 1 ){` |
|         - | 5250 | `			/* Keep the FIRST refusal's bare text and line: it is the one php reports,` |
|         - | 5251 | `			 * and it is the message an include's ParseError carries. */` |
|         - | 5252 | `			va_list apF;` |
|      1221 | 5253 | `			SyBlobReset(&pGen->sFirstErr);` |
|      1221 | 5254 | `			va_start(apF,zFormat);` |
|      1221 | 5255 | `			SyBlobFormatAp(&pGen->sFirstErr,zFormat,apF);` |
|      1221 | 5256 | `			va_end(apF);` |
|      1221 | 5257 | `			pGen->nFirstErrLine = nLine;` |
|       613 | 5258 | `		}else{` |
|         - | 5259 | `			/* php stops at the first one. This generator recovers and carries on so` |
|         - | 5260 | `			 * that the rest of the unit is still walked (a later pass needs the` |
|         - | 5261 | `			 * symbols), but everything it says after the first refusal is its own` |
|         - | 5262 | `			 * recovery talking -- and printing it put diagnostics on the user's` |
|         - | 5263 | `			 * screen that php, having stopped, never reaches.` |
|         - | 5264 | `			 *` |
|         - | 5265 | `			 * The recovery is still bounded, and now SILENTLY: the old limit` |
|         - | 5266 | ``			 * announced itself with a `Error count limit reached` line of PH7's own`` |
|         - | 5267 | `			 * invention, which no php prints and which would land on top of the one` |
|         - | 5268 | `			 * diagnostic php does. The unit has already failed and its first message` |
|         - | 5269 | `			 * is already recorded, so there is nothing left to say. */` |
|       318 | 5270 | `			return (pGen->nErr > 15) ? SXERR_ABORT : SXRET_OK;` |
|         - | 5271 | `		}` |
|       608 | 5272 | `	}` |
|      1263 | 5273 | `	if( nErrType == E_PARSE && pGen->bParseThrows ){` |
|         - | 5274 | `		/* An include/require unit: php's parser throws a ParseError rather than` |
|         - | 5275 | `		 * printing, and the text only reaches the screen if nobody catches it.` |
|         - | 5276 | `		 * The caller (VmEvalChunk) raises it from sFirstErr. */` |
|         8 | 5277 | `		return SXRET_OK;` |
|         - | 5278 | `	}` |
|      1257 | 5279 | `	if( pGen->xErr == 0 ){` |
|         - | 5280 | `		/* No consumer — but keep the BARE message in the error buffer so a caller` |
|         - | 5281 | `		 * that needs the text can read it back. eval() compiles with logging off` |
|         - | 5282 | `		 * (a parse error there is php's catchable ParseError, not a printed` |
|         - | 5283 | `		 * diagnostic) and needs exactly this string for the exception message. The` |
|         - | 5284 | `		 * first message stands: everything after it is this generator's recovery` |
|         - | 5285 | `		 * talking, and php never got that far. */` |
|        97 | 5286 | `		if( SyBlobLength(pWorker) < 1 ){` |
|        97 | 5287 | `			va_start(ap,zFormat);` |
|        97 | 5288 | `			SyBlobFormatAp(pWorker,zFormat,ap);` |
|        97 | 5289 | `			va_end(ap);` |
|        47 | 5290 | `		}` |
|        97 | 5291 | `		return SXRET_OK;` |
|         - | 5292 | `	}` |
|      1163 | 5293 | `	switch(nErrType){` |
|       698 | 5294 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|        47 | 5295 | `	case E_WARNING: zErr = "Warning";     break;` |
|       426 | 5296 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       ! 0 | 5297 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|       ! 0 | 5298 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|       ! 0 | 5299 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|       ! 0 | 5300 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|       ! 0 | 5301 | `	case 8192 /* E_DEPRECATED */: zErr = "Deprecated"; break;` |
|       ! 0 | 5302 | `	default:` |
|       ! 0 | 5303 | `		break;` |
|         - | 5304 | `	}` |
|      1163 | 5305 | `	rc = SXRET_OK;` |
|         - | 5306 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|      1163 | 5307 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|      1163 | 5308 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|      1163 | 5309 | `	va_start(ap,zFormat);` |
|      1163 | 5310 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|      1163 | 5311 | `	va_end(ap);` |
|      1163 | 5312 | `	if( pFile ){` |
|      1163 | 5313 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       579 | 5314 | `	}` |
|      1163 | 5315 | `	if( nErrType == E_ERROR ){` |
|       698 | 5316 | `		PH7_GenAppendFatalTrace(pGen->pVm,pWorker,pGen->iFatalTrace);` |
|       347 | 5317 | `	}` |
|         - | 5318 | `	/* iFatalTrace is a ONE-SHOT: a site that raises one of php's non-compiler` |
|         - | 5319 | `	 * refusals sets it just before the call and this consumes it, so no site has to` |
|         - | 5320 | `	 * remember to put it back and none of them can leak it onto a later refusal. */` |
|      1163 | 5321 | `	pGen->iFatalTrace = PH7_FATAL_TRACE_COMPILE;` |
|         - | 5322 | `	/* Append a new line */` |
|      1163 | 5323 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|      1163 | 5324 | `	if( SyBlobLength(pWorker) > 0 ){` |
|         - | 5325 | `		/* Consume the generated error message */` |
|      1163 | 5326 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       579 | 5327 | `	}` |
|      1163 | 5328 | `	return rc;` |
|       791 | 5329 | `}` |
|         - | 5330 |  |
