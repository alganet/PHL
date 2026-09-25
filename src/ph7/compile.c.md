# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1861/2001 lines (93.00%)

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
|        - |    9 | ` * This file implement a thread-safe and full-reentrant compiler for the PH7 engine.` |
|        - |   10 | ` * That is, routines defined in this file takes a stream of tokens and output` |
|        - |   11 | ` * PH7 bytecode instructions.` |
|        - |   12 | ` */` |
|        - |   13 | `/* Forward declaration */` |
|        - |   14 | `/*` |
|        - |   15 | ` * Local utility routines used in the code generation phase.` |
|        - |   16 | ` */` |
|        - |   17 | `/*` |
|        - |   18 | ` * Check if the given name refer to a valid label declared in the given function` |
|        - |   19 | ` * (NULL = file scope).` |
|        - |   20 | ` * Return SXRET_OK and write a pointer to that label on success.` |
|        - |   21 | ` * Any other return value indicates no such label.` |
|        - |   22 | ` *` |
|        - |   23 | ` * Labels are scoped PER FUNCTION in php, so the owning function is part of the key:` |
|        - |   24 | ` * the same name may be declared in as many functions as one likes, and each goto sees` |
|        - |   25 | ` * only its own. Matching on the name alone made the first declaration win everywhere,` |
|        - |   26 | `` * which rejected `function a(){ done: } function b(){ goto done; done: }` — ordinary`` |
|        - |   27 | ` * php — as a jump to an undefined label.` |
|        - |   28 | ` *` |
|        - |   29 | ` * Also serves PH7_CompileLabel, which asks the same question at DECLARATION time to reject` |
|        - |   30 | ` * a name its function already declared.` |
|        - |   31 | ` */` |
|      432 |   32 | `PH7_PRIVATE sxi32 GenStateGetLabel(ph7_gen_state *pGen,SyString *pName,ph7_vm_func *pFunc,Label **ppOut)` |
|        5 |   33 | `{` |
|        - |   34 | `	Label *aLabel;` |
|        - |   35 | `	sxu32 n;` |
|        - |   36 | `	/* Perform a linear scan on the label table */` |
|      437 |   37 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|     1601 |   38 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     1329 |   39 | `		if( aLabel[n].pFunc == pFunc && SyStringCmp(&aLabel[n].sName,pName,SyMemcmp) == 0 ){` |
|        - |   40 | `			/* Jump destination found */` |
|      165 |   41 | `			if( ppOut ){` |
|      161 |   42 | `				*ppOut = &aLabel[n];` |
|       78 |   43 | `			}` |
|      165 |   44 | `			return SXRET_OK;` |
|        - |   45 | `		}` |
|      587 |   46 | `	}` |
|        - |   47 | `	/* No such destination */` |
|      277 |   48 | `	return SXERR_NOTFOUND;` |
|      221 |   49 | `}` |
|        - |   50 | `/*` |
|        - |   51 | ` * Fetch a block that correspond to the given criteria from the stack of` |
|        - |   52 | ` * compiled blocks.` |
|        - |   53 | ` * Return a pointer to that block on success. NULL otherwise.` |
|        - |   54 | ` */` |
|    41496 |   55 | `PH7_PRIVATE GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|        5 |   56 | `{` |
|    41501 |   57 | `	GenBlock *pBlock = pCurrent;` |
|    93082 |   58 | `	for(;;){` |
|   186169 |   59 | `		if( pBlock->iFlags & iBlockType ){` |
|    41483 |   60 | `			iCount--; /* Decrement nesting level */` |
|    41483 |   61 | `			if( iCount < 1 ){` |
|        - |   62 | `				/* Block meet with the desired criteria */` |
|    41451 |   63 | `				return pBlock;` |
|        - |   64 | `			}` |
|       16 |   65 | `		}` |
|        - |   66 | `		/* Point to the upper block */` |
|   144723 |   67 | `		pBlock = pBlock->pParent;` |
|   144723 |   68 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|        - |   69 | `			/* Forbidden */` |
|       28 |   70 | `			break;` |
|        - |   71 | `		}` |
|        5 |   72 | `	}` |
|        - |   73 | `	/* No such block */` |
|       53 |   74 | `	return 0;` |
|    20753 |   75 | `}` |
|        - |   76 | `/*` |
|        - |   77 | ` * Initialize a freshly allocated block instance.` |
|        - |   78 | ` */` |
|  1286306 |   79 | `static void GenStateInitBlock(` |
|        - |   80 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |   81 | `	GenBlock *pBlock,    /* Target block */` |
|        - |   82 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|        - |   83 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|        - |   84 | `	void *pUserData      /* Upper layer private data */` |
|        - |   85 | `	)` |
|        5 |   86 | `{` |
|        - |   87 | `	/* Initialize block fields */` |
|  1286311 |   88 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  1286311 |   89 | `	pBlock->pUserData   = pUserData;` |
|  1286311 |   90 | `	pBlock->pGen        = pGen;` |
|  1286311 |   91 | `	pBlock->iFlags      = iType;` |
|  1286311 |   92 | `	pBlock->pParent     = 0;` |
|  1286311 |   93 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  1286311 |   94 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  1286311 |   95 | `}` |
|        - |   96 | `/*` |
|        - |   97 | ` * Allocate a new block instance.` |
|        - |   98 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|        - |   99 | ` * on success.Otherwise generate a compile-time error and abort` |
|        - |  100 | ` * processing on failure.` |
|        - |  101 | ` */` |
|  1281156 |  102 | `PH7_PRIVATE sxi32 GenStateEnterBlock(` |
|        - |  103 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - |  104 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|        - |  105 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|        - |  106 | `	void *pUserData,      /* Upper layer private data */` |
|        - |  107 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|        - |  108 | `	)` |
|        5 |  109 | `{` |
|        - |  110 | `	GenBlock *pBlock;` |
|        - |  111 | `	/* Allocate a new block instance */` |
|  1281161 |  112 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  1281161 |  113 | `	if( pBlock == 0 ){` |
|        - |  114 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  115 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  116 | `		 */` |
|      ! 0 |  117 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|        - |  118 | `		/* Abort processing immediately */` |
|      ! 0 |  119 | `		return SXERR_ABORT;` |
|        - |  120 | `	}` |
|        - |  121 | `	/* Zero the structure */` |
|  1281161 |  122 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  1281161 |  123 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|        - |  124 | `	/* Link to the parent block */` |
|  1281161 |  125 | `	pBlock->pParent = pGen->pCurrent;` |
|        - |  126 | `	/* A loop or switch gets an id, and remembers the loop it nests inside, so a goto's` |
|        - |  127 | `	 * and a label's positions can be compared after compilation (see aLoopParent). */` |
|  1281161 |  128 | `	if( iType & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|   105413 |  129 | `		sxu32 nParent = pGen->nCurLoopId;` |
|   105413 |  130 | `		pGen->nLoopId++;` |
|   105413 |  131 | `		SySetPut(&pGen->aLoopParent,(const void *)&nParent);` |
|   105413 |  132 | `		pBlock->nLoopId = pGen->nLoopId;` |
|   105413 |  133 | `		pBlock->nOuterLoopId = nParent;` |
|   105413 |  134 | `		pGen->nCurLoopId = pGen->nLoopId;` |
|    52704 |  135 | `	}` |
|        - |  136 | `	/* A try/catch/finally block gets a scope id, and remembers the scope it nests inside,` |
|        - |  137 | `	 * so the chain between any two points can be walked after compilation (aScope). Every` |
|        - |  138 | `	 * other block simply inherits the scope in effect. */` |
|  1281161 |  139 | `	pBlock->nOuterScopeId = pGen->nCurScopeId;` |
|  1281161 |  140 | `	pBlock->nScopeId = pGen->nCurScopeId;` |
|  1281161 |  141 | `	if( iType & GEN_BLOCK_EXCEPTION ){` |
|        - |  142 | `		GenScope sScope;` |
|     7175 |  143 | `		sScope.nParent = pGen->nCurScopeId;` |
|     7175 |  144 | `		sScope.pUserData = pUserData;` |
|     7175 |  145 | `		if( iType & GEN_BLOCK_FINALLY ){` |
|      299 |  146 | `			sScope.iKind = GEN_SCOPE_FINALLY;` |
|     7028 |  147 | `		}else if( iType & GEN_BLOCK_DETACHED ){` |
|     3275 |  148 | `			sScope.iKind = GEN_SCOPE_DETACHED;` |
|     1640 |  149 | `		}else{` |
|        - |  150 | `			/* A try. pUserData is its ph7_exception, which every try-block site passes at` |
|        - |  151 | `			 * ENTRY precisely so this can classify it. */` |
|     3611 |  152 | `			sScope.iKind = GenStateInlineTryCatch(pGen) ? GEN_SCOPE_TRY_INLINE : GEN_SCOPE_TRY;` |
|        - |  153 | `		}` |
|     7175 |  154 | `		if( SySetPut(&pGen->aScope,(const void *)&sScope) == SXRET_OK ){` |
|     7175 |  155 | `			pBlock->nScopeId = SySetUsed(&pGen->aScope);` |
|     7175 |  156 | `			pGen->nCurScopeId = pBlock->nScopeId;` |
|     3585 |  157 | `		}` |
|     3585 |  158 | `	}` |
|        - |  159 | `	/* Mark as the current block */` |
|  1281161 |  160 | `	pGen->pCurrent = pBlock;` |
|  1281161 |  161 | `	if( ppBlock ){` |
|        - |  162 | `		/* Write a pointer to the new instance */` |
|   606897 |  163 | `		*ppBlock = pBlock;` |
|   303446 |  164 | `	}` |
|  1281161 |  165 | `	return SXRET_OK;` |
|   640583 |  166 | `}` |
|        - |  167 | `/*` |
|        - |  168 | ` * Release block fields without freeing the whole instance.` |
|        - |  169 | ` */` |
|  1281150 |  170 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|        5 |  171 | `{` |
|  1281155 |  172 | `	SySetRelease(&pBlock->aPostContFix);` |
|  1281155 |  173 | `	SySetRelease(&pBlock->aJumpFix);` |
|  1281155 |  174 | `}` |
|        - |  175 | `/*` |
|        - |  176 | ` * Release a block.` |
|        - |  177 | ` */` |
|  1281146 |  178 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|        5 |  179 | `{` |
|  1281151 |  180 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  1281151 |  181 | `	GenStateReleaseBlock(&(*pBlock));` |
|        - |  182 | `	/* Free the instance */` |
|  1281151 |  183 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  1281151 |  184 | `}` |
|        - |  185 | `/*` |
|        - |  186 | ` * POP and release a block from the stack of compiled blocks.` |
|        - |  187 | ` */` |
|  1281146 |  188 | `PH7_PRIVATE sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|        5 |  189 | `{` |
|  1281151 |  190 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  1281151 |  191 | `	if( pBlock == 0 ){` |
|        - |  192 | `		/* No more block to pop */` |
|      ! 0 |  193 | `		return SXERR_EMPTY;` |
|        - |  194 | `	}` |
|  1281151 |  195 | `	if( pBlock->iFlags & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|   105405 |  196 | `		pGen->nCurLoopId = pBlock->nOuterLoopId;` |
|    52700 |  197 | `	}` |
|  1281151 |  198 | `	if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|     7175 |  199 | `		pGen->nCurScopeId = pBlock->nOuterScopeId;` |
|     3585 |  200 | `	}` |
|        - |  201 | `	/* Point to the upper block */` |
|  1281151 |  202 | `	pGen->pCurrent = pBlock->pParent;` |
|  1281151 |  203 | `	if( ppBlock ){` |
|        - |  204 | `		/* Write a pointer to the popped block */` |
|      ! 0 |  205 | `		*ppBlock = pBlock;` |
|      ! 0 |  206 | `	}else{` |
|        - |  207 | `		/* Safely release the block */` |
|  1281151 |  208 | `		GenStateFreeBlock(&(*pBlock));` |
|        - |  209 | `	}` |
|  1281151 |  210 | `	return SXRET_OK;` |
|   640578 |  211 | `}` |
|        - |  212 | `/*` |
|        - |  213 | ` * PHP-parity redeclaration guard.` |
|        - |  214 | ` *` |
|        - |  215 | ` * PHP raises a fatal "Cannot redeclare ..." when a class/interface/trait/enum` |
|        - |  216 | ` * or a function is declared a second time. PHL hoists every declaration into` |
|        - |  217 | `` * the VM at compile time (so `if(false){class C{}}` already makes C exist), and`` |
|        - |  218 | ` * historically it silently *overwrote* duplicates. We reproduce PHP for the` |
|        - |  219 | ` * case that matters and that real code hits: a declaration that is` |
|        - |  220 | ` * UNCONDITIONAL and at file top level, whose name is already bound by another` |
|        - |  221 | ` * unconditional top-level declaration (or by a builtin). Conditional` |
|        - |  222 | ` * declarations (inside if/loops/switch/try or nested in a function) are left` |
|        - |  223 | `` * hoisting as before, so the `if(!class_exists('C')){class C{}}` and`` |
|        - |  224 | `` * `if(false){class C{}} class C{}` guard idioms keep working.`` |
|        - |  225 | ` *` |
|        - |  226 | ` * Included files compile at include time (i.e. at run time relative to the main` |
|        - |  227 | ` * script), so this compile-time check surfaces the fatal at the same moment PHP` |
|        - |  228 | ` * does for the cross-include case too.` |
|        - |  229 | ` */` |
|   150540 |  230 | `PH7_PRIVATE int GenStateUnconditionalTopLevel(ph7_gen_state *pGen)` |
|        5 |  231 | `{` |
|   150545 |  232 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   150745 |  233 | `	while( pBlock ){` |
|   150745 |  234 | `		if( pBlock->iFlags & (GEN_BLOCK_COND\|GEN_BLOCK_LOOP\|GEN_BLOCK_FUNC\|GEN_BLOCK_SWITCH\|GEN_BLOCK_EXCEPTION) ){` |
|      161 |  235 | `			return 0; /* conditional / nested */` |
|        - |  236 | `		}` |
|   150589 |  237 | `		if( pBlock->iFlags & GEN_BLOCK_GLOBAL ){` |
|   150389 |  238 | `			return 1; /* reached the global block with no conditional ancestor */` |
|        - |  239 | `		}` |
|      205 |  240 | `		pBlock = pBlock->pParent;` |
|        5 |  241 | `	}` |
|      ! 0 |  242 | `	return 1;` |
|    75275 |  243 | `}` |
|        - |  244 | `/*` |
|        - |  245 | ` * Guard a top-level function about to be installed. Same contract as the class` |
|        - |  246 | ` * guard above.` |
|        - |  247 | ` */` |
|   146720 |  248 | `PH7_PRIVATE sxi32 GenStateGuardFuncRedeclaration(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|        5 |  249 | `{` |
|        - |  250 | `	SyHashEntry *pEntry;` |
|   146725 |  251 | `	if( !GenStateUnconditionalTopLevel(pGen) ){` |
|      103 |  252 | `		return SXRET_OK;` |
|        - |  253 | `	}` |
|   146625 |  254 | `	pFunc->iFlags \|= VM_FUNC_BOUND;` |
|   146625 |  255 | `	if( pGen->pVm->bCompilingBuiltin ){` |
|   144093 |  256 | `		return SXRET_OK;` |
|        - |  257 | `	}` |
|        - |  258 | `	/* Host functions present AT COMPILE TIME are guarded here, and they have to be` |
|        - |  259 | `	 * for the migration of the builtin library into C to be behaviour-preserving: a` |
|        - |  260 | `	 * function moving from an embedded PHP chunk to a C routine moves from hFunction` |
|        - |  261 | `	 * to hHostFunction, and would otherwise silently LOSE the redeclaration guard it` |
|        - |  262 | ``	 * had. `function ini_get(){}` really did win over the builtin for the rest of`` |
|        - |  263 | `	 * the program once ini_get became C.` |
|        - |  264 | `	 *` |
|        - |  265 | `	 * Which builtins that covers depends on WHERE they register. The subsystems` |
|        - |  266 | `	 * installed inside PH7_VmInit's bCompilingBuiltin window (INI, libxml, ...) are` |
|        - |  267 | `	 * in hHostFunction before any user code compiles, so they are caught. The ~650` |
|        - |  268 | `	 * core builtins (strlen, ...) register later, in PH7_VmMakeReady, which runs` |
|        - |  269 | `	 * AFTER compilation — hHostFunction has no entry for them yet, so shadowing one` |
|        - |  270 | `	 * remains the known divergence it has always been (php fatals; §7.2). Nothing` |
|        - |  271 | `	 * about their behaviour changes here.` |
|        - |  272 | `	 *` |
|        - |  273 | `	 * The bCompilingBuiltin early-return above keeps the prelude itself exempt. */` |
|     2537 |  274 | `	if( SyHashGet(&pGen->pVm->hHostFunction,(const void *)pFunc->sName.zString,pFunc->sName.nByte) ){` |
|        8 |  275 | `		PH7_GenCompileError(pGen,E_ERROR,pFunc->nLine,` |
|        2 |  276 | `			"Cannot redeclare function %z()",&pFunc->sName);` |
|        6 |  277 | `		return SXERR_ABORT;` |
|        - |  278 | `	}` |
|     2533 |  279 | `	pEntry = SyHashGet(&pGen->pVm->hFunction,(const void *)pFunc->sName.zString,pFunc->sName.nByte);` |
|     2533 |  280 | `	if( pEntry ){` |
|        8 |  281 | `		ph7_vm_func *pPrev = (ph7_vm_func *)pEntry->pUserData;` |
|       10 |  282 | `		while( pPrev ){` |
|        8 |  283 | `			if( pPrev->iFlags & VM_FUNC_BOUND ){` |
|        5 |  284 | `				if( pPrev->sFile.nByte > 0 ){` |
|        7 |  285 | `					PH7_GenCompileError(pGen,E_ERROR,pFunc->nLine,` |
|        - |  286 | `						"Cannot redeclare function %z() (previously declared in %.*s:%u)",` |
|        2 |  287 | `						&pFunc->sName,pPrev->sFile.nByte,pPrev->sFile.zString,pPrev->nLine);` |
|        3 |  288 | `				}else{` |
|      ! 0 |  289 | `					PH7_GenCompileError(pGen,E_ERROR,pFunc->nLine,` |
|      ! 0 |  290 | `						"Cannot redeclare function %z()",&pFunc->sName);` |
|        - |  291 | `				}` |
|        5 |  292 | `				return SXERR_ABORT;` |
|        - |  293 | `			}` |
|        3 |  294 | `			pPrev = pPrev->pNextName;` |
|        1 |  295 | `		}` |
|        1 |  296 | `	}` |
|     2529 |  297 | `	return SXRET_OK;` |
|    73365 |  298 | `}` |
|        - |  299 | `/*` |
|        - |  300 | ` * Emit a forward jump.` |
|        - |  301 | ` * Notes on forward jumps` |
|        - |  302 | ` *  Compilation of some PHP constructs such as if,for,while and the logical or` |
|        - |  303 | ` *  (\|\|) and logical and (&&) operators in expressions requires the` |
|        - |  304 | ` *  generation of forward jumps.` |
|        - |  305 | ` *  Since the destination PC target of these jumps isn't known when the jumps` |
|        - |  306 | ` *  are emitted, we record each forward jump in an instance of the following` |
|        - |  307 | ` *  structure. Those jumps are fixed later when the jump destination is resolved.` |
|        - |  308 | ` */` |
|   695796 |  309 | `PH7_PRIVATE sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|        5 |  310 | `{` |
|        - |  311 | `	JumpFixup sJumpFix;` |
|        - |  312 | `	sxi32 rc;` |
|        - |  313 | `	/* Init the JumpFixup structure */` |
|   695801 |  314 | `	sJumpFix.nJumpType = nJumpType;` |
|   695801 |  315 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|        - |  316 | `	/* Remember which bytecode array the emitted instruction lives in: the block whose` |
|        - |  317 | `	 * table this lands in may be resolved after a container swap (see JumpFixup). */` |
|   695801 |  318 | `	sJumpFix.pContainer = PH7_VmGetByteCodeContainer(pBlock->pGen->pVm);` |
|        - |  319 | `	/* Insert in the jump fixup table */` |
|   695801 |  320 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|   695801 |  321 | `	return rc;` |
|        5 |  322 | `}` |
|        - |  323 | `/*` |
|        - |  324 | ` * TRUE when the body being compiled has its try/catch/finally compiled INLINE` |
|        - |  325 | ` * (ROOT C, generator bodies) rather than into detached mini-programs.` |
|        - |  326 | ` */` |
|     3606 |  327 | `PH7_PRIVATE int GenStateInlineTryCatch(ph7_gen_state *pGen)` |
|        5 |  328 | `{` |
|     3611 |  329 | `	return pGen->bInGenerator && pGen->pVm->bInlineTryCatch;` |
|        5 |  330 | `}` |
|        - |  331 | `/*` |
|        - |  332 | ` * Walk the scope chain from nFrom (where a jump is) out to nTo (where it lands) and` |
|        - |  333 | `` * describe what it crosses. One walk serves `break`, `continue` and `goto` alike,`` |
|        - |  334 | ` * because they all ask the same question of the same chain — only the two endpoints` |
|        - |  335 | ` * differ, and for a goto they are not both known until compilation ends.` |
|        - |  336 | ` *` |
|        - |  337 | ` * Returns TRUE when nTo was actually reached, i.e. the target's scope ENCLOSES the` |
|        - |  338 | ` * jump. FALSE means the target sits inside a try/catch the jump is not in — jumping` |
|        - |  339 | ` * into one, which PHL cannot express (its handler is pushed by the try's` |
|        - |  340 | ` * OP_LOAD_EXCEPTION, and a catch body is a mini-program entered at instruction 0).` |
|        - |  341 | ` * Depth counting cannot answer this: two sibling trys have the same depth.` |
|        - |  342 | ` *` |
|        - |  343 | ` * What is counted, for the opcode the caller then picks:` |
|        - |  344 | ` *  nDet    — DETACHED catch/finally bodies left. Each is its own bytecode array, so a` |
|        - |  345 | ` *            jump out of one cannot be a plain OP_JMP: it parks and travels out through` |
|        - |  346 | ` *            one OP_POP_EXCEPTION landing pad per boundary (OP_CATCH_JMP);` |
|        - |  347 | ` *  nTry    — legacy trys left whose OP_POP_EXCEPTION the jump SKIPS, so nothing else` |
|        - |  348 | ` *            would run their finally. Trys BELOW the first boundary do not qualify: a` |
|        - |  349 | ` *            break/continue emits their OP_POP_EXCEPTION right here (bEmitPops), and a` |
|        - |  350 | ` *            goto drains them where it parks — hence the reset when one is reached;` |
|        - |  351 | ` *  nInline — ROOT C inline trys left. Their finallys are driven by VmFinallyAdvance,` |
|        - |  352 | ` *            not by the aException drain, so they are crossed with OP_SET_FINALLY_JMP;` |
|        - |  353 | `` *  nFinally — `finally` bodies left, which php forbids outright. When this is non-zero the`` |
|        - |  354 | ` *            three above are NOT computed: callers must test it first and reject.` |
|        - |  355 | ` */` |
|    41600 |  356 | `PH7_PRIVATE int GenStateJumpScope(ph7_gen_state *pGen,sxu32 nFrom,sxu32 nTo,int bEmitPops,` |
|        - |  357 | `	GenJumpScope *pScope)` |
|        5 |  358 | `{` |
|    41605 |  359 | `	GenScope *aScope = (GenScope *)SySetBasePtr(&pGen->aScope);` |
|    41605 |  360 | `	sxu32 nUsed = SySetUsed(&pGen->aScope);` |
|    41605 |  361 | `	sxu32 nCur = nFrom;` |
|    41605 |  362 | `	SyZero(pScope,sizeof(*pScope));` |
|    41719 |  363 | `	while( nCur != nTo ){` |
|        - |  364 | `		GenScope *pScopeEnt;` |
|      123 |  365 | `		if( nCur == 0 \|\| nCur > nUsed ){` |
|        6 |  366 | `			return FALSE; /* ran off the top without meeting nTo */` |
|        - |  367 | `		}` |
|      119 |  368 | `		pScopeEnt = &aScope[nCur - 1];` |
|      119 |  369 | `		if( pScopeEnt->iKind == GEN_SCOPE_FINALLY ){` |
|        - |  370 | ``			/* php: `jump out of a finally block is disallowed`. Counted rather than`` |
|        - |  371 | `			 * rejected here because the caller owns the diagnostic and its line — but` |
|        - |  372 | `			 * ONLY counted: the jump is illegal, so the other three fields are left as` |
|        - |  373 | `			 * they are rather than pretending to describe a crossing that will never be` |
|        - |  374 | `			 * emitted. (They could not be right anyway: this kind covers both the legacy` |
|        - |  375 | `			 * detached finally and the generator's INLINE one, which is not a separate` |
|        - |  376 | `			 * bytecode container.) Every caller tests nFinally first. A jump that stays` |
|        - |  377 | `			 * INSIDE the finally never reaches this scope, so it stays legal. */` |
|       14 |  378 | `			pScope->nFinally++;` |
|      113 |  379 | `		}else if( pScopeEnt->iKind == GEN_SCOPE_DETACHED ){` |
|       73 |  380 | `			if( pScope->nDet == 0 ){` |
|       69 |  381 | `				pScope->nTry = 0;    /* below the first boundary: not the landing pad's */` |
|       69 |  382 | `				pScope->nInline = 0;` |
|       33 |  383 | `			}` |
|       73 |  384 | `			pScope->nDet++;` |
|       73 |  385 | `		}else if( pScopeEnt->iKind == GEN_SCOPE_TRY_INLINE ){` |
|       10 |  386 | `			pScope->nInline++;` |
|       34 |  387 | `		}else if( pScope->nDet == 0 && bEmitPops ){` |
|        3 |  388 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pScopeEnt->pUserData,0);` |
|        2 |  389 | `		}else{` |
|       27 |  390 | `			pScope->nTry++;` |
|        - |  391 | `		}` |
|      119 |  392 | `		nCur = pScopeEnt->nParent;` |
|        5 |  393 | `	}` |
|    41601 |  394 | `	return TRUE;` |
|    20805 |  395 | `}` |
|        - |  396 | `/*` |
|        - |  397 | ` * Pick the jump opcode for a crossing described by GenStateJumpScope, and its iP1.` |
|        - |  398 | ` * Shared by break/continue (which know their target at emit time) and goto (which` |
|        - |  399 | ` * settles this in GenStateFixGoto, once the label fixes the counts).` |
|        - |  400 | ` */` |
|    41488 |  401 | `PH7_PRIVATE sxi32 GenStateScopeJumpOp(const GenJumpScope *pCross,sxi32 *piP1)` |
|        5 |  402 | `{` |
|    41493 |  403 | `	if( pCross->nDet > 0 \|\| pCross->nTry > 0 ){` |
|       83 |  404 | `		*piP1 = PH7_CATCH_JMP_P1(pCross->nDet,pCross->nTry);` |
|       83 |  405 | `		return PH7_OP_CATCH_JMP;` |
|        - |  406 | `	}` |
|    41413 |  407 | `	if( pCross->nInline > 0 ){` |
|       10 |  408 | `		*piP1 = (sxi32)pCross->nInline;` |
|       10 |  409 | `		return PH7_OP_SET_FINALLY_JMP;` |
|        - |  410 | `	}` |
|    41405 |  411 | `	*piP1 = 0;` |
|    41405 |  412 | `	return PH7_OP_JMP;` |
|    20749 |  413 | `}` |
|        - |  414 | `/*` |
|        - |  415 | ` * Resolve a recorded fixup to its VM instruction, in the container it was emitted` |
|        - |  416 | ` * into (see JumpFixup.pContainer) rather than whichever one is current now.` |
|        - |  417 | ` */` |
|   711402 |  418 | `PH7_PRIVATE VmInstr * GenStateFixupInstr(const JumpFixup *pFix)` |
|        5 |  419 | `{` |
|   711407 |  420 | `	return (VmInstr *)SySetAt(pFix->pContainer,pFix->nInstrIdx);` |
|        5 |  421 | `}` |
|        - |  422 | `/*` |
|        - |  423 | ` * Fix a forward jump now the jump destination is resolved.` |
|        - |  424 | ` * Return the total number of fixed jumps.` |
|        - |  425 | ` * Notes on forward jumps:` |
|        - |  426 | ` *  Compilation of some PHP constructs such as if,for,while and the logical or` |
|        - |  427 | ` *  (\|\|) and logical and (&&) operators in expressions requires the` |
|        - |  428 | ` *  generation of forward jumps.` |
|        - |  429 | ` *  Since the destination PC target of these jumps isn't known when the jumps` |
|        - |  430 | ` *  are emitted, we record each forward jump in an instance of the following` |
|        - |  431 | ` *  structure.Those jumps are fixed later when the jump destination is resolved.` |
|        - |  432 | ` */` |
|  1001660 |  433 | `PH7_PRIVATE sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|        5 |  434 | `{` |
|        - |  435 | `	JumpFixup *aFix;` |
|        - |  436 | `	VmInstr *pInstr;` |
|        - |  437 | `	sxu32 nFixed;` |
|        - |  438 | `	sxu32 n;` |
|        - |  439 | `	/* Point to the jump fixup table */` |
|  1001665 |  440 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|        - |  441 | `	/* Fix the desired jumps */` |
|  2263167 |  442 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  1261507 |  443 | `		if( aFix[n].nJumpType < 0 ){` |
|        - |  444 | `			/* Already fixed */` |
|   441265 |  445 | `			continue;` |
|        - |  446 | `		}` |
|   820247 |  447 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|        - |  448 | `			/* Not of our interest */` |
|   124453 |  449 | `			continue;` |
|        - |  450 | `		}` |
|        - |  451 | `		/* Point to the instruction to fix */` |
|   695799 |  452 | `		pInstr = GenStateFixupInstr(&aFix[n]);` |
|   695799 |  453 | `		if( pInstr ){` |
|   695799 |  454 | `			pInstr->iP2 = nJumpDest;` |
|   695799 |  455 | `			nFixed++;` |
|        - |  456 | `			/* Mark as fixed */` |
|   695799 |  457 | `			aFix[n].nJumpType = -1;` |
|   347897 |  458 | `		}` |
|   347902 |  459 | `	}` |
|        - |  460 | `	/* Total number of fixed jumps */` |
|  1001665 |  461 | `	return nFixed;` |
|        5 |  462 | `}` |
|        - |  463 | `/*` |
|        - |  464 | ` * Fix a 'goto' now the jump destination is resolved.` |
|        - |  465 | ` * The goto statement can be used to jump to another section` |
|        - |  466 | ` * in the program.` |
|        - |  467 | ` * Refer to the routine responsible of compiling the goto` |
|        - |  468 | ` * statement for more information.` |
|        - |  469 | ` */` |
|   170194 |  470 | `PH7_PRIVATE sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|        5 |  471 | `{` |
|        - |  472 | `	JumpFixup *pJump,*aJumps;` |
|        - |  473 | `	GenJumpScope sCross;` |
|        - |  474 | `	Label *pLabel;` |
|        - |  475 | `	VmInstr *pInstr;` |
|        - |  476 | `	sxi32 rc;` |
|        - |  477 | `	sxu32 n;` |
|        - |  478 | `	/* Point to the goto table */` |
|   170199 |  479 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|        - |  480 | `	/* Fix */` |
|   170417 |  481 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
|      225 |  482 | `		pJump = &aJumps[n];` |
|        - |  483 | `		/* Extract the target label */` |
|        - |  484 | `		/* A label declared in ANOTHER function is not a destination: the lookup is keyed` |
|        - |  485 | `		 * on the goto's own function, so a same-named label elsewhere simply does not` |
|        - |  486 | `		 * answer and this reports php's undefined-label fatal. */` |
|      225 |  487 | `		rc = GenStateGetLabel(&(*pGen),&pJump->sLabel,pJump->pFunc,&pLabel);` |
|      225 |  488 | `		if( rc != SXRET_OK ){` |
|        - |  489 | `			/* No such label */` |
|       68 |  490 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"'goto' to undefined label '%z'",&pJump->sLabel);` |
|       68 |  491 | `			if( rc == SXERR_ABORT ){` |
|        3 |  492 | `				return SXERR_ABORT;` |
|        - |  493 | `			}` |
|       66 |  494 | `			continue;` |
|        - |  495 | `		}` |
|        - |  496 | `		/* php's one goto restriction: you may not jump INTO a loop or a switch. The label` |
|        - |  497 | `		 * is inside one exactly when it carries a loop id; that is legal only if the same` |
|        - |  498 | `		 * loop also encloses the goto, i.e. the label's loop is the goto's loop or one of` |
|        - |  499 | `		 * its ancestors. Walk up from the goto's loop looking for the label's. */` |
|      161 |  500 | `		if( pLabel->nLoopId != 0 ){` |
|        5 |  501 | `			sxu32 *aParent = (sxu32 *)SySetBasePtr(&pGen->aLoopParent);` |
|        5 |  502 | `			sxu32 nCur = pJump->nLoopId;` |
|        5 |  503 | `			int bInside = 0;` |
|        5 |  504 | `			while( nCur != 0 ){` |
|        5 |  505 | `				if( nCur == pLabel->nLoopId ){` |
|        5 |  506 | `					bInside = 1;` |
|        5 |  507 | `					break;` |
|        - |  508 | `				}` |
|      ! 0 |  509 | `				nCur = (nCur <= SySetUsed(&pGen->aLoopParent)) ? aParent[nCur - 1] : 0;` |
|      ! 0 |  510 | `			}` |
|        5 |  511 | `			if( !bInside ){` |
|      ! 0 |  512 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,` |
|        - |  513 | `					"'goto' into loop or switch statement is disallowed");` |
|      ! 0 |  514 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  515 | `					return SXERR_ABORT;` |
|        - |  516 | `				}` |
|      ! 0 |  517 | `				continue;` |
|        - |  518 | `			}` |
|        2 |  519 | `		}` |
|        - |  520 | `		/* What the jump crosses, and whether it is legal at all: the label's scope must` |
|        - |  521 | `		 * ENCLOSE the goto. Jumping INTO a try/catch/finally is fine in php (its handlers` |
|        - |  522 | `		 * are instruction RANGES, so landing anywhere in the body is being in the try),` |
|        - |  523 | `		 * but PHL pushes a handler at the try's OP_LOAD_EXCEPTION and runs a catch body` |
|        - |  524 | `		 * as a mini-program entered at its first instruction — there is no way to arrive` |
|        - |  525 | `		 * mid-body with the handler live. Say so rather than jump nowhere in silence,` |
|        - |  526 | `		 * skip a finally, or land in a foreign array. */` |
|      161 |  527 | `		if( !GenStateJumpScope(&(*pGen),pJump->nScopeId,pLabel->nScopeId,FALSE,&sCross) ){` |
|        6 |  528 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,` |
|        - |  529 | `				"'goto' into a try, catch or finally block is disallowed");` |
|        6 |  530 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  531 | `				return SXERR_ABORT;` |
|        - |  532 | `			}` |
|        6 |  533 | `			continue;` |
|        - |  534 | `		}` |
|      157 |  535 | `		if( sCross.nFinally > 0 ){` |
|        - |  536 | `			/* php's other structural rule, shared with break/continue. Tested AFTER the` |
|        - |  537 | `			 * reach test above, so a goto that both leaves a finally and lands somewhere` |
|        - |  538 | `			 * that does not enclose it reports the into-a-try wording instead of this` |
|        - |  539 | `			 * one. Both are fatal on the same line, and the two cannot be told apart` |
|        - |  540 | `			 * without a second walk outward from the LABEL — php accepts one of them` |
|        - |  541 | ``			 * (`finally { goto L; try { L: … } }`), which is the §7.2 divergence, and`` |
|        - |  542 | `			 * rejects the other. Not worth a second walk for a message on input that is` |
|        - |  543 | `			 * rejected either way. */` |
|        3 |  544 | `			if( GenStateJumpOutOfFinally(&(*pGen),pJump->nLine) == SXERR_ABORT ){` |
|      ! 0 |  545 | `				return SXERR_ABORT;` |
|        - |  546 | `			}` |
|        3 |  547 | `			continue;` |
|        - |  548 | `		}` |
|        - |  549 | `		/* Fix the jump now the destination is resolved — in the container the goto was` |
|        - |  550 | `		 * emitted into, which for a goto inside a catch/finally body is not the one` |
|        - |  551 | `		 * current here (gotos resolve at end of compilation, after every swap back). */` |
|      155 |  552 | `		pInstr = GenStateFixupInstr(pJump);` |
|      155 |  553 | `		if( pInstr ){` |
|      155 |  554 | `			pInstr->iP2 = pLabel->nJumpDest;` |
|      155 |  555 | `			if( pInstr->iOp == PH7_OP_CATCH_JMP ){` |
|        - |  556 | `				/* Emitted as a structure-crossing jump because the goto sits inside a` |
|        - |  557 | `				 * try or a detached body. Now that the crossing is known it may well` |
|        - |  558 | `				 * turn out to leave nothing, and downgrade to a plain OP_JMP. */` |
|       47 |  559 | `				sxi32 iP1 = 0;` |
|       47 |  560 | `				pInstr->iOp = (sxu8)GenStateScopeJumpOp(&sCross,&iP1);` |
|       47 |  561 | `				pInstr->iP1 = iP1;` |
|       22 |  562 | `			}` |
|       75 |  563 | `		}` |
|       80 |  564 | `	}` |
|        - |  565 | `	/* php says nothing about a label nobody jumps to — the old "defined but not` |
|        - |  566 | `	 * referenced" warning was a PH7-ism with no counterpart in the oracle. */` |
|   170197 |  567 | `	return SXRET_OK;` |
|    85102 |  568 | `}` |
|        - |  569 | `/*` |
|        - |  570 | ` * Check if a given token value is installed in the literal table.` |
|        - |  571 | ` */` |
|  1170978 |  572 | `PH7_PRIVATE sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|        5 |  573 | `{` |
|        - |  574 | `	SyHashEntry *pEntry;` |
|  1170983 |  575 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  1170983 |  576 | `	if( pEntry == 0 ){` |
|   578185 |  577 | `		return SXERR_NOTFOUND;` |
|        - |  578 | `	}` |
|   592803 |  579 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|   592803 |  580 | `	return SXRET_OK;` |
|   585494 |  581 | `}` |
|        - |  582 | `/*` |
|        - |  583 | ` * Install a given constant index in the literal table.` |
|        - |  584 | ` * In order to be installed, the ph7_value must be of type string.` |
|        - |  585 | ` *` |
|        - |  586 | ` * NOTE: empty strings are deliberately omitted here.  The VM reserves a` |
|        - |  587 | ` * single shared constant for "" during initialization (pVm->nEmptyStringIdx)` |
|        - |  588 | ` * and the compiler emits a LOADC referencing that slot whenever an empty` |
|        - |  589 | ` * literal is encountered.  This keeps the literal hash from growing when` |
|        - |  590 | ` * many "" literals appear in user code.` |
|        - |  591 | ` */` |
|   578180 |  592 | `PH7_PRIVATE sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|        5 |  593 | `{` |
|   578185 |  594 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|   578185 |  595 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   289090 |  596 | `	}` |
|   578185 |  597 | `	return SXRET_OK;` |
|        5 |  598 | `}` |
|        - |  599 | `/*` |
|        - |  600 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|        - |  601 | ` * in the constant table.` |
|        - |  602 | ` */` |
|   681284 |  603 | `PH7_PRIVATE ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|        5 |  604 | `{` |
|        - |  605 | `	ph7_value *pObj;` |
|   681289 |  606 | `	sxu32 nIdx = 0; /* cc warning */` |
|        - |  607 | `	/* Reserve a new constant */` |
|   681289 |  608 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   681289 |  609 | `	if( pObj == 0 ){` |
|      ! 0 |  610 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  611 | `		return 0;` |
|        - |  612 | `	}` |
|   681289 |  613 | `	*pIdx = nIdx;` |
|        - |  614 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|        - |  615 | `	 * the constant string iterals table [optimization purposes].` |
|        - |  616 | `	 */` |
|   681289 |  617 | `	return pObj;` |
|   340647 |  618 | `}` |
|        - |  619 | `/*` |
|        - |  620 | ` * Implementation of the PHP language constructs.` |
|        - |  621 | ` */` |
|        - |  622 | `/*` |
|        - |  623 | ` * Ensure the about-to-be-emitted CALL/NEW opcode carries a VmCallArgMap` |
|        - |  624 | ` * that reflects the caller file's strict_types mode. Returns the (possibly` |
|        - |  625 | ` * newly allocated and zero-initialized) map pointer. In weak-mode files` |
|        - |  626 | ` * this is a no-op and the caller's p3 is returned unchanged.` |
|        - |  627 | ` *` |
|        - |  628 | ` * NOTE: on allocation failure the call reverts to weak semantics rather` |
|        - |  629 | ` * than aborting compilation — out-of-memory during a map allocation is` |
|        - |  630 | ` * vanishingly unlikely and silently dropping to weak mode matches the` |
|        - |  631 | ` * surrounding callsites' zero-check fallback pattern.` |
|        - |  632 | ` */` |
|   757308 |  633 | `PH7_PRIVATE void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|        5 |  634 | `{` |
|        - |  635 | `	VmCallArgMap *pMap;` |
|   757313 |  636 | `	if( !pGen->bStrictTypes ) return p3;` |
|      318 |  637 | `	if( p3 == 0 ){` |
|       44 |  638 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|       44 |  639 | `		if( pMap == 0 ) return 0;` |
|       44 |  640 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|       44 |  641 | `		p3 = (void *)pMap;` |
|       20 |  642 | `	}` |
|      318 |  643 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|      318 |  644 | `	return p3;` |
|   378659 |  645 | `}` |
|        - |  646 | `/* Forward declaration */` |
|        - |  647 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut);` |
|        - |  648 | `/* Forward declarations */` |
|        - |  649 | `/*` |
|        - |  650 | ` * Recover from a compile-time error. In other words synchronize` |
|        - |  651 | ` * the token stream cursor with the first semi-colon seen.` |
|        - |  652 | ` */` |
|        8 |  653 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|        1 |  654 | `{` |
|        - |  655 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|       17 |  656 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|        9 |  657 | `		pGen->pIn++;` |
|        1 |  658 | `	}` |
|        9 |  659 | `	return SXRET_OK;` |
|        1 |  660 | `}` |
|        - |  661 | `/*` |
|        - |  662 | ` * Check if the given identifier name is reserved or not.` |
|        - |  663 | ` * Return TRUE if reserved.FALSE otherwise.` |
|        - |  664 | ` */` |
|      176 |  665 | `PH7_PRIVATE int GenStateIsReservedConstant(SyString *pName)` |
|        5 |  666 | `{` |
|      181 |  667 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|       18 |  668 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|        3 |  669 | `			return TRUE;` |
|       16 |  670 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|        6 |  671 | `			return TRUE;` |
|        3 |  672 | `		}` |
|      171 |  673 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       15 |  674 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|        3 |  675 | `			return TRUE;` |
|        - |  676 | `		}` |
|        5 |  677 | `	}` |
|        - |  678 | `	/* Not a reserved constant */` |
|      173 |  679 | `	return FALSE;` |
|       93 |  680 | `}` |
|        - |  681 | `/*` |
|        - |  682 | ` * Chain operators participate in a postfix member-access chain.` |
|        - |  683 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|        - |  684 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|        - |  685 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|        - |  686 | ` */` |
|        - |  687 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|        - |  688 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|        - |  689 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|        - |  690 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|        - |  691 |  |
|        - |  692 | `/*` |
|        - |  693 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|        - |  694 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|        - |  695 | ` * patched entries from the pending set.` |
|        - |  696 | ` */` |
|  6870844 |  697 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|        5 |  698 | `{` |
|  6870849 |  699 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|        - |  700 | `	sxu32 nTarget;` |
|        - |  701 | `	sxu32 *aIdx;` |
|        - |  702 | `	sxu32 i;` |
|  6870849 |  703 | `	if( nCur <= nBaseline ){` |
|  6870729 |  704 | `		return;` |
|        - |  705 | `	}` |
|      125 |  706 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|      125 |  707 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|      253 |  708 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|      133 |  709 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|      133 |  710 | `		if( pInstr ){` |
|      133 |  711 | `			pInstr->iP2 = (sxi32)nTarget;` |
|       64 |  712 | `		}` |
|       69 |  713 | `	}` |
|      125 |  714 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
|  3435427 |  715 | `}` |
|        - |  716 |  |
|        - |  717 | `/*` |
|        - |  718 | ` * By-reference out-parameters of builtin functions.` |
|        - |  719 | ` *` |
|        - |  720 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|        - |  721 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|        - |  722 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|        - |  723 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|        - |  724 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|        - |  725 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|        - |  726 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|        - |  727 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|        - |  728 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|        - |  729 | ` * creates it" behaviour).` |
|        - |  730 | ` *` |
|        - |  731 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|        - |  732 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|        - |  733 | ` */` |
|   672178 |  734 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|        5 |  735 | `{` |
|        - |  736 | `	static const struct {` |
|        - |  737 | `		const char *zName;` |
|        - |  738 | `		sxu32 nByte;` |
|        - |  739 | `		sxu32 mask;` |
|        - |  740 | `	} aByRef[] = {` |
|        - |  741 | `		{ "parse_str",              9, 1u<<1 },  /* &$result (apArg[1]) */` |
|        - |  742 | `		{ "settype",                7, 1u<<0 },  /* &$var    (apArg[0]) */` |
|        - |  743 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|        - |  744 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|        - |  745 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|        - |  746 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|        - |  747 | `		{ "flock",                  5, 1u<<2 },  /* &$would_block (apArg[2]) */` |
|        - |  748 | `		{ "getopt",                 6, 1u<<2 },  /* &$rest_index (apArg[2]) */` |
|        - |  749 | `		{ "is_callable",           11, 1u<<2 },  /* &$callable_name (apArg[2]) */` |
|        - |  750 | `		{ "similar_text",          12, 1u<<2 },  /* &$percent (apArg[2]) */` |
|        - |  751 | `		{ "str_replace",           11, 1u<<3 },  /* &$count  (apArg[3]) */` |
|        - |  752 | `		{ "str_ireplace",          12, 1u<<3 },  /* &$count  (apArg[3]) */` |
|        - |  753 | `		{ "fsockopen",              9, (1u<<2)\|(1u<<3) },  /* &$error_code, &$error_message */` |
|        - |  754 | `		{ "pfsockopen",            10, (1u<<2)\|(1u<<3) },  /* same */` |
|        - |  755 | `		{ "stream_socket_client",  20, (1u<<1)\|(1u<<2) },  /* &$error_code, &$error_message */` |
|        - |  756 | `		{ "stream_socket_server",  20, (1u<<1)\|(1u<<2) },  /* same pair */` |
|        - |  757 | `		{ "stream_socket_accept",  20, 1u<<2 },            /* &$peer_name (apArg[2]) */` |
|        - |  758 | `		{ "stream_select",         13, (1u<<0)\|(1u<<1)\|(1u<<2) }, /* &$read, &$write, &$except */` |
|        - |  759 | `		{ "stream_socket_recvfrom",22, 1u<<3 },            /* &$address (apArg[3]) */` |
|        - |  760 | `		{ "proc_open",              9, 1u<<2 },  /* &$pipes (apArg[2]) */` |
|        - |  761 | `		{ "exec",                   4, (1u<<1)\|(1u<<2) },  /* &$output, &$result_code */` |
|        - |  762 | `		{ "system",                 6, 1u<<1 },  /* &$result_code (apArg[1]) */` |
|        - |  763 | `		{ "passthru",               8, 1u<<1 },  /* &$result_code (apArg[1]) */` |
|        - |  764 | `	};` |
|        - |  765 | `	sxu32 i;` |
|   672183 |  766 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|    28273 |  767 | `		return 0;` |
|        - |  768 | `	}` |
| 15038549 |  769 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
| 14416524 |  770 | `		if( pName->nByte == aByRef[i].nByte` |
|  7697748 |  771 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|    21895 |  772 | `			return aByRef[i].mask;` |
|        - |  773 | `		}` |
|  7197322 |  774 | `	}` |
|   622025 |  775 | `	return 0;` |
|   336094 |  776 | `}` |
|        - |  777 | `/*` |
|        - |  778 | ` * What may be passed by REFERENCE is decided from the argument's SHAPE, at compile` |
|        - |  779 | ` * time, exactly as php decides it (zend_compile_args -> zend_is_variable).` |
|        - |  780 | ` *` |
|        - |  781 | ` * php sorts every actual argument into three buckets:` |
|        - |  782 | ` *` |
|        - |  783 | ` *   GEN_ARG_LVALUE   a variable, an element, a property, a static property. It has a` |
|        - |  784 | ` *                    slot, so a by-ref parameter aliases it.` |
|        - |  785 | `` *   GEN_ARG_TEMPCALL the result of a call or of `new`. It has no slot, but php cannot`` |
|        - |  786 | ` *                    know at compile time whether the callee returns a reference, so it` |
|        - |  787 | ` *                    defers: E_NOTICE "Only variables should be passed by reference",` |
|        - |  788 | ` *                    then it operates on the temporary.` |
|        - |  789 | ` *   GEN_ARG_NONE     everything else — a literal, an operator/cast result, a class` |
|        - |  790 | `` *                    constant, `@$x`, `$o?->p`, an assignment. Binding one to a by-ref`` |
|        - |  791 | ` *                    parameter is a catchable Error at the CALL.` |
|        - |  792 | ` *` |
|        - |  793 | ` * Deciding it from the argument's runtime memobj instead does not work and was silently` |
|        - |  794 | ` * wrong in both directions: an arithmetic or concatenation result keeps its LEFT operand's` |
|        - |  795 | `` * slot index, so `f($i + 1)` with `function f(&$x)` aliased and overwrote `$i`; and a`` |
|        - |  796 | ` * builtin's by-ref row saw only "no slot", which a call result has too.` |
|        - |  797 | ` */` |
|        - |  798 | `#define GEN_ARG_LVALUE   0` |
|        - |  799 | `#define GEN_ARG_TEMPCALL 1` |
|        - |  800 | `#define GEN_ARG_NONE     2` |
|   907428 |  801 | `static int GenStateArgShape(ph7_expr_node *pNode)` |
|        5 |  802 | `{` |
|   907433 |  803 | `	if( pNode == 0 ){` |
|      ! 0 |  804 | `		return GEN_ARG_NONE;` |
|        - |  805 | `	}` |
|   907433 |  806 | `	if( pNode->pOp == 0 ){` |
|        - |  807 | ``		/* A leaf: only the `$…` family is a variable. Everything else the parser`` |
|        - |  808 | ``		 * files here — a literal, an array/list constructor, a closure, `match`,`` |
|        - |  809 | ``		 * `clone` — is a temporary. */`` |
|   758595 |  810 | `		return pNode->xCode == PH7_CompileVariable ? GEN_ARG_LVALUE : GEN_ARG_NONE;` |
|        - |  811 | `	}` |
|   148843 |  812 | `	switch( pNode->pOp->iOp ){` |
|    11279 |  813 | `	case EXPR_OP_SUBSCRIPT: /* $a[k], and any base: php accepts g()[0] and C::m()[0] */` |
|        - |  814 | `	case EXPR_OP_ARROW:     /* $o->p */` |
|    22563 |  815 | `		return GEN_ARG_LVALUE;` |
|      163 |  816 | `	case EXPR_OP_DC:` |
|        - |  817 | ``		/* `C::$s` is a static property (an lvalue); `C::K` is a class constant and`` |
|        - |  818 | ``		 * `C::CASE` an enum case, neither of which php will bind. The right operand`` |
|        - |  819 | `		 * tells them apart. */` |
|      494 |  820 | `		return ( pNode->pRight && pNode->pRight->pOp == 0` |
|      326 |  821 | `		      && pNode->pRight->xCode == PH7_CompileVariable )` |
|      326 |  822 | `			? GEN_ARG_LVALUE : GEN_ARG_NONE;` |
|    20197 |  823 | `	case EXPR_OP_FUNC_CALL:` |
|        - |  824 | `	case EXPR_OP_NEW:` |
|    40399 |  825 | `		return GEN_ARG_TEMPCALL;` |
|    42780 |  826 | `	default:` |
|        - |  827 | ``		/* Includes `?->` (php: "Cannot use nullsafe operator in write context"),`` |
|        - |  828 | ``		 * `@$x`, `$q = …`, `clone $o` and every arithmetic/logical operator. */`` |
|    85565 |  829 | `		return GEN_ARG_NONE;` |
|        - |  830 | `	}` |
|   453719 |  831 | `}` |
|        - |  832 | `/*` |
|        - |  833 | ` * Recover the bare global-builtin name from a call's callee node.` |
|        - |  834 | ` *` |
|        - |  835 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|        - |  836 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|        - |  837 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|        - |  838 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|        - |  839 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|        - |  840 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|        - |  841 | ` */` |
|  1308880 |  842 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|        5 |  843 | `{` |
|        - |  844 | `	SyToken *p, *pEnd;` |
|  1308885 |  845 | `	pOut->zString = 0;` |
|  1308885 |  846 | `	pOut->nByte = 0;` |
|  1308885 |  847 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|      ! 0 |  848 | `		return;` |
|        - |  849 | `	}` |
|  1308885 |  850 | `	p = pLeft->pStart;` |
|  1308885 |  851 | `	pEnd = pLeft->pEnd;` |
|        - |  852 | `	/* Optional single leading namespace separator (absolute path). */` |
|  1308885 |  853 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|      100 |  854 | `		p++;` |
|       48 |  855 | `	}` |
|  1308885 |  856 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|    32147 |  857 | `		return;` |
|        - |  858 | `	}` |
|        - |  859 | `	/* Must be a single component: nothing follows the name token. */` |
|  1276743 |  860 | `	if( p + 1 != pEnd ){` |
|      121 |  861 | `		return;` |
|        - |  862 | `	}` |
|  1276627 |  863 | `	*pOut = p->sData;` |
|   654445 |  864 | `}` |
|        - |  865 | `/*` |
|        - |  866 | `` * Is this expression node the bare variable `$this`?`` |
|        - |  867 | ` */` |
|   828128 |  868 | `PH7_PRIVATE int PH7_ExprNodeIsThis(ph7_expr_node *pNode)` |
|        5 |  869 | `{` |
|        - |  870 | `	SyToken *pTok;` |
|   828133 |  871 | `	if( pNode == 0 \|\| pNode->pOp != 0 \|\| pNode->xCode != PH7_CompileVariable ){` |
|   122287 |  872 | `		return 0;` |
|        - |  873 | `	}` |
|   705851 |  874 | `	pTok = pNode->pStart;` |
|   705851 |  875 | `	if( pTok == 0 \|\| pNode->pEnd == 0 \|\| pNode->pEnd < &pTok[2] ){` |
|      ! 0 |  876 | `		return 0;` |
|        - |  877 | `	}` |
|  1058774 |  878 | `	return (pTok[0].nType & PH7_TK_DOLLAR) != 0` |
|   705846 |  879 | `		&& (pTok[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|   705835 |  880 | `		&& pTok[1].sData.nByte == sizeof("this")-1` |
|  1058769 |  881 | `		&& SyMemcmp((const void *)pTok[1].sData.zString,(const void *)"this",sizeof("this")-1) == 0;` |
|   414069 |  882 | `}` |
|        - |  883 | `/*` |
|        - |  884 | ` * The two write-target rules php decides at COMPILE time, in one place because` |
|        - |  885 | ` * every write site has to make both of them.` |
|        - |  886 | ` *` |
|        - |  887 | `` * **`$this`** is not a variable a program may re-point: php refuses the`` |
|        - |  888 | `` * assignment, the reference bind, a foreach/list target and `unset()` where they`` |
|        - |  889 | `` * are WRITTEN. PHL performed all of them, so `$this = 5;` inside a method`` |
|        - |  890 | ` * replaced the receiver with an int for the rest of the call and every later` |
|        - |  891 | `` * `$this->x` failed somewhere else entirely.`` |
|        - |  892 | ` *` |
|        - |  893 | ``  * **A temporary** cannot be written THROUGH: `(new A)->p = 1` and `"s"->p = 1` `` |
|        - |  894 | ` * modify an object/value that no longer exists after the statement, so php` |
|        - |  895 | `` * refuses the whole chain — every write kind, including `+=`, `++`, `=&` and`` |
|        - |  896 | `` * `unset()`. The base of the access chain decides: a variable and a userland`` |
|        - |  897 | `` * CALL are writable (`f()->p = 1` is php-legal), a `new`, a literal and any`` |
|        - |  898 | ` * other computed value are not, and an internal function's result gets php's own` |
|        - |  899 | `` * separate wording — which is what `(clone $o)->p = 1` is, `clone` being a`` |
|        - |  900 | ` * function in php 8.5.` |
|        - |  901 | ` */` |
|   828128 |  902 | `PH7_PRIVATE sxi32 GenStateWriteTargetCheck(ph7_gen_state *pGen,ph7_expr_node *pTarget,int bUnset)` |
|        5 |  903 | `{` |
|   828133 |  904 | `	ph7_expr_node *pBase = pTarget;` |
|   828133 |  905 | `	const char *zMsg = 0;` |
|        - |  906 | `	sxi32 rc;` |
|   828133 |  907 | `	if( pTarget == 0 ){` |
|      ! 0 |  908 | `		return SXRET_OK;` |
|        - |  909 | `	}` |
|   828133 |  910 | `	if( PH7_ExprNodeIsThis(pTarget) ){` |
|       11 |  911 | `		zMsg = bUnset ? "Cannot unset $this" : "Cannot re-assign $this";` |
|        7 |  912 | `	}else{` |
|        - |  913 | `		/* Walk to the base of the access chain; the links themselves are writable. */` |
|   950427 |  914 | `		while( pBase && pBase->pOp ){` |
|   122485 |  915 | `			if( pBase->pOp->iOp == EXPR_OP_DC ){` |
|        - |  916 | `` 				/* A `::` left operand is a CLASS reference, not a value — `C::$s = 1` `` |
|        - |  917 | ``				 * and even `(new C)::$s = 1` write class-level storage that outlives`` |
|        - |  918 | `				 * any temporary, so the chain stops being about a base here. */` |
|      165 |  919 | `				return SXRET_OK;` |
|        - |  920 | `			}` |
|   122320 |  921 | `			if( pBase->pOp->iOp != EXPR_OP_ARROW && pBase->pOp->iOp != EXPR_OP_NULLSAFE_ARROW` |
|   120664 |  922 | `			 && pBase->pOp->iOp != EXPR_OP_SUBSCRIPT ){` |
|       22 |  923 | `				break;` |
|        - |  924 | `			}` |
|   122307 |  925 | `			pBase = pBase->pLeft;` |
|        5 |  926 | `		}` |
|   827965 |  927 | `		if( pBase == 0 \|\| pBase == pTarget ){` |
|        - |  928 | `			/* No chain: a non-variable target of its own is the caller's business` |
|        - |  929 | `			 * (php reports its parse error / "Assignments can only happen to` |
|        - |  930 | `			 * writable values" there, and so does PHL). */` |
|   706031 |  931 | `			return SXRET_OK;` |
|        - |  932 | `		}` |
|   121939 |  933 | `		if( pBase->pOp == 0 ){` |
|   121931 |  934 | `			if( pBase->xCode != PH7_CompileVariable ){` |
|      ! 0 |  935 | `				zMsg = "Cannot use temporary expression in write context";` |
|        5 |  936 | `			}` |
|    60974 |  937 | `		}else if( pBase->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|        - |  938 | `			/* php: the result of an INTERNAL function is not writable through,` |
|        - |  939 | `			 * a userland one is. */` |
|        - |  940 | `			SyString sName;` |
|        5 |  941 | `			GenStateCallBuiltinName(pBase,&sName);` |
|        4 |  942 | `			if( sName.nByte > 0 && pGen->pVm` |
|        1 |  943 | `			 && SyHashGet(&pGen->pVm->hHostFunction,(const void *)sName.zString,sName.nByte) ){` |
|      ! 0 |  944 | `				zMsg = "Cannot use result of built-in function in write context";` |
|        1 |  945 | `			}` |
|        8 |  946 | `		}else if( pBase->pOp->iOp == EXPR_OP_CLONE ){` |
|        - |  947 | ``			/* php 8.5 implements `clone` AS a function, so a write through its result`` |
|        - |  948 | `			 * takes the internal-function wording rather than the temporary one. */` |
|        3 |  949 | `			zMsg = "Cannot use result of built-in function in write context";` |
|        2 |  950 | `		}else{` |
|        - |  951 | ``			/* `new`, and every other computed base. */`` |
|        3 |  952 | `			zMsg = "Cannot use temporary expression in write context";` |
|        - |  953 | `		}` |
|        - |  954 | `	}` |
|   121947 |  955 | `	if( zMsg == 0 ){` |
|   121935 |  956 | `		return SXRET_OK;` |
|        - |  957 | `	}` |
|       16 |  958 | `	rc = PH7_GenCompileError(&(*pGen),E_ERROR,` |
|       12 |  959 | `		pTarget->pStart ? pTarget->pStart->nLine : 0,"%s",zMsg);` |
|       16 |  960 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_INVALID;` |
|   414069 |  961 | `}` |
|        - |  962 | `/*` |
|        - |  963 | ` * What emitting a call's ARGUMENT LIST decided, handed back to the CALL codegen.` |
|        - |  964 | ` * The arguments are emitted from their own routine because php evaluates them` |
|        - |  965 | ` * AFTER the callee has been resolved, so this runs between the callee's emission` |
|        - |  966 | ` * and the OP_CALL — see GenStateEmitCallArgs.` |
|        - |  967 | ` */` |
|        - |  968 | `typedef struct GenCallArgs GenCallArgs;` |
|        - |  969 | `struct GenCallArgs {` |
|        - |  970 | `	sxi32 iP1;      /* OP_CALL.iP1: the compile-time argument count */` |
|        - |  971 | ``	sxu32 iP2;      /* OP_CALL.iP2: 1 if any argument unpacks (`...$a`) */`` |
|        - |  972 | `	void *p3;       /* OP_CALL.p3: the VmCallArgMap, which may ALREADY carry the callee's` |
|        - |  973 | `	                 * namespace qualification — the callee is emitted first now */` |
|        - |  974 | ``	int bFcc;       /* First-class callable `f(...)`: no arguments, OP_LOAD_FCC follows */`` |
|        - |  975 | ``	int bAnySpread; /* Any `...` argument (iP2 says the same; kept for the shape masks) */`` |
|        - |  976 | `};` |
|        - |  977 | `static sxi32 GenStateEmitCallArgs(ph7_gen_state *pGen,ph7_expr_node *pNode,sxi32 iFlags,` |
|        - |  978 | `	GenCallArgs *pArgs);` |
|        - |  979 | `/*` |
|        - |  980 | ` * Generate bytecode for a given expression tree.` |
|        - |  981 | ` * If something goes wrong while generating bytecode` |
|        - |  982 | ` * for the expression tree (A very unlikely scenario)` |
|        - |  983 | ` * this function takes care of generating the appropriate` |
|        - |  984 | ` * error message.` |
|        - |  985 | ` */` |
|  7814330 |  986 | `static sxi32 GenStateEmitExprCode(` |
|        - |  987 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - |  988 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|        - |  989 | `	sxi32 iFlags /* Control flags */` |
|        - |  990 | `	)` |
|        5 |  991 | `{` |
|        - |  992 | `	VmInstr *pInstr;` |
|        - |  993 | `	sxu32 nJmpIdx;` |
|  7814335 |  994 | `	sxi32 iP1 = 0;` |
|  7814335 |  995 | `	sxu32 iP2 = 0;` |
|  7814335 |  996 | `	void *p3  = 0;` |
|        - |  997 | `	sxi32 iVmOp;` |
|        - |  998 | `	sxi32 rc;` |
|  7814335 |  999 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
|  7814335 | 1000 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
|  7814335 | 1001 | `	sxu32 nRhsNsBase = 0;` |
|        - | 1002 | ``	/* Consumed here so it describes THIS node only — the direct operand of a `new` —`` |
|        - | 1003 | `	 * and never travels down into the operand's own sub-expressions. */` |
|  7814335 | 1004 | `	int bNewCallee = (iFlags & EXPR_FLAG_NEW_CALLEE) != 0;` |
|  7814335 | 1005 | `	iFlags &= ~EXPR_FLAG_NEW_CALLEE;` |
|  7814335 | 1006 | `	if( pNode->xCode ){` |
|        - | 1007 | `		SyToken *pTmpIn,*pTmpEnd;` |
|        - | 1008 | `		/* Compile node */` |
|  4825425 | 1009 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
|  4825425 | 1010 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
|  4825425 | 1011 | `		RE_SWAP_DELIMITER(pGen);` |
|  4825425 | 1012 | `		return rc;` |
|        - | 1013 | `	}` |
|  2988915 | 1014 | `	if( pNode->pOp == 0 ){` |
|      ! 0 | 1015 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|        - | 1016 | `			"Invalid expression node,PH7 is aborting compilation");` |
|      ! 0 | 1017 | `		return SXERR_ABORT;` |
|        - | 1018 | `	}` |
|  2988915 | 1019 | `	iVmOp = pNode->pOp->iVmOp;` |
|  2988915 | 1020 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|        - | 1021 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|        - | 1022 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|        - | 1023 | `		 * and later errors are still reported. */` |
|        3 | 1024 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|        - | 1025 | `			"The (unset) cast is no longer supported");` |
|        3 | 1026 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1027 | `			return SXERR_ABORT;` |
|        - | 1028 | `		}` |
|        1 | 1029 | `	}` |
|  2988915 | 1030 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|      157 | 1031 | `		sxu32 nJmp = 0;` |
|        - | 1032 | `		sxu32 nNcNsBase;` |
|        - | 1033 | `		VmInstr *pInstrFix;` |
|        - | 1034 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|        - | 1035 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|        - | 1036 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|        - | 1037 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|        - | 1038 | `		 * stack slot carries a writable nIdx. */` |
|      157 | 1039 | `		if( pNode->pRight ){` |
|        - | 1040 | ``			/* `$a[] ??= v` READS its target before deciding to write, so php refuses the`` |
|        - | 1041 | ``			 * append form anywhere in that target's chain (`$a[][0] ??= v` too), even`` |
|        - | 1042 | ``			 * though the same tag makes every other `[]` on this path a legal write`` |
|        - | 1043 | ``			 * target. Only the container chain is walked — a `[]` inside an INDEX`` |
|        - | 1044 | `			 * expression is an ordinary read and the subscript codegen refuses it. */` |
|      157 | 1045 | `			ph7_expr_node *pTgt = pNode->pRight;` |
|      352 | 1046 | `			while( pTgt && pTgt->pOp && (pTgt->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|       84 | 1047 | `			      \|\| pTgt->pOp->iOp == EXPR_OP_ARROW \|\| pTgt->pOp->iOp == EXPR_OP_DC) ){` |
|      133 | 1048 | `				if( pTgt->pOp->iOp == EXPR_OP_SUBSCRIPT && SySetUsed(&pTgt->aNodeArgs) < 1 ){` |
|      ! 0 | 1049 | `					break;` |
|        - | 1050 | `				}` |
|      133 | 1051 | `				pTgt = pTgt->pLeft;` |
|        3 | 1052 | `			}` |
|      154 | 1053 | `			if( pTgt && pTgt->pOp && pTgt->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|        3 | 1054 | `			 && SySetUsed(&pTgt->aNodeArgs) < 1 ){` |
|      ! 0 | 1055 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,` |
|      ! 0 | 1056 | `					pNode->pRight->pStart ? pNode->pRight->pStart->nLine : 0,` |
|        - | 1057 | `					"Cannot use [] for reading");` |
|      ! 0 | 1058 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1059 | `			}` |
|      157 | 1060 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      157 | 1061 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|      157 | 1062 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1063 | `				return rc;` |
|        - | 1064 | `			}` |
|      157 | 1065 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|        - | 1066 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|        - | 1067 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|        - | 1068 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|        - | 1069 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|        - | 1070 | `			 * the store, so the parent array does not need to be copied at` |
|        - | 1071 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|        - | 1072 | `			 * cascade for the actual write path stays correct. */` |
|      157 | 1073 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|      157 | 1074 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|       89 | 1075 | `				pInstrFix->iP2 = 3;` |
|       43 | 1076 | `			}` |
|       77 | 1077 | `		}` |
|        - | 1078 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|      157 | 1079 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|        - | 1080 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|      157 | 1081 | `		if( pNode->pLeft ){` |
|      157 | 1082 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      157 | 1083 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|      157 | 1084 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1085 | `				return rc;` |
|        - | 1086 | `			}` |
|      157 | 1087 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|       77 | 1088 | `		}` |
|        - | 1089 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|      157 | 1090 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|        - | 1091 | `		/* Patch the short-circuit jump to land after the store. */` |
|      157 | 1092 | `		if( nJmp > 0 ){` |
|      157 | 1093 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|      157 | 1094 | `			if( pInstrFix ){` |
|      157 | 1095 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|       77 | 1096 | `			}` |
|       77 | 1097 | `		}` |
|      157 | 1098 | `		return SXRET_OK;` |
|        - | 1099 | `	}` |
|  2988761 | 1100 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|        - | 1101 | `		sxu32 nJz,nJmp;` |
|        - | 1102 | `		sxu32 nTernaryNsBase;` |
|        - | 1103 | `		/* Ternary operator require special handling */` |
|        - | 1104 | `		/* Phase#1: Compile the condition */` |
|    34465 | 1105 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    34465 | 1106 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    34465 | 1107 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1108 | `			return rc;` |
|        - | 1109 | `		}` |
|        - | 1110 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|        - | 1111 | `		 * compiling the condition must short-circuit to the end of the` |
|        - | 1112 | `		 * condition expression, not leak past the ternary. */` |
|    34465 | 1113 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    34465 | 1114 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    34465 | 1115 | `		if( pNode->pLeft ){` |
|        - | 1116 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|        - | 1117 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    34387 | 1118 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|        - | 1119 | `			/* Phase#3: Compile the 'then' expression  */` |
|    34387 | 1120 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    34387 | 1121 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    34387 | 1122 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1123 | `				return rc;` |
|        - | 1124 | `			}` |
|    34387 | 1125 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    17196 | 1126 | `		}else{` |
|        - | 1127 | `			/* Elvis operator: (expr) ?: (else)` |
|        - | 1128 | `			 * Duplicate condition so original value is the 'then' result.` |
|        - | 1129 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|       82 | 1130 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       82 | 1131 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|        - | 1132 | `		}` |
|        - | 1133 | `		/* Phase#4: Emit the unconditional jump */` |
|    34465 | 1134 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|        - | 1135 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    34465 | 1136 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    34465 | 1137 | `		if( pInstr ){` |
|    34465 | 1138 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    17230 | 1139 | `		}` |
|    34465 | 1140 | `		if( !pNode->pLeft ){` |
|        - | 1141 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|       82 | 1142 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       39 | 1143 | `		}` |
|        - | 1144 | `		/* Phase#6: Compile the 'else' expression */` |
|    34465 | 1145 | `		if( pNode->pRight ){` |
|    34465 | 1146 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    34465 | 1147 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    34465 | 1148 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1149 | `				return rc;` |
|        - | 1150 | `			}` |
|    34465 | 1151 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    17230 | 1152 | `		}` |
|    34465 | 1153 | `		if( nJmp > 0 ){` |
|        - | 1154 | `			/* Phase#7: Fix the unconditional jump */` |
|    34465 | 1155 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    34465 | 1156 | `			if( pInstr ){` |
|    34465 | 1157 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    17230 | 1158 | `			}` |
|    17230 | 1159 | `		}` |
|        - | 1160 | `		/* All done */` |
|    34465 | 1161 | `		return SXRET_OK;` |
|        - | 1162 | `	}` |
|  2954301 | 1163 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
|        - | 1164 | ``		/* PHP 8.5 pipe: `$lhs \|> $rhs` invokes the RHS callable with the LHS`` |
|        - | 1165 | ``		 * value as its sole argument [i.e. `$rhs($lhs)`]. Evaluate the LHS (the`` |
|        - | 1166 | `		 * argument) first, then the RHS callable, then emit a one-argument` |
|        - | 1167 | `		 * OP_CALL — the same stack shape the function-call path builds (the` |
|        - | 1168 | `		 * argument sits below the callee). The RHS is any callable expression:` |
|        - | 1169 | ``		 * an FCC `f(...)` (an OP_LOAD_FCC Closure), a closure variable, an`` |
|        - | 1170 | ``		 * `[obj,method]` pair, or a callable string. */`` |
|        - | 1171 | `		sxu32 nPipeNsBase;` |
|       27 | 1172 | `		sxi32 iOperandFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE\|EXPR_FLAG_RDONLY_LOAD);` |
|       27 | 1173 | `		if( pNode->pLeft == 0 \|\| pNode->pRight == 0 ){` |
|      ! 0 | 1174 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|        - | 1175 | `				"'\|>': Missing operand");` |
|      ! 0 | 1176 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1177 | `		}` |
|        - | 1178 | `		/* Argument: the LHS value. */` |
|       27 | 1179 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       27 | 1180 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iOperandFlags);` |
|       27 | 1181 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1182 | `			return rc;` |
|        - | 1183 | `		}` |
|       27 | 1184 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|        - | 1185 | `		/* Callable: the RHS. */` |
|       27 | 1186 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       27 | 1187 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iOperandFlags);` |
|       27 | 1188 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1189 | `			return rc;` |
|        - | 1190 | `		}` |
|       27 | 1191 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|        - | 1192 | `		/* Invoke the callable with the single piped argument. */` |
|       27 | 1193 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       27 | 1194 | `		return SXRET_OK;` |
|        - | 1195 | `	}` |
|  2954275 | 1196 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|        - | 1197 | `	/* Generate code for the left tree */` |
|  2954275 | 1198 | `	if( pNode->pLeft ){` |
|  2954251 | 1199 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        - | 1200 | `		GenCallArgs sArgs;` |
|  2954251 | 1201 | `		int bArgsEmitted = 0;` |
|  2954251 | 1202 | ``		sxu32 nNewClassInstr = 0; /* index+1 of a `new` operand's class-name push */`` |
|  2954251 | 1203 | `		SyZero(&sArgs,sizeof(sArgs));` |
|        - | 1204 | `		{` |
|        - | 1205 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|        - | 1206 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|        - | 1207 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|        - | 1208 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|        - | 1209 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|        - | 1210 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|        - | 1211 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|        - | 1212 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
|  2954251 | 1213 | `			sxi32 iLeftFlags = iFlags;` |
|  2954251 | 1214 | `			sxu32 nNullcLhsFirst = PH7_VmInstrLength(pGen->pVm);` |
|  2954251 | 1215 | `			int bNullcLhs = 0;` |
|        - | 1216 | `			/* D1 commit 2: a deferred element/property call arg records its lvalue chain, but` |
|        - | 1217 | `			 * that chain must be CONTIGUOUS. Only propagate DEFER_ARG to the base when the base` |
|        - | 1218 | `			 * is itself a continuable lvalue — a plain variable (an undefined base auto-defers),` |
|        - | 1219 | ``			 * another subscript, or a `->` member. If the base is anything else (most importantly`` |
|        - | 1220 | ``			 * a method CALL, e.g. `$o->items()->prop` or `$r->attributes->item(0)->nodeName`),`` |
|        - | 1221 | `			 * strip DEFER so that intermediate read is a NORMAL read, not a record-mode carrier. */` |
|  2954251 | 1222 | `			if( iLeftFlags & EXPR_FLAG_DEFER_ARG ){` |
|    23569 | 1223 | `				int bContinuable = pNode->pLeft` |
|    17802 | 1224 | `					&& ( (pNode->pLeft->pOp == 0 && pNode->pLeft->xCode == PH7_CompileVariable)` |
|     6138 | 1225 | `					  \|\| (pNode->pLeft->pOp != 0 && (pNode->pLeft->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|      211 | 1226 | `					                              \|\| pNode->pLeft->pOp->iOp == EXPR_OP_ARROW)) );` |
|    11787 | 1227 | `				if( !bContinuable ){` |
|      149 | 1228 | `					iLeftFlags &= ~EXPR_FLAG_DEFER_ARG;` |
|       72 | 1229 | `				}` |
|     5891 | 1230 | `			}` |
|  2954246 | 1231 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
|  2457672 | 1232 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|   980581 | 1233 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|   967759 | 1234 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|    27025 | 1235 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|  2940741 | 1236 | `			}else if( iLeftFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|        - | 1237 | ``				/* A SUBSCRIPT intermediate of an unset chain (`unset($a['k']['n'])`) keeps`` |
|        - | 1238 | `				 * the unset context — it must COW-separate the parent and must NOT vivify a` |
|        - | 1239 | `				 * missing key — but it is a READ of the container, not an unset of it. The` |
|        - | 1240 | ``				 * UNSET_BASE context says exactly that: `$a['k']` is loaded, where the`` |
|        - | 1241 | `				 * plain unset context would have removed the ELEMENT (and, for an` |
|        - | 1242 | `				 * ArrayAccess base, called offsetUnset() on the intermediate key). */` |
|      215 | 1243 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|      215 | 1244 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_UNSET_BASE;` |
|      105 | 1245 | `			}` |
|        - | 1246 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|        - | 1247 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|        - | 1248 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|        - | 1249 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|        - | 1250 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|        - | 1251 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|        - | 1252 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
|  2954246 | 1253 | `			if( pNode->pOp` |
|  4416704 | 1254 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
|  2939640 | 1255 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  2924970 | 1256 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|    31839 | 1257 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|    15917 | 1258 | `			}` |
|        - | 1259 | ``			/* `++`/`--` mutate their operand in place — the operand is a write`` |
|        - | 1260 | ``			 * lvalue exactly like a compound assign's (`$o->m[0]++` must tag the`` |
|        - | 1261 | ``			 * member base PH7_MEMBER_WRITE the way `$o->m[0] += 1` does: hooked`` |
|        - | 1262 | `			 * properties throw php's Indirect-modification Error, missing ones` |
|        - | 1263 | `			 * auto-vivify). The prec-18 site below handles the assign family;` |
|        - | 1264 | ``			 * `++`/`--` are unary, their operand is pLeft. */`` |
|  2954246 | 1265 | `			if( pNode->pOp` |
|  2954251 | 1266 | `				&& (pNode->pOp->iVmOp == PH7_OP_INCR \|\| pNode->pOp->iVmOp == PH7_OP_DECR) ){` |
|        - | 1267 | ``				/* `(new A)->p++` writes through a temporary exactly as `= 1` does. */`` |
|    47079 | 1268 | `				rc = GenStateWriteTargetCheck(&(*pGen),pNode->pLeft,0);` |
|    47079 | 1269 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 1270 | `					return rc;` |
|        - | 1271 | `				}` |
|    47079 | 1272 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE` |
|        - | 1273 | ``					\| EXPR_FLAG_RMW_LOAD /* php warns before seeding `$undef++` */;`` |
|    23537 | 1274 | `			}` |
|        - | 1275 | ``			/* `??` reads its LEFT operand in isset-context: an undefined or`` |
|        - | 1276 | `			 * UNINITIALIZED typed PROPERTY must yield the default rather than a` |
|        - | 1277 | `			 * warning/Error (php). Tag a member-access LHS so its OP_MEMBER takes` |
|        - | 1278 | `			 * the silent-lookup path (iP2 = ISSET), which still loads a present` |
|        - | 1279 | `			 * value. A SUBSCRIPT LHS is left untagged: LOAD_IDX's ISSET mode means` |
|        - | 1280 | ``			 * offsetExists (a bool), but `$o[$k] ?? d` needs the offsetGet value —`` |
|        - | 1281 | `			 * that path is already handled correctly by OP_NULLC. */` |
|  2954251 | 1282 | `			if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC && pNode->pLeft ){` |
|        - | 1283 | ``				/* php reads the ENTIRE left operand of `??` in isset-context: no`` |
|        - | 1284 | ``				 * "Undefined variable" for `$x ?? d` OR for the base of`` |
|        - | 1285 | ``				 * `$x['k'] ?? d`. QUIET_VAR silences the variable read wherever it`` |
|        - | 1286 | `				 * sits in the chain. */` |
|      341 | 1287 | `				iLeftFlags \|= EXPR_FLAG_QUIET_VAR;` |
|      341 | 1288 | `				bNullcLhs = 1;` |
|      336 | 1289 | `				if( pNode->pLeft->pOp` |
|      403 | 1290 | `					&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|      242 | 1291 | `						\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|      210 | 1292 | `						\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|        - | 1293 | `					/* A member-access LHS additionally takes OP_MEMBER's SILENT` |
|        - | 1294 | `					 * lookup so an uninitialized typed property yields the default` |
|        - | 1295 | `					 * instead of an Error. It used to borrow isset()'s context for` |
|        - | 1296 | `					 * that, which is the same mistake the comment below records for` |
|        - | 1297 | `					 * subscripts: silence is shared, but isset() context makes every` |
|        - | 1298 | ``					 * ACCESSOR answer a truth, and `$o->p ?? d` needs the accessor's`` |
|        - | 1299 | ``					 * VALUE — so `??` has its own member context. A SUBSCRIPT LHS`` |
|        - | 1300 | `					 * still takes neither: LOAD_IDX's ISSET mode means offsetExists` |
|        - | 1301 | ``					 * (a bool), while `$o[$k] ?? d` needs the offsetGet value —`` |
|        - | 1302 | `					 * OP_NULLC already handles that path. */` |
|       69 | 1303 | `					iLeftFlags \|= EXPR_FLAG_MEMBER_COALESCE;` |
|       33 | 1304 | `				}` |
|      168 | 1305 | `			}` |
|  2954251 | 1306 | `			if( iVmOp == PH7_OP_ERR_CTRL ){` |
|        - | 1307 | `				/* '@' must suppress the diagnostics raised WHILE its operand runs, so` |
|        - | 1308 | `				 * open the window here; the trailing emit below closes it (iP1 = 0). */` |
|    16155 | 1309 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_ERR_CTRL,1,0,0,0);` |
|     8075 | 1310 | `			}` |
|  2954251 | 1311 | `			if( iVmOp == PH7_OP_NEW ){` |
|        - | 1312 | ``				/* Mark the direct operand so a call node under it (`new C($a)`) keeps the`` |
|        - | 1313 | `				 * emission order OP_NEW is assembled from — see the bNewCallee branch above. */` |
|    84361 | 1314 | `				iLeftFlags \|= EXPR_FLAG_NEW_CALLEE;` |
|    42178 | 1315 | `			}` |
|  2954251 | 1316 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags\|EXPR_FLAG_RDONLY_LOAD);` |
|  2954251 | 1317 | `			if( rc == SXRET_OK && bNullcLhs ){` |
|        - | 1318 | ``				/* Mark EVERY subscript read in the `??` left chain quiet (iP2=8).`` |
|        - | 1319 | `				 * Peeking at the next instruction only catches the OUTERMOST` |
|        - | 1320 | ``				 * access, so `$d['x']['y'] ?? $v` still warned for the inner one;`` |
|        - | 1321 | `				 * and a forward scan would be unsound, since an unrelated sibling` |
|        - | 1322 | ``				 * access can sit immediately before a coalesce (`f($a['k'],`` |
|        - | 1323 | ``				 * $b['j'] ?? 1)`). The compiler knows the real extent, so it marks`` |
|        - | 1324 | `				 * the range it just emitted. Write-context codes (1/3/5) belong to` |
|        - | 1325 | ``				 * `??=` and keep their meaning. */`` |
|      341 | 1326 | `				sxu32 nEnd = PH7_VmInstrLength(pGen->pVm);` |
|        - | 1327 | `				sxu32 nAt;` |
|     1393 | 1328 | `				for( nAt = nNullcLhsFirst ; nAt < nEnd ; ++nAt ){` |
|     1057 | 1329 | `					VmInstr *pFix = PH7_VmGetInstr(pGen->pVm,nAt);` |
|     1057 | 1330 | `					if( pFix && pFix->iOp == PH7_OP_LOAD_IDX && pFix->iP2 == 0 ){` |
|      187 | 1331 | `						pFix->iP2 = 8;` |
|       91 | 1332 | `					}` |
|      531 | 1333 | `				}` |
|      168 | 1334 | `			}` |
|        - | 1335 | `		}` |
|  2954251 | 1336 | `		if( rc != SXRET_OK ){` |
|       67 | 1337 | `			return rc;` |
|        - | 1338 | `		}` |
|  2954189 | 1339 | `		if( !bIsChainOp ){` |
|        - | 1340 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|        - | 1341 | `			 * target the end of that LHS chain, which is right here. */` |
|  2042499 | 1342 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|  1021247 | 1343 | `		}` |
|  2954189 | 1344 | `		if( iVmOp == PH7_OP_CALL ){` |
|   672785 | 1345 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   672785 | 1346 | `			if( pInstr ){` |
|   672785 | 1347 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|   644473 | 1348 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|        - | 1349 | `					sxu32 nQual;` |
|   644473 | 1350 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|        - | 1351 | `					/* Prevent constant expansion but preserve the absolute flag` |
|        - | 1352 | `					 * so the later NEW handler (if any) can see it. */` |
|   644473 | 1353 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|        - | 1354 | `					/* Namespace-qualify the function name for CALL, unless the` |
|        - | 1355 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|        - | 1356 | `					 * imports — class imports must NOT affect function` |
|        - | 1357 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|        - | 1358 | `					 * before NEW; we store the original literal index in the` |
|        - | 1359 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|        - | 1360 | `					 * the unqualified name and re-qualify with class imports. */` |
|   644473 | 1361 | `					if( bAbsolute ){` |
|       83 | 1362 | `						pInstr->iP2 = (sxi32)nOrig;` |
|       44 | 1363 | `					}else{` |
|   644395 | 1364 | `						int fromImport = 0;` |
|   644395 | 1365 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|   644395 | 1366 | `						pInstr->iP2 = (sxi32)nQual;` |
|   644395 | 1367 | `						if( nQual != nOrig ){` |
|        - | 1368 | `							/* Record the original literal index in the arg map` |
|        - | 1369 | `							 * (NOT in the CALL's iP2 — that is the hasSpread` |
|        - | 1370 | `							 * flag) so the NEW handler can recover the` |
|        - | 1371 | `							 * unqualified name and re-qualify with CLASS` |
|        - | 1372 | `							 * imports. */` |
|      167 | 1373 | `							if( p3 == 0 ){` |
|      167 | 1374 | `								VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      162 | 1375 | `									&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|      167 | 1376 | `								if( pMap ){` |
|      167 | 1377 | `									SyZero(pMap, sizeof(VmCallArgMap));` |
|      167 | 1378 | `									p3 = (void *)pMap;` |
|       81 | 1379 | `								}` |
|       81 | 1380 | `							}` |
|      167 | 1381 | `							if( p3 ){` |
|      167 | 1382 | `								((VmCallArgMap *)p3)->nOrigNameLit = nOrig + 1;` |
|      167 | 1383 | `								if( !fromImport ){` |
|        - | 1384 | `									/* Mark as namespace-qualified */` |
|      143 | 1385 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|       69 | 1386 | `								}` |
|       81 | 1387 | `							}` |
|       81 | 1388 | `						}` |
|        - | 1389 | `					}` |
|   350551 | 1390 | `				}else if( (pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */` |
|    26987 | 1391 | `						&& !(pNode->pLeft && (pNode->pLeft->iFlags & EXPR_NODE_PARENS)))` |
|    15501 | 1392 | `					\|\| pInstr->iOp == PH7_OP_NEW ){` |
|        - | 1393 | `					/* Method call,flag that. But NOT when the callee was an explicitly` |
|        - | 1394 | ``					 * PARENTHESISED member access: `($o->p)(...)` / `($o::$p)(...)` invokes`` |
|        - | 1395 | `					 * the VALUE of the property (php's variable-invocation), so the OP_MEMBER` |
|        - | 1396 | `					 * must stay a plain property READ (leaving the callable on the stack for` |
|        - | 1397 | `					 * OP_CALL to invoke) rather than being rewritten into a method-name` |
|        - | 1398 | ``					 * resolution — the parens are exactly what distinguishes `($o->p)()` from`` |
|        - | 1399 | ``					 * the method call `$o->p()`. */`` |
|    25643 | 1400 | `					pInstr->iP2 = 1;` |
|        - | 1401 | ``					/* A static call with a DYNAMIC method name (`C::$m(...)`): the`` |
|        - | 1402 | ``					 * static-`::` codegen folded the variable NAME into OP_MEMBER->p3`` |
|        - | 1403 | ``					 * as if it were a static-PROPERTY read (`C::$m`), but in a CALL the`` |
|        - | 1404 | `					 * method name is the variable's VALUE. Rebuild the sequence` |
|        - | 1405 | `					 * [class, OP_LOAD $m -> value, OP_MEMBER(static, method)] so the` |
|        - | 1406 | `					 * dynamic name is read off the stack, matching the instance` |
|        - | 1407 | ``					 * (`$o->$m()`) path. iP1==1 marks a static member. */`` |
|    25643 | 1408 | `					if( pInstr->iOp == PH7_OP_MEMBER && pInstr->iP1 == 1 && pInstr->p3 ){` |
|       17 | 1409 | `						void *pDynName = pInstr->p3;` |
|       17 | 1410 | `						(void)PH7_VmPopInstr(pGen->pVm);` |
|       17 | 1411 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,pDynName,0);` |
|       17 | 1412 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_MEMBER,1,PH7_MEMBER_METHOD,0,0);` |
|        8 | 1413 | `					}` |
|    12819 | 1414 | `				}` |
|   336390 | 1415 | `			}` |
|        - | 1416 | `			/* The callee is resolved; NOW emit the arguments. php's order — the callee` |
|        - | 1417 | `			 * first, at its INIT_FCALL / INIT_METHOD_CALL, and the arguments only after` |
|        - | 1418 | ``			 * it — is what makes `(new C)->priv(boom())` report php's `Call to private`` |
|        - | 1419 | `` 			 * method` instead of whatever the argument threw, and what keeps `$o?->m(f())` `` |
|        - | 1420 | ``			 * from running `f()` on a null receiver. It also puts the callee in reach of`` |
|        - | 1421 | `			 * the argument ops one opcode EARLIER than OP_CALL.` |
|        - | 1422 | `			 *` |
|        - | 1423 | `			 * The stack that leaves here is therefore [callee][args…] — the mirror of the` |
|        - | 1424 | `			 * layout OP_CALL's whole dispatch is written against (the method-name pair` |
|        - | 1425 | `			 * below the arguments, the spread runs counted down from the top, the` |
|        - | 1426 | `			 * deferred-argument re-walk). OP_ROT_CALLEE turns the region back over just` |
|        - | 1427 | `			 * before the call, so nothing downstream of it changes. */` |
|   672785 | 1428 | `			if( !bArgsEmitted ){` |
|        - | 1429 | `				int bTwoSlot;` |
|   672785 | 1430 | `				sArgs.p3 = p3; /* the namespace map built just above, if any */` |
|        - | 1431 | `				/* A METHOD callee leaves TWO slots — [receiver][method name] — which` |
|        - | 1432 | `				 * OP_CALL reads as one callee (the receiver answers $this and the` |
|        - | 1433 | `				 * late-static-binding class); anything else leaves one. The instruction` |
|        - | 1434 | `				 * just emitted is what decides it. */` |
|   672785 | 1435 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   685616 | 1436 | `				bTwoSlot = pInstr && pInstr->iOp == PH7_OP_MEMBER` |
|   349221 | 1437 | `					&& pInstr->iP2 == PH7_MEMBER_METHOD` |
|        - | 1438 | `					/* …unless the member NAME was folded into p3 rather than pushed:` |
|        - | 1439 | `					 * that shape pushes the target alone, so the op leaves one slot,` |
|        - | 1440 | `					 * which is the same distinction vm_ops_oo.c makes before popping. */` |
|  1009170 | 1441 | `					&& pInstr->p3 == 0;` |
|        - | 1442 | `				/* Screen the callee HERE, where php screens it: an undefined function, a` |
|        - | 1443 | `				 * callable string/array naming nothing, a value that is not callable at` |
|        - | 1444 | `				 * all. A METHOD callee needs none — its OP_MEMBER just did it, against` |
|        - | 1445 | `				 * the entry it chose. An FCC needs none either: OP_LOAD_FCC runs the same` |
|        - | 1446 | `				 * screen, and nothing runs before it. And with NO arguments the call` |
|        - | 1447 | `				 * itself is already the first thing to happen, so there is nothing to` |
|        - | 1448 | `				 * order and no reason to pay for a second resolution. */` |
|        - | 1449 | `				{` |
|   672785 | 1450 | `					ph7_expr_node **apCallArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   672785 | 1451 | `					sxi32 nCallArg = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|   786641 | 1452 | `					int bNodeFcc = nCallArg == 1 && apCallArg[0]` |
|   897903 | 1453 | `						&& (apCallArg[0]->iFlags & EXPR_NODE_FCC);` |
|   672785 | 1454 | `					if( bNewCallee ){` |
|        - | 1455 | ``						/* A `new`'s operand: the screen is OP_NEW itself, run with no`` |
|        - | 1456 | `						 * arguments on the stack (iP1 = -1). It asks every refusal the` |
|        - | 1457 | `						 * real pass asks and leaves the class name standing, so the two` |
|        - | 1458 | `						 * cannot disagree. Record where that push is — the NEW codegen` |
|        - | 1459 | `						 * used to find it one instruction behind the trailing OP_CALL,` |
|        - | 1460 | `						 * and the argument list now sits in between. */` |
|    82737 | 1461 | `						nNewClassInstr = PH7_VmInstrLength(pGen->pVm);` |
|    82737 | 1462 | `						if( nCallArg > 0 ){` |
|    80521 | 1463 | `							PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,-1,0,0,0);` |
|    40263 | 1464 | `						}` |
|   631419 | 1465 | `					}else if( nCallArg > 0 && !bTwoSlot && !bNodeFcc ){` |
|   554043 | 1466 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL_INIT,0,` |
|   554008 | 1467 | `							(p3 && ((VmCallArgMap *)p3)->bIsNamespaced) ? 1 : 0,0,0);` |
|   277004 | 1468 | `					}` |
|        - | 1469 | `				}` |
|   672785 | 1470 | `				rc = GenStateEmitCallArgs(&(*pGen),pNode,iFlags,&sArgs);` |
|   672785 | 1471 | `				if( rc != SXRET_OK ){` |
|       10 | 1472 | `					return rc;` |
|        - | 1473 | `				}` |
|   672777 | 1474 | `				iP1 = sArgs.iP1;` |
|   672777 | 1475 | `				iP2 = sArgs.iP2;` |
|   672777 | 1476 | `				p3  = sArgs.p3;` |
|   672777 | 1477 | `				bFcc = sArgs.bFcc;` |
|   672777 | 1478 | `				if( iP1 > 0 \|\| iP2 ){` |
|   955373 | 1479 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_ROT_CALLEE,iP1,` |
|   636912 | 1480 | `						(iP2 ? PH7_ROT_SPREAD : 0) \| (bTwoSlot ? PH7_ROT_TWOSLOT : 0),0,0);` |
|   318456 | 1481 | `				}` |
|   672777 | 1482 | `				if( bNewCallee && nNewClassInstr > 0 ){` |
|    82737 | 1483 | `					if( p3 == 0 ){` |
|     2183 | 1484 | `						VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|     2178 | 1485 | `							&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|     2183 | 1486 | `						if( pMap ){` |
|     2183 | 1487 | `							SyZero(pMap,sizeof(VmCallArgMap));` |
|     2183 | 1488 | `							p3 = (void *)pMap;` |
|     1089 | 1489 | `						}` |
|     1089 | 1490 | `					}` |
|    82737 | 1491 | `					if( p3 ){` |
|    82737 | 1492 | `						((VmCallArgMap *)p3)->nNewClassInstr = nNewClassInstr;` |
|    41366 | 1493 | `					}` |
|    41366 | 1494 | `				}` |
|   336391 | 1495 | `			}` |
|  2617795 | 1496 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|        - | 1497 | `			ph7_expr_node **apNode;` |
|        - | 1498 | `			sxi32 n;` |
|   207081 | 1499 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|        - | 1500 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|        - | 1501 | `				\|EXPR_FLAG_LOAD_IDX_UNSET_BASE` |
|        - | 1502 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE` |
|        - | 1503 | `				\|EXPR_FLAG_MEMBER_COALESCE` |
|        - | 1504 | `				\|EXPR_FLAG_QUIET_VAR\|EXPR_FLAG_RMW_LOAD\|EXPR_FLAG_DEFER_ARG);` |
|        - | 1505 | `			/* Recurse and generate bytecodes for array index */` |
|   207081 | 1506 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   382291 | 1507 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   175215 | 1508 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   175215 | 1509 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   175215 | 1510 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 1511 | `					return rc;` |
|        - | 1512 | `				}` |
|        - | 1513 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   175215 | 1514 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|    87610 | 1515 | `			}` |
|   207081 | 1516 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   175215 | 1517 | `				iP1 = 1; /* Node have an index associated with it */` |
|    87610 | 1518 | `			}else{` |
|        - | 1519 | ``				/* `[]` names the element a WRITE is about to create, so php allows it`` |
|        - | 1520 | `				 * only where a write lands: an assignment target (plain, compound,` |
|        - | 1521 | ``				 * `=&`, a list()/foreach target) and a by-reference argument. Every`` |
|        - | 1522 | `				 * other placement is a COMPILE error there — PHL accepted them all and` |
|        - | 1523 | ``				 * answered NULL after a PH7-worded notice, so `$x = $a[];`,`` |
|        - | 1524 | ``				 * `isset($a[])` and `unset($a[][0])` were silent no-ops on source php`` |
|        - | 1525 | `				 * refuses to run. A call ARGUMENT is the one shape php also leaves to` |
|        - | 1526 | `				 * runtime (it cannot know the parameter's by-ref-ness at compile time),` |
|        - | 1527 | `				 * which is what DEFER_ARG marks. */` |
|    31871 | 1528 | `				if( iFlags & (EXPR_FLAG_LOAD_IDX_UNSET\|EXPR_FLAG_LOAD_IDX_UNSET_BASE) ){` |
|      ! 0 | 1529 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,` |
|      ! 0 | 1530 | `						pNode->pStart ? pNode->pStart->nLine : 0,` |
|        - | 1531 | `						"Cannot use [] for unsetting");` |
|      ! 0 | 1532 | `					return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1533 | `				}` |
|    31871 | 1534 | `				if( (iFlags & (EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_DEFER_ARG)) == 0 ){` |
|      ! 0 | 1535 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,` |
|      ! 0 | 1536 | `						pNode->pStart ? pNode->pStart->nLine : 0,` |
|        - | 1537 | `						"Cannot use [] for reading");` |
|      ! 0 | 1538 | `					return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1539 | `				}` |
|        - | 1540 | `			}` |
|   207081 | 1541 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|        - | 1542 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|    10703 | 1543 | `				iP2 = 4;` |
|   201732 | 1544 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|        - | 1545 | `				/* offsetUnset for ArrayAccess; for an array, remove the ELEMENT. */` |
|      185 | 1546 | `				iP2 = 5;` |
|   196293 | 1547 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET_BASE ){` |
|        - | 1548 | `				/* An unset chain's intermediate container: read it, but with the` |
|        - | 1549 | `				 * unset context's COW-separate and no-vivify rules. */` |
|       20 | 1550 | `				iP2 = 10;` |
|   196194 | 1551 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|        - | 1552 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|        - | 1553 | `				 * short-circuit on missing keys without invoking offsetGet` |
|        - | 1554 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|       45 | 1555 | `				iP2 = 6;` |
|   196165 | 1556 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|        - | 1557 | `				/* Create an empty entry when the desired index is not found */` |
|   120573 | 1558 | `				iP2 = 1;` |
|   135861 | 1559 | `			}else if( iFlags & EXPR_FLAG_DEFER_ARG ){` |
|        - | 1560 | `				/* D1 commit 2: deferred by-ref/by-value element arg. Behaves as a read but,` |
|        - | 1561 | `				 * on a lookup miss, records the lvalue path instead of warning; OP_CALL` |
|        - | 1562 | `				 * re-walks it in vivify (by-ref) or read+warn (by-value) mode. */` |
|    11081 | 1563 | `				iP2 = 9;` |
|     5543 | 1564 | `			}` |
|  2177871 | 1565 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|        - | 1566 | `			/* POP the left node */` |
|        5 | 1567 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        2 | 1568 | `		}` |
|  1477088 | 1569 | `	}` |
|  2954205 | 1570 | `	rc = SXRET_OK;` |
|  2954205 | 1571 | `	nJmpIdx = 0;` |
|        - | 1572 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|        - | 1573 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|        - | 1574 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  2954205 | 1575 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     2523 | 1576 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     2523 | 1577 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     2523 | 1578 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     2523 | 1579 | `			int isSpecial = 0;` |
|     2523 | 1580 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     2391 | 1581 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     2391 | 1582 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     2386 | 1583 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     2187 | 1584 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     1101 | 1585 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      447 | 1586 | `					isSpecial = 1;` |
|      221 | 1587 | `				}` |
|     1226 | 1588 | `			}` |
|     2589 | 1589 | `			pInstr->iP1 = 0;` |
|        - | 1590 | ``			/* A leading `\` (NSSEP) makes the class name ABSOLUTE: it names the`` |
|        - | 1591 | `			 * global class, so it must NOT be re-qualified with the current namespace.` |
|        - | 1592 | ``			 * `\Closure::bind(...)` inside `namespace X` is Closure, not X\Closure.`` |
|        - | 1593 | ``			 * (Multi-component `\A\B::m` already resolves absolutely because its`` |
|        - | 1594 | ``			 * literal keeps a backslash; the single-component `\Closure` lost it.) */`` |
|        - | 1595 | `			{` |
|     3815 | 1596 | `				int bAbsolute = (pNode->pLeft && pNode->pLeft->pStart` |
|     3678 | 1597 | `					&& (pNode->pLeft->pStart->nType & PH7_TK_NSSEP));` |
|     2457 | 1598 | `				if( !isSpecial && !bAbsolute ){` |
|     1993 | 1599 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      994 | 1600 | `				}` |
|        - | 1601 | `			}` |
|        - | 1602 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|        - | 1603 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     2457 | 1604 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|     2015 | 1605 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|     2015 | 1606 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      214 | 1607 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      131 | 1608 | `					return SXRET_OK;` |
|        - | 1609 | `				}` |
|      942 | 1610 | `			}` |
|     1163 | 1611 | `		}` |
|     1232 | 1612 | `	}` |
|        - | 1613 | `	/* Generate code for the right tree */` |
|  2954055 | 1614 | `	if( pNode->pRight ){` |
|  1687183 | 1615 | `		if( iVmOp == PH7_OP_LAND ){` |
|        - | 1616 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    82897 | 1617 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  1645737 | 1618 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|        - | 1619 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    72215 | 1620 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  1568186 | 1621 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|        - | 1622 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|      341 | 1623 | `			iVmOp = 0; /* No binary operator to emit */` |
|      341 | 1624 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  1531977 | 1625 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|        - | 1626 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|        - | 1627 | `			 * the entire containing postfix chain to null. The jump target is` |
|        - | 1628 | `			 * patched later by the innermost non-chain ancestor (or by` |
|        - | 1629 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|        - | 1630 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|        - | 1631 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|      133 | 1632 | `			sxu32 nNsJmp = 0;` |
|      133 | 1633 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|      133 | 1634 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  1531681 | 1635 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */` |
|  1183933 | 1636 | ``			\|\| pNode->pOp->iOp == EXPR_OP_REF /* `=&` reference bind */ ){`` |
|        - | 1637 | `` 			/* The lvalue is the RIGHT operand (prec-18 ops are right-associative; `=&` `` |
|        - | 1638 | `			 * swaps its operands in parse.c so its target is pRight too). Mark it a write` |
|        - | 1639 | `			 * target so a missing base (the container of a subscript-write, or a bare` |
|        - | 1640 | `` 			 * `$o->p`) is auto-created — PHP auto-vivifies on a plain write AND on a `=&` `` |
|        - | 1641 | ``			 * bind (`$a[0] =& $x` creates $a as [0 => &$x], it does not warn). */`` |
|        - | 1642 | `			/* php's compile-time write-target rules first ($this, a temporary base). */` |
|   695573 | 1643 | `			rc = GenStateWriteTargetCheck(&(*pGen),pNode->pRight,0);` |
|   695573 | 1644 | `			if( rc != SXRET_OK ){` |
|       11 | 1645 | `				return rc;` |
|        - | 1646 | `			}` |
|   695565 | 1647 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|   695565 | 1648 | `			if( iVmOp != PH7_OP_STORE && pNode->pOp->iOp != EXPR_OP_REF ){` |
|        - | 1649 | ``				/* COMPOUND assignment (`.=`, `+=`, ...) READS the target first, so`` |
|        - | 1650 | ``				 * php warns when it is undefined and then seeds it; a plain `=` and a`` |
|        - | 1651 | ``				 * `=&` rebind write without reading the target and stay silent. */`` |
|    16021 | 1652 | `				iFlags \|= EXPR_FLAG_RMW_LOAD;` |
|     8008 | 1653 | `			}` |
|   347780 | 1654 | `		}` |
|  1687175 | 1655 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  1687175 | 1656 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_RDONLY_LOAD);` |
|  1687175 | 1657 | `		if( !bIsChainOp ){` |
|        - | 1658 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|        - | 1659 | `			 * operator instruction is emitted. */` |
|  1655467 | 1660 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|   827731 | 1661 | `		}` |
|  1687175 | 1662 | `		if( iVmOp == PH7_OP_STORE ){` |
|   679351 | 1663 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|   679306 | 1664 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|        - | 1665 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|        - | 1666 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|        - | 1667 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|        - | 1668 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|        - | 1669 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|        - | 1670 | `				 */` |
|      177 | 1671 | `				iVmOp = 0;` |
|   679265 | 1672 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|   679179 | 1673 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|        - | 1674 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|     1191 | 1675 | `					iP2 = 1;` |
|      598 | 1676 | `				}else{` |
|   677993 | 1677 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|        - | 1678 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   120153 | 1679 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   120153 | 1680 | `						iP1 = pInstr->iP1;` |
|    60079 | 1681 | `					}else{` |
|   557845 | 1682 | `						p3 = pInstr->p3;` |
|        - | 1683 | `					}` |
|        - | 1684 | `					/* POP the last dynamic load instruction */` |
|   677993 | 1685 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|        - | 1686 | `				}` |
|   339592 | 1687 | `			}` |
|  1347502 | 1688 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|        - | 1689 | `			/* Peek first: a member LHS ($o->p =& $x, C::$s =& $x) keeps its` |
|        - | 1690 | `			 * OP_MEMBER in place (it resolves + stashes the target slot at` |
|        - | 1691 | `			 * runtime), unlike the variable/array shapes which fold their load` |
|        - | 1692 | `			 * away. Mirrors the normal member-store fold above (iP2 kept-MEMBER). */` |
|      203 | 1693 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      203 | 1694 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|        - | 1695 | `				/* Tag the member as a reference target and flag STORE_REF (iP2=1)` |
|        - | 1696 | `				 * to take the member-rebind path in the VM. */` |
|       46 | 1697 | `				pInstr->iP2 = PH7_MEMBER_REF_TARGET;` |
|       46 | 1698 | `				iP2 = 1;` |
|       24 | 1699 | `			}else{` |
|      159 | 1700 | `				pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      159 | 1701 | `				if( pInstr ){` |
|      159 | 1702 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|        - | 1703 | `						/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|        - | 1704 | `						 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|        - | 1705 | `						 */` |
|       55 | 1706 | `						iVmOp = PH7_OP_STORE_IDX_REF;` |
|       55 | 1707 | `						iP1 = pInstr->iP1;` |
|       55 | 1708 | `						iP2 = pInstr->iP2;` |
|       55 | 1709 | `						p3  = pInstr->p3;` |
|       29 | 1710 | `					}else{` |
|      107 | 1711 | `						p3 = pInstr->p3;` |
|        - | 1712 | `					}` |
|       77 | 1713 | `				}` |
|        - | 1714 | `			}` |
|       99 | 1715 | `		}` |
|   843585 | 1716 | `	}` |
|  2954042 | 1717 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|    42995 | 1718 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|        - | 1719 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|        - | 1720 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|       79 | 1721 | `		iVmOp = 0;` |
|       37 | 1722 | `	}` |
|  2954047 | 1723 | `	if( iVmOp > 0 ){` |
|  2953461 | 1724 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|    47079 | 1725 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|        - | 1726 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|     5205 | 1727 | `				iP1 = 1;` |
|     2605 | 1728 | `			}` |
|  2929924 | 1729 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|        - | 1730 | `			/* Namespace-qualify the class name for NEW */ {` |
|    84287 | 1731 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|    84287 | 1732 | `				VmInstr *pCallInstr = 0;` |
|    84287 | 1733 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|    82737 | 1734 | `					VmCallArgMap *pNewMap = (VmCallArgMap *)pPeek->p3;` |
|    82737 | 1735 | `					pCallInstr = pPeek;` |
|        - | 1736 | `` 					/* The class-name push sits one instruction back only when this `new` `` |
|        - | 1737 | `					 * takes no arguments; with an argument list the reorder puts the whole` |
|        - | 1738 | `					 * list (and its screen and rotation) in between, so the call node` |
|        - | 1739 | `					 * recorded where the push is. */` |
|   124103 | 1740 | `					pPeek = (pNewMap && pNewMap->nNewClassInstr > 0)` |
|    82732 | 1741 | `						? PH7_VmGetInstr(pGen->pVm,pNewMap->nNewClassInstr - 1)` |
|    41366 | 1742 | `						: PH7_VmPeekNextInstr(pGen->pVm);` |
|    41366 | 1743 | `				}` |
|    84287 | 1744 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|    84251 | 1745 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|        - | 1746 | `					sxu32 nLitForClass;` |
|    84251 | 1747 | `					VmCallArgMap *pCallNsMap = pCallInstr ? (VmCallArgMap *)pCallInstr->p3 : 0;` |
|        - | 1748 | `					/* If the CALL handler qualified the name with FUNCTION` |
|        - | 1749 | `					 * imports, recover the original literal (recorded in the` |
|        - | 1750 | `					 * arg map — OP_CALL's iP2 is the hasSpread flag, and` |
|        - | 1751 | `` 					 * misreading it as a literal index made `new C(...$args)` `` |
|        - | 1752 | `					 * fatal with "Class ' ' is not defined") and re-qualify` |
|        - | 1753 | `					 * with class imports. */` |
|    84251 | 1754 | `					if( pCallNsMap && pCallNsMap->nOrigNameLit > 0 ){` |
|       64 | 1755 | `						nLitForClass = pCallNsMap->nOrigNameLit - 1;` |
|       34 | 1756 | `					}else{` |
|    84191 | 1757 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|        - | 1758 | `					}` |
|    84251 | 1759 | `					pPeek->iP1 = 0;` |
|    84251 | 1760 | `					if( !bAbsolute ){` |
|        - | 1761 | `						/* self/static/parent are resolved at runtime against the` |
|        - | 1762 | `						 * current class — never namespace-qualify them (else` |
|        - | 1763 | ``						 * `new self` in namespace N becomes "N\self"). Mirrors the`` |
|        - | 1764 | `						 * instanceof (IS_A) guard below. */` |
|    84195 | 1765 | `						ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nLitForClass);` |
|    84195 | 1766 | `						int isSpecialNew = 0;` |
|    84195 | 1767 | `						if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|    84195 | 1768 | `							const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|    84195 | 1769 | `							sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|    84190 | 1770 | `							if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    84301 | 1771 | `								(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    42210 | 1772 | `								(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|       46 | 1773 | `								isSpecialNew = 1;` |
|       22 | 1774 | `							}` |
|    42095 | 1775 | `						}` |
|    84195 | 1776 | `						if( isSpecialNew ){` |
|       46 | 1777 | `							pPeek->iP2 = (sxi32)nLitForClass;` |
|       24 | 1778 | `						}else{` |
|    84151 | 1779 | `							pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|        - | 1780 | `						}` |
|    42100 | 1781 | `					}else{` |
|       61 | 1782 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|        - | 1783 | `					}` |
|    42123 | 1784 | `				}` |
|        - | 1785 | `			}` |
|    84287 | 1786 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    84287 | 1787 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|        - | 1788 | `				VmInstr *pPrev;` |
|        - | 1789 | `				int bPrevMember;` |
|    82737 | 1790 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|        - | 1791 | `				/* "Was the callee a MEMBER access?" — which, once the reorder puts a` |
|        - | 1792 | `				 * rotation between the callee and its call, is the question the rotation` |
|        - | 1793 | `				 * already answers (a method callee is the two-slot one). */` |
|   124103 | 1794 | `				bPrevMember = pPrev && (pPrev->iOp == PH7_OP_ROT_CALLEE` |
|    80516 | 1795 | `					? (pPrev->iP2 & PH7_ROT_TWOSLOT) != 0` |
|     2216 | 1796 | `					: pPrev->iOp == PH7_OP_MEMBER);` |
|    82737 | 1797 | `				if( !bPrevMember ){` |
|        - | 1798 | `					/* Pop the call instruction, preserve named-arg map and` |
|        - | 1799 | `					 * the hasSpread flag (OP_NEW consumes the spread` |
|        - | 1800 | `					 * accumulator exactly like OP_CALL would have). */` |
|    82737 | 1801 | `					iP1 = pInstr->iP1;` |
|    82737 | 1802 | `					iP2 = pInstr->iP2;` |
|    82737 | 1803 | `					if( pInstr->p3 ){` |
|    82737 | 1804 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|    41366 | 1805 | `					}` |
|    82737 | 1806 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    41366 | 1807 | `				}` |
|    41371 | 1808 | `			}` |
|  2864246 | 1809 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|        - | 1810 | `			/* instanceof: right operand is a class name, not a constant.` |
|        - | 1811 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|    10705 | 1812 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    10705 | 1813 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    10705 | 1814 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    10705 | 1815 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|    10705 | 1816 | `				int isSpecialIs = 0;` |
|    10705 | 1817 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|    10705 | 1818 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|    10705 | 1819 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|    10700 | 1820 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    10703 | 1821 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     5350 | 1822 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|       12 | 1823 | `						isSpecialIs = 1;` |
|        5 | 1824 | `					}` |
|     5350 | 1825 | `				}` |
|    10705 | 1826 | `				pInstr->iP1 = 0;` |
|    10705 | 1827 | `				if( !isSpecialIs && !bAbsolute ){` |
|    10669 | 1828 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     5332 | 1829 | `				}` |
|     5355 | 1830 | `			}` |
|  2816755 | 1831 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|        - | 1832 | `			/* Prevent constant expansion for member/property names.` |
|        - | 1833 | `			 * The right child (member name) was just compiled — its LOADC` |
|        - | 1834 | `			 * should not trigger constant lookup. */` |
|    31713 | 1835 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    31713 | 1836 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    31283 | 1837 | `				pInstr->iP1 = 0;` |
|    15639 | 1838 | `			}` |
|    31713 | 1839 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|        - | 1840 | `				/* Static member access,remember that */` |
|     2373 | 1841 | `				iP1 = 1;` |
|     2373 | 1842 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     2373 | 1843 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      399 | 1844 | `					p3 = pInstr->p3;` |
|        - | 1845 | ``					/* A `$`-form (`C::$s`, `C::$$x`, `C::${$e}`) is a STATIC PROPERTY`` |
|        - | 1846 | `					 * access, never a constant. A LITERAL name folds into p3 (non-zero)` |
|        - | 1847 | ``					 * and the exec side reads it there; a DYNAMIC name (`$$x`/`${$e}`)`` |
|        - | 1848 | `					 * leaves p3==0 with the computed name on the stack — the SAME shape` |
|        - | 1849 | ``					 * as a bareword constant `C::C`. Mark iP1=2 so exec still routes it`` |
|        - | 1850 | `					 * to the property table (hAttr), not the constant table (hConst). */` |
|      399 | 1851 | `					if( p3 == 0 ){` |
|        8 | 1852 | `						iP1 = 2;` |
|        3 | 1853 | `					}` |
|      399 | 1854 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|      197 | 1855 | `				}` |
|     1184 | 1856 | `			}` |
|        - | 1857 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|        - | 1858 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|        - | 1859 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|        - | 1860 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|    31713 | 1861 | `			if( iP2 == PH7_MEMBER_READ ){` |
|    31713 | 1862 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|       65 | 1863 | `					iP2 = PH7_MEMBER_UNSET;` |
|    31682 | 1864 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|      167 | 1865 | `					iP2 = PH7_MEMBER_ISSET;` |
|    31570 | 1866 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|       34 | 1867 | `					iP2 = PH7_MEMBER_EMPTY;` |
|    31474 | 1868 | `				}else if( iFlags & EXPR_FLAG_MEMBER_COALESCE ){` |
|       73 | 1869 | `					iP2 = PH7_MEMBER_COALESCE;` |
|    31424 | 1870 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|        - | 1871 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|     1695 | 1872 | `					iP2 = PH7_MEMBER_WRITE;` |
|    30544 | 1873 | `				}else if( iFlags & EXPR_FLAG_DEFER_ARG ){` |
|        - | 1874 | `					/* D1 commit 2: deferred by-ref/by-value property arg ($o->p). */` |
|      711 | 1875 | `					iP2 = PH7_MEMBER_DEFPATH;` |
|      353 | 1876 | `				}` |
|    15854 | 1877 | `			}` |
|    15854 | 1878 | `		}` |
|        - | 1879 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|        - | 1880 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|        - | 1881 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|        - | 1882 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|        - | 1883 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
|  2953461 | 1884 | `		if( bFcc ){` |
|      257 | 1885 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|        - | 1886 | `			/* php's global fallback applies to a first-class callable exactly as it does` |
|        - | 1887 | ``			 * to the call it stands for: inside a namespace, `strlen(...)` is the global`` |
|        - | 1888 | `			 * function when the current namespace has none. The callee's literal was` |
|        - | 1889 | `			 * namespace-qualified above and the arg map that records it is dropped here` |
|        - | 1890 | `			 * (an FCC has no arguments), so carry the one bit the resolution needs in the` |
|        - | 1891 | ``			 * instruction itself — without it `strlen(...)` in a namespaced file was`` |
|        - | 1892 | ``			 * `Call to undefined function A\B\strlen()`, a fatal on ordinary php. */`` |
|      257 | 1893 | `			iP2 = (p3 && ((VmCallArgMap *)p3)->bIsNamespaced) ? 1 : 0;` |
|      257 | 1894 | `			p3 = 0;` |
|      257 | 1895 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      257 | 1896 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER && pInstr->iP2 == PH7_MEMBER_METHOD ){` |
|        - | 1897 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|        - | 1898 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|        - | 1899 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|        - | 1900 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|      148 | 1901 | `				void *pMemberName = pInstr->p3;` |
|      148 | 1902 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|      148 | 1903 | `				if( pMemberName ){` |
|      ! 0 | 1904 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|      ! 0 | 1905 | `				}` |
|      148 | 1906 | `				iP1 = 2;` |
|       76 | 1907 | `			}else{` |
|        - | 1908 | `				/* Only a METHOD member is the callee's NAME. A parenthesised PROPERTY read` |
|        - | 1909 | ``				 * (`($o->cb)(...)`, `(C::$cb)(...)`) is php's variable-invocation: the member`` |
|        - | 1910 | `				 * op stays, its VALUE is the callable, and this is the iP1=1 wrap. Dropping it` |
|        - | 1911 | `				 * here read the property NAME as a method name and answered` |
|        - | 1912 | ``				 * `Call to undefined method H::cb()` for a closure the object was holding —`` |
|        - | 1913 | `				 * the CALL codegen above already made the distinction (it leaves the member a` |
|        - | 1914 | `				 * plain read for a parenthesised callee) and this branch undid it. */` |
|      111 | 1915 | `				iP1 = 1;` |
|        - | 1916 | `			}` |
|      126 | 1917 | `		}` |
|        - | 1918 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|        - | 1919 | `		 * This is the primary emit path for user-visible calls. */` |
|  2953461 | 1920 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|   756807 | 1921 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|   378401 | 1922 | `		}` |
|        - | 1923 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  2953461 | 1924 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  1476728 | 1925 | `	}` |
|  2954047 | 1926 | `	if( nJmpIdx > 0 ){` |
|        - | 1927 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   155443 | 1928 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   155443 | 1929 | `		if( pInstr ){` |
|   155443 | 1930 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    77719 | 1931 | `		}` |
|    77719 | 1932 | `	}` |
|  2954047 | 1933 | `	return rc;` |
|  3907158 | 1934 | `}` |
|        - | 1935 | `/*` |
|        - | 1936 | ` * Emit a call's ARGUMENT LIST, and decide everything about it the OP_CALL then carries:` |
|        - | 1937 | ` * the count, the unpack flag, the named-argument / assert-source / argument-shape map.` |
|        - | 1938 | ` *` |
|        - | 1939 | ` * Split out of GenStateEmitExprCode because php resolves a callee BEFORE it evaluates` |
|        - | 1940 | ` * the arguments, so this now runs AFTER the callee sub-tree has been emitted (the one` |
|        - | 1941 | `` * exception is a `new`'s constructor list — see EXPR_FLAG_NEW_CALLEE). It is otherwise`` |
|        - | 1942 | ` * the same code, and reads only the node: nothing here inspects the instructions the` |
|        - | 1943 | ` * callee left behind.` |
|        - | 1944 | ` *` |
|        - | 1945 | ` * pArgs->p3 may arrive non-NULL — the callee's own namespace qualification builds the` |
|        - | 1946 | ` * VmCallArgMap first now — and every allocation site below reuses it.` |
|        - | 1947 | ` */` |
|   672780 | 1948 | `static sxi32 GenStateEmitCallArgs(` |
|        - | 1949 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - | 1950 | `	ph7_expr_node *pNode, /* The call node */` |
|        - | 1951 | `	sxi32 iFlags,         /* Control flags of the call site */` |
|        - | 1952 | `	GenCallArgs *pArgs    /* OUT: what the OP_CALL needs */` |
|        - | 1953 | `	)` |
|        5 | 1954 | `{` |
|   672785 | 1955 | `	void *p3 = pArgs->p3;` |
|   672785 | 1956 | `	sxi32 iP1 = 0;` |
|   672785 | 1957 | `	sxu32 iP2 = 0;` |
|   672785 | 1958 | `	int bFcc = 0;` |
|        - | 1959 | `	sxi32 rc;` |
|        - | 1960 | `	ph7_expr_node **apNode;` |
|   672785 | 1961 | `	int hasSpread = 0;` |
|   672785 | 1962 | `	int hasNamed = 0;` |
|   672785 | 1963 | `	sxu32 byRefMask = 0;` |
|        - | 1964 | `	sxi32 nArgs;` |
|        - | 1965 | `	sxi32 n;` |
|   672785 | 1966 | `	int bAnySpread = 0;` |
|        - | 1967 | `	/* Recurse and generate bytecodes for function arguments */` |
|   672785 | 1968 | `	apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   672785 | 1969 | `	nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|        - | 1970 | ``	/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|        - | 1971 | `	 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|        - | 1972 | `	 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|   672785 | 1973 | `	if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|      257 | 1974 | `		bFcc = 1;` |
|      257 | 1975 | `		nArgs = 0;` |
|      126 | 1976 | `	}` |
|        - | 1977 | `	/* Validate argument order like php: no positional argument after a` |
|        - | 1978 | ``	 * named one OR after unpacking, and `name: ...$x` is a parse error. */`` |
|        - | 1979 | `	{` |
|   672785 | 1980 | `		int seenNamed = 0;` |
|   672785 | 1981 | `		int seenSpread = 0;` |
|  1580573 | 1982 | `		for( n = 0; n < nArgs; ++n ){` |
|   907795 | 1983 | `			if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|      274 | 1984 | `				bAnySpread = 1;` |
|      274 | 1985 | `				seenSpread = 1;` |
|      274 | 1986 | `				if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      ! 0 | 1987 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|        - | 1988 | `						"syntax error, unexpected token \"...\"");` |
|      ! 0 | 1989 | `					return SXERR_SYNTAX;` |
|        4 | 1990 | `				}` |
|   907660 | 1991 | `			}else if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      589 | 1992 | `				seenNamed = 1;` |
|      589 | 1993 | `				hasNamed = 1;` |
|   907233 | 1994 | `			}else if( seenNamed ){` |
|        3 | 1995 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|        - | 1996 | `					"Cannot use positional argument after named argument");` |
|        3 | 1997 | `				return SXERR_SYNTAX;` |
|   906939 | 1998 | `			}else if( seenSpread ){` |
|      ! 0 | 1999 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|        - | 2000 | `					"Cannot use positional argument after argument unpacking");` |
|      ! 0 | 2001 | `				return SXERR_SYNTAX;` |
|        - | 2002 | `			}` |
|   453899 | 2003 | `		}` |
|        - | 2004 | `	}` |
|        - | 2005 | `	/* Read-only load */` |
|   672783 | 2006 | `	iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|        - | 2007 | `	/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|        - | 2008 | ``	 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|        - | 2009 | `	 * objects dispatch to the right method (offsetExists for both;` |
|        - | 2010 | `	 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|   672783 | 2011 | `	if( pNode->pLeft && pNode->pLeft->pStart ){` |
|   672783 | 2012 | `		SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|  1034781 | 2013 | `		int bIsset = pCallName->nByte == 5` |
|   672778 | 2014 | `			&& SyStrnicmp(pCallName->zString,"isset",5) == 0;` |
|  1034781 | 2015 | `		int bEmpty = pCallName->nByte == 5` |
|   672778 | 2016 | `			&& SyStrnicmp(pCallName->zString,"empty",5) == 0;` |
|        - | 2017 | `		/* isset()/empty() are language CONSTRUCTS, not functions: php parses` |
|        - | 2018 | `		 * their argument list in the grammar and a missing operand is a parse` |
|        - | 2019 | `		 * error on the ')'. They compile through this ordinary call loop, which` |
|        - | 2020 | ``		 * never checked arity, so `empty()` quietly evaluated to true and`` |
|        - | 2021 | ``		 * `isset()` to false. (empty() also takes exactly one operand in php,`` |
|        - | 2022 | `		 * unlike isset(), which is variadic.) */` |
|   672783 | 2023 | `		if( (bIsset \|\| bEmpty) && nArgs < 1 ){` |
|        - | 2024 | `			/* php names the ')' itself as the unexpected token, so point at the` |
|        - | 2025 | `			 * node's last token rather than pGen->pIn (which has already moved` |
|        - | 2026 | `			 * past the call to the statement's ';'). */` |
|        5 | 2027 | `			SyToken *pTok = pNode->pEnd;` |
|        5 | 2028 | `			if( pTok && pTok > pNode->pStart && (pTok->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 | 2029 | `				pTok--;` |
|      ! 0 | 2030 | `			}` |
|        5 | 2031 | `			PH7_GenSyntaxError(&(*pGen),pTok,0);` |
|        5 | 2032 | `			return SXERR_ABORT;` |
|        - | 2033 | `		}` |
|   672779 | 2034 | `		if( bIsset ){` |
|    10905 | 2035 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|   667329 | 2036 | `		}else if( bEmpty ){` |
|      157 | 2037 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|       76 | 2038 | `		}` |
|        - | 2039 | `		/* Auto-vivify by-reference out-params of known builtins so an` |
|        - | 2040 | `		 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|        - | 2041 | `		 * $m never assigned) gets a real memobj slot for the builtin to` |
|        - | 2042 | `		 * write back through. Skipped when spread/named args are present:` |
|        - | 2043 | `		 * the compile-time positional index no longer maps to the` |
|        - | 2044 | `		 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|   672779 | 2045 | `		if( !bAnySpread && !hasNamed ){` |
|        - | 2046 | `			SyString sBuiltin;` |
|   672183 | 2047 | `			GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|   672183 | 2048 | `			byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|   336089 | 2049 | `		}` |
|   336387 | 2050 | `	}` |
|  1580563 | 2051 | `	for( n = 0 ; n < nArgs ; ++n ){` |
|   907791 | 2052 | `		sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   907791 | 2053 | `		sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|        - | 2054 | `		/* For a by-ref argument position, drop the read-only flag so the` |
|        - | 2055 | `		 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|        - | 2056 | `		 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|        - | 2057 | `		 * auto-vivifies its element and exposes a writable memobj slot for the` |
|        - | 2058 | `		 * builtin to write back through. A plain $var target is unaffected` |
|        - | 2059 | `		 * (iP1=0 either way). */` |
|   907791 | 2060 | `		if( n < 31 && (byRefMask & (1u<<n)) ){` |
|    16139 | 2061 | `			iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|    16139 | 2062 | `			iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|     8067 | 2063 | `		}` |
|        - | 2064 | ``		/* D1: a plain `$var` argument may bind to a by-ref parameter whose signature`` |
|        - | 2065 | `		 * is unknown at compile time (forward reference, dynamic call, or method` |
|        - | 2066 | ``		 * dispatch — e.g. PHPUnit's `willReturnReference($undef)`). We used to clear`` |
|        - | 2067 | `		 * the read-only flag here so an undefined variable vivified a real slot the` |
|        - | 2068 | `		 * by-ref write-back could reach — but that also invented the variable as NULL` |
|        - | 2069 | `		 * in the caller when the parameter turned out by-VALUE, and suppressed php's` |
|        - | 2070 | ``		 * `Undefined variable $x` warning. Instead mark it DEFERRED: OP_LOAD leaves an`` |
|        - | 2071 | `		 * undefined variable uncreated and carries a lazy-lvalue marker, and OP_CALL` |
|        - | 2072 | `		 * materializes it ONLY for a by-ref parameter once the callee is resolved` |
|        - | 2073 | `		 * (VmResolveDeferredArgs). Excludes isset()/empty()/unset(), which compile` |
|        - | 2074 | `		 * through this same call loop but must NEVER create their operand, and` |
|        - | 2075 | `		 * spread args (whose elements have no positional index of their own). A NAMED` |
|        - | 2076 | `		 * arg defers too: it binds to the formal its NAME picks, which the resolver` |
|        - | 2077 | `		 * looks up through the call's own argument map — excluding it left` |
|        - | 2078 | ``		 * `r(x: $a['new'])` warning `Undefined array key` and passing NULL where php`` |
|        - | 2079 | ``		 * creates the element for the by-ref parameter `$x`.`` |
|        - | 2080 | `		 *` |
|        - | 2081 | `		 * D1 commit 2: the same reasoning extends to an array-element ($a["k"]) or` |
|        - | 2082 | `		 * property ($o->p) argument — a by-ref user-function parameter must vivify the` |
|        - | 2083 | `		 * element/property, a by-value one must warn and NOT vivify. Those nodes carry a` |
|        - | 2084 | `		 * subscript/arrow operator (pOp != 0). The DEFER flag rides down to the base LOAD` |
|        - | 2085 | `		 * (undefined base auto-defers via commit 1) and to the LOAD_IDX/MEMBER, which` |
|        - | 2086 | ``		 * record the lvalue path on a lookup miss. Static `::` and nullsafe `?->` stay`` |
|        - | 2087 | `		 * eager. */` |
|   907786 | 2088 | `		if( (iFlags & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_LOAD_IDX_UNSET` |
|   453893 | 2089 | `		               \|EXPR_FLAG_MEMBER_COALESCE)) == 0` |
|   902247 | 2090 | `		 && (iArgFlags & EXPR_FLAG_RDONLY_LOAD) /* not a known builtin by-ref slot (kept eager above) */` |
|   888641 | 2091 | `		 && (apNode[n]->iFlags & EXPR_NODE_SPREAD) == 0` |
|   943912 | 2092 | `		 && ( (apNode[n]->pOp == 0 && apNode[n]->xCode == PH7_CompileVariable)` |
|   628215 | 2093 | `		   \|\| (apNode[n]->pOp != 0 && (apNode[n]->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|   132449 | 2094 | `		                            \|\| apNode[n]->pOp->iOp == EXPR_OP_ARROW)) ) ){` |
|   515851 | 2095 | `			iArgFlags \|= EXPR_FLAG_DEFER_ARG;` |
|   257923 | 2096 | `		}` |
|   907791 | 2097 | `		rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|   907791 | 2098 | `		if( rc != SXRET_OK ){` |
|        3 | 2099 | `			return rc;` |
|        - | 2100 | `		}` |
|        - | 2101 | `		/* Each argument is an independent nullsafe scope. */` |
|   907789 | 2102 | `		GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|   907789 | 2103 | `		if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|        - | 2104 | `			/* Emit spread opcode to unpack this array argument. iP1 marks a` |
|        - | 2105 | ``			 * source php will unpack BY REFERENCE: only a plain `$var` (php`` |
|        - | 2106 | ``			 * fetches every other shape — `$a[0]`, `$o->p`, `C::$s`, a cast, a`` |
|        - | 2107 | `			 * call — as an R-value, so a by-ref parameter binds its elements in` |
|        - | 2108 | `			 * a temporary and the write-back is invisible). The expander needs` |
|        - | 2109 | `			 * the distinction because it carries each element's slot for the` |
|        - | 2110 | ``			 * by-ref binder; without it `r(...$a[0])` wrote through to the real`` |
|        - | 2111 | `			 * element, which php leaves alone. */` |
|      379 | 2112 | `			PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD,` |
|      270 | 2113 | `				(apNode[n]->pOp == 0 && apNode[n]->xCode == PH7_CompileVariable) ? 1 : 0,` |
|        - | 2114 | `				0, 0, 0);` |
|      274 | 2115 | `			hasSpread = 1;` |
|      135 | 2116 | `		}` |
|   453897 | 2117 | `	}` |
|        - | 2118 | `	/* Total number of given arguments */` |
|   672777 | 2119 | `	iP1 = nArgs;` |
|   672777 | 2120 | `	iP2 = hasSpread;` |
|        - | 2121 | `	/* Build VmCallArgMap if named arguments are present.` |
|        - | 2122 | `	 * Deep-copy name strings so they survive token stream cleanup. */` |
|   672777 | 2123 | `	if( hasNamed ){` |
|      379 | 2124 | `		sxu32 nStrBytes = 0;` |
|        - | 2125 | `		char *zBuf;` |
|     1107 | 2126 | `		for( n = 0; n < nArgs; ++n ){` |
|      733 | 2127 | `			if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      587 | 2128 | `				nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|      291 | 2129 | `			}` |
|      369 | 2130 | `		}` |
|        - | 2131 | `		{` |
|      379 | 2132 | `		sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|      379 | 2133 | `		VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      374 | 2134 | `			&pGen->pVm->sAllocator, mapSize);` |
|      379 | 2135 | `		if( pMap ){` |
|      379 | 2136 | `			SyZero(pMap, mapSize);` |
|      379 | 2137 | `			pMap->bHasNamed = 1;` |
|      379 | 2138 | `			pMap->nTotal = (sxu32)nArgs;` |
|      379 | 2139 | `			pMap->aNames = (SyString *)&pMap[1];` |
|      379 | 2140 | `			zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|     1107 | 2141 | `			for( n = 0; n < nArgs; ++n ){` |
|      733 | 2142 | `				if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      587 | 2143 | `					sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|      587 | 2144 | `					SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|      587 | 2145 | `					SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|      587 | 2146 | `					zBuf += nb;` |
|      291 | 2147 | `				}` |
|        - | 2148 | `				/* else: aNames[n] remains {NULL, 0} for positional */` |
|      369 | 2149 | `			}` |
|      379 | 2150 | `			p3 = (void *)pMap;` |
|      187 | 2151 | `		}` |
|        - | 2152 | `		}` |
|      187 | 2153 | `	}` |
|        - | 2154 | `	/* assert(): php's compiler keeps a copy of the assertion's AST and a` |
|        - | 2155 | ``	 * failing assert reports its rendered SOURCE (`assert(1 == 2)`), not the`` |
|        - | 2156 | `	 * evaluated value. Render the first argument's token span` |
|        - | 2157 | `	 * into the call map so vm_builtin_assert can echo it. Only a DIRECT` |
|        - | 2158 | `	 * unqualified/absolute call qualifies — matching php, an indirect call` |
|        - | 2159 | `	 * (call_user_func, a callable variable) has no source text and its` |
|        - | 2160 | `	 * AssertionError carries an empty message. A spread first argument is` |
|        - | 2161 | `	 * skipped (its span is the unpacked array, not the assertion). */` |
|   672772 | 2162 | `	if( nArgs >= 1 && !bFcc` |
|   636917 | 2163 | `	 && (apNode[0]->iFlags & EXPR_NODE_SPREAD) == 0 ){` |
|        - | 2164 | `		SyString sCallee;` |
|   636703 | 2165 | `		GenStateCallBuiltinName(pNode->pLeft,&sCallee);` |
|   636698 | 2166 | `		if( sCallee.nByte == sizeof("assert")-1` |
|   397942 | 2167 | `		 && SyStrnicmp(sCallee.zString,"assert",sizeof("assert")-1) == 0 ){` |
|        - | 2168 | `			/* An operator root's pStart/pEnd name only the operator token` |
|        - | 2169 | ``			 * (`1 == 2` roots at `==`); the subtree walk recovers the whole`` |
|        - | 2170 | `			 * raw extent, re-adding parens the grouping pass consumed. */` |
|       67 | 2171 | `			SyToken *pSpanIn = 0;` |
|       67 | 2172 | `			SyToken *pSpanEnd = 0;` |
|        - | 2173 | `			SyBlob sSrc;` |
|       67 | 2174 | `			PH7_ExprSubtreeSpan(apNode[0],&pSpanIn,&pSpanEnd);` |
|       67 | 2175 | `			SyBlobInit(&sSrc,&pGen->pVm->sAllocator);` |
|       67 | 2176 | `			if( pSpanIn && pSpanEnd && pSpanIn < pSpanEnd ){` |
|       67 | 2177 | `				if( apNode[0]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|        - | 2178 | ``					/* php renders the name too: `assert(assertion: 1 == 2)`. */`` |
|        3 | 2179 | `					SyBlobAppend(&sSrc,apNode[0]->sArgName.zString,apNode[0]->sArgName.nByte);` |
|        3 | 2180 | `					SyBlobAppend(&sSrc,": ",2);` |
|        1 | 2181 | `				}` |
|       67 | 2182 | `				PH7_GenRenderAssertSpan(pGen,pSpanIn,pSpanEnd,&sSrc);` |
|       31 | 2183 | `			}` |
|       67 | 2184 | `			if( SyBlobLength(&sSrc) > 0 ){` |
|       98 | 2185 | `				char *zDup = (char *)SyMemBackendDup(&pGen->pVm->sAllocator,` |
|       62 | 2186 | `					SyBlobData(&sSrc),SyBlobLength(&sSrc));` |
|       67 | 2187 | `				if( zDup ){` |
|       67 | 2188 | `					if( p3 == 0 ){` |
|       65 | 2189 | `						VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|       60 | 2190 | `							&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|       65 | 2191 | `						if( pMap ){` |
|       65 | 2192 | `							SyZero(pMap,sizeof(VmCallArgMap));` |
|       65 | 2193 | `							p3 = (void *)pMap;` |
|       30 | 2194 | `						}` |
|       30 | 2195 | `					}` |
|       67 | 2196 | `					if( p3 ){` |
|       67 | 2197 | `						SyStringInitFromBuf(&((VmCallArgMap *)p3)->sAssertSrc,` |
|        - | 2198 | `							zDup,SyBlobLength(&sSrc));` |
|       31 | 2199 | `					}` |
|       31 | 2200 | `				}` |
|       31 | 2201 | `			}` |
|       67 | 2202 | `			SyBlobRelease(&sSrc);` |
|       31 | 2203 | `		}` |
|   318349 | 2204 | `	}` |
|        - | 2205 | `	/* Record each argument's compile-time SHAPE so the by-ref binders can` |
|        - | 2206 | `	 * refuse a non-variable where php refuses it — at the CALL, before the` |
|        - | 2207 | `	 * callee's ZPP runs. Skipped when the call SPREADS (one compile-time` |
|        - | 2208 | `	 * argument becomes N runtime slots, so the positions no longer line up)` |
|        - | 2209 | `	 * or when it carries more arguments than the masks can hold; a call` |
|        - | 2210 | `	 * without the flag keeps the old runtime nIdx test. Named arguments are` |
|        - | 2211 | `	 * fine: they change which FORMAL a slot binds to, not the slot's index. */` |
|   672777 | 2212 | `	if( !bAnySpread && nArgs > 0 && nArgs <= 31 && !bFcc ){` |
|   636661 | 2213 | `		sxu32 nNonLval = 0;` |
|   636661 | 2214 | `		sxu32 nTempCall = 0;` |
|  1544089 | 2215 | `		for( n = 0 ; n < nArgs ; ++n ){` |
|   907433 | 2216 | `			int iShape = GenStateArgShape(apNode[n]);` |
|   907433 | 2217 | `			if( iShape == GEN_ARG_NONE ){` |
|   323955 | 2218 | `				nNonLval \|= (1u << n);` |
|   745458 | 2219 | `			}else if( iShape == GEN_ARG_TEMPCALL ){` |
|    40399 | 2220 | `				nTempCall \|= (1u << n);` |
|    20197 | 2221 | `			}` |
|   453719 | 2222 | `		}` |
|   636661 | 2223 | `		if( p3 == 0 ){` |
|   636191 | 2224 | `			VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|   636186 | 2225 | `				&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|   636191 | 2226 | `			if( pMap ){` |
|   636191 | 2227 | `				SyZero(pMap,sizeof(VmCallArgMap));` |
|   636191 | 2228 | `				p3 = (void *)pMap;` |
|   318093 | 2229 | `			}` |
|   318093 | 2230 | `		}` |
|   636661 | 2231 | `		if( p3 ){` |
|   636661 | 2232 | `			((VmCallArgMap *)p3)->bArgShapes = 1;` |
|   636661 | 2233 | `			((VmCallArgMap *)p3)->nNonLvalMask = nNonLval;` |
|   636661 | 2234 | `			((VmCallArgMap *)p3)->nTempCallMask = nTempCall;` |
|   318328 | 2235 | `		}` |
|   318328 | 2236 | `	}` |
|   672777 | 2237 | `	pArgs->iP1 = iP1;` |
|   672777 | 2238 | `	pArgs->iP2 = iP2;` |
|   672777 | 2239 | `	pArgs->p3  = p3;` |
|   672777 | 2240 | `	pArgs->bFcc = bFcc;` |
|   672777 | 2241 | `	pArgs->bAnySpread = bAnySpread;` |
|   672777 | 2242 | `	return SXRET_OK;` |
|   336395 | 2243 | `}` |
|        - | 2244 | `/*` |
|        - | 2245 | ` * Compile a PHP expression.` |
|        - | 2246 | ` * According to the PHP language reference manual:` |
|        - | 2247 | ` *  Expressions are the most important building stones of PHP.` |
|        - | 2248 | ` *  In PHP, almost anything you write is an expression.` |
|        - | 2249 | ` *  The simplest yet most accurate way to define an expression` |
|        - | 2250 | ` *  is "anything that has a value".` |
|        - | 2251 | ` * If something goes wrong while compiling the expression,this` |
|        - | 2252 | ` * function takes care of generating the appropriate error` |
|        - | 2253 | ` * message.` |
|        - | 2254 | ` */` |
|        - | 2255 | `/*` |
|        - | 2256 | ` * Does this expression tree contain a comma OPERATOR node?` |
|        - | 2257 | ` *` |
|        - | 2258 | `` * PH7 shipped `,` as a lowest-precedence binary operator (IMP-0139-COMMA), so`` |
|        - | 2259 | `` * `(1, 2)` and `$x = (f(), $y)` compile and evaluate to the right operand.`` |
|        - | 2260 | ` * php 8 has no comma operator: its grammar only allows comma-separated` |
|        - | 2261 | ` * expression LISTS inside for(...) clauses (call arguments, array literals and` |
|        - | 2262 | ` * list() are split by the parser, never by this node). Accepting it changes the` |
|        - | 2263 | ` * meaning of source php rejects, which §10 classes as a bug — so every context` |
|        - | 2264 | ` * except for() now reports php's parse error.` |
|        - | 2265 | ` */` |
| 25487072 | 2266 | `static int GenStateTreeHasComma(ph7_expr_node *pNode)` |
|        5 | 2267 | `{` |
|        - | 2268 | `	ph7_expr_node **apArg;` |
|        - | 2269 | `	sxu32 n;` |
| 25487077 | 2270 | `	if( pNode == 0 ){` |
| 17978239 | 2271 | `		return 0;` |
|        - | 2272 | `	}` |
|  7508843 | 2273 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|        6 | 2274 | `		return 1;` |
|        - | 2275 | `	}` |
|  7508834 | 2276 | `	if( GenStateTreeHasComma(pNode->pLeft) \|\| GenStateTreeHasComma(pNode->pRight)` |
|  7508835 | 2277 | `	 \|\| GenStateTreeHasComma(pNode->pCond) ){` |
|        6 | 2278 | `		return 1;` |
|        - | 2279 | `	}` |
|  7508835 | 2280 | `	apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  8576643 | 2281 | `	for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; n++ ){` |
|  1067813 | 2282 | `		if( GenStateTreeHasComma(apArg[n]) ){` |
|      ! 0 | 2283 | `			return 1;` |
|        - | 2284 | `		}` |
|   533909 | 2285 | `	}` |
|  7508835 | 2286 | `	return 0;` |
| 12743541 | 2287 | `}` |
|  1986440 | 2288 | `PH7_PRIVATE sxi32 PH7_CompileExpr(` |
|        - | 2289 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 2290 | `	sxi32 iFlags,        /* Control flags */` |
|        - | 2291 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|        - | 2292 | `	)` |
|        5 | 2293 | `{` |
|        - | 2294 | `	ph7_expr_node *pRoot;` |
|        - | 2295 | `	SySet sExprNode;` |
|        - | 2296 | `	SyToken *pEnd;` |
|        - | 2297 | `	sxi32 nExpr;` |
|        - | 2298 | `	sxi32 iNest;` |
|        - | 2299 | `	sxi32 rc;` |
|        - | 2300 | `	sxu32 nNullsafeBase;` |
|        - | 2301 | `	/* Initialize worker variables */` |
|  1986445 | 2302 | `	nExpr = 0;` |
|  1986445 | 2303 | `	pRoot = 0;` |
|        - | 2304 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|        - | 2305 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  1986445 | 2306 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  1986445 | 2307 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  1986445 | 2308 | `	SySetAlloc(&sExprNode,0x10);` |
|  1986445 | 2309 | `	rc = SXRET_OK;` |
|        - | 2310 | `	/* Delimit the expression */` |
|  1986445 | 2311 | `	pEnd = pGen->pIn;` |
|  1986445 | 2312 | `	iNest = 0;` |
| 14456343 | 2313 | `	while( pEnd < pGen->pEnd ){` |
| 13596705 | 2314 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|        - | 2315 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     4149 | 2316 | `			iNest++;` |
| 13594633 | 2317 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     4159 | 2318 | `			iNest--;` |
| 13590484 | 2319 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  1134005 | 2320 | `			if( iNest <= 0 ){` |
|  1126807 | 2321 | `				break;` |
|        - | 2322 | `			}` |
|     3599 | 2323 | `		}` |
| 12469903 | 2324 | `		pEnd++;` |
|        5 | 2325 | `	}` |
|  1986445 | 2326 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|     2305 | 2327 | `		SyToken *pEnd2 = pGen->pIn;` |
|     2305 | 2328 | `		iNest = 0;` |
|        - | 2329 | `		/* Stop at the first comma */` |
|    18173 | 2330 | `		while( pEnd2 < pEnd ){` |
|    15891 | 2331 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|      291 | 2332 | `				iNest++;` |
|    15748 | 2333 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|      291 | 2334 | `				iNest--;` |
|    15462 | 2335 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|     6129 | 2336 | `				if( iNest <= 0 ){` |
|       21 | 2337 | `					break;` |
|        - | 2338 | `				}` |
|     3053 | 2339 | `			}` |
|    15873 | 2340 | `			pEnd2++;` |
|        5 | 2341 | `		}` |
|     2305 | 2342 | `		if( pEnd2 <pEnd ){` |
|       21 | 2343 | `			pEnd = pEnd2;` |
|        9 | 2344 | `		}` |
|     1150 | 2345 | `	}` |
|  1986445 | 2346 | `	if( pEnd > pGen->pIn ){` |
|  1986431 | 2347 | `		SyToken *pTmp = pGen->pEnd;` |
|        - | 2348 | `		/* Swap delimiter */` |
|  1986431 | 2349 | `		pGen->pEnd = pEnd;` |
|        - | 2350 | `		/* Try to get an expression tree */` |
|  1986431 | 2351 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  1986426 | 2352 | `		if( rc == SXRET_OK && pRoot && pGen->nCommaExprOk < 1` |
|  1939508 | 2353 | `		 && GenStateTreeHasComma(pRoot) ){` |
|        - | 2354 | `			/* php has no comma operator outside a for() clause */` |
|        6 | 2355 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pRoot->pStart->nLine,` |
|        - | 2356 | `				"syntax error, unexpected token \",\"");` |
|        6 | 2357 | `			pGen->pEnd = pTmp;` |
|        6 | 2358 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2359 | `				SySetRelease(&sExprNode);` |
|      ! 0 | 2360 | `				return SXERR_ABORT;` |
|        - | 2361 | `			}` |
|        6 | 2362 | `			pGen->pIn = pEnd;` |
|        6 | 2363 | `			SySetRelease(&sExprNode);` |
|        6 | 2364 | `			SySetTruncate(&pGen->aNullsafeJmp,nNullsafeBase);` |
|        6 | 2365 | `			return SXRET_OK;` |
|        - | 2366 | `		}` |
|  1986427 | 2367 | `		if( rc == SXRET_OK && pRoot ){` |
|  1986237 | 2368 | `			rc = SXRET_OK;` |
|  1986237 | 2369 | `			if( xTreeValidator ){` |
|        - | 2370 | `				/* Call the upper layer validator callback */` |
|   153045 | 2371 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    76520 | 2372 | `			}` |
|  1986237 | 2373 | `			if( rc != SXERR_ABORT ){` |
|        - | 2374 | `				/* Generate code for the given tree */` |
|  1986237 | 2375 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|        - | 2376 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|        - | 2377 | `				 * expression so they short-circuit to its end. */` |
|  1986237 | 2378 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|   993116 | 2379 | `			}` |
|  1986237 | 2380 | `			nExpr = 1;` |
|   993116 | 2381 | `		}` |
|        - | 2382 | `		/* Release the whole tree */` |
|  1986427 | 2383 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|        - | 2384 | `		/* Synchronize token stream */` |
|  1986427 | 2385 | `		pGen->pEnd = pTmp;` |
|  1986427 | 2386 | `		pGen->pIn  = pEnd;` |
|  1986427 | 2387 | `		if( rc == SXERR_ABORT ){` |
|       59 | 2388 | `			SySetRelease(&sExprNode);` |
|       59 | 2389 | `			return SXERR_ABORT;` |
|        - | 2390 | `		}` |
|   993184 | 2391 | `	}` |
|  1986387 | 2392 | `	SySetRelease(&sExprNode);` |
|  1986387 | 2393 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|   993225 | 2394 | `}` |
|        - | 2395 | `/*` |
|        - | 2396 | ` * Return a pointer to the node construct handler associated` |
|        - | 2397 | ` * with a given node type [i.e: string,integer,float,...].` |
|        - | 2398 | ` */` |
|  1134314 | 2399 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|        5 | 2400 | `{` |
|  1134319 | 2401 | `	if( nNodeType & PH7_TK_NUM ){` |
|        - | 2402 | `		/* Numeric literal: Either real or integer */` |
|   693301 | 2403 | `		return PH7_CompileNumLiteral;` |
|   441023 | 2404 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|        - | 2405 | `		/* Double quoted string */` |
|    49615 | 2406 | `		return PH7_CompileString;` |
|   391413 | 2407 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|        - | 2408 | `		/* Single quoted string */` |
|   391279 | 2409 | `		return PH7_CompileSimpleString;` |
|      139 | 2410 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|        - | 2411 | `		/* Heredoc */` |
|       80 | 2412 | `		return PH7_CompileHereDoc;` |
|       63 | 2413 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|        - | 2414 | `		/* Nowdoc */` |
|       59 | 2415 | `		return PH7_CompileNowDoc;` |
|        6 | 2416 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|        - | 2417 | `		/* Backtick quoted string */` |
|        3 | 2418 | `		return PH7_CompileBacktic;` |
|        - | 2419 | `	}` |
|        3 | 2420 | `	return 0;` |
|   567162 | 2421 | `}` |
|        - | 2422 | `/*` |
|        - | 2423 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|        - | 2424 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|        - | 2425 | ` * in write context" parse error.` |
|        - | 2426 | ` */` |
|      244 | 2427 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|        5 | 2428 | `{` |
|        - | 2429 | `	sxi32 rc;` |
|      249 | 2430 | `	rc = GenStateWriteTargetCheck(&(*pGen),pNode,1 /* unset wording for $this */);` |
|      249 | 2431 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 2432 | `		return rc;` |
|        - | 2433 | `	}` |
|      249 | 2434 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|      247 | 2435 | `		return SXRET_OK;` |
|        - | 2436 | `	}` |
|        5 | 2437 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|        2 | 2438 | `		pNode ? pNode->pStart->nLine : 1,` |
|        - | 2439 | `		"Can't use nullsafe operator in write context");` |
|        3 | 2440 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|      127 | 2441 | `}` |
|        - | 2442 | `/*` |
|        - | 2443 | ` * Compile an unset() statement.` |
|        - | 2444 | ` * unset($var, $arr[$key], ...);` |
|        - | 2445 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|        - | 2446 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|        - | 2447 | ` * parent array before extracting the element to unset.` |
|        - | 2448 | ` */` |
|     3192 | 2449 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|        5 | 2450 | `{` |
|     3197 | 2451 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|     3197 | 2452 | `	sxu32 nIdx = 0;` |
|        - | 2453 | `	SyString sName;` |
|        - | 2454 | `	sxi32 rc;` |
|        - | 2455 | `	/* Jump the 'unset' keyword */` |
|     3197 | 2456 | `	pGen->pIn++;` |
|        - | 2457 | `	/* Save delimiter */` |
|     3197 | 2458 | `	pTmp = pGen->pEnd;` |
|        - | 2459 | `	/* Skip optional opening parenthesis and find the matching close */` |
|     3197 | 2460 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|     3197 | 2461 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        - | 2462 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|        - | 2463 | `		SyToken *pClose;` |
|     3197 | 2464 | `		pGen->pIn++;   /* Skip '(' */` |
|     3197 | 2465 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|     3197 | 2466 | `		pEnd = pClose; /* Stop at ')' */` |
|     1596 | 2467 | `	}` |
|     3197 | 2468 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|        - | 2469 | `	/* Resolve the 'unset' builtin name once */` |
|     3197 | 2470 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|      511 | 2471 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      511 | 2472 | `		if( pObj == 0 ){` |
|      ! 0 | 2473 | `			return SXERR_ABORT;` |
|        - | 2474 | `		}` |
|      511 | 2475 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      511 | 2476 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      253 | 2477 | `	}` |
|        - | 2478 | `	/* Compile each comma-separated argument */` |
|    10953 | 2479 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|     7763 | 2480 | `		if( pGen->pIn < pNext ){` |
|        - | 2481 | `			/*` |
|        - | 2482 | ``			 * A PLAIN variable (`unset($x)`, exactly two tokens: '$' and the name) drops a`` |
|        - | 2483 | `			 * single NAME binding, which the unset() builtin cannot express — all it ever` |
|        - | 2484 | `			 * receives is the value's slot index, and unsetting the slot destroys whatever` |
|        - | 2485 | `			 * else aliases it. Emit OP_UNSET_VAR with the name instead. Subscripts and` |
|        - | 2486 | ``			 * properties (`unset($a[k])`, `unset($o->p)`) keep the existing path, which`` |
|        - | 2487 | `			 * already removes just the element/property.` |
|        - | 2488 | `			 */` |
|     7758 | 2489 | `			if( &pGen->pIn[2] == pNext` |
|     7636 | 2490 | `				&& (pGen->pIn[0].nType & PH7_TK_DOLLAR)` |
|     7519 | 2491 | `				&& (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        - | 2492 | `				SyString *pVarName;` |
|        - | 2493 | ``				/* php refuses `unset($this)` where it is written. The tree validator`` |
|        - | 2494 | `				 * cannot see it — this fast path never builds a tree. */` |
|     7512 | 2495 | `				if( pGen->pIn[1].sData.nByte == sizeof("this")-1` |
|     3989 | 2496 | `				 && SyMemcmp((const void *)pGen->pIn[1].sData.zString,` |
|      228 | 2497 | `				             (const void *)"this",sizeof("this")-1) == 0 ){` |
|        3 | 2498 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 2499 | `						"Cannot unset $this");` |
|        3 | 2500 | `					return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 2501 | `				}` |
|    11270 | 2502 | `				char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     7510 | 2503 | `					pGen->pIn[1].sData.zString,pGen->pIn[1].sData.nByte);` |
|     7515 | 2504 | `				pVarName = (SyString *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(SyString));` |
|     7515 | 2505 | `				if( zDup == 0 \|\| pVarName == 0 ){` |
|      ! 0 | 2506 | `					PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 2507 | `						"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2508 | `					return SXERR_ABORT;` |
|        - | 2509 | `				}` |
|     7515 | 2510 | `				SyStringInitFromBuf(pVarName,zDup,pGen->pIn[1].sData.nByte);` |
|     7515 | 2511 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_UNSET_VAR,0,0,pVarName,0);` |
|     7515 | 2512 | `				pGen->pIn = pNext;` |
|     7515 | 2513 | `				if( pGen->pIn < pEnd ){` |
|     4551 | 2514 | `					pGen->pIn++; /* Jump the trailing comma */` |
|     2273 | 2515 | `				}` |
|     7515 | 2516 | `				continue;` |
|        - | 2517 | `			}` |
|      251 | 2518 | `			pGen->pEnd = pNext;` |
|      251 | 2519 | `			rc = PH7_CompileExpr(&(*pGen),` |
|        - | 2520 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|        - | 2521 | `				GenStateUnsetValidator);` |
|      251 | 2522 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2523 | `				return SXERR_ABORT;` |
|        - | 2524 | `			}` |
|      251 | 2525 | `			if( rc != SXERR_EMPTY ){` |
|        - | 2526 | `				/* Emit call for this single argument */` |
|      249 | 2527 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      249 | 2528 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|      249 | 2529 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      122 | 2530 | `			}` |
|      123 | 2531 | `		}` |
|        - | 2532 | `		/* Jump trailing commas */` |
|      273 | 2533 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|       25 | 2534 | `			pNext++;` |
|        3 | 2535 | `		}` |
|      251 | 2536 | `		pGen->pIn = pNext;` |
|        5 | 2537 | `	}` |
|        - | 2538 | `	/* Skip past the closing ')' if present */` |
|     3195 | 2539 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|     3195 | 2540 | `		pGen->pIn++;` |
|     1595 | 2541 | `	}` |
|        - | 2542 | `	/* Restore token stream */` |
|     3195 | 2543 | `	pGen->pEnd = pTmp;` |
|     3195 | 2544 | `	return SXRET_OK;` |
|     1601 | 2545 | `}` |
|        - | 2546 | `/*` |
|        - | 2547 | ` * PHP Language construct table.` |
|        - | 2548 | ` */` |
|        - | 2549 | `static const LangConstruct aLangConstruct[] = {` |
|        - | 2550 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|        - | 2551 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|        - | 2552 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|        - | 2553 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|        - | 2554 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|        - | 2555 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|        - | 2556 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|        - | 2557 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|        - | 2558 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|        - | 2559 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|        - | 2560 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|        - | 2561 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|        - | 2562 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|        - | 2563 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|        - | 2564 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|        - | 2565 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|        - | 2566 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|        - | 2567 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|        - | 2568 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|        - | 2569 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|        - | 2570 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|        - | 2571 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|        - | 2572 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|        - | 2573 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|        - | 2574 | `};` |
|        - | 2575 | `/*` |
|        - | 2576 | ` * Return a pointer to the statement handler routine associated` |
|        - | 2577 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|        - | 2578 | ` */` |
|   982304 | 2579 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|        - | 2580 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|        - | 2581 | `	SyToken *pLookahed  /* Look-ahead token */` |
|        - | 2582 | `	)` |
|        5 | 2583 | `{` |
|   982309 | 2584 | `	sxu32 n = 0;` |
|  2984579 | 2585 | `	for(;;){` |
|  5969163 | 2586 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|     4361 | 2587 | `			break;` |
|        - | 2588 | `		}` |
|  5964807 | 2589 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|   977953 | 2590 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|      ! 0 | 2591 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|      ! 0 | 2592 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|        - | 2593 | `					/* 'static' (class context),return null */` |
|      ! 0 | 2594 | `					return 0;` |
|        - | 2595 | `				}` |
|      ! 0 | 2596 | `			}` |
|   977948 | 2597 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       32 | 2598 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       23 | 2599 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|        - | 2600 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|        3 | 2601 | `				return 0;` |
|        - | 2602 | `			}` |
|        - | 2603 | `			/* Return a pointer to the handler.` |
|        - | 2604 | `			*/` |
|   977951 | 2605 | `			return aLangConstruct[n].xConstruct;` |
|        - | 2606 | `		}` |
|  4986859 | 2607 | `		n++;` |
|        5 | 2608 | `	}` |
|     4361 | 2609 | `	if( pLookahed ){` |
|     4361 | 2610 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|      193 | 2611 | `			return PH7_CompileClassInterface;` |
|     4173 | 2612 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|     3215 | 2613 | `			return PH7_CompileClass;` |
|      963 | 2614 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      197 | 2615 | `			return PH7_CompileTrait;` |
|        - | 2616 | `		}` |
|        - | 2617 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|        - | 2618 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|        - | 2619 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|        - | 2620 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|      383 | 2621 | `	}` |
|        - | 2622 | `	/* Not a language construct */` |
|      771 | 2623 | `	return 0;` |
|   491157 | 2624 | `}` |
|        - | 2625 | `/*` |
|        - | 2626 | ` * Check if the given keyword is in fact a PHP language construct.` |
|        - | 2627 | ` * Return TRUE on success. FALSE otheriwse.` |
|        - | 2628 | ` */` |
|      768 | 2629 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|        5 | 2630 | `{` |
|        - | 2631 | `	int rc;` |
|      773 | 2632 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|      773 | 2633 | `	if( rc == FALSE ){` |
|      558 | 2634 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|      456 | 2635 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|        - | 2636 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|        - | 2637 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|        - | 2638 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|        - | 2639 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|        - | 2640 | `			*/` |
|        - | 2641 | `			){` |
|      555 | 2642 | `				rc = TRUE;` |
|      275 | 2643 | `		}` |
|      279 | 2644 | `	}` |
|      773 | 2645 | `	return rc;` |
|        5 | 2646 | `}` |
|        - | 2647 | `/*` |
|        - | 2648 | ` * Compile a PHP chunk.` |
|        - | 2649 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|        - | 2650 | ` * takes care of generating the appropriate error message.` |
|        - | 2651 | ` */` |
|        - | 2652 | `/*` |
|        - | 2653 | ` * Update pGen->sPendingDoc for the statement whose first token is` |
|        - | 2654 | ` * pGen->pIn: when a docblock trivia is keyed to that token's index in` |
|        - | 2655 | ` * the chunk token set it becomes the pending docblock. An existing` |
|        - | 2656 | ` * pending docblock is LEFT in place otherwise: Zend keeps the last-seen` |
|        - | 2657 | ` * doc comment until a declaration consumes it, so a docblock survives` |
|        - | 2658 | ` * intervening non-declaration statements.` |
|        - | 2659 | ` */` |
|  1736370 | 2660 | `PH7_PRIVATE void GenStateSetPendingDoc(ph7_gen_state *pGen)` |
|        5 | 2661 | `{` |
|  1736375 | 2662 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|  1736375 | 2663 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|  1736375 | 2664 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|        - | 2665 | `	sxu32 nIdx, n;` |
|  1736370 | 2666 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|     4527 | 2667 | `	 \|\| pGen->pIn < pBase \|\| pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|        - | 2668 | `		/* Re-tokenized substream (string interpolation, synthesized code):` |
|        - | 2669 | `		 * indexes do not map to the sidecar */` |
|  1731853 | 2670 | `		return;` |
|        - | 2671 | `	}` |
|     4527 | 2672 | `	nIdx = (sxu32)(pGen->pIn - pBase);` |
|        - | 2673 | `	/* Attributes must be adjacent to their declaration (unlike docblocks):` |
|        - | 2674 | `	 * reset at every boundary, then collect the groups keyed to this token. */` |
|     4527 | 2675 | `	SySetReset(&pGen->aPendingAttrs);` |
|    15439 | 2676 | `	for( n = 0 ; n < nT ; n++ ){` |
|    10917 | 2677 | `		if( aT[n].nTokIdx != nIdx ){` |
|    10639 | 2678 | `			continue;` |
|        - | 2679 | `		}` |
|      283 | 2680 | `		if( aT[n].iKind == PH7_TRIVIA_DOC ){` |
|       65 | 2681 | `			pGen->sPendingDoc = aT[n].sText;` |
|      253 | 2682 | `		}else if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|      223 | 2683 | `			SySetPut(&pGen->aPendingAttrs,(const void *)&aT[n]);` |
|      109 | 2684 | `		}` |
|      144 | 2685 | `	}` |
|   868190 | 2686 | `}` |
|        - | 2687 | `/*` |
|        - | 2688 | ` * Hand the pending docblock (if any) to a declaration: duplicate it into` |
|        - | 2689 | ` * the VM allocator (the raw script buffer dies after compilation) and` |
|        - | 2690 | ` * clear the pending slot so sibling declarations do not inherit it.` |
|        - | 2691 | ` */` |
|   159590 | 2692 | `PH7_PRIVATE void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut)` |
|        5 | 2693 | `{` |
|        - | 2694 | `	char *zDup;` |
|   159595 | 2695 | `	if( SyStringLength(&pGen->sPendingDoc) < 1 ){` |
|   159539 | 2696 | `		return;` |
|        - | 2697 | `	}` |
|       89 | 2698 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       28 | 2699 | `		SyStringData(&pGen->sPendingDoc),SyStringLength(&pGen->sPendingDoc));` |
|       61 | 2700 | `	if( zDup ){` |
|       61 | 2701 | `		SyStringInitFromBuf(pOut,zDup,SyStringLength(&pGen->sPendingDoc));` |
|       28 | 2702 | `	}` |
|       61 | 2703 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|    79800 | 2704 | `}` |
|        - | 2705 | `/*` |
|        - | 2706 | ` * Compile one recorded #[...] attribute group (the span between the group` |
|        - | 2707 | ` * delimiters) into ph7_attribute records appended to pOut. The span is` |
|        - | 2708 | ` * duplicated into the VM allocator FIRST (compiled bytecode and interned` |
|        - | 2709 | ` * names may point into the token text, which must outlive the raw script` |
|        - | 2710 | ` * buffer), then re-tokenized on its own. Each argument expression compiles` |
|        - | 2711 | ` * with the container-swap idiom into its own OP_DONE-terminated set,` |
|        - | 2712 | ` * evaluated lazily at ReflectionAttribute time (PHP semantics).` |
|        - | 2713 | ` */` |
|      232 | 2714 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut)` |
|        5 | 2715 | `{` |
|        - | 2716 | `	SySet *pToken;` |
|        - | 2717 | `	SyToken *pIn, *pEnd, *pSavedIn, *pSavedEnd;` |
|        - | 2718 | `	char *zSpan;` |
|      237 | 2719 | `	sxi32 rc = SXRET_OK;` |
|      237 | 2720 | `	if( SyStringLength(&pTrivia->sText) < 1 ){` |
|      ! 0 | 2721 | `		return SXRET_OK;` |
|        - | 2722 | `	}` |
|      353 | 2723 | `	zSpan = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      116 | 2724 | `		SyStringData(&pTrivia->sText),SyStringLength(&pTrivia->sText));` |
|      237 | 2725 | `	if( zSpan == 0 ){` |
|      ! 0 | 2726 | `		return SXRET_OK;` |
|        - | 2727 | `	}` |
|        - | 2728 | `	/* The token set must outlive compilation too: interned operands may` |
|        - | 2729 | `	 * reference token payloads. Pool-allocated, never released — bounded by` |
|        - | 2730 | `	 * the number of attribute declarations in the program. */` |
|      237 | 2731 | `	pToken = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      237 | 2732 | `	if( pToken == 0 ){` |
|      ! 0 | 2733 | `		return SXRET_OK;` |
|        - | 2734 | `	}` |
|      237 | 2735 | `	SySetInit(pToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      237 | 2736 | `	PH7_TokenizePHP(zSpan,SyStringLength(&pTrivia->sText),pTrivia->nLine,pToken,0);` |
|      237 | 2737 | `	pIn = (SyToken *)SySetBasePtr(pToken);` |
|      237 | 2738 | `	pEnd = &pIn[SySetUsed(pToken)];` |
|      237 | 2739 | `	pSavedIn = pGen->pIn;` |
|      237 | 2740 | `	pSavedEnd = pGen->pEnd;` |
|      241 | 2741 | `	while( pIn < pEnd ){` |
|        - | 2742 | `		ph7_attribute sAttr;` |
|        - | 2743 | `		SyBlob sFQN;` |
|      241 | 2744 | `		int bAbsolute = 0;` |
|      241 | 2745 | `		SyZero(&sAttr,sizeof(sAttr));` |
|      241 | 2746 | `		SySetInit(&sAttr.aArgs,&pGen->pVm->sAllocator,sizeof(ph7_attr_arg));` |
|      241 | 2747 | `		sAttr.nLine = pIn->nLine;` |
|      241 | 2748 | `		if( pIn->nType & PH7_TK_NSSEP ){` |
|       89 | 2749 | `			bAbsolute = 1;` |
|       89 | 2750 | `			pIn++;` |
|       42 | 2751 | `		}` |
|      241 | 2752 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        - | 2753 | ``		/* `#[namespace\Attr]` — the current namespace, absolute from there. */`` |
|      241 | 2754 | `		if( !bAbsolute && GenStateNsRelPrefix(pGen,&pIn,pEnd,&sFQN) ){` |
|      ! 0 | 2755 | `			bAbsolute = 1;` |
|      ! 0 | 2756 | `		}` |
|      241 | 2757 | `		while( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      241 | 2758 | `			SyBlobAppend(&sFQN,pIn->sData.zString,pIn->sData.nByte);` |
|      241 | 2759 | `			pIn++;` |
|      241 | 2760 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|      ! 0 | 2761 | `				SyBlobAppend(&sFQN,"\\",1);` |
|      ! 0 | 2762 | `				pIn++;` |
|      ! 0 | 2763 | `				continue;` |
|        - | 2764 | `			}` |
|      241 | 2765 | `			break;` |
|      ! 0 | 2766 | `		}` |
|      241 | 2767 | `		if( SyBlobLength(&sFQN) < 1 ){` |
|        - | 2768 | `			/* Malformed group: stop quietly (the group was inert trivia before` |
|        - | 2769 | `			 * this feature; never turn it into a new fatal) */` |
|      ! 0 | 2770 | `			SyBlobRelease(&sFQN);` |
|      ! 0 | 2771 | `			break;` |
|        - | 2772 | `		}` |
|        - | 2773 | `		/* Resolve to an FQN: absolute names verbatim; else use-import alias,` |
|        - | 2774 | `		 * else current-namespace prefix (PHP attribute name resolution) */` |
|        - | 2775 | `		{` |
|      241 | 2776 | `			const char *zName = (const char *)SyBlobData(&sFQN);` |
|      241 | 2777 | `			sxu32 nName = SyBlobLength(&sFQN);` |
|      241 | 2778 | `			char *zDup = 0;` |
|      241 | 2779 | `			if( !bAbsolute ){` |
|      155 | 2780 | `				SyHashEntry *pImp = SyHashGet(&pGen->hUseImports,(const void *)zName,nName);` |
|      155 | 2781 | `				if( pImp ){` |
|        3 | 2782 | `					const char *zFqn = (const char *)pImp->pUserData;` |
|        3 | 2783 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFqn,SyStrlen(zFqn));` |
|        3 | 2784 | `					if( zDup ){` |
|        3 | 2785 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyStrlen(zDup));` |
|        2 | 2786 | `					}` |
|      153 | 2787 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        - | 2788 | `					SyBlob sTmp;` |
|      ! 0 | 2789 | `					SyBlobInit(&sTmp,&pGen->pVm->sAllocator);` |
|      ! 0 | 2790 | `					SyBlobAppend(&sTmp,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      ! 0 | 2791 | `					SyBlobAppend(&sTmp,"\\",1);` |
|      ! 0 | 2792 | `					SyBlobAppend(&sTmp,zName,nName);` |
|      ! 0 | 2793 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      ! 0 | 2794 | `						(const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|      ! 0 | 2795 | `					if( zDup ){` |
|      ! 0 | 2796 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyBlobLength(&sTmp));` |
|      ! 0 | 2797 | `					}` |
|      ! 0 | 2798 | `					SyBlobRelease(&sTmp);` |
|      ! 0 | 2799 | `				}` |
|       76 | 2800 | `			}` |
|      241 | 2801 | `			if( SyStringLength(&sAttr.sName) < 1 ){` |
|      239 | 2802 | `				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|      239 | 2803 | `				if( zDup ){` |
|      239 | 2804 | `					SyStringInitFromBuf(&sAttr.sName,zDup,nName);` |
|      117 | 2805 | `				}` |
|      117 | 2806 | `			}` |
|        - | 2807 | `		}` |
|      241 | 2808 | `		SyBlobRelease(&sFQN);` |
|      241 | 2809 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|        - | 2810 | `			SyToken *pArgsEnd;` |
|       95 | 2811 | `			pIn++;` |
|       95 | 2812 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pArgsEnd);` |
|      261 | 2813 | `			while( pIn < pArgsEnd ){` |
|      169 | 2814 | `				SyToken *pArgStart = pIn, *pArgStop = pIn;` |
|      169 | 2815 | `				sxi32 iDepth = 0;` |
|        - | 2816 | `				ph7_attr_arg sArgRec;` |
|      577 | 2817 | `				while( pArgStop < pArgsEnd ){` |
|      487 | 2818 | `					if( pArgStop->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       29 | 2819 | `						iDepth++;` |
|      473 | 2820 | `					}else if( pArgStop->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       29 | 2821 | `						iDepth--;` |
|      445 | 2822 | `					}else if( (pArgStop->nType & PH7_TK_COMMA) && iDepth == 0 ){` |
|       77 | 2823 | `						break;` |
|        - | 2824 | `					}` |
|      411 | 2825 | `					pArgStop++;` |
|        3 | 2826 | `				}` |
|      169 | 2827 | `				SyZero(&sArgRec,sizeof(sArgRec));` |
|      169 | 2828 | `				SySetInit(&sArgRec.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      166 | 2829 | `				if( pArgStart < pArgStop && (pArgStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|      115 | 2830 | `				 && &pArgStart[1] < pArgStop && (pArgStart[1].nType & PH7_TK_COLON) ){` |
|       37 | 2831 | `					char *zN = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       12 | 2832 | `						pArgStart->sData.zString,pArgStart->sData.nByte);` |
|       25 | 2833 | `					if( zN ){` |
|       25 | 2834 | `						SyStringInitFromBuf(&sArgRec.sName,zN,pArgStart->sData.nByte);` |
|       12 | 2835 | `					}` |
|       25 | 2836 | `					pArgStart += 2;` |
|       12 | 2837 | `				}` |
|      169 | 2838 | `				if( pArgStart < pArgStop ){` |
|        - | 2839 | `					SySet *pInstrContainer;` |
|      169 | 2840 | `					pGen->pIn = pArgStart;` |
|      169 | 2841 | `					pGen->pEnd = pArgStop;` |
|      169 | 2842 | `					pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      169 | 2843 | `					PH7_VmSetByteCodeContainer(pGen->pVm,&sArgRec.aByteCode);` |
|      169 | 2844 | `					rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      169 | 2845 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      169 | 2846 | `					PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      169 | 2847 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 2848 | `						pGen->pIn = pSavedIn;` |
|      ! 0 | 2849 | `						pGen->pEnd = pSavedEnd;` |
|      ! 0 | 2850 | `						return SXERR_ABORT;` |
|        - | 2851 | `					}` |
|      169 | 2852 | `					SySetPut(&sAttr.aArgs,(const void *)&sArgRec);` |
|       83 | 2853 | `				}` |
|      169 | 2854 | `				pIn = pArgStop;` |
|      169 | 2855 | `				if( pIn < pArgsEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|       77 | 2856 | `					pIn++;` |
|       38 | 2857 | `				}` |
|        3 | 2858 | `			}` |
|       95 | 2859 | `			pIn = (pArgsEnd < pEnd) ? &pArgsEnd[1] : pEnd;` |
|       46 | 2860 | `		}` |
|      241 | 2861 | `		SySetPut(pOut,(const void *)&sAttr);` |
|      241 | 2862 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        5 | 2863 | `			pIn++;` |
|        5 | 2864 | `			continue;` |
|        - | 2865 | `		}` |
|      237 | 2866 | `		break;` |
|      ! 0 | 2867 | `	}` |
|      237 | 2868 | `	pGen->pIn = pSavedIn;` |
|      237 | 2869 | `	pGen->pEnd = pSavedEnd;` |
|      237 | 2870 | `	return SXRET_OK;` |
|      121 | 2871 | `}` |
|        - | 2872 | `/*` |
|        - | 2873 | ` * Hand the pending attribute groups (if any) to a declaration: compile` |
|        - | 2874 | ` * every recorded group into pOut and clear the pending list.` |
|        - | 2875 | ` */` |
|   159596 | 2876 | `PH7_PRIVATE sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut)` |
|        5 | 2877 | `{` |
|   159601 | 2878 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|        - | 2879 | `	sxu32 n;` |
|        - | 2880 | `	sxi32 rc;` |
|   159819 | 2881 | `	for( n = 0 ; n < SySetUsed(&pGen->aPendingAttrs) ; n++ ){` |
|      223 | 2882 | `		rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|      223 | 2883 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2884 | `			return SXERR_ABORT;` |
|        - | 2885 | `		}` |
|      114 | 2886 | `	}` |
|   159601 | 2887 | `	SySetReset(&pGen->aPendingAttrs);` |
|   159601 | 2888 | `	return SXRET_OK;` |
|    79803 | 2889 | `}` |
|        - | 2890 | `/*` |
|        - | 2891 | ` * Compile the attribute groups keyed to the given token (a parameter's` |
|        - | 2892 | ` * first token inside a signature) into pOut. Parameters are parsed from` |
|        - | 2893 | ` * the main token stream, so the sidecar indexes map directly.` |
|        - | 2894 | ` */` |
|   280882 | 2895 | `PH7_PRIVATE sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut)` |
|        5 | 2896 | `{` |
|   280887 | 2897 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|   280887 | 2898 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|   280887 | 2899 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|        - | 2900 | `	sxu32 nIdx, n;` |
|        - | 2901 | `	sxi32 rc;` |
|   280882 | 2902 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|      585 | 2903 | `	 \|\| pTok < pBase \|\| pTok >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|   280307 | 2904 | `		return SXRET_OK;` |
|        - | 2905 | `	}` |
|      585 | 2906 | `	nIdx = (sxu32)(pTok - pBase);` |
|     1699 | 2907 | `	for( n = 0 ; n < nT ; n++ ){` |
|     1119 | 2908 | `		if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|       16 | 2909 | `			rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|       16 | 2910 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2911 | `				return SXERR_ABORT;` |
|        - | 2912 | `			}` |
|        7 | 2913 | `		}` |
|      562 | 2914 | `	}` |
|      585 | 2915 | `	return SXRET_OK;` |
|   140446 | 2916 | `}` |
|  1539616 | 2917 | `PH7_PRIVATE sxi32 GenStateCompileChunk(` |
|        - | 2918 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 2919 | `	sxi32 iFlags         /* Compile flags */` |
|        - | 2920 | `	)` |
|        5 | 2921 | `{` |
|        - | 2922 | `	ProcLangConstruct xCons;` |
|        - | 2923 | `	sxi32 rc;` |
|  1539621 | 2924 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|   977232 | 2925 | `	for(;;){` |
|  1747045 | 2926 | `		int bStmtIsDeclare = 0;` |
|  1747045 | 2927 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 2928 | `			/* No more input to process */` |
|    16911 | 2929 | `			break;` |
|        - | 2930 | `		}` |
|        - | 2931 | `		/* Bind a directly-preceding docblock to this statement */` |
|  1730139 | 2932 | `		GenStateSetPendingDoc(&(*pGen));` |
|  1730139 | 2933 | `		if( SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|        - | 2934 | `			/* php: a statement-position attribute group must be followed by a` |
|        - | 2935 | ``			 * declaration (function/class-like/const) — `#[A] $x = 1;` is a`` |
|        - | 2936 | `` 			 * parse error, never a silent discard. `static`/`fn`/`function` `` |
|        - | 2937 | ``			 * cover bare closure-expression statements; `readonly`/`enum` are`` |
|        - | 2938 | `			 * context-sensitive IDs handled by the modified-class/enum scans. */` |
|      107 | 2939 | `			int bAttrTarget = 0;` |
|      104 | 2940 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd)` |
|      107 | 2941 | `			 \|\| GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|      ! 0 | 2942 | `				bAttrTarget = 1;` |
|      107 | 2943 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      107 | 2944 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      104 | 2945 | `				if( nKw == PH7_TKWRD_FUNCTION \|\| nKw == PH7_TKWRD_CLASS` |
|       39 | 2946 | `				 \|\| nKw == PH7_TKWRD_INTERFACE \|\| nKw == PH7_TKWRD_TRAIT` |
|        6 | 2947 | `				 \|\| nKw == PH7_TKWRD_ABSTRACT \|\| nKw == PH7_TKWRD_FINAL` |
|        6 | 2948 | `				 \|\| nKw == PH7_TKWRD_CONST \|\| nKw == PH7_TKWRD_STATIC` |
|        3 | 2949 | `				 \|\| nKw == PH7_TKWRD_FN ){` |
|      107 | 2950 | `					bAttrTarget = 1;` |
|       52 | 2951 | `				}` |
|       52 | 2952 | `			}` |
|      107 | 2953 | `			if( !bAttrTarget ){` |
|      ! 0 | 2954 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 2955 | `					"syntax error, unexpected token \"%z\" after attribute group; expecting a declaration",` |
|      ! 0 | 2956 | `					&pGen->pIn->sData);` |
|      ! 0 | 2957 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2958 | `					break;` |
|        - | 2959 | `				}` |
|      ! 0 | 2960 | `				SySetReset(&pGen->aPendingAttrs);` |
|      ! 0 | 2961 | `			}` |
|       52 | 2962 | `		}` |
|        - | 2963 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|        - | 2964 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  1730139 | 2965 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   982423 | 2966 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   982423 | 2967 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|       61 | 2968 | `				bStmtIsDeclare = 1;` |
|       28 | 2969 | `			}` |
|   491209 | 2970 | `		}` |
|  1730139 | 2971 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|        - | 2972 | `			/* Any non-declare top-level statement locks the strict_types` |
|        - | 2973 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|   207449 | 2974 | `			pGen->bStrictTypesLocked = 1;` |
|   103722 | 2975 | `		}` |
|  1730139 | 2976 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|        - | 2977 | `			/* Compile block */` |
|       59 | 2978 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|       59 | 2979 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2980 | `				break;` |
|        - | 2981 | `			}` |
|       32 | 2982 | `		}else{` |
|  1730085 | 2983 | `			xCons = 0;` |
|  1730085 | 2984 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|        - | 2985 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|        - | 2986 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|        - | 2987 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|      143 | 2988 | `				xCons = PH7_CompileClassModifiers;` |
|  1730016 | 2989 | `			}else if( GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|        - | 2990 | ``				/* `enum Name …` (PHP 8.1) — `enum` is a context-sensitive ID,`` |
|        - | 2991 | `				 * so it is detected here rather than the keyword dispatcher. */` |
|      107 | 2992 | `				xCons = PH7_CompileEnum;` |
|  1729896 | 2993 | `			}else if( GenStateIsNsRelName(pGen->pIn,pGen->pEnd) ){` |
|        - | 2994 | ``				/* A statement that STARTS with php's `namespace\X` name operator`` |
|        - | 2995 | ``				 * (`namespace\Cee::m();`) is an expression, not a namespace`` |
|        - | 2996 | ``				 * DECLARATION — the glued `\` is what tells the two apart. */`` |
|        7 | 2997 | `				xCons = 0;` |
|  1729842 | 2998 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   982309 | 2999 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        - | 3000 | `				/* Try to extract a language construct handler */` |
|   982309 | 3001 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|   982309 | 3002 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|       13 | 3003 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3004 | `						"Syntax error: Unexpected keyword '%z'",` |
|        8 | 3005 | `						&pGen->pIn->sData);` |
|        9 | 3006 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3007 | `						break;` |
|        - | 3008 | `					}` |
|        - | 3009 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|        - | 3010 | `					 * this erroneous statement.` |
|        - | 3011 | `					 */` |
|        9 | 3012 | `					xCons = PH7_ErrorRecover;` |
|        4 | 3013 | `				}` |
|  1238687 | 3014 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|    74957 | 3015 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|        - | 3016 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|      217 | 3017 | `				xCons = PH7_CompileLabel;` |
|      106 | 3018 | `			}` |
|  1730085 | 3019 | `			if( xCons == 0 ){` |
|        - | 3020 | `				/* Assume an expression an try to compile it */` |
|   748089 | 3021 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   748089 | 3022 | `				if(  rc != SXERR_EMPTY ){` |
|        - | 3023 | `					/* Pop l-value */` |
|   747925 | 3024 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|   373960 | 3025 | `				}` |
|   374047 | 3026 | `			}else{` |
|        - | 3027 | `				/* Go compile the sucker */` |
|   982001 | 3028 | `				rc = xCons(&(*pGen));` |
|        - | 3029 | `			}` |
|  1730085 | 3030 | `			if( rc == SXERR_ABORT ){` |
|        - | 3031 | `				/* Request to abort compilation */` |
|       81 | 3032 | `				break;` |
|        - | 3033 | `			}` |
|        - | 3034 | `		}` |
|        - | 3035 | `		/* Ignore trailing semi-colons ';' */` |
|  2863953 | 3036 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  1133895 | 3037 | `			pGen->pIn++;` |
|        5 | 3038 | `		}` |
|  1730063 | 3039 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|        - | 3040 | `			/* Compile a single statement and return */` |
|  1522639 | 3041 | `			break;` |
|        - | 3042 | `		}` |
|        - | 3043 | `		/* LOOP ONE */` |
|        - | 3044 | `		/* LOOP TWO */` |
|        - | 3045 | `		/* LOOP THREE */` |
|        - | 3046 | `		/* LOOP FOUR */` |
|        5 | 3047 | `	}` |
|        - | 3048 | `	/* Return compilation status */` |
|  1539621 | 3049 | `	return rc;` |
|        5 | 3050 | `}` |
|        - | 3051 | `/*` |
|        - | 3052 | ` * Compile a Raw PHP chunk.` |
|        - | 3053 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|        - | 3054 | ` * takes care of generating the appropriate error message.` |
|        - | 3055 | ` */` |
|    16984 | 3056 | `static sxi32 PH7_CompilePHP(` |
|        - | 3057 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - | 3058 | `	SySet *pTokenSet,     /* Token set */` |
|        - | 3059 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|        - | 3060 | `	)` |
|        5 | 3061 | `{` |
|    16989 | 3062 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|        - | 3063 | `	sxi32 rc;` |
|        - | 3064 | `	/* Reset the token set (and its trivia sidecar) */` |
|    16989 | 3065 | `	SySetReset(&(*pTokenSet));` |
|    16989 | 3066 | `	SySetReset(&pGen->aTrivia);` |
|        - | 3067 | `	/* Mark as the default token set */` |
|    16989 | 3068 | `	pGen->pTokenSet = &(*pTokenSet);` |
|        - | 3069 | `	/* Advance the stream cursor */` |
|    16989 | 3070 | `	pGen->pRawIn++;` |
|        - | 3071 | `	/* Tokenize the PHP chunk first */` |
|    16989 | 3072 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet),&pGen->aTrivia);` |
|        - | 3073 | `	/* Point to the head and tail of the token stream. */` |
|    16989 | 3074 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|    16989 | 3075 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|    16989 | 3076 | `	if( is_expr ){` |
|      ! 0 | 3077 | `		rc = SXERR_EMPTY;` |
|      ! 0 | 3078 | `		if( pGen->pIn < pGen->pEnd ){` |
|        - | 3079 | `			/* A simple expression,compile it */` |
|      ! 0 | 3080 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|      ! 0 | 3081 | `		}` |
|        - | 3082 | `		/* Emit the DONE instruction */` |
|      ! 0 | 3083 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      ! 0 | 3084 | `		return SXRET_OK;` |
|        - | 3085 | `	}` |
|    16989 | 3086 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|        - | 3087 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|        - | 3088 | `		/*` |
|        - | 3089 | `		 * Shortcut syntax for the 'echo' language construct.` |
|        - | 3090 | `		 * According to the PHP reference manual:` |
|        - | 3091 | `		 *  echo() also has a shortcut syntax, where you can` |
|        - | 3092 | `		 *  immediately follow` |
|        - | 3093 | `		 *  the opening tag with an equals sign as follows:` |
|        - | 3094 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|        - | 3095 | `		 * Symisc extension:` |
|        - | 3096 | `		 *   This short syntax works with all PHP opening` |
|        - | 3097 | `		 *   tags unlike the default PHP engine that handle` |
|        - | 3098 | `		 *   only short tag.` |
|        - | 3099 | `		 */` |
|        - | 3100 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|        3 | 3101 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|        3 | 3102 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|        3 | 3103 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|        - | 3104 | `		/* This synthesized echo is compiled as an EXPRESSION, which is otherwise a` |
|        - | 3105 | `		 * parse error; allow it for the duration of this one compile. */` |
|        3 | 3106 | `		pGen->nExprEchoOk++;` |
|        3 | 3107 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|        3 | 3108 | `		pGen->nExprEchoOk--;` |
|        3 | 3109 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3110 | `			return SXERR_ABORT;` |
|        - | 3111 | `		}` |
|        3 | 3112 | `		if( rc != SXERR_EMPTY ){` |
|        3 | 3113 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        1 | 3114 | `		}` |
|        3 | 3115 | `		return SXRET_OK;` |
|        - | 3116 | `	}` |
|        - | 3117 | `	/* Compile the PHP chunk */` |
|    16987 | 3118 | `	rc = GenStateCompileChunk(pGen,0);` |
|        - | 3119 | `	/* Fix exceptions jumps */` |
|    16987 | 3120 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        - | 3121 | `	/* Fix gotos now, the jump destination is resolved */` |
|    16987 | 3122 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|        3 | 3123 | `		rc = SXERR_ABORT;` |
|        1 | 3124 | `	}` |
|        - | 3125 | `	/* Reset container */` |
|    16987 | 3126 | `	SySetReset(&pGen->aGoto);` |
|    16987 | 3127 | `	SySetReset(&pGen->aLabel);` |
|    16987 | 3128 | `	SySetReset(&pGen->aNullsafeJmp);` |
|        - | 3129 | `	/* Compilation result */` |
|    16987 | 3130 | `	return rc;` |
|     8497 | 3131 | `}` |
|        - | 3132 | `/*` |
|        - | 3133 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|        - | 3134 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|        - | 3135 | ` * This is the only compile interface exported from this file.` |
|        - | 3136 | ` */` |
|    20722 | 3137 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|        - | 3138 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|        - | 3139 | `	SyString *pScript,  /* Script to compile */` |
|        - | 3140 | `	sxi32 iFlags        /* Compile flags */` |
|        - | 3141 | `	)` |
|        5 | 3142 | `{` |
|        - | 3143 | `	SySet aPhpToken,aRawToken;` |
|        - | 3144 | `	ph7_gen_state *pCodeGen;` |
|        - | 3145 | `	ph7_value *pRawObj;` |
|        - | 3146 | `	sxu32 nObjIdx;` |
|        - | 3147 | `	sxi32 nRawObj;` |
|        - | 3148 | `	int is_expr;` |
|        - | 3149 | `	sxi8 bSavedStrict;` |
|        - | 3150 | `	sxi8 bSavedStrictLocked;` |
|        - | 3151 | `	SyToken *pSavedIn,*pSavedEnd;` |
|        - | 3152 | `	sxi32 rc;` |
|    20727 | 3153 | `	sxu32 nBaseLine = 1;` |
|    20727 | 3154 | `	if( pScript->nByte < 1 ){` |
|        - | 3155 | `		/* Nothing to compile */` |
|      ! 0 | 3156 | `		return PH7_OK;` |
|        - | 3157 | `	}` |
|        - | 3158 | `	/* php skips a "#!" shebang on the first line of a CLI script: consume it` |
|        - | 3159 | `	 * (including its newline) so it is not echoed as inline text, and bump the` |
|        - | 3160 | `	 * base line to 2 so the code below still reports php-matching line numbers. */` |
|    20727 | 3161 | `	if( pScript->nByte >= 2 && pScript->zString[0]=='#' && pScript->zString[1]=='!' ){` |
|        3 | 3162 | `		const char *z = pScript->zString;` |
|        3 | 3163 | `		const char *zEnd = &z[pScript->nByte];` |
|       39 | 3164 | `		while( z < zEnd && z[0] != '\n' ){ z++; }` |
|        3 | 3165 | `		if( z < zEnd ){ z++; } /* consume the newline too */` |
|        3 | 3166 | `		pScript->nByte -= (sxu32)(z - pScript->zString);` |
|        3 | 3167 | `		pScript->zString = z;` |
|        3 | 3168 | `		nBaseLine = 2;` |
|        3 | 3169 | `		if( pScript->nByte < 1 ){` |
|      ! 0 | 3170 | `			return PH7_OK;` |
|        - | 3171 | `		}` |
|        1 | 3172 | `	}` |
|        - | 3173 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|        - | 3174 | `	 * file's flags so include/require restore them on return. */` |
|    20727 | 3175 | `	pCodeGen = &pVm->sCodeGen;` |
|        - | 3176 | ``	/* aPhpToken below is a LOCAL token set: once it is released at `cleanup`, any`` |
|        - | 3177 | `	 * pGen->pIn/pEnd still pointing into it DANGLE. PH7_VmEmitInstr reads pIn to stamp` |
|        - | 3178 | `	 * each instruction's source line, and instructions are still emitted after this` |
|        - | 3179 | `	 * function returns (VmEvalChunk's trailing OP_DONE) -- a use-after-free ASan caught` |
|        - | 3180 | `	 * immediately. Save the caller's cursor and restore it on the way out, so an` |
|        - | 3181 | `	 * enclosing compile keeps its (live) tokens and the top level goes back to NULL. */` |
|    20727 | 3182 | `	pSavedIn = pCodeGen->pIn;` |
|    20727 | 3183 | `	pSavedEnd = pCodeGen->pEnd;` |
|    20727 | 3184 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|    20727 | 3185 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|    20727 | 3186 | `	pCodeGen->bStrictTypes = 0;` |
|    20727 | 3187 | `	pCodeGen->bStrictTypesLocked = 0;` |
|        - | 3188 | `	/* Initialize the tokens containers */` |
|    20727 | 3189 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|    20727 | 3190 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|    20727 | 3191 | `	SySetAlloc(&aPhpToken,0xc0);` |
|    20727 | 3192 | `	is_expr = 0;` |
|    20727 | 3193 | `	if( iFlags & PH7_PHP_ONLY ){` |
|        - | 3194 | `		SyToken sTmp;` |
|        - | 3195 | `		/* PHP only: -*/` |
|     5461 | 3196 | `		sTmp.nLine = 1;` |
|     5461 | 3197 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|     5461 | 3198 | `		sTmp.pUserData = 0;` |
|     5461 | 3199 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|     5461 | 3200 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|     5461 | 3201 | `		if( iFlags & PH7_PHP_EXPR ){` |
|        - | 3202 | `			/* A simple PHP expression */` |
|      ! 0 | 3203 | `			is_expr = 1;` |
|      ! 0 | 3204 | `		}` |
|     2733 | 3205 | `	}else{` |
|        - | 3206 | `		/* Tokenize raw text */` |
|    15271 | 3207 | `		SySetAlloc(&aRawToken,32);` |
|    15271 | 3208 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken,nBaseLine);` |
|        - | 3209 | `	}` |
|        - | 3210 | `	/* Process high-level tokens */` |
|    20727 | 3211 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|    20727 | 3212 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|    20727 | 3213 | `	rc = PH7_OK;` |
|    20727 | 3214 | `	if( is_expr ){` |
|        - | 3215 | `		/* Compile the expression */` |
|      ! 0 | 3216 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|      ! 0 | 3217 | `		goto cleanup;` |
|        - | 3218 | `	}` |
|    20727 | 3219 | `	nObjIdx = 0;` |
|        - | 3220 | `	/* Start the compilation process */` |
|    17998 | 3221 | `	for(;;){` |
|    52907 | 3222 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|    20649 | 3223 | `			break; /* No more tokens to process */` |
|        - | 3224 | `		}` |
|    32263 | 3225 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|        - | 3226 | `			/* Compile the PHP chunk */` |
|    16989 | 3227 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|    16989 | 3228 | `			if( rc == SXERR_ABORT ){` |
|       83 | 3229 | `				break;` |
|        - | 3230 | `			}` |
|    16911 | 3231 | `			continue;` |
|        - | 3232 | `		}` |
|        - | 3233 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|    15279 | 3234 | `		nRawObj = 0;` |
|    30553 | 3235 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|        - | 3236 | `			/* Consume the raw chunk without any processing */` |
|    15279 | 3237 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|    15279 | 3238 | `			if( pRawObj == 0 ){` |
|      ! 0 | 3239 | `				rc = SXERR_MEM;` |
|      ! 0 | 3240 | `				break;` |
|        - | 3241 | `			}` |
|        - | 3242 | `			/* Mark as constant and emit the load constant instruction */` |
|    15279 | 3243 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|    15279 | 3244 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|    15279 | 3245 | `			++nRawObj;` |
|    15279 | 3246 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|        5 | 3247 | `		}` |
|    15279 | 3248 | `		if( nRawObj > 0 ){` |
|        - | 3249 | `			/* Emit the consume instruction */` |
|    15279 | 3250 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     7637 | 3251 | `		}` |
|    10366 | 3252 | `	}` |
|    10361 | 3253 | `cleanup:` |
|        - | 3254 | `	/* Drop the cursor into the token set BEFORE the set is freed (see above). */` |
|    20727 | 3255 | `	pCodeGen->pIn = pSavedIn;` |
|    20727 | 3256 | `	pCodeGen->pEnd = pSavedEnd;` |
|    20727 | 3257 | `	SySetRelease(&aRawToken);` |
|    20727 | 3258 | `	SySetRelease(&aPhpToken);` |
|        - | 3259 | `	/* Restore outer file's strict_types scope */` |
|    20727 | 3260 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|    20727 | 3261 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|    20727 | 3262 | `	return rc;` |
|    10366 | 3263 | `}` |
|        - | 3264 | `/*` |
|        - | 3265 | ` * Utility routines.Initialize the code generator.` |
|        - | 3266 | ` */` |
|     5146 | 3267 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|        - | 3268 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 3269 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|        - | 3270 | `	void *pErrData     /* Last argument to xErr() */` |
|        - | 3271 | `	)` |
|        5 | 3272 | `{` |
|     5151 | 3273 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|        - | 3274 | `	/* Zero the structure */` |
|     5151 | 3275 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|        - | 3276 | `	/* Initial state */` |
|     5151 | 3277 | `	pGen->pVm  = &(*pVm);` |
|     5151 | 3278 | `	pGen->xErr = xErr;` |
|     5151 | 3279 | `	pGen->pErrData = pErrData;` |
|     5151 | 3280 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|     5151 | 3281 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|     5151 | 3282 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|     5151 | 3283 | `	SySetInit(&pGen->aScope,&pVm->sAllocator,sizeof(GenScope));` |
|     5151 | 3284 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|     5151 | 3285 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|     5151 | 3286 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|     5151 | 3287 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|     5151 | 3288 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|     5151 | 3289 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|        - | 3290 | `	/* Error log buffer */` |
|     5151 | 3291 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|        - | 3292 | `	/* General purpose working buffer */` |
|     5151 | 3293 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|        - | 3294 | `	/* Namespace state */` |
|     5151 | 3295 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|     5151 | 3296 | `	GenStateInitUseImports(&(*pGen),&(*pVm));` |
|     5151 | 3297 | `	GenStateInitSeenSymbols(&(*pGen),&(*pVm));` |
|        - | 3298 | `	/* Create the global scope */` |
|     5151 | 3299 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|        - | 3300 | `	/* Point to the global scope */` |
|     5151 | 3301 | `	pGen->pCurrent = &pGen->sGlobal;` |
|     5151 | 3302 | `	return SXRET_OK;` |
|        5 | 3303 | `}` |
|        - | 3304 | `/*` |
|        - | 3305 | ` * Utility routines. Reset the code generator to it's initial state.` |
|        - | 3306 | ` */` |
|    25270 | 3307 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|        - | 3308 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 3309 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|        - | 3310 | `	void *pErrData     /* Last argument to xErr() */` |
|        - | 3311 | `	)` |
|        5 | 3312 | `{` |
|    25275 | 3313 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|        - | 3314 | `	GenBlock *pBlock,*pParent;` |
|        - | 3315 | `	/* Reset state */` |
|    25275 | 3316 | `	SySetReset(&pGen->aLabel);` |
|    25275 | 3317 | `	SySetReset(&pGen->aGoto);` |
|    25275 | 3318 | `	SySetReset(&pGen->aNullsafeJmp);` |
|    25275 | 3319 | `	SySetReset(&pGen->aTrivia);` |
|    25275 | 3320 | `	SySetReset(&pGen->aPendingAttrs);` |
|    25275 | 3321 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|    25275 | 3322 | `	SyBlobRelease(&pGen->sErrBuf);` |
|    25275 | 3323 | `	SyBlobRelease(&pGen->sWorker);` |
|    25275 | 3324 | `	SyBlobRelease(&pGen->sNamespace);` |
|    25275 | 3325 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    25275 | 3326 | `	GenStateResetUseImports(&(*pGen),&(*pVm));` |
|        - | 3327 | `	/* A fresh compile unit has declared nothing yet. */` |
|    25275 | 3328 | `	GenStateResetSeenSymbols(&(*pGen),&(*pVm));` |
|        - | 3329 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|        - | 3330 | `	 * They intern variable names and literal strings that are referenced by` |
|        - | 3331 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|        - | 3332 | `	 * Releasing them would either leak the interned strings or require freeing` |
|        - | 3333 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|        - | 3334 | `	 * number of unique names, which is acceptable. */` |
|        - | 3335 | `	/* Point to the global scope */` |
|    25275 | 3336 | `	pBlock = pGen->pCurrent;` |
|    25275 | 3337 | `	while( pBlock->pParent != 0 ){` |
|      ! 0 | 3338 | `		pParent = pBlock->pParent;` |
|      ! 0 | 3339 | `		GenStateFreeBlock(pBlock);` |
|      ! 0 | 3340 | `		pBlock = pParent;` |
|      ! 0 | 3341 | `	}` |
|    25275 | 3342 | `	pGen->xErr = xErr;` |
|    25275 | 3343 | `	pGen->pErrData = pErrData;` |
|    25275 | 3344 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    25275 | 3345 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|    25275 | 3346 | `	pGen->pIn = pGen->pEnd = 0;` |
|    25275 | 3347 | `	pGen->nErr = 0;` |
|        - | 3348 | `	/* Clear the class-body context (a prior compile aborted mid-class-body would` |
|        - | 3349 | `	 * otherwise leave these live for the next eval/include on this VM). */` |
|    25275 | 3350 | `	pGen->pCurClass = 0;` |
|    25275 | 3351 | `	pGen->iInMemberDefault = 0;` |
|    25275 | 3352 | `	return SXRET_OK;` |
|        5 | 3353 | `}` |
|        - | 3354 | `/*` |
|        - | 3355 | ` * Save the code generator's compile-position state and hand the live generator a` |
|        - | 3356 | ` * fresh, empty one for a NESTED compilation unit.` |
|        - | 3357 | ` *` |
|        - | 3358 | ` * A require/include normally runs at execution time, when no compile is in flight,` |
|        - | 3359 | ` * so VmEvalChunk can call PH7_ResetCodeGenerator to wipe the generator. But an` |
|        - | 3360 | `` * autoload can fire in the MIDDLE of compiling a class: resolving `class Child`` |
|        - | 3361 | `` * extends Base` calls PH7_VmExtractClass, which triggers the autoloader, whose`` |
|        - | 3362 | `` * body `require`s Base's file — a nested compile while the outer one is mid-token.`` |
|        - | 3363 | ` * PH7_ResetCodeGenerator would then blow away the outer compile's cursor` |
|        - | 3364 | ` * (pIn/pEnd), current block, use-imports, labels, etc.; the outer parse resumes` |
|        - | 3365 | ` * with pIn == NULL and reports a bogus "Expected '{' after class 'Child'". (Common` |
|        - | 3366 | ` * in every real framework: Composer autoloads parent classes on demand.)` |
|        - | 3367 | ` *` |
|        - | 3368 | ` * This snapshots the position/scope fields into *pSaved (a caller-stack` |
|        - | 3369 | ` * ph7_gen_state used purely as storage) and re-initializes the live generator's` |
|        - | 3370 | ` * position containers to FRESH, empty ones WITHOUT releasing the outer's (the` |
|        - | 3371 | ` * snapshot now owns those). hLiteral/hNumLiteral/hVar are intentionally left LIVE` |
|        - | 3372 | ` * and shared — compiled bytecode interns names/literals into them and they grow` |
|        - | 3373 | ` * monotonically (exactly why PH7_ResetCodeGenerator keeps them); restoring an old` |
|        - | 3374 | ` * copy of those headers after a nested grow would use-after-free the bucket array.` |
|        - | 3375 | ` */` |
|        4 | 3376 | `PH7_PRIVATE void PH7_CompilerSaveState(ph7_vm *pVm,ph7_gen_state *pSaved,ProcConsumer xErr,void *pErrData)` |
|        1 | 3377 | `{` |
|        5 | 3378 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|        - | 3379 | `	/* Shallow-copy every field; the position containers below are then replaced` |
|        - | 3380 | `	 * with fresh ones on the live struct, so *pSaved keeps the outer's memory. */` |
|        5 | 3381 | `	*pSaved = *pGen;` |
|        5 | 3382 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|        5 | 3383 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|        5 | 3384 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|        5 | 3385 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|        5 | 3386 | `	SySetInit(&pGen->aScope,&pVm->sAllocator,sizeof(GenScope));` |
|        5 | 3387 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|        5 | 3388 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|        5 | 3389 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|        5 | 3390 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|        5 | 3391 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|        5 | 3392 | `	GenStateInitUseImports(&(*pGen),&(*pVm));` |
|        5 | 3393 | `	GenStateInitSeenSymbols(&(*pGen),&(*pVm));` |
|        - | 3394 | `	/* Fresh global scope for the nested unit (address of the embedded sGlobal is` |
|        - | 3395 | `	 * stable, so any outer block still parented to it stays valid across restore). */` |
|        5 | 3396 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(pVm),0);` |
|        5 | 3397 | `	pGen->pCurrent = &pGen->sGlobal;` |
|        5 | 3398 | `	pGen->pIn = pGen->pEnd = 0;` |
|        5 | 3399 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|        5 | 3400 | `	pGen->pTokenSet = 0;` |
|        5 | 3401 | `	pGen->nErr = 0;` |
|        5 | 3402 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|        5 | 3403 | `	pGen->nCommaExprOk = 0;` |
|        5 | 3404 | `	pGen->zClauseCloser = 0;` |
|        5 | 3405 | `	pGen->bInGenerator = 0;` |
|        5 | 3406 | `	pGen->bStrictTypes = 0;` |
|        5 | 3407 | `	pGen->bStrictTypesLocked = 0;` |
|        - | 3408 | `	/* The nested unit is a fresh top-level compile: it is not lexically inside the` |
|        - | 3409 | `	 * outer's class body nor its member default, so a __TRAIT__ in the nested file` |
|        - | 3410 | `	 * must not inherit the outer's trait. (Restore below carries the outer's values` |
|        - | 3411 | `	 * back, so only the nested unit sees these zeros.) */` |
|        5 | 3412 | `	pGen->pCurClass = 0;` |
|        5 | 3413 | `	pGen->iInMemberDefault = 0;` |
|        5 | 3414 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|        5 | 3415 | `	pGen->xErr = xErr;` |
|        5 | 3416 | `	pGen->pErrData = pErrData;` |
|        5 | 3417 | `}` |
|        - | 3418 | `/*` |
|        - | 3419 | ` * Restore the outer compile-position state saved by PH7_CompilerSaveState,` |
|        - | 3420 | ` * releasing the nested unit's position containers first. The shared` |
|        - | 3421 | ` * hLiteral/hNumLiteral/hVar tables (grown by the nested compile) are carried` |
|        - | 3422 | ` * forward, NOT rolled back to the snapshot's stale headers.` |
|        - | 3423 | ` */` |
|        4 | 3424 | `PH7_PRIVATE void PH7_CompilerRestoreState(ph7_vm *pVm,ph7_gen_state *pSaved)` |
|        1 | 3425 | `{` |
|        5 | 3426 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|        - | 3427 | `	GenBlock *pBlock,*pParent;` |
|        - | 3428 | `	SyHash hVar,hLiteral,hNumLiteral;` |
|        - | 3429 | `	/* Free any nested blocks left open (e.g. an aborted nested compile), then the` |
|        - | 3430 | `	 * nested global block's own fixup sets. */` |
|        5 | 3431 | `	pBlock = pGen->pCurrent;` |
|        5 | 3432 | `	while( pBlock && pBlock->pParent != 0 ){` |
|      ! 0 | 3433 | `		pParent = pBlock->pParent;` |
|      ! 0 | 3434 | `		GenStateFreeBlock(pBlock);` |
|      ! 0 | 3435 | `		pBlock = pParent;` |
|      ! 0 | 3436 | `	}` |
|        5 | 3437 | `	GenStateReleaseBlock(&pGen->sGlobal);` |
|        - | 3438 | `	/* Release the nested unit's position containers. */` |
|        5 | 3439 | `	SySetRelease(&pGen->aLabel);` |
|        5 | 3440 | `	SySetRelease(&pGen->aGoto);` |
|        5 | 3441 | `	SySetRelease(&pGen->aNullsafeJmp);` |
|        5 | 3442 | `	SySetRelease(&pGen->aLoopParent);` |
|        5 | 3443 | `	SySetRelease(&pGen->aScope);` |
|        5 | 3444 | `	SySetRelease(&pGen->aTrivia);` |
|        5 | 3445 | `	SySetRelease(&pGen->aPendingAttrs);` |
|        5 | 3446 | `	SyBlobRelease(&pGen->sWorker);` |
|        5 | 3447 | `	SyBlobRelease(&pGen->sErrBuf);` |
|        5 | 3448 | `	SyBlobRelease(&pGen->sNamespace);` |
|        5 | 3449 | `	SyHashRelease(&pGen->hUseImports);` |
|        5 | 3450 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|        5 | 3451 | `	SyHashRelease(&pGen->hUseConstImports);` |
|        5 | 3452 | `	GenStateReleaseSeenSymbols(&(*pGen));` |
|        - | 3453 | `	/* Preserve the (possibly grown) shared intern tables across the restore. */` |
|        5 | 3454 | `	hVar = pGen->hVar;` |
|        5 | 3455 | `	hLiteral = pGen->hLiteral;` |
|        5 | 3456 | `	hNumLiteral = pGen->hNumLiteral;` |
|        5 | 3457 | `	*pGen = *pSaved;` |
|        5 | 3458 | `	pGen->hVar = hVar;` |
|        5 | 3459 | `	pGen->hLiteral = hLiteral;` |
|        5 | 3460 | `	pGen->hNumLiteral = hNumLiteral;` |
|        5 | 3461 | `}` |
|        - | 3462 | `/*` |
|        - | 3463 | ` * Raise php's parse error for an unexpected token: E_PARSE with the exact text` |
|        - | 3464 | ` * php's parser prints, e.g.` |
|        - | 3465 | ` *` |
|        - | 3466 | ` *   syntax error, unexpected token ";", expecting "{"` |
|        - | 3467 | ` *   syntax error, unexpected identifier "invalid", expecting "("` |
|        - | 3468 | ` *   syntax error, unexpected end of file` |
|        - | 3469 | ` *` |
|        - | 3470 | ` * php names the token by CLASS, not just by text: an identifier, a variable and` |
|        - | 3471 | ` * a number each get their own noun, while everything else (keywords, operators,` |
|        - | 3472 | ` * punctuation) is a "token". zExpecting is the optional ", expecting ..." tail` |
|        - | 3473 | ` * — pass NULL when the site cannot say what it wanted (php often can't either).` |
|        - | 3474 | ` * pTok == NULL means the input ran out: "unexpected end of file".` |
|        - | 3475 | ` *` |
|        - | 3476 | ` * PHL's hand-written recursive-descent parser has no bison expectation sets, so` |
|        - | 3477 | ` * a site can only claim an "expecting" clause it genuinely knows; every clause` |
|        - | 3478 | ` * emitted here was verified against php 8.5.7 for the construct in question.` |
|        - | 3479 | ` */` |
|      212 | 3480 | `PH7_PRIVATE sxi32 PH7_GenSyntaxError(` |
|        - | 3481 | `	ph7_gen_state *pGen,   /* Code generator state */` |
|        - | 3482 | `	SyToken *pTok,         /* Offending token, or NULL for end of file */` |
|        - | 3483 | `	const char *zExpecting /* ", expecting <this>" tail, or NULL */` |
|        - | 3484 | `	)` |
|        5 | 3485 | `{` |
|        - | 3486 | ``	/* php's noun for the offending token. An ALPHA-stream operator (`and`, `or`,`` |
|        - | 3487 | ``	 * `xor`, `new`, `clone`, `instanceof`) is lexed ID\|OP here but php calls it a`` |
|        - | 3488 | `	 * TOKEN, like every other reserved word — only a real identifier gets the` |
|        - | 3489 | `	 * "identifier" noun. */` |
|      217 | 3490 | `	const char *zNoun = "token";` |
|        - | 3491 | `	sxu32 nLine;` |
|      217 | 3492 | `	if( pTok == 0 && pGen->pTokenSet ){` |
|        - | 3493 | `		/* The caller ran out of tokens inside its own slice — but a statement's slice stops` |
|        - | 3494 | `		 * BEFORE its terminator, so the token php actually names (typically the ';') is the` |
|        - | 3495 | `		 * one sitting just past the slice, still inside the chunk's token stream. Reach for` |
|        - | 3496 | `		 * it before concluding "end of file". */` |
|       99 | 3497 | `		SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|       99 | 3498 | `		SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|       99 | 3499 | `		if( pGen->pEnd >= pBase && pGen->pEnd < pStreamEnd ){` |
|       92 | 3500 | `			pTok = pGen->pEnd;` |
|       44 | 3501 | `		}` |
|       47 | 3502 | `	}` |
|      217 | 3503 | `	nLine = pTok ? pTok->nLine : (pGen->pIn > (SyToken *)SySetBasePtr(pGen->pTokenSet) ? pGen->pIn[-1].nLine : 1);` |
|      217 | 3504 | `	if( pTok == 0 ){` |
|       12 | 3505 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        3 | 3506 | `			zExpecting ? "syntax error, unexpected end of file, expecting %s"` |
|        - | 3507 | `			           : "syntax error, unexpected end of file",` |
|        3 | 3508 | `			zExpecting);` |
|        - | 3509 | `	}` |
|      211 | 3510 | `	if( (pTok->nType & PH7_TK_ID) && (pTok->nType & PH7_TK_OP) == 0 ){` |
|       22 | 3511 | `		zNoun = "identifier";` |
|      202 | 3512 | `	}else if( pTok->nType & PH7_TK_DOLLAR ){` |
|        9 | 3513 | `		zNoun = "variable";` |
|        - | 3514 | `		/* The '$' is its own token and carries only "$" as text; the NAME is the` |
|        - | 3515 | ``		 * token after it. php names the whole variable, so `$x` was being reported`` |
|        - | 3516 | ``		 * as the nameless `variable "$"`. Stitch the two back together. */`` |
|        9 | 3517 | `		if( pGen->pTokenSet ){` |
|        9 | 3518 | `			SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        9 | 3519 | `			SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|        9 | 3520 | `			SyToken *pName = &pTok[1];` |
|        6 | 3521 | `			if( pTok >= pBase && pName < pStreamEnd` |
|        6 | 3522 | `				&& (pName->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|        9 | 3523 | `				&& pName->sData.nByte > 0 ){` |
|        9 | 3524 | `				SyBlobReset(&pGen->sWorker);` |
|        9 | 3525 | `				SyBlobAppend(&pGen->sWorker,"$",sizeof(char));` |
|        9 | 3526 | `				SyBlobAppend(&pGen->sWorker,pName->sData.zString,pName->sData.nByte);` |
|        - | 3527 | `				{` |
|        - | 3528 | `					SyString sVar;` |
|        9 | 3529 | `					SyStringInitFromBuf(&sVar,SyBlobData(&pGen->sWorker),` |
|        - | 3530 | `						SyBlobLength(&pGen->sWorker));` |
|        9 | 3531 | `					if( zExpecting ){` |
|       12 | 3532 | `						return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        - | 3533 | `							"syntax error, unexpected %s \"%z\", expecting %s",` |
|        3 | 3534 | `							zNoun,&sVar,zExpecting);` |
|        - | 3535 | `					}` |
|      ! 0 | 3536 | `					return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|      ! 0 | 3537 | `						"syntax error, unexpected %s \"%z\"",zNoun,&sVar);` |
|        - | 3538 | `				}` |
|        - | 3539 | `			}` |
|      ! 0 | 3540 | `		}` |
|      187 | 3541 | `	}else if( pTok->nType & PH7_TK_INTEGER ){` |
|       27 | 3542 | `		zNoun = "integer";` |
|      175 | 3543 | `	}else if( pTok->nType & PH7_TK_REAL ){` |
|      ! 0 | 3544 | `		zNoun = "float";` |
|      ! 0 | 3545 | `	}` |
|      205 | 3546 | `	if( zExpecting ){` |
|      146 | 3547 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|       47 | 3548 | `			"syntax error, unexpected %s \"%z\", expecting %s",zNoun,&pTok->sData,zExpecting);` |
|        - | 3549 | `	}` |
|      164 | 3550 | `	return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|       53 | 3551 | `		"syntax error, unexpected %s \"%z\"",zNoun,&pTok->sData);` |
|      111 | 3552 | `}` |
|        - | 3553 | `/*` |
|        - | 3554 | ` * Generate a compile-time error message.` |
|        - | 3555 | ` * If the error count limit is reached (usually 15 error message)` |
|        - | 3556 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|        - | 3557 | ` * abort compilation immediately.` |
|        - | 3558 | ` */` |
|      874 | 3559 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|        5 | 3560 | `{` |
|      879 | 3561 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|      879 | 3562 | `	const char *zErr = "Error";` |
|        - | 3563 | `	SyString *pFile;` |
|        - | 3564 | `	va_list ap;` |
|        - | 3565 | `	sxi32 rc;` |
|        - | 3566 | `	/* Reset the working buffer */` |
|      879 | 3567 | `	SyBlobReset(pWorker);` |
|        - | 3568 | `	/* Peek the processed file path if available */` |
|      879 | 3569 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|      879 | 3570 | `	if( nErrType == E_ERROR \|\| nErrType == E_PARSE ){` |
|        - | 3571 | `		/* Increment the error counter. A PARSE error is every bit as fatal as an` |
|        - | 3572 | `		 * E_ERROR one: php compiles nothing, runs nothing and exits 255. Counting` |
|        - | 3573 | `		 * only E_ERROR let a parse error print its diagnostic and then fall through` |
|        - | 3574 | `		 * into execution with a 0 exit status. */` |
|      837 | 3575 | `		pGen->nErr++;` |
|      837 | 3576 | `		if( pGen->nErr > 15 ){` |
|        - | 3577 | `			/* Error count limit reached */` |
|        6 | 3578 | `			if( pGen->xErr ){` |
|        6 | 3579 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|        6 | 3580 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|        6 | 3581 | `				if( pFile ){` |
|        6 | 3582 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|        2 | 3583 | `				}` |
|        6 | 3584 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|        6 | 3585 | `				if( SyBlobLength(pWorker) > 0 ){` |
|        6 | 3586 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|        2 | 3587 | `				}` |
|        2 | 3588 | `			}` |
|        - | 3589 | `			/* Abort immediately */` |
|        6 | 3590 | `			return SXERR_ABORT;` |
|        - | 3591 | `		}` |
|      414 | 3592 | `	}` |
|      875 | 3593 | `	if( pGen->xErr == 0 ){` |
|        - | 3594 | `		/* No consumer — but keep the BARE message in the error buffer so a caller` |
|        - | 3595 | `		 * that needs the text can read it back. eval() compiles with logging off` |
|        - | 3596 | `		 * (a parse error there is php's catchable ParseError, not a printed` |
|        - | 3597 | `		 * diagnostic) and needs exactly this string for the exception message. */` |
|       41 | 3598 | `		va_start(ap,zFormat);` |
|       41 | 3599 | `		SyBlobFormatAp(pWorker,zFormat,ap);` |
|       41 | 3600 | `		va_end(ap);` |
|       41 | 3601 | `		return SXRET_OK;` |
|        - | 3602 | `	}` |
|      835 | 3603 | `	switch(nErrType){` |
|      424 | 3604 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|       47 | 3605 | `	case E_WARNING: zErr = "Warning";     break;` |
|      372 | 3606 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|      ! 0 | 3607 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|      ! 0 | 3608 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|      ! 0 | 3609 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|      ! 0 | 3610 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|      ! 0 | 3611 | `	case 8192 /* E_DEPRECATED */: zErr = "Deprecated"; break;` |
|      ! 0 | 3612 | `	default:` |
|      ! 0 | 3613 | `		break;` |
|        - | 3614 | `	}` |
|      835 | 3615 | `	rc = SXRET_OK;` |
|        - | 3616 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|      835 | 3617 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|      835 | 3618 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|      835 | 3619 | `	va_start(ap,zFormat);` |
|      835 | 3620 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|      835 | 3621 | `	va_end(ap);` |
|      835 | 3622 | `	if( pFile ){` |
|      835 | 3623 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|      415 | 3624 | `	}` |
|        - | 3625 | `	/* Append a new line */` |
|      835 | 3626 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|      835 | 3627 | `	if( SyBlobLength(pWorker) > 0 ){` |
|        - | 3628 | `		/* Consume the generated error message */` |
|      835 | 3629 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|      415 | 3630 | `	}` |
|      835 | 3631 | `	return rc;` |
|      442 | 3632 | `}` |
|        - | 3633 |  |
