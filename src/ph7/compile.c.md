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
|    37664 |   55 | `PH7_PRIVATE GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|        5 |   56 | `{` |
|    37669 |   57 | `	GenBlock *pBlock = pCurrent;` |
|    84484 |   58 | `	for(;;){` |
|   168973 |   59 | `		if( pBlock->iFlags & iBlockType ){` |
|    37651 |   60 | `			iCount--; /* Decrement nesting level */` |
|    37651 |   61 | `			if( iCount < 1 ){` |
|        - |   62 | `				/* Block meet with the desired criteria */` |
|    37619 |   63 | `				return pBlock;` |
|        - |   64 | `			}` |
|       16 |   65 | `		}` |
|        - |   66 | `		/* Point to the upper block */` |
|   131359 |   67 | `		pBlock = pBlock->pParent;` |
|   131359 |   68 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|        - |   69 | `			/* Forbidden */` |
|       28 |   70 | `			break;` |
|        - |   71 | `		}` |
|        5 |   72 | `	}` |
|        - |   73 | `	/* No such block */` |
|       53 |   74 | `	return 0;` |
|    18837 |   75 | `}` |
|        - |   76 | `/*` |
|        - |   77 | ` * Initialize a freshly allocated block instance.` |
|        - |   78 | ` */` |
|  1176248 |   79 | `static void GenStateInitBlock(` |
|        - |   80 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |   81 | `	GenBlock *pBlock,    /* Target block */` |
|        - |   82 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|        - |   83 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|        - |   84 | `	void *pUserData      /* Upper layer private data */` |
|        - |   85 | `	)` |
|        5 |   86 | `{` |
|        - |   87 | `	/* Initialize block fields */` |
|  1176253 |   88 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  1176253 |   89 | `	pBlock->pUserData   = pUserData;` |
|  1176253 |   90 | `	pBlock->pGen        = pGen;` |
|  1176253 |   91 | `	pBlock->iFlags      = iType;` |
|  1176253 |   92 | `	pBlock->pParent     = 0;` |
|  1176253 |   93 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  1176253 |   94 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  1176253 |   95 | `}` |
|        - |   96 | `/*` |
|        - |   97 | ` * Allocate a new block instance.` |
|        - |   98 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|        - |   99 | ` * on success.Otherwise generate a compile-time error and abort` |
|        - |  100 | ` * processing on failure.` |
|        - |  101 | ` */` |
|  1171574 |  102 | `PH7_PRIVATE sxi32 GenStateEnterBlock(` |
|        - |  103 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - |  104 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|        - |  105 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|        - |  106 | `	void *pUserData,      /* Upper layer private data */` |
|        - |  107 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|        - |  108 | `	)` |
|        5 |  109 | `{` |
|        - |  110 | `	GenBlock *pBlock;` |
|        - |  111 | `	/* Allocate a new block instance */` |
|  1171579 |  112 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  1171579 |  113 | `	if( pBlock == 0 ){` |
|        - |  114 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  115 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  116 | `		 */` |
|      ! 0 |  117 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|        - |  118 | `		/* Abort processing immediately */` |
|      ! 0 |  119 | `		return SXERR_ABORT;` |
|        - |  120 | `	}` |
|        - |  121 | `	/* Zero the structure */` |
|  1171579 |  122 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  1171579 |  123 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|        - |  124 | `	/* Link to the parent block */` |
|  1171579 |  125 | `	pBlock->pParent = pGen->pCurrent;` |
|        - |  126 | `	/* A loop or switch gets an id, and remembers the loop it nests inside, so a goto's` |
|        - |  127 | `	 * and a label's positions can be compared after compilation (see aLoopParent). */` |
|  1171579 |  128 | `	if( iType & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    95479 |  129 | `		sxu32 nParent = pGen->nCurLoopId;` |
|    95479 |  130 | `		pGen->nLoopId++;` |
|    95479 |  131 | `		SySetPut(&pGen->aLoopParent,(const void *)&nParent);` |
|    95479 |  132 | `		pBlock->nLoopId = pGen->nLoopId;` |
|    95479 |  133 | `		pBlock->nOuterLoopId = nParent;` |
|    95479 |  134 | `		pGen->nCurLoopId = pGen->nLoopId;` |
|    47737 |  135 | `	}` |
|        - |  136 | `	/* A try/catch/finally block gets a scope id, and remembers the scope it nests inside,` |
|        - |  137 | `	 * so the chain between any two points can be walked after compilation (aScope). Every` |
|        - |  138 | `	 * other block simply inherits the scope in effect. */` |
|  1171579 |  139 | `	pBlock->nOuterScopeId = pGen->nCurScopeId;` |
|  1171579 |  140 | `	pBlock->nScopeId = pGen->nCurScopeId;` |
|  1171579 |  141 | `	if( iType & GEN_BLOCK_EXCEPTION ){` |
|        - |  142 | `		GenScope sScope;` |
|     6647 |  143 | `		sScope.nParent = pGen->nCurScopeId;` |
|     6647 |  144 | `		sScope.pUserData = pUserData;` |
|     6647 |  145 | `		if( iType & GEN_BLOCK_FINALLY ){` |
|      297 |  146 | `			sScope.iKind = GEN_SCOPE_FINALLY;` |
|     6501 |  147 | `		}else if( iType & GEN_BLOCK_DETACHED ){` |
|     3013 |  148 | `			sScope.iKind = GEN_SCOPE_DETACHED;` |
|     1509 |  149 | `		}else{` |
|        - |  150 | `			/* A try. pUserData is its ph7_exception, which every try-block site passes at` |
|        - |  151 | `			 * ENTRY precisely so this can classify it. */` |
|     3347 |  152 | `			sScope.iKind = GenStateInlineTryCatch(pGen) ? GEN_SCOPE_TRY_INLINE : GEN_SCOPE_TRY;` |
|        - |  153 | `		}` |
|     6647 |  154 | `		if( SySetPut(&pGen->aScope,(const void *)&sScope) == SXRET_OK ){` |
|     6647 |  155 | `			pBlock->nScopeId = SySetUsed(&pGen->aScope);` |
|     6647 |  156 | `			pGen->nCurScopeId = pBlock->nScopeId;` |
|     3321 |  157 | `		}` |
|     3321 |  158 | `	}` |
|        - |  159 | `	/* Mark as the current block */` |
|  1171579 |  160 | `	pGen->pCurrent = pBlock;` |
|  1171579 |  161 | `	if( ppBlock ){` |
|        - |  162 | `		/* Write a pointer to the new instance */` |
|   555157 |  163 | `		*ppBlock = pBlock;` |
|   277576 |  164 | `	}` |
|  1171579 |  165 | `	return SXRET_OK;` |
|   585792 |  166 | `}` |
|        - |  167 | `/*` |
|        - |  168 | ` * Release block fields without freeing the whole instance.` |
|        - |  169 | ` */` |
|  1171568 |  170 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|        5 |  171 | `{` |
|  1171573 |  172 | `	SySetRelease(&pBlock->aPostContFix);` |
|  1171573 |  173 | `	SySetRelease(&pBlock->aJumpFix);` |
|  1171573 |  174 | `}` |
|        - |  175 | `/*` |
|        - |  176 | ` * Release a block.` |
|        - |  177 | ` */` |
|  1171564 |  178 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|        5 |  179 | `{` |
|  1171569 |  180 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  1171569 |  181 | `	GenStateReleaseBlock(&(*pBlock));` |
|        - |  182 | `	/* Free the instance */` |
|  1171569 |  183 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  1171569 |  184 | `}` |
|        - |  185 | `/*` |
|        - |  186 | ` * POP and release a block from the stack of compiled blocks.` |
|        - |  187 | ` */` |
|  1171564 |  188 | `PH7_PRIVATE sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|        5 |  189 | `{` |
|  1171569 |  190 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  1171569 |  191 | `	if( pBlock == 0 ){` |
|        - |  192 | `		/* No more block to pop */` |
|      ! 0 |  193 | `		return SXERR_EMPTY;` |
|        - |  194 | `	}` |
|  1171569 |  195 | `	if( pBlock->iFlags & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    95471 |  196 | `		pGen->nCurLoopId = pBlock->nOuterLoopId;` |
|    47733 |  197 | `	}` |
|  1171569 |  198 | `	if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|     6647 |  199 | `		pGen->nCurScopeId = pBlock->nOuterScopeId;` |
|     3321 |  200 | `	}` |
|        - |  201 | `	/* Point to the upper block */` |
|  1171569 |  202 | `	pGen->pCurrent = pBlock->pParent;` |
|  1171569 |  203 | `	if( ppBlock ){` |
|        - |  204 | `		/* Write a pointer to the popped block */` |
|      ! 0 |  205 | `		*ppBlock = pBlock;` |
|      ! 0 |  206 | `	}else{` |
|        - |  207 | `		/* Safely release the block */` |
|  1171569 |  208 | `		GenStateFreeBlock(&(*pBlock));` |
|        - |  209 | `	}` |
|  1171569 |  210 | `	return SXRET_OK;` |
|   585787 |  211 | `}` |
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
|   146156 |  230 | `PH7_PRIVATE int GenStateUnconditionalTopLevel(ph7_gen_state *pGen)` |
|        5 |  231 | `{` |
|   146161 |  232 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   146325 |  233 | `	while( pBlock ){` |
|   146325 |  234 | `		if( pBlock->iFlags & (GEN_BLOCK_COND\|GEN_BLOCK_LOOP\|GEN_BLOCK_FUNC\|GEN_BLOCK_SWITCH\|GEN_BLOCK_EXCEPTION) ){` |
|      114 |  235 | `			return 0; /* conditional / nested */` |
|        - |  236 | `		}` |
|   146215 |  237 | `		if( pBlock->iFlags & GEN_BLOCK_GLOBAL ){` |
|   146051 |  238 | `			return 1; /* reached the global block with no conditional ancestor */` |
|        - |  239 | `		}` |
|      168 |  240 | `		pBlock = pBlock->pParent;` |
|        4 |  241 | `	}` |
|      ! 0 |  242 | `	return 1;` |
|    73083 |  243 | `}` |
|        - |  244 | `/*` |
|        - |  245 | ` * Guard a top-level function about to be installed. Same contract as the class` |
|        - |  246 | ` * guard above.` |
|        - |  247 | ` */` |
|   142516 |  248 | `PH7_PRIVATE sxi32 GenStateGuardFuncRedeclaration(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|        5 |  249 | `{` |
|        - |  250 | `	SyHashEntry *pEntry;` |
|   142521 |  251 | `	if( !GenStateUnconditionalTopLevel(pGen) ){` |
|       57 |  252 | `		return SXRET_OK;` |
|        - |  253 | `	}` |
|   142467 |  254 | `	pFunc->iFlags \|= VM_FUNC_BOUND;` |
|   142467 |  255 | `	if( pGen->pVm->bCompilingBuiltin ){` |
|   140105 |  256 | `		return SXRET_OK;` |
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
|     2367 |  274 | `	if( SyHashGet(&pGen->pVm->hHostFunction,(const void *)pFunc->sName.zString,pFunc->sName.nByte) ){` |
|        8 |  275 | `		PH7_GenCompileError(pGen,E_ERROR,pFunc->nLine,` |
|        2 |  276 | `			"Cannot redeclare function %z()",&pFunc->sName);` |
|        6 |  277 | `		return SXERR_ABORT;` |
|        - |  278 | `	}` |
|     2363 |  279 | `	pEntry = SyHashGet(&pGen->pVm->hFunction,(const void *)pFunc->sName.zString,pFunc->sName.nByte);` |
|     2363 |  280 | `	if( pEntry ){` |
|        9 |  281 | `		ph7_vm_func *pPrev = (ph7_vm_func *)pEntry->pUserData;` |
|       11 |  282 | `		while( pPrev ){` |
|        9 |  283 | `			if( pPrev->iFlags & VM_FUNC_BOUND ){` |
|        6 |  284 | `				if( pPrev->sFile.nByte > 0 ){` |
|        8 |  285 | `					PH7_GenCompileError(pGen,E_ERROR,pFunc->nLine,` |
|        - |  286 | `						"Cannot redeclare function %z() (previously declared in %.*s:%u)",` |
|        2 |  287 | `						&pFunc->sName,pPrev->sFile.nByte,pPrev->sFile.zString,pPrev->nLine);` |
|        4 |  288 | `				}else{` |
|      ! 0 |  289 | `					PH7_GenCompileError(pGen,E_ERROR,pFunc->nLine,` |
|      ! 0 |  290 | `						"Cannot redeclare function %z()",&pFunc->sName);` |
|        - |  291 | `				}` |
|        6 |  292 | `				return SXERR_ABORT;` |
|        - |  293 | `			}` |
|        3 |  294 | `			pPrev = pPrev->pNextName;` |
|        1 |  295 | `		}` |
|        1 |  296 | `	}` |
|     2359 |  297 | `	return SXRET_OK;` |
|    71263 |  298 | `}` |
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
|   617202 |  309 | `PH7_PRIVATE sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|        5 |  310 | `{` |
|        - |  311 | `	JumpFixup sJumpFix;` |
|        - |  312 | `	sxi32 rc;` |
|        - |  313 | `	/* Init the JumpFixup structure */` |
|   617207 |  314 | `	sJumpFix.nJumpType = nJumpType;` |
|   617207 |  315 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|        - |  316 | `	/* Remember which bytecode array the emitted instruction lives in: the block whose` |
|        - |  317 | `	 * table this lands in may be resolved after a container swap (see JumpFixup). */` |
|   617207 |  318 | `	sJumpFix.pContainer = PH7_VmGetByteCodeContainer(pBlock->pGen->pVm);` |
|        - |  319 | `	/* Insert in the jump fixup table */` |
|   617207 |  320 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|   617207 |  321 | `	return rc;` |
|        5 |  322 | `}` |
|        - |  323 | `/*` |
|        - |  324 | ` * TRUE when the body being compiled has its try/catch/finally compiled INLINE` |
|        - |  325 | ` * (ROOT C, generator bodies) rather than into detached mini-programs.` |
|        - |  326 | ` */` |
|     3342 |  327 | `PH7_PRIVATE int GenStateInlineTryCatch(ph7_gen_state *pGen)` |
|        5 |  328 | `{` |
|     3347 |  329 | `	return pGen->bInGenerator && pGen->pVm->bInlineTryCatch;` |
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
|    37768 |  356 | `PH7_PRIVATE int GenStateJumpScope(ph7_gen_state *pGen,sxu32 nFrom,sxu32 nTo,int bEmitPops,` |
|        - |  357 | `	GenJumpScope *pScope)` |
|        5 |  358 | `{` |
|    37773 |  359 | `	GenScope *aScope = (GenScope *)SySetBasePtr(&pGen->aScope);` |
|    37773 |  360 | `	sxu32 nUsed = SySetUsed(&pGen->aScope);` |
|    37773 |  361 | `	sxu32 nCur = nFrom;` |
|    37773 |  362 | `	SyZero(pScope,sizeof(*pScope));` |
|    37887 |  363 | `	while( nCur != nTo ){` |
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
|      114 |  379 | `		}else if( pScopeEnt->iKind == GEN_SCOPE_DETACHED ){` |
|       74 |  380 | `			if( pScope->nDet == 0 ){` |
|       70 |  381 | `				pScope->nTry = 0;    /* below the first boundary: not the landing pad's */` |
|       70 |  382 | `				pScope->nInline = 0;` |
|       33 |  383 | `			}` |
|       74 |  384 | `			pScope->nDet++;` |
|       74 |  385 | `		}else if( pScopeEnt->iKind == GEN_SCOPE_TRY_INLINE ){` |
|       11 |  386 | `			pScope->nInline++;` |
|       35 |  387 | `		}else if( pScope->nDet == 0 && bEmitPops ){` |
|        3 |  388 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pScopeEnt->pUserData,0);` |
|        2 |  389 | `		}else{` |
|       28 |  390 | `			pScope->nTry++;` |
|        - |  391 | `		}` |
|      119 |  392 | `		nCur = pScopeEnt->nParent;` |
|        5 |  393 | `	}` |
|    37769 |  394 | `	return TRUE;` |
|    18889 |  395 | `}` |
|        - |  396 | `/*` |
|        - |  397 | ` * Pick the jump opcode for a crossing described by GenStateJumpScope, and its iP1.` |
|        - |  398 | ` * Shared by break/continue (which know their target at emit time) and goto (which` |
|        - |  399 | ` * settles this in GenStateFixGoto, once the label fixes the counts).` |
|        - |  400 | ` */` |
|    37656 |  401 | `PH7_PRIVATE sxi32 GenStateScopeJumpOp(const GenJumpScope *pCross,sxi32 *piP1)` |
|        5 |  402 | `{` |
|    37661 |  403 | `	if( pCross->nDet > 0 \|\| pCross->nTry > 0 ){` |
|       84 |  404 | `		*piP1 = PH7_CATCH_JMP_P1(pCross->nDet,pCross->nTry);` |
|       84 |  405 | `		return PH7_OP_CATCH_JMP;` |
|        - |  406 | `	}` |
|    37581 |  407 | `	if( pCross->nInline > 0 ){` |
|       11 |  408 | `		*piP1 = (sxi32)pCross->nInline;` |
|       11 |  409 | `		return PH7_OP_SET_FINALLY_JMP;` |
|        - |  410 | `	}` |
|    37573 |  411 | `	*piP1 = 0;` |
|    37573 |  412 | `	return PH7_OP_JMP;` |
|    18833 |  413 | `}` |
|        - |  414 | `/*` |
|        - |  415 | ` * Resolve a recorded fixup to its VM instruction, in the container it was emitted` |
|        - |  416 | ` * into (see JumpFixup.pContainer) rather than whichever one is current now.` |
|        - |  417 | ` */` |
|   631378 |  418 | `PH7_PRIVATE VmInstr * GenStateFixupInstr(const JumpFixup *pFix)` |
|        5 |  419 | `{` |
|   631383 |  420 | `	return (VmInstr *)SySetAt(pFix->pContainer,pFix->nInstrIdx);` |
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
|   909212 |  433 | `PH7_PRIVATE sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|        5 |  434 | `{` |
|        - |  435 | `	JumpFixup *aFix;` |
|        - |  436 | `	VmInstr *pInstr;` |
|        - |  437 | `	sxu32 nFixed;` |
|        - |  438 | `	sxu32 n;` |
|        - |  439 | `	/* Point to the jump fixup table */` |
|   909217 |  440 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|        - |  441 | `	/* Fix the desired jumps */` |
|  2036229 |  442 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  1127017 |  443 | `		if( aFix[n].nJumpType < 0 ){` |
|        - |  444 | `			/* Already fixed */` |
|   396807 |  445 | `			continue;` |
|        - |  446 | `		}` |
|   730215 |  447 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|        - |  448 | `			/* Not of our interest */` |
|   113015 |  449 | `			continue;` |
|        - |  450 | `		}` |
|        - |  451 | `		/* Point to the instruction to fix */` |
|   617205 |  452 | `		pInstr = GenStateFixupInstr(&aFix[n]);` |
|   617205 |  453 | `		if( pInstr ){` |
|   617205 |  454 | `			pInstr->iP2 = nJumpDest;` |
|   617205 |  455 | `			nFixed++;` |
|        - |  456 | `			/* Mark as fixed */` |
|   617205 |  457 | `			aFix[n].nJumpType = -1;` |
|   308600 |  458 | `		}` |
|   308605 |  459 | `	}` |
|        - |  460 | `	/* Total number of fixed jumps */` |
|   909217 |  461 | `	return nFixed;` |
|        5 |  462 | `}` |
|        - |  463 | `/*` |
|        - |  464 | ` * Fix a 'goto' now the jump destination is resolved.` |
|        - |  465 | ` * The goto statement can be used to jump to another section` |
|        - |  466 | ` * in the program.` |
|        - |  467 | ` * Refer to the routine responsible of compiling the goto` |
|        - |  468 | ` * statement for more information.` |
|        - |  469 | ` */` |
|   164212 |  470 | `PH7_PRIVATE sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|        5 |  471 | `{` |
|        - |  472 | `	JumpFixup *pJump,*aJumps;` |
|        - |  473 | `	GenJumpScope sCross;` |
|        - |  474 | `	Label *pLabel;` |
|        - |  475 | `	VmInstr *pInstr;` |
|        - |  476 | `	sxi32 rc;` |
|        - |  477 | `	sxu32 n;` |
|        - |  478 | `	/* Point to the goto table */` |
|   164217 |  479 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|        - |  480 | `	/* Fix */` |
|   164435 |  481 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|       48 |  559 | `				sxi32 iP1 = 0;` |
|       48 |  560 | `				pInstr->iOp = (sxu8)GenStateScopeJumpOp(&sCross,&iP1);` |
|       48 |  561 | `				pInstr->iP1 = iP1;` |
|       22 |  562 | `			}` |
|       75 |  563 | `		}` |
|       80 |  564 | `	}` |
|        - |  565 | `	/* php says nothing about a label nobody jumps to — the old "defined but not` |
|        - |  566 | `	 * referenced" warning was a PH7-ism with no counterpart in the oracle. */` |
|   164215 |  567 | `	return SXRET_OK;` |
|    82111 |  568 | `}` |
|        - |  569 | `/*` |
|        - |  570 | ` * Check if a given token value is installed in the literal table.` |
|        - |  571 | ` */` |
|  1025346 |  572 | `PH7_PRIVATE sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|        5 |  573 | `{` |
|        - |  574 | `	SyHashEntry *pEntry;` |
|  1025351 |  575 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  1025351 |  576 | `	if( pEntry == 0 ){` |
|   492901 |  577 | `		return SXERR_NOTFOUND;` |
|        - |  578 | `	}` |
|   532455 |  579 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|   532455 |  580 | `	return SXRET_OK;` |
|   512678 |  581 | `}` |
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
|   492896 |  592 | `PH7_PRIVATE sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|        5 |  593 | `{` |
|   492901 |  594 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|   492901 |  595 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   246448 |  596 | `	}` |
|   492901 |  597 | `	return SXRET_OK;` |
|        5 |  598 | `}` |
|        - |  599 | `/*` |
|        - |  600 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|        - |  601 | ` * in the constant table.` |
|        - |  602 | ` */` |
|   646560 |  603 | `PH7_PRIVATE ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|        5 |  604 | `{` |
|        - |  605 | `	ph7_value *pObj;` |
|   646565 |  606 | `	sxu32 nIdx = 0; /* cc warning */` |
|        - |  607 | `	/* Reserve a new constant */` |
|   646565 |  608 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   646565 |  609 | `	if( pObj == 0 ){` |
|      ! 0 |  610 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  611 | `		return 0;` |
|        - |  612 | `	}` |
|   646565 |  613 | `	*pIdx = nIdx;` |
|        - |  614 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|        - |  615 | `	 * the constant string iterals table [optimization purposes].` |
|        - |  616 | `	 */` |
|   646565 |  617 | `	return pObj;` |
|   323285 |  618 | `}` |
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
|   668436 |  633 | `PH7_PRIVATE void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|        5 |  634 | `{` |
|        - |  635 | `	VmCallArgMap *pMap;` |
|   668441 |  636 | `	if( !pGen->bStrictTypes ) return p3;` |
|      318 |  637 | `	if( p3 == 0 ){` |
|       44 |  638 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|       44 |  639 | `		if( pMap == 0 ) return 0;` |
|       44 |  640 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|       44 |  641 | `		p3 = (void *)pMap;` |
|       20 |  642 | `	}` |
|      318 |  643 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|      318 |  644 | `	return p3;` |
|   334223 |  645 | `}` |
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
|      154 |  665 | `PH7_PRIVATE int GenStateIsReservedConstant(SyString *pName)` |
|        5 |  666 | `{` |
|      159 |  667 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|       17 |  668 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|        3 |  669 | `			return TRUE;` |
|       15 |  670 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|        5 |  671 | `			return TRUE;` |
|        2 |  672 | `		}` |
|      149 |  673 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       16 |  674 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|        3 |  675 | `			return TRUE;` |
|        - |  676 | `		}` |
|        5 |  677 | `	}` |
|        - |  678 | `	/* Not a reserved constant */` |
|      151 |  679 | `	return FALSE;` |
|       82 |  680 | `}` |
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
|  6252604 |  697 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|        5 |  698 | `{` |
|  6252609 |  699 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|        - |  700 | `	sxu32 nTarget;` |
|        - |  701 | `	sxu32 *aIdx;` |
|        - |  702 | `	sxu32 i;` |
|  6252609 |  703 | `	if( nCur <= nBaseline ){` |
|  6252489 |  704 | `		return;` |
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
|  3126307 |  715 | `}` |
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
|   600086 |  734 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
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
|        - |  756 | `		{ "proc_open",              9, 1u<<2 },  /* &$pipes (apArg[2]) */` |
|        - |  757 | `		{ "exec",                   4, (1u<<1)\|(1u<<2) },  /* &$output, &$result_code */` |
|        - |  758 | `		{ "system",                 6, 1u<<1 },  /* &$result_code (apArg[1]) */` |
|        - |  759 | `		{ "passthru",               8, 1u<<1 },  /* &$result_code (apArg[1]) */` |
|        - |  760 | `	};` |
|        - |  761 | `	sxu32 i;` |
|   600091 |  762 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|    25837 |  763 | `		return 0;` |
|        - |  764 | `	}` |
| 11188983 |  765 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
| 10634358 |  766 | `		if( pName->nByte == aByRef[i].nByte` |
|  5747521 |  767 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|    19639 |  768 | `			return aByRef[i].mask;` |
|        - |  769 | `		}` |
|  5307367 |  770 | `	}` |
|   554625 |  771 | `	return 0;` |
|   300048 |  772 | `}` |
|        - |  773 | `/*` |
|        - |  774 | ` * What may be passed by REFERENCE is decided from the argument's SHAPE, at compile` |
|        - |  775 | ` * time, exactly as php decides it (zend_compile_args -> zend_is_variable).` |
|        - |  776 | ` *` |
|        - |  777 | ` * php sorts every actual argument into three buckets:` |
|        - |  778 | ` *` |
|        - |  779 | ` *   GEN_ARG_LVALUE   a variable, an element, a property, a static property. It has a` |
|        - |  780 | ` *                    slot, so a by-ref parameter aliases it.` |
|        - |  781 | `` *   GEN_ARG_TEMPCALL the result of a call or of `new`. It has no slot, but php cannot`` |
|        - |  782 | ` *                    know at compile time whether the callee returns a reference, so it` |
|        - |  783 | ` *                    defers: E_NOTICE "Only variables should be passed by reference",` |
|        - |  784 | ` *                    then it operates on the temporary.` |
|        - |  785 | ` *   GEN_ARG_NONE     everything else — a literal, an operator/cast result, a class` |
|        - |  786 | `` *                    constant, `@$x`, `$o?->p`, an assignment. Binding one to a by-ref`` |
|        - |  787 | ` *                    parameter is a catchable Error at the CALL.` |
|        - |  788 | ` *` |
|        - |  789 | ` * Deciding it from the argument's runtime memobj instead does not work and was silently` |
|        - |  790 | ` * wrong in both directions: an arithmetic or concatenation result keeps its LEFT operand's` |
|        - |  791 | `` * slot index, so `f($i + 1)` with `function f(&$x)` aliased and overwrote `$i`; and a`` |
|        - |  792 | ` * builtin's by-ref row saw only "no slot", which a call result has too.` |
|        - |  793 | ` */` |
|        - |  794 | `#define GEN_ARG_LVALUE   0` |
|        - |  795 | `#define GEN_ARG_TEMPCALL 1` |
|        - |  796 | `#define GEN_ARG_NONE     2` |
|   827454 |  797 | `static int GenStateArgShape(ph7_expr_node *pNode)` |
|        5 |  798 | `{` |
|   827459 |  799 | `	if( pNode == 0 ){` |
|      ! 0 |  800 | `		return GEN_ARG_NONE;` |
|        - |  801 | `	}` |
|   827459 |  802 | `	if( pNode->pOp == 0 ){` |
|        - |  803 | ``		/* A leaf: only the `$…` family is a variable. Everything else the parser`` |
|        - |  804 | ``		 * files here — a literal, an array/list constructor, a closure, `match`,`` |
|        - |  805 | ``		 * `clone` — is a temporary. */`` |
|   689617 |  806 | `		return pNode->xCode == PH7_CompileVariable ? GEN_ARG_LVALUE : GEN_ARG_NONE;` |
|        - |  807 | `	}` |
|   137847 |  808 | `	switch( pNode->pOp->iOp ){` |
|    10205 |  809 | `	case EXPR_OP_SUBSCRIPT: /* $a[k], and any base: php accepts g()[0] and C::m()[0] */` |
|        - |  810 | `	case EXPR_OP_ARROW:     /* $o->p */` |
|    20415 |  811 | `		return GEN_ARG_LVALUE;` |
|      149 |  812 | `	case EXPR_OP_DC:` |
|        - |  813 | ``		/* `C::$s` is a static property (an lvalue); `C::K` is a class constant and`` |
|        - |  814 | ``		 * `C::CASE` an enum case, neither of which php will bind. The right operand`` |
|        - |  815 | `		 * tells them apart. */` |
|      452 |  816 | `		return ( pNode->pRight && pNode->pRight->pOp == 0` |
|      298 |  817 | `		      && pNode->pRight->xCode == PH7_CompileVariable )` |
|      298 |  818 | `			? GEN_ARG_LVALUE : GEN_ARG_NONE;` |
|    20047 |  819 | `	case EXPR_OP_FUNC_CALL:` |
|        - |  820 | `	case EXPR_OP_NEW:` |
|    40099 |  821 | `		return GEN_ARG_TEMPCALL;` |
|    38520 |  822 | `	default:` |
|        - |  823 | ``		/* Includes `?->` (php: "Cannot use nullsafe operator in write context"),`` |
|        - |  824 | ``		 * `@$x`, `$q = …`, `clone $o` and every arithmetic/logical operator. */`` |
|    77045 |  825 | `		return GEN_ARG_NONE;` |
|        - |  826 | `	}` |
|   413732 |  827 | `}` |
|        - |  828 | `/*` |
|        - |  829 | ` * Recover the bare global-builtin name from a call's callee node.` |
|        - |  830 | ` *` |
|        - |  831 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|        - |  832 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|        - |  833 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|        - |  834 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|        - |  835 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|        - |  836 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|        - |  837 | ` */` |
|  1168066 |  838 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|        5 |  839 | `{` |
|        - |  840 | `	SyToken *p, *pEnd;` |
|  1168071 |  841 | `	pOut->zString = 0;` |
|  1168071 |  842 | `	pOut->nByte = 0;` |
|  1168071 |  843 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|      ! 0 |  844 | `		return;` |
|        - |  845 | `	}` |
|  1168071 |  846 | `	p = pLeft->pStart;` |
|  1168071 |  847 | `	pEnd = pLeft->pEnd;` |
|        - |  848 | `	/* Optional single leading namespace separator (absolute path). */` |
|  1168071 |  849 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|      101 |  850 | `		p++;` |
|       48 |  851 | `	}` |
|  1168071 |  852 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|    29133 |  853 | `		return;` |
|        - |  854 | `	}` |
|        - |  855 | `	/* Must be a single component: nothing follows the name token. */` |
|  1138943 |  856 | `	if( p + 1 != pEnd ){` |
|      117 |  857 | `		return;` |
|        - |  858 | `	}` |
|  1138831 |  859 | `	*pOut = p->sData;` |
|   584038 |  860 | `}` |
|        - |  861 | `/*` |
|        - |  862 | `` * Is this expression node the bare variable `$this`?`` |
|        - |  863 | ` */` |
|   755588 |  864 | `PH7_PRIVATE int PH7_ExprNodeIsThis(ph7_expr_node *pNode)` |
|        5 |  865 | `{` |
|        - |  866 | `	SyToken *pTok;` |
|   755593 |  867 | `	if( pNode == 0 \|\| pNode->pOp != 0 \|\| pNode->xCode != PH7_CompileVariable ){` |
|   110807 |  868 | `		return 0;` |
|        - |  869 | `	}` |
|   644791 |  870 | `	pTok = pNode->pStart;` |
|   644791 |  871 | `	if( pTok == 0 \|\| pNode->pEnd == 0 \|\| pNode->pEnd < &pTok[2] ){` |
|      ! 0 |  872 | `		return 0;` |
|        - |  873 | `	}` |
|   967184 |  874 | `	return (pTok[0].nType & PH7_TK_DOLLAR) != 0` |
|   644786 |  875 | `		&& (pTok[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|   644775 |  876 | `		&& pTok[1].sData.nByte == sizeof("this")-1` |
|   967179 |  877 | `		&& SyMemcmp((const void *)pTok[1].sData.zString,(const void *)"this",sizeof("this")-1) == 0;` |
|   377799 |  878 | `}` |
|        - |  879 | `/*` |
|        - |  880 | ` * The two write-target rules php decides at COMPILE time, in one place because` |
|        - |  881 | ` * every write site has to make both of them.` |
|        - |  882 | ` *` |
|        - |  883 | `` * **`$this`** is not a variable a program may re-point: php refuses the`` |
|        - |  884 | `` * assignment, the reference bind, a foreach/list target and `unset()` where they`` |
|        - |  885 | `` * are WRITTEN. PHL performed all of them, so `$this = 5;` inside a method`` |
|        - |  886 | ` * replaced the receiver with an int for the rest of the call and every later` |
|        - |  887 | `` * `$this->x` failed somewhere else entirely.`` |
|        - |  888 | ` *` |
|        - |  889 | ``  * **A temporary** cannot be written THROUGH: `(new A)->p = 1` and `"s"->p = 1` `` |
|        - |  890 | ` * modify an object/value that no longer exists after the statement, so php` |
|        - |  891 | `` * refuses the whole chain — every write kind, including `+=`, `++`, `=&` and`` |
|        - |  892 | `` * `unset()`. The base of the access chain decides: a variable and a userland`` |
|        - |  893 | `` * CALL are writable (`f()->p = 1` is php-legal), a `new`, a literal and any`` |
|        - |  894 | ` * other computed value are not, and an internal function's result gets php's own` |
|        - |  895 | `` * separate wording — which is what `(clone $o)->p = 1` is, `clone` being a`` |
|        - |  896 | ` * function in php 8.5.` |
|        - |  897 | ` */` |
|   755588 |  898 | `PH7_PRIVATE sxi32 GenStateWriteTargetCheck(ph7_gen_state *pGen,ph7_expr_node *pTarget,int bUnset)` |
|        5 |  899 | `{` |
|   755593 |  900 | `	ph7_expr_node *pBase = pTarget;` |
|   755593 |  901 | `	const char *zMsg = 0;` |
|        - |  902 | `	sxi32 rc;` |
|   755593 |  903 | `	if( pTarget == 0 ){` |
|      ! 0 |  904 | `		return SXRET_OK;` |
|        - |  905 | `	}` |
|   755593 |  906 | `	if( PH7_ExprNodeIsThis(pTarget) ){` |
|       11 |  907 | `		zMsg = bUnset ? "Cannot unset $this" : "Cannot re-assign $this";` |
|        7 |  908 | `	}else{` |
|        - |  909 | `		/* Walk to the base of the access chain; the links themselves are writable. */` |
|   866433 |  910 | `		while( pBase && pBase->pOp ){` |
|   110941 |  911 | `			if( pBase->pOp->iOp == EXPR_OP_DC ){` |
|        - |  912 | `` 				/* A `::` left operand is a CLASS reference, not a value — `C::$s = 1` `` |
|        - |  913 | ``				 * and even `(new C)::$s = 1` write class-level storage that outlives`` |
|        - |  914 | `				 * any temporary, so the chain stops being about a base here. */` |
|       75 |  915 | `				return SXRET_OK;` |
|        - |  916 | `			}` |
|   110866 |  917 | `			if( pBase->pOp->iOp != EXPR_OP_ARROW && pBase->pOp->iOp != EXPR_OP_NULLSAFE_ARROW` |
|   109306 |  918 | `			 && pBase->pOp->iOp != EXPR_OP_SUBSCRIPT ){` |
|       23 |  919 | `				break;` |
|        - |  920 | `			}` |
|   110853 |  921 | `			pBase = pBase->pLeft;` |
|        5 |  922 | `		}` |
|   755515 |  923 | `		if( pBase == 0 \|\| pBase == pTarget ){` |
|        - |  924 | `			/* No chain: a non-variable target of its own is the caller's business` |
|        - |  925 | `			 * (php reports its parse error / "Assignments can only happen to` |
|        - |  926 | `			 * writable values" there, and so does PHL). */` |
|   644957 |  927 | `			return SXRET_OK;` |
|        - |  928 | `		}` |
|   110563 |  929 | `		if( pBase->pOp == 0 ){` |
|   110555 |  930 | `			if( pBase->xCode != PH7_CompileVariable ){` |
|      ! 0 |  931 | `				zMsg = "Cannot use temporary expression in write context";` |
|        5 |  932 | `			}` |
|    55286 |  933 | `		}else if( pBase->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|        - |  934 | `			/* php: the result of an INTERNAL function is not writable through,` |
|        - |  935 | `			 * a userland one is. */` |
|        - |  936 | `			SyString sName;` |
|        5 |  937 | `			GenStateCallBuiltinName(pBase,&sName);` |
|        4 |  938 | `			if( sName.nByte > 0 && pGen->pVm` |
|        1 |  939 | `			 && SyHashGet(&pGen->pVm->hHostFunction,(const void *)sName.zString,sName.nByte) ){` |
|      ! 0 |  940 | `				zMsg = "Cannot use result of built-in function in write context";` |
|        1 |  941 | `			}` |
|        8 |  942 | `		}else if( pBase->pOp->iOp == EXPR_OP_CLONE ){` |
|        - |  943 | ``			/* php 8.5 implements `clone` AS a function, so a write through its result`` |
|        - |  944 | `			 * takes the internal-function wording rather than the temporary one. */` |
|        3 |  945 | `			zMsg = "Cannot use result of built-in function in write context";` |
|        2 |  946 | `		}else{` |
|        - |  947 | ``			/* `new`, and every other computed base. */`` |
|        3 |  948 | `			zMsg = "Cannot use temporary expression in write context";` |
|        - |  949 | `		}` |
|        - |  950 | `	}` |
|   110571 |  951 | `	if( zMsg == 0 ){` |
|   110559 |  952 | `		return SXRET_OK;` |
|        - |  953 | `	}` |
|       16 |  954 | `	rc = PH7_GenCompileError(&(*pGen),E_ERROR,` |
|       12 |  955 | `		pTarget->pStart ? pTarget->pStart->nLine : 0,"%s",zMsg);` |
|       16 |  956 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_INVALID;` |
|   377799 |  957 | `}` |
|        - |  958 | `/*` |
|        - |  959 | ` * What emitting a call's ARGUMENT LIST decided, handed back to the CALL codegen.` |
|        - |  960 | ` * The arguments are emitted from their own routine because php evaluates them` |
|        - |  961 | ` * AFTER the callee has been resolved, so this runs between the callee's emission` |
|        - |  962 | ` * and the OP_CALL — see GenStateEmitCallArgs.` |
|        - |  963 | ` */` |
|        - |  964 | `typedef struct GenCallArgs GenCallArgs;` |
|        - |  965 | `struct GenCallArgs {` |
|        - |  966 | `	sxi32 iP1;      /* OP_CALL.iP1: the compile-time argument count */` |
|        - |  967 | ``	sxu32 iP2;      /* OP_CALL.iP2: 1 if any argument unpacks (`...$a`) */`` |
|        - |  968 | `	void *p3;       /* OP_CALL.p3: the VmCallArgMap, which may ALREADY carry the callee's` |
|        - |  969 | `	                 * namespace qualification — the callee is emitted first now */` |
|        - |  970 | ``	int bFcc;       /* First-class callable `f(...)`: no arguments, OP_LOAD_FCC follows */`` |
|        - |  971 | ``	int bAnySpread; /* Any `...` argument (iP2 says the same; kept for the shape masks) */`` |
|        - |  972 | `};` |
|        - |  973 | `static sxi32 GenStateEmitCallArgs(ph7_gen_state *pGen,ph7_expr_node *pNode,sxi32 iFlags,` |
|        - |  974 | `	GenCallArgs *pArgs);` |
|        - |  975 | `/*` |
|        - |  976 | ` * Generate bytecode for a given expression tree.` |
|        - |  977 | ` * If something goes wrong while generating bytecode` |
|        - |  978 | ` * for the expression tree (A very unlikely scenario)` |
|        - |  979 | ` * this function takes care of generating the appropriate` |
|        - |  980 | ` * error message.` |
|        - |  981 | ` */` |
|  7100234 |  982 | `static sxi32 GenStateEmitExprCode(` |
|        - |  983 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - |  984 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|        - |  985 | `	sxi32 iFlags /* Control flags */` |
|        - |  986 | `	)` |
|        5 |  987 | `{` |
|        - |  988 | `	VmInstr *pInstr;` |
|        - |  989 | `	sxu32 nJmpIdx;` |
|  7100239 |  990 | `	sxi32 iP1 = 0;` |
|  7100239 |  991 | `	sxu32 iP2 = 0;` |
|  7100239 |  992 | `	void *p3  = 0;` |
|        - |  993 | `	sxi32 iVmOp;` |
|        - |  994 | `	sxi32 rc;` |
|  7100239 |  995 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
|  7100239 |  996 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
|  7100239 |  997 | `	sxu32 nRhsNsBase = 0;` |
|        - |  998 | ``	/* Consumed here so it describes THIS node only — the direct operand of a `new` —`` |
|        - |  999 | `	 * and never travels down into the operand's own sub-expressions. */` |
|  7100239 | 1000 | `	int bNewCallee = (iFlags & EXPR_FLAG_NEW_CALLEE) != 0;` |
|  7100239 | 1001 | `	iFlags &= ~EXPR_FLAG_NEW_CALLEE;` |
|  7100239 | 1002 | `	if( pNode->xCode ){` |
|        - | 1003 | `		SyToken *pTmpIn,*pTmpEnd;` |
|        - | 1004 | `		/* Compile node */` |
|  4397377 | 1005 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
|  4397377 | 1006 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
|  4397377 | 1007 | `		RE_SWAP_DELIMITER(pGen);` |
|  4397377 | 1008 | `		return rc;` |
|        - | 1009 | `	}` |
|  2702867 | 1010 | `	if( pNode->pOp == 0 ){` |
|      ! 0 | 1011 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|        - | 1012 | `			"Invalid expression node,PH7 is aborting compilation");` |
|      ! 0 | 1013 | `		return SXERR_ABORT;` |
|        - | 1014 | `	}` |
|  2702867 | 1015 | `	iVmOp = pNode->pOp->iVmOp;` |
|  2702867 | 1016 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|        - | 1017 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|        - | 1018 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|        - | 1019 | `		 * and later errors are still reported. */` |
|        3 | 1020 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|        - | 1021 | `			"The (unset) cast is no longer supported");` |
|        3 | 1022 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1023 | `			return SXERR_ABORT;` |
|        - | 1024 | `		}` |
|        1 | 1025 | `	}` |
|  2702867 | 1026 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|      157 | 1027 | `		sxu32 nJmp = 0;` |
|        - | 1028 | `		sxu32 nNcNsBase;` |
|        - | 1029 | `		VmInstr *pInstrFix;` |
|        - | 1030 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|        - | 1031 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|        - | 1032 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|        - | 1033 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|        - | 1034 | `		 * stack slot carries a writable nIdx. */` |
|      157 | 1035 | `		if( pNode->pRight ){` |
|        - | 1036 | ``			/* `$a[] ??= v` READS its target before deciding to write, so php refuses the`` |
|        - | 1037 | ``			 * append form anywhere in that target's chain (`$a[][0] ??= v` too), even`` |
|        - | 1038 | ``			 * though the same tag makes every other `[]` on this path a legal write`` |
|        - | 1039 | ``			 * target. Only the container chain is walked — a `[]` inside an INDEX`` |
|        - | 1040 | `			 * expression is an ordinary read and the subscript codegen refuses it. */` |
|      157 | 1041 | `			ph7_expr_node *pTgt = pNode->pRight;` |
|      352 | 1042 | `			while( pTgt && pTgt->pOp && (pTgt->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|       84 | 1043 | `			      \|\| pTgt->pOp->iOp == EXPR_OP_ARROW \|\| pTgt->pOp->iOp == EXPR_OP_DC) ){` |
|      133 | 1044 | `				if( pTgt->pOp->iOp == EXPR_OP_SUBSCRIPT && SySetUsed(&pTgt->aNodeArgs) < 1 ){` |
|      ! 0 | 1045 | `					break;` |
|        - | 1046 | `				}` |
|      133 | 1047 | `				pTgt = pTgt->pLeft;` |
|        3 | 1048 | `			}` |
|      154 | 1049 | `			if( pTgt && pTgt->pOp && pTgt->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|        3 | 1050 | `			 && SySetUsed(&pTgt->aNodeArgs) < 1 ){` |
|      ! 0 | 1051 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,` |
|      ! 0 | 1052 | `					pNode->pRight->pStart ? pNode->pRight->pStart->nLine : 0,` |
|        - | 1053 | `					"Cannot use [] for reading");` |
|      ! 0 | 1054 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1055 | `			}` |
|      157 | 1056 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      157 | 1057 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|      157 | 1058 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1059 | `				return rc;` |
|        - | 1060 | `			}` |
|      157 | 1061 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|        - | 1062 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|        - | 1063 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|        - | 1064 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|        - | 1065 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|        - | 1066 | `			 * the store, so the parent array does not need to be copied at` |
|        - | 1067 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|        - | 1068 | `			 * cascade for the actual write path stays correct. */` |
|      157 | 1069 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|      157 | 1070 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|       89 | 1071 | `				pInstrFix->iP2 = 3;` |
|       43 | 1072 | `			}` |
|       77 | 1073 | `		}` |
|        - | 1074 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|      157 | 1075 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|        - | 1076 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|      157 | 1077 | `		if( pNode->pLeft ){` |
|      157 | 1078 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      157 | 1079 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|      157 | 1080 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1081 | `				return rc;` |
|        - | 1082 | `			}` |
|      157 | 1083 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|       77 | 1084 | `		}` |
|        - | 1085 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|      157 | 1086 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|        - | 1087 | `		/* Patch the short-circuit jump to land after the store. */` |
|      157 | 1088 | `		if( nJmp > 0 ){` |
|      157 | 1089 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|      157 | 1090 | `			if( pInstrFix ){` |
|      157 | 1091 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|       77 | 1092 | `			}` |
|       77 | 1093 | `		}` |
|      157 | 1094 | `		return SXRET_OK;` |
|        - | 1095 | `	}` |
|  2702713 | 1096 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|        - | 1097 | `		sxu32 nJz,nJmp;` |
|        - | 1098 | `		sxu32 nTernaryNsBase;` |
|        - | 1099 | `		/* Ternary operator require special handling */` |
|        - | 1100 | `		/* Phase#1: Compile the condition */` |
|    31483 | 1101 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    31483 | 1102 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    31483 | 1103 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1104 | `			return rc;` |
|        - | 1105 | `		}` |
|        - | 1106 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|        - | 1107 | `		 * compiling the condition must short-circuit to the end of the` |
|        - | 1108 | `		 * condition expression, not leak past the ternary. */` |
|    31483 | 1109 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    31483 | 1110 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    31483 | 1111 | `		if( pNode->pLeft ){` |
|        - | 1112 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|        - | 1113 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    31411 | 1114 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|        - | 1115 | `			/* Phase#3: Compile the 'then' expression  */` |
|    31411 | 1116 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    31411 | 1117 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    31411 | 1118 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1119 | `				return rc;` |
|        - | 1120 | `			}` |
|    31411 | 1121 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    15708 | 1122 | `		}else{` |
|        - | 1123 | `			/* Elvis operator: (expr) ?: (else)` |
|        - | 1124 | `			 * Duplicate condition so original value is the 'then' result.` |
|        - | 1125 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|       75 | 1126 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       75 | 1127 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|        - | 1128 | `		}` |
|        - | 1129 | `		/* Phase#4: Emit the unconditional jump */` |
|    31483 | 1130 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|        - | 1131 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    31483 | 1132 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    31483 | 1133 | `		if( pInstr ){` |
|    31483 | 1134 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    15739 | 1135 | `		}` |
|    31483 | 1136 | `		if( !pNode->pLeft ){` |
|        - | 1137 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|       75 | 1138 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       36 | 1139 | `		}` |
|        - | 1140 | `		/* Phase#6: Compile the 'else' expression */` |
|    31483 | 1141 | `		if( pNode->pRight ){` |
|    31483 | 1142 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    31483 | 1143 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    31483 | 1144 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1145 | `				return rc;` |
|        - | 1146 | `			}` |
|    31483 | 1147 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    15739 | 1148 | `		}` |
|    31483 | 1149 | `		if( nJmp > 0 ){` |
|        - | 1150 | `			/* Phase#7: Fix the unconditional jump */` |
|    31483 | 1151 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    31483 | 1152 | `			if( pInstr ){` |
|    31483 | 1153 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    15739 | 1154 | `			}` |
|    15739 | 1155 | `		}` |
|        - | 1156 | `		/* All done */` |
|    31483 | 1157 | `		return SXRET_OK;` |
|        - | 1158 | `	}` |
|  2671235 | 1159 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
|        - | 1160 | ``		/* PHP 8.5 pipe: `$lhs \|> $rhs` invokes the RHS callable with the LHS`` |
|        - | 1161 | ``		 * value as its sole argument [i.e. `$rhs($lhs)`]. Evaluate the LHS (the`` |
|        - | 1162 | `		 * argument) first, then the RHS callable, then emit a one-argument` |
|        - | 1163 | `		 * OP_CALL — the same stack shape the function-call path builds (the` |
|        - | 1164 | `		 * argument sits below the callee). The RHS is any callable expression:` |
|        - | 1165 | ``		 * an FCC `f(...)` (an OP_LOAD_FCC Closure), a closure variable, an`` |
|        - | 1166 | ``		 * `[obj,method]` pair, or a callable string. */`` |
|        - | 1167 | `		sxu32 nPipeNsBase;` |
|       27 | 1168 | `		sxi32 iOperandFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE\|EXPR_FLAG_RDONLY_LOAD);` |
|       27 | 1169 | `		if( pNode->pLeft == 0 \|\| pNode->pRight == 0 ){` |
|      ! 0 | 1170 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|        - | 1171 | `				"'\|>': Missing operand");` |
|      ! 0 | 1172 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1173 | `		}` |
|        - | 1174 | `		/* Argument: the LHS value. */` |
|       27 | 1175 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       27 | 1176 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iOperandFlags);` |
|       27 | 1177 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1178 | `			return rc;` |
|        - | 1179 | `		}` |
|       27 | 1180 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|        - | 1181 | `		/* Callable: the RHS. */` |
|       27 | 1182 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       27 | 1183 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iOperandFlags);` |
|       27 | 1184 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1185 | `			return rc;` |
|        - | 1186 | `		}` |
|       27 | 1187 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|        - | 1188 | `		/* Invoke the callable with the single piped argument. */` |
|       27 | 1189 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       27 | 1190 | `		return SXRET_OK;` |
|        - | 1191 | `	}` |
|  2671209 | 1192 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|        - | 1193 | `	/* Generate code for the left tree */` |
|  2671209 | 1194 | `	if( pNode->pLeft ){` |
|  2671173 | 1195 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        - | 1196 | `		GenCallArgs sArgs;` |
|  2671173 | 1197 | `		int bArgsEmitted = 0;` |
|  2671173 | 1198 | ``		sxu32 nNewClassInstr = 0; /* index+1 of a `new` operand's class-name push */`` |
|  2671173 | 1199 | `		SyZero(&sArgs,sizeof(sArgs));` |
|        - | 1200 | `		{` |
|        - | 1201 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|        - | 1202 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|        - | 1203 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|        - | 1204 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|        - | 1205 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|        - | 1206 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|        - | 1207 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|        - | 1208 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
|  2671173 | 1209 | `			sxi32 iLeftFlags = iFlags;` |
|  2671173 | 1210 | `			sxu32 nNullcLhsFirst = PH7_VmInstrLength(pGen->pVm);` |
|  2671173 | 1211 | `			int bNullcLhs = 0;` |
|        - | 1212 | `			/* D1 commit 2: a deferred element/property call arg records its lvalue chain, but` |
|        - | 1213 | `			 * that chain must be CONTIGUOUS. Only propagate DEFER_ARG to the base when the base` |
|        - | 1214 | `			 * is itself a continuable lvalue — a plain variable (an undefined base auto-defers),` |
|        - | 1215 | ``			 * another subscript, or a `->` member. If the base is anything else (most importantly`` |
|        - | 1216 | ``			 * a method CALL, e.g. `$o->items()->prop` or `$r->attributes->item(0)->nodeName`),`` |
|        - | 1217 | `			 * strip DEFER so that intermediate read is a NORMAL read, not a record-mode carrier. */` |
|  2671173 | 1218 | `			if( iLeftFlags & EXPR_FLAG_DEFER_ARG ){` |
|    21237 | 1219 | `				int bContinuable = pNode->pLeft` |
|    16028 | 1220 | `					&& ( (pNode->pLeft->pOp == 0 && pNode->pLeft->xCode == PH7_CompileVariable)` |
|     5506 | 1221 | `					  \|\| (pNode->pLeft->pOp != 0 && (pNode->pLeft->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|      170 | 1222 | `					                              \|\| pNode->pLeft->pOp->iOp == EXPR_OP_ARROW)) );` |
|    10621 | 1223 | `				if( !bContinuable ){` |
|      112 | 1224 | `					iLeftFlags &= ~EXPR_FLAG_DEFER_ARG;` |
|       54 | 1225 | `				}` |
|     5308 | 1226 | `			}` |
|  2671168 | 1227 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
|  2215975 | 1228 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|   880423 | 1229 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|   868495 | 1230 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|    25069 | 1231 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|  2658641 | 1232 | `			}else if( iLeftFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|        - | 1233 | ``				/* A SUBSCRIPT intermediate of an unset chain (`unset($a['k']['n'])`) keeps`` |
|        - | 1234 | `				 * the unset context — it must COW-separate the parent and must NOT vivify a` |
|        - | 1235 | `				 * missing key — but it is a READ of the container, not an unset of it. The` |
|        - | 1236 | ``				 * UNSET_BASE context says exactly that: `$a['k']` is loaded, where the`` |
|        - | 1237 | `				 * plain unset context would have removed the ELEMENT (and, for an` |
|        - | 1238 | `				 * ArrayAccess base, called offsetUnset() on the intermediate key). */` |
|      203 | 1239 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|      203 | 1240 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_UNSET_BASE;` |
|       99 | 1241 | `			}` |
|        - | 1242 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|        - | 1243 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|        - | 1244 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|        - | 1245 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|        - | 1246 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|        - | 1247 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|        - | 1248 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
|  2671168 | 1249 | `			if( pNode->pOp` |
|  3993080 | 1250 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
|  2657555 | 1251 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  2643878 | 1252 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|    29637 | 1253 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|    14816 | 1254 | `			}` |
|        - | 1255 | ``			/* `++`/`--` mutate their operand in place — the operand is a write`` |
|        - | 1256 | ``			 * lvalue exactly like a compound assign's (`$o->m[0]++` must tag the`` |
|        - | 1257 | ``			 * member base PH7_MEMBER_WRITE the way `$o->m[0] += 1` does: hooked`` |
|        - | 1258 | `			 * properties throw php's Indirect-modification Error, missing ones` |
|        - | 1259 | `			 * auto-vivify). The prec-18 site below handles the assign family;` |
|        - | 1260 | ``			 * `++`/`--` are unary, their operand is pLeft. */`` |
|  2671168 | 1261 | `			if( pNode->pOp` |
|  2671173 | 1262 | `				&& (pNode->pOp->iVmOp == PH7_OP_INCR \|\| pNode->pOp->iVmOp == PH7_OP_DECR) ){` |
|        - | 1263 | ``				/* `(new A)->p++` writes through a temporary exactly as `= 1` does. */`` |
|    42729 | 1264 | `				rc = GenStateWriteTargetCheck(&(*pGen),pNode->pLeft,0);` |
|    42729 | 1265 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 1266 | `					return rc;` |
|        - | 1267 | `				}` |
|    42729 | 1268 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE` |
|        - | 1269 | ``					\| EXPR_FLAG_RMW_LOAD /* php warns before seeding `$undef++` */;`` |
|    21362 | 1270 | `			}` |
|        - | 1271 | ``			/* `??` reads its LEFT operand in isset-context: an undefined or`` |
|        - | 1272 | `			 * UNINITIALIZED typed PROPERTY must yield the default rather than a` |
|        - | 1273 | `			 * warning/Error (php). Tag a member-access LHS so its OP_MEMBER takes` |
|        - | 1274 | `			 * the silent-lookup path (iP2 = ISSET), which still loads a present` |
|        - | 1275 | `			 * value. A SUBSCRIPT LHS is left untagged: LOAD_IDX's ISSET mode means` |
|        - | 1276 | ``			 * offsetExists (a bool), but `$o[$k] ?? d` needs the offsetGet value —`` |
|        - | 1277 | `			 * that path is already handled correctly by OP_NULLC. */` |
|  2671173 | 1278 | `			if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC && pNode->pLeft ){` |
|        - | 1279 | ``				/* php reads the ENTIRE left operand of `??` in isset-context: no`` |
|        - | 1280 | ``				 * "Undefined variable" for `$x ?? d` OR for the base of`` |
|        - | 1281 | ``				 * `$x['k'] ?? d`. QUIET_VAR silences the variable read wherever it`` |
|        - | 1282 | `				 * sits in the chain. */` |
|      313 | 1283 | `				iLeftFlags \|= EXPR_FLAG_QUIET_VAR;` |
|      313 | 1284 | `				bNullcLhs = 1;` |
|      308 | 1285 | `				if( pNode->pLeft->pOp` |
|      365 | 1286 | `					&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|      218 | 1287 | `						\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|      188 | 1288 | `						\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|        - | 1289 | `					/* A member-access LHS additionally takes OP_MEMBER's SILENT` |
|        - | 1290 | `					 * lookup so an uninitialized typed property yields the default` |
|        - | 1291 | `					 * instead of an Error. It used to borrow isset()'s context for` |
|        - | 1292 | `					 * that, which is the same mistake the comment below records for` |
|        - | 1293 | `					 * subscripts: silence is shared, but isset() context makes every` |
|        - | 1294 | ``					 * ACCESSOR answer a truth, and `$o->p ?? d` needs the accessor's`` |
|        - | 1295 | ``					 * VALUE — so `??` has its own member context. A SUBSCRIPT LHS`` |
|        - | 1296 | `					 * still takes neither: LOAD_IDX's ISSET mode means offsetExists` |
|        - | 1297 | ``					 * (a bool), while `$o[$k] ?? d` needs the offsetGet value —`` |
|        - | 1298 | `					 * OP_NULLC already handles that path. */` |
|       64 | 1299 | `					iLeftFlags \|= EXPR_FLAG_MEMBER_COALESCE;` |
|       31 | 1300 | `				}` |
|      154 | 1301 | `			}` |
|  2671173 | 1302 | `			if( iVmOp == PH7_OP_ERR_CTRL ){` |
|        - | 1303 | `				/* '@' must suppress the diagnostics raised WHILE its operand runs, so` |
|        - | 1304 | `				 * open the window here; the trailing emit below closes it (iP1 = 0). */` |
|    14405 | 1305 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_ERR_CTRL,1,0,0,0);` |
|     7200 | 1306 | `			}` |
|  2671173 | 1307 | `			if( iVmOp == PH7_OP_NEW ){` |
|        - | 1308 | ``				/* Mark the direct operand so a call node under it (`new C($a)`) keeps the`` |
|        - | 1309 | `				 * emission order OP_NEW is assembled from — see the bNewCallee branch above. */` |
|    67643 | 1310 | `				iLeftFlags \|= EXPR_FLAG_NEW_CALLEE;` |
|    33819 | 1311 | `			}` |
|  2671173 | 1312 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags\|EXPR_FLAG_RDONLY_LOAD);` |
|  2671173 | 1313 | `			if( rc == SXRET_OK && bNullcLhs ){` |
|        - | 1314 | ``				/* Mark EVERY subscript read in the `??` left chain quiet (iP2=8).`` |
|        - | 1315 | `				 * Peeking at the next instruction only catches the OUTERMOST` |
|        - | 1316 | ``				 * access, so `$d['x']['y'] ?? $v` still warned for the inner one;`` |
|        - | 1317 | `				 * and a forward scan would be unsound, since an unrelated sibling` |
|        - | 1318 | ``				 * access can sit immediately before a coalesce (`f($a['k'],`` |
|        - | 1319 | ``				 * $b['j'] ?? 1)`). The compiler knows the real extent, so it marks`` |
|        - | 1320 | `				 * the range it just emitted. Write-context codes (1/3/5) belong to` |
|        - | 1321 | ``				 * `??=` and keep their meaning. */`` |
|      313 | 1322 | `				sxu32 nEnd = PH7_VmInstrLength(pGen->pVm);` |
|        - | 1323 | `				sxu32 nAt;` |
|     1269 | 1324 | `				for( nAt = nNullcLhsFirst ; nAt < nEnd ; ++nAt ){` |
|      961 | 1325 | `					VmInstr *pFix = PH7_VmGetInstr(pGen->pVm,nAt);` |
|      961 | 1326 | `					if( pFix && pFix->iOp == PH7_OP_LOAD_IDX && pFix->iP2 == 0 ){` |
|      165 | 1327 | `						pFix->iP2 = 8;` |
|       80 | 1328 | `					}` |
|      483 | 1329 | `				}` |
|      154 | 1330 | `			}` |
|        - | 1331 | `		}` |
|  2671173 | 1332 | `		if( rc != SXRET_OK ){` |
|       67 | 1333 | `			return rc;` |
|        - | 1334 | `		}` |
|  2671111 | 1335 | `		if( !bIsChainOp ){` |
|        - | 1336 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|        - | 1337 | `			 * target the end of that LHS chain, which is right here. */` |
|  1853091 | 1338 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|   926543 | 1339 | `		}` |
|  2671111 | 1340 | `		if( iVmOp == PH7_OP_CALL ){` |
|   600685 | 1341 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   600685 | 1342 | `			if( pInstr ){` |
|   600685 | 1343 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|   574809 | 1344 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|        - | 1345 | `					sxu32 nQual;` |
|   574809 | 1346 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|        - | 1347 | `					/* Prevent constant expansion but preserve the absolute flag` |
|        - | 1348 | `					 * so the later NEW handler (if any) can see it. */` |
|   574809 | 1349 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|        - | 1350 | `					/* Namespace-qualify the function name for CALL, unless the` |
|        - | 1351 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|        - | 1352 | `					 * imports — class imports must NOT affect function` |
|        - | 1353 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|        - | 1354 | `					 * before NEW; we store the original literal index in the` |
|        - | 1355 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|        - | 1356 | `					 * the unqualified name and re-qualify with class imports. */` |
|   574809 | 1357 | `					if( bAbsolute ){` |
|       83 | 1358 | `						pInstr->iP2 = (sxi32)nOrig;` |
|       44 | 1359 | `					}else{` |
|   574731 | 1360 | `						int fromImport = 0;` |
|   574731 | 1361 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|   574731 | 1362 | `						pInstr->iP2 = (sxi32)nQual;` |
|   574731 | 1363 | `						if( nQual != nOrig ){` |
|        - | 1364 | `							/* Record the original literal index in the arg map` |
|        - | 1365 | `							 * (NOT in the CALL's iP2 — that is the hasSpread` |
|        - | 1366 | `							 * flag) so the NEW handler can recover the` |
|        - | 1367 | `							 * unqualified name and re-qualify with CLASS` |
|        - | 1368 | `							 * imports. */` |
|      167 | 1369 | `							if( p3 == 0 ){` |
|      167 | 1370 | `								VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      162 | 1371 | `									&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|      167 | 1372 | `								if( pMap ){` |
|      167 | 1373 | `									SyZero(pMap, sizeof(VmCallArgMap));` |
|      167 | 1374 | `									p3 = (void *)pMap;` |
|       81 | 1375 | `								}` |
|       81 | 1376 | `							}` |
|      167 | 1377 | `							if( p3 ){` |
|      167 | 1378 | `								((VmCallArgMap *)p3)->nOrigNameLit = nOrig + 1;` |
|      167 | 1379 | `								if( !fromImport ){` |
|        - | 1380 | `									/* Mark as namespace-qualified */` |
|      143 | 1381 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|       69 | 1382 | `								}` |
|       81 | 1383 | `							}` |
|       81 | 1384 | `						}` |
|        - | 1385 | `					}` |
|   313283 | 1386 | `				}else if( (pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */` |
|    24869 | 1387 | `						&& !(pNode->pLeft && (pNode->pLeft->iFlags & EXPR_NODE_PARENS)))` |
|    13965 | 1388 | `					\|\| pInstr->iOp == PH7_OP_NEW ){` |
|        - | 1389 | `					/* Method call,flag that. But NOT when the callee was an explicitly` |
|        - | 1390 | ``					 * PARENTHESISED member access: `($o->p)(...)` / `($o::$p)(...)` invokes`` |
|        - | 1391 | `					 * the VALUE of the property (php's variable-invocation), so the OP_MEMBER` |
|        - | 1392 | `					 * must stay a plain property READ (leaving the callable on the stack for` |
|        - | 1393 | `					 * OP_CALL to invoke) rather than being rewritten into a method-name` |
|        - | 1394 | ``					 * resolution — the parens are exactly what distinguishes `($o->p)()` from`` |
|        - | 1395 | ``					 * the method call `$o->p()`. */`` |
|    23843 | 1396 | `					pInstr->iP2 = 1;` |
|        - | 1397 | ``					/* A static call with a DYNAMIC method name (`C::$m(...)`): the`` |
|        - | 1398 | ``					 * static-`::` codegen folded the variable NAME into OP_MEMBER->p3`` |
|        - | 1399 | ``					 * as if it were a static-PROPERTY read (`C::$m`), but in a CALL the`` |
|        - | 1400 | `					 * method name is the variable's VALUE. Rebuild the sequence` |
|        - | 1401 | `					 * [class, OP_LOAD $m -> value, OP_MEMBER(static, method)] so the` |
|        - | 1402 | `					 * dynamic name is read off the stack, matching the instance` |
|        - | 1403 | ``					 * (`$o->$m()`) path. iP1==1 marks a static member. */`` |
|    23843 | 1404 | `					if( pInstr->iOp == PH7_OP_MEMBER && pInstr->iP1 == 1 && pInstr->p3 ){` |
|       17 | 1405 | `						void *pDynName = pInstr->p3;` |
|       17 | 1406 | `						(void)PH7_VmPopInstr(pGen->pVm);` |
|       17 | 1407 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,pDynName,0);` |
|       17 | 1408 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_MEMBER,1,PH7_MEMBER_METHOD,0,0);` |
|        8 | 1409 | `					}` |
|    11919 | 1410 | `				}` |
|   300340 | 1411 | `			}` |
|        - | 1412 | `			/* The callee is resolved; NOW emit the arguments. php's order — the callee` |
|        - | 1413 | `			 * first, at its INIT_FCALL / INIT_METHOD_CALL, and the arguments only after` |
|        - | 1414 | ``			 * it — is what makes `(new C)->priv(boom())` report php's `Call to private`` |
|        - | 1415 | `` 			 * method` instead of whatever the argument threw, and what keeps `$o?->m(f())` `` |
|        - | 1416 | ``			 * from running `f()` on a null receiver. It also puts the callee in reach of`` |
|        - | 1417 | `			 * the argument ops one opcode EARLIER than OP_CALL.` |
|        - | 1418 | `			 *` |
|        - | 1419 | `			 * The stack that leaves here is therefore [callee][args…] — the mirror of the` |
|        - | 1420 | `			 * layout OP_CALL's whole dispatch is written against (the method-name pair` |
|        - | 1421 | `			 * below the arguments, the spread runs counted down from the top, the` |
|        - | 1422 | `			 * deferred-argument re-walk). OP_ROT_CALLEE turns the region back over just` |
|        - | 1423 | `			 * before the call, so nothing downstream of it changes. */` |
|   600685 | 1424 | `			if( !bArgsEmitted ){` |
|        - | 1425 | `				int bTwoSlot;` |
|   600685 | 1426 | `				sArgs.p3 = p3; /* the namespace map built just above, if any */` |
|        - | 1427 | `				/* A METHOD callee leaves TWO slots — [receiver][method name] — which` |
|        - | 1428 | `				 * OP_CALL reads as one callee (the receiver answers $this and the` |
|        - | 1429 | `				 * late-static-binding class); anything else leaves one. The instruction` |
|        - | 1430 | `				 * just emitted is what decides it. */` |
|   600685 | 1431 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   612616 | 1432 | `				bTwoSlot = pInstr && pInstr->iOp == PH7_OP_MEMBER` |
|   312271 | 1433 | `					&& pInstr->iP2 == PH7_MEMBER_METHOD` |
|        - | 1434 | `					/* …unless the member NAME was folded into p3 rather than pushed:` |
|        - | 1435 | `					 * that shape pushes the target alone, so the op leaves one slot,` |
|        - | 1436 | `					 * which is the same distinction vm_ops_oo.c makes before popping. */` |
|   901020 | 1437 | `					&& pInstr->p3 == 0;` |
|        - | 1438 | `				/* Screen the callee HERE, where php screens it: an undefined function, a` |
|        - | 1439 | `				 * callable string/array naming nothing, a value that is not callable at` |
|        - | 1440 | `				 * all. A METHOD callee needs none — its OP_MEMBER just did it, against` |
|        - | 1441 | `				 * the entry it chose. An FCC needs none either: OP_LOAD_FCC runs the same` |
|        - | 1442 | `				 * screen, and nothing runs before it. And with NO arguments the call` |
|        - | 1443 | `				 * itself is already the first thing to happen, so there is nothing to` |
|        - | 1444 | `				 * order and no reason to pay for a second resolution. */` |
|        - | 1445 | `				{` |
|   600685 | 1446 | `					ph7_expr_node **apCallArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   600685 | 1447 | `					sxi32 nCallArg = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|   693119 | 1448 | `					int bNodeFcc = nCallArg == 1 && apCallArg[0]` |
|   797067 | 1449 | `						&& (apCallArg[0]->iFlags & EXPR_NODE_FCC);` |
|   600685 | 1450 | `					if( bNewCallee ){` |
|        - | 1451 | ``						/* A `new`'s operand: the screen is OP_NEW itself, run with no`` |
|        - | 1452 | `						 * arguments on the stack (iP1 = -1). It asks every refusal the` |
|        - | 1453 | `						 * real pass asks and leaves the class name standing, so the two` |
|        - | 1454 | `						 * cannot disagree. Record where that push is — the NEW codegen` |
|        - | 1455 | `						 * used to find it one instruction behind the trailing OP_CALL,` |
|        - | 1456 | `						 * and the argument list now sits in between. */` |
|    66139 | 1457 | `						nNewClassInstr = PH7_VmInstrLength(pGen->pVm);` |
|    66139 | 1458 | `						if( nCallArg > 0 ){` |
|    63963 | 1459 | `							PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,-1,0,0,0);` |
|    31984 | 1460 | `						}` |
|   567618 | 1461 | `					}else if( nCallArg > 0 && !bTwoSlot && !bNodeFcc ){` |
|   501905 | 1462 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL_INIT,0,` |
|   501870 | 1463 | `							(p3 && ((VmCallArgMap *)p3)->bIsNamespaced) ? 1 : 0,0,0);` |
|   250935 | 1464 | `					}` |
|        - | 1465 | `				}` |
|   600685 | 1466 | `				rc = GenStateEmitCallArgs(&(*pGen),pNode,iFlags,&sArgs);` |
|   600685 | 1467 | `				if( rc != SXRET_OK ){` |
|        9 | 1468 | `					return rc;` |
|        - | 1469 | `				}` |
|   600677 | 1470 | `				iP1 = sArgs.iP1;` |
|   600677 | 1471 | `				iP2 = sArgs.iP2;` |
|   600677 | 1472 | `				p3  = sArgs.p3;` |
|   600677 | 1473 | `				bFcc = sArgs.bFcc;` |
|   600677 | 1474 | `				if( iP1 > 0 \|\| iP2 ){` |
|   852290 | 1475 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_ROT_CALLEE,iP1,` |
|   568190 | 1476 | `						(iP2 ? PH7_ROT_SPREAD : 0) \| (bTwoSlot ? PH7_ROT_TWOSLOT : 0),0,0);` |
|   284095 | 1477 | `				}` |
|   600677 | 1478 | `				if( bNewCallee && nNewClassInstr > 0 ){` |
|    66139 | 1479 | `					if( p3 == 0 ){` |
|     2143 | 1480 | `						VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|     2138 | 1481 | `							&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|     2143 | 1482 | `						if( pMap ){` |
|     2143 | 1483 | `							SyZero(pMap,sizeof(VmCallArgMap));` |
|     2143 | 1484 | `							p3 = (void *)pMap;` |
|     1069 | 1485 | `						}` |
|     1069 | 1486 | `					}` |
|    66139 | 1487 | `					if( p3 ){` |
|    66139 | 1488 | `						((VmCallArgMap *)p3)->nNewClassInstr = nNewClassInstr;` |
|    33067 | 1489 | `					}` |
|    33067 | 1490 | `				}` |
|   300341 | 1491 | `			}` |
|  2370767 | 1492 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|        - | 1493 | `			ph7_expr_node **apNode;` |
|        - | 1494 | `			sxi32 n;` |
|   187713 | 1495 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|        - | 1496 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|        - | 1497 | `				\|EXPR_FLAG_LOAD_IDX_UNSET_BASE` |
|        - | 1498 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE` |
|        - | 1499 | `				\|EXPR_FLAG_MEMBER_COALESCE` |
|        - | 1500 | `				\|EXPR_FLAG_QUIET_VAR\|EXPR_FLAG_RMW_LOAD\|EXPR_FLAG_DEFER_ARG);` |
|        - | 1501 | `			/* Recurse and generate bytecodes for array index */` |
|   187713 | 1502 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   346713 | 1503 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   159005 | 1504 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   159005 | 1505 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   159005 | 1506 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 1507 | `					return rc;` |
|        - | 1508 | `				}` |
|        - | 1509 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   159005 | 1510 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|    79505 | 1511 | `			}` |
|   187713 | 1512 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   159005 | 1513 | `				iP1 = 1; /* Node have an index associated with it */` |
|    79505 | 1514 | `			}else{` |
|        - | 1515 | ``				/* `[]` names the element a WRITE is about to create, so php allows it`` |
|        - | 1516 | `				 * only where a write lands: an assignment target (plain, compound,` |
|        - | 1517 | ``				 * `=&`, a list()/foreach target) and a by-reference argument. Every`` |
|        - | 1518 | `				 * other placement is a COMPILE error there — PHL accepted them all and` |
|        - | 1519 | ``				 * answered NULL after a PH7-worded notice, so `$x = $a[];`,`` |
|        - | 1520 | ``				 * `isset($a[])` and `unset($a[][0])` were silent no-ops on source php`` |
|        - | 1521 | `				 * refuses to run. A call ARGUMENT is the one shape php also leaves to` |
|        - | 1522 | `				 * runtime (it cannot know the parameter's by-ref-ness at compile time),` |
|        - | 1523 | `				 * which is what DEFER_ARG marks. */` |
|    28713 | 1524 | `				if( iFlags & (EXPR_FLAG_LOAD_IDX_UNSET\|EXPR_FLAG_LOAD_IDX_UNSET_BASE) ){` |
|      ! 0 | 1525 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,` |
|      ! 0 | 1526 | `						pNode->pStart ? pNode->pStart->nLine : 0,` |
|        - | 1527 | `						"Cannot use [] for unsetting");` |
|      ! 0 | 1528 | `					return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1529 | `				}` |
|    28713 | 1530 | `				if( (iFlags & (EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_DEFER_ARG)) == 0 ){` |
|      ! 0 | 1531 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,` |
|      ! 0 | 1532 | `						pNode->pStart ? pNode->pStart->nLine : 0,` |
|        - | 1533 | `						"Cannot use [] for reading");` |
|      ! 0 | 1534 | `					return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1535 | `				}` |
|        - | 1536 | `			}` |
|   187713 | 1537 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|        - | 1538 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|     9709 | 1539 | `				iP2 = 4;` |
|   182861 | 1540 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|        - | 1541 | `				/* offsetUnset for ArrayAccess; for an array, remove the ELEMENT. */` |
|      177 | 1542 | `				iP2 = 5;` |
|   177923 | 1543 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET_BASE ){` |
|        - | 1544 | `				/* An unset chain's intermediate container: read it, but with the` |
|        - | 1545 | `				 * unset context's COW-separate and no-vivify rules. */` |
|       20 | 1546 | `				iP2 = 10;` |
|   177828 | 1547 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|        - | 1548 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|        - | 1549 | `				 * short-circuit on missing keys without invoking offsetGet` |
|        - | 1550 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|       45 | 1551 | `				iP2 = 6;` |
|   177799 | 1552 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|        - | 1553 | `				/* Create an empty entry when the desired index is not found */` |
|   109223 | 1554 | `				iP2 = 1;` |
|   123170 | 1555 | `			}else if( iFlags & EXPR_FLAG_DEFER_ARG ){` |
|        - | 1556 | `				/* D1 commit 2: deferred by-ref/by-value element arg. Behaves as a read but,` |
|        - | 1557 | `				 * on a lookup miss, records the lvalue path instead of warning; OP_CALL` |
|        - | 1558 | `				 * re-walks it in vivify (by-ref) or read+warn (by-value) mode. */` |
|     9937 | 1559 | `				iP2 = 9;` |
|     4971 | 1560 | `			}` |
|  1976577 | 1561 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|        - | 1562 | `			/* POP the left node */` |
|        5 | 1563 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        2 | 1564 | `		}` |
|  1335549 | 1565 | `	}` |
|  2671139 | 1566 | `	rc = SXRET_OK;` |
|  2671139 | 1567 | `	nJmpIdx = 0;` |
|        - | 1568 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|        - | 1569 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|        - | 1570 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  2671139 | 1571 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     2319 | 1572 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     2319 | 1573 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     2319 | 1574 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     2319 | 1575 | `			int isSpecial = 0;` |
|     2319 | 1576 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     2179 | 1577 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     2179 | 1578 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     2174 | 1579 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     2083 | 1580 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     1050 | 1581 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      329 | 1582 | `					isSpecial = 1;` |
|      162 | 1583 | `				}` |
|     1122 | 1584 | `			}` |
|     2389 | 1585 | `			pInstr->iP1 = 0;` |
|        - | 1586 | ``			/* A leading `\` (NSSEP) makes the class name ABSOLUTE: it names the`` |
|        - | 1587 | `			 * global class, so it must NOT be re-qualified with the current namespace.` |
|        - | 1588 | ``			 * `\Closure::bind(...)` inside `namespace X` is Closure, not X\Closure.`` |
|        - | 1589 | ``			 * (Multi-component `\A\B::m` already resolves absolutely because its`` |
|        - | 1590 | ``			 * literal keeps a backslash; the single-component `\Closure` lost it.) */`` |
|        - | 1591 | `			{` |
|     3511 | 1592 | `				int bAbsolute = (pNode->pLeft && pNode->pLeft->pStart` |
|     3366 | 1593 | `					&& (pNode->pLeft->pStart->nType & PH7_TK_NSSEP));` |
|     2249 | 1594 | `				if( !isSpecial && !bAbsolute ){` |
|     1903 | 1595 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      949 | 1596 | `				}` |
|        - | 1597 | `			}` |
|        - | 1598 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|        - | 1599 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     2249 | 1600 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|     1925 | 1601 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|     1925 | 1602 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      210 | 1603 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      127 | 1604 | `					return SXRET_OK;` |
|        - | 1605 | `				}` |
|      899 | 1606 | `			}` |
|     1061 | 1607 | `		}` |
|     1150 | 1608 | `	}` |
|        - | 1609 | `	/* Generate code for the right tree */` |
|  2670981 | 1610 | `	if( pNode->pRight ){` |
|  1535861 | 1611 | `		if( iVmOp == PH7_OP_LAND ){` |
|        - | 1612 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    75251 | 1613 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  1498238 | 1614 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|        - | 1615 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    65517 | 1616 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  1427859 | 1617 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|        - | 1618 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|      313 | 1619 | `			iVmOp = 0; /* No binary operator to emit */` |
|      313 | 1620 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  1395013 | 1621 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|        - | 1622 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|        - | 1623 | `			 * the entire containing postfix chain to null. The jump target is` |
|        - | 1624 | `			 * patched later by the innermost non-chain ancestor (or by` |
|        - | 1625 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|        - | 1626 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|        - | 1627 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|      133 | 1628 | `			sxu32 nNsJmp = 0;` |
|      133 | 1629 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|      133 | 1630 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  1394731 | 1631 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */` |
|  1077042 | 1632 | ``			\|\| pNode->pOp->iOp == EXPR_OP_REF /* `=&` reference bind */ ){`` |
|        - | 1633 | `` 			/* The lvalue is the RIGHT operand (prec-18 ops are right-associative; `=&` `` |
|        - | 1634 | `			 * swaps its operands in parse.c so its target is pRight too). Mark it a write` |
|        - | 1635 | `			 * target so a missing base (the container of a subscript-write, or a bare` |
|        - | 1636 | `` 			 * `$o->p`) is auto-created — PHP auto-vivifies on a plain write AND on a `=&` `` |
|        - | 1637 | ``			 * bind (`$a[0] =& $x` creates $a as [0 => &$x], it does not warn). */`` |
|        - | 1638 | `			/* php's compile-time write-target rules first ($this, a temporary base). */` |
|   635455 | 1639 | `			rc = GenStateWriteTargetCheck(&(*pGen),pNode->pRight,0);` |
|   635455 | 1640 | `			if( rc != SXRET_OK ){` |
|       11 | 1641 | `				return rc;` |
|        - | 1642 | `			}` |
|   635447 | 1643 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|   635447 | 1644 | `			if( iVmOp != PH7_OP_STORE && pNode->pOp->iOp != EXPR_OP_REF ){` |
|        - | 1645 | ``				/* COMPOUND assignment (`.=`, `+=`, ...) READS the target first, so`` |
|        - | 1646 | ``				 * php warns when it is undefined and then seeds it; a plain `=` and a`` |
|        - | 1647 | ``				 * `=&` rebind write without reading the target and stay silent. */`` |
|    14563 | 1648 | `				iFlags \|= EXPR_FLAG_RMW_LOAD;` |
|     7279 | 1649 | `			}` |
|   317721 | 1650 | `		}` |
|  1535853 | 1651 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  1535853 | 1652 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_RDONLY_LOAD);` |
|  1535853 | 1653 | `		if( !bIsChainOp ){` |
|        - | 1654 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|        - | 1655 | `			 * operator instruction is emitted. */` |
|  1506343 | 1656 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|   753169 | 1657 | `		}` |
|  1535853 | 1658 | `		if( iVmOp == PH7_OP_STORE ){` |
|   620691 | 1659 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|   620646 | 1660 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|        - | 1661 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|        - | 1662 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|        - | 1663 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|        - | 1664 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|        - | 1665 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|        - | 1666 | `				 */` |
|      163 | 1667 | `				iVmOp = 0;` |
|   620612 | 1668 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|   620533 | 1669 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|        - | 1670 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|     1107 | 1671 | `					iP2 = 1;` |
|      556 | 1672 | `				}else{` |
|   619431 | 1673 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|        - | 1674 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   108803 | 1675 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   108803 | 1676 | `						iP1 = pInstr->iP1;` |
|    54404 | 1677 | `					}else{` |
|   510633 | 1678 | `						p3 = pInstr->p3;` |
|        - | 1679 | `					}` |
|        - | 1680 | `					/* POP the last dynamic load instruction */` |
|   619431 | 1681 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|        - | 1682 | `				}` |
|   310269 | 1683 | `			}` |
|  1225510 | 1684 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|        - | 1685 | `			/* Peek first: a member LHS ($o->p =& $x, C::$s =& $x) keeps its` |
|        - | 1686 | `			 * OP_MEMBER in place (it resolves + stashes the target slot at` |
|        - | 1687 | `			 * runtime), unlike the variable/array shapes which fold their load` |
|        - | 1688 | `			 * away. Mirrors the normal member-store fold above (iP2 kept-MEMBER). */` |
|      202 | 1689 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      202 | 1690 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|        - | 1691 | `				/* Tag the member as a reference target and flag STORE_REF (iP2=1)` |
|        - | 1692 | `				 * to take the member-rebind path in the VM. */` |
|       47 | 1693 | `				pInstr->iP2 = PH7_MEMBER_REF_TARGET;` |
|       47 | 1694 | `				iP2 = 1;` |
|       25 | 1695 | `			}else{` |
|      158 | 1696 | `				pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      158 | 1697 | `				if( pInstr ){` |
|      158 | 1698 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|        - | 1699 | `						/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|        - | 1700 | `						 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|        - | 1701 | `						 */` |
|       54 | 1702 | `						iVmOp = PH7_OP_STORE_IDX_REF;` |
|       54 | 1703 | `						iP1 = pInstr->iP1;` |
|       54 | 1704 | `						iP2 = pInstr->iP2;` |
|       54 | 1705 | `						p3  = pInstr->p3;` |
|       28 | 1706 | `					}else{` |
|      106 | 1707 | `						p3 = pInstr->p3;` |
|        - | 1708 | `					}` |
|       77 | 1709 | `				}` |
|        - | 1710 | `			}` |
|       99 | 1711 | `		}` |
|   767924 | 1712 | `	}` |
|  2670968 | 1713 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|    34576 | 1714 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|        - | 1715 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|        - | 1716 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|       79 | 1717 | `		iVmOp = 0;` |
|       37 | 1718 | `	}` |
|  2670973 | 1719 | `	if( iVmOp > 0 ){` |
|  2670429 | 1720 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|    42729 | 1721 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|        - | 1722 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|     4729 | 1723 | `				iP1 = 1;` |
|     2367 | 1724 | `			}` |
|  2649067 | 1725 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|        - | 1726 | `			/* Namespace-qualify the class name for NEW */ {` |
|    67569 | 1727 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|    67569 | 1728 | `				VmInstr *pCallInstr = 0;` |
|    67569 | 1729 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|    66139 | 1730 | `					VmCallArgMap *pNewMap = (VmCallArgMap *)pPeek->p3;` |
|    66139 | 1731 | `					pCallInstr = pPeek;` |
|        - | 1732 | `` 					/* The class-name push sits one instruction back only when this `new` `` |
|        - | 1733 | `					 * takes no arguments; with an argument list the reorder puts the whole` |
|        - | 1734 | `					 * list (and its screen and rotation) in between, so the call node` |
|        - | 1735 | `					 * recorded where the push is. */` |
|    99206 | 1736 | `					pPeek = (pNewMap && pNewMap->nNewClassInstr > 0)` |
|    66134 | 1737 | `						? PH7_VmGetInstr(pGen->pVm,pNewMap->nNewClassInstr - 1)` |
|    33067 | 1738 | `						: PH7_VmPeekNextInstr(pGen->pVm);` |
|    33067 | 1739 | `				}` |
|    67569 | 1740 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|    67533 | 1741 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|        - | 1742 | `					sxu32 nLitForClass;` |
|    67533 | 1743 | `					VmCallArgMap *pCallNsMap = pCallInstr ? (VmCallArgMap *)pCallInstr->p3 : 0;` |
|        - | 1744 | `					/* If the CALL handler qualified the name with FUNCTION` |
|        - | 1745 | `					 * imports, recover the original literal (recorded in the` |
|        - | 1746 | `					 * arg map — OP_CALL's iP2 is the hasSpread flag, and` |
|        - | 1747 | `` 					 * misreading it as a literal index made `new C(...$args)` `` |
|        - | 1748 | `					 * fatal with "Class ' ' is not defined") and re-qualify` |
|        - | 1749 | `					 * with class imports. */` |
|    67533 | 1750 | `					if( pCallNsMap && pCallNsMap->nOrigNameLit > 0 ){` |
|       64 | 1751 | `						nLitForClass = pCallNsMap->nOrigNameLit - 1;` |
|       34 | 1752 | `					}else{` |
|    67473 | 1753 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|        - | 1754 | `					}` |
|    67533 | 1755 | `					pPeek->iP1 = 0;` |
|    67533 | 1756 | `					if( !bAbsolute ){` |
|        - | 1757 | `						/* self/static/parent are resolved at runtime against the` |
|        - | 1758 | `						 * current class — never namespace-qualify them (else` |
|        - | 1759 | ``						 * `new self` in namespace N becomes "N\self"). Mirrors the`` |
|        - | 1760 | `						 * instanceof (IS_A) guard below. */` |
|    67477 | 1761 | `						ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nLitForClass);` |
|    67477 | 1762 | `						int isSpecialNew = 0;` |
|    67477 | 1763 | `						if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|    67477 | 1764 | `							const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|    67477 | 1765 | `							sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|    67472 | 1766 | `							if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    67581 | 1767 | `								(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    33849 | 1768 | `								(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|       46 | 1769 | `								isSpecialNew = 1;` |
|       22 | 1770 | `							}` |
|    33736 | 1771 | `						}` |
|    67477 | 1772 | `						if( isSpecialNew ){` |
|       46 | 1773 | `							pPeek->iP2 = (sxi32)nLitForClass;` |
|       24 | 1774 | `						}else{` |
|    67433 | 1775 | `							pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|        - | 1776 | `						}` |
|    33741 | 1777 | `					}else{` |
|       61 | 1778 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|        - | 1779 | `					}` |
|    33764 | 1780 | `				}` |
|        - | 1781 | `			}` |
|    67569 | 1782 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    67569 | 1783 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|        - | 1784 | `				VmInstr *pPrev;` |
|        - | 1785 | `				int bPrevMember;` |
|    66139 | 1786 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|        - | 1787 | `				/* "Was the callee a MEMBER access?" — which, once the reorder puts a` |
|        - | 1788 | `				 * rotation between the callee and its call, is the question the rotation` |
|        - | 1789 | `				 * already answers (a method callee is the two-slot one). */` |
|    99206 | 1790 | `				bPrevMember = pPrev && (pPrev->iOp == PH7_OP_ROT_CALLEE` |
|    63958 | 1791 | `					? (pPrev->iP2 & PH7_ROT_TWOSLOT) != 0` |
|     2176 | 1792 | `					: pPrev->iOp == PH7_OP_MEMBER);` |
|    66139 | 1793 | `				if( !bPrevMember ){` |
|        - | 1794 | `					/* Pop the call instruction, preserve named-arg map and` |
|        - | 1795 | `					 * the hasSpread flag (OP_NEW consumes the spread` |
|        - | 1796 | `					 * accumulator exactly like OP_CALL would have). */` |
|    66139 | 1797 | `					iP1 = pInstr->iP1;` |
|    66139 | 1798 | `					iP2 = pInstr->iP2;` |
|    66139 | 1799 | `					if( pInstr->p3 ){` |
|    66139 | 1800 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|    33067 | 1801 | `					}` |
|    66139 | 1802 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    33067 | 1803 | `				}` |
|    33072 | 1804 | `			}` |
|  2593923 | 1805 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|        - | 1806 | `			/* instanceof: right operand is a class name, not a constant.` |
|        - | 1807 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     9745 | 1808 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     9745 | 1809 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     9745 | 1810 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     9745 | 1811 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     9745 | 1812 | `				int isSpecialIs = 0;` |
|     9745 | 1813 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     9745 | 1814 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     9745 | 1815 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     9740 | 1816 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     9743 | 1817 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     4870 | 1818 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|       12 | 1819 | `						isSpecialIs = 1;` |
|        5 | 1820 | `					}` |
|     4870 | 1821 | `				}` |
|     9745 | 1822 | `				pInstr->iP1 = 0;` |
|     9745 | 1823 | `				if( !isSpecialIs && !bAbsolute ){` |
|     9709 | 1824 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     4852 | 1825 | `				}` |
|     4875 | 1826 | `			}` |
|  2555271 | 1827 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|        - | 1828 | `			/* Prevent constant expansion for member/property names.` |
|        - | 1829 | `			 * The right child (member name) was just compiled — its LOADC` |
|        - | 1830 | `			 * should not trigger constant lookup. */` |
|    29515 | 1831 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    29515 | 1832 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    29231 | 1833 | `				pInstr->iP1 = 0;` |
|    14613 | 1834 | `			}` |
|    29515 | 1835 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|        - | 1836 | `				/* Static member access,remember that */` |
|     2161 | 1837 | `				iP1 = 1;` |
|     2161 | 1838 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     2161 | 1839 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      255 | 1840 | `					p3 = pInstr->p3;` |
|        - | 1841 | ``					/* A `$`-form (`C::$s`, `C::$$x`, `C::${$e}`) is a STATIC PROPERTY`` |
|        - | 1842 | `					 * access, never a constant. A LITERAL name folds into p3 (non-zero)` |
|        - | 1843 | ``					 * and the exec side reads it there; a DYNAMIC name (`$$x`/`${$e}`)`` |
|        - | 1844 | `					 * leaves p3==0 with the computed name on the stack — the SAME shape` |
|        - | 1845 | ``					 * as a bareword constant `C::C`. Mark iP1=2 so exec still routes it`` |
|        - | 1846 | `					 * to the property table (hAttr), not the constant table (hConst). */` |
|      255 | 1847 | `					if( p3 == 0 ){` |
|        8 | 1848 | `						iP1 = 2;` |
|        3 | 1849 | `					}` |
|      255 | 1850 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|      125 | 1851 | `				}` |
|     1078 | 1852 | `			}` |
|        - | 1853 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|        - | 1854 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|        - | 1855 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|        - | 1856 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|    29515 | 1857 | `			if( iP2 == PH7_MEMBER_READ ){` |
|    29515 | 1858 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|       58 | 1859 | `					iP2 = PH7_MEMBER_UNSET;` |
|    29488 | 1860 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|      159 | 1861 | `					iP2 = PH7_MEMBER_ISSET;` |
|    29384 | 1862 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|       31 | 1863 | `					iP2 = PH7_MEMBER_EMPTY;` |
|    29293 | 1864 | `				}else if( iFlags & EXPR_FLAG_MEMBER_COALESCE ){` |
|       68 | 1865 | `					iP2 = PH7_MEMBER_COALESCE;` |
|    29246 | 1866 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|        - | 1867 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|     1521 | 1868 | `					iP2 = PH7_MEMBER_WRITE;` |
|    28455 | 1869 | `				}else if( iFlags & EXPR_FLAG_DEFER_ARG ){` |
|        - | 1870 | `					/* D1 commit 2: deferred by-ref/by-value property arg ($o->p). */` |
|      689 | 1871 | `					iP2 = PH7_MEMBER_DEFPATH;` |
|      342 | 1872 | `				}` |
|    14755 | 1873 | `			}` |
|    14755 | 1874 | `		}` |
|        - | 1875 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|        - | 1876 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|        - | 1877 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|        - | 1878 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|        - | 1879 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
|  2670429 | 1880 | `		if( bFcc ){` |
|      247 | 1881 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|        - | 1882 | `			/* php's global fallback applies to a first-class callable exactly as it does` |
|        - | 1883 | ``			 * to the call it stands for: inside a namespace, `strlen(...)` is the global`` |
|        - | 1884 | `			 * function when the current namespace has none. The callee's literal was` |
|        - | 1885 | `			 * namespace-qualified above and the arg map that records it is dropped here` |
|        - | 1886 | `			 * (an FCC has no arguments), so carry the one bit the resolution needs in the` |
|        - | 1887 | ``			 * instruction itself — without it `strlen(...)` in a namespaced file was`` |
|        - | 1888 | ``			 * `Call to undefined function A\B\strlen()`, a fatal on ordinary php. */`` |
|      247 | 1889 | `			iP2 = (p3 && ((VmCallArgMap *)p3)->bIsNamespaced) ? 1 : 0;` |
|      247 | 1890 | `			p3 = 0;` |
|      247 | 1891 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      247 | 1892 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER && pInstr->iP2 == PH7_MEMBER_METHOD ){` |
|        - | 1893 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|        - | 1894 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|        - | 1895 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|        - | 1896 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|      143 | 1897 | `				void *pMemberName = pInstr->p3;` |
|      143 | 1898 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|      143 | 1899 | `				if( pMemberName ){` |
|      ! 0 | 1900 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|      ! 0 | 1901 | `				}` |
|      143 | 1902 | `				iP1 = 2;` |
|       74 | 1903 | `			}else{` |
|        - | 1904 | `				/* Only a METHOD member is the callee's NAME. A parenthesised PROPERTY read` |
|        - | 1905 | ``				 * (`($o->cb)(...)`, `(C::$cb)(...)`) is php's variable-invocation: the member`` |
|        - | 1906 | `				 * op stays, its VALUE is the callable, and this is the iP1=1 wrap. Dropping it` |
|        - | 1907 | `				 * here read the property NAME as a method name and answered` |
|        - | 1908 | ``				 * `Call to undefined method H::cb()` for a closure the object was holding —`` |
|        - | 1909 | `				 * the CALL codegen above already made the distinction (it leaves the member a` |
|        - | 1910 | `				 * plain read for a parenthesised callee) and this branch undid it. */` |
|      108 | 1911 | `				iP1 = 1;` |
|        - | 1912 | `			}` |
|      121 | 1913 | `		}` |
|        - | 1914 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|        - | 1915 | `		 * This is the primary emit path for user-visible calls. */` |
|  2670429 | 1916 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|   667999 | 1917 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|   333997 | 1918 | `		}` |
|        - | 1919 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  2670429 | 1920 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  1335212 | 1921 | `	}` |
|  2670973 | 1922 | `	if( nJmpIdx > 0 ){` |
|        - | 1923 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   141071 | 1924 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   141071 | 1925 | `		if( pInstr ){` |
|   141071 | 1926 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    70533 | 1927 | `		}` |
|    70533 | 1928 | `	}` |
|  2670973 | 1929 | `	return rc;` |
|  3550104 | 1930 | `}` |
|        - | 1931 | `/*` |
|        - | 1932 | ` * Emit a call's ARGUMENT LIST, and decide everything about it the OP_CALL then carries:` |
|        - | 1933 | ` * the count, the unpack flag, the named-argument / assert-source / argument-shape map.` |
|        - | 1934 | ` *` |
|        - | 1935 | ` * Split out of GenStateEmitExprCode because php resolves a callee BEFORE it evaluates` |
|        - | 1936 | ` * the arguments, so this now runs AFTER the callee sub-tree has been emitted (the one` |
|        - | 1937 | `` * exception is a `new`'s constructor list — see EXPR_FLAG_NEW_CALLEE). It is otherwise`` |
|        - | 1938 | ` * the same code, and reads only the node: nothing here inspects the instructions the` |
|        - | 1939 | ` * callee left behind.` |
|        - | 1940 | ` *` |
|        - | 1941 | ` * pArgs->p3 may arrive non-NULL — the callee's own namespace qualification builds the` |
|        - | 1942 | ` * VmCallArgMap first now — and every allocation site below reuses it.` |
|        - | 1943 | ` */` |
|   600680 | 1944 | `static sxi32 GenStateEmitCallArgs(` |
|        - | 1945 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - | 1946 | `	ph7_expr_node *pNode, /* The call node */` |
|        - | 1947 | `	sxi32 iFlags,         /* Control flags of the call site */` |
|        - | 1948 | `	GenCallArgs *pArgs    /* OUT: what the OP_CALL needs */` |
|        - | 1949 | `	)` |
|        5 | 1950 | `{` |
|   600685 | 1951 | `	void *p3 = pArgs->p3;` |
|   600685 | 1952 | `	sxi32 iP1 = 0;` |
|   600685 | 1953 | `	sxu32 iP2 = 0;` |
|   600685 | 1954 | `	int bFcc = 0;` |
|        - | 1955 | `	sxi32 rc;` |
|        - | 1956 | `	ph7_expr_node **apNode;` |
|   600685 | 1957 | `	int hasSpread = 0;` |
|   600685 | 1958 | `	int hasNamed = 0;` |
|   600685 | 1959 | `	sxu32 byRefMask = 0;` |
|        - | 1960 | `	sxi32 nArgs;` |
|        - | 1961 | `	sxi32 n;` |
|   600685 | 1962 | `	int bAnySpread = 0;` |
|        - | 1963 | `	/* Recurse and generate bytecodes for function arguments */` |
|   600685 | 1964 | `	apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   600685 | 1965 | `	nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|        - | 1966 | ``	/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|        - | 1967 | `	 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|        - | 1968 | `	 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|   600685 | 1969 | `	if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|      247 | 1970 | `		bFcc = 1;` |
|      247 | 1971 | `		nArgs = 0;` |
|      121 | 1972 | `	}` |
|        - | 1973 | `	/* Validate argument order like php: no positional argument after a` |
|        - | 1974 | ``	 * named one OR after unpacking, and `name: ...$x` is a parse error. */`` |
|        - | 1975 | `	{` |
|   600685 | 1976 | `		int seenNamed = 0;` |
|   600685 | 1977 | `		int seenSpread = 0;` |
|  1428483 | 1978 | `		for( n = 0; n < nArgs; ++n ){` |
|   827805 | 1979 | `			if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|      265 | 1980 | `				bAnySpread = 1;` |
|      265 | 1981 | `				seenSpread = 1;` |
|      265 | 1982 | `				if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      ! 0 | 1983 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|        - | 1984 | `						"syntax error, unexpected token \"...\"");` |
|      ! 0 | 1985 | `					return SXERR_SYNTAX;` |
|        3 | 1986 | `				}` |
|   827674 | 1987 | `			}else if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      589 | 1988 | `				seenNamed = 1;` |
|      589 | 1989 | `				hasNamed = 1;` |
|   827251 | 1990 | `			}else if( seenNamed ){` |
|        3 | 1991 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|        - | 1992 | `					"Cannot use positional argument after named argument");` |
|        3 | 1993 | `				return SXERR_SYNTAX;` |
|   826957 | 1994 | `			}else if( seenSpread ){` |
|      ! 0 | 1995 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|        - | 1996 | `					"Cannot use positional argument after argument unpacking");` |
|      ! 0 | 1997 | `				return SXERR_SYNTAX;` |
|        - | 1998 | `			}` |
|   413904 | 1999 | `		}` |
|        - | 2000 | `	}` |
|        - | 2001 | `	/* Read-only load */` |
|   600683 | 2002 | `	iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|        - | 2003 | `	/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|        - | 2004 | ``	 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|        - | 2005 | `	 * objects dispatch to the right method (offsetExists for both;` |
|        - | 2006 | `	 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|   600683 | 2007 | `	if( pNode->pLeft && pNode->pLeft->pStart ){` |
|   600683 | 2008 | `		SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|   921693 | 2009 | `		int bIsset = pCallName->nByte == 5` |
|   600678 | 2010 | `			&& SyStrnicmp(pCallName->zString,"isset",5) == 0;` |
|   921693 | 2011 | `		int bEmpty = pCallName->nByte == 5` |
|   600678 | 2012 | `			&& SyStrnicmp(pCallName->zString,"empty",5) == 0;` |
|        - | 2013 | `		/* isset()/empty() are language CONSTRUCTS, not functions: php parses` |
|        - | 2014 | `		 * their argument list in the grammar and a missing operand is a parse` |
|        - | 2015 | `		 * error on the ')'. They compile through this ordinary call loop, which` |
|        - | 2016 | ``		 * never checked arity, so `empty()` quietly evaluated to true and`` |
|        - | 2017 | ``		 * `isset()` to false. (empty() also takes exactly one operand in php,`` |
|        - | 2018 | `		 * unlike isset(), which is variadic.) */` |
|   600683 | 2019 | `		if( (bIsset \|\| bEmpty) && nArgs < 1 ){` |
|        - | 2020 | `			/* php names the ')' itself as the unexpected token, so point at the` |
|        - | 2021 | `			 * node's last token rather than pGen->pIn (which has already moved` |
|        - | 2022 | `			 * past the call to the statement's ';'). */` |
|        5 | 2023 | `			SyToken *pTok = pNode->pEnd;` |
|        5 | 2024 | `			if( pTok && pTok > pNode->pStart && (pTok->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 | 2025 | `				pTok--;` |
|      ! 0 | 2026 | `			}` |
|        5 | 2027 | `			PH7_GenSyntaxError(&(*pGen),pTok,0);` |
|        5 | 2028 | `			return SXERR_ABORT;` |
|        - | 2029 | `		}` |
|   600679 | 2030 | `		if( bIsset ){` |
|     9909 | 2031 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|   595727 | 2032 | `		}else if( bEmpty ){` |
|      155 | 2033 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|       75 | 2034 | `		}` |
|        - | 2035 | `		/* Auto-vivify by-reference out-params of known builtins so an` |
|        - | 2036 | `		 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|        - | 2037 | `		 * $m never assigned) gets a real memobj slot for the builtin to` |
|        - | 2038 | `		 * write back through. Skipped when spread/named args are present:` |
|        - | 2039 | `		 * the compile-time positional index no longer maps to the` |
|        - | 2040 | `		 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|   600679 | 2041 | `		if( !bAnySpread && !hasNamed ){` |
|        - | 2042 | `			SyString sBuiltin;` |
|   600091 | 2043 | `			GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|   600091 | 2044 | `			byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|   300043 | 2045 | `		}` |
|   300337 | 2046 | `	}` |
|  1428473 | 2047 | `	for( n = 0 ; n < nArgs ; ++n ){` |
|   827801 | 2048 | `		sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   827801 | 2049 | `		sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|        - | 2050 | `		/* For a by-ref argument position, drop the read-only flag so the` |
|        - | 2051 | `		 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|        - | 2052 | `		 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|        - | 2053 | `		 * auto-vivifies its element and exposes a writable memobj slot for the` |
|        - | 2054 | `		 * builtin to write back through. A plain $var target is unaffected` |
|        - | 2055 | `		 * (iP1=0 either way). */` |
|   827801 | 2056 | `		if( n < 31 && (byRefMask & (1u<<n)) ){` |
|    14383 | 2057 | `			iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|    14383 | 2058 | `			iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|     7189 | 2059 | `		}` |
|        - | 2060 | ``		/* D1: a plain `$var` argument may bind to a by-ref parameter whose signature`` |
|        - | 2061 | `		 * is unknown at compile time (forward reference, dynamic call, or method` |
|        - | 2062 | ``		 * dispatch — e.g. PHPUnit's `willReturnReference($undef)`). We used to clear`` |
|        - | 2063 | `		 * the read-only flag here so an undefined variable vivified a real slot the` |
|        - | 2064 | `		 * by-ref write-back could reach — but that also invented the variable as NULL` |
|        - | 2065 | `		 * in the caller when the parameter turned out by-VALUE, and suppressed php's` |
|        - | 2066 | ``		 * `Undefined variable $x` warning. Instead mark it DEFERRED: OP_LOAD leaves an`` |
|        - | 2067 | `		 * undefined variable uncreated and carries a lazy-lvalue marker, and OP_CALL` |
|        - | 2068 | `		 * materializes it ONLY for a by-ref parameter once the callee is resolved` |
|        - | 2069 | `		 * (VmResolveDeferredArgs). Excludes isset()/empty()/unset(), which compile` |
|        - | 2070 | `		 * through this same call loop but must NEVER create their operand, and` |
|        - | 2071 | `		 * spread args (whose elements have no positional index of their own). A NAMED` |
|        - | 2072 | `		 * arg defers too: it binds to the formal its NAME picks, which the resolver` |
|        - | 2073 | `		 * looks up through the call's own argument map — excluding it left` |
|        - | 2074 | ``		 * `r(x: $a['new'])` warning `Undefined array key` and passing NULL where php`` |
|        - | 2075 | ``		 * creates the element for the by-ref parameter `$x`.`` |
|        - | 2076 | `		 *` |
|        - | 2077 | `		 * D1 commit 2: the same reasoning extends to an array-element ($a["k"]) or` |
|        - | 2078 | `		 * property ($o->p) argument — a by-ref user-function parameter must vivify the` |
|        - | 2079 | `		 * element/property, a by-value one must warn and NOT vivify. Those nodes carry a` |
|        - | 2080 | `		 * subscript/arrow operator (pOp != 0). The DEFER flag rides down to the base LOAD` |
|        - | 2081 | `		 * (undefined base auto-defers via commit 1) and to the LOAD_IDX/MEMBER, which` |
|        - | 2082 | ``		 * record the lvalue path on a lookup miss. Static `::` and nullsafe `?->` stay`` |
|        - | 2083 | `		 * eager. */` |
|   827796 | 2084 | `		if( (iFlags & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_LOAD_IDX_UNSET` |
|   413898 | 2085 | `		               \|EXPR_FLAG_MEMBER_COALESCE)) == 0` |
|   822756 | 2086 | `		 && (iArgFlags & EXPR_FLAG_RDONLY_LOAD) /* not a known builtin by-ref slot (kept eager above) */` |
|   810527 | 2087 | `		 && (apNode[n]->iFlags & EXPR_NODE_SPREAD) == 0` |
|   862245 | 2088 | `		 && ( (apNode[n]->pOp == 0 && apNode[n]->xCode == PH7_CompileVariable)` |
|   580127 | 2089 | `		   \|\| (apNode[n]->pOp != 0 && (apNode[n]->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|   123014 | 2090 | `		                            \|\| apNode[n]->pOp->iOp == EXPR_OP_ARROW)) ) ){` |
|   456419 | 2091 | `			iArgFlags \|= EXPR_FLAG_DEFER_ARG;` |
|   228207 | 2092 | `		}` |
|   827801 | 2093 | `		rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|   827801 | 2094 | `		if( rc != SXRET_OK ){` |
|        3 | 2095 | `			return rc;` |
|        - | 2096 | `		}` |
|        - | 2097 | `		/* Each argument is an independent nullsafe scope. */` |
|   827799 | 2098 | `		GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|   827799 | 2099 | `		if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|        - | 2100 | `			/* Emit spread opcode to unpack this array argument. iP1 marks a` |
|        - | 2101 | ``			 * source php will unpack BY REFERENCE: only a plain `$var` (php`` |
|        - | 2102 | ``			 * fetches every other shape — `$a[0]`, `$o->p`, `C::$s`, a cast, a`` |
|        - | 2103 | `			 * call — as an R-value, so a by-ref parameter binds its elements in` |
|        - | 2104 | `			 * a temporary and the write-back is invisible). The expander needs` |
|        - | 2105 | `			 * the distinction because it carries each element's slot for the` |
|        - | 2106 | ``			 * by-ref binder; without it `r(...$a[0])` wrote through to the real`` |
|        - | 2107 | `			 * element, which php leaves alone. */` |
|      366 | 2108 | `			PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD,` |
|      262 | 2109 | `				(apNode[n]->pOp == 0 && apNode[n]->xCode == PH7_CompileVariable) ? 1 : 0,` |
|        - | 2110 | `				0, 0, 0);` |
|      265 | 2111 | `			hasSpread = 1;` |
|      131 | 2112 | `		}` |
|   413902 | 2113 | `	}` |
|        - | 2114 | `	/* Total number of given arguments */` |
|   600677 | 2115 | `	iP1 = nArgs;` |
|   600677 | 2116 | `	iP2 = hasSpread;` |
|        - | 2117 | `	/* Build VmCallArgMap if named arguments are present.` |
|        - | 2118 | `	 * Deep-copy name strings so they survive token stream cleanup. */` |
|   600677 | 2119 | `	if( hasNamed ){` |
|      379 | 2120 | `		sxu32 nStrBytes = 0;` |
|        - | 2121 | `		char *zBuf;` |
|     1107 | 2122 | `		for( n = 0; n < nArgs; ++n ){` |
|      733 | 2123 | `			if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      587 | 2124 | `				nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|      291 | 2125 | `			}` |
|      369 | 2126 | `		}` |
|        - | 2127 | `		{` |
|      379 | 2128 | `		sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|      379 | 2129 | `		VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      374 | 2130 | `			&pGen->pVm->sAllocator, mapSize);` |
|      379 | 2131 | `		if( pMap ){` |
|      379 | 2132 | `			SyZero(pMap, mapSize);` |
|      379 | 2133 | `			pMap->bHasNamed = 1;` |
|      379 | 2134 | `			pMap->nTotal = (sxu32)nArgs;` |
|      379 | 2135 | `			pMap->aNames = (SyString *)&pMap[1];` |
|      379 | 2136 | `			zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|     1107 | 2137 | `			for( n = 0; n < nArgs; ++n ){` |
|      733 | 2138 | `				if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      587 | 2139 | `					sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|      587 | 2140 | `					SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|      587 | 2141 | `					SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|      587 | 2142 | `					zBuf += nb;` |
|      291 | 2143 | `				}` |
|        - | 2144 | `				/* else: aNames[n] remains {NULL, 0} for positional */` |
|      369 | 2145 | `			}` |
|      379 | 2146 | `			p3 = (void *)pMap;` |
|      187 | 2147 | `		}` |
|        - | 2148 | `		}` |
|      187 | 2149 | `	}` |
|        - | 2150 | `	/* assert(): php's compiler keeps a copy of the assertion's AST and a` |
|        - | 2151 | ``	 * failing assert reports its rendered SOURCE (`assert(1 == 2)`), not the`` |
|        - | 2152 | `	 * evaluated value. Render the first argument's token span` |
|        - | 2153 | `	 * into the call map so vm_builtin_assert can echo it. Only a DIRECT` |
|        - | 2154 | `	 * unqualified/absolute call qualifies — matching php, an indirect call` |
|        - | 2155 | `	 * (call_user_func, a callable variable) has no source text and its` |
|        - | 2156 | `	 * AssertionError carries an empty message. A spread first argument is` |
|        - | 2157 | `	 * skipped (its span is the unpacked array, not the assertion). */` |
|   600672 | 2158 | `	if( nArgs >= 1 && !bFcc` |
|   568195 | 2159 | `	 && (apNode[0]->iFlags & EXPR_NODE_SPREAD) == 0 ){` |
|        - | 2160 | `		SyString sCallee;` |
|   567981 | 2161 | `		GenStateCallBuiltinName(pNode->pLeft,&sCallee);` |
|   567976 | 2162 | `		if( sCallee.nByte == sizeof("assert")-1` |
|   353519 | 2163 | `		 && SyStrnicmp(sCallee.zString,"assert",sizeof("assert")-1) == 0 ){` |
|        - | 2164 | `			/* An operator root's pStart/pEnd name only the operator token` |
|        - | 2165 | ``			 * (`1 == 2` roots at `==`); the subtree walk recovers the whole`` |
|        - | 2166 | `			 * raw extent, re-adding parens the grouping pass consumed. */` |
|       67 | 2167 | `			SyToken *pSpanIn = 0;` |
|       67 | 2168 | `			SyToken *pSpanEnd = 0;` |
|        - | 2169 | `			SyBlob sSrc;` |
|       67 | 2170 | `			PH7_ExprSubtreeSpan(apNode[0],&pSpanIn,&pSpanEnd);` |
|       67 | 2171 | `			SyBlobInit(&sSrc,&pGen->pVm->sAllocator);` |
|       67 | 2172 | `			if( pSpanIn && pSpanEnd && pSpanIn < pSpanEnd ){` |
|       67 | 2173 | `				if( apNode[0]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|        - | 2174 | ``					/* php renders the name too: `assert(assertion: 1 == 2)`. */`` |
|        3 | 2175 | `					SyBlobAppend(&sSrc,apNode[0]->sArgName.zString,apNode[0]->sArgName.nByte);` |
|        3 | 2176 | `					SyBlobAppend(&sSrc,": ",2);` |
|        1 | 2177 | `				}` |
|       67 | 2178 | `				PH7_GenRenderAssertSpan(pGen,pSpanIn,pSpanEnd,&sSrc);` |
|       31 | 2179 | `			}` |
|       67 | 2180 | `			if( SyBlobLength(&sSrc) > 0 ){` |
|       98 | 2181 | `				char *zDup = (char *)SyMemBackendDup(&pGen->pVm->sAllocator,` |
|       62 | 2182 | `					SyBlobData(&sSrc),SyBlobLength(&sSrc));` |
|       67 | 2183 | `				if( zDup ){` |
|       67 | 2184 | `					if( p3 == 0 ){` |
|       65 | 2185 | `						VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|       60 | 2186 | `							&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|       65 | 2187 | `						if( pMap ){` |
|       65 | 2188 | `							SyZero(pMap,sizeof(VmCallArgMap));` |
|       65 | 2189 | `							p3 = (void *)pMap;` |
|       30 | 2190 | `						}` |
|       30 | 2191 | `					}` |
|       67 | 2192 | `					if( p3 ){` |
|       67 | 2193 | `						SyStringInitFromBuf(&((VmCallArgMap *)p3)->sAssertSrc,` |
|        - | 2194 | `							zDup,SyBlobLength(&sSrc));` |
|       31 | 2195 | `					}` |
|       31 | 2196 | `				}` |
|       31 | 2197 | `			}` |
|       67 | 2198 | `			SyBlobRelease(&sSrc);` |
|       31 | 2199 | `		}` |
|   283988 | 2200 | `	}` |
|        - | 2201 | `	/* Record each argument's compile-time SHAPE so the by-ref binders can` |
|        - | 2202 | `	 * refuse a non-variable where php refuses it — at the CALL, before the` |
|        - | 2203 | `	 * callee's ZPP runs. Skipped when the call SPREADS (one compile-time` |
|        - | 2204 | `	 * argument becomes N runtime slots, so the positions no longer line up)` |
|        - | 2205 | `	 * or when it carries more arguments than the masks can hold; a call` |
|        - | 2206 | `	 * without the flag keeps the old runtime nIdx test. Named arguments are` |
|        - | 2207 | `	 * fine: they change which FORMAL a slot binds to, not the slot's index. */` |
|   600677 | 2208 | `	if( !bAnySpread && nArgs > 0 && nArgs <= 31 && !bFcc ){` |
|   567947 | 2209 | `		sxu32 nNonLval = 0;` |
|   567947 | 2210 | `		sxu32 nTempCall = 0;` |
|  1395401 | 2211 | `		for( n = 0 ; n < nArgs ; ++n ){` |
|   827459 | 2212 | `			int iShape = GenStateArgShape(apNode[n]);` |
|   827459 | 2213 | `			if( iShape == GEN_ARG_NONE ){` |
|   306483 | 2214 | `				nNonLval \|= (1u << n);` |
|   674220 | 2215 | `			}else if( iShape == GEN_ARG_TEMPCALL ){` |
|    40099 | 2216 | `				nTempCall \|= (1u << n);` |
|    20047 | 2217 | `			}` |
|   413732 | 2218 | `		}` |
|   567947 | 2219 | `		if( p3 == 0 ){` |
|   567477 | 2220 | `			VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|   567472 | 2221 | `				&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|   567477 | 2222 | `			if( pMap ){` |
|   567477 | 2223 | `				SyZero(pMap,sizeof(VmCallArgMap));` |
|   567477 | 2224 | `				p3 = (void *)pMap;` |
|   283736 | 2225 | `			}` |
|   283736 | 2226 | `		}` |
|   567947 | 2227 | `		if( p3 ){` |
|   567947 | 2228 | `			((VmCallArgMap *)p3)->bArgShapes = 1;` |
|   567947 | 2229 | `			((VmCallArgMap *)p3)->nNonLvalMask = nNonLval;` |
|   567947 | 2230 | `			((VmCallArgMap *)p3)->nTempCallMask = nTempCall;` |
|   283971 | 2231 | `		}` |
|   283971 | 2232 | `	}` |
|   600677 | 2233 | `	pArgs->iP1 = iP1;` |
|   600677 | 2234 | `	pArgs->iP2 = iP2;` |
|   600677 | 2235 | `	pArgs->p3  = p3;` |
|   600677 | 2236 | `	pArgs->bFcc = bFcc;` |
|   600677 | 2237 | `	pArgs->bAnySpread = bAnySpread;` |
|   600677 | 2238 | `	return SXRET_OK;` |
|   300345 | 2239 | `}` |
|        - | 2240 | `/*` |
|        - | 2241 | ` * Compile a PHP expression.` |
|        - | 2242 | ` * According to the PHP language reference manual:` |
|        - | 2243 | ` *  Expressions are the most important building stones of PHP.` |
|        - | 2244 | ` *  In PHP, almost anything you write is an expression.` |
|        - | 2245 | ` *  The simplest yet most accurate way to define an expression` |
|        - | 2246 | ` *  is "anything that has a value".` |
|        - | 2247 | ` * If something goes wrong while compiling the expression,this` |
|        - | 2248 | ` * function takes care of generating the appropriate error` |
|        - | 2249 | ` * message.` |
|        - | 2250 | ` */` |
|        - | 2251 | `/*` |
|        - | 2252 | ` * Does this expression tree contain a comma OPERATOR node?` |
|        - | 2253 | ` *` |
|        - | 2254 | `` * PH7 shipped `,` as a lowest-precedence binary operator (IMP-0139-COMMA), so`` |
|        - | 2255 | `` * `(1, 2)` and `$x = (f(), $y)` compile and evaluate to the right operand.`` |
|        - | 2256 | ` * php 8 has no comma operator: its grammar only allows comma-separated` |
|        - | 2257 | ` * expression LISTS inside for(...) clauses (call arguments, array literals and` |
|        - | 2258 | ` * list() are split by the parser, never by this node). Accepting it changes the` |
|        - | 2259 | ` * meaning of source php rejects, which §10 classes as a bug — so every context` |
|        - | 2260 | ` * except for() now reports php's parse error.` |
|        - | 2261 | ` */` |
| 23169426 | 2262 | `static int GenStateTreeHasComma(ph7_expr_node *pNode)` |
|        5 | 2263 | `{` |
|        - | 2264 | `	ph7_expr_node **apArg;` |
|        - | 2265 | `	sxu32 n;` |
| 23169431 | 2266 | `	if( pNode == 0 ){` |
| 16346259 | 2267 | `		return 0;` |
|        - | 2268 | `	}` |
|  6823177 | 2269 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|        6 | 2270 | `		return 1;` |
|        - | 2271 | `	}` |
|  6823168 | 2272 | `	if( GenStateTreeHasComma(pNode->pLeft) \|\| GenStateTreeHasComma(pNode->pRight)` |
|  6823169 | 2273 | `	 \|\| GenStateTreeHasComma(pNode->pCond) ){` |
|        6 | 2274 | `		return 1;` |
|        - | 2275 | `	}` |
|  6823169 | 2276 | `	apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  7796197 | 2277 | `	for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; n++ ){` |
|   973033 | 2278 | `		if( GenStateTreeHasComma(apArg[n]) ){` |
|      ! 0 | 2279 | `			return 1;` |
|        - | 2280 | `		}` |
|   486519 | 2281 | `	}` |
|  6823169 | 2282 | `	return 0;` |
| 11584718 | 2283 | `}` |
|  1811872 | 2284 | `PH7_PRIVATE sxi32 PH7_CompileExpr(` |
|        - | 2285 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 2286 | `	sxi32 iFlags,        /* Control flags */` |
|        - | 2287 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|        - | 2288 | `	)` |
|        5 | 2289 | `{` |
|        - | 2290 | `	ph7_expr_node *pRoot;` |
|        - | 2291 | `	SySet sExprNode;` |
|        - | 2292 | `	SyToken *pEnd;` |
|        - | 2293 | `	sxi32 nExpr;` |
|        - | 2294 | `	sxi32 iNest;` |
|        - | 2295 | `	sxi32 rc;` |
|        - | 2296 | `	sxu32 nNullsafeBase;` |
|        - | 2297 | `	/* Initialize worker variables */` |
|  1811877 | 2298 | `	nExpr = 0;` |
|  1811877 | 2299 | `	pRoot = 0;` |
|        - | 2300 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|        - | 2301 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  1811877 | 2302 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  1811877 | 2303 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  1811877 | 2304 | `	SySetAlloc(&sExprNode,0x10);` |
|  1811877 | 2305 | `	rc = SXRET_OK;` |
|        - | 2306 | `	/* Delimit the expression */` |
|  1811877 | 2307 | `	pEnd = pGen->pIn;` |
|  1811877 | 2308 | `	iNest = 0;` |
| 13129329 | 2309 | `	while( pEnd < pGen->pEnd ){` |
| 12346165 | 2310 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|        - | 2311 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     3485 | 2312 | `			iNest++;` |
| 12344425 | 2313 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     3495 | 2314 | `			iNest--;` |
| 12340940 | 2315 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  1034371 | 2316 | `			if( iNest <= 0 ){` |
|  1028713 | 2317 | `				break;` |
|        - | 2318 | `			}` |
|     2829 | 2319 | `		}` |
| 11317457 | 2320 | `		pEnd++;` |
|        5 | 2321 | `	}` |
|  1811877 | 2322 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|     2169 | 2323 | `		SyToken *pEnd2 = pGen->pIn;` |
|     2169 | 2324 | `		iNest = 0;` |
|        - | 2325 | `		/* Stop at the first comma */` |
|    17773 | 2326 | `		while( pEnd2 < pEnd ){` |
|    15627 | 2327 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|      251 | 2328 | `				iNest++;` |
|    15504 | 2329 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|      251 | 2330 | `				iNest--;` |
|    15258 | 2331 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|     6125 | 2332 | `				if( iNest <= 0 ){` |
|       21 | 2333 | `					break;` |
|        - | 2334 | `				}` |
|     3051 | 2335 | `			}` |
|    15609 | 2336 | `			pEnd2++;` |
|        5 | 2337 | `		}` |
|     2169 | 2338 | `		if( pEnd2 <pEnd ){` |
|       21 | 2339 | `			pEnd = pEnd2;` |
|        9 | 2340 | `		}` |
|     1082 | 2341 | `	}` |
|  1811877 | 2342 | `	if( pEnd > pGen->pIn ){` |
|  1811863 | 2343 | `		SyToken *pTmp = pGen->pEnd;` |
|        - | 2344 | `		/* Swap delimiter */` |
|  1811863 | 2345 | `		pGen->pEnd = pEnd;` |
|        - | 2346 | `		/* Try to get an expression tree */` |
|  1811863 | 2347 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  1811858 | 2348 | `		if( rc == SXRET_OK && pRoot && pGen->nCommaExprOk < 1` |
|  1769290 | 2349 | `		 && GenStateTreeHasComma(pRoot) ){` |
|        - | 2350 | `			/* php has no comma operator outside a for() clause */` |
|        6 | 2351 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pRoot->pStart->nLine,` |
|        - | 2352 | `				"syntax error, unexpected token \",\"");` |
|        6 | 2353 | `			pGen->pEnd = pTmp;` |
|        6 | 2354 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2355 | `				SySetRelease(&sExprNode);` |
|      ! 0 | 2356 | `				return SXERR_ABORT;` |
|        - | 2357 | `			}` |
|        6 | 2358 | `			pGen->pIn = pEnd;` |
|        6 | 2359 | `			SySetRelease(&sExprNode);` |
|        6 | 2360 | `			SySetTruncate(&pGen->aNullsafeJmp,nNullsafeBase);` |
|        6 | 2361 | `			return SXRET_OK;` |
|        - | 2362 | `		}` |
|  1811859 | 2363 | `		if( rc == SXRET_OK && pRoot ){` |
|  1811669 | 2364 | `			rc = SXRET_OK;` |
|  1811669 | 2365 | `			if( xTreeValidator ){` |
|        - | 2366 | `				/* Call the upper layer validator callback */` |
|   129409 | 2367 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    64702 | 2368 | `			}` |
|  1811669 | 2369 | `			if( rc != SXERR_ABORT ){` |
|        - | 2370 | `				/* Generate code for the given tree */` |
|  1811669 | 2371 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|        - | 2372 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|        - | 2373 | `				 * expression so they short-circuit to its end. */` |
|  1811669 | 2374 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|   905832 | 2375 | `			}` |
|  1811669 | 2376 | `			nExpr = 1;` |
|   905832 | 2377 | `		}` |
|        - | 2378 | `		/* Release the whole tree */` |
|  1811859 | 2379 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|        - | 2380 | `		/* Synchronize token stream */` |
|  1811859 | 2381 | `		pGen->pEnd = pTmp;` |
|  1811859 | 2382 | `		pGen->pIn  = pEnd;` |
|  1811859 | 2383 | `		if( rc == SXERR_ABORT ){` |
|       59 | 2384 | `			SySetRelease(&sExprNode);` |
|       59 | 2385 | `			return SXERR_ABORT;` |
|        - | 2386 | `		}` |
|   905900 | 2387 | `	}` |
|  1811819 | 2388 | `	SySetRelease(&sExprNode);` |
|  1811819 | 2389 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|   905941 | 2390 | `}` |
|        - | 2391 | `/*` |
|        - | 2392 | ` * Return a pointer to the node construct handler associated` |
|        - | 2393 | ` * with a given node type [i.e: string,integer,float,...].` |
|        - | 2394 | ` */` |
|  1048502 | 2395 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|        5 | 2396 | `{` |
|  1048507 | 2397 | `	if( nNodeType & PH7_TK_NUM ){` |
|        - | 2398 | `		/* Numeric literal: Either real or integer */` |
|   657501 | 2399 | `		return PH7_CompileNumLiteral;` |
|   391011 | 2400 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|        - | 2401 | `		/* Double quoted string */` |
|    45091 | 2402 | `		return PH7_CompileString;` |
|   345925 | 2403 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|        - | 2404 | `		/* Single quoted string */` |
|   345793 | 2405 | `		return PH7_CompileSimpleString;` |
|      136 | 2406 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|        - | 2407 | `		/* Heredoc */` |
|       80 | 2408 | `		return PH7_CompileHereDoc;` |
|       60 | 2409 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|        - | 2410 | `		/* Nowdoc */` |
|       56 | 2411 | `		return PH7_CompileNowDoc;` |
|        6 | 2412 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|        - | 2413 | `		/* Backtick quoted string */` |
|        3 | 2414 | `		return PH7_CompileBacktic;` |
|        - | 2415 | `	}` |
|        3 | 2416 | `	return 0;` |
|   524256 | 2417 | `}` |
|        - | 2418 | `/*` |
|        - | 2419 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|        - | 2420 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|        - | 2421 | ` * in write context" parse error.` |
|        - | 2422 | ` */` |
|      228 | 2423 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|        5 | 2424 | `{` |
|        - | 2425 | `	sxi32 rc;` |
|      233 | 2426 | `	rc = GenStateWriteTargetCheck(&(*pGen),pNode,1 /* unset wording for $this */);` |
|      233 | 2427 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 2428 | `		return rc;` |
|        - | 2429 | `	}` |
|      233 | 2430 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|      231 | 2431 | `		return SXRET_OK;` |
|        - | 2432 | `	}` |
|        5 | 2433 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|        2 | 2434 | `		pNode ? pNode->pStart->nLine : 1,` |
|        - | 2435 | `		"Can't use nullsafe operator in write context");` |
|        3 | 2436 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|      119 | 2437 | `}` |
|        - | 2438 | `/*` |
|        - | 2439 | ` * Compile an unset() statement.` |
|        - | 2440 | ` * unset($var, $arr[$key], ...);` |
|        - | 2441 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|        - | 2442 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|        - | 2443 | ` * parent array before extracting the element to unset.` |
|        - | 2444 | ` */` |
|     3114 | 2445 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|        5 | 2446 | `{` |
|     3119 | 2447 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|     3119 | 2448 | `	sxu32 nIdx = 0;` |
|        - | 2449 | `	SyString sName;` |
|        - | 2450 | `	sxi32 rc;` |
|        - | 2451 | `	/* Jump the 'unset' keyword */` |
|     3119 | 2452 | `	pGen->pIn++;` |
|        - | 2453 | `	/* Save delimiter */` |
|     3119 | 2454 | `	pTmp = pGen->pEnd;` |
|        - | 2455 | `	/* Skip optional opening parenthesis and find the matching close */` |
|     3119 | 2456 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|     3119 | 2457 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        - | 2458 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|        - | 2459 | `		SyToken *pClose;` |
|     3119 | 2460 | `		pGen->pIn++;   /* Skip '(' */` |
|     3119 | 2461 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|     3119 | 2462 | `		pEnd = pClose; /* Stop at ')' */` |
|     1557 | 2463 | `	}` |
|     3119 | 2464 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|        - | 2465 | `	/* Resolve the 'unset' builtin name once */` |
|     3119 | 2466 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|      445 | 2467 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      445 | 2468 | `		if( pObj == 0 ){` |
|      ! 0 | 2469 | `			return SXERR_ABORT;` |
|        - | 2470 | `		}` |
|      445 | 2471 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      445 | 2472 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      220 | 2473 | `	}` |
|        - | 2474 | `	/* Compile each comma-separated argument */` |
|    10543 | 2475 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|     7431 | 2476 | `		if( pGen->pIn < pNext ){` |
|        - | 2477 | `			/*` |
|        - | 2478 | ``			 * A PLAIN variable (`unset($x)`, exactly two tokens: '$' and the name) drops a`` |
|        - | 2479 | `			 * single NAME binding, which the unset() builtin cannot express — all it ever` |
|        - | 2480 | `			 * receives is the value's slot index, and unsetting the slot destroys whatever` |
|        - | 2481 | `			 * else aliases it. Emit OP_UNSET_VAR with the name instead. Subscripts and` |
|        - | 2482 | ``			 * properties (`unset($a[k])`, `unset($o->p)`) keep the existing path, which`` |
|        - | 2483 | `			 * already removes just the element/property.` |
|        - | 2484 | `			 */` |
|     7426 | 2485 | `			if( &pGen->pIn[2] == pNext` |
|     7312 | 2486 | `				&& (pGen->pIn[0].nType & PH7_TK_DOLLAR)` |
|     7203 | 2487 | `				&& (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        - | 2488 | `				SyString *pVarName;` |
|        - | 2489 | ``				/* php refuses `unset($this)` where it is written. The tree validator`` |
|        - | 2490 | `				 * cannot see it — this fast path never builds a tree. */` |
|     7196 | 2491 | `				if( pGen->pIn[1].sData.nByte == sizeof("this")-1` |
|     3821 | 2492 | `				 && SyMemcmp((const void *)pGen->pIn[1].sData.zString,` |
|      218 | 2493 | `				             (const void *)"this",sizeof("this")-1) == 0 ){` |
|        3 | 2494 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 2495 | `						"Cannot unset $this");` |
|        3 | 2496 | `					return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 2497 | `				}` |
|    10796 | 2498 | `				char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     7194 | 2499 | `					pGen->pIn[1].sData.zString,pGen->pIn[1].sData.nByte);` |
|     7199 | 2500 | `				pVarName = (SyString *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(SyString));` |
|     7199 | 2501 | `				if( zDup == 0 \|\| pVarName == 0 ){` |
|      ! 0 | 2502 | `					PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 2503 | `						"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2504 | `					return SXERR_ABORT;` |
|        - | 2505 | `				}` |
|     7199 | 2506 | `				SyStringInitFromBuf(pVarName,zDup,pGen->pIn[1].sData.nByte);` |
|     7199 | 2507 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_UNSET_VAR,0,0,pVarName,0);` |
|     7199 | 2508 | `				pGen->pIn = pNext;` |
|     7199 | 2509 | `				if( pGen->pIn < pEnd ){` |
|     4303 | 2510 | `					pGen->pIn++; /* Jump the trailing comma */` |
|     2149 | 2511 | `				}` |
|     7199 | 2512 | `				continue;` |
|        - | 2513 | `			}` |
|      235 | 2514 | `			pGen->pEnd = pNext;` |
|      235 | 2515 | `			rc = PH7_CompileExpr(&(*pGen),` |
|        - | 2516 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|        - | 2517 | `				GenStateUnsetValidator);` |
|      235 | 2518 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2519 | `				return SXERR_ABORT;` |
|        - | 2520 | `			}` |
|      235 | 2521 | `			if( rc != SXERR_EMPTY ){` |
|        - | 2522 | `				/* Emit call for this single argument */` |
|      233 | 2523 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      233 | 2524 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|      233 | 2525 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      114 | 2526 | `			}` |
|      115 | 2527 | `		}` |
|        - | 2528 | `		/* Jump trailing commas */` |
|      251 | 2529 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|       18 | 2530 | `			pNext++;` |
|        2 | 2531 | `		}` |
|      235 | 2532 | `		pGen->pIn = pNext;` |
|        5 | 2533 | `	}` |
|        - | 2534 | `	/* Skip past the closing ')' if present */` |
|     3117 | 2535 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|     3117 | 2536 | `		pGen->pIn++;` |
|     1556 | 2537 | `	}` |
|        - | 2538 | `	/* Restore token stream */` |
|     3117 | 2539 | `	pGen->pEnd = pTmp;` |
|     3117 | 2540 | `	return SXRET_OK;` |
|     1562 | 2541 | `}` |
|        - | 2542 | `/*` |
|        - | 2543 | ` * PHP Language construct table.` |
|        - | 2544 | ` */` |
|        - | 2545 | `static const LangConstruct aLangConstruct[] = {` |
|        - | 2546 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|        - | 2547 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|        - | 2548 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|        - | 2549 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|        - | 2550 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|        - | 2551 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|        - | 2552 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|        - | 2553 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|        - | 2554 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|        - | 2555 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|        - | 2556 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|        - | 2557 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|        - | 2558 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|        - | 2559 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|        - | 2560 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|        - | 2561 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|        - | 2562 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|        - | 2563 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|        - | 2564 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|        - | 2565 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|        - | 2566 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|        - | 2567 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|        - | 2568 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|        - | 2569 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|        - | 2570 | `};` |
|        - | 2571 | `/*` |
|        - | 2572 | ` * Return a pointer to the statement handler routine associated` |
|        - | 2573 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|        - | 2574 | ` */` |
|   906432 | 2575 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|        - | 2576 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|        - | 2577 | `	SyToken *pLookahed  /* Look-ahead token */` |
|        - | 2578 | `	)` |
|        5 | 2579 | `{` |
|   906437 | 2580 | `	sxu32 n = 0;` |
|  2741580 | 2581 | `	for(;;){` |
|  5483165 | 2582 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|     4095 | 2583 | `			break;` |
|        - | 2584 | `		}` |
|  5479075 | 2585 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|   902347 | 2586 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|      ! 0 | 2587 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|      ! 0 | 2588 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|        - | 2589 | `					/* 'static' (class context),return null */` |
|      ! 0 | 2590 | `					return 0;` |
|        - | 2591 | `				}` |
|      ! 0 | 2592 | `			}` |
|   902342 | 2593 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       32 | 2594 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       23 | 2595 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|        - | 2596 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|        3 | 2597 | `				return 0;` |
|        - | 2598 | `			}` |
|        - | 2599 | `			/* Return a pointer to the handler.` |
|        - | 2600 | `			*/` |
|   902345 | 2601 | `			return aLangConstruct[n].xConstruct;` |
|        - | 2602 | `		}` |
|  4576733 | 2603 | `		n++;` |
|        5 | 2604 | `	}` |
|     4095 | 2605 | `	if( pLookahed ){` |
|     4095 | 2606 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|      181 | 2607 | `			return PH7_CompileClassInterface;` |
|     3919 | 2608 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|     3059 | 2609 | `			return PH7_CompileClass;` |
|      865 | 2610 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      189 | 2611 | `			return PH7_CompileTrait;` |
|        - | 2612 | `		}` |
|        - | 2613 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|        - | 2614 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|        - | 2615 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|        - | 2616 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|      338 | 2617 | `	}` |
|        - | 2618 | `	/* Not a language construct */` |
|      681 | 2619 | `	return 0;` |
|   453221 | 2620 | `}` |
|        - | 2621 | `/*` |
|        - | 2622 | ` * Check if the given keyword is in fact a PHP language construct.` |
|        - | 2623 | ` * Return TRUE on success. FALSE otheriwse.` |
|        - | 2624 | ` */` |
|      678 | 2625 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|        5 | 2626 | `{` |
|        - | 2627 | `	int rc;` |
|      683 | 2628 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|      683 | 2629 | `	if( rc == FALSE ){` |
|      482 | 2630 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|      456 | 2631 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|        - | 2632 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|        - | 2633 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|        - | 2634 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|        - | 2635 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|        - | 2636 | `			*/` |
|        - | 2637 | `			){` |
|      479 | 2638 | `				rc = TRUE;` |
|      237 | 2639 | `		}` |
|      241 | 2640 | `	}` |
|      683 | 2641 | `	return rc;` |
|        5 | 2642 | `}` |
|        - | 2643 | `/*` |
|        - | 2644 | ` * Compile a PHP chunk.` |
|        - | 2645 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|        - | 2646 | ` * takes care of generating the appropriate error message.` |
|        - | 2647 | ` */` |
|        - | 2648 | `/*` |
|        - | 2649 | ` * Update pGen->sPendingDoc for the statement whose first token is` |
|        - | 2650 | ` * pGen->pIn: when a docblock trivia is keyed to that token's index in` |
|        - | 2651 | ` * the chunk token set it becomes the pending docblock. An existing` |
|        - | 2652 | ` * pending docblock is LEFT in place otherwise: Zend keeps the last-seen` |
|        - | 2653 | ` * doc comment until a declaration consumes it, so a docblock survives` |
|        - | 2654 | ` * intervening non-declaration statements.` |
|        - | 2655 | ` */` |
|  1587846 | 2656 | `PH7_PRIVATE void GenStateSetPendingDoc(ph7_gen_state *pGen)` |
|        5 | 2657 | `{` |
|  1587851 | 2658 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|  1587851 | 2659 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|  1587851 | 2660 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|        - | 2661 | `	sxu32 nIdx, n;` |
|  1587846 | 2662 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|     4473 | 2663 | `	 \|\| pGen->pIn < pBase \|\| pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|        - | 2664 | `		/* Re-tokenized substream (string interpolation, synthesized code):` |
|        - | 2665 | `		 * indexes do not map to the sidecar */` |
|  1583383 | 2666 | `		return;` |
|        - | 2667 | `	}` |
|     4473 | 2668 | `	nIdx = (sxu32)(pGen->pIn - pBase);` |
|        - | 2669 | `	/* Attributes must be adjacent to their declaration (unlike docblocks):` |
|        - | 2670 | `	 * reset at every boundary, then collect the groups keyed to this token. */` |
|     4473 | 2671 | `	SySetReset(&pGen->aPendingAttrs);` |
|    15331 | 2672 | `	for( n = 0 ; n < nT ; n++ ){` |
|    10863 | 2673 | `		if( aT[n].nTokIdx != nIdx ){` |
|    10591 | 2674 | `			continue;` |
|        - | 2675 | `		}` |
|      277 | 2676 | `		if( aT[n].iKind == PH7_TRIVIA_DOC ){` |
|       59 | 2677 | `			pGen->sPendingDoc = aT[n].sText;` |
|      250 | 2678 | `		}else if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|      223 | 2679 | `			SySetPut(&pGen->aPendingAttrs,(const void *)&aT[n]);` |
|      109 | 2680 | `		}` |
|      141 | 2681 | `	}` |
|   793928 | 2682 | `}` |
|        - | 2683 | `/*` |
|        - | 2684 | ` * Hand the pending docblock (if any) to a declaration: duplicate it into` |
|        - | 2685 | ` * the VM allocator (the raw script buffer dies after compilation) and` |
|        - | 2686 | ` * clear the pending slot so sibling declarations do not inherit it.` |
|        - | 2687 | ` */` |
|   154354 | 2688 | `PH7_PRIVATE void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut)` |
|        5 | 2689 | `{` |
|        - | 2690 | `	char *zDup;` |
|   154359 | 2691 | `	if( SyStringLength(&pGen->sPendingDoc) < 1 ){` |
|   154309 | 2692 | `		return;` |
|        - | 2693 | `	}` |
|       80 | 2694 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       25 | 2695 | `		SyStringData(&pGen->sPendingDoc),SyStringLength(&pGen->sPendingDoc));` |
|       55 | 2696 | `	if( zDup ){` |
|       55 | 2697 | `		SyStringInitFromBuf(pOut,zDup,SyStringLength(&pGen->sPendingDoc));` |
|       25 | 2698 | `	}` |
|       55 | 2699 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|    77182 | 2700 | `}` |
|        - | 2701 | `/*` |
|        - | 2702 | ` * Compile one recorded #[...] attribute group (the span between the group` |
|        - | 2703 | ` * delimiters) into ph7_attribute records appended to pOut. The span is` |
|        - | 2704 | ` * duplicated into the VM allocator FIRST (compiled bytecode and interned` |
|        - | 2705 | ` * names may point into the token text, which must outlive the raw script` |
|        - | 2706 | ` * buffer), then re-tokenized on its own. Each argument expression compiles` |
|        - | 2707 | ` * with the container-swap idiom into its own OP_DONE-terminated set,` |
|        - | 2708 | ` * evaluated lazily at ReflectionAttribute time (PHP semantics).` |
|        - | 2709 | ` */` |
|      232 | 2710 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut)` |
|        5 | 2711 | `{` |
|        - | 2712 | `	SySet *pToken;` |
|        - | 2713 | `	SyToken *pIn, *pEnd, *pSavedIn, *pSavedEnd;` |
|        - | 2714 | `	char *zSpan;` |
|      237 | 2715 | `	sxi32 rc = SXRET_OK;` |
|      237 | 2716 | `	if( SyStringLength(&pTrivia->sText) < 1 ){` |
|      ! 0 | 2717 | `		return SXRET_OK;` |
|        - | 2718 | `	}` |
|      353 | 2719 | `	zSpan = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      116 | 2720 | `		SyStringData(&pTrivia->sText),SyStringLength(&pTrivia->sText));` |
|      237 | 2721 | `	if( zSpan == 0 ){` |
|      ! 0 | 2722 | `		return SXRET_OK;` |
|        - | 2723 | `	}` |
|        - | 2724 | `	/* The token set must outlive compilation too: interned operands may` |
|        - | 2725 | `	 * reference token payloads. Pool-allocated, never released — bounded by` |
|        - | 2726 | `	 * the number of attribute declarations in the program. */` |
|      237 | 2727 | `	pToken = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      237 | 2728 | `	if( pToken == 0 ){` |
|      ! 0 | 2729 | `		return SXRET_OK;` |
|        - | 2730 | `	}` |
|      237 | 2731 | `	SySetInit(pToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      237 | 2732 | `	PH7_TokenizePHP(zSpan,SyStringLength(&pTrivia->sText),pTrivia->nLine,pToken,0);` |
|      237 | 2733 | `	pIn = (SyToken *)SySetBasePtr(pToken);` |
|      237 | 2734 | `	pEnd = &pIn[SySetUsed(pToken)];` |
|      237 | 2735 | `	pSavedIn = pGen->pIn;` |
|      237 | 2736 | `	pSavedEnd = pGen->pEnd;` |
|      241 | 2737 | `	while( pIn < pEnd ){` |
|        - | 2738 | `		ph7_attribute sAttr;` |
|        - | 2739 | `		SyBlob sFQN;` |
|      241 | 2740 | `		int bAbsolute = 0;` |
|      241 | 2741 | `		SyZero(&sAttr,sizeof(sAttr));` |
|      241 | 2742 | `		SySetInit(&sAttr.aArgs,&pGen->pVm->sAllocator,sizeof(ph7_attr_arg));` |
|      241 | 2743 | `		sAttr.nLine = pIn->nLine;` |
|      241 | 2744 | `		if( pIn->nType & PH7_TK_NSSEP ){` |
|       89 | 2745 | `			bAbsolute = 1;` |
|       89 | 2746 | `			pIn++;` |
|       42 | 2747 | `		}` |
|      241 | 2748 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        - | 2749 | ``		/* `#[namespace\Attr]` — the current namespace, absolute from there. */`` |
|      241 | 2750 | `		if( !bAbsolute && GenStateNsRelPrefix(pGen,&pIn,pEnd,&sFQN) ){` |
|      ! 0 | 2751 | `			bAbsolute = 1;` |
|      ! 0 | 2752 | `		}` |
|      241 | 2753 | `		while( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      241 | 2754 | `			SyBlobAppend(&sFQN,pIn->sData.zString,pIn->sData.nByte);` |
|      241 | 2755 | `			pIn++;` |
|      241 | 2756 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|      ! 0 | 2757 | `				SyBlobAppend(&sFQN,"\\",1);` |
|      ! 0 | 2758 | `				pIn++;` |
|      ! 0 | 2759 | `				continue;` |
|        - | 2760 | `			}` |
|      241 | 2761 | `			break;` |
|      ! 0 | 2762 | `		}` |
|      241 | 2763 | `		if( SyBlobLength(&sFQN) < 1 ){` |
|        - | 2764 | `			/* Malformed group: stop quietly (the group was inert trivia before` |
|        - | 2765 | `			 * this feature; never turn it into a new fatal) */` |
|      ! 0 | 2766 | `			SyBlobRelease(&sFQN);` |
|      ! 0 | 2767 | `			break;` |
|        - | 2768 | `		}` |
|        - | 2769 | `		/* Resolve to an FQN: absolute names verbatim; else use-import alias,` |
|        - | 2770 | `		 * else current-namespace prefix (PHP attribute name resolution) */` |
|        - | 2771 | `		{` |
|      241 | 2772 | `			const char *zName = (const char *)SyBlobData(&sFQN);` |
|      241 | 2773 | `			sxu32 nName = SyBlobLength(&sFQN);` |
|      241 | 2774 | `			char *zDup = 0;` |
|      241 | 2775 | `			if( !bAbsolute ){` |
|      155 | 2776 | `				SyHashEntry *pImp = SyHashGet(&pGen->hUseImports,(const void *)zName,nName);` |
|      155 | 2777 | `				if( pImp ){` |
|        3 | 2778 | `					const char *zFqn = (const char *)pImp->pUserData;` |
|        3 | 2779 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFqn,SyStrlen(zFqn));` |
|        3 | 2780 | `					if( zDup ){` |
|        3 | 2781 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyStrlen(zDup));` |
|        2 | 2782 | `					}` |
|      153 | 2783 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        - | 2784 | `					SyBlob sTmp;` |
|      ! 0 | 2785 | `					SyBlobInit(&sTmp,&pGen->pVm->sAllocator);` |
|      ! 0 | 2786 | `					SyBlobAppend(&sTmp,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      ! 0 | 2787 | `					SyBlobAppend(&sTmp,"\\",1);` |
|      ! 0 | 2788 | `					SyBlobAppend(&sTmp,zName,nName);` |
|      ! 0 | 2789 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      ! 0 | 2790 | `						(const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|      ! 0 | 2791 | `					if( zDup ){` |
|      ! 0 | 2792 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyBlobLength(&sTmp));` |
|      ! 0 | 2793 | `					}` |
|      ! 0 | 2794 | `					SyBlobRelease(&sTmp);` |
|      ! 0 | 2795 | `				}` |
|       76 | 2796 | `			}` |
|      241 | 2797 | `			if( SyStringLength(&sAttr.sName) < 1 ){` |
|      239 | 2798 | `				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|      239 | 2799 | `				if( zDup ){` |
|      239 | 2800 | `					SyStringInitFromBuf(&sAttr.sName,zDup,nName);` |
|      117 | 2801 | `				}` |
|      117 | 2802 | `			}` |
|        - | 2803 | `		}` |
|      241 | 2804 | `		SyBlobRelease(&sFQN);` |
|      241 | 2805 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|        - | 2806 | `			SyToken *pArgsEnd;` |
|       95 | 2807 | `			pIn++;` |
|       95 | 2808 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pArgsEnd);` |
|      261 | 2809 | `			while( pIn < pArgsEnd ){` |
|      169 | 2810 | `				SyToken *pArgStart = pIn, *pArgStop = pIn;` |
|      169 | 2811 | `				sxi32 iDepth = 0;` |
|        - | 2812 | `				ph7_attr_arg sArgRec;` |
|      577 | 2813 | `				while( pArgStop < pArgsEnd ){` |
|      487 | 2814 | `					if( pArgStop->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       29 | 2815 | `						iDepth++;` |
|      473 | 2816 | `					}else if( pArgStop->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       29 | 2817 | `						iDepth--;` |
|      445 | 2818 | `					}else if( (pArgStop->nType & PH7_TK_COMMA) && iDepth == 0 ){` |
|       77 | 2819 | `						break;` |
|        - | 2820 | `					}` |
|      411 | 2821 | `					pArgStop++;` |
|        3 | 2822 | `				}` |
|      169 | 2823 | `				SyZero(&sArgRec,sizeof(sArgRec));` |
|      169 | 2824 | `				SySetInit(&sArgRec.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      166 | 2825 | `				if( pArgStart < pArgStop && (pArgStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|      115 | 2826 | `				 && &pArgStart[1] < pArgStop && (pArgStart[1].nType & PH7_TK_COLON) ){` |
|       37 | 2827 | `					char *zN = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       12 | 2828 | `						pArgStart->sData.zString,pArgStart->sData.nByte);` |
|       25 | 2829 | `					if( zN ){` |
|       25 | 2830 | `						SyStringInitFromBuf(&sArgRec.sName,zN,pArgStart->sData.nByte);` |
|       12 | 2831 | `					}` |
|       25 | 2832 | `					pArgStart += 2;` |
|       12 | 2833 | `				}` |
|      169 | 2834 | `				if( pArgStart < pArgStop ){` |
|        - | 2835 | `					SySet *pInstrContainer;` |
|      169 | 2836 | `					pGen->pIn = pArgStart;` |
|      169 | 2837 | `					pGen->pEnd = pArgStop;` |
|      169 | 2838 | `					pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      169 | 2839 | `					PH7_VmSetByteCodeContainer(pGen->pVm,&sArgRec.aByteCode);` |
|      169 | 2840 | `					rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      169 | 2841 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      169 | 2842 | `					PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      169 | 2843 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 2844 | `						pGen->pIn = pSavedIn;` |
|      ! 0 | 2845 | `						pGen->pEnd = pSavedEnd;` |
|      ! 0 | 2846 | `						return SXERR_ABORT;` |
|        - | 2847 | `					}` |
|      169 | 2848 | `					SySetPut(&sAttr.aArgs,(const void *)&sArgRec);` |
|       83 | 2849 | `				}` |
|      169 | 2850 | `				pIn = pArgStop;` |
|      169 | 2851 | `				if( pIn < pArgsEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|       77 | 2852 | `					pIn++;` |
|       38 | 2853 | `				}` |
|        3 | 2854 | `			}` |
|       95 | 2855 | `			pIn = (pArgsEnd < pEnd) ? &pArgsEnd[1] : pEnd;` |
|       46 | 2856 | `		}` |
|      241 | 2857 | `		SySetPut(pOut,(const void *)&sAttr);` |
|      241 | 2858 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        5 | 2859 | `			pIn++;` |
|        5 | 2860 | `			continue;` |
|        - | 2861 | `		}` |
|      237 | 2862 | `		break;` |
|      ! 0 | 2863 | `	}` |
|      237 | 2864 | `	pGen->pIn = pSavedIn;` |
|      237 | 2865 | `	pGen->pEnd = pSavedEnd;` |
|      237 | 2866 | `	return SXRET_OK;` |
|      121 | 2867 | `}` |
|        - | 2868 | `/*` |
|        - | 2869 | ` * Hand the pending attribute groups (if any) to a declaration: compile` |
|        - | 2870 | ` * every recorded group into pOut and clear the pending list.` |
|        - | 2871 | ` */` |
|   154360 | 2872 | `PH7_PRIVATE sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut)` |
|        5 | 2873 | `{` |
|   154365 | 2874 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|        - | 2875 | `	sxu32 n;` |
|        - | 2876 | `	sxi32 rc;` |
|   154583 | 2877 | `	for( n = 0 ; n < SySetUsed(&pGen->aPendingAttrs) ; n++ ){` |
|      223 | 2878 | `		rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|      223 | 2879 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2880 | `			return SXERR_ABORT;` |
|        - | 2881 | `		}` |
|      114 | 2882 | `	}` |
|   154365 | 2883 | `	SySetReset(&pGen->aPendingAttrs);` |
|   154365 | 2884 | `	return SXRET_OK;` |
|    77185 | 2885 | `}` |
|        - | 2886 | `/*` |
|        - | 2887 | ` * Compile the attribute groups keyed to the given token (a parameter's` |
|        - | 2888 | ` * first token inside a signature) into pOut. Parameters are parsed from` |
|        - | 2889 | ` * the main token stream, so the sidecar indexes map directly.` |
|        - | 2890 | ` */` |
|   272730 | 2891 | `PH7_PRIVATE sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut)` |
|        5 | 2892 | `{` |
|   272735 | 2893 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|   272735 | 2894 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|   272735 | 2895 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|        - | 2896 | `	sxu32 nIdx, n;` |
|        - | 2897 | `	sxi32 rc;` |
|   272730 | 2898 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|      573 | 2899 | `	 \|\| pTok < pBase \|\| pTok >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|   272167 | 2900 | `		return SXRET_OK;` |
|        - | 2901 | `	}` |
|      573 | 2902 | `	nIdx = (sxu32)(pTok - pBase);` |
|     1675 | 2903 | `	for( n = 0 ; n < nT ; n++ ){` |
|     1107 | 2904 | `		if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|       16 | 2905 | `			rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|       16 | 2906 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2907 | `				return SXERR_ABORT;` |
|        - | 2908 | `			}` |
|        7 | 2909 | `		}` |
|      556 | 2910 | `	}` |
|      573 | 2911 | `	return SXRET_OK;` |
|   136370 | 2912 | `}` |
|  1401416 | 2913 | `PH7_PRIVATE sxi32 GenStateCompileChunk(` |
|        - | 2914 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 2915 | `	sxi32 iFlags         /* Compile flags */` |
|        - | 2916 | `	)` |
|        5 | 2917 | `{` |
|        - | 2918 | `	ProcLangConstruct xCons;` |
|        - | 2919 | `	sxi32 rc;` |
|  1401421 | 2920 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|   897184 | 2921 | `	for(;;){` |
|  1597897 | 2922 | `		int bStmtIsDeclare = 0;` |
|  1597897 | 2923 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 2924 | `			/* No more input to process */` |
|    15829 | 2925 | `			break;` |
|        - | 2926 | `		}` |
|        - | 2927 | `		/* Bind a directly-preceding docblock to this statement */` |
|  1582073 | 2928 | `		GenStateSetPendingDoc(&(*pGen));` |
|  1582073 | 2929 | `		if( SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|        - | 2930 | `			/* php: a statement-position attribute group must be followed by a` |
|        - | 2931 | ``			 * declaration (function/class-like/const) — `#[A] $x = 1;` is a`` |
|        - | 2932 | `` 			 * parse error, never a silent discard. `static`/`fn`/`function` `` |
|        - | 2933 | ``			 * cover bare closure-expression statements; `readonly`/`enum` are`` |
|        - | 2934 | `			 * context-sensitive IDs handled by the modified-class/enum scans. */` |
|      107 | 2935 | `			int bAttrTarget = 0;` |
|      104 | 2936 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd)` |
|      107 | 2937 | `			 \|\| GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|      ! 0 | 2938 | `				bAttrTarget = 1;` |
|      107 | 2939 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      107 | 2940 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      104 | 2941 | `				if( nKw == PH7_TKWRD_FUNCTION \|\| nKw == PH7_TKWRD_CLASS` |
|       39 | 2942 | `				 \|\| nKw == PH7_TKWRD_INTERFACE \|\| nKw == PH7_TKWRD_TRAIT` |
|        6 | 2943 | `				 \|\| nKw == PH7_TKWRD_ABSTRACT \|\| nKw == PH7_TKWRD_FINAL` |
|        6 | 2944 | `				 \|\| nKw == PH7_TKWRD_CONST \|\| nKw == PH7_TKWRD_STATIC` |
|        3 | 2945 | `				 \|\| nKw == PH7_TKWRD_FN ){` |
|      107 | 2946 | `					bAttrTarget = 1;` |
|       52 | 2947 | `				}` |
|       52 | 2948 | `			}` |
|      107 | 2949 | `			if( !bAttrTarget ){` |
|      ! 0 | 2950 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 2951 | `					"syntax error, unexpected token \"%z\" after attribute group; expecting a declaration",` |
|      ! 0 | 2952 | `					&pGen->pIn->sData);` |
|      ! 0 | 2953 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2954 | `					break;` |
|        - | 2955 | `				}` |
|      ! 0 | 2956 | `				SySetReset(&pGen->aPendingAttrs);` |
|      ! 0 | 2957 | `			}` |
|       52 | 2958 | `		}` |
|        - | 2959 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|        - | 2960 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  1582073 | 2961 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   906551 | 2962 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   906551 | 2963 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|       61 | 2964 | `				bStmtIsDeclare = 1;` |
|       28 | 2965 | `			}` |
|   453273 | 2966 | `		}` |
|  1582073 | 2967 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|        - | 2968 | `			/* Any non-declare top-level statement locks the strict_types` |
|        - | 2969 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|   196501 | 2970 | `			pGen->bStrictTypesLocked = 1;` |
|    98248 | 2971 | `		}` |
|  1582073 | 2972 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|        - | 2973 | `			/* Compile block */` |
|       59 | 2974 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|       59 | 2975 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2976 | `				break;` |
|        - | 2977 | `			}` |
|       32 | 2978 | `		}else{` |
|  1582019 | 2979 | `			xCons = 0;` |
|  1582019 | 2980 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|        - | 2981 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|        - | 2982 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|        - | 2983 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|      143 | 2984 | `				xCons = PH7_CompileClassModifiers;` |
|  1581950 | 2985 | `			}else if( GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|        - | 2986 | ``				/* `enum Name …` (PHP 8.1) — `enum` is a context-sensitive ID,`` |
|        - | 2987 | `				 * so it is detected here rather than the keyword dispatcher. */` |
|      103 | 2988 | `				xCons = PH7_CompileEnum;` |
|  1581832 | 2989 | `			}else if( GenStateIsNsRelName(pGen->pIn,pGen->pEnd) ){` |
|        - | 2990 | ``				/* A statement that STARTS with php's `namespace\X` name operator`` |
|        - | 2991 | ``				 * (`namespace\Cee::m();`) is an expression, not a namespace`` |
|        - | 2992 | ``				 * DECLARATION — the glued `\` is what tells the two apart. */`` |
|        7 | 2993 | `				xCons = 0;` |
|  1581780 | 2994 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   906437 | 2995 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        - | 2996 | `				/* Try to extract a language construct handler */` |
|   906437 | 2997 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|   906437 | 2998 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|       13 | 2999 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3000 | `						"Syntax error: Unexpected keyword '%z'",` |
|        8 | 3001 | `						&pGen->pIn->sData);` |
|        9 | 3002 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3003 | `						break;` |
|        - | 3004 | `					}` |
|        - | 3005 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|        - | 3006 | `					 * this erroneous statement.` |
|        - | 3007 | `					 */` |
|        9 | 3008 | `					xCons = PH7_ErrorRecover;` |
|        4 | 3009 | `				}` |
|  1128561 | 3010 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|    60985 | 3011 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|        - | 3012 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|      217 | 3013 | `				xCons = PH7_CompileLabel;` |
|      106 | 3014 | `			}` |
|  1582019 | 3015 | `			if( xCons == 0 ){` |
|        - | 3016 | `				/* Assume an expression an try to compile it */` |
|   675809 | 3017 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   675809 | 3018 | `				if(  rc != SXERR_EMPTY ){` |
|        - | 3019 | `					/* Pop l-value */` |
|   675645 | 3020 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|   337820 | 3021 | `				}` |
|   337907 | 3022 | `			}else{` |
|        - | 3023 | `				/* Go compile the sucker */` |
|   906215 | 3024 | `				rc = xCons(&(*pGen));` |
|        - | 3025 | `			}` |
|  1582019 | 3026 | `			if( rc == SXERR_ABORT ){` |
|        - | 3027 | `				/* Request to abort compilation */` |
|       81 | 3028 | `				break;` |
|        - | 3029 | `			}` |
|        - | 3030 | `		}` |
|        - | 3031 | `		/* Ignore trailing semi-colons ';' */` |
|  2618123 | 3032 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  1036131 | 3033 | `			pGen->pIn++;` |
|        5 | 3034 | `		}` |
|  1581997 | 3035 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|        - | 3036 | `			/* Compile a single statement and return */` |
|  1385521 | 3037 | `			break;` |
|        - | 3038 | `		}` |
|        - | 3039 | `		/* LOOP ONE */` |
|        - | 3040 | `		/* LOOP TWO */` |
|        - | 3041 | `		/* LOOP THREE */` |
|        - | 3042 | `		/* LOOP FOUR */` |
|        5 | 3043 | `	}` |
|        - | 3044 | `	/* Return compilation status */` |
|  1401421 | 3045 | `	return rc;` |
|        5 | 3046 | `}` |
|        - | 3047 | `/*` |
|        - | 3048 | ` * Compile a Raw PHP chunk.` |
|        - | 3049 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|        - | 3050 | ` * takes care of generating the appropriate error message.` |
|        - | 3051 | ` */` |
|    15902 | 3052 | `static sxi32 PH7_CompilePHP(` |
|        - | 3053 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - | 3054 | `	SySet *pTokenSet,     /* Token set */` |
|        - | 3055 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|        - | 3056 | `	)` |
|        5 | 3057 | `{` |
|    15907 | 3058 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|        - | 3059 | `	sxi32 rc;` |
|        - | 3060 | `	/* Reset the token set (and its trivia sidecar) */` |
|    15907 | 3061 | `	SySetReset(&(*pTokenSet));` |
|    15907 | 3062 | `	SySetReset(&pGen->aTrivia);` |
|        - | 3063 | `	/* Mark as the default token set */` |
|    15907 | 3064 | `	pGen->pTokenSet = &(*pTokenSet);` |
|        - | 3065 | `	/* Advance the stream cursor */` |
|    15907 | 3066 | `	pGen->pRawIn++;` |
|        - | 3067 | `	/* Tokenize the PHP chunk first */` |
|    15907 | 3068 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet),&pGen->aTrivia);` |
|        - | 3069 | `	/* Point to the head and tail of the token stream. */` |
|    15907 | 3070 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|    15907 | 3071 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|    15907 | 3072 | `	if( is_expr ){` |
|      ! 0 | 3073 | `		rc = SXERR_EMPTY;` |
|      ! 0 | 3074 | `		if( pGen->pIn < pGen->pEnd ){` |
|        - | 3075 | `			/* A simple expression,compile it */` |
|      ! 0 | 3076 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|      ! 0 | 3077 | `		}` |
|        - | 3078 | `		/* Emit the DONE instruction */` |
|      ! 0 | 3079 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      ! 0 | 3080 | `		return SXRET_OK;` |
|        - | 3081 | `	}` |
|    15907 | 3082 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|        - | 3083 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|        - | 3084 | `		/*` |
|        - | 3085 | `		 * Shortcut syntax for the 'echo' language construct.` |
|        - | 3086 | `		 * According to the PHP reference manual:` |
|        - | 3087 | `		 *  echo() also has a shortcut syntax, where you can` |
|        - | 3088 | `		 *  immediately follow` |
|        - | 3089 | `		 *  the opening tag with an equals sign as follows:` |
|        - | 3090 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|        - | 3091 | `		 * Symisc extension:` |
|        - | 3092 | `		 *   This short syntax works with all PHP opening` |
|        - | 3093 | `		 *   tags unlike the default PHP engine that handle` |
|        - | 3094 | `		 *   only short tag.` |
|        - | 3095 | `		 */` |
|        - | 3096 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|        3 | 3097 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|        3 | 3098 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|        3 | 3099 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|        - | 3100 | `		/* This synthesized echo is compiled as an EXPRESSION, which is otherwise a` |
|        - | 3101 | `		 * parse error; allow it for the duration of this one compile. */` |
|        3 | 3102 | `		pGen->nExprEchoOk++;` |
|        3 | 3103 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|        3 | 3104 | `		pGen->nExprEchoOk--;` |
|        3 | 3105 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3106 | `			return SXERR_ABORT;` |
|        - | 3107 | `		}` |
|        3 | 3108 | `		if( rc != SXERR_EMPTY ){` |
|        3 | 3109 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        1 | 3110 | `		}` |
|        3 | 3111 | `		return SXRET_OK;` |
|        - | 3112 | `	}` |
|        - | 3113 | `	/* Compile the PHP chunk */` |
|    15905 | 3114 | `	rc = GenStateCompileChunk(pGen,0);` |
|        - | 3115 | `	/* Fix exceptions jumps */` |
|    15905 | 3116 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        - | 3117 | `	/* Fix gotos now, the jump destination is resolved */` |
|    15905 | 3118 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|        3 | 3119 | `		rc = SXERR_ABORT;` |
|        1 | 3120 | `	}` |
|        - | 3121 | `	/* Reset container */` |
|    15905 | 3122 | `	SySetReset(&pGen->aGoto);` |
|    15905 | 3123 | `	SySetReset(&pGen->aLabel);` |
|    15905 | 3124 | `	SySetReset(&pGen->aNullsafeJmp);` |
|        - | 3125 | `	/* Compilation result */` |
|    15905 | 3126 | `	return rc;` |
|     7956 | 3127 | `}` |
|        - | 3128 | `/*` |
|        - | 3129 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|        - | 3130 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|        - | 3131 | ` * This is the only compile interface exported from this file.` |
|        - | 3132 | ` */` |
|    19568 | 3133 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|        - | 3134 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|        - | 3135 | `	SyString *pScript,  /* Script to compile */` |
|        - | 3136 | `	sxi32 iFlags        /* Compile flags */` |
|        - | 3137 | `	)` |
|        5 | 3138 | `{` |
|        - | 3139 | `	SySet aPhpToken,aRawToken;` |
|        - | 3140 | `	ph7_gen_state *pCodeGen;` |
|        - | 3141 | `	ph7_value *pRawObj;` |
|        - | 3142 | `	sxu32 nObjIdx;` |
|        - | 3143 | `	sxi32 nRawObj;` |
|        - | 3144 | `	int is_expr;` |
|        - | 3145 | `	sxi8 bSavedStrict;` |
|        - | 3146 | `	sxi8 bSavedStrictLocked;` |
|        - | 3147 | `	SyToken *pSavedIn,*pSavedEnd;` |
|        - | 3148 | `	sxi32 rc;` |
|    19573 | 3149 | `	sxu32 nBaseLine = 1;` |
|    19573 | 3150 | `	if( pScript->nByte < 1 ){` |
|        - | 3151 | `		/* Nothing to compile */` |
|      ! 0 | 3152 | `		return PH7_OK;` |
|        - | 3153 | `	}` |
|        - | 3154 | `	/* php skips a "#!" shebang on the first line of a CLI script: consume it` |
|        - | 3155 | `	 * (including its newline) so it is not echoed as inline text, and bump the` |
|        - | 3156 | `	 * base line to 2 so the code below still reports php-matching line numbers. */` |
|    19573 | 3157 | `	if( pScript->nByte >= 2 && pScript->zString[0]=='#' && pScript->zString[1]=='!' ){` |
|        3 | 3158 | `		const char *z = pScript->zString;` |
|        3 | 3159 | `		const char *zEnd = &z[pScript->nByte];` |
|       39 | 3160 | `		while( z < zEnd && z[0] != '\n' ){ z++; }` |
|        3 | 3161 | `		if( z < zEnd ){ z++; } /* consume the newline too */` |
|        3 | 3162 | `		pScript->nByte -= (sxu32)(z - pScript->zString);` |
|        3 | 3163 | `		pScript->zString = z;` |
|        3 | 3164 | `		nBaseLine = 2;` |
|        3 | 3165 | `		if( pScript->nByte < 1 ){` |
|      ! 0 | 3166 | `			return PH7_OK;` |
|        - | 3167 | `		}` |
|        1 | 3168 | `	}` |
|        - | 3169 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|        - | 3170 | `	 * file's flags so include/require restore them on return. */` |
|    19573 | 3171 | `	pCodeGen = &pVm->sCodeGen;` |
|        - | 3172 | ``	/* aPhpToken below is a LOCAL token set: once it is released at `cleanup`, any`` |
|        - | 3173 | `	 * pGen->pIn/pEnd still pointing into it DANGLE. PH7_VmEmitInstr reads pIn to stamp` |
|        - | 3174 | `	 * each instruction's source line, and instructions are still emitted after this` |
|        - | 3175 | `	 * function returns (VmEvalChunk's trailing OP_DONE) -- a use-after-free ASan caught` |
|        - | 3176 | `	 * immediately. Save the caller's cursor and restore it on the way out, so an` |
|        - | 3177 | `	 * enclosing compile keeps its (live) tokens and the top level goes back to NULL. */` |
|    19573 | 3178 | `	pSavedIn = pCodeGen->pIn;` |
|    19573 | 3179 | `	pSavedEnd = pCodeGen->pEnd;` |
|    19573 | 3180 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|    19573 | 3181 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|    19573 | 3182 | `	pCodeGen->bStrictTypes = 0;` |
|    19573 | 3183 | `	pCodeGen->bStrictTypesLocked = 0;` |
|        - | 3184 | `	/* Initialize the tokens containers */` |
|    19573 | 3185 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|    19573 | 3186 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|    19573 | 3187 | `	SySetAlloc(&aPhpToken,0xc0);` |
|    19573 | 3188 | `	is_expr = 0;` |
|    19573 | 3189 | `	if( iFlags & PH7_PHP_ONLY ){` |
|        - | 3190 | `		SyToken sTmp;` |
|        - | 3191 | `		/* PHP only: -*/` |
|     4973 | 3192 | `		sTmp.nLine = 1;` |
|     4973 | 3193 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|     4973 | 3194 | `		sTmp.pUserData = 0;` |
|     4973 | 3195 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|     4973 | 3196 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|     4973 | 3197 | `		if( iFlags & PH7_PHP_EXPR ){` |
|        - | 3198 | `			/* A simple PHP expression */` |
|      ! 0 | 3199 | `			is_expr = 1;` |
|      ! 0 | 3200 | `		}` |
|     2489 | 3201 | `	}else{` |
|        - | 3202 | `		/* Tokenize raw text */` |
|    14605 | 3203 | `		SySetAlloc(&aRawToken,32);` |
|    14605 | 3204 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken,nBaseLine);` |
|        - | 3205 | `	}` |
|        - | 3206 | `	/* Process high-level tokens */` |
|    19573 | 3207 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|    19573 | 3208 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|    19573 | 3209 | `	rc = PH7_OK;` |
|    19573 | 3210 | `	if( is_expr ){` |
|        - | 3211 | `		/* Compile the expression */` |
|      ! 0 | 3212 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|      ! 0 | 3213 | `		goto cleanup;` |
|        - | 3214 | `	}` |
|    19573 | 3215 | `	nObjIdx = 0;` |
|        - | 3216 | `	/* Start the compilation process */` |
|    17088 | 3217 | `	for(;;){` |
|    50005 | 3218 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|    19495 | 3219 | `			break; /* No more tokens to process */` |
|        - | 3220 | `		}` |
|    30515 | 3221 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|        - | 3222 | `			/* Compile the PHP chunk */` |
|    15907 | 3223 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|    15907 | 3224 | `			if( rc == SXERR_ABORT ){` |
|       83 | 3225 | `				break;` |
|        - | 3226 | `			}` |
|    15829 | 3227 | `			continue;` |
|        - | 3228 | `		}` |
|        - | 3229 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|    14613 | 3230 | `		nRawObj = 0;` |
|    29221 | 3231 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|        - | 3232 | `			/* Consume the raw chunk without any processing */` |
|    14613 | 3233 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|    14613 | 3234 | `			if( pRawObj == 0 ){` |
|      ! 0 | 3235 | `				rc = SXERR_MEM;` |
|      ! 0 | 3236 | `				break;` |
|        - | 3237 | `			}` |
|        - | 3238 | `			/* Mark as constant and emit the load constant instruction */` |
|    14613 | 3239 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|    14613 | 3240 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|    14613 | 3241 | `			++nRawObj;` |
|    14613 | 3242 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|        5 | 3243 | `		}` |
|    14613 | 3244 | `		if( nRawObj > 0 ){` |
|        - | 3245 | `			/* Emit the consume instruction */` |
|    14613 | 3246 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     7304 | 3247 | `		}` |
|     9789 | 3248 | `	}` |
|     9784 | 3249 | `cleanup:` |
|        - | 3250 | `	/* Drop the cursor into the token set BEFORE the set is freed (see above). */` |
|    19573 | 3251 | `	pCodeGen->pIn = pSavedIn;` |
|    19573 | 3252 | `	pCodeGen->pEnd = pSavedEnd;` |
|    19573 | 3253 | `	SySetRelease(&aRawToken);` |
|    19573 | 3254 | `	SySetRelease(&aPhpToken);` |
|        - | 3255 | `	/* Restore outer file's strict_types scope */` |
|    19573 | 3256 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|    19573 | 3257 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|    19573 | 3258 | `	return rc;` |
|     9789 | 3259 | `}` |
|        - | 3260 | `/*` |
|        - | 3261 | ` * Utility routines.Initialize the code generator.` |
|        - | 3262 | ` */` |
|     4670 | 3263 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|        - | 3264 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 3265 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|        - | 3266 | `	void *pErrData     /* Last argument to xErr() */` |
|        - | 3267 | `	)` |
|        5 | 3268 | `{` |
|     4675 | 3269 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|        - | 3270 | `	/* Zero the structure */` |
|     4675 | 3271 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|        - | 3272 | `	/* Initial state */` |
|     4675 | 3273 | `	pGen->pVm  = &(*pVm);` |
|     4675 | 3274 | `	pGen->xErr = xErr;` |
|     4675 | 3275 | `	pGen->pErrData = pErrData;` |
|     4675 | 3276 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|     4675 | 3277 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|     4675 | 3278 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|     4675 | 3279 | `	SySetInit(&pGen->aScope,&pVm->sAllocator,sizeof(GenScope));` |
|     4675 | 3280 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|     4675 | 3281 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|     4675 | 3282 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|     4675 | 3283 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|     4675 | 3284 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|     4675 | 3285 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|        - | 3286 | `	/* Error log buffer */` |
|     4675 | 3287 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|        - | 3288 | `	/* General purpose working buffer */` |
|     4675 | 3289 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|        - | 3290 | `	/* Namespace state */` |
|     4675 | 3291 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|     4675 | 3292 | `	GenStateInitUseImports(&(*pGen),&(*pVm));` |
|     4675 | 3293 | `	GenStateInitSeenSymbols(&(*pGen),&(*pVm));` |
|        - | 3294 | `	/* Create the global scope */` |
|     4675 | 3295 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|        - | 3296 | `	/* Point to the global scope */` |
|     4675 | 3297 | `	pGen->pCurrent = &pGen->sGlobal;` |
|     4675 | 3298 | `	return SXRET_OK;` |
|        5 | 3299 | `}` |
|        - | 3300 | `/*` |
|        - | 3301 | ` * Utility routines. Reset the code generator to it's initial state.` |
|        - | 3302 | ` */` |
|    23640 | 3303 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|        - | 3304 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 3305 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|        - | 3306 | `	void *pErrData     /* Last argument to xErr() */` |
|        - | 3307 | `	)` |
|        5 | 3308 | `{` |
|    23645 | 3309 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|        - | 3310 | `	GenBlock *pBlock,*pParent;` |
|        - | 3311 | `	/* Reset state */` |
|    23645 | 3312 | `	SySetReset(&pGen->aLabel);` |
|    23645 | 3313 | `	SySetReset(&pGen->aGoto);` |
|    23645 | 3314 | `	SySetReset(&pGen->aNullsafeJmp);` |
|    23645 | 3315 | `	SySetReset(&pGen->aTrivia);` |
|    23645 | 3316 | `	SySetReset(&pGen->aPendingAttrs);` |
|    23645 | 3317 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|    23645 | 3318 | `	SyBlobRelease(&pGen->sErrBuf);` |
|    23645 | 3319 | `	SyBlobRelease(&pGen->sWorker);` |
|    23645 | 3320 | `	SyBlobRelease(&pGen->sNamespace);` |
|    23645 | 3321 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    23645 | 3322 | `	GenStateResetUseImports(&(*pGen),&(*pVm));` |
|        - | 3323 | `	/* A fresh compile unit has declared nothing yet. */` |
|    23645 | 3324 | `	GenStateResetSeenSymbols(&(*pGen),&(*pVm));` |
|        - | 3325 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|        - | 3326 | `	 * They intern variable names and literal strings that are referenced by` |
|        - | 3327 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|        - | 3328 | `	 * Releasing them would either leak the interned strings or require freeing` |
|        - | 3329 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|        - | 3330 | `	 * number of unique names, which is acceptable. */` |
|        - | 3331 | `	/* Point to the global scope */` |
|    23645 | 3332 | `	pBlock = pGen->pCurrent;` |
|    23645 | 3333 | `	while( pBlock->pParent != 0 ){` |
|      ! 0 | 3334 | `		pParent = pBlock->pParent;` |
|      ! 0 | 3335 | `		GenStateFreeBlock(pBlock);` |
|      ! 0 | 3336 | `		pBlock = pParent;` |
|      ! 0 | 3337 | `	}` |
|    23645 | 3338 | `	pGen->xErr = xErr;` |
|    23645 | 3339 | `	pGen->pErrData = pErrData;` |
|    23645 | 3340 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    23645 | 3341 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|    23645 | 3342 | `	pGen->pIn = pGen->pEnd = 0;` |
|    23645 | 3343 | `	pGen->nErr = 0;` |
|        - | 3344 | `	/* Clear the class-body context (a prior compile aborted mid-class-body would` |
|        - | 3345 | `	 * otherwise leave these live for the next eval/include on this VM). */` |
|    23645 | 3346 | `	pGen->pCurClass = 0;` |
|    23645 | 3347 | `	pGen->iInMemberDefault = 0;` |
|    23645 | 3348 | `	return SXRET_OK;` |
|        5 | 3349 | `}` |
|        - | 3350 | `/*` |
|        - | 3351 | ` * Save the code generator's compile-position state and hand the live generator a` |
|        - | 3352 | ` * fresh, empty one for a NESTED compilation unit.` |
|        - | 3353 | ` *` |
|        - | 3354 | ` * A require/include normally runs at execution time, when no compile is in flight,` |
|        - | 3355 | ` * so VmEvalChunk can call PH7_ResetCodeGenerator to wipe the generator. But an` |
|        - | 3356 | `` * autoload can fire in the MIDDLE of compiling a class: resolving `class Child`` |
|        - | 3357 | `` * extends Base` calls PH7_VmExtractClass, which triggers the autoloader, whose`` |
|        - | 3358 | `` * body `require`s Base's file — a nested compile while the outer one is mid-token.`` |
|        - | 3359 | ` * PH7_ResetCodeGenerator would then blow away the outer compile's cursor` |
|        - | 3360 | ` * (pIn/pEnd), current block, use-imports, labels, etc.; the outer parse resumes` |
|        - | 3361 | ` * with pIn == NULL and reports a bogus "Expected '{' after class 'Child'". (Common` |
|        - | 3362 | ` * in every real framework: Composer autoloads parent classes on demand.)` |
|        - | 3363 | ` *` |
|        - | 3364 | ` * This snapshots the position/scope fields into *pSaved (a caller-stack` |
|        - | 3365 | ` * ph7_gen_state used purely as storage) and re-initializes the live generator's` |
|        - | 3366 | ` * position containers to FRESH, empty ones WITHOUT releasing the outer's (the` |
|        - | 3367 | ` * snapshot now owns those). hLiteral/hNumLiteral/hVar are intentionally left LIVE` |
|        - | 3368 | ` * and shared — compiled bytecode interns names/literals into them and they grow` |
|        - | 3369 | ` * monotonically (exactly why PH7_ResetCodeGenerator keeps them); restoring an old` |
|        - | 3370 | ` * copy of those headers after a nested grow would use-after-free the bucket array.` |
|        - | 3371 | ` */` |
|        4 | 3372 | `PH7_PRIVATE void PH7_CompilerSaveState(ph7_vm *pVm,ph7_gen_state *pSaved,ProcConsumer xErr,void *pErrData)` |
|        1 | 3373 | `{` |
|        5 | 3374 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|        - | 3375 | `	/* Shallow-copy every field; the position containers below are then replaced` |
|        - | 3376 | `	 * with fresh ones on the live struct, so *pSaved keeps the outer's memory. */` |
|        5 | 3377 | `	*pSaved = *pGen;` |
|        5 | 3378 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|        5 | 3379 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|        5 | 3380 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|        5 | 3381 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|        5 | 3382 | `	SySetInit(&pGen->aScope,&pVm->sAllocator,sizeof(GenScope));` |
|        5 | 3383 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|        5 | 3384 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|        5 | 3385 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|        5 | 3386 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|        5 | 3387 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|        5 | 3388 | `	GenStateInitUseImports(&(*pGen),&(*pVm));` |
|        5 | 3389 | `	GenStateInitSeenSymbols(&(*pGen),&(*pVm));` |
|        - | 3390 | `	/* Fresh global scope for the nested unit (address of the embedded sGlobal is` |
|        - | 3391 | `	 * stable, so any outer block still parented to it stays valid across restore). */` |
|        5 | 3392 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(pVm),0);` |
|        5 | 3393 | `	pGen->pCurrent = &pGen->sGlobal;` |
|        5 | 3394 | `	pGen->pIn = pGen->pEnd = 0;` |
|        5 | 3395 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|        5 | 3396 | `	pGen->pTokenSet = 0;` |
|        5 | 3397 | `	pGen->nErr = 0;` |
|        5 | 3398 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|        5 | 3399 | `	pGen->nCommaExprOk = 0;` |
|        5 | 3400 | `	pGen->zClauseCloser = 0;` |
|        5 | 3401 | `	pGen->bInGenerator = 0;` |
|        5 | 3402 | `	pGen->bStrictTypes = 0;` |
|        5 | 3403 | `	pGen->bStrictTypesLocked = 0;` |
|        - | 3404 | `	/* The nested unit is a fresh top-level compile: it is not lexically inside the` |
|        - | 3405 | `	 * outer's class body nor its member default, so a __TRAIT__ in the nested file` |
|        - | 3406 | `	 * must not inherit the outer's trait. (Restore below carries the outer's values` |
|        - | 3407 | `	 * back, so only the nested unit sees these zeros.) */` |
|        5 | 3408 | `	pGen->pCurClass = 0;` |
|        5 | 3409 | `	pGen->iInMemberDefault = 0;` |
|        5 | 3410 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|        5 | 3411 | `	pGen->xErr = xErr;` |
|        5 | 3412 | `	pGen->pErrData = pErrData;` |
|        5 | 3413 | `}` |
|        - | 3414 | `/*` |
|        - | 3415 | ` * Restore the outer compile-position state saved by PH7_CompilerSaveState,` |
|        - | 3416 | ` * releasing the nested unit's position containers first. The shared` |
|        - | 3417 | ` * hLiteral/hNumLiteral/hVar tables (grown by the nested compile) are carried` |
|        - | 3418 | ` * forward, NOT rolled back to the snapshot's stale headers.` |
|        - | 3419 | ` */` |
|        4 | 3420 | `PH7_PRIVATE void PH7_CompilerRestoreState(ph7_vm *pVm,ph7_gen_state *pSaved)` |
|        1 | 3421 | `{` |
|        5 | 3422 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|        - | 3423 | `	GenBlock *pBlock,*pParent;` |
|        - | 3424 | `	SyHash hVar,hLiteral,hNumLiteral;` |
|        - | 3425 | `	/* Free any nested blocks left open (e.g. an aborted nested compile), then the` |
|        - | 3426 | `	 * nested global block's own fixup sets. */` |
|        5 | 3427 | `	pBlock = pGen->pCurrent;` |
|        5 | 3428 | `	while( pBlock && pBlock->pParent != 0 ){` |
|      ! 0 | 3429 | `		pParent = pBlock->pParent;` |
|      ! 0 | 3430 | `		GenStateFreeBlock(pBlock);` |
|      ! 0 | 3431 | `		pBlock = pParent;` |
|      ! 0 | 3432 | `	}` |
|        5 | 3433 | `	GenStateReleaseBlock(&pGen->sGlobal);` |
|        - | 3434 | `	/* Release the nested unit's position containers. */` |
|        5 | 3435 | `	SySetRelease(&pGen->aLabel);` |
|        5 | 3436 | `	SySetRelease(&pGen->aGoto);` |
|        5 | 3437 | `	SySetRelease(&pGen->aNullsafeJmp);` |
|        5 | 3438 | `	SySetRelease(&pGen->aLoopParent);` |
|        5 | 3439 | `	SySetRelease(&pGen->aScope);` |
|        5 | 3440 | `	SySetRelease(&pGen->aTrivia);` |
|        5 | 3441 | `	SySetRelease(&pGen->aPendingAttrs);` |
|        5 | 3442 | `	SyBlobRelease(&pGen->sWorker);` |
|        5 | 3443 | `	SyBlobRelease(&pGen->sErrBuf);` |
|        5 | 3444 | `	SyBlobRelease(&pGen->sNamespace);` |
|        5 | 3445 | `	SyHashRelease(&pGen->hUseImports);` |
|        5 | 3446 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|        5 | 3447 | `	SyHashRelease(&pGen->hUseConstImports);` |
|        5 | 3448 | `	GenStateReleaseSeenSymbols(&(*pGen));` |
|        - | 3449 | `	/* Preserve the (possibly grown) shared intern tables across the restore. */` |
|        5 | 3450 | `	hVar = pGen->hVar;` |
|        5 | 3451 | `	hLiteral = pGen->hLiteral;` |
|        5 | 3452 | `	hNumLiteral = pGen->hNumLiteral;` |
|        5 | 3453 | `	*pGen = *pSaved;` |
|        5 | 3454 | `	pGen->hVar = hVar;` |
|        5 | 3455 | `	pGen->hLiteral = hLiteral;` |
|        5 | 3456 | `	pGen->hNumLiteral = hNumLiteral;` |
|        5 | 3457 | `}` |
|        - | 3458 | `/*` |
|        - | 3459 | ` * Raise php's parse error for an unexpected token: E_PARSE with the exact text` |
|        - | 3460 | ` * php's parser prints, e.g.` |
|        - | 3461 | ` *` |
|        - | 3462 | ` *   syntax error, unexpected token ";", expecting "{"` |
|        - | 3463 | ` *   syntax error, unexpected identifier "invalid", expecting "("` |
|        - | 3464 | ` *   syntax error, unexpected end of file` |
|        - | 3465 | ` *` |
|        - | 3466 | ` * php names the token by CLASS, not just by text: an identifier, a variable and` |
|        - | 3467 | ` * a number each get their own noun, while everything else (keywords, operators,` |
|        - | 3468 | ` * punctuation) is a "token". zExpecting is the optional ", expecting ..." tail` |
|        - | 3469 | ` * — pass NULL when the site cannot say what it wanted (php often can't either).` |
|        - | 3470 | ` * pTok == NULL means the input ran out: "unexpected end of file".` |
|        - | 3471 | ` *` |
|        - | 3472 | ` * PHL's hand-written recursive-descent parser has no bison expectation sets, so` |
|        - | 3473 | ` * a site can only claim an "expecting" clause it genuinely knows; every clause` |
|        - | 3474 | ` * emitted here was verified against php 8.5.7 for the construct in question.` |
|        - | 3475 | ` */` |
|      212 | 3476 | `PH7_PRIVATE sxi32 PH7_GenSyntaxError(` |
|        - | 3477 | `	ph7_gen_state *pGen,   /* Code generator state */` |
|        - | 3478 | `	SyToken *pTok,         /* Offending token, or NULL for end of file */` |
|        - | 3479 | `	const char *zExpecting /* ", expecting <this>" tail, or NULL */` |
|        - | 3480 | `	)` |
|        5 | 3481 | `{` |
|        - | 3482 | ``	/* php's noun for the offending token. An ALPHA-stream operator (`and`, `or`,`` |
|        - | 3483 | ``	 * `xor`, `new`, `clone`, `instanceof`) is lexed ID\|OP here but php calls it a`` |
|        - | 3484 | `	 * TOKEN, like every other reserved word — only a real identifier gets the` |
|        - | 3485 | `	 * "identifier" noun. */` |
|      217 | 3486 | `	const char *zNoun = "token";` |
|        - | 3487 | `	sxu32 nLine;` |
|      217 | 3488 | `	if( pTok == 0 && pGen->pTokenSet ){` |
|        - | 3489 | `		/* The caller ran out of tokens inside its own slice — but a statement's slice stops` |
|        - | 3490 | `		 * BEFORE its terminator, so the token php actually names (typically the ';') is the` |
|        - | 3491 | `		 * one sitting just past the slice, still inside the chunk's token stream. Reach for` |
|        - | 3492 | `		 * it before concluding "end of file". */` |
|       99 | 3493 | `		SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|       99 | 3494 | `		SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|       99 | 3495 | `		if( pGen->pEnd >= pBase && pGen->pEnd < pStreamEnd ){` |
|       92 | 3496 | `			pTok = pGen->pEnd;` |
|       44 | 3497 | `		}` |
|       47 | 3498 | `	}` |
|      217 | 3499 | `	nLine = pTok ? pTok->nLine : (pGen->pIn > (SyToken *)SySetBasePtr(pGen->pTokenSet) ? pGen->pIn[-1].nLine : 1);` |
|      217 | 3500 | `	if( pTok == 0 ){` |
|       12 | 3501 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        3 | 3502 | `			zExpecting ? "syntax error, unexpected end of file, expecting %s"` |
|        - | 3503 | `			           : "syntax error, unexpected end of file",` |
|        3 | 3504 | `			zExpecting);` |
|        - | 3505 | `	}` |
|      211 | 3506 | `	if( (pTok->nType & PH7_TK_ID) && (pTok->nType & PH7_TK_OP) == 0 ){` |
|       22 | 3507 | `		zNoun = "identifier";` |
|      202 | 3508 | `	}else if( pTok->nType & PH7_TK_DOLLAR ){` |
|        9 | 3509 | `		zNoun = "variable";` |
|        - | 3510 | `		/* The '$' is its own token and carries only "$" as text; the NAME is the` |
|        - | 3511 | ``		 * token after it. php names the whole variable, so `$x` was being reported`` |
|        - | 3512 | ``		 * as the nameless `variable "$"`. Stitch the two back together. */`` |
|        9 | 3513 | `		if( pGen->pTokenSet ){` |
|        9 | 3514 | `			SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        9 | 3515 | `			SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|        9 | 3516 | `			SyToken *pName = &pTok[1];` |
|        6 | 3517 | `			if( pTok >= pBase && pName < pStreamEnd` |
|        6 | 3518 | `				&& (pName->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|        9 | 3519 | `				&& pName->sData.nByte > 0 ){` |
|        9 | 3520 | `				SyBlobReset(&pGen->sWorker);` |
|        9 | 3521 | `				SyBlobAppend(&pGen->sWorker,"$",sizeof(char));` |
|        9 | 3522 | `				SyBlobAppend(&pGen->sWorker,pName->sData.zString,pName->sData.nByte);` |
|        - | 3523 | `				{` |
|        - | 3524 | `					SyString sVar;` |
|        9 | 3525 | `					SyStringInitFromBuf(&sVar,SyBlobData(&pGen->sWorker),` |
|        - | 3526 | `						SyBlobLength(&pGen->sWorker));` |
|        9 | 3527 | `					if( zExpecting ){` |
|       12 | 3528 | `						return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        - | 3529 | `							"syntax error, unexpected %s \"%z\", expecting %s",` |
|        3 | 3530 | `							zNoun,&sVar,zExpecting);` |
|        - | 3531 | `					}` |
|      ! 0 | 3532 | `					return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|      ! 0 | 3533 | `						"syntax error, unexpected %s \"%z\"",zNoun,&sVar);` |
|        - | 3534 | `				}` |
|        - | 3535 | `			}` |
|      ! 0 | 3536 | `		}` |
|      187 | 3537 | `	}else if( pTok->nType & PH7_TK_INTEGER ){` |
|       28 | 3538 | `		zNoun = "integer";` |
|      175 | 3539 | `	}else if( pTok->nType & PH7_TK_REAL ){` |
|      ! 0 | 3540 | `		zNoun = "float";` |
|      ! 0 | 3541 | `	}` |
|      205 | 3542 | `	if( zExpecting ){` |
|      146 | 3543 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|       47 | 3544 | `			"syntax error, unexpected %s \"%z\", expecting %s",zNoun,&pTok->sData,zExpecting);` |
|        - | 3545 | `	}` |
|      164 | 3546 | `	return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|       53 | 3547 | `		"syntax error, unexpected %s \"%z\"",zNoun,&pTok->sData);` |
|      111 | 3548 | `}` |
|        - | 3549 | `/*` |
|        - | 3550 | ` * Generate a compile-time error message.` |
|        - | 3551 | ` * If the error count limit is reached (usually 15 error message)` |
|        - | 3552 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|        - | 3553 | ` * abort compilation immediately.` |
|        - | 3554 | ` */` |
|      874 | 3555 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|        5 | 3556 | `{` |
|      879 | 3557 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|      879 | 3558 | `	const char *zErr = "Error";` |
|        - | 3559 | `	SyString *pFile;` |
|        - | 3560 | `	va_list ap;` |
|        - | 3561 | `	sxi32 rc;` |
|        - | 3562 | `	/* Reset the working buffer */` |
|      879 | 3563 | `	SyBlobReset(pWorker);` |
|        - | 3564 | `	/* Peek the processed file path if available */` |
|      879 | 3565 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|      879 | 3566 | `	if( nErrType == E_ERROR \|\| nErrType == E_PARSE ){` |
|        - | 3567 | `		/* Increment the error counter. A PARSE error is every bit as fatal as an` |
|        - | 3568 | `		 * E_ERROR one: php compiles nothing, runs nothing and exits 255. Counting` |
|        - | 3569 | `		 * only E_ERROR let a parse error print its diagnostic and then fall through` |
|        - | 3570 | `		 * into execution with a 0 exit status. */` |
|      837 | 3571 | `		pGen->nErr++;` |
|      837 | 3572 | `		if( pGen->nErr > 15 ){` |
|        - | 3573 | `			/* Error count limit reached */` |
|        6 | 3574 | `			if( pGen->xErr ){` |
|        6 | 3575 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|        6 | 3576 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|        6 | 3577 | `				if( pFile ){` |
|        6 | 3578 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|        2 | 3579 | `				}` |
|        6 | 3580 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|        6 | 3581 | `				if( SyBlobLength(pWorker) > 0 ){` |
|        6 | 3582 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|        2 | 3583 | `				}` |
|        2 | 3584 | `			}` |
|        - | 3585 | `			/* Abort immediately */` |
|        6 | 3586 | `			return SXERR_ABORT;` |
|        - | 3587 | `		}` |
|      414 | 3588 | `	}` |
|      875 | 3589 | `	if( pGen->xErr == 0 ){` |
|        - | 3590 | `		/* No consumer — but keep the BARE message in the error buffer so a caller` |
|        - | 3591 | `		 * that needs the text can read it back. eval() compiles with logging off` |
|        - | 3592 | `		 * (a parse error there is php's catchable ParseError, not a printed` |
|        - | 3593 | `		 * diagnostic) and needs exactly this string for the exception message. */` |
|       41 | 3594 | `		va_start(ap,zFormat);` |
|       41 | 3595 | `		SyBlobFormatAp(pWorker,zFormat,ap);` |
|       41 | 3596 | `		va_end(ap);` |
|       41 | 3597 | `		return SXRET_OK;` |
|        - | 3598 | `	}` |
|      835 | 3599 | `	switch(nErrType){` |
|      424 | 3600 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|       47 | 3601 | `	case E_WARNING: zErr = "Warning";     break;` |
|      372 | 3602 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|      ! 0 | 3603 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|      ! 0 | 3604 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|      ! 0 | 3605 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|      ! 0 | 3606 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|      ! 0 | 3607 | `	case 8192 /* E_DEPRECATED */: zErr = "Deprecated"; break;` |
|      ! 0 | 3608 | `	default:` |
|      ! 0 | 3609 | `		break;` |
|        - | 3610 | `	}` |
|      835 | 3611 | `	rc = SXRET_OK;` |
|        - | 3612 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|      835 | 3613 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|      835 | 3614 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|      835 | 3615 | `	va_start(ap,zFormat);` |
|      835 | 3616 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|      835 | 3617 | `	va_end(ap);` |
|      835 | 3618 | `	if( pFile ){` |
|      835 | 3619 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|      415 | 3620 | `	}` |
|        - | 3621 | `	/* Append a new line */` |
|      835 | 3622 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|      835 | 3623 | `	if( SyBlobLength(pWorker) > 0 ){` |
|        - | 3624 | `		/* Consume the generated error message */` |
|      835 | 3625 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|      415 | 3626 | `	}` |
|      835 | 3627 | `	return rc;` |
|      442 | 3628 | `}` |
|        - | 3629 |  |
