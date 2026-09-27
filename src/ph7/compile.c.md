# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1869/2009 lines (93.03%)

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
|    42378 |   55 | `PH7_PRIVATE GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|        5 |   56 | `{` |
|    42383 |   57 | `	GenBlock *pBlock = pCurrent;` |
|    95053 |   58 | `	for(;;){` |
|   190111 |   59 | `		if( pBlock->iFlags & iBlockType ){` |
|    42365 |   60 | `			iCount--; /* Decrement nesting level */` |
|    42365 |   61 | `			if( iCount < 1 ){` |
|        - |   62 | `				/* Block meet with the desired criteria */` |
|    42333 |   63 | `				return pBlock;` |
|        - |   64 | `			}` |
|       16 |   65 | `		}` |
|        - |   66 | `		/* Point to the upper block */` |
|   147783 |   67 | `		pBlock = pBlock->pParent;` |
|   147783 |   68 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|        - |   69 | `			/* Forbidden */` |
|       28 |   70 | `			break;` |
|        - |   71 | `		}` |
|        5 |   72 | `	}` |
|        - |   73 | `	/* No such block */` |
|       53 |   74 | `	return 0;` |
|    21194 |   75 | `}` |
|        - |   76 | `/*` |
|        - |   77 | ` * Initialize a freshly allocated block instance.` |
|        - |   78 | ` */` |
|  1317856 |   79 | `static void GenStateInitBlock(` |
|        - |   80 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |   81 | `	GenBlock *pBlock,    /* Target block */` |
|        - |   82 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|        - |   83 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|        - |   84 | `	void *pUserData      /* Upper layer private data */` |
|        - |   85 | `	)` |
|        5 |   86 | `{` |
|        - |   87 | `	/* Initialize block fields */` |
|  1317861 |   88 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  1317861 |   89 | `	pBlock->pUserData   = pUserData;` |
|  1317861 |   90 | `	pBlock->pGen        = pGen;` |
|  1317861 |   91 | `	pBlock->iFlags      = iType;` |
|  1317861 |   92 | `	pBlock->pParent     = 0;` |
|  1317861 |   93 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  1317861 |   94 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  1317861 |   95 | `}` |
|        - |   96 | `/*` |
|        - |   97 | ` * Allocate a new block instance.` |
|        - |   98 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|        - |   99 | ` * on success.Otherwise generate a compile-time error and abort` |
|        - |  100 | ` * processing on failure.` |
|        - |  101 | ` */` |
|  1312598 |  102 | `PH7_PRIVATE sxi32 GenStateEnterBlock(` |
|        - |  103 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - |  104 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|        - |  105 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|        - |  106 | `	void *pUserData,      /* Upper layer private data */` |
|        - |  107 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|        - |  108 | `	)` |
|        5 |  109 | `{` |
|        - |  110 | `	GenBlock *pBlock;` |
|        - |  111 | `	/* Allocate a new block instance */` |
|  1312603 |  112 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  1312603 |  113 | `	if( pBlock == 0 ){` |
|        - |  114 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  115 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  116 | `		 */` |
|      ! 0 |  117 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|        - |  118 | `		/* Abort processing immediately */` |
|      ! 0 |  119 | `		return SXERR_ABORT;` |
|        - |  120 | `	}` |
|        - |  121 | `	/* Zero the structure */` |
|  1312603 |  122 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  1312603 |  123 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|        - |  124 | `	/* Link to the parent block */` |
|  1312603 |  125 | `	pBlock->pParent = pGen->pCurrent;` |
|        - |  126 | `	/* A loop or switch gets an id, and remembers the loop it nests inside, so a goto's` |
|        - |  127 | `	 * and a label's positions can be compared after compilation (see aLoopParent). */` |
|  1312603 |  128 | `	if( iType & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|   108045 |  129 | `		sxu32 nParent = pGen->nCurLoopId;` |
|   108045 |  130 | `		pGen->nLoopId++;` |
|   108045 |  131 | `		SySetPut(&pGen->aLoopParent,(const void *)&nParent);` |
|   108045 |  132 | `		pBlock->nLoopId = pGen->nLoopId;` |
|   108045 |  133 | `		pBlock->nOuterLoopId = nParent;` |
|   108045 |  134 | `		pGen->nCurLoopId = pGen->nLoopId;` |
|    54020 |  135 | `	}` |
|        - |  136 | `	/* A try/catch/finally block gets a scope id, and remembers the scope it nests inside,` |
|        - |  137 | `	 * so the chain between any two points can be walked after compilation (aScope). Every` |
|        - |  138 | `	 * other block simply inherits the scope in effect. */` |
|  1312603 |  139 | `	pBlock->nOuterScopeId = pGen->nCurScopeId;` |
|  1312603 |  140 | `	pBlock->nScopeId = pGen->nCurScopeId;` |
|  1312603 |  141 | `	if( iType & GEN_BLOCK_EXCEPTION ){` |
|        - |  142 | `		GenScope sScope;` |
|     7987 |  143 | `		sScope.nParent = pGen->nCurScopeId;` |
|     7987 |  144 | `		sScope.pUserData = pUserData;` |
|     7987 |  145 | `		if( iType & GEN_BLOCK_FINALLY ){` |
|      299 |  146 | `			sScope.iKind = GEN_SCOPE_FINALLY;` |
|     7840 |  147 | `		}else if( iType & GEN_BLOCK_DETACHED ){` |
|     3681 |  148 | `			sScope.iKind = GEN_SCOPE_DETACHED;` |
|     1843 |  149 | `		}else{` |
|        - |  150 | `			/* A try. pUserData is its ph7_exception, which every try-block site passes at` |
|        - |  151 | `			 * ENTRY precisely so this can classify it. */` |
|     4017 |  152 | `			sScope.iKind = GenStateInlineTryCatch(pGen) ? GEN_SCOPE_TRY_INLINE : GEN_SCOPE_TRY;` |
|        - |  153 | `		}` |
|     7987 |  154 | `		if( SySetPut(&pGen->aScope,(const void *)&sScope) == SXRET_OK ){` |
|     7987 |  155 | `			pBlock->nScopeId = SySetUsed(&pGen->aScope);` |
|     7987 |  156 | `			pGen->nCurScopeId = pBlock->nScopeId;` |
|     3991 |  157 | `		}` |
|     3991 |  158 | `	}` |
|        - |  159 | `	/* Mark as the current block */` |
|  1312603 |  160 | `	pGen->pCurrent = pBlock;` |
|  1312603 |  161 | `	if( ppBlock ){` |
|        - |  162 | `		/* Write a pointer to the new instance */` |
|   622309 |  163 | `		*ppBlock = pBlock;` |
|   311152 |  164 | `	}` |
|  1312603 |  165 | `	return SXRET_OK;` |
|   656304 |  166 | `}` |
|        - |  167 | `/*` |
|        - |  168 | ` * Release block fields without freeing the whole instance.` |
|        - |  169 | ` */` |
|  1312592 |  170 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|        5 |  171 | `{` |
|  1312597 |  172 | `	SySetRelease(&pBlock->aPostContFix);` |
|  1312597 |  173 | `	SySetRelease(&pBlock->aJumpFix);` |
|  1312597 |  174 | `}` |
|        - |  175 | `/*` |
|        - |  176 | ` * Release a block.` |
|        - |  177 | ` */` |
|  1312588 |  178 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|        5 |  179 | `{` |
|  1312593 |  180 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  1312593 |  181 | `	GenStateReleaseBlock(&(*pBlock));` |
|        - |  182 | `	/* Free the instance */` |
|  1312593 |  183 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  1312593 |  184 | `}` |
|        - |  185 | `/*` |
|        - |  186 | ` * POP and release a block from the stack of compiled blocks.` |
|        - |  187 | ` */` |
|  1312588 |  188 | `PH7_PRIVATE sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|        5 |  189 | `{` |
|  1312593 |  190 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  1312593 |  191 | `	if( pBlock == 0 ){` |
|        - |  192 | `		/* No more block to pop */` |
|      ! 0 |  193 | `		return SXERR_EMPTY;` |
|        - |  194 | `	}` |
|  1312593 |  195 | `	if( pBlock->iFlags & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|   108037 |  196 | `		pGen->nCurLoopId = pBlock->nOuterLoopId;` |
|    54016 |  197 | `	}` |
|  1312593 |  198 | `	if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|     7987 |  199 | `		pGen->nCurScopeId = pBlock->nOuterScopeId;` |
|     3991 |  200 | `	}` |
|        - |  201 | `	/* Point to the upper block */` |
|  1312593 |  202 | `	pGen->pCurrent = pBlock->pParent;` |
|  1312593 |  203 | `	if( ppBlock ){` |
|        - |  204 | `		/* Write a pointer to the popped block */` |
|      ! 0 |  205 | `		*ppBlock = pBlock;` |
|      ! 0 |  206 | `	}else{` |
|        - |  207 | `		/* Safely release the block */` |
|  1312593 |  208 | `		GenStateFreeBlock(&(*pBlock));` |
|        - |  209 | `	}` |
|  1312593 |  210 | `	return SXRET_OK;` |
|   656299 |  211 | `}` |
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
|   153790 |  230 | `PH7_PRIVATE int GenStateUnconditionalTopLevel(ph7_gen_state *pGen)` |
|        5 |  231 | `{` |
|   153795 |  232 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   153995 |  233 | `	while( pBlock ){` |
|   153995 |  234 | `		if( pBlock->iFlags & (GEN_BLOCK_COND\|GEN_BLOCK_LOOP\|GEN_BLOCK_FUNC\|GEN_BLOCK_SWITCH\|GEN_BLOCK_EXCEPTION) ){` |
|      161 |  235 | `			return 0; /* conditional / nested */` |
|        - |  236 | `		}` |
|   153839 |  237 | `		if( pBlock->iFlags & GEN_BLOCK_GLOBAL ){` |
|   153639 |  238 | `			return 1; /* reached the global block with no conditional ancestor */` |
|        - |  239 | `		}` |
|      205 |  240 | `		pBlock = pBlock->pParent;` |
|        5 |  241 | `	}` |
|      ! 0 |  242 | `	return 1;` |
|    76900 |  243 | `}` |
|        - |  244 | `/*` |
|        - |  245 | ` * Guard a top-level function about to be installed. Same contract as the class` |
|        - |  246 | ` * guard above.` |
|        - |  247 | ` */` |
|   149882 |  248 | `PH7_PRIVATE sxi32 GenStateGuardFuncRedeclaration(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|        5 |  249 | `{` |
|        - |  250 | `	SyHashEntry *pEntry;` |
|   149887 |  251 | `	if( !GenStateUnconditionalTopLevel(pGen) ){` |
|      104 |  252 | `		return SXRET_OK;` |
|        - |  253 | `	}` |
|   149787 |  254 | `	pFunc->iFlags \|= VM_FUNC_BOUND;` |
|   149787 |  255 | `	if( pGen->pVm->bCompilingBuiltin ){` |
|   147117 |  256 | `		return SXRET_OK;` |
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
|     2675 |  274 | `	if( SyHashGet(&pGen->pVm->hHostFunction,(const void *)pFunc->sName.zString,pFunc->sName.nByte) ){` |
|        8 |  275 | `		PH7_GenCompileError(pGen,E_ERROR,pFunc->nLine,` |
|        2 |  276 | `			"Cannot redeclare function %z()",&pFunc->sName);` |
|        6 |  277 | `		return SXERR_ABORT;` |
|        - |  278 | `	}` |
|     2671 |  279 | `	pEntry = SyHashGet(&pGen->pVm->hFunction,(const void *)pFunc->sName.zString,pFunc->sName.nByte);` |
|     2671 |  280 | `	if( pEntry ){` |
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
|     2667 |  297 | `	return SXRET_OK;` |
|    74946 |  298 | `}` |
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
|   711670 |  309 | `PH7_PRIVATE sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|        5 |  310 | `{` |
|        - |  311 | `	JumpFixup sJumpFix;` |
|        - |  312 | `	sxi32 rc;` |
|        - |  313 | `	/* Init the JumpFixup structure */` |
|   711675 |  314 | `	sJumpFix.nJumpType = nJumpType;` |
|   711675 |  315 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|        - |  316 | `	/* Remember which bytecode array the emitted instruction lives in: the block whose` |
|        - |  317 | `	 * table this lands in may be resolved after a container swap (see JumpFixup). */` |
|   711675 |  318 | `	sJumpFix.pContainer = PH7_VmGetByteCodeContainer(pBlock->pGen->pVm);` |
|        - |  319 | `	/* Insert in the jump fixup table */` |
|   711675 |  320 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|   711675 |  321 | `	return rc;` |
|        5 |  322 | `}` |
|        - |  323 | `/*` |
|        - |  324 | ` * TRUE when the body being compiled has its try/catch/finally compiled INLINE` |
|        - |  325 | ` * (ROOT C, generator bodies) rather than into detached mini-programs.` |
|        - |  326 | ` */` |
|     4012 |  327 | `PH7_PRIVATE int GenStateInlineTryCatch(ph7_gen_state *pGen)` |
|        5 |  328 | `{` |
|     4017 |  329 | `	return pGen->bInGenerator && pGen->pVm->bInlineTryCatch;` |
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
|    42482 |  356 | `PH7_PRIVATE int GenStateJumpScope(ph7_gen_state *pGen,sxu32 nFrom,sxu32 nTo,int bEmitPops,` |
|        - |  357 | `	GenJumpScope *pScope)` |
|        5 |  358 | `{` |
|    42487 |  359 | `	GenScope *aScope = (GenScope *)SySetBasePtr(&pGen->aScope);` |
|    42487 |  360 | `	sxu32 nUsed = SySetUsed(&pGen->aScope);` |
|    42487 |  361 | `	sxu32 nCur = nFrom;` |
|    42487 |  362 | `	SyZero(pScope,sizeof(*pScope));` |
|    42601 |  363 | `	while( nCur != nTo ){` |
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
|       73 |  385 | `		}else if( pScopeEnt->iKind == GEN_SCOPE_TRY_INLINE ){` |
|       11 |  386 | `			pScope->nInline++;` |
|       34 |  387 | `		}else if( pScope->nDet == 0 && bEmitPops ){` |
|        3 |  388 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pScopeEnt->pUserData,0);` |
|        2 |  389 | `		}else{` |
|       27 |  390 | `			pScope->nTry++;` |
|        - |  391 | `		}` |
|      119 |  392 | `		nCur = pScopeEnt->nParent;` |
|        5 |  393 | `	}` |
|    42483 |  394 | `	return TRUE;` |
|    21246 |  395 | `}` |
|        - |  396 | `/*` |
|        - |  397 | ` * Pick the jump opcode for a crossing described by GenStateJumpScope, and its iP1.` |
|        - |  398 | ` * Shared by break/continue (which know their target at emit time) and goto (which` |
|        - |  399 | ` * settles this in GenStateFixGoto, once the label fixes the counts).` |
|        - |  400 | ` */` |
|    42370 |  401 | `PH7_PRIVATE sxi32 GenStateScopeJumpOp(const GenJumpScope *pCross,sxi32 *piP1)` |
|        5 |  402 | `{` |
|    42375 |  403 | `	if( pCross->nDet > 0 \|\| pCross->nTry > 0 ){` |
|       84 |  404 | `		*piP1 = PH7_CATCH_JMP_P1(pCross->nDet,pCross->nTry);` |
|       84 |  405 | `		return PH7_OP_CATCH_JMP;` |
|        - |  406 | `	}` |
|    42295 |  407 | `	if( pCross->nInline > 0 ){` |
|       11 |  408 | `		*piP1 = (sxi32)pCross->nInline;` |
|       11 |  409 | `		return PH7_OP_SET_FINALLY_JMP;` |
|        - |  410 | `	}` |
|    42287 |  411 | `	*piP1 = 0;` |
|    42287 |  412 | `	return PH7_OP_JMP;` |
|    21190 |  413 | `}` |
|        - |  414 | `/*` |
|        - |  415 | ` * Resolve a recorded fixup to its VM instruction, in the container it was emitted` |
|        - |  416 | ` * into (see JumpFixup.pContainer) rather than whichever one is current now.` |
|        - |  417 | ` */` |
|   727600 |  418 | `PH7_PRIVATE VmInstr * GenStateFixupInstr(const JumpFixup *pFix)` |
|        5 |  419 | `{` |
|   727605 |  420 | `	return (VmInstr *)SySetAt(pFix->pContainer,pFix->nInstrIdx);` |
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
|  1025604 |  433 | `PH7_PRIVATE sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|        5 |  434 | `{` |
|        - |  435 | `	JumpFixup *aFix;` |
|        - |  436 | `	VmInstr *pInstr;` |
|        - |  437 | `	sxu32 nFixed;` |
|        - |  438 | `	sxu32 n;` |
|        - |  439 | `	/* Point to the jump fixup table */` |
|  1025609 |  440 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|        - |  441 | `	/* Fix the desired jumps */` |
|  2315105 |  442 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  1289501 |  443 | `		if( aFix[n].nJumpType < 0 ){` |
|        - |  444 | `			/* Already fixed */` |
|   450785 |  445 | `			continue;` |
|        - |  446 | `		}` |
|   838721 |  447 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|        - |  448 | `			/* Not of our interest */` |
|   127053 |  449 | `			continue;` |
|        - |  450 | `		}` |
|        - |  451 | `		/* Point to the instruction to fix */` |
|   711673 |  452 | `		pInstr = GenStateFixupInstr(&aFix[n]);` |
|   711673 |  453 | `		if( pInstr ){` |
|   711673 |  454 | `			pInstr->iP2 = nJumpDest;` |
|   711673 |  455 | `			nFixed++;` |
|        - |  456 | `			/* Mark as fixed */` |
|   711673 |  457 | `			aFix[n].nJumpType = -1;` |
|   355834 |  458 | `		}` |
|   355839 |  459 | `	}` |
|        - |  460 | `	/* Total number of fixed jumps */` |
|  1025609 |  461 | `	return nFixed;` |
|        5 |  462 | `}` |
|        - |  463 | `/*` |
|        - |  464 | ` * Fix a 'goto' now the jump destination is resolved.` |
|        - |  465 | ` * The goto statement can be used to jump to another section` |
|        - |  466 | ` * in the program.` |
|        - |  467 | ` * Refer to the routine responsible of compiling the goto` |
|        - |  468 | ` * statement for more information.` |
|        - |  469 | ` */` |
|   174628 |  470 | `PH7_PRIVATE sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|        5 |  471 | `{` |
|        - |  472 | `	JumpFixup *pJump,*aJumps;` |
|        - |  473 | `	GenJumpScope sCross;` |
|        - |  474 | `	Label *pLabel;` |
|        - |  475 | `	VmInstr *pInstr;` |
|        - |  476 | `	sxi32 rc;` |
|        - |  477 | `	sxu32 n;` |
|        - |  478 | `	/* Point to the goto table */` |
|   174633 |  479 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|        - |  480 | `	/* Fix */` |
|   174851 |  481 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|       46 |  559 | `				sxi32 iP1 = 0;` |
|       46 |  560 | `				pInstr->iOp = (sxu8)GenStateScopeJumpOp(&sCross,&iP1);` |
|       46 |  561 | `				pInstr->iP1 = iP1;` |
|       22 |  562 | `			}` |
|       75 |  563 | `		}` |
|       80 |  564 | `	}` |
|        - |  565 | `	/* php says nothing about a label nobody jumps to — the old "defined but not` |
|        - |  566 | `	 * referenced" warning was a PH7-ism with no counterpart in the oracle. */` |
|   174631 |  567 | `	return SXRET_OK;` |
|    87319 |  568 | `}` |
|        - |  569 | `/*` |
|        - |  570 | ` * Check if a given token value is installed in the literal table.` |
|        - |  571 | ` */` |
|  1220376 |  572 | `PH7_PRIVATE sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|        5 |  573 | `{` |
|        - |  574 | `	SyHashEntry *pEntry;` |
|  1220381 |  575 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  1220381 |  576 | `	if( pEntry == 0 ){` |
|   595353 |  577 | `		return SXERR_NOTFOUND;` |
|        - |  578 | `	}` |
|   625033 |  579 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|   625033 |  580 | `	return SXRET_OK;` |
|   610193 |  581 | `}` |
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
|   595348 |  592 | `PH7_PRIVATE sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|        5 |  593 | `{` |
|   595353 |  594 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|   595353 |  595 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   297674 |  596 | `	}` |
|   595353 |  597 | `	return SXRET_OK;` |
|        5 |  598 | `}` |
|        - |  599 | `/*` |
|        - |  600 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|        - |  601 | ` * in the constant table.` |
|        - |  602 | ` */` |
|   697792 |  603 | `PH7_PRIVATE ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|        5 |  604 | `{` |
|        - |  605 | `	ph7_value *pObj;` |
|   697797 |  606 | `	sxu32 nIdx = 0; /* cc warning */` |
|        - |  607 | `	/* Reserve a new constant */` |
|   697797 |  608 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   697797 |  609 | `	if( pObj == 0 ){` |
|      ! 0 |  610 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  611 | `		return 0;` |
|        - |  612 | `	}` |
|   697797 |  613 | `	*pIdx = nIdx;` |
|        - |  614 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|        - |  615 | `	 * the constant string iterals table [optimization purposes].` |
|        - |  616 | `	 */` |
|   697797 |  617 | `	return pObj;` |
|   348901 |  618 | `}` |
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
|   786850 |  633 | `PH7_PRIVATE void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|        5 |  634 | `{` |
|        - |  635 | `	VmCallArgMap *pMap;` |
|   786855 |  636 | `	if( !pGen->bStrictTypes ) return p3;` |
|      318 |  637 | `	if( p3 == 0 ){` |
|       44 |  638 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|       44 |  639 | `		if( pMap == 0 ) return 0;` |
|       44 |  640 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|       44 |  641 | `		p3 = (void *)pMap;` |
|       20 |  642 | `	}` |
|      318 |  643 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|      318 |  644 | `	return p3;` |
|   393430 |  645 | `}` |
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
|       17 |  668 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|        3 |  669 | `			return TRUE;` |
|       15 |  670 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|        6 |  671 | `			return TRUE;` |
|        2 |  672 | `		}` |
|      171 |  673 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       16 |  674 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
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
|        - |  691 | `/*` |
|        - |  692 | ` * The chain operators that ACCESS a container -- the same four minus the call,` |
|        - |  693 | ` * whose result is an ordinary value however the chain around it is read. Used` |
|        - |  694 | ` * to spot an INTERMEDIATE link of an isset()/empty() chain, which php reads for` |
|        - |  695 | ` * its value rather than for a truth.` |
|        - |  696 | ` */` |
|        - |  697 | `#define GEN_IS_ACCESS_OP(iOp) \` |
|        - |  698 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|        - |  699 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT)` |
|        - |  700 |  |
|        - |  701 | `/*` |
|        - |  702 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|        - |  703 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|        - |  704 | ` * patched entries from the pending set.` |
|        - |  705 | ` */` |
|  7061546 |  706 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|        5 |  707 | `{` |
|  7061551 |  708 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|        - |  709 | `	sxu32 nTarget;` |
|        - |  710 | `	sxu32 *aIdx;` |
|        - |  711 | `	sxu32 i;` |
|  7061551 |  712 | `	if( nCur <= nBaseline ){` |
|  7061421 |  713 | `		return;` |
|        - |  714 | `	}` |
|      135 |  715 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|      135 |  716 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|      273 |  717 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|      143 |  718 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|      143 |  719 | `		if( pInstr ){` |
|      143 |  720 | `			pInstr->iP2 = (sxi32)nTarget;` |
|       69 |  721 | `		}` |
|       74 |  722 | `	}` |
|      135 |  723 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
|  3530778 |  724 | `}` |
|        - |  725 |  |
|        - |  726 | `/*` |
|        - |  727 | ` * By-reference out-parameters of builtin functions.` |
|        - |  728 | ` *` |
|        - |  729 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|        - |  730 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|        - |  731 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|        - |  732 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|        - |  733 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|        - |  734 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|        - |  735 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|        - |  736 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|        - |  737 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|        - |  738 | ` * creates it" behaviour).` |
|        - |  739 | ` *` |
|        - |  740 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|        - |  741 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|        - |  742 | ` */` |
|   699004 |  743 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|        5 |  744 | `{` |
|        - |  745 | `	static const struct {` |
|        - |  746 | `		const char *zName;` |
|        - |  747 | `		sxu32 nByte;` |
|        - |  748 | `		sxu32 mask;` |
|        - |  749 | `	} aByRef[] = {` |
|        - |  750 | `		{ "parse_str",              9, 1u<<1 },  /* &$result (apArg[1]) */` |
|        - |  751 | `		{ "settype",                7, 1u<<0 },  /* &$var    (apArg[0]) */` |
|        - |  752 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|        - |  753 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|        - |  754 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|        - |  755 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|        - |  756 | `		{ "flock",                  5, 1u<<2 },  /* &$would_block (apArg[2]) */` |
|        - |  757 | `		{ "getopt",                 6, 1u<<2 },  /* &$rest_index (apArg[2]) */` |
|        - |  758 | `		{ "is_callable",           11, 1u<<2 },  /* &$callable_name (apArg[2]) */` |
|        - |  759 | `		{ "similar_text",          12, 1u<<2 },  /* &$percent (apArg[2]) */` |
|        - |  760 | `		{ "str_replace",           11, 1u<<3 },  /* &$count  (apArg[3]) */` |
|        - |  761 | `		{ "str_ireplace",          12, 1u<<3 },  /* &$count  (apArg[3]) */` |
|        - |  762 | `		{ "fsockopen",              9, (1u<<2)\|(1u<<3) },  /* &$error_code, &$error_message */` |
|        - |  763 | `		{ "pfsockopen",            10, (1u<<2)\|(1u<<3) },  /* same */` |
|        - |  764 | `		{ "stream_socket_client",  20, (1u<<1)\|(1u<<2) },  /* &$error_code, &$error_message */` |
|        - |  765 | `		{ "stream_socket_server",  20, (1u<<1)\|(1u<<2) },  /* same pair */` |
|        - |  766 | `		{ "stream_socket_accept",  20, 1u<<2 },            /* &$peer_name (apArg[2]) */` |
|        - |  767 | `		{ "stream_select",         13, (1u<<0)\|(1u<<1)\|(1u<<2) }, /* &$read, &$write, &$except */` |
|        - |  768 | `		{ "stream_socket_recvfrom",22, 1u<<3 },            /* &$address (apArg[3]) */` |
|        - |  769 | `		{ "proc_open",              9, 1u<<2 },  /* &$pipes (apArg[2]) */` |
|        - |  770 | `		{ "exec",                   4, (1u<<1)\|(1u<<2) },  /* &$output, &$result_code */` |
|        - |  771 | `		{ "system",                 6, 1u<<1 },  /* &$result_code (apArg[1]) */` |
|        - |  772 | `		{ "passthru",               8, 1u<<1 },  /* &$result_code (apArg[1]) */` |
|        - |  773 | `	};` |
|        - |  774 | `	sxu32 i;` |
|   699009 |  775 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|    35379 |  776 | `		return 0;` |
|        - |  777 | `	}` |
| 15502025 |  778 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
| 14860820 |  779 | `		if( pName->nByte == aByRef[i].nByte` |
|  7934529 |  780 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|    22435 |  781 | `			return aByRef[i].mask;` |
|        - |  782 | `		}` |
|  7419200 |  783 | `	}` |
|   641205 |  784 | `	return 0;` |
|   349507 |  785 | `}` |
|        - |  786 | `/*` |
|        - |  787 | ` * What may be passed by REFERENCE is decided from the argument's SHAPE, at compile` |
|        - |  788 | ` * time, exactly as php decides it (zend_compile_args -> zend_is_variable).` |
|        - |  789 | ` *` |
|        - |  790 | ` * php sorts every actual argument into three buckets:` |
|        - |  791 | ` *` |
|        - |  792 | ` *   GEN_ARG_LVALUE   a variable, an element, a property, a static property. It has a` |
|        - |  793 | ` *                    slot, so a by-ref parameter aliases it.` |
|        - |  794 | `` *   GEN_ARG_TEMPCALL the result of a call or of `new`. It has no slot, but php cannot`` |
|        - |  795 | ` *                    know at compile time whether the callee returns a reference, so it` |
|        - |  796 | ` *                    defers: E_NOTICE "Only variables should be passed by reference",` |
|        - |  797 | ` *                    then it operates on the temporary.` |
|        - |  798 | ` *   GEN_ARG_NONE     everything else — a literal, an operator/cast result, a class` |
|        - |  799 | `` *                    constant, `@$x`, `$o?->p`, an assignment. Binding one to a by-ref`` |
|        - |  800 | ` *                    parameter is a catchable Error at the CALL.` |
|        - |  801 | ` *` |
|        - |  802 | ` * Deciding it from the argument's runtime memobj instead does not work and was silently` |
|        - |  803 | ` * wrong in both directions: an arithmetic or concatenation result keeps its LEFT operand's` |
|        - |  804 | `` * slot index, so `f($i + 1)` with `function f(&$x)` aliased and overwrote `$i`; and a`` |
|        - |  805 | ` * builtin's by-ref row saw only "no slot", which a call result has too.` |
|        - |  806 | ` */` |
|        - |  807 | `#define GEN_ARG_LVALUE   0` |
|        - |  808 | `#define GEN_ARG_TEMPCALL 1` |
|        - |  809 | `#define GEN_ARG_NONE     2` |
|   943680 |  810 | `static int GenStateArgShape(ph7_expr_node *pNode)` |
|        5 |  811 | `{` |
|   943685 |  812 | `	if( pNode == 0 ){` |
|      ! 0 |  813 | `		return GEN_ARG_NONE;` |
|        - |  814 | `	}` |
|   943685 |  815 | `	if( pNode->pOp == 0 ){` |
|        - |  816 | ``		/* A leaf: only the `$…` family is a variable. Everything else the parser`` |
|        - |  817 | ``		 * files here — a literal, an array/list constructor, a closure, `match`,`` |
|        - |  818 | ``		 * `clone` — is a temporary. */`` |
|   786157 |  819 | `		return pNode->xCode == PH7_CompileVariable ? GEN_ARG_LVALUE : GEN_ARG_NONE;` |
|        - |  820 | `	}` |
|   157533 |  821 | `	switch( pNode->pOp->iOp ){` |
|    12168 |  822 | `	case EXPR_OP_SUBSCRIPT: /* $a[k], and any base: php accepts g()[0] and C::m()[0] */` |
|        - |  823 | `	case EXPR_OP_ARROW:     /* $o->p */` |
|    24341 |  824 | `		return GEN_ARG_LVALUE;` |
|      297 |  825 | `	case EXPR_OP_DC:` |
|        - |  826 | ``		/* `C::$s` is a static property (an lvalue); `C::K` is a class constant and`` |
|        - |  827 | ``		 * `C::CASE` an enum case, neither of which php will bind. The right operand`` |
|        - |  828 | `		 * tells them apart. */` |
|      896 |  829 | `		return ( pNode->pRight && pNode->pRight->pOp == 0` |
|      594 |  830 | `		      && pNode->pRight->xCode == PH7_CompileVariable )` |
|      594 |  831 | `			? GEN_ARG_LVALUE : GEN_ARG_NONE;` |
|    22342 |  832 | `	case EXPR_OP_FUNC_CALL:` |
|        - |  833 | `	case EXPR_OP_NEW:` |
|    44689 |  834 | `		return GEN_ARG_TEMPCALL;` |
|    43957 |  835 | `	default:` |
|        - |  836 | ``		/* Includes `?->` (php: "Cannot use nullsafe operator in write context"),`` |
|        - |  837 | ``		 * `@$x`, `$q = …`, `clone $o` and every arithmetic/logical operator. */`` |
|    87919 |  838 | `		return GEN_ARG_NONE;` |
|        - |  839 | `	}` |
|   471845 |  840 | `}` |
|        - |  841 | `/*` |
|        - |  842 | ` * Recover the bare global-builtin name from a call's callee node.` |
|        - |  843 | ` *` |
|        - |  844 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|        - |  845 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|        - |  846 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|        - |  847 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|        - |  848 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|        - |  849 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|        - |  850 | ` */` |
|  1359966 |  851 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|        5 |  852 | `{` |
|        - |  853 | `	SyToken *p, *pEnd;` |
|  1359971 |  854 | `	pOut->zString = 0;` |
|  1359971 |  855 | `	pOut->nByte = 0;` |
|  1359971 |  856 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|      ! 0 |  857 | `		return;` |
|        - |  858 | `	}` |
|  1359971 |  859 | `	p = pLeft->pStart;` |
|  1359971 |  860 | `	pEnd = pLeft->pEnd;` |
|        - |  861 | `	/* Optional single leading namespace separator (absolute path). */` |
|  1359971 |  862 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|      101 |  863 | `		p++;` |
|       48 |  864 | `	}` |
|  1359971 |  865 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|    44281 |  866 | `		return;` |
|        - |  867 | `	}` |
|        - |  868 | `	/* Must be a single component: nothing follows the name token. */` |
|  1315695 |  869 | `	if( p + 1 != pEnd ){` |
|      129 |  870 | `		return;` |
|        - |  871 | `	}` |
|  1315571 |  872 | `	*pOut = p->sData;` |
|   679988 |  873 | `}` |
|        - |  874 | `/*` |
|        - |  875 | `` * Is this expression node the bare variable `$this`?`` |
|        - |  876 | ` */` |
|   848838 |  877 | `PH7_PRIVATE int PH7_ExprNodeIsThis(ph7_expr_node *pNode)` |
|        5 |  878 | `{` |
|        - |  879 | `	SyToken *pTok;` |
|   848843 |  880 | `	if( pNode == 0 \|\| pNode->pOp != 0 \|\| pNode->xCode != PH7_CompileVariable ){` |
|   125229 |  881 | `		return 0;` |
|        - |  882 | `	}` |
|   723619 |  883 | `	pTok = pNode->pStart;` |
|   723619 |  884 | `	if( pTok == 0 \|\| pNode->pEnd == 0 \|\| pNode->pEnd < &pTok[2] ){` |
|      ! 0 |  885 | `		return 0;` |
|        - |  886 | `	}` |
|  1085426 |  887 | `	return (pTok[0].nType & PH7_TK_DOLLAR) != 0` |
|   723614 |  888 | `		&& (pTok[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|   723603 |  889 | `		&& pTok[1].sData.nByte == sizeof("this")-1` |
|  1085421 |  890 | `		&& SyMemcmp((const void *)pTok[1].sData.zString,(const void *)"this",sizeof("this")-1) == 0;` |
|   424424 |  891 | `}` |
|        - |  892 | `/*` |
|        - |  893 | ` * The two write-target rules php decides at COMPILE time, in one place because` |
|        - |  894 | ` * every write site has to make both of them.` |
|        - |  895 | ` *` |
|        - |  896 | `` * **`$this`** is not a variable a program may re-point: php refuses the`` |
|        - |  897 | `` * assignment, the reference bind, a foreach/list target and `unset()` where they`` |
|        - |  898 | `` * are WRITTEN. PHL performed all of them, so `$this = 5;` inside a method`` |
|        - |  899 | ` * replaced the receiver with an int for the rest of the call and every later` |
|        - |  900 | `` * `$this->x` failed somewhere else entirely.`` |
|        - |  901 | ` *` |
|        - |  902 | ``  * **A temporary** cannot be written THROUGH: `(new A)->p = 1` and `"s"->p = 1` `` |
|        - |  903 | ` * modify an object/value that no longer exists after the statement, so php` |
|        - |  904 | `` * refuses the whole chain — every write kind, including `+=`, `++`, `=&` and`` |
|        - |  905 | `` * `unset()`. The base of the access chain decides: a variable and a userland`` |
|        - |  906 | `` * CALL are writable (`f()->p = 1` is php-legal), a `new`, a literal and any`` |
|        - |  907 | ` * other computed value are not, and an internal function's result gets php's own` |
|        - |  908 | `` * separate wording — which is what `(clone $o)->p = 1` is, `clone` being a`` |
|        - |  909 | ` * function in php 8.5.` |
|        - |  910 | ` */` |
|   848838 |  911 | `PH7_PRIVATE sxi32 GenStateWriteTargetCheck(ph7_gen_state *pGen,ph7_expr_node *pTarget,int bUnset)` |
|        5 |  912 | `{` |
|   848843 |  913 | `	ph7_expr_node *pBase = pTarget;` |
|   848843 |  914 | `	const char *zMsg = 0;` |
|        - |  915 | `	sxi32 rc;` |
|   848843 |  916 | `	if( pTarget == 0 ){` |
|      ! 0 |  917 | `		return SXRET_OK;` |
|        - |  918 | `	}` |
|   848843 |  919 | `	if( PH7_ExprNodeIsThis(pTarget) ){` |
|       11 |  920 | `		zMsg = bUnset ? "Cannot unset $this" : "Cannot re-assign $this";` |
|        7 |  921 | `	}else{` |
|        - |  922 | `		/* Walk to the base of the access chain; the links themselves are writable. */` |
|   974107 |  923 | `		while( pBase && pBase->pOp ){` |
|   125473 |  924 | `			if( pBase->pOp->iOp == EXPR_OP_DC ){` |
|        - |  925 | `` 				/* A `::` left operand is a CLASS reference, not a value — `C::$s = 1` `` |
|        - |  926 | ``				 * and even `(new C)::$s = 1` write class-level storage that outlives`` |
|        - |  927 | `				 * any temporary, so the chain stops being about a base here. */` |
|      165 |  928 | `				return SXRET_OK;` |
|        - |  929 | `			}` |
|   125308 |  930 | `			if( pBase->pOp->iOp != EXPR_OP_ARROW && pBase->pOp->iOp != EXPR_OP_NULLSAFE_ARROW` |
|   123264 |  931 | `			 && pBase->pOp->iOp != EXPR_OP_SUBSCRIPT ){` |
|       41 |  932 | `				break;` |
|        - |  933 | `			}` |
|   125277 |  934 | `			pBase = pBase->pLeft;` |
|        5 |  935 | `		}` |
|   848675 |  936 | `		if( pBase == 0 \|\| pBase == pTarget ){` |
|        - |  937 | `			/* No chain: a non-variable target of its own is the caller's business` |
|        - |  938 | `			 * (php reports its parse error / "Assignments can only happen to` |
|        - |  939 | `			 * writable values" there, and so does PHL). */` |
|   723845 |  940 | `			return SXRET_OK;` |
|        - |  941 | `		}` |
|   124835 |  942 | `		if( pBase->pOp == 0 ){` |
|   124809 |  943 | `			if( pBase->xCode != PH7_CompileVariable ){` |
|      ! 0 |  944 | `				zMsg = "Cannot use temporary expression in write context";` |
|        5 |  945 | `			}` |
|    62431 |  946 | `		}else if( pBase->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|        - |  947 | `			/* php: the result of an INTERNAL function is not writable through,` |
|        - |  948 | `			 * a userland one is. */` |
|        - |  949 | `			SyString sName;` |
|       23 |  950 | `			GenStateCallBuiltinName(pBase,&sName);` |
|       22 |  951 | `			if( sName.nByte > 0 && pGen->pVm` |
|        1 |  952 | `			 && SyHashGet(&pGen->pVm->hHostFunction,(const void *)sName.zString,sName.nByte) ){` |
|      ! 0 |  953 | `				zMsg = "Cannot use result of built-in function in write context";` |
|        1 |  954 | `			}` |
|       17 |  955 | `		}else if( pBase->pOp->iOp == EXPR_OP_CLONE ){` |
|        - |  956 | ``			/* php 8.5 implements `clone` AS a function, so a write through its result`` |
|        - |  957 | `			 * takes the internal-function wording rather than the temporary one. */` |
|        3 |  958 | `			zMsg = "Cannot use result of built-in function in write context";` |
|        2 |  959 | `		}else{` |
|        - |  960 | ``			/* `new`, and every other computed base. */`` |
|        3 |  961 | `			zMsg = "Cannot use temporary expression in write context";` |
|        - |  962 | `		}` |
|        - |  963 | `	}` |
|   124843 |  964 | `	if( zMsg == 0 ){` |
|   124831 |  965 | `		return SXRET_OK;` |
|        - |  966 | `	}` |
|       16 |  967 | `	rc = PH7_GenCompileError(&(*pGen),E_ERROR,` |
|       12 |  968 | `		pTarget->pStart ? pTarget->pStart->nLine : 0,"%s",zMsg);` |
|       16 |  969 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_INVALID;` |
|   424424 |  970 | `}` |
|        - |  971 | `/*` |
|        - |  972 | ` * What emitting a call's ARGUMENT LIST decided, handed back to the CALL codegen.` |
|        - |  973 | ` * The arguments are emitted from their own routine because php evaluates them` |
|        - |  974 | ` * AFTER the callee has been resolved, so this runs between the callee's emission` |
|        - |  975 | ` * and the OP_CALL — see GenStateEmitCallArgs.` |
|        - |  976 | ` */` |
|        - |  977 | `typedef struct GenCallArgs GenCallArgs;` |
|        - |  978 | `struct GenCallArgs {` |
|        - |  979 | `	sxi32 iP1;      /* OP_CALL.iP1: the compile-time argument count */` |
|        - |  980 | ``	sxu32 iP2;      /* OP_CALL.iP2: 1 if any argument unpacks (`...$a`) */`` |
|        - |  981 | `	void *p3;       /* OP_CALL.p3: the VmCallArgMap, which may ALREADY carry the callee's` |
|        - |  982 | `	                 * namespace qualification — the callee is emitted first now */` |
|        - |  983 | ``	int bFcc;       /* First-class callable `f(...)`: no arguments, OP_LOAD_FCC follows */`` |
|        - |  984 | ``	int bAnySpread; /* Any `...` argument (iP2 says the same; kept for the shape masks) */`` |
|        - |  985 | `};` |
|        - |  986 | `static sxi32 GenStateEmitCallArgs(ph7_gen_state *pGen,ph7_expr_node *pNode,sxi32 iFlags,` |
|        - |  987 | `	GenCallArgs *pArgs);` |
|        - |  988 | `/*` |
|        - |  989 | ` * Generate bytecode for a given expression tree.` |
|        - |  990 | ` * If something goes wrong while generating bytecode` |
|        - |  991 | ` * for the expression tree (A very unlikely scenario)` |
|        - |  992 | ` * this function takes care of generating the appropriate` |
|        - |  993 | ` * error message.` |
|        - |  994 | ` */` |
|  8055548 |  995 | `static sxi32 GenStateEmitExprCode(` |
|        - |  996 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - |  997 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|        - |  998 | `	sxi32 iFlags /* Control flags */` |
|        - |  999 | `	)` |
|        5 | 1000 | `{` |
|        - | 1001 | `	VmInstr *pInstr;` |
|        - | 1002 | `	sxu32 nJmpIdx;` |
|  8055553 | 1003 | `	sxi32 iP1 = 0;` |
|  8055553 | 1004 | `	sxu32 iP2 = 0;` |
|  8055553 | 1005 | `	void *p3  = 0;` |
|        - | 1006 | `	sxi32 iVmOp;` |
|        - | 1007 | `	sxi32 rc;` |
|  8055553 | 1008 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
|  8055553 | 1009 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
|  8055553 | 1010 | `	sxu32 nRhsNsBase = 0;` |
|        - | 1011 | ``	/* Consumed here so it describes THIS node only — the direct operand of a `new` —`` |
|        - | 1012 | `	 * and never travels down into the operand's own sub-expressions. */` |
|  8055553 | 1013 | `	int bNewCallee = (iFlags & EXPR_FLAG_NEW_CALLEE) != 0;` |
|  8055553 | 1014 | `	iFlags &= ~EXPR_FLAG_NEW_CALLEE;` |
|  8055553 | 1015 | `	if( pNode->xCode ){` |
|        - | 1016 | `		SyToken *pTmpIn,*pTmpEnd;` |
|        - | 1017 | `		/* Compile node */` |
|  4976829 | 1018 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
|  4976829 | 1019 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
|  4976829 | 1020 | `		RE_SWAP_DELIMITER(pGen);` |
|  4976829 | 1021 | `		return rc;` |
|        - | 1022 | `	}` |
|  3078729 | 1023 | `	if( pNode->pOp == 0 ){` |
|      ! 0 | 1024 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|        - | 1025 | `			"Invalid expression node,PH7 is aborting compilation");` |
|      ! 0 | 1026 | `		return SXERR_ABORT;` |
|        - | 1027 | `	}` |
|  3078729 | 1028 | `	iVmOp = pNode->pOp->iVmOp;` |
|  3078729 | 1029 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|        - | 1030 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|        - | 1031 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|        - | 1032 | `		 * and later errors are still reported. */` |
|        3 | 1033 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|        - | 1034 | `			"The (unset) cast is no longer supported");` |
|        3 | 1035 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1036 | `			return SXERR_ABORT;` |
|        - | 1037 | `		}` |
|        1 | 1038 | `	}` |
|  3078729 | 1039 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|      167 | 1040 | `		sxu32 nJmp = 0;` |
|        - | 1041 | `		sxu32 nNcNsBase;` |
|        - | 1042 | `		VmInstr *pInstrFix;` |
|        - | 1043 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|        - | 1044 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|        - | 1045 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|        - | 1046 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|        - | 1047 | `		 * stack slot carries a writable nIdx. */` |
|      167 | 1048 | `		if( pNode->pRight ){` |
|        - | 1049 | ``			/* `$a[] ??= v` READS its target before deciding to write, so php refuses the`` |
|        - | 1050 | ``			 * append form anywhere in that target's chain (`$a[][0] ??= v` too), even`` |
|        - | 1051 | ``			 * though the same tag makes every other `[]` on this path a legal write`` |
|        - | 1052 | ``			 * target. Only the container chain is walked — a `[]` inside an INDEX`` |
|        - | 1053 | `			 * expression is an ordinary read and the subscript codegen refuses it. */` |
|      167 | 1054 | `			ph7_expr_node *pTgt = pNode->pRight;` |
|      374 | 1055 | `			while( pTgt && pTgt->pOp && (pTgt->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|       88 | 1056 | `			      \|\| pTgt->pOp->iOp == EXPR_OP_ARROW \|\| pTgt->pOp->iOp == EXPR_OP_DC) ){` |
|      141 | 1057 | `				if( pTgt->pOp->iOp == EXPR_OP_SUBSCRIPT && SySetUsed(&pTgt->aNodeArgs) < 1 ){` |
|      ! 0 | 1058 | `					break;` |
|        - | 1059 | `				}` |
|      141 | 1060 | `				pTgt = pTgt->pLeft;` |
|        3 | 1061 | `			}` |
|      164 | 1062 | `			if( pTgt && pTgt->pOp && pTgt->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|        3 | 1063 | `			 && SySetUsed(&pTgt->aNodeArgs) < 1 ){` |
|      ! 0 | 1064 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,` |
|      ! 0 | 1065 | `					pNode->pRight->pStart ? pNode->pRight->pStart->nLine : 0,` |
|        - | 1066 | `					"Cannot use [] for reading");` |
|      ! 0 | 1067 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1068 | `			}` |
|      167 | 1069 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      167 | 1070 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|      167 | 1071 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1072 | `				return rc;` |
|        - | 1073 | `			}` |
|      167 | 1074 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|        - | 1075 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|        - | 1076 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|        - | 1077 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|        - | 1078 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|        - | 1079 | `			 * the store, so the parent array does not need to be copied at` |
|        - | 1080 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|        - | 1081 | `			 * cascade for the actual write path stays correct. */` |
|      167 | 1082 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|      167 | 1083 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|       97 | 1084 | `				pInstrFix->iP2 = 3;` |
|       47 | 1085 | `			}` |
|       82 | 1086 | `		}` |
|        - | 1087 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|      167 | 1088 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|        - | 1089 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|      167 | 1090 | `		if( pNode->pLeft ){` |
|      167 | 1091 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      167 | 1092 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|      167 | 1093 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1094 | `				return rc;` |
|        - | 1095 | `			}` |
|      167 | 1096 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|       82 | 1097 | `		}` |
|        - | 1098 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|      167 | 1099 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|        - | 1100 | `		/* Patch the short-circuit jump to land after the store. */` |
|      167 | 1101 | `		if( nJmp > 0 ){` |
|      167 | 1102 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|      167 | 1103 | `			if( pInstrFix ){` |
|      167 | 1104 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|       82 | 1105 | `			}` |
|       82 | 1106 | `		}` |
|      167 | 1107 | `		return SXRET_OK;` |
|        - | 1108 | `	}` |
|  3078565 | 1109 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|        - | 1110 | `		sxu32 nJz,nJmp;` |
|        - | 1111 | `		sxu32 nTernaryNsBase;` |
|        - | 1112 | `		/* Ternary operator require special handling */` |
|        - | 1113 | `		/* Phase#1: Compile the condition */` |
|    35299 | 1114 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    35299 | 1115 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    35299 | 1116 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1117 | `			return rc;` |
|        - | 1118 | `		}` |
|        - | 1119 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|        - | 1120 | `		 * compiling the condition must short-circuit to the end of the` |
|        - | 1121 | `		 * condition expression, not leak past the ternary. */` |
|    35299 | 1122 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    35299 | 1123 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    35299 | 1124 | `		if( pNode->pLeft ){` |
|        - | 1125 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|        - | 1126 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    35221 | 1127 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|        - | 1128 | `			/* Phase#3: Compile the 'then' expression  */` |
|    35221 | 1129 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    35221 | 1130 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    35221 | 1131 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1132 | `				return rc;` |
|        - | 1133 | `			}` |
|    35221 | 1134 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    17613 | 1135 | `		}else{` |
|        - | 1136 | `			/* Elvis operator: (expr) ?: (else)` |
|        - | 1137 | `			 * Duplicate condition so original value is the 'then' result.` |
|        - | 1138 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|       82 | 1139 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       82 | 1140 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|        - | 1141 | `		}` |
|        - | 1142 | `		/* Phase#4: Emit the unconditional jump */` |
|    35299 | 1143 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|        - | 1144 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    35299 | 1145 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    35299 | 1146 | `		if( pInstr ){` |
|    35299 | 1147 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    17647 | 1148 | `		}` |
|    35299 | 1149 | `		if( !pNode->pLeft ){` |
|        - | 1150 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|       82 | 1151 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       39 | 1152 | `		}` |
|        - | 1153 | `		/* Phase#6: Compile the 'else' expression */` |
|    35299 | 1154 | `		if( pNode->pRight ){` |
|    35299 | 1155 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    35299 | 1156 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    35299 | 1157 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1158 | `				return rc;` |
|        - | 1159 | `			}` |
|    35299 | 1160 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    17647 | 1161 | `		}` |
|    35299 | 1162 | `		if( nJmp > 0 ){` |
|        - | 1163 | `			/* Phase#7: Fix the unconditional jump */` |
|    35299 | 1164 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    35299 | 1165 | `			if( pInstr ){` |
|    35299 | 1166 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    17647 | 1167 | `			}` |
|    17647 | 1168 | `		}` |
|        - | 1169 | `		/* All done */` |
|    35299 | 1170 | `		return SXRET_OK;` |
|        - | 1171 | `	}` |
|  3043271 | 1172 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
|        - | 1173 | ``		/* PHP 8.5 pipe: `$lhs \|> $rhs` invokes the RHS callable with the LHS`` |
|        - | 1174 | ``		 * value as its sole argument [i.e. `$rhs($lhs)`]. Evaluate the LHS (the`` |
|        - | 1175 | `		 * argument) first, then the RHS callable, then emit a one-argument` |
|        - | 1176 | `		 * OP_CALL — the same stack shape the function-call path builds (the` |
|        - | 1177 | `		 * argument sits below the callee). The RHS is any callable expression:` |
|        - | 1178 | ``		 * an FCC `f(...)` (an OP_LOAD_FCC Closure), a closure variable, an`` |
|        - | 1179 | ``		 * `[obj,method]` pair, or a callable string. */`` |
|        - | 1180 | `		sxu32 nPipeNsBase;` |
|       27 | 1181 | `		sxi32 iOperandFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE\|EXPR_FLAG_RDONLY_LOAD);` |
|       27 | 1182 | `		if( pNode->pLeft == 0 \|\| pNode->pRight == 0 ){` |
|      ! 0 | 1183 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|        - | 1184 | `				"'\|>': Missing operand");` |
|      ! 0 | 1185 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1186 | `		}` |
|        - | 1187 | `		/* Argument: the LHS value. */` |
|       27 | 1188 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       27 | 1189 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iOperandFlags);` |
|       27 | 1190 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1191 | `			return rc;` |
|        - | 1192 | `		}` |
|       27 | 1193 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|        - | 1194 | `		/* Callable: the RHS. */` |
|       27 | 1195 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       27 | 1196 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iOperandFlags);` |
|       27 | 1197 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1198 | `			return rc;` |
|        - | 1199 | `		}` |
|       27 | 1200 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|        - | 1201 | `		/* Invoke the callable with the single piped argument. */` |
|       27 | 1202 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       27 | 1203 | `		return SXRET_OK;` |
|        - | 1204 | `	}` |
|  3043245 | 1205 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|        - | 1206 | `	/* Generate code for the left tree */` |
|  3043245 | 1207 | `	if( pNode->pLeft ){` |
|  3043221 | 1208 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        - | 1209 | `		GenCallArgs sArgs;` |
|  3043221 | 1210 | `		int bArgsEmitted = 0;` |
|  3043221 | 1211 | ``		sxu32 nNewClassInstr = 0; /* index+1 of a `new` operand's class-name push */`` |
|  3043221 | 1212 | `		SyZero(&sArgs,sizeof(sArgs));` |
|        - | 1213 | `		{` |
|        - | 1214 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|        - | 1215 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|        - | 1216 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|        - | 1217 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|        - | 1218 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|        - | 1219 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|        - | 1220 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|        - | 1221 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
|  3043221 | 1222 | `			sxi32 iLeftFlags = iFlags;` |
|        - | 1223 | `			/* The LHS chain whose subscript reads must be QUIET (LOAD_IDX iP2=8):` |
|        - | 1224 | ``			 * `??`'s left operand, and an isset()/empty() chain's intermediate`` |
|        - | 1225 | `			 * links -- php reads both silently and for the value. */` |
|  3043221 | 1226 | `			sxu32 nQuietLhsFirst = PH7_VmInstrLength(pGen->pVm);` |
|  3043221 | 1227 | `			int bQuietLhs = 0;` |
|        - | 1228 | `			/* D1 commit 2: a deferred element/property call arg records its lvalue chain, but` |
|        - | 1229 | `			 * that chain must be CONTIGUOUS. Only propagate DEFER_ARG to the base when the base` |
|        - | 1230 | `			 * is itself a continuable lvalue — a plain variable (an undefined base auto-defers),` |
|        - | 1231 | ``			 * another subscript, or a `->` member. If the base is anything else (most importantly`` |
|        - | 1232 | ``			 * a method CALL, e.g. `$o->items()->prop` or `$r->attributes->item(0)->nodeName`),`` |
|        - | 1233 | `			 * strip DEFER so that intermediate read is a NORMAL read, not a record-mode carrier. */` |
|  3043221 | 1234 | `			if( iLeftFlags & EXPR_FLAG_DEFER_ARG ){` |
|    26929 | 1235 | `				int bContinuable = pNode->pLeft` |
|    20491 | 1236 | `					&& ( (pNode->pLeft->pOp == 0 && pNode->pLeft->xCode == PH7_CompileVariable)` |
|     7316 | 1237 | `					  \|\| (pNode->pLeft->pOp != 0 && (pNode->pLeft->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|      538 | 1238 | `					                              \|\| pNode->pLeft->pOp->iOp == EXPR_OP_ARROW)) );` |
|    13467 | 1239 | `				if( !bContinuable ){` |
|      235 | 1240 | `					iLeftFlags &= ~EXPR_FLAG_DEFER_ARG;` |
|      115 | 1241 | `				}` |
|     6731 | 1242 | `			}` |
|        - | 1243 | `			/*` |
|        - | 1244 | `			 * An isset()/empty() CHAIN reads its intermediate links for their` |
|        - | 1245 | ``			 * VALUE, not for a truth. php walks `isset($o->a->b)` by fetching`` |
|        - | 1246 | ``			 * `$o->a` in BP_VAR_IS mode -- silent, but a real read that runs`` |
|        - | 1247 | `			 * __isset AND THEN __get (or offsetExists and then offsetGet) --` |
|        - | 1248 | `			 * and only the LAST link answers the isset question. PHL gave every` |
|        - | 1249 | `			 * link the terminal context, so the intermediate pushed a bool and` |
|        - | 1250 | `` 			 * the final `->b` was a property of `true`: `isset($model->rel->id)` `` |
|        - | 1251 | `			 * was FALSE for every class with accessors, and so was` |
|        - | 1252 | ``			 * `isset($container['k']['j'])` over ArrayAccess -- a silently wrong`` |
|        - | 1253 | `			 * guard, not a diagnostic.` |
|        - | 1254 | `			 *` |
|        - | 1255 | ``			 * The intermediate context is `??`'s (PH7_MEMBER_COALESCE for a`` |
|        - | 1256 | `			 * member, LOAD_IDX iP2=8 for a subscript, patched over the emitted` |
|        - | 1257 | `			 * range below), which is exactly "silent, and the value": EMPTY's` |
|        - | 1258 | `			 * would read a shade differently, since a class declaring __get with` |
|        - | 1259 | `			 * no __isset is read through __get for an intermediate link and is` |
|        - | 1260 | `			 * NOT for a terminal isset()/empty().` |
|        - | 1261 | `			 */` |
|  3043216 | 1262 | `			if( (iLeftFlags & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY))` |
|  1527230 | 1263 | `				&& pNode->pOp && pNode->pLeft && pNode->pLeft->pOp` |
|     5688 | 1264 | `				&& GEN_IS_ACCESS_OP(pNode->pOp->iOp)` |
|      134 | 1265 | `				&& GEN_IS_ACCESS_OP(pNode->pLeft->pOp->iOp) ){` |
|      109 | 1266 | `				iLeftFlags &= ~(EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY);` |
|      109 | 1267 | `				iLeftFlags \|= EXPR_FLAG_MEMBER_COALESCE\|EXPR_FLAG_QUIET_VAR;` |
|      109 | 1268 | `				bQuietLhs = 1;` |
|       52 | 1269 | `			}` |
|  3043216 | 1270 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
|  2529737 | 1271 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|  1008161 | 1272 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|   991775 | 1273 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|    34233 | 1274 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|  3026107 | 1275 | `			}else if( iLeftFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|        - | 1276 | ``				/* A SUBSCRIPT intermediate of an unset chain (`unset($a['k']['n'])`) keeps`` |
|        - | 1277 | `				 * the unset context — it must COW-separate the parent and must NOT vivify a` |
|        - | 1278 | `				 * missing key — but it is a READ of the container, not an unset of it. The` |
|        - | 1279 | ``				 * UNSET_BASE context says exactly that: `$a['k']` is loaded, where the`` |
|        - | 1280 | `				 * plain unset context would have removed the ELEMENT (and, for an` |
|        - | 1281 | `				 * ArrayAccess base, called offsetUnset() on the intermediate key). */` |
|      223 | 1282 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|      223 | 1283 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_UNSET_BASE;` |
|      109 | 1284 | `			}` |
|        - | 1285 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|        - | 1286 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|        - | 1287 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|        - | 1288 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|        - | 1289 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|        - | 1290 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|        - | 1291 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
|  3043216 | 1292 | `			if( pNode->pOp` |
|  4545621 | 1293 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
|  3024077 | 1294 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  3004869 | 1295 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|    41391 | 1296 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|    20693 | 1297 | `			}` |
|        - | 1298 | ``			/* `++`/`--` mutate their operand in place — the operand is a write`` |
|        - | 1299 | ``			 * lvalue exactly like a compound assign's (`$o->m[0]++` must tag the`` |
|        - | 1300 | ``			 * member base PH7_MEMBER_WRITE the way `$o->m[0] += 1` does: hooked`` |
|        - | 1301 | `			 * properties throw php's Indirect-modification Error, missing ones` |
|        - | 1302 | `			 * auto-vivify). The prec-18 site below handles the assign family;` |
|        - | 1303 | ``			 * `++`/`--` are unary, their operand is pLeft. */`` |
|  3043216 | 1304 | `			if( pNode->pOp` |
|  3043221 | 1305 | `				&& (pNode->pOp->iVmOp == PH7_OP_INCR \|\| pNode->pOp->iVmOp == PH7_OP_DECR) ){` |
|        - | 1306 | ``				/* `(new A)->p++` writes through a temporary exactly as `= 1` does. */`` |
|    48069 | 1307 | `				rc = GenStateWriteTargetCheck(&(*pGen),pNode->pLeft,0);` |
|    48069 | 1308 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 1309 | `					return rc;` |
|        - | 1310 | `				}` |
|    48069 | 1311 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE` |
|        - | 1312 | ``					\| EXPR_FLAG_RMW_LOAD /* php warns before seeding `$undef++` */;`` |
|    24032 | 1313 | `			}` |
|        - | 1314 | ``			/* `??` reads its LEFT operand in isset-context: an undefined or`` |
|        - | 1315 | `			 * UNINITIALIZED typed PROPERTY must yield the default rather than a` |
|        - | 1316 | `			 * warning/Error (php). Tag a member-access LHS so its OP_MEMBER takes` |
|        - | 1317 | `			 * the silent-lookup path (iP2 = ISSET), which still loads a present` |
|        - | 1318 | `			 * value. A SUBSCRIPT LHS is left untagged: LOAD_IDX's ISSET mode means` |
|        - | 1319 | ``			 * offsetExists (a bool), but `$o[$k] ?? d` needs the offsetGet value —`` |
|        - | 1320 | `			 * that path is already handled correctly by OP_NULLC. */` |
|  3043221 | 1321 | `			if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC && pNode->pLeft ){` |
|        - | 1322 | ``				/* php reads the ENTIRE left operand of `??` in isset-context: no`` |
|        - | 1323 | ``				 * "Undefined variable" for `$x ?? d` OR for the base of`` |
|        - | 1324 | ``				 * `$x['k'] ?? d`. QUIET_VAR silences the variable read wherever it`` |
|        - | 1325 | `				 * sits in the chain. */` |
|      377 | 1326 | `				iLeftFlags \|= EXPR_FLAG_QUIET_VAR;` |
|      377 | 1327 | `				bQuietLhs = 1;` |
|      372 | 1328 | `				if( pNode->pLeft->pOp` |
|      450 | 1329 | `					&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|      271 | 1330 | `						\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|      234 | 1331 | `						\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|        - | 1332 | `					/* A member-access LHS additionally takes OP_MEMBER's SILENT` |
|        - | 1333 | `					 * lookup so an uninitialized typed property yields the default` |
|        - | 1334 | `					 * instead of an Error. It used to borrow isset()'s context for` |
|        - | 1335 | `					 * that, which is the same mistake the comment below records for` |
|        - | 1336 | `					 * subscripts: silence is shared, but isset() context makes every` |
|        - | 1337 | ``					 * ACCESSOR answer a truth, and `$o->p ?? d` needs the accessor's`` |
|        - | 1338 | ``					 * VALUE — so `??` has its own member context. A SUBSCRIPT LHS`` |
|        - | 1339 | `					 * still takes neither: LOAD_IDX's ISSET mode means offsetExists` |
|        - | 1340 | ``					 * (a bool), while `$o[$k] ?? d` needs the offsetGet value —`` |
|        - | 1341 | `					 * OP_NULLC already handles that path. */` |
|       80 | 1342 | `					iLeftFlags \|= EXPR_FLAG_MEMBER_COALESCE;` |
|       38 | 1343 | `				}` |
|      186 | 1344 | `			}` |
|  3043221 | 1345 | `			if( iVmOp == PH7_OP_ERR_CTRL ){` |
|        - | 1346 | `				/* '@' must suppress the diagnostics raised WHILE its operand runs, so` |
|        - | 1347 | `				 * open the window here; the trailing emit below closes it (iP1 = 0). */` |
|    16579 | 1348 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_ERR_CTRL,1,0,0,0);` |
|     8287 | 1349 | `			}` |
|  3043221 | 1350 | `			if( iVmOp == PH7_OP_NEW ){` |
|        - | 1351 | ``				/* Mark the direct operand so a call node under it (`new C($a)`) keeps the`` |
|        - | 1352 | `				 * emission order OP_NEW is assembled from — see the bNewCallee branch above. */` |
|    87067 | 1353 | `				iLeftFlags \|= EXPR_FLAG_NEW_CALLEE;` |
|    43531 | 1354 | `			}` |
|  3043221 | 1355 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags\|EXPR_FLAG_RDONLY_LOAD);` |
|  3043221 | 1356 | `			if( rc == SXRET_OK && bQuietLhs ){` |
|        - | 1357 | `				/* Mark EVERY subscript read in the quiet left chain (iP2=8).` |
|        - | 1358 | `				 * Peeking at the next instruction only catches the OUTERMOST` |
|        - | 1359 | ``				 * access, so `$d['x']['y'] ?? $v` still warned for the inner one;`` |
|        - | 1360 | `				 * and a forward scan would be unsound, since an unrelated sibling` |
|        - | 1361 | ``				 * access can sit immediately before a coalesce (`f($a['k'],`` |
|        - | 1362 | ``				 * $b['j'] ?? 1)`). The compiler knows the real extent, so it marks`` |
|        - | 1363 | `				 * the range it just emitted. Write-context codes (1/3/5) belong to` |
|        - | 1364 | ``				 * `??=` and keep their meaning. */`` |
|      481 | 1365 | `				sxu32 nEnd = PH7_VmInstrLength(pGen->pVm);` |
|        - | 1366 | `				sxu32 nAt;` |
|     2025 | 1367 | `				for( nAt = nQuietLhsFirst ; nAt < nEnd ; ++nAt ){` |
|     1549 | 1368 | `					VmInstr *pFix = PH7_VmGetInstr(pGen->pVm,nAt);` |
|     1549 | 1369 | `					if( pFix && pFix->iOp == PH7_OP_LOAD_IDX && pFix->iP2 == 0 ){` |
|      223 | 1370 | `						pFix->iP2 = 8;` |
|      109 | 1371 | `					}` |
|      777 | 1372 | `				}` |
|      238 | 1373 | `			}` |
|        - | 1374 | `		}` |
|  3043221 | 1375 | `		if( rc != SXRET_OK ){` |
|       67 | 1376 | `			return rc;` |
|        - | 1377 | `		}` |
|  3043159 | 1378 | `		if( !bIsChainOp ){` |
|        - | 1379 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|        - | 1380 | `			 * target the end of that LHS chain, which is right here. */` |
|  2090505 | 1381 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|  1045250 | 1382 | `		}` |
|  3043159 | 1383 | `		if( iVmOp == PH7_OP_CALL ){` |
|   699611 | 1384 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   699611 | 1385 | `			if( pInstr ){` |
|   699611 | 1386 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|   664195 | 1387 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|        - | 1388 | `					sxu32 nQual;` |
|   664195 | 1389 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|        - | 1390 | `					/* Prevent constant expansion but preserve the absolute flag` |
|        - | 1391 | `					 * so the later NEW handler (if any) can see it. */` |
|   664195 | 1392 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|        - | 1393 | `					/* Namespace-qualify the function name for CALL, unless the` |
|        - | 1394 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|        - | 1395 | `					 * imports — class imports must NOT affect function` |
|        - | 1396 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|        - | 1397 | `					 * before NEW; we store the original literal index in the` |
|        - | 1398 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|        - | 1399 | `					 * the unqualified name and re-qualify with class imports. */` |
|   664195 | 1400 | `					if( bAbsolute ){` |
|       83 | 1401 | `						pInstr->iP2 = (sxi32)nOrig;` |
|       44 | 1402 | `					}else{` |
|   664117 | 1403 | `						int fromImport = 0;` |
|   664117 | 1404 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|   664117 | 1405 | `						pInstr->iP2 = (sxi32)nQual;` |
|   664117 | 1406 | `						if( nQual != nOrig ){` |
|        - | 1407 | `							/* Record the original literal index in the arg map` |
|        - | 1408 | `							 * (NOT in the CALL's iP2 — that is the hasSpread` |
|        - | 1409 | `							 * flag) so the NEW handler can recover the` |
|        - | 1410 | `							 * unqualified name and re-qualify with CLASS` |
|        - | 1411 | `							 * imports. */` |
|      167 | 1412 | `							if( p3 == 0 ){` |
|      167 | 1413 | `								VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      162 | 1414 | `									&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|      167 | 1415 | `								if( pMap ){` |
|      167 | 1416 | `									SyZero(pMap, sizeof(VmCallArgMap));` |
|      167 | 1417 | `									p3 = (void *)pMap;` |
|       81 | 1418 | `								}` |
|       81 | 1419 | `							}` |
|      167 | 1420 | `							if( p3 ){` |
|      167 | 1421 | `								((VmCallArgMap *)p3)->nOrigNameLit = nOrig + 1;` |
|      167 | 1422 | `								if( !fromImport ){` |
|        - | 1423 | `									/* Mark as namespace-qualified */` |
|      143 | 1424 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|       69 | 1425 | `								}` |
|       81 | 1426 | `							}` |
|       81 | 1427 | `						}` |
|        - | 1428 | `					}` |
|   367516 | 1429 | `				}else if( (pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */` |
|    33311 | 1430 | `						&& !(pNode->pLeft && (pNode->pLeft->iFlags & EXPR_NODE_PARENS)))` |
|    19833 | 1431 | `					\|\| pInstr->iOp == PH7_OP_NEW ){` |
|        - | 1432 | `					/* Method call,flag that. But NOT when the callee was an explicitly` |
|        - | 1433 | ``					 * PARENTHESISED member access: `($o->p)(...)` / `($o::$p)(...)` invokes`` |
|        - | 1434 | `					 * the VALUE of the property (php's variable-invocation), so the OP_MEMBER` |
|        - | 1435 | `					 * must stay a plain property READ (leaving the callable on the stack for` |
|        - | 1436 | `					 * OP_CALL to invoke) rather than being rewritten into a method-name` |
|        - | 1437 | ``					 * resolution — the parens are exactly what distinguishes `($o->p)()` from`` |
|        - | 1438 | ``					 * the method call `$o->p()`. */`` |
|    31187 | 1439 | `					pInstr->iP2 = 1;` |
|        - | 1440 | ``					/* A static call with a DYNAMIC method name (`C::$m(...)`): the`` |
|        - | 1441 | ``					 * static-`::` codegen folded the variable NAME into OP_MEMBER->p3`` |
|        - | 1442 | ``					 * as if it were a static-PROPERTY read (`C::$m`), but in a CALL the`` |
|        - | 1443 | `					 * method name is the variable's VALUE. Rebuild the sequence` |
|        - | 1444 | `					 * [class, OP_LOAD $m -> value, OP_MEMBER(static, method)] so the` |
|        - | 1445 | `					 * dynamic name is read off the stack, matching the instance` |
|        - | 1446 | ``					 * (`$o->$m()`) path. iP1==1 marks a static member. */`` |
|    31187 | 1447 | `					if( pInstr->iOp == PH7_OP_MEMBER && pInstr->iP1 == 1 && pInstr->p3 ){` |
|       17 | 1448 | `						void *pDynName = pInstr->p3;` |
|       17 | 1449 | `						(void)PH7_VmPopInstr(pGen->pVm);` |
|       17 | 1450 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,pDynName,0);` |
|       17 | 1451 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_MEMBER,1,PH7_MEMBER_METHOD,0,0);` |
|        8 | 1452 | `					}` |
|    15591 | 1453 | `				}` |
|   349803 | 1454 | `			}` |
|        - | 1455 | `			/* The callee is resolved; NOW emit the arguments. php's order — the callee` |
|        - | 1456 | `			 * first, at its INIT_FCALL / INIT_METHOD_CALL, and the arguments only after` |
|        - | 1457 | ``			 * it — is what makes `(new C)->priv(boom())` report php's `Call to private`` |
|        - | 1458 | `` 			 * method` instead of whatever the argument threw, and what keeps `$o?->m(f())` `` |
|        - | 1459 | ``			 * from running `f()` on a null receiver. It also puts the callee in reach of`` |
|        - | 1460 | `			 * the argument ops one opcode EARLIER than OP_CALL.` |
|        - | 1461 | `			 *` |
|        - | 1462 | `			 * The stack that leaves here is therefore [callee][args…] — the mirror of the` |
|        - | 1463 | `			 * layout OP_CALL's whole dispatch is written against (the method-name pair` |
|        - | 1464 | `			 * below the arguments, the spread runs counted down from the top, the` |
|        - | 1465 | `			 * deferred-argument re-walk). OP_ROT_CALLEE turns the region back over just` |
|        - | 1466 | `			 * before the call, so nothing downstream of it changes. */` |
|   699611 | 1467 | `			if( !bArgsEmitted ){` |
|        - | 1468 | `				int bTwoSlot;` |
|   699611 | 1469 | `				sArgs.p3 = p3; /* the namespace map built just above, if any */` |
|        - | 1470 | `				/* A METHOD callee leaves TWO slots — [receiver][method name] — which` |
|        - | 1471 | `				 * OP_CALL reads as one callee (the receiver answers $this and the` |
|        - | 1472 | `				 * late-static-binding class); anything else leaves one. The instruction` |
|        - | 1473 | `				 * just emitted is what decides it. */` |
|   699611 | 1474 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   715214 | 1475 | `				bTwoSlot = pInstr && pInstr->iOp == PH7_OP_MEMBER` |
|   365406 | 1476 | `					&& pInstr->iP2 == PH7_MEMBER_METHOD` |
|        - | 1477 | `					/* …unless the member NAME was folded into p3 rather than pushed:` |
|        - | 1478 | `					 * that shape pushes the target alone, so the op leaves one slot,` |
|        - | 1479 | `					 * which is the same distinction vm_ops_oo.c makes before popping. */` |
|  1049409 | 1480 | `					&& pInstr->p3 == 0;` |
|        - | 1481 | `				/* Screen the callee HERE, where php screens it: an undefined function, a` |
|        - | 1482 | `				 * callable string/array naming nothing, a value that is not callable at` |
|        - | 1483 | `				 * all. A METHOD callee needs none — its OP_MEMBER just did it, against` |
|        - | 1484 | `				 * the entry it chose. An FCC needs none either: OP_LOAD_FCC runs the same` |
|        - | 1485 | `				 * screen, and nothing runs before it. And with NO arguments the call` |
|        - | 1486 | `				 * itself is already the first thing to happen, so there is nothing to` |
|        - | 1487 | `				 * order and no reason to pay for a second resolution. */` |
|        - | 1488 | `				{` |
|   699611 | 1489 | `					ph7_expr_node **apCallArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   699611 | 1490 | `					sxi32 nCallArg = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|   816106 | 1491 | `					int bNodeFcc = nCallArg == 1 && apCallArg[0]` |
|   932755 | 1492 | `						&& (apCallArg[0]->iFlags & EXPR_NODE_FCC);` |
|   699611 | 1493 | `					if( bNewCallee ){` |
|        - | 1494 | ``						/* A `new`'s operand: the screen is OP_NEW itself, run with no`` |
|        - | 1495 | `						 * arguments on the stack (iP1 = -1). It asks every refusal the` |
|        - | 1496 | `						 * real pass asks and leaves the class name standing, so the two` |
|        - | 1497 | `						 * cannot disagree. Record where that push is — the NEW codegen` |
|        - | 1498 | `						 * used to find it one instruction behind the trailing OP_CALL,` |
|        - | 1499 | `						 * and the argument list now sits in between. */` |
|    84875 | 1500 | `						nNewClassInstr = PH7_VmInstrLength(pGen->pVm);` |
|    84875 | 1501 | `						if( nCallArg > 0 ){` |
|    82655 | 1502 | `							PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,-1,0,0,0);` |
|    41330 | 1503 | `						}` |
|   657176 | 1504 | `					}else if( nCallArg > 0 && !bTwoSlot && !bNodeFcc ){` |
|   572313 | 1505 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL_INIT,0,` |
|   572278 | 1506 | `							(p3 && ((VmCallArgMap *)p3)->bIsNamespaced) ? 1 : 0,0,0);` |
|   286139 | 1507 | `					}` |
|        - | 1508 | `				}` |
|   699611 | 1509 | `				rc = GenStateEmitCallArgs(&(*pGen),pNode,iFlags,&sArgs);` |
|   699611 | 1510 | `				if( rc != SXRET_OK ){` |
|       10 | 1511 | `					return rc;` |
|        - | 1512 | `				}` |
|   699603 | 1513 | `				iP1 = sArgs.iP1;` |
|   699603 | 1514 | `				iP2 = sArgs.iP2;` |
|   699603 | 1515 | `				p3  = sArgs.p3;` |
|   699603 | 1516 | `				bFcc = sArgs.bFcc;` |
|   699603 | 1517 | `				if( iP1 > 0 \|\| iP2 ){` |
|   991736 | 1518 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_ROT_CALLEE,iP1,` |
|   661154 | 1519 | `						(iP2 ? PH7_ROT_SPREAD : 0) \| (bTwoSlot ? PH7_ROT_TWOSLOT : 0),0,0);` |
|   330577 | 1520 | `				}` |
|   699603 | 1521 | `				if( bNewCallee && nNewClassInstr > 0 ){` |
|    84875 | 1522 | `					if( p3 == 0 ){` |
|     2187 | 1523 | `						VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|     2182 | 1524 | `							&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|     2187 | 1525 | `						if( pMap ){` |
|     2187 | 1526 | `							SyZero(pMap,sizeof(VmCallArgMap));` |
|     2187 | 1527 | `							p3 = (void *)pMap;` |
|     1091 | 1528 | `						}` |
|     1091 | 1529 | `					}` |
|    84875 | 1530 | `					if( p3 ){` |
|    84875 | 1531 | `						((VmCallArgMap *)p3)->nNewClassInstr = nNewClassInstr;` |
|    42435 | 1532 | `					}` |
|    42435 | 1533 | `				}` |
|   349804 | 1534 | `			}` |
|  2693352 | 1535 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|        - | 1536 | `			ph7_expr_node **apNode;` |
|        - | 1537 | `			sxi32 n;` |
|   211667 | 1538 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|        - | 1539 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|        - | 1540 | `				\|EXPR_FLAG_LOAD_IDX_UNSET_BASE` |
|        - | 1541 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE` |
|        - | 1542 | `				\|EXPR_FLAG_MEMBER_COALESCE` |
|        - | 1543 | `				\|EXPR_FLAG_QUIET_VAR\|EXPR_FLAG_RMW_LOAD\|EXPR_FLAG_DEFER_ARG);` |
|        - | 1544 | `			/* Recurse and generate bytecodes for array index */` |
|   211667 | 1545 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   390753 | 1546 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   179091 | 1547 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   179091 | 1548 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   179091 | 1549 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 1550 | `					return rc;` |
|        - | 1551 | `				}` |
|        - | 1552 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   179091 | 1553 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|    89548 | 1554 | `			}` |
|   211667 | 1555 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   179091 | 1556 | `				iP1 = 1; /* Node have an index associated with it */` |
|    89548 | 1557 | `			}else{` |
|        - | 1558 | ``				/* `[]` names the element a WRITE is about to create, so php allows it`` |
|        - | 1559 | `				 * only where a write lands: an assignment target (plain, compound,` |
|        - | 1560 | ``				 * `=&`, a list()/foreach target) and a by-reference argument. Every`` |
|        - | 1561 | `				 * other placement is a COMPILE error there — PHL accepted them all and` |
|        - | 1562 | ``				 * answered NULL after a PH7-worded notice, so `$x = $a[];`,`` |
|        - | 1563 | ``				 * `isset($a[])` and `unset($a[][0])` were silent no-ops on source php`` |
|        - | 1564 | `				 * refuses to run. A call ARGUMENT is the one shape php also leaves to` |
|        - | 1565 | `				 * runtime (it cannot know the parameter's by-ref-ness at compile time),` |
|        - | 1566 | `				 * which is what DEFER_ARG marks. */` |
|    32581 | 1567 | `				if( iFlags & (EXPR_FLAG_LOAD_IDX_UNSET\|EXPR_FLAG_LOAD_IDX_UNSET_BASE) ){` |
|      ! 0 | 1568 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,` |
|      ! 0 | 1569 | `						pNode->pStart ? pNode->pStart->nLine : 0,` |
|        - | 1570 | `						"Cannot use [] for unsetting");` |
|      ! 0 | 1571 | `					return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1572 | `				}` |
|    32581 | 1573 | `				if( (iFlags & (EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_DEFER_ARG)) == 0 ){` |
|      ! 0 | 1574 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,` |
|      ! 0 | 1575 | `						pNode->pStart ? pNode->pStart->nLine : 0,` |
|        - | 1576 | `						"Cannot use [] for reading");` |
|      ! 0 | 1577 | `					return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1578 | `				}` |
|        - | 1579 | `			}` |
|   211667 | 1580 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|        - | 1581 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|    10935 | 1582 | `				iP2 = 4;` |
|   206202 | 1583 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|        - | 1584 | `				/* offsetUnset for ArrayAccess; for an array, remove the ELEMENT. */` |
|      193 | 1585 | `				iP2 = 5;` |
|   200643 | 1586 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET_BASE ){` |
|        - | 1587 | `				/* An unset chain's intermediate container: read it, but with the` |
|        - | 1588 | `				 * unset context's COW-separate and no-vivify rules. */` |
|       20 | 1589 | `				iP2 = 10;` |
|   200540 | 1590 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|        - | 1591 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|        - | 1592 | `				 * short-circuit on missing keys without invoking offsetGet` |
|        - | 1593 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|       51 | 1594 | `				iP2 = 6;` |
|   200508 | 1595 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|        - | 1596 | `				/* Create an empty entry when the desired index is not found */` |
|   123155 | 1597 | `				iP2 = 1;` |
|   138910 | 1598 | `			}else if( iFlags & EXPR_FLAG_DEFER_ARG ){` |
|        - | 1599 | `				/* D1 commit 2: deferred by-ref/by-value element arg. Behaves as a read but,` |
|        - | 1600 | `				 * on a lookup miss, records the lvalue path instead of warning; OP_CALL` |
|        - | 1601 | `				 * re-walks it in vivify (by-ref) or read+warn (by-value) mode. */` |
|    11409 | 1602 | `				iP2 = 9;` |
|     5707 | 1603 | `			}` |
|  2237722 | 1604 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|        - | 1605 | `			/* POP the left node */` |
|        5 | 1606 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        2 | 1607 | `		}` |
|  1521573 | 1608 | `	}` |
|  3043175 | 1609 | `	rc = SXRET_OK;` |
|  3043175 | 1610 | `	nJmpIdx = 0;` |
|        - | 1611 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|        - | 1612 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|        - | 1613 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  3043175 | 1614 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     2999 | 1615 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     2999 | 1616 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     2999 | 1617 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     2999 | 1618 | `			int isSpecial = 0;` |
|     2999 | 1619 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     2867 | 1620 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     2867 | 1621 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     2862 | 1622 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     2663 | 1623 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     1339 | 1624 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      447 | 1625 | `					isSpecial = 1;` |
|      221 | 1626 | `				}` |
|     1464 | 1627 | `			}` |
|     3065 | 1628 | `			pInstr->iP1 = 0;` |
|        - | 1629 | ``			/* A leading `\` (NSSEP) makes the class name ABSOLUTE: it names the`` |
|        - | 1630 | `			 * global class, so it must NOT be re-qualified with the current namespace.` |
|        - | 1631 | ``			 * `\Closure::bind(...)` inside `namespace X` is Closure, not X\Closure.`` |
|        - | 1632 | ``			 * (Multi-component `\A\B::m` already resolves absolutely because its`` |
|        - | 1633 | ``			 * literal keeps a backslash; the single-component `\Closure` lost it.) */`` |
|        - | 1634 | `			{` |
|     4529 | 1635 | `				int bAbsolute = (pNode->pLeft && pNode->pLeft->pStart` |
|     4392 | 1636 | `					&& (pNode->pLeft->pStart->nType & PH7_TK_NSSEP));` |
|     2933 | 1637 | `				if( !isSpecial && !bAbsolute ){` |
|     2469 | 1638 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     1232 | 1639 | `				}` |
|        - | 1640 | `			}` |
|        - | 1641 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|        - | 1642 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     2933 | 1643 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|     2491 | 1644 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|     2491 | 1645 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      214 | 1646 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      131 | 1647 | `					return SXRET_OK;` |
|        - | 1648 | `				}` |
|     1180 | 1649 | `			}` |
|     1401 | 1650 | `		}` |
|     1470 | 1651 | `	}` |
|        - | 1652 | `	/* Generate code for the right tree */` |
|  3043025 | 1653 | `	if( pNode->pRight ){` |
|  1735255 | 1654 | `		if( iVmOp == PH7_OP_LAND ){` |
|        - | 1655 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    84657 | 1656 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  1692929 | 1657 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|        - | 1658 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    73753 | 1659 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  1613729 | 1660 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|        - | 1661 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|      377 | 1662 | `			iVmOp = 0; /* No binary operator to emit */` |
|      377 | 1663 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  1576738 | 1664 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|        - | 1665 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|        - | 1666 | `			 * the entire containing postfix chain to null. The jump target is` |
|        - | 1667 | `			 * patched later by the innermost non-chain ancestor (or by` |
|        - | 1668 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|        - | 1669 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|        - | 1670 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|      143 | 1671 | `			sxu32 nNsJmp = 0;` |
|      143 | 1672 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|      143 | 1673 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  1576414 | 1674 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */` |
|  1220000 | 1675 | ``			\|\| pNode->pOp->iOp == EXPR_OP_REF /* `=&` reference bind */ ){`` |
|        - | 1676 | `` 			/* The lvalue is the RIGHT operand (prec-18 ops are right-associative; `=&` `` |
|        - | 1677 | `			 * swaps its operands in parse.c so its target is pRight too). Mark it a write` |
|        - | 1678 | `			 * target so a missing base (the container of a subscript-write, or a bare` |
|        - | 1679 | `` 			 * `$o->p`) is auto-created — PHP auto-vivifies on a plain write AND on a `=&` `` |
|        - | 1680 | ``			 * bind (`$a[0] =& $x` creates $a as [0 => &$x], it does not warn). */`` |
|        - | 1681 | `			/* php's compile-time write-target rules first ($this, a temporary base). */` |
|   712899 | 1682 | `			rc = GenStateWriteTargetCheck(&(*pGen),pNode->pRight,0);` |
|   712899 | 1683 | `			if( rc != SXRET_OK ){` |
|       11 | 1684 | `				return rc;` |
|        - | 1685 | `			}` |
|   712891 | 1686 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|   712891 | 1687 | `			if( iVmOp != PH7_OP_STORE && pNode->pOp->iOp != EXPR_OP_REF ){` |
|        - | 1688 | ``				/* COMPOUND assignment (`.=`, `+=`, ...) READS the target first, so`` |
|        - | 1689 | ``				 * php warns when it is undefined and then seeds it; a plain `=` and a`` |
|        - | 1690 | ``				 * `=&` rebind write without reading the target and stay silent. */`` |
|    16399 | 1691 | `				iFlags \|= EXPR_FLAG_RMW_LOAD;` |
|     8197 | 1692 | `			}` |
|   356443 | 1693 | `		}` |
|  1735247 | 1694 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  1735247 | 1695 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_RDONLY_LOAD);` |
|  1735247 | 1696 | `		if( !bIsChainOp ){` |
|        - | 1697 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|        - | 1698 | `			 * operator instruction is emitted. */` |
|  1693987 | 1699 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|   846991 | 1700 | `		}` |
|  1735247 | 1701 | `		if( iVmOp == PH7_OP_STORE ){` |
|   696295 | 1702 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|   696248 | 1703 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|        - | 1704 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|        - | 1705 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|        - | 1706 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|        - | 1707 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|        - | 1708 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|        - | 1709 | `				 */` |
|      223 | 1710 | `				iVmOp = 0;` |
|   696186 | 1711 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|   696077 | 1712 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|        - | 1713 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|     1485 | 1714 | `					iP2 = 1;` |
|      745 | 1715 | `				}else{` |
|   694597 | 1716 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|        - | 1717 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   122709 | 1718 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   122709 | 1719 | `						iP1 = pInstr->iP1;` |
|    61357 | 1720 | `					}else{` |
|   571893 | 1721 | `						p3 = pInstr->p3;` |
|        - | 1722 | `					}` |
|        - | 1723 | `					/* POP the last dynamic load instruction */` |
|   694597 | 1724 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|        - | 1725 | `				}` |
|   348041 | 1726 | `			}` |
|  1387102 | 1727 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|        - | 1728 | `			/* Peek first: a member LHS ($o->p =& $x, C::$s =& $x) keeps its` |
|        - | 1729 | `			 * OP_MEMBER in place (it resolves + stashes the target slot at` |
|        - | 1730 | `			 * runtime), unlike the variable/array shapes which fold their load` |
|        - | 1731 | `			 * away. Mirrors the normal member-store fold above (iP2 kept-MEMBER). */` |
|      206 | 1732 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      206 | 1733 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|        - | 1734 | `				/* Tag the member as a reference target and flag STORE_REF (iP2=1)` |
|        - | 1735 | `				 * to take the member-rebind path in the VM. */` |
|       48 | 1736 | `				pInstr->iP2 = PH7_MEMBER_REF_TARGET;` |
|       48 | 1737 | `				iP2 = 1;` |
|       25 | 1738 | `			}else{` |
|      160 | 1739 | `				pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      160 | 1740 | `				if( pInstr ){` |
|      160 | 1741 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|        - | 1742 | `						/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|        - | 1743 | `						 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|        - | 1744 | `						 */` |
|       55 | 1745 | `						iVmOp = PH7_OP_STORE_IDX_REF;` |
|       55 | 1746 | `						iP1 = pInstr->iP1;` |
|       55 | 1747 | `						iP2 = pInstr->iP2;` |
|       55 | 1748 | `						p3  = pInstr->p3;` |
|       29 | 1749 | `					}else{` |
|      108 | 1750 | `						p3 = pInstr->p3;` |
|        - | 1751 | `					}` |
|       78 | 1752 | `				}` |
|        - | 1753 | `			}` |
|      101 | 1754 | `		}` |
|   867621 | 1755 | `	}` |
|  3043012 | 1756 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|    44632 | 1757 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|        - | 1758 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|        - | 1759 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|       79 | 1760 | `		iVmOp = 0;` |
|       37 | 1761 | `	}` |
|  3043017 | 1762 | `	if( iVmOp > 0 ){` |
|  3042349 | 1763 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|    48069 | 1764 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|        - | 1765 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|     5315 | 1766 | `				iP1 = 1;` |
|     2660 | 1767 | `			}` |
|  3018317 | 1768 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|        - | 1769 | `			/* Namespace-qualify the class name for NEW */ {` |
|    86993 | 1770 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|    86993 | 1771 | `				VmInstr *pCallInstr = 0;` |
|    86993 | 1772 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|    84875 | 1773 | `					VmCallArgMap *pNewMap = (VmCallArgMap *)pPeek->p3;` |
|    84875 | 1774 | `					pCallInstr = pPeek;` |
|        - | 1775 | `` 					/* The class-name push sits one instruction back only when this `new` `` |
|        - | 1776 | `					 * takes no arguments; with an argument list the reorder puts the whole` |
|        - | 1777 | `					 * list (and its screen and rotation) in between, so the call node` |
|        - | 1778 | `					 * recorded where the push is. */` |
|   127310 | 1779 | `					pPeek = (pNewMap && pNewMap->nNewClassInstr > 0)` |
|    84870 | 1780 | `						? PH7_VmGetInstr(pGen->pVm,pNewMap->nNewClassInstr - 1)` |
|    42435 | 1781 | `						: PH7_VmPeekNextInstr(pGen->pVm);` |
|    42435 | 1782 | `				}` |
|    86993 | 1783 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|    86957 | 1784 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|        - | 1785 | `					sxu32 nLitForClass;` |
|    86957 | 1786 | `					VmCallArgMap *pCallNsMap = pCallInstr ? (VmCallArgMap *)pCallInstr->p3 : 0;` |
|        - | 1787 | `					/* If the CALL handler qualified the name with FUNCTION` |
|        - | 1788 | `					 * imports, recover the original literal (recorded in the` |
|        - | 1789 | `					 * arg map — OP_CALL's iP2 is the hasSpread flag, and` |
|        - | 1790 | `` 					 * misreading it as a literal index made `new C(...$args)` `` |
|        - | 1791 | `					 * fatal with "Class ' ' is not defined") and re-qualify` |
|        - | 1792 | `					 * with class imports. */` |
|    86957 | 1793 | `					if( pCallNsMap && pCallNsMap->nOrigNameLit > 0 ){` |
|       64 | 1794 | `						nLitForClass = pCallNsMap->nOrigNameLit - 1;` |
|       34 | 1795 | `					}else{` |
|    86897 | 1796 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|        - | 1797 | `					}` |
|    86957 | 1798 | `					pPeek->iP1 = 0;` |
|    86957 | 1799 | `					if( !bAbsolute ){` |
|        - | 1800 | `						/* self/static/parent are resolved at runtime against the` |
|        - | 1801 | `						 * current class — never namespace-qualify them (else` |
|        - | 1802 | ``						 * `new self` in namespace N becomes "N\self"). Mirrors the`` |
|        - | 1803 | `						 * instanceof (IS_A) guard below. */` |
|    86901 | 1804 | `						ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nLitForClass);` |
|    86901 | 1805 | `						int isSpecialNew = 0;` |
|    86901 | 1806 | `						if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|    86901 | 1807 | `							const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|    86901 | 1808 | `							sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|    86896 | 1809 | `							if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    87007 | 1810 | `								(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    43563 | 1811 | `								(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|       46 | 1812 | `								isSpecialNew = 1;` |
|       22 | 1813 | `							}` |
|    43448 | 1814 | `						}` |
|    86901 | 1815 | `						if( isSpecialNew ){` |
|       46 | 1816 | `							pPeek->iP2 = (sxi32)nLitForClass;` |
|       24 | 1817 | `						}else{` |
|    86857 | 1818 | `							pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|        - | 1819 | `						}` |
|    43453 | 1820 | `					}else{` |
|       61 | 1821 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|        - | 1822 | `					}` |
|    43476 | 1823 | `				}` |
|        - | 1824 | `			}` |
|    86993 | 1825 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    86993 | 1826 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|        - | 1827 | `				VmInstr *pPrev;` |
|        - | 1828 | `				int bPrevMember;` |
|    84875 | 1829 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|        - | 1830 | `				/* "Was the callee a MEMBER access?" — which, once the reorder puts a` |
|        - | 1831 | `				 * rotation between the callee and its call, is the question the rotation` |
|        - | 1832 | `				 * already answers (a method callee is the two-slot one). */` |
|   127310 | 1833 | `				bPrevMember = pPrev && (pPrev->iOp == PH7_OP_ROT_CALLEE` |
|    82650 | 1834 | `					? (pPrev->iP2 & PH7_ROT_TWOSLOT) != 0` |
|     2220 | 1835 | `					: pPrev->iOp == PH7_OP_MEMBER);` |
|    84875 | 1836 | `				if( !bPrevMember ){` |
|        - | 1837 | `					/* Pop the call instruction, preserve named-arg map and` |
|        - | 1838 | `					 * the hasSpread flag (OP_NEW consumes the spread` |
|        - | 1839 | `					 * accumulator exactly like OP_CALL would have). */` |
|    84875 | 1840 | `					iP1 = pInstr->iP1;` |
|    84875 | 1841 | `					iP2 = pInstr->iP2;` |
|    84875 | 1842 | `					if( pInstr->p3 ){` |
|    84875 | 1843 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|    42435 | 1844 | `					}` |
|    84875 | 1845 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    42435 | 1846 | `				}` |
|    42440 | 1847 | `			}` |
|  2950791 | 1848 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|        - | 1849 | `			/* instanceof: right operand is a class name, not a constant.` |
|        - | 1850 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|    10985 | 1851 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    10985 | 1852 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    10985 | 1853 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    10985 | 1854 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|    10985 | 1855 | `				int isSpecialIs = 0;` |
|    10985 | 1856 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|    10985 | 1857 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|    10985 | 1858 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|    10980 | 1859 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    10983 | 1860 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     5490 | 1861 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|       12 | 1862 | `						isSpecialIs = 1;` |
|        5 | 1863 | `					}` |
|     5490 | 1864 | `				}` |
|    10985 | 1865 | `				pInstr->iP1 = 0;` |
|    10985 | 1866 | `				if( !isSpecialIs && !bAbsolute ){` |
|    10949 | 1867 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     5472 | 1868 | `				}` |
|     5495 | 1869 | `			}` |
|  2901807 | 1870 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|        - | 1871 | `			/* Prevent constant expansion for member/property names.` |
|        - | 1872 | `			 * The right child (member name) was just compiled — its LOADC` |
|        - | 1873 | `			 * should not trigger constant lookup. */` |
|    41265 | 1874 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    41265 | 1875 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    40791 | 1876 | `				pInstr->iP1 = 0;` |
|    20393 | 1877 | `			}` |
|    41265 | 1878 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|        - | 1879 | `				/* Static member access,remember that */` |
|     2849 | 1880 | `				iP1 = 1;` |
|     2849 | 1881 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     2849 | 1882 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      399 | 1883 | `					p3 = pInstr->p3;` |
|        - | 1884 | ``					/* A `$`-form (`C::$s`, `C::$$x`, `C::${$e}`) is a STATIC PROPERTY`` |
|        - | 1885 | `					 * access, never a constant. A LITERAL name folds into p3 (non-zero)` |
|        - | 1886 | ``					 * and the exec side reads it there; a DYNAMIC name (`$$x`/`${$e}`)`` |
|        - | 1887 | `					 * leaves p3==0 with the computed name on the stack — the SAME shape` |
|        - | 1888 | ``					 * as a bareword constant `C::C`. Mark iP1=2 so exec still routes it`` |
|        - | 1889 | `					 * to the property table (hAttr), not the constant table (hConst). */` |
|      399 | 1890 | `					if( p3 == 0 ){` |
|        8 | 1891 | `						iP1 = 2;` |
|        3 | 1892 | `					}` |
|      399 | 1893 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|      197 | 1894 | `				}` |
|     1422 | 1895 | `			}` |
|        - | 1896 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|        - | 1897 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|        - | 1898 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|        - | 1899 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|    41265 | 1900 | `			if( iP2 == PH7_MEMBER_READ ){` |
|    41265 | 1901 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|       67 | 1902 | `					iP2 = PH7_MEMBER_UNSET;` |
|    41233 | 1903 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|      204 | 1904 | `					iP2 = PH7_MEMBER_ISSET;` |
|    41101 | 1905 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|       38 | 1906 | `					iP2 = PH7_MEMBER_EMPTY;` |
|    40984 | 1907 | `				}else if( iFlags & EXPR_FLAG_MEMBER_COALESCE ){` |
|      173 | 1908 | `					iP2 = PH7_MEMBER_COALESCE;` |
|    40883 | 1909 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|        - | 1910 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|     2009 | 1911 | `					iP2 = PH7_MEMBER_WRITE;` |
|    39797 | 1912 | `				}else if( iFlags & EXPR_FLAG_DEFER_ARG ){` |
|        - | 1913 | `					/* D1 commit 2: deferred by-ref/by-value property arg ($o->p). */` |
|     2063 | 1914 | `					iP2 = PH7_MEMBER_DEFPATH;` |
|     1029 | 1915 | `				}` |
|    20630 | 1916 | `			}` |
|    20630 | 1917 | `		}` |
|        - | 1918 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|        - | 1919 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|        - | 1920 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|        - | 1921 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|        - | 1922 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
|  3042349 | 1923 | `		if( bFcc ){` |
|      257 | 1924 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|        - | 1925 | `			/* php's global fallback applies to a first-class callable exactly as it does` |
|        - | 1926 | ``			 * to the call it stands for: inside a namespace, `strlen(...)` is the global`` |
|        - | 1927 | `			 * function when the current namespace has none. The callee's literal was` |
|        - | 1928 | `			 * namespace-qualified above and the arg map that records it is dropped here` |
|        - | 1929 | `			 * (an FCC has no arguments), so carry the one bit the resolution needs in the` |
|        - | 1930 | ``			 * instruction itself — without it `strlen(...)` in a namespaced file was`` |
|        - | 1931 | ``			 * `Call to undefined function A\B\strlen()`, a fatal on ordinary php. */`` |
|      257 | 1932 | `			iP2 = (p3 && ((VmCallArgMap *)p3)->bIsNamespaced) ? 1 : 0;` |
|      257 | 1933 | `			p3 = 0;` |
|      257 | 1934 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      257 | 1935 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER && pInstr->iP2 == PH7_MEMBER_METHOD ){` |
|        - | 1936 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|        - | 1937 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|        - | 1938 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|        - | 1939 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|      149 | 1940 | `				void *pMemberName = pInstr->p3;` |
|      149 | 1941 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|      149 | 1942 | `				if( pMemberName ){` |
|      ! 0 | 1943 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|      ! 0 | 1944 | `				}` |
|      149 | 1945 | `				iP1 = 2;` |
|       77 | 1946 | `			}else{` |
|        - | 1947 | `				/* Only a METHOD member is the callee's NAME. A parenthesised PROPERTY read` |
|        - | 1948 | ``				 * (`($o->cb)(...)`, `(C::$cb)(...)`) is php's variable-invocation: the member`` |
|        - | 1949 | `				 * op stays, its VALUE is the callable, and this is the iP1=1 wrap. Dropping it` |
|        - | 1950 | `				 * here read the property NAME as a method name and answered` |
|        - | 1951 | ``				 * `Call to undefined method H::cb()` for a closure the object was holding —`` |
|        - | 1952 | `				 * the CALL codegen above already made the distinction (it leaves the member a` |
|        - | 1953 | `				 * plain read for a parenthesised callee) and this branch undid it. */` |
|      111 | 1954 | `				iP1 = 1;` |
|        - | 1955 | `			}` |
|      126 | 1956 | `		}` |
|        - | 1957 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|        - | 1958 | `		 * This is the primary emit path for user-visible calls. */` |
|  3042349 | 1959 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|   786339 | 1960 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|   393167 | 1961 | `		}` |
|        - | 1962 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  3042349 | 1963 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  1521172 | 1964 | `	}` |
|  3043017 | 1965 | `	if( nJmpIdx > 0 ){` |
|        - | 1966 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   158777 | 1967 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   158777 | 1968 | `		if( pInstr ){` |
|   158777 | 1969 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    79386 | 1970 | `		}` |
|    79386 | 1971 | `	}` |
|  3043017 | 1972 | `	return rc;` |
|  4027767 | 1973 | `}` |
|        - | 1974 | `/*` |
|        - | 1975 | ` * Emit a call's ARGUMENT LIST, and decide everything about it the OP_CALL then carries:` |
|        - | 1976 | ` * the count, the unpack flag, the named-argument / assert-source / argument-shape map.` |
|        - | 1977 | ` *` |
|        - | 1978 | ` * Split out of GenStateEmitExprCode because php resolves a callee BEFORE it evaluates` |
|        - | 1979 | ` * the arguments, so this now runs AFTER the callee sub-tree has been emitted (the one` |
|        - | 1980 | `` * exception is a `new`'s constructor list — see EXPR_FLAG_NEW_CALLEE). It is otherwise`` |
|        - | 1981 | ` * the same code, and reads only the node: nothing here inspects the instructions the` |
|        - | 1982 | ` * callee left behind.` |
|        - | 1983 | ` *` |
|        - | 1984 | ` * pArgs->p3 may arrive non-NULL — the callee's own namespace qualification builds the` |
|        - | 1985 | ` * VmCallArgMap first now — and every allocation site below reuses it.` |
|        - | 1986 | ` */` |
|   699606 | 1987 | `static sxi32 GenStateEmitCallArgs(` |
|        - | 1988 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - | 1989 | `	ph7_expr_node *pNode, /* The call node */` |
|        - | 1990 | `	sxi32 iFlags,         /* Control flags of the call site */` |
|        - | 1991 | `	GenCallArgs *pArgs    /* OUT: what the OP_CALL needs */` |
|        - | 1992 | `	)` |
|        5 | 1993 | `{` |
|   699611 | 1994 | `	void *p3 = pArgs->p3;` |
|   699611 | 1995 | `	sxi32 iP1 = 0;` |
|   699611 | 1996 | `	sxu32 iP2 = 0;` |
|   699611 | 1997 | `	int bFcc = 0;` |
|        - | 1998 | `	sxi32 rc;` |
|        - | 1999 | `	ph7_expr_node **apNode;` |
|   699611 | 2000 | `	int hasSpread = 0;` |
|   699611 | 2001 | `	int hasNamed = 0;` |
|   699611 | 2002 | `	sxu32 byRefMask = 0;` |
|        - | 2003 | `	sxi32 nArgs;` |
|        - | 2004 | `	sxi32 n;` |
|   699611 | 2005 | `	int bAnySpread = 0;` |
|        - | 2006 | `	/* Recurse and generate bytecodes for function arguments */` |
|   699611 | 2007 | `	apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   699611 | 2008 | `	nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|        - | 2009 | ``	/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|        - | 2010 | `	 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|        - | 2011 | `	 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|   699611 | 2012 | `	if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|      257 | 2013 | `		bFcc = 1;` |
|      257 | 2014 | `		nArgs = 0;` |
|      126 | 2015 | `	}` |
|        - | 2016 | `	/* Validate argument order like php: no positional argument after a` |
|        - | 2017 | ``	 * named one OR after unpacking, and `name: ...$x` is a parse error. */`` |
|        - | 2018 | `	{` |
|   699611 | 2019 | `		int seenNamed = 0;` |
|   699611 | 2020 | `		int seenSpread = 0;` |
|  1643651 | 2021 | `		for( n = 0; n < nArgs; ++n ){` |
|   944047 | 2022 | `			if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|      274 | 2023 | `				bAnySpread = 1;` |
|      274 | 2024 | `				seenSpread = 1;` |
|      274 | 2025 | `				if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      ! 0 | 2026 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|        - | 2027 | `						"syntax error, unexpected token \"...\"");` |
|      ! 0 | 2028 | `					return SXERR_SYNTAX;` |
|        4 | 2029 | `				}` |
|   943912 | 2030 | `			}else if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      589 | 2031 | `				seenNamed = 1;` |
|      589 | 2032 | `				hasNamed = 1;` |
|   943485 | 2033 | `			}else if( seenNamed ){` |
|        3 | 2034 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|        - | 2035 | `					"Cannot use positional argument after named argument");` |
|        3 | 2036 | `				return SXERR_SYNTAX;` |
|   943191 | 2037 | `			}else if( seenSpread ){` |
|      ! 0 | 2038 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|        - | 2039 | `					"Cannot use positional argument after argument unpacking");` |
|      ! 0 | 2040 | `				return SXERR_SYNTAX;` |
|        - | 2041 | `			}` |
|   472025 | 2042 | `		}` |
|        - | 2043 | `	}` |
|        - | 2044 | `	/* Read-only load */` |
|   699609 | 2045 | `	iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|        - | 2046 | `	/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|        - | 2047 | ``	 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|        - | 2048 | `	 * objects dispatch to the right method (offsetExists for both;` |
|        - | 2049 | `	 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|   699609 | 2050 | `	if( pNode->pLeft && pNode->pLeft->pStart ){` |
|   699609 | 2051 | `		SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|  1075672 | 2052 | `		int bIsset = pCallName->nByte == 5` |
|   699604 | 2053 | `			&& SyStrnicmp(pCallName->zString,"isset",5) == 0;` |
|  1075672 | 2054 | `		int bEmpty = pCallName->nByte == 5` |
|   699604 | 2055 | `			&& SyStrnicmp(pCallName->zString,"empty",5) == 0;` |
|        - | 2056 | `		/* isset()/empty() are language CONSTRUCTS, not functions: php parses` |
|        - | 2057 | `		 * their argument list in the grammar and a missing operand is a parse` |
|        - | 2058 | `		 * error on the ')'. They compile through this ordinary call loop, which` |
|        - | 2059 | ``		 * never checked arity, so `empty()` quietly evaluated to true and`` |
|        - | 2060 | ``		 * `isset()` to false. (empty() also takes exactly one operand in php,`` |
|        - | 2061 | `		 * unlike isset(), which is variadic.) */` |
|   699609 | 2062 | `		if( (bIsset \|\| bEmpty) && nArgs < 1 ){` |
|        - | 2063 | `			/* php names the ')' itself as the unexpected token, so point at the` |
|        - | 2064 | `			 * node's last token rather than pGen->pIn (which has already moved` |
|        - | 2065 | `			 * past the call to the statement's ';'). */` |
|        5 | 2066 | `			SyToken *pTok = pNode->pEnd;` |
|        5 | 2067 | `			if( pTok && pTok > pNode->pStart && (pTok->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 | 2068 | `				pTok--;` |
|      ! 0 | 2069 | `			}` |
|        5 | 2070 | `			PH7_GenSyntaxError(&(*pGen),pTok,0);` |
|        5 | 2071 | `			return SXERR_ABORT;` |
|        - | 2072 | `		}` |
|   699605 | 2073 | `		if( bIsset ){` |
|    11241 | 2074 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|   693987 | 2075 | `		}else if( bEmpty ){` |
|      173 | 2076 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|       84 | 2077 | `		}` |
|        - | 2078 | `		/* Auto-vivify by-reference out-params of known builtins so an` |
|        - | 2079 | `		 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|        - | 2080 | `		 * $m never assigned) gets a real memobj slot for the builtin to` |
|        - | 2081 | `		 * write back through. Skipped when spread/named args are present:` |
|        - | 2082 | `		 * the compile-time positional index no longer maps to the` |
|        - | 2083 | `		 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|   699605 | 2084 | `		if( !bAnySpread && !hasNamed ){` |
|        - | 2085 | `			SyString sBuiltin;` |
|   699009 | 2086 | `			GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|   699009 | 2087 | `			byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|   349502 | 2088 | `		}` |
|   349800 | 2089 | `	}` |
|  1643641 | 2090 | `	for( n = 0 ; n < nArgs ; ++n ){` |
|   944043 | 2091 | `		sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   944043 | 2092 | `		sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|        - | 2093 | `		/* For a by-ref argument position, drop the read-only flag so the` |
|        - | 2094 | `		 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|        - | 2095 | `		 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|        - | 2096 | `		 * auto-vivifies its element and exposes a writable memobj slot for the` |
|        - | 2097 | `		 * builtin to write back through. A plain $var target is unaffected` |
|        - | 2098 | `		 * (iP1=0 either way). */` |
|   944043 | 2099 | `		if( n < 31 && (byRefMask & (1u<<n)) ){` |
|    16473 | 2100 | `			iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|    16473 | 2101 | `			iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|     8234 | 2102 | `		}` |
|        - | 2103 | ``		/* D1: a plain `$var` argument may bind to a by-ref parameter whose signature`` |
|        - | 2104 | `		 * is unknown at compile time (forward reference, dynamic call, or method` |
|        - | 2105 | ``		 * dispatch — e.g. PHPUnit's `willReturnReference($undef)`). We used to clear`` |
|        - | 2106 | `		 * the read-only flag here so an undefined variable vivified a real slot the` |
|        - | 2107 | `		 * by-ref write-back could reach — but that also invented the variable as NULL` |
|        - | 2108 | `		 * in the caller when the parameter turned out by-VALUE, and suppressed php's` |
|        - | 2109 | ``		 * `Undefined variable $x` warning. Instead mark it DEFERRED: OP_LOAD leaves an`` |
|        - | 2110 | `		 * undefined variable uncreated and carries a lazy-lvalue marker, and OP_CALL` |
|        - | 2111 | `		 * materializes it ONLY for a by-ref parameter once the callee is resolved` |
|        - | 2112 | `		 * (VmResolveDeferredArgs). Excludes isset()/empty()/unset(), which compile` |
|        - | 2113 | `		 * through this same call loop but must NEVER create their operand, and` |
|        - | 2114 | `		 * spread args (whose elements have no positional index of their own). A NAMED` |
|        - | 2115 | `		 * arg defers too: it binds to the formal its NAME picks, which the resolver` |
|        - | 2116 | `		 * looks up through the call's own argument map — excluding it left` |
|        - | 2117 | ``		 * `r(x: $a['new'])` warning `Undefined array key` and passing NULL where php`` |
|        - | 2118 | ``		 * creates the element for the by-ref parameter `$x`.`` |
|        - | 2119 | `		 *` |
|        - | 2120 | `		 * D1 commit 2: the same reasoning extends to an array-element ($a["k"]) or` |
|        - | 2121 | `		 * property ($o->p) argument — a by-ref user-function parameter must vivify the` |
|        - | 2122 | `		 * element/property, a by-value one must warn and NOT vivify. Those nodes carry a` |
|        - | 2123 | `		 * subscript/arrow operator (pOp != 0). The DEFER flag rides down to the base LOAD` |
|        - | 2124 | `		 * (undefined base auto-defers via commit 1) and to the LOAD_IDX/MEMBER, which` |
|        - | 2125 | ``		 * record the lvalue path on a lookup miss. Static `::` and nullsafe `?->` stay`` |
|        - | 2126 | `		 * eager. */` |
|   944038 | 2127 | `		if( (iFlags & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_LOAD_IDX_UNSET` |
|   472019 | 2128 | `		               \|EXPR_FLAG_MEMBER_COALESCE)) == 0` |
|   938323 | 2129 | `		 && (iArgFlags & EXPR_FLAG_RDONLY_LOAD) /* not a known builtin by-ref slot (kept eager above) */` |
|   924374 | 2130 | `		 && (apNode[n]->iFlags & EXPR_NODE_SPREAD) == 0` |
|   983495 | 2131 | `		 && ( (apNode[n]->pOp == 0 && apNode[n]->xCode == PH7_CompileVariable)` |
|   656970 | 2132 | `		   \|\| (apNode[n]->pOp != 0 && (apNode[n]->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|   140636 | 2133 | `		                            \|\| apNode[n]->pOp->iOp == EXPR_OP_ARROW)) ) ){` |
|   530901 | 2134 | `			iArgFlags \|= EXPR_FLAG_DEFER_ARG;` |
|   265448 | 2135 | `		}` |
|   944043 | 2136 | `		rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|   944043 | 2137 | `		if( rc != SXRET_OK ){` |
|        3 | 2138 | `			return rc;` |
|        - | 2139 | `		}` |
|        - | 2140 | `		/* Each argument is an independent nullsafe scope. */` |
|   944041 | 2141 | `		GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|   944041 | 2142 | `		if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|        - | 2143 | `			/* Emit spread opcode to unpack this array argument. iP1 marks a` |
|        - | 2144 | ``			 * source php will unpack BY REFERENCE: only a plain `$var` (php`` |
|        - | 2145 | ``			 * fetches every other shape — `$a[0]`, `$o->p`, `C::$s`, a cast, a`` |
|        - | 2146 | `			 * call — as an R-value, so a by-ref parameter binds its elements in` |
|        - | 2147 | `			 * a temporary and the write-back is invisible). The expander needs` |
|        - | 2148 | `			 * the distinction because it carries each element's slot for the` |
|        - | 2149 | ``			 * by-ref binder; without it `r(...$a[0])` wrote through to the real`` |
|        - | 2150 | `			 * element, which php leaves alone. */` |
|      379 | 2151 | `			PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD,` |
|      270 | 2152 | `				(apNode[n]->pOp == 0 && apNode[n]->xCode == PH7_CompileVariable) ? 1 : 0,` |
|        - | 2153 | `				0, 0, 0);` |
|      274 | 2154 | `			hasSpread = 1;` |
|      135 | 2155 | `		}` |
|   472023 | 2156 | `	}` |
|        - | 2157 | `	/* Total number of given arguments */` |
|   699603 | 2158 | `	iP1 = nArgs;` |
|   699603 | 2159 | `	iP2 = hasSpread;` |
|        - | 2160 | `	/* Build VmCallArgMap if named arguments are present.` |
|        - | 2161 | `	 * Deep-copy name strings so they survive token stream cleanup. */` |
|   699603 | 2162 | `	if( hasNamed ){` |
|      379 | 2163 | `		sxu32 nStrBytes = 0;` |
|        - | 2164 | `		char *zBuf;` |
|     1107 | 2165 | `		for( n = 0; n < nArgs; ++n ){` |
|      733 | 2166 | `			if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      587 | 2167 | `				nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|      291 | 2168 | `			}` |
|      369 | 2169 | `		}` |
|        - | 2170 | `		{` |
|      379 | 2171 | `		sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|      379 | 2172 | `		VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      374 | 2173 | `			&pGen->pVm->sAllocator, mapSize);` |
|      379 | 2174 | `		if( pMap ){` |
|      379 | 2175 | `			SyZero(pMap, mapSize);` |
|      379 | 2176 | `			pMap->bHasNamed = 1;` |
|      379 | 2177 | `			pMap->nTotal = (sxu32)nArgs;` |
|      379 | 2178 | `			pMap->aNames = (SyString *)&pMap[1];` |
|      379 | 2179 | `			zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|     1107 | 2180 | `			for( n = 0; n < nArgs; ++n ){` |
|      733 | 2181 | `				if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      587 | 2182 | `					sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|      587 | 2183 | `					SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|      587 | 2184 | `					SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|      587 | 2185 | `					zBuf += nb;` |
|      291 | 2186 | `				}` |
|        - | 2187 | `				/* else: aNames[n] remains {NULL, 0} for positional */` |
|      369 | 2188 | `			}` |
|      379 | 2189 | `			p3 = (void *)pMap;` |
|      187 | 2190 | `		}` |
|        - | 2191 | `		}` |
|      187 | 2192 | `	}` |
|        - | 2193 | `	/* assert(): php's compiler keeps a copy of the assertion's AST and a` |
|        - | 2194 | ``	 * failing assert reports its rendered SOURCE (`assert(1 == 2)`), not the`` |
|        - | 2195 | `	 * evaluated value. Render the first argument's token span` |
|        - | 2196 | `	 * into the call map so vm_builtin_assert can echo it. Only a DIRECT` |
|        - | 2197 | `	 * unqualified/absolute call qualifies — matching php, an indirect call` |
|        - | 2198 | `	 * (call_user_func, a callable variable) has no source text and its` |
|        - | 2199 | `	 * AssertionError carries an empty message. A spread first argument is` |
|        - | 2200 | `	 * skipped (its span is the unpacked array, not the assertion). */` |
|   699598 | 2201 | `	if( nArgs >= 1 && !bFcc` |
|   661159 | 2202 | `	 && (apNode[0]->iFlags & EXPR_NODE_SPREAD) == 0 ){` |
|        - | 2203 | `		SyString sCallee;` |
|   660945 | 2204 | `		GenStateCallBuiltinName(pNode->pLeft,&sCallee);` |
|   660940 | 2205 | `		if( sCallee.nByte == sizeof("assert")-1` |
|   411884 | 2206 | `		 && SyStrnicmp(sCallee.zString,"assert",sizeof("assert")-1) == 0 ){` |
|        - | 2207 | `			/* An operator root's pStart/pEnd name only the operator token` |
|        - | 2208 | ``			 * (`1 == 2` roots at `==`); the subtree walk recovers the whole`` |
|        - | 2209 | `			 * raw extent, re-adding parens the grouping pass consumed. */` |
|       67 | 2210 | `			SyToken *pSpanIn = 0;` |
|       67 | 2211 | `			SyToken *pSpanEnd = 0;` |
|        - | 2212 | `			SyBlob sSrc;` |
|       67 | 2213 | `			PH7_ExprSubtreeSpan(apNode[0],&pSpanIn,&pSpanEnd);` |
|       67 | 2214 | `			SyBlobInit(&sSrc,&pGen->pVm->sAllocator);` |
|       67 | 2215 | `			if( pSpanIn && pSpanEnd && pSpanIn < pSpanEnd ){` |
|       67 | 2216 | `				if( apNode[0]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|        - | 2217 | ``					/* php renders the name too: `assert(assertion: 1 == 2)`. */`` |
|        3 | 2218 | `					SyBlobAppend(&sSrc,apNode[0]->sArgName.zString,apNode[0]->sArgName.nByte);` |
|        3 | 2219 | `					SyBlobAppend(&sSrc,": ",2);` |
|        1 | 2220 | `				}` |
|       67 | 2221 | `				PH7_GenRenderAssertSpan(pGen,pSpanIn,pSpanEnd,&sSrc);` |
|       31 | 2222 | `			}` |
|       67 | 2223 | `			if( SyBlobLength(&sSrc) > 0 ){` |
|       98 | 2224 | `				char *zDup = (char *)SyMemBackendDup(&pGen->pVm->sAllocator,` |
|       62 | 2225 | `					SyBlobData(&sSrc),SyBlobLength(&sSrc));` |
|       67 | 2226 | `				if( zDup ){` |
|       67 | 2227 | `					if( p3 == 0 ){` |
|       65 | 2228 | `						VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|       60 | 2229 | `							&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|       65 | 2230 | `						if( pMap ){` |
|       65 | 2231 | `							SyZero(pMap,sizeof(VmCallArgMap));` |
|       65 | 2232 | `							p3 = (void *)pMap;` |
|       30 | 2233 | `						}` |
|       30 | 2234 | `					}` |
|       67 | 2235 | `					if( p3 ){` |
|       67 | 2236 | `						SyStringInitFromBuf(&((VmCallArgMap *)p3)->sAssertSrc,` |
|        - | 2237 | `							zDup,SyBlobLength(&sSrc));` |
|       31 | 2238 | `					}` |
|       31 | 2239 | `				}` |
|       31 | 2240 | `			}` |
|       67 | 2241 | `			SyBlobRelease(&sSrc);` |
|       31 | 2242 | `		}` |
|   330470 | 2243 | `	}` |
|        - | 2244 | `	/* Record each argument's compile-time SHAPE so the by-ref binders can` |
|        - | 2245 | `	 * refuse a non-variable where php refuses it — at the CALL, before the` |
|        - | 2246 | `	 * callee's ZPP runs. Skipped when the call SPREADS (one compile-time` |
|        - | 2247 | `	 * argument becomes N runtime slots, so the positions no longer line up)` |
|        - | 2248 | `	 * or when it carries more arguments than the masks can hold; a call` |
|        - | 2249 | `	 * without the flag keeps the old runtime nIdx test. Named arguments are` |
|        - | 2250 | `	 * fine: they change which FORMAL a slot binds to, not the slot's index. */` |
|   699603 | 2251 | `	if( !bAnySpread && nArgs > 0 && nArgs <= 31 && !bFcc ){` |
|   660903 | 2252 | `		sxu32 nNonLval = 0;` |
|   660903 | 2253 | `		sxu32 nTempCall = 0;` |
|  1604583 | 2254 | `		for( n = 0 ; n < nArgs ; ++n ){` |
|   943685 | 2255 | `			int iShape = GenStateArgShape(apNode[n]);` |
|   943685 | 2256 | `			if( iShape == GEN_ARG_NONE ){` |
|   340181 | 2257 | `				nNonLval \|= (1u << n);` |
|   773597 | 2258 | `			}else if( iShape == GEN_ARG_TEMPCALL ){` |
|    44689 | 2259 | `				nTempCall \|= (1u << n);` |
|    22342 | 2260 | `			}` |
|   471845 | 2261 | `		}` |
|   660903 | 2262 | `		if( p3 == 0 ){` |
|   660433 | 2263 | `			VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|   660428 | 2264 | `				&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|   660433 | 2265 | `			if( pMap ){` |
|   660433 | 2266 | `				SyZero(pMap,sizeof(VmCallArgMap));` |
|   660433 | 2267 | `				p3 = (void *)pMap;` |
|   330214 | 2268 | `			}` |
|   330214 | 2269 | `		}` |
|   660903 | 2270 | `		if( p3 ){` |
|   660903 | 2271 | `			((VmCallArgMap *)p3)->bArgShapes = 1;` |
|   660903 | 2272 | `			((VmCallArgMap *)p3)->nNonLvalMask = nNonLval;` |
|   660903 | 2273 | `			((VmCallArgMap *)p3)->nTempCallMask = nTempCall;` |
|   330449 | 2274 | `		}` |
|   330449 | 2275 | `	}` |
|   699603 | 2276 | `	pArgs->iP1 = iP1;` |
|   699603 | 2277 | `	pArgs->iP2 = iP2;` |
|   699603 | 2278 | `	pArgs->p3  = p3;` |
|   699603 | 2279 | `	pArgs->bFcc = bFcc;` |
|   699603 | 2280 | `	pArgs->bAnySpread = bAnySpread;` |
|   699603 | 2281 | `	return SXRET_OK;` |
|   349808 | 2282 | `}` |
|        - | 2283 | `/*` |
|        - | 2284 | ` * Compile a PHP expression.` |
|        - | 2285 | ` * According to the PHP language reference manual:` |
|        - | 2286 | ` *  Expressions are the most important building stones of PHP.` |
|        - | 2287 | ` *  In PHP, almost anything you write is an expression.` |
|        - | 2288 | ` *  The simplest yet most accurate way to define an expression` |
|        - | 2289 | ` *  is "anything that has a value".` |
|        - | 2290 | ` * If something goes wrong while compiling the expression,this` |
|        - | 2291 | ` * function takes care of generating the appropriate error` |
|        - | 2292 | ` * message.` |
|        - | 2293 | ` */` |
|        - | 2294 | `/*` |
|        - | 2295 | ` * Does this expression tree contain a comma OPERATOR node?` |
|        - | 2296 | ` *` |
|        - | 2297 | `` * PH7 shipped `,` as a lowest-precedence binary operator (IMP-0139-COMMA), so`` |
|        - | 2298 | `` * `(1, 2)` and `$x = (f(), $y)` compile and evaluate to the right operand.`` |
|        - | 2299 | ` * php 8 has no comma operator: its grammar only allows comma-separated` |
|        - | 2300 | ` * expression LISTS inside for(...) clauses (call arguments, array literals and` |
|        - | 2301 | ` * list() are split by the parser, never by this node). Accepting it changes the` |
|        - | 2302 | ` * meaning of source php rejects, which §10 classes as a bug — so every context` |
|        - | 2303 | ` * except for() now reports php's parse error.` |
|        - | 2304 | ` */` |
| 26290936 | 2305 | `static int GenStateTreeHasComma(ph7_expr_node *pNode)` |
|        5 | 2306 | `{` |
|        - | 2307 | `	ph7_expr_node **apArg;` |
|        - | 2308 | `	sxu32 n;` |
| 26290941 | 2309 | `	if( pNode == 0 ){` |
| 18547275 | 2310 | `		return 0;` |
|        - | 2311 | `	}` |
|  7743671 | 2312 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|        6 | 2313 | `		return 1;` |
|        - | 2314 | `	}` |
|  7743662 | 2315 | `	if( GenStateTreeHasComma(pNode->pLeft) \|\| GenStateTreeHasComma(pNode->pRight)` |
|  7743663 | 2316 | `	 \|\| GenStateTreeHasComma(pNode->pCond) ){` |
|        6 | 2317 | `		return 1;` |
|        - | 2318 | `	}` |
|  7743663 | 2319 | `	apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  8851275 | 2320 | `	for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; n++ ){` |
|  1107617 | 2321 | `		if( GenStateTreeHasComma(apArg[n]) ){` |
|      ! 0 | 2322 | `			return 1;` |
|        - | 2323 | `		}` |
|   553811 | 2324 | `	}` |
|  7743663 | 2325 | `	return 0;` |
| 13145473 | 2326 | `}` |
|  2047966 | 2327 | `PH7_PRIVATE sxi32 PH7_CompileExpr(` |
|        - | 2328 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 2329 | `	sxi32 iFlags,        /* Control flags */` |
|        - | 2330 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|        - | 2331 | `	)` |
|        5 | 2332 | `{` |
|        - | 2333 | `	ph7_expr_node *pRoot;` |
|        - | 2334 | `	SySet sExprNode;` |
|        - | 2335 | `	SyToken *pEnd;` |
|        - | 2336 | `	sxi32 nExpr;` |
|        - | 2337 | `	sxi32 iNest;` |
|        - | 2338 | `	sxi32 rc;` |
|        - | 2339 | `	sxu32 nNullsafeBase;` |
|        - | 2340 | `	/* Initialize worker variables */` |
|  2047971 | 2341 | `	nExpr = 0;` |
|  2047971 | 2342 | `	pRoot = 0;` |
|        - | 2343 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|        - | 2344 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  2047971 | 2345 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  2047971 | 2346 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  2047971 | 2347 | `	SySetAlloc(&sExprNode,0x10);` |
|  2047971 | 2348 | `	rc = SXRET_OK;` |
|        - | 2349 | `	/* Delimit the expression */` |
|  2047971 | 2350 | `	pEnd = pGen->pIn;` |
|  2047971 | 2351 | `	iNest = 0;` |
| 14950943 | 2352 | `	while( pEnd < pGen->pEnd ){` |
| 14061583 | 2353 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|        - | 2354 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     5065 | 2355 | `			iNest++;` |
| 14059053 | 2356 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     5075 | 2357 | `			iNest--;` |
| 14053988 | 2358 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  1167973 | 2359 | `			if( iNest <= 0 ){` |
|  1158611 | 2360 | `				break;` |
|        - | 2361 | `			}` |
|     4681 | 2362 | `		}` |
| 12902977 | 2363 | `		pEnd++;` |
|        5 | 2364 | `	}` |
|  2047971 | 2365 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|     2323 | 2366 | `		SyToken *pEnd2 = pGen->pIn;` |
|     2323 | 2367 | `		iNest = 0;` |
|        - | 2368 | `		/* Stop at the first comma */` |
|    18211 | 2369 | `		while( pEnd2 < pEnd ){` |
|    15911 | 2370 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|      293 | 2371 | `				iNest++;` |
|    15767 | 2372 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|      293 | 2373 | `				iNest--;` |
|    15479 | 2374 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|     6129 | 2375 | `				if( iNest <= 0 ){` |
|       21 | 2376 | `					break;` |
|        - | 2377 | `				}` |
|     3053 | 2378 | `			}` |
|    15893 | 2379 | `			pEnd2++;` |
|        5 | 2380 | `		}` |
|     2323 | 2381 | `		if( pEnd2 <pEnd ){` |
|       21 | 2382 | `			pEnd = pEnd2;` |
|        9 | 2383 | `		}` |
|     1159 | 2384 | `	}` |
|  2047971 | 2385 | `	if( pEnd > pGen->pIn ){` |
|  2047957 | 2386 | `		SyToken *pTmp = pGen->pEnd;` |
|        - | 2387 | `		/* Swap delimiter */` |
|  2047957 | 2388 | `		pGen->pEnd = pEnd;` |
|        - | 2389 | `		/* Try to get an expression tree */` |
|  2047957 | 2390 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  2047952 | 2391 | `		if( rc == SXRET_OK && pRoot && pGen->nCommaExprOk < 1` |
|  2000059 | 2392 | `		 && GenStateTreeHasComma(pRoot) ){` |
|        - | 2393 | `			/* php has no comma operator outside a for() clause */` |
|        6 | 2394 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pRoot->pStart->nLine,` |
|        - | 2395 | `				"syntax error, unexpected token \",\"");` |
|        6 | 2396 | `			pGen->pEnd = pTmp;` |
|        6 | 2397 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2398 | `				SySetRelease(&sExprNode);` |
|      ! 0 | 2399 | `				return SXERR_ABORT;` |
|        - | 2400 | `			}` |
|        6 | 2401 | `			pGen->pIn = pEnd;` |
|        6 | 2402 | `			SySetRelease(&sExprNode);` |
|        6 | 2403 | `			SySetTruncate(&pGen->aNullsafeJmp,nNullsafeBase);` |
|        6 | 2404 | `			return SXRET_OK;` |
|        - | 2405 | `		}` |
|  2047953 | 2406 | `		if( rc == SXRET_OK && pRoot ){` |
|  2047763 | 2407 | `			rc = SXRET_OK;` |
|  2047763 | 2408 | `			if( xTreeValidator ){` |
|        - | 2409 | `				/* Call the upper layer validator callback */` |
|   156857 | 2410 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    78426 | 2411 | `			}` |
|  2047763 | 2412 | `			if( rc != SXERR_ABORT ){` |
|        - | 2413 | `				/* Generate code for the given tree */` |
|  2047763 | 2414 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|        - | 2415 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|        - | 2416 | `				 * expression so they short-circuit to its end. */` |
|  2047763 | 2417 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|  1023879 | 2418 | `			}` |
|  2047763 | 2419 | `			nExpr = 1;` |
|  1023879 | 2420 | `		}` |
|        - | 2421 | `		/* Release the whole tree */` |
|  2047953 | 2422 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|        - | 2423 | `		/* Synchronize token stream */` |
|  2047953 | 2424 | `		pGen->pEnd = pTmp;` |
|  2047953 | 2425 | `		pGen->pIn  = pEnd;` |
|  2047953 | 2426 | `		if( rc == SXERR_ABORT ){` |
|       59 | 2427 | `			SySetRelease(&sExprNode);` |
|       59 | 2428 | `			return SXERR_ABORT;` |
|        - | 2429 | `		}` |
|  1023947 | 2430 | `	}` |
|  2047913 | 2431 | `	SySetRelease(&sExprNode);` |
|  2047913 | 2432 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  1023988 | 2433 | `}` |
|        - | 2434 | `/*` |
|        - | 2435 | ` * Return a pointer to the node construct handler associated` |
|        - | 2436 | ` * with a given node type [i.e: string,integer,float,...].` |
|        - | 2437 | ` */` |
|  1171810 | 2438 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|        5 | 2439 | `{` |
|  1171815 | 2440 | `	if( nNodeType & PH7_TK_NUM ){` |
|        - | 2441 | `		/* Numeric literal: Either real or integer */` |
|   710223 | 2442 | `		return PH7_CompileNumLiteral;` |
|   461597 | 2443 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|        - | 2444 | `		/* Double quoted string */` |
|    53157 | 2445 | `		return PH7_CompileString;` |
|   408445 | 2446 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|        - | 2447 | `		/* Single quoted string */` |
|   408311 | 2448 | `		return PH7_CompileSimpleString;` |
|      139 | 2449 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|        - | 2450 | `		/* Heredoc */` |
|       80 | 2451 | `		return PH7_CompileHereDoc;` |
|       62 | 2452 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|        - | 2453 | `		/* Nowdoc */` |
|       58 | 2454 | `		return PH7_CompileNowDoc;` |
|        5 | 2455 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|        - | 2456 | `		/* Backtick quoted string */` |
|        3 | 2457 | `		return PH7_CompileBacktic;` |
|        - | 2458 | `	}` |
|        3 | 2459 | `	return 0;` |
|   585910 | 2460 | `}` |
|        - | 2461 | `/*` |
|        - | 2462 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|        - | 2463 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|        - | 2464 | ` * in write context" parse error.` |
|        - | 2465 | ` */` |
|      254 | 2466 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|        5 | 2467 | `{` |
|        - | 2468 | `	sxi32 rc;` |
|      259 | 2469 | `	rc = GenStateWriteTargetCheck(&(*pGen),pNode,1 /* unset wording for $this */);` |
|      259 | 2470 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 2471 | `		return rc;` |
|        - | 2472 | `	}` |
|      259 | 2473 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|      257 | 2474 | `		return SXRET_OK;` |
|        - | 2475 | `	}` |
|        5 | 2476 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|        2 | 2477 | `		pNode ? pNode->pStart->nLine : 1,` |
|        - | 2478 | `		"Can't use nullsafe operator in write context");` |
|        3 | 2479 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|      132 | 2480 | `}` |
|        - | 2481 | `/*` |
|        - | 2482 | ` * Compile an unset() statement.` |
|        - | 2483 | ` * unset($var, $arr[$key], ...);` |
|        - | 2484 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|        - | 2485 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|        - | 2486 | ` * parent array before extracting the element to unset.` |
|        - | 2487 | ` */` |
|     3216 | 2488 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|        5 | 2489 | `{` |
|     3221 | 2490 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|     3221 | 2491 | `	sxu32 nIdx = 0;` |
|        - | 2492 | `	SyString sName;` |
|        - | 2493 | `	sxi32 rc;` |
|        - | 2494 | `	/* Jump the 'unset' keyword */` |
|     3221 | 2495 | `	pGen->pIn++;` |
|        - | 2496 | `	/* Save delimiter */` |
|     3221 | 2497 | `	pTmp = pGen->pEnd;` |
|        - | 2498 | `	/* Skip optional opening parenthesis and find the matching close */` |
|     3221 | 2499 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|     3221 | 2500 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        - | 2501 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|        - | 2502 | `		SyToken *pClose;` |
|     3221 | 2503 | `		pGen->pIn++;   /* Skip '(' */` |
|     3221 | 2504 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|     3221 | 2505 | `		pEnd = pClose; /* Stop at ')' */` |
|     1608 | 2506 | `	}` |
|     3221 | 2507 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|        - | 2508 | `	/* Resolve the 'unset' builtin name once */` |
|     3221 | 2509 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|      513 | 2510 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      513 | 2511 | `		if( pObj == 0 ){` |
|      ! 0 | 2512 | `			return SXERR_ABORT;` |
|        - | 2513 | `		}` |
|      513 | 2514 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      513 | 2515 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      254 | 2516 | `	}` |
|        - | 2517 | `	/* Compile each comma-separated argument */` |
|    11071 | 2518 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|     7857 | 2519 | `		if( pGen->pIn < pNext ){` |
|        - | 2520 | `			/*` |
|        - | 2521 | ``			 * A PLAIN variable (`unset($x)`, exactly two tokens: '$' and the name) drops a`` |
|        - | 2522 | `			 * single NAME binding, which the unset() builtin cannot express — all it ever` |
|        - | 2523 | `			 * receives is the value's slot index, and unsetting the slot destroys whatever` |
|        - | 2524 | `			 * else aliases it. Emit OP_UNSET_VAR with the name instead. Subscripts and` |
|        - | 2525 | ``			 * properties (`unset($a[k])`, `unset($o->p)`) keep the existing path, which`` |
|        - | 2526 | `			 * already removes just the element/property.` |
|        - | 2527 | `			 */` |
|     7852 | 2528 | `			if( &pGen->pIn[2] == pNext` |
|     7725 | 2529 | `				&& (pGen->pIn[0].nType & PH7_TK_DOLLAR)` |
|     7603 | 2530 | `				&& (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        - | 2531 | `				SyString *pVarName;` |
|        - | 2532 | ``				/* php refuses `unset($this)` where it is written. The tree validator`` |
|        - | 2533 | `				 * cannot see it — this fast path never builds a tree. */` |
|     7596 | 2534 | `				if( pGen->pIn[1].sData.nByte == sizeof("this")-1` |
|     4034 | 2535 | `				 && SyMemcmp((const void *)pGen->pIn[1].sData.zString,` |
|      231 | 2536 | `				             (const void *)"this",sizeof("this")-1) == 0 ){` |
|        3 | 2537 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 2538 | `						"Cannot unset $this");` |
|        3 | 2539 | `					return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 2540 | `				}` |
|    11396 | 2541 | `				char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     7594 | 2542 | `					pGen->pIn[1].sData.zString,pGen->pIn[1].sData.nByte);` |
|     7599 | 2543 | `				pVarName = (SyString *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(SyString));` |
|     7599 | 2544 | `				if( zDup == 0 \|\| pVarName == 0 ){` |
|      ! 0 | 2545 | `					PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 2546 | `						"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2547 | `					return SXERR_ABORT;` |
|        - | 2548 | `				}` |
|     7599 | 2549 | `				SyStringInitFromBuf(pVarName,zDup,pGen->pIn[1].sData.nByte);` |
|     7599 | 2550 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_UNSET_VAR,0,0,pVarName,0);` |
|     7599 | 2551 | `				pGen->pIn = pNext;` |
|     7599 | 2552 | `				if( pGen->pIn < pEnd ){` |
|     4621 | 2553 | `					pGen->pIn++; /* Jump the trailing comma */` |
|     2308 | 2554 | `				}` |
|     7599 | 2555 | `				continue;` |
|        - | 2556 | `			}` |
|      261 | 2557 | `			pGen->pEnd = pNext;` |
|      261 | 2558 | `			rc = PH7_CompileExpr(&(*pGen),` |
|        - | 2559 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|        - | 2560 | `				GenStateUnsetValidator);` |
|      261 | 2561 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2562 | `				return SXERR_ABORT;` |
|        - | 2563 | `			}` |
|      261 | 2564 | `			if( rc != SXERR_EMPTY ){` |
|        - | 2565 | `				/* Emit call for this single argument */` |
|      259 | 2566 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      259 | 2567 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|      259 | 2568 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      127 | 2569 | `			}` |
|      128 | 2570 | `		}` |
|        - | 2571 | `		/* Jump trailing commas */` |
|      283 | 2572 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|       25 | 2573 | `			pNext++;` |
|        3 | 2574 | `		}` |
|      261 | 2575 | `		pGen->pIn = pNext;` |
|        5 | 2576 | `	}` |
|        - | 2577 | `	/* Skip past the closing ')' if present */` |
|     3219 | 2578 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|     3219 | 2579 | `		pGen->pIn++;` |
|     1607 | 2580 | `	}` |
|        - | 2581 | `	/* Restore token stream */` |
|     3219 | 2582 | `	pGen->pEnd = pTmp;` |
|     3219 | 2583 | `	return SXRET_OK;` |
|     1613 | 2584 | `}` |
|        - | 2585 | `/*` |
|        - | 2586 | ` * PHP Language construct table.` |
|        - | 2587 | ` */` |
|        - | 2588 | `static const LangConstruct aLangConstruct[] = {` |
|        - | 2589 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|        - | 2590 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|        - | 2591 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|        - | 2592 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|        - | 2593 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|        - | 2594 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|        - | 2595 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|        - | 2596 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|        - | 2597 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|        - | 2598 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|        - | 2599 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|        - | 2600 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|        - | 2601 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|        - | 2602 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|        - | 2603 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|        - | 2604 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|        - | 2605 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|        - | 2606 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|        - | 2607 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|        - | 2608 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|        - | 2609 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|        - | 2610 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|        - | 2611 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|        - | 2612 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|        - | 2613 | `};` |
|        - | 2614 | `/*` |
|        - | 2615 | ` * Return a pointer to the statement handler routine associated` |
|        - | 2616 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|        - | 2617 | ` */` |
|  1005180 | 2618 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|        - | 2619 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|        - | 2620 | `	SyToken *pLookahed  /* Look-ahead token */` |
|        - | 2621 | `	)` |
|        5 | 2622 | `{` |
|  1005185 | 2623 | `	sxu32 n = 0;` |
|  3053455 | 2624 | `	for(;;){` |
|  6106915 | 2625 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|     4449 | 2626 | `			break;` |
|        - | 2627 | `		}` |
|  6102471 | 2628 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  1000741 | 2629 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|      ! 0 | 2630 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|      ! 0 | 2631 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|        - | 2632 | `					/* 'static' (class context),return null */` |
|      ! 0 | 2633 | `					return 0;` |
|        - | 2634 | `				}` |
|      ! 0 | 2635 | `			}` |
|  1000736 | 2636 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       32 | 2637 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       23 | 2638 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|        - | 2639 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|        3 | 2640 | `				return 0;` |
|        - | 2641 | `			}` |
|        - | 2642 | `			/* Return a pointer to the handler.` |
|        - | 2643 | `			*/` |
|  1000739 | 2644 | `			return aLangConstruct[n].xConstruct;` |
|        - | 2645 | `		}` |
|  5101735 | 2646 | `		n++;` |
|        5 | 2647 | `	}` |
|     4449 | 2648 | `	if( pLookahed ){` |
|     4449 | 2649 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|      193 | 2650 | `			return PH7_CompileClassInterface;` |
|     4261 | 2651 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|     3301 | 2652 | `			return PH7_CompileClass;` |
|      965 | 2653 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      197 | 2654 | `			return PH7_CompileTrait;` |
|        - | 2655 | `		}` |
|        - | 2656 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|        - | 2657 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|        - | 2658 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|        - | 2659 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|      384 | 2660 | `	}` |
|        - | 2661 | `	/* Not a language construct */` |
|      773 | 2662 | `	return 0;` |
|   502595 | 2663 | `}` |
|        - | 2664 | `/*` |
|        - | 2665 | ` * Check if the given keyword is in fact a PHP language construct.` |
|        - | 2666 | ` * Return TRUE on success. FALSE otheriwse.` |
|        - | 2667 | ` */` |
|      770 | 2668 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|        5 | 2669 | `{` |
|        - | 2670 | `	int rc;` |
|      775 | 2671 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|      775 | 2672 | `	if( rc == FALSE ){` |
|      558 | 2673 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|      456 | 2674 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|        - | 2675 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|        - | 2676 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|        - | 2677 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|        - | 2678 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|        - | 2679 | `			*/` |
|        - | 2680 | `			){` |
|      555 | 2681 | `				rc = TRUE;` |
|      275 | 2682 | `		}` |
|      279 | 2683 | `	}` |
|      775 | 2684 | `	return rc;` |
|        5 | 2685 | `}` |
|        - | 2686 | `/*` |
|        - | 2687 | ` * Compile a PHP chunk.` |
|        - | 2688 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|        - | 2689 | ` * takes care of generating the appropriate error message.` |
|        - | 2690 | ` */` |
|        - | 2691 | `/*` |
|        - | 2692 | ` * Update pGen->sPendingDoc for the statement whose first token is` |
|        - | 2693 | ` * pGen->pIn: when a docblock trivia is keyed to that token's index in` |
|        - | 2694 | ` * the chunk token set it becomes the pending docblock. An existing` |
|        - | 2695 | ` * pending docblock is LEFT in place otherwise: Zend keeps the last-seen` |
|        - | 2696 | ` * doc comment until a declaration consumes it, so a docblock survives` |
|        - | 2697 | ` * intervening non-declaration statements.` |
|        - | 2698 | ` */` |
|  1782676 | 2699 | `PH7_PRIVATE void GenStateSetPendingDoc(ph7_gen_state *pGen)` |
|        5 | 2700 | `{` |
|  1782681 | 2701 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|  1782681 | 2702 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|  1782681 | 2703 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|        - | 2704 | `	sxu32 nIdx, n;` |
|  1782676 | 2705 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|     4527 | 2706 | `	 \|\| pGen->pIn < pBase \|\| pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|        - | 2707 | `		/* Re-tokenized substream (string interpolation, synthesized code):` |
|        - | 2708 | `		 * indexes do not map to the sidecar */` |
|  1778159 | 2709 | `		return;` |
|        - | 2710 | `	}` |
|     4527 | 2711 | `	nIdx = (sxu32)(pGen->pIn - pBase);` |
|        - | 2712 | `	/* Attributes must be adjacent to their declaration (unlike docblocks):` |
|        - | 2713 | `	 * reset at every boundary, then collect the groups keyed to this token. */` |
|     4527 | 2714 | `	SySetReset(&pGen->aPendingAttrs);` |
|    15439 | 2715 | `	for( n = 0 ; n < nT ; n++ ){` |
|    10917 | 2716 | `		if( aT[n].nTokIdx != nIdx ){` |
|    10639 | 2717 | `			continue;` |
|        - | 2718 | `		}` |
|      283 | 2719 | `		if( aT[n].iKind == PH7_TRIVIA_DOC ){` |
|       65 | 2720 | `			pGen->sPendingDoc = aT[n].sText;` |
|      253 | 2721 | `		}else if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|      223 | 2722 | `			SySetPut(&pGen->aPendingAttrs,(const void *)&aT[n]);` |
|      109 | 2723 | `		}` |
|      144 | 2724 | `	}` |
|   891343 | 2725 | `}` |
|        - | 2726 | `/*` |
|        - | 2727 | ` * Hand the pending docblock (if any) to a declaration: duplicate it into` |
|        - | 2728 | ` * the VM allocator (the raw script buffer dies after compilation) and` |
|        - | 2729 | ` * clear the pending slot so sibling declarations do not inherit it.` |
|        - | 2730 | ` */` |
|   163652 | 2731 | `PH7_PRIVATE void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut)` |
|        5 | 2732 | `{` |
|        - | 2733 | `	char *zDup;` |
|   163657 | 2734 | `	if( SyStringLength(&pGen->sPendingDoc) < 1 ){` |
|   163601 | 2735 | `		return;` |
|        - | 2736 | `	}` |
|       89 | 2737 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       28 | 2738 | `		SyStringData(&pGen->sPendingDoc),SyStringLength(&pGen->sPendingDoc));` |
|       61 | 2739 | `	if( zDup ){` |
|       61 | 2740 | `		SyStringInitFromBuf(pOut,zDup,SyStringLength(&pGen->sPendingDoc));` |
|       28 | 2741 | `	}` |
|       61 | 2742 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|    81831 | 2743 | `}` |
|        - | 2744 | `/*` |
|        - | 2745 | ` * Compile one recorded #[...] attribute group (the span between the group` |
|        - | 2746 | ` * delimiters) into ph7_attribute records appended to pOut. The span is` |
|        - | 2747 | ` * duplicated into the VM allocator FIRST (compiled bytecode and interned` |
|        - | 2748 | ` * names may point into the token text, which must outlive the raw script` |
|        - | 2749 | ` * buffer), then re-tokenized on its own. Each argument expression compiles` |
|        - | 2750 | ` * with the container-swap idiom into its own OP_DONE-terminated set,` |
|        - | 2751 | ` * evaluated lazily at ReflectionAttribute time (PHP semantics).` |
|        - | 2752 | ` */` |
|      232 | 2753 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut)` |
|        5 | 2754 | `{` |
|        - | 2755 | `	SySet *pToken;` |
|        - | 2756 | `	SyToken *pIn, *pEnd, *pSavedIn, *pSavedEnd;` |
|        - | 2757 | `	char *zSpan;` |
|      237 | 2758 | `	sxi32 rc = SXRET_OK;` |
|      237 | 2759 | `	if( SyStringLength(&pTrivia->sText) < 1 ){` |
|      ! 0 | 2760 | `		return SXRET_OK;` |
|        - | 2761 | `	}` |
|      353 | 2762 | `	zSpan = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      116 | 2763 | `		SyStringData(&pTrivia->sText),SyStringLength(&pTrivia->sText));` |
|      237 | 2764 | `	if( zSpan == 0 ){` |
|      ! 0 | 2765 | `		return SXRET_OK;` |
|        - | 2766 | `	}` |
|        - | 2767 | `	/* The token set must outlive compilation too: interned operands may` |
|        - | 2768 | `	 * reference token payloads. Pool-allocated, never released — bounded by` |
|        - | 2769 | `	 * the number of attribute declarations in the program. */` |
|      237 | 2770 | `	pToken = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      237 | 2771 | `	if( pToken == 0 ){` |
|      ! 0 | 2772 | `		return SXRET_OK;` |
|        - | 2773 | `	}` |
|      237 | 2774 | `	SySetInit(pToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      237 | 2775 | `	PH7_TokenizePHP(zSpan,SyStringLength(&pTrivia->sText),pTrivia->nLine,pToken,0);` |
|      237 | 2776 | `	pIn = (SyToken *)SySetBasePtr(pToken);` |
|      237 | 2777 | `	pEnd = &pIn[SySetUsed(pToken)];` |
|      237 | 2778 | `	pSavedIn = pGen->pIn;` |
|      237 | 2779 | `	pSavedEnd = pGen->pEnd;` |
|      241 | 2780 | `	while( pIn < pEnd ){` |
|        - | 2781 | `		ph7_attribute sAttr;` |
|        - | 2782 | `		SyBlob sFQN;` |
|      241 | 2783 | `		int bAbsolute = 0;` |
|      241 | 2784 | `		SyZero(&sAttr,sizeof(sAttr));` |
|      241 | 2785 | `		SySetInit(&sAttr.aArgs,&pGen->pVm->sAllocator,sizeof(ph7_attr_arg));` |
|      241 | 2786 | `		sAttr.nLine = pIn->nLine;` |
|      241 | 2787 | `		if( pIn->nType & PH7_TK_NSSEP ){` |
|       89 | 2788 | `			bAbsolute = 1;` |
|       89 | 2789 | `			pIn++;` |
|       42 | 2790 | `		}` |
|      241 | 2791 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        - | 2792 | ``		/* `#[namespace\Attr]` — the current namespace, absolute from there. */`` |
|      241 | 2793 | `		if( !bAbsolute && GenStateNsRelPrefix(pGen,&pIn,pEnd,&sFQN) ){` |
|      ! 0 | 2794 | `			bAbsolute = 1;` |
|      ! 0 | 2795 | `		}` |
|      241 | 2796 | `		while( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      241 | 2797 | `			SyBlobAppend(&sFQN,pIn->sData.zString,pIn->sData.nByte);` |
|      241 | 2798 | `			pIn++;` |
|      241 | 2799 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|      ! 0 | 2800 | `				SyBlobAppend(&sFQN,"\\",1);` |
|      ! 0 | 2801 | `				pIn++;` |
|      ! 0 | 2802 | `				continue;` |
|        - | 2803 | `			}` |
|      241 | 2804 | `			break;` |
|      ! 0 | 2805 | `		}` |
|      241 | 2806 | `		if( SyBlobLength(&sFQN) < 1 ){` |
|        - | 2807 | `			/* Malformed group: stop quietly (the group was inert trivia before` |
|        - | 2808 | `			 * this feature; never turn it into a new fatal) */` |
|      ! 0 | 2809 | `			SyBlobRelease(&sFQN);` |
|      ! 0 | 2810 | `			break;` |
|        - | 2811 | `		}` |
|        - | 2812 | `		/* Resolve to an FQN: absolute names verbatim; else use-import alias,` |
|        - | 2813 | `		 * else current-namespace prefix (PHP attribute name resolution) */` |
|        - | 2814 | `		{` |
|      241 | 2815 | `			const char *zName = (const char *)SyBlobData(&sFQN);` |
|      241 | 2816 | `			sxu32 nName = SyBlobLength(&sFQN);` |
|      241 | 2817 | `			char *zDup = 0;` |
|      241 | 2818 | `			if( !bAbsolute ){` |
|      154 | 2819 | `				SyHashEntry *pImp = SyHashGet(&pGen->hUseImports,(const void *)zName,nName);` |
|      154 | 2820 | `				if( pImp ){` |
|        3 | 2821 | `					const char *zFqn = (const char *)pImp->pUserData;` |
|        3 | 2822 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFqn,SyStrlen(zFqn));` |
|        3 | 2823 | `					if( zDup ){` |
|        3 | 2824 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyStrlen(zDup));` |
|        2 | 2825 | `					}` |
|      153 | 2826 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        - | 2827 | `					SyBlob sTmp;` |
|      ! 0 | 2828 | `					SyBlobInit(&sTmp,&pGen->pVm->sAllocator);` |
|      ! 0 | 2829 | `					SyBlobAppend(&sTmp,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|      ! 0 | 2830 | `					SyBlobAppend(&sTmp,"\\",1);` |
|      ! 0 | 2831 | `					SyBlobAppend(&sTmp,zName,nName);` |
|      ! 0 | 2832 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      ! 0 | 2833 | `						(const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|      ! 0 | 2834 | `					if( zDup ){` |
|      ! 0 | 2835 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyBlobLength(&sTmp));` |
|      ! 0 | 2836 | `					}` |
|      ! 0 | 2837 | `					SyBlobRelease(&sTmp);` |
|      ! 0 | 2838 | `				}` |
|       76 | 2839 | `			}` |
|      241 | 2840 | `			if( SyStringLength(&sAttr.sName) < 1 ){` |
|      239 | 2841 | `				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|      239 | 2842 | `				if( zDup ){` |
|      239 | 2843 | `					SyStringInitFromBuf(&sAttr.sName,zDup,nName);` |
|      117 | 2844 | `				}` |
|      117 | 2845 | `			}` |
|        - | 2846 | `		}` |
|      241 | 2847 | `		SyBlobRelease(&sFQN);` |
|      241 | 2848 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|        - | 2849 | `			SyToken *pArgsEnd;` |
|       94 | 2850 | `			pIn++;` |
|       94 | 2851 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pArgsEnd);` |
|      260 | 2852 | `			while( pIn < pArgsEnd ){` |
|      168 | 2853 | `				SyToken *pArgStart = pIn, *pArgStop = pIn;` |
|      168 | 2854 | `				sxi32 iDepth = 0;` |
|        - | 2855 | `				ph7_attr_arg sArgRec;` |
|      576 | 2856 | `				while( pArgStop < pArgsEnd ){` |
|      486 | 2857 | `					if( pArgStop->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       29 | 2858 | `						iDepth++;` |
|      472 | 2859 | `					}else if( pArgStop->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       29 | 2860 | `						iDepth--;` |
|      444 | 2861 | `					}else if( (pArgStop->nType & PH7_TK_COMMA) && iDepth == 0 ){` |
|       77 | 2862 | `						break;` |
|        - | 2863 | `					}` |
|      410 | 2864 | `					pArgStop++;` |
|        2 | 2865 | `				}` |
|      168 | 2866 | `				SyZero(&sArgRec,sizeof(sArgRec));` |
|      168 | 2867 | `				SySetInit(&sArgRec.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      166 | 2868 | `				if( pArgStart < pArgStop && (pArgStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|      114 | 2869 | `				 && &pArgStart[1] < pArgStop && (pArgStart[1].nType & PH7_TK_COLON) ){` |
|       37 | 2870 | `					char *zN = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       12 | 2871 | `						pArgStart->sData.zString,pArgStart->sData.nByte);` |
|       25 | 2872 | `					if( zN ){` |
|       25 | 2873 | `						SyStringInitFromBuf(&sArgRec.sName,zN,pArgStart->sData.nByte);` |
|       12 | 2874 | `					}` |
|       25 | 2875 | `					pArgStart += 2;` |
|       12 | 2876 | `				}` |
|      168 | 2877 | `				if( pArgStart < pArgStop ){` |
|        - | 2878 | `					SySet *pInstrContainer;` |
|      168 | 2879 | `					pGen->pIn = pArgStart;` |
|      168 | 2880 | `					pGen->pEnd = pArgStop;` |
|      168 | 2881 | `					pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      168 | 2882 | `					PH7_VmSetByteCodeContainer(pGen->pVm,&sArgRec.aByteCode);` |
|      168 | 2883 | `					rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      168 | 2884 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      168 | 2885 | `					PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      168 | 2886 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 2887 | `						pGen->pIn = pSavedIn;` |
|      ! 0 | 2888 | `						pGen->pEnd = pSavedEnd;` |
|      ! 0 | 2889 | `						return SXERR_ABORT;` |
|        - | 2890 | `					}` |
|      168 | 2891 | `					SySetPut(&sAttr.aArgs,(const void *)&sArgRec);` |
|       83 | 2892 | `				}` |
|      168 | 2893 | `				pIn = pArgStop;` |
|      168 | 2894 | `				if( pIn < pArgsEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|       77 | 2895 | `					pIn++;` |
|       38 | 2896 | `				}` |
|        2 | 2897 | `			}` |
|       94 | 2898 | `			pIn = (pArgsEnd < pEnd) ? &pArgsEnd[1] : pEnd;` |
|       46 | 2899 | `		}` |
|      241 | 2900 | `		SySetPut(pOut,(const void *)&sAttr);` |
|      241 | 2901 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        5 | 2902 | `			pIn++;` |
|        5 | 2903 | `			continue;` |
|        - | 2904 | `		}` |
|      237 | 2905 | `		break;` |
|      ! 0 | 2906 | `	}` |
|      237 | 2907 | `	pGen->pIn = pSavedIn;` |
|      237 | 2908 | `	pGen->pEnd = pSavedEnd;` |
|      237 | 2909 | `	return SXRET_OK;` |
|      121 | 2910 | `}` |
|        - | 2911 | `/*` |
|        - | 2912 | ` * Hand the pending attribute groups (if any) to a declaration: compile` |
|        - | 2913 | ` * every recorded group into pOut and clear the pending list.` |
|        - | 2914 | ` */` |
|   163658 | 2915 | `PH7_PRIVATE sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut)` |
|        5 | 2916 | `{` |
|   163663 | 2917 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|        - | 2918 | `	sxu32 n;` |
|        - | 2919 | `	sxi32 rc;` |
|   163881 | 2920 | `	for( n = 0 ; n < SySetUsed(&pGen->aPendingAttrs) ; n++ ){` |
|      223 | 2921 | `		rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|      223 | 2922 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 2923 | `			return SXERR_ABORT;` |
|        - | 2924 | `		}` |
|      114 | 2925 | `	}` |
|   163663 | 2926 | `	SySetReset(&pGen->aPendingAttrs);` |
|   163663 | 2927 | `	return SXRET_OK;` |
|    81834 | 2928 | `}` |
|        - | 2929 | `/*` |
|        - | 2930 | ` * Compile the attribute groups keyed to the given token (a parameter's` |
|        - | 2931 | ` * first token inside a signature) into pOut. Parameters are parsed from` |
|        - | 2932 | ` * the main token stream, so the sidecar indexes map directly.` |
|        - | 2933 | ` */` |
|   289018 | 2934 | `PH7_PRIVATE sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut)` |
|        5 | 2935 | `{` |
|   289023 | 2936 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|   289023 | 2937 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|   289023 | 2938 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|        - | 2939 | `	sxu32 nIdx, n;` |
|        - | 2940 | `	sxi32 rc;` |
|   289018 | 2941 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|      585 | 2942 | `	 \|\| pTok < pBase \|\| pTok >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|   288443 | 2943 | `		return SXRET_OK;` |
|        - | 2944 | `	}` |
|      585 | 2945 | `	nIdx = (sxu32)(pTok - pBase);` |
|     1699 | 2946 | `	for( n = 0 ; n < nT ; n++ ){` |
|     1119 | 2947 | `		if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|       16 | 2948 | `			rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|       16 | 2949 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2950 | `				return SXERR_ABORT;` |
|        - | 2951 | `			}` |
|        7 | 2952 | `		}` |
|      562 | 2953 | `	}` |
|      585 | 2954 | `	return SXRET_OK;` |
|   144514 | 2955 | `}` |
|  1575656 | 2956 | `PH7_PRIVATE sxi32 GenStateCompileChunk(` |
|        - | 2957 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 2958 | `	sxi32 iFlags         /* Compile flags */` |
|        - | 2959 | `	)` |
|        5 | 2960 | `{` |
|        - | 2961 | `	ProcLangConstruct xCons;` |
|        - | 2962 | `	sxi32 rc;` |
|  1575661 | 2963 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  1005894 | 2964 | `	for(;;){` |
|  1793727 | 2965 | `		int bStmtIsDeclare = 0;` |
|  1793727 | 2966 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 2967 | `			/* No more input to process */` |
|    17409 | 2968 | `			break;` |
|        - | 2969 | `		}` |
|        - | 2970 | `		/* Bind a directly-preceding docblock to this statement */` |
|  1776323 | 2971 | `		GenStateSetPendingDoc(&(*pGen));` |
|  1776323 | 2972 | `		if( SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|        - | 2973 | `			/* php: a statement-position attribute group must be followed by a` |
|        - | 2974 | ``			 * declaration (function/class-like/const) — `#[A] $x = 1;` is a`` |
|        - | 2975 | `` 			 * parse error, never a silent discard. `static`/`fn`/`function` `` |
|        - | 2976 | ``			 * cover bare closure-expression statements; `readonly`/`enum` are`` |
|        - | 2977 | `			 * context-sensitive IDs handled by the modified-class/enum scans. */` |
|      106 | 2978 | `			int bAttrTarget = 0;` |
|      104 | 2979 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd)` |
|      106 | 2980 | `			 \|\| GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|      ! 0 | 2981 | `				bAttrTarget = 1;` |
|      106 | 2982 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      106 | 2983 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      104 | 2984 | `				if( nKw == PH7_TKWRD_FUNCTION \|\| nKw == PH7_TKWRD_CLASS` |
|       39 | 2985 | `				 \|\| nKw == PH7_TKWRD_INTERFACE \|\| nKw == PH7_TKWRD_TRAIT` |
|        6 | 2986 | `				 \|\| nKw == PH7_TKWRD_ABSTRACT \|\| nKw == PH7_TKWRD_FINAL` |
|        6 | 2987 | `				 \|\| nKw == PH7_TKWRD_CONST \|\| nKw == PH7_TKWRD_STATIC` |
|        2 | 2988 | `				 \|\| nKw == PH7_TKWRD_FN ){` |
|      106 | 2989 | `					bAttrTarget = 1;` |
|       52 | 2990 | `				}` |
|       52 | 2991 | `			}` |
|      106 | 2992 | `			if( !bAttrTarget ){` |
|      ! 0 | 2993 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 2994 | `					"syntax error, unexpected token \"%z\" after attribute group; expecting a declaration",` |
|      ! 0 | 2995 | `					&pGen->pIn->sData);` |
|      ! 0 | 2996 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 2997 | `					break;` |
|        - | 2998 | `				}` |
|      ! 0 | 2999 | `				SySetReset(&pGen->aPendingAttrs);` |
|      ! 0 | 3000 | `			}` |
|       52 | 3001 | `		}` |
|        - | 3002 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|        - | 3003 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  1776323 | 3004 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  1005301 | 3005 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  1005301 | 3006 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|       61 | 3007 | `				bStmtIsDeclare = 1;` |
|       28 | 3008 | `			}` |
|   502648 | 3009 | `		}` |
|  1776323 | 3010 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|        - | 3011 | `			/* Any non-declare top-level statement locks the strict_types` |
|        - | 3012 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|   218091 | 3013 | `			pGen->bStrictTypesLocked = 1;` |
|   109043 | 3014 | `		}` |
|  1776323 | 3015 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|        - | 3016 | `			/* Compile block */` |
|       59 | 3017 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|       59 | 3018 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3019 | `				break;` |
|        - | 3020 | `			}` |
|       32 | 3021 | `		}else{` |
|  1776269 | 3022 | `			xCons = 0;` |
|  1776269 | 3023 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|        - | 3024 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|        - | 3025 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|        - | 3026 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|      145 | 3027 | `				xCons = PH7_CompileClassModifiers;` |
|  1776199 | 3028 | `			}else if( GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|        - | 3029 | ``				/* `enum Name …` (PHP 8.1) — `enum` is a context-sensitive ID,`` |
|        - | 3030 | `				 * so it is detected here rather than the keyword dispatcher. */` |
|      107 | 3031 | `				xCons = PH7_CompileEnum;` |
|  1776078 | 3032 | `			}else if( GenStateIsNsRelName(pGen->pIn,pGen->pEnd) ){` |
|        - | 3033 | ``				/* A statement that STARTS with php's `namespace\X` name operator`` |
|        - | 3034 | ``				 * (`namespace\Cee::m();`) is an expression, not a namespace`` |
|        - | 3035 | ``				 * DECLARATION — the glued `\` is what tells the two apart. */`` |
|        7 | 3036 | `				xCons = 0;` |
|  1776024 | 3037 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  1005185 | 3038 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        - | 3039 | `				/* Try to extract a language construct handler */` |
|  1005185 | 3040 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  1005185 | 3041 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|       13 | 3042 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3043 | `						"Syntax error: Unexpected keyword '%z'",` |
|        8 | 3044 | `						&pGen->pIn->sData);` |
|        9 | 3045 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3046 | `						break;` |
|        - | 3047 | `					}` |
|        - | 3048 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|        - | 3049 | `					 * this erroneous statement.` |
|        - | 3050 | `					 */` |
|        9 | 3051 | `					xCons = PH7_ErrorRecover;` |
|        4 | 3052 | `				}` |
|  1273431 | 3053 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|    79495 | 3054 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|        - | 3055 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|      217 | 3056 | `				xCons = PH7_CompileLabel;` |
|      106 | 3057 | `			}` |
|  1776269 | 3058 | `			if( xCons == 0 ){` |
|        - | 3059 | `				/* Assume an expression an try to compile it */` |
|   771397 | 3060 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   771397 | 3061 | `				if(  rc != SXERR_EMPTY ){` |
|        - | 3062 | `					/* Pop l-value */` |
|   771233 | 3063 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|   385614 | 3064 | `				}` |
|   385701 | 3065 | `			}else{` |
|        - | 3066 | `				/* Go compile the sucker */` |
|  1004877 | 3067 | `				rc = xCons(&(*pGen));` |
|        - | 3068 | `			}` |
|  1776269 | 3069 | `			if( rc == SXERR_ABORT ){` |
|        - | 3070 | `				/* Request to abort compilation */` |
|       81 | 3071 | `				break;` |
|        - | 3072 | `			}` |
|        - | 3073 | `		}` |
|        - | 3074 | `		/* Ignore trailing semi-colons ';' */` |
|  2942933 | 3075 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  1166691 | 3076 | `			pGen->pIn++;` |
|        5 | 3077 | `		}` |
|  1776247 | 3078 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|        - | 3079 | `			/* Compile a single statement and return */` |
|  1558181 | 3080 | `			break;` |
|        - | 3081 | `		}` |
|        - | 3082 | `		/* LOOP ONE */` |
|        - | 3083 | `		/* LOOP TWO */` |
|        - | 3084 | `		/* LOOP THREE */` |
|        - | 3085 | `		/* LOOP FOUR */` |
|        5 | 3086 | `	}` |
|        - | 3087 | `	/* Return compilation status */` |
|  1575661 | 3088 | `	return rc;` |
|        5 | 3089 | `}` |
|        - | 3090 | `/*` |
|        - | 3091 | ` * Compile a Raw PHP chunk.` |
|        - | 3092 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|        - | 3093 | ` * takes care of generating the appropriate error message.` |
|        - | 3094 | ` */` |
|    17482 | 3095 | `static sxi32 PH7_CompilePHP(` |
|        - | 3096 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - | 3097 | `	SySet *pTokenSet,     /* Token set */` |
|        - | 3098 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|        - | 3099 | `	)` |
|        5 | 3100 | `{` |
|    17487 | 3101 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|        - | 3102 | `	sxi32 rc;` |
|        - | 3103 | `	/* Reset the token set (and its trivia sidecar) */` |
|    17487 | 3104 | `	SySetReset(&(*pTokenSet));` |
|    17487 | 3105 | `	SySetReset(&pGen->aTrivia);` |
|        - | 3106 | `	/* Mark as the default token set */` |
|    17487 | 3107 | `	pGen->pTokenSet = &(*pTokenSet);` |
|        - | 3108 | `	/* Advance the stream cursor */` |
|    17487 | 3109 | `	pGen->pRawIn++;` |
|        - | 3110 | `	/* Tokenize the PHP chunk first */` |
|    17487 | 3111 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet),&pGen->aTrivia);` |
|        - | 3112 | `	/* Point to the head and tail of the token stream. */` |
|    17487 | 3113 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|    17487 | 3114 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|    17487 | 3115 | `	if( is_expr ){` |
|      ! 0 | 3116 | `		rc = SXERR_EMPTY;` |
|      ! 0 | 3117 | `		if( pGen->pIn < pGen->pEnd ){` |
|        - | 3118 | `			/* A simple expression,compile it */` |
|      ! 0 | 3119 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|      ! 0 | 3120 | `		}` |
|        - | 3121 | `		/* Emit the DONE instruction */` |
|      ! 0 | 3122 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      ! 0 | 3123 | `		return SXRET_OK;` |
|        - | 3124 | `	}` |
|    17487 | 3125 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|        - | 3126 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|        - | 3127 | `		/*` |
|        - | 3128 | `		 * Shortcut syntax for the 'echo' language construct.` |
|        - | 3129 | `		 * According to the PHP reference manual:` |
|        - | 3130 | `		 *  echo() also has a shortcut syntax, where you can` |
|        - | 3131 | `		 *  immediately follow` |
|        - | 3132 | `		 *  the opening tag with an equals sign as follows:` |
|        - | 3133 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|        - | 3134 | `		 * Symisc extension:` |
|        - | 3135 | `		 *   This short syntax works with all PHP opening` |
|        - | 3136 | `		 *   tags unlike the default PHP engine that handle` |
|        - | 3137 | `		 *   only short tag.` |
|        - | 3138 | `		 */` |
|        - | 3139 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|        3 | 3140 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|        3 | 3141 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|        3 | 3142 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|        - | 3143 | `		/* This synthesized echo is compiled as an EXPRESSION, which is otherwise a` |
|        - | 3144 | `		 * parse error; allow it for the duration of this one compile. */` |
|        3 | 3145 | `		pGen->nExprEchoOk++;` |
|        3 | 3146 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|        3 | 3147 | `		pGen->nExprEchoOk--;` |
|        3 | 3148 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3149 | `			return SXERR_ABORT;` |
|        - | 3150 | `		}` |
|        3 | 3151 | `		if( rc != SXERR_EMPTY ){` |
|        3 | 3152 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        1 | 3153 | `		}` |
|        3 | 3154 | `		return SXRET_OK;` |
|        - | 3155 | `	}` |
|        - | 3156 | `	/* Compile the PHP chunk */` |
|    17485 | 3157 | `	rc = GenStateCompileChunk(pGen,0);` |
|        - | 3158 | `	/* Fix exceptions jumps */` |
|    17485 | 3159 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        - | 3160 | `	/* Fix gotos now, the jump destination is resolved */` |
|    17485 | 3161 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|        3 | 3162 | `		rc = SXERR_ABORT;` |
|        1 | 3163 | `	}` |
|        - | 3164 | `	/* Reset container */` |
|    17485 | 3165 | `	SySetReset(&pGen->aGoto);` |
|    17485 | 3166 | `	SySetReset(&pGen->aLabel);` |
|    17485 | 3167 | `	SySetReset(&pGen->aNullsafeJmp);` |
|        - | 3168 | `	/* Compilation result */` |
|    17485 | 3169 | `	return rc;` |
|     8746 | 3170 | `}` |
|        - | 3171 | `/*` |
|        - | 3172 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|        - | 3173 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|        - | 3174 | ` * This is the only compile interface exported from this file.` |
|        - | 3175 | ` */` |
|    21238 | 3176 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|        - | 3177 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|        - | 3178 | `	SyString *pScript,  /* Script to compile */` |
|        - | 3179 | `	sxi32 iFlags        /* Compile flags */` |
|        - | 3180 | `	)` |
|        5 | 3181 | `{` |
|        - | 3182 | `	SySet aPhpToken,aRawToken;` |
|        - | 3183 | `	ph7_gen_state *pCodeGen;` |
|        - | 3184 | `	ph7_value *pRawObj;` |
|        - | 3185 | `	sxu32 nObjIdx;` |
|        - | 3186 | `	sxi32 nRawObj;` |
|        - | 3187 | `	int is_expr;` |
|        - | 3188 | `	sxi8 bSavedStrict;` |
|        - | 3189 | `	sxi8 bSavedStrictLocked;` |
|        - | 3190 | `	SyToken *pSavedIn,*pSavedEnd;` |
|        - | 3191 | `	sxi32 rc;` |
|    21243 | 3192 | `	sxu32 nBaseLine = 1;` |
|    21243 | 3193 | `	if( pScript->nByte < 1 ){` |
|        - | 3194 | `		/* Nothing to compile */` |
|      ! 0 | 3195 | `		return PH7_OK;` |
|        - | 3196 | `	}` |
|        - | 3197 | `	/* php skips a "#!" shebang on the first line of a CLI script: consume it` |
|        - | 3198 | `	 * (including its newline) so it is not echoed as inline text, and bump the` |
|        - | 3199 | `	 * base line to 2 so the code below still reports php-matching line numbers. */` |
|    21243 | 3200 | `	if( pScript->nByte >= 2 && pScript->zString[0]=='#' && pScript->zString[1]=='!' ){` |
|        3 | 3201 | `		const char *z = pScript->zString;` |
|        3 | 3202 | `		const char *zEnd = &z[pScript->nByte];` |
|       39 | 3203 | `		while( z < zEnd && z[0] != '\n' ){ z++; }` |
|        3 | 3204 | `		if( z < zEnd ){ z++; } /* consume the newline too */` |
|        3 | 3205 | `		pScript->nByte -= (sxu32)(z - pScript->zString);` |
|        3 | 3206 | `		pScript->zString = z;` |
|        3 | 3207 | `		nBaseLine = 2;` |
|        3 | 3208 | `		if( pScript->nByte < 1 ){` |
|      ! 0 | 3209 | `			return PH7_OK;` |
|        - | 3210 | `		}` |
|        1 | 3211 | `	}` |
|        - | 3212 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|        - | 3213 | `	 * file's flags so include/require restore them on return. */` |
|    21243 | 3214 | `	pCodeGen = &pVm->sCodeGen;` |
|        - | 3215 | ``	/* aPhpToken below is a LOCAL token set: once it is released at `cleanup`, any`` |
|        - | 3216 | `	 * pGen->pIn/pEnd still pointing into it DANGLE. PH7_VmEmitInstr reads pIn to stamp` |
|        - | 3217 | `	 * each instruction's source line, and instructions are still emitted after this` |
|        - | 3218 | `	 * function returns (VmEvalChunk's trailing OP_DONE) -- a use-after-free ASan caught` |
|        - | 3219 | `	 * immediately. Save the caller's cursor and restore it on the way out, so an` |
|        - | 3220 | `	 * enclosing compile keeps its (live) tokens and the top level goes back to NULL. */` |
|    21243 | 3221 | `	pSavedIn = pCodeGen->pIn;` |
|    21243 | 3222 | `	pSavedEnd = pCodeGen->pEnd;` |
|    21243 | 3223 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|    21243 | 3224 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|    21243 | 3225 | `	pCodeGen->bStrictTypes = 0;` |
|    21243 | 3226 | `	pCodeGen->bStrictTypesLocked = 0;` |
|        - | 3227 | `	/* Initialize the tokens containers */` |
|    21243 | 3228 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|    21243 | 3229 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|    21243 | 3230 | `	SySetAlloc(&aPhpToken,0xc0);` |
|    21243 | 3231 | `	is_expr = 0;` |
|    21243 | 3232 | `	if( iFlags & PH7_PHP_ONLY ){` |
|        - | 3233 | `		SyToken sTmp;` |
|        - | 3234 | `		/* PHP only: -*/` |
|     5569 | 3235 | `		sTmp.nLine = 1;` |
|     5569 | 3236 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|     5569 | 3237 | `		sTmp.pUserData = 0;` |
|     5569 | 3238 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|     5569 | 3239 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|     5569 | 3240 | `		if( iFlags & PH7_PHP_EXPR ){` |
|        - | 3241 | `			/* A simple PHP expression */` |
|      ! 0 | 3242 | `			is_expr = 1;` |
|      ! 0 | 3243 | `		}` |
|     2787 | 3244 | `	}else{` |
|        - | 3245 | `		/* Tokenize raw text */` |
|    15679 | 3246 | `		SySetAlloc(&aRawToken,32);` |
|    15679 | 3247 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken,nBaseLine);` |
|        - | 3248 | `	}` |
|        - | 3249 | `	/* Process high-level tokens */` |
|    21243 | 3250 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|    21243 | 3251 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|    21243 | 3252 | `	rc = PH7_OK;` |
|    21243 | 3253 | `	if( is_expr ){` |
|        - | 3254 | `		/* Compile the expression */` |
|      ! 0 | 3255 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|      ! 0 | 3256 | `		goto cleanup;` |
|        - | 3257 | `	}` |
|    21243 | 3258 | `	nObjIdx = 0;` |
|        - | 3259 | `	/* Start the compilation process */` |
|    18460 | 3260 | `	for(;;){` |
|    54329 | 3261 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|    21165 | 3262 | `			break; /* No more tokens to process */` |
|        - | 3263 | `		}` |
|    33169 | 3264 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|        - | 3265 | `			/* Compile the PHP chunk */` |
|    17487 | 3266 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|    17487 | 3267 | `			if( rc == SXERR_ABORT ){` |
|       83 | 3268 | `				break;` |
|        - | 3269 | `			}` |
|    17409 | 3270 | `			continue;` |
|        - | 3271 | `		}` |
|        - | 3272 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|    15687 | 3273 | `		nRawObj = 0;` |
|    31369 | 3274 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|        - | 3275 | `			/* Consume the raw chunk without any processing */` |
|    15687 | 3276 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|    15687 | 3277 | `			if( pRawObj == 0 ){` |
|      ! 0 | 3278 | `				rc = SXERR_MEM;` |
|      ! 0 | 3279 | `				break;` |
|        - | 3280 | `			}` |
|        - | 3281 | `			/* Mark as constant and emit the load constant instruction */` |
|    15687 | 3282 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|    15687 | 3283 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|    15687 | 3284 | `			++nRawObj;` |
|    15687 | 3285 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|        5 | 3286 | `		}` |
|    15687 | 3287 | `		if( nRawObj > 0 ){` |
|        - | 3288 | `			/* Emit the consume instruction */` |
|    15687 | 3289 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     7841 | 3290 | `		}` |
|    10624 | 3291 | `	}` |
|    10619 | 3292 | `cleanup:` |
|        - | 3293 | `	/* Drop the cursor into the token set BEFORE the set is freed (see above). */` |
|    21243 | 3294 | `	pCodeGen->pIn = pSavedIn;` |
|    21243 | 3295 | `	pCodeGen->pEnd = pSavedEnd;` |
|    21243 | 3296 | `	SySetRelease(&aRawToken);` |
|    21243 | 3297 | `	SySetRelease(&aPhpToken);` |
|        - | 3298 | `	/* Restore outer file's strict_types scope */` |
|    21243 | 3299 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|    21243 | 3300 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|    21243 | 3301 | `	return rc;` |
|    10624 | 3302 | `}` |
|        - | 3303 | `/*` |
|        - | 3304 | ` * Utility routines.Initialize the code generator.` |
|        - | 3305 | ` */` |
|     5254 | 3306 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|        - | 3307 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 3308 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|        - | 3309 | `	void *pErrData     /* Last argument to xErr() */` |
|        - | 3310 | `	)` |
|        5 | 3311 | `{` |
|     5259 | 3312 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|        - | 3313 | `	/* Zero the structure */` |
|     5259 | 3314 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|        - | 3315 | `	/* Initial state */` |
|     5259 | 3316 | `	pGen->pVm  = &(*pVm);` |
|     5259 | 3317 | `	pGen->xErr = xErr;` |
|     5259 | 3318 | `	pGen->pErrData = pErrData;` |
|     5259 | 3319 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|     5259 | 3320 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|     5259 | 3321 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|     5259 | 3322 | `	SySetInit(&pGen->aScope,&pVm->sAllocator,sizeof(GenScope));` |
|     5259 | 3323 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|     5259 | 3324 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|     5259 | 3325 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|     5259 | 3326 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|     5259 | 3327 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|     5259 | 3328 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|        - | 3329 | `	/* Error log buffer */` |
|     5259 | 3330 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|        - | 3331 | `	/* General purpose working buffer */` |
|     5259 | 3332 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|        - | 3333 | `	/* Namespace state */` |
|     5259 | 3334 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|     5259 | 3335 | `	GenStateInitUseImports(&(*pGen),&(*pVm));` |
|     5259 | 3336 | `	GenStateInitSeenSymbols(&(*pGen),&(*pVm));` |
|        - | 3337 | `	/* Create the global scope */` |
|     5259 | 3338 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|        - | 3339 | `	/* Point to the global scope */` |
|     5259 | 3340 | `	pGen->pCurrent = &pGen->sGlobal;` |
|     5259 | 3341 | `	return SXRET_OK;` |
|        5 | 3342 | `}` |
|        - | 3343 | `/*` |
|        - | 3344 | ` * Utility routines. Reset the code generator to it's initial state.` |
|        - | 3345 | ` */` |
|    25894 | 3346 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|        - | 3347 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 3348 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|        - | 3349 | `	void *pErrData     /* Last argument to xErr() */` |
|        - | 3350 | `	)` |
|        5 | 3351 | `{` |
|    25899 | 3352 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|        - | 3353 | `	GenBlock *pBlock,*pParent;` |
|        - | 3354 | `	/* Reset state */` |
|    25899 | 3355 | `	SySetReset(&pGen->aLabel);` |
|    25899 | 3356 | `	SySetReset(&pGen->aGoto);` |
|    25899 | 3357 | `	SySetReset(&pGen->aNullsafeJmp);` |
|    25899 | 3358 | `	SySetReset(&pGen->aTrivia);` |
|    25899 | 3359 | `	SySetReset(&pGen->aPendingAttrs);` |
|    25899 | 3360 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|    25899 | 3361 | `	SyBlobRelease(&pGen->sErrBuf);` |
|    25899 | 3362 | `	SyBlobRelease(&pGen->sWorker);` |
|    25899 | 3363 | `	SyBlobRelease(&pGen->sNamespace);` |
|    25899 | 3364 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    25899 | 3365 | `	GenStateResetUseImports(&(*pGen),&(*pVm));` |
|        - | 3366 | `	/* A fresh compile unit has declared nothing yet. */` |
|    25899 | 3367 | `	GenStateResetSeenSymbols(&(*pGen),&(*pVm));` |
|        - | 3368 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|        - | 3369 | `	 * They intern variable names and literal strings that are referenced by` |
|        - | 3370 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|        - | 3371 | `	 * Releasing them would either leak the interned strings or require freeing` |
|        - | 3372 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|        - | 3373 | `	 * number of unique names, which is acceptable. */` |
|        - | 3374 | `	/* Point to the global scope */` |
|    25899 | 3375 | `	pBlock = pGen->pCurrent;` |
|    25899 | 3376 | `	while( pBlock->pParent != 0 ){` |
|      ! 0 | 3377 | `		pParent = pBlock->pParent;` |
|      ! 0 | 3378 | `		GenStateFreeBlock(pBlock);` |
|      ! 0 | 3379 | `		pBlock = pParent;` |
|      ! 0 | 3380 | `	}` |
|    25899 | 3381 | `	pGen->xErr = xErr;` |
|    25899 | 3382 | `	pGen->pErrData = pErrData;` |
|    25899 | 3383 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    25899 | 3384 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|    25899 | 3385 | `	pGen->pIn = pGen->pEnd = 0;` |
|    25899 | 3386 | `	pGen->nErr = 0;` |
|        - | 3387 | `	/* Clear the class-body context (a prior compile aborted mid-class-body would` |
|        - | 3388 | `	 * otherwise leave these live for the next eval/include on this VM). */` |
|    25899 | 3389 | `	pGen->pCurClass = 0;` |
|    25899 | 3390 | `	pGen->iInMemberDefault = 0;` |
|    25899 | 3391 | `	return SXRET_OK;` |
|        5 | 3392 | `}` |
|        - | 3393 | `/*` |
|        - | 3394 | ` * Save the code generator's compile-position state and hand the live generator a` |
|        - | 3395 | ` * fresh, empty one for a NESTED compilation unit.` |
|        - | 3396 | ` *` |
|        - | 3397 | ` * A require/include normally runs at execution time, when no compile is in flight,` |
|        - | 3398 | ` * so VmEvalChunk can call PH7_ResetCodeGenerator to wipe the generator. But an` |
|        - | 3399 | `` * autoload can fire in the MIDDLE of compiling a class: resolving `class Child`` |
|        - | 3400 | `` * extends Base` calls PH7_VmExtractClass, which triggers the autoloader, whose`` |
|        - | 3401 | `` * body `require`s Base's file — a nested compile while the outer one is mid-token.`` |
|        - | 3402 | ` * PH7_ResetCodeGenerator would then blow away the outer compile's cursor` |
|        - | 3403 | ` * (pIn/pEnd), current block, use-imports, labels, etc.; the outer parse resumes` |
|        - | 3404 | ` * with pIn == NULL and reports a bogus "Expected '{' after class 'Child'". (Common` |
|        - | 3405 | ` * in every real framework: Composer autoloads parent classes on demand.)` |
|        - | 3406 | ` *` |
|        - | 3407 | ` * This snapshots the position/scope fields into *pSaved (a caller-stack` |
|        - | 3408 | ` * ph7_gen_state used purely as storage) and re-initializes the live generator's` |
|        - | 3409 | ` * position containers to FRESH, empty ones WITHOUT releasing the outer's (the` |
|        - | 3410 | ` * snapshot now owns those). hLiteral/hNumLiteral/hVar are intentionally left LIVE` |
|        - | 3411 | ` * and shared — compiled bytecode interns names/literals into them and they grow` |
|        - | 3412 | ` * monotonically (exactly why PH7_ResetCodeGenerator keeps them); restoring an old` |
|        - | 3413 | ` * copy of those headers after a nested grow would use-after-free the bucket array.` |
|        - | 3414 | ` */` |
|        4 | 3415 | `PH7_PRIVATE void PH7_CompilerSaveState(ph7_vm *pVm,ph7_gen_state *pSaved,ProcConsumer xErr,void *pErrData)` |
|        1 | 3416 | `{` |
|        5 | 3417 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|        - | 3418 | `	/* Shallow-copy every field; the position containers below are then replaced` |
|        - | 3419 | `	 * with fresh ones on the live struct, so *pSaved keeps the outer's memory. */` |
|        5 | 3420 | `	*pSaved = *pGen;` |
|        5 | 3421 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|        5 | 3422 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|        5 | 3423 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|        5 | 3424 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|        5 | 3425 | `	SySetInit(&pGen->aScope,&pVm->sAllocator,sizeof(GenScope));` |
|        5 | 3426 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|        5 | 3427 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|        5 | 3428 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|        5 | 3429 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|        5 | 3430 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|        5 | 3431 | `	GenStateInitUseImports(&(*pGen),&(*pVm));` |
|        5 | 3432 | `	GenStateInitSeenSymbols(&(*pGen),&(*pVm));` |
|        - | 3433 | `	/* Fresh global scope for the nested unit (address of the embedded sGlobal is` |
|        - | 3434 | `	 * stable, so any outer block still parented to it stays valid across restore). */` |
|        5 | 3435 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(pVm),0);` |
|        5 | 3436 | `	pGen->pCurrent = &pGen->sGlobal;` |
|        5 | 3437 | `	pGen->pIn = pGen->pEnd = 0;` |
|        5 | 3438 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|        5 | 3439 | `	pGen->pTokenSet = 0;` |
|        5 | 3440 | `	pGen->nErr = 0;` |
|        5 | 3441 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|        5 | 3442 | `	pGen->nCommaExprOk = 0;` |
|        5 | 3443 | `	pGen->zClauseCloser = 0;` |
|        5 | 3444 | `	pGen->bInGenerator = 0;` |
|        5 | 3445 | `	pGen->bStrictTypes = 0;` |
|        5 | 3446 | `	pGen->bStrictTypesLocked = 0;` |
|        - | 3447 | `	/* The nested unit is a fresh top-level compile: it is not lexically inside the` |
|        - | 3448 | `	 * outer's class body nor its member default, so a __TRAIT__ in the nested file` |
|        - | 3449 | `	 * must not inherit the outer's trait. (Restore below carries the outer's values` |
|        - | 3450 | `	 * back, so only the nested unit sees these zeros.) */` |
|        5 | 3451 | `	pGen->pCurClass = 0;` |
|        5 | 3452 | `	pGen->iInMemberDefault = 0;` |
|        5 | 3453 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|        5 | 3454 | `	pGen->xErr = xErr;` |
|        5 | 3455 | `	pGen->pErrData = pErrData;` |
|        5 | 3456 | `}` |
|        - | 3457 | `/*` |
|        - | 3458 | ` * Restore the outer compile-position state saved by PH7_CompilerSaveState,` |
|        - | 3459 | ` * releasing the nested unit's position containers first. The shared` |
|        - | 3460 | ` * hLiteral/hNumLiteral/hVar tables (grown by the nested compile) are carried` |
|        - | 3461 | ` * forward, NOT rolled back to the snapshot's stale headers.` |
|        - | 3462 | ` */` |
|        4 | 3463 | `PH7_PRIVATE void PH7_CompilerRestoreState(ph7_vm *pVm,ph7_gen_state *pSaved)` |
|        1 | 3464 | `{` |
|        5 | 3465 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|        - | 3466 | `	GenBlock *pBlock,*pParent;` |
|        - | 3467 | `	SyHash hVar,hLiteral,hNumLiteral;` |
|        - | 3468 | `	/* Free any nested blocks left open (e.g. an aborted nested compile), then the` |
|        - | 3469 | `	 * nested global block's own fixup sets. */` |
|        5 | 3470 | `	pBlock = pGen->pCurrent;` |
|        5 | 3471 | `	while( pBlock && pBlock->pParent != 0 ){` |
|      ! 0 | 3472 | `		pParent = pBlock->pParent;` |
|      ! 0 | 3473 | `		GenStateFreeBlock(pBlock);` |
|      ! 0 | 3474 | `		pBlock = pParent;` |
|      ! 0 | 3475 | `	}` |
|        5 | 3476 | `	GenStateReleaseBlock(&pGen->sGlobal);` |
|        - | 3477 | `	/* Release the nested unit's position containers. */` |
|        5 | 3478 | `	SySetRelease(&pGen->aLabel);` |
|        5 | 3479 | `	SySetRelease(&pGen->aGoto);` |
|        5 | 3480 | `	SySetRelease(&pGen->aNullsafeJmp);` |
|        5 | 3481 | `	SySetRelease(&pGen->aLoopParent);` |
|        5 | 3482 | `	SySetRelease(&pGen->aScope);` |
|        5 | 3483 | `	SySetRelease(&pGen->aTrivia);` |
|        5 | 3484 | `	SySetRelease(&pGen->aPendingAttrs);` |
|        5 | 3485 | `	SyBlobRelease(&pGen->sWorker);` |
|        5 | 3486 | `	SyBlobRelease(&pGen->sErrBuf);` |
|        5 | 3487 | `	SyBlobRelease(&pGen->sNamespace);` |
|        5 | 3488 | `	SyHashRelease(&pGen->hUseImports);` |
|        5 | 3489 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|        5 | 3490 | `	SyHashRelease(&pGen->hUseConstImports);` |
|        5 | 3491 | `	GenStateReleaseSeenSymbols(&(*pGen));` |
|        - | 3492 | `	/* Preserve the (possibly grown) shared intern tables across the restore. */` |
|        5 | 3493 | `	hVar = pGen->hVar;` |
|        5 | 3494 | `	hLiteral = pGen->hLiteral;` |
|        5 | 3495 | `	hNumLiteral = pGen->hNumLiteral;` |
|        5 | 3496 | `	*pGen = *pSaved;` |
|        5 | 3497 | `	pGen->hVar = hVar;` |
|        5 | 3498 | `	pGen->hLiteral = hLiteral;` |
|        5 | 3499 | `	pGen->hNumLiteral = hNumLiteral;` |
|        5 | 3500 | `}` |
|        - | 3501 | `/*` |
|        - | 3502 | ` * Raise php's parse error for an unexpected token: E_PARSE with the exact text` |
|        - | 3503 | ` * php's parser prints, e.g.` |
|        - | 3504 | ` *` |
|        - | 3505 | ` *   syntax error, unexpected token ";", expecting "{"` |
|        - | 3506 | ` *   syntax error, unexpected identifier "invalid", expecting "("` |
|        - | 3507 | ` *   syntax error, unexpected end of file` |
|        - | 3508 | ` *` |
|        - | 3509 | ` * php names the token by CLASS, not just by text: an identifier, a variable and` |
|        - | 3510 | ` * a number each get their own noun, while everything else (keywords, operators,` |
|        - | 3511 | ` * punctuation) is a "token". zExpecting is the optional ", expecting ..." tail` |
|        - | 3512 | ` * — pass NULL when the site cannot say what it wanted (php often can't either).` |
|        - | 3513 | ` * pTok == NULL means the input ran out: "unexpected end of file".` |
|        - | 3514 | ` *` |
|        - | 3515 | ` * PHL's hand-written recursive-descent parser has no bison expectation sets, so` |
|        - | 3516 | ` * a site can only claim an "expecting" clause it genuinely knows; every clause` |
|        - | 3517 | ` * emitted here was verified against php 8.5.7 for the construct in question.` |
|        - | 3518 | ` */` |
|      212 | 3519 | `PH7_PRIVATE sxi32 PH7_GenSyntaxError(` |
|        - | 3520 | `	ph7_gen_state *pGen,   /* Code generator state */` |
|        - | 3521 | `	SyToken *pTok,         /* Offending token, or NULL for end of file */` |
|        - | 3522 | `	const char *zExpecting /* ", expecting <this>" tail, or NULL */` |
|        - | 3523 | `	)` |
|        5 | 3524 | `{` |
|        - | 3525 | ``	/* php's noun for the offending token. An ALPHA-stream operator (`and`, `or`,`` |
|        - | 3526 | ``	 * `xor`, `new`, `clone`, `instanceof`) is lexed ID\|OP here but php calls it a`` |
|        - | 3527 | `	 * TOKEN, like every other reserved word — only a real identifier gets the` |
|        - | 3528 | `	 * "identifier" noun. */` |
|      217 | 3529 | `	const char *zNoun = "token";` |
|        - | 3530 | `	sxu32 nLine;` |
|      217 | 3531 | `	if( pTok == 0 && pGen->pTokenSet ){` |
|        - | 3532 | `		/* The caller ran out of tokens inside its own slice — but a statement's slice stops` |
|        - | 3533 | `		 * BEFORE its terminator, so the token php actually names (typically the ';') is the` |
|        - | 3534 | `		 * one sitting just past the slice, still inside the chunk's token stream. Reach for` |
|        - | 3535 | `		 * it before concluding "end of file". */` |
|       99 | 3536 | `		SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|       99 | 3537 | `		SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|       99 | 3538 | `		if( pGen->pEnd >= pBase && pGen->pEnd < pStreamEnd ){` |
|       92 | 3539 | `			pTok = pGen->pEnd;` |
|       44 | 3540 | `		}` |
|       47 | 3541 | `	}` |
|      217 | 3542 | `	nLine = pTok ? pTok->nLine : (pGen->pIn > (SyToken *)SySetBasePtr(pGen->pTokenSet) ? pGen->pIn[-1].nLine : 1);` |
|      217 | 3543 | `	if( pTok == 0 ){` |
|       11 | 3544 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        3 | 3545 | `			zExpecting ? "syntax error, unexpected end of file, expecting %s"` |
|        - | 3546 | `			           : "syntax error, unexpected end of file",` |
|        3 | 3547 | `			zExpecting);` |
|        - | 3548 | `	}` |
|      211 | 3549 | `	if( (pTok->nType & PH7_TK_ID) && (pTok->nType & PH7_TK_OP) == 0 ){` |
|       22 | 3550 | `		zNoun = "identifier";` |
|      202 | 3551 | `	}else if( pTok->nType & PH7_TK_DOLLAR ){` |
|        8 | 3552 | `		zNoun = "variable";` |
|        - | 3553 | `		/* The '$' is its own token and carries only "$" as text; the NAME is the` |
|        - | 3554 | ``		 * token after it. php names the whole variable, so `$x` was being reported`` |
|        - | 3555 | ``		 * as the nameless `variable "$"`. Stitch the two back together. */`` |
|        8 | 3556 | `		if( pGen->pTokenSet ){` |
|        8 | 3557 | `			SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        8 | 3558 | `			SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|        8 | 3559 | `			SyToken *pName = &pTok[1];` |
|        6 | 3560 | `			if( pTok >= pBase && pName < pStreamEnd` |
|        6 | 3561 | `				&& (pName->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|        8 | 3562 | `				&& pName->sData.nByte > 0 ){` |
|        8 | 3563 | `				SyBlobReset(&pGen->sWorker);` |
|        8 | 3564 | `				SyBlobAppend(&pGen->sWorker,"$",sizeof(char));` |
|        8 | 3565 | `				SyBlobAppend(&pGen->sWorker,pName->sData.zString,pName->sData.nByte);` |
|        - | 3566 | `				{` |
|        - | 3567 | `					SyString sVar;` |
|        8 | 3568 | `					SyStringInitFromBuf(&sVar,SyBlobData(&pGen->sWorker),` |
|        - | 3569 | `						SyBlobLength(&pGen->sWorker));` |
|        8 | 3570 | `					if( zExpecting ){` |
|       11 | 3571 | `						return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        - | 3572 | `							"syntax error, unexpected %s \"%z\", expecting %s",` |
|        3 | 3573 | `							zNoun,&sVar,zExpecting);` |
|        - | 3574 | `					}` |
|      ! 0 | 3575 | `					return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|      ! 0 | 3576 | `						"syntax error, unexpected %s \"%z\"",zNoun,&sVar);` |
|        - | 3577 | `				}` |
|        - | 3578 | `			}` |
|      ! 0 | 3579 | `		}` |
|      187 | 3580 | `	}else if( pTok->nType & PH7_TK_INTEGER ){` |
|       27 | 3581 | `		zNoun = "integer";` |
|      175 | 3582 | `	}else if( pTok->nType & PH7_TK_REAL ){` |
|      ! 0 | 3583 | `		zNoun = "float";` |
|      ! 0 | 3584 | `	}` |
|      205 | 3585 | `	if( zExpecting ){` |
|      146 | 3586 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|       47 | 3587 | `			"syntax error, unexpected %s \"%z\", expecting %s",zNoun,&pTok->sData,zExpecting);` |
|        - | 3588 | `	}` |
|      164 | 3589 | `	return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|       53 | 3590 | `		"syntax error, unexpected %s \"%z\"",zNoun,&pTok->sData);` |
|      111 | 3591 | `}` |
|        - | 3592 | `/*` |
|        - | 3593 | ` * Generate a compile-time error message.` |
|        - | 3594 | ` * If the error count limit is reached (usually 15 error message)` |
|        - | 3595 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|        - | 3596 | ` * abort compilation immediately.` |
|        - | 3597 | ` */` |
|      874 | 3598 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|        5 | 3599 | `{` |
|      879 | 3600 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|      879 | 3601 | `	const char *zErr = "Error";` |
|        - | 3602 | `	SyString *pFile;` |
|        - | 3603 | `	va_list ap;` |
|        - | 3604 | `	sxi32 rc;` |
|        - | 3605 | `	/* Reset the working buffer */` |
|      879 | 3606 | `	SyBlobReset(pWorker);` |
|        - | 3607 | `	/* Peek the processed file path if available */` |
|      879 | 3608 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|      879 | 3609 | `	if( nErrType == E_ERROR \|\| nErrType == E_PARSE ){` |
|        - | 3610 | `		/* Increment the error counter. A PARSE error is every bit as fatal as an` |
|        - | 3611 | `		 * E_ERROR one: php compiles nothing, runs nothing and exits 255. Counting` |
|        - | 3612 | `		 * only E_ERROR let a parse error print its diagnostic and then fall through` |
|        - | 3613 | `		 * into execution with a 0 exit status. */` |
|      837 | 3614 | `		pGen->nErr++;` |
|      837 | 3615 | `		if( pGen->nErr > 15 ){` |
|        - | 3616 | `			/* Error count limit reached */` |
|        6 | 3617 | `			if( pGen->xErr ){` |
|        6 | 3618 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|        6 | 3619 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|        6 | 3620 | `				if( pFile ){` |
|        6 | 3621 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|        2 | 3622 | `				}` |
|        6 | 3623 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|        6 | 3624 | `				if( SyBlobLength(pWorker) > 0 ){` |
|        6 | 3625 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|        2 | 3626 | `				}` |
|        2 | 3627 | `			}` |
|        - | 3628 | `			/* Abort immediately */` |
|        6 | 3629 | `			return SXERR_ABORT;` |
|        - | 3630 | `		}` |
|      414 | 3631 | `	}` |
|      875 | 3632 | `	if( pGen->xErr == 0 ){` |
|        - | 3633 | `		/* No consumer — but keep the BARE message in the error buffer so a caller` |
|        - | 3634 | `		 * that needs the text can read it back. eval() compiles with logging off` |
|        - | 3635 | `		 * (a parse error there is php's catchable ParseError, not a printed` |
|        - | 3636 | `		 * diagnostic) and needs exactly this string for the exception message. */` |
|       41 | 3637 | `		va_start(ap,zFormat);` |
|       41 | 3638 | `		SyBlobFormatAp(pWorker,zFormat,ap);` |
|       41 | 3639 | `		va_end(ap);` |
|       41 | 3640 | `		return SXRET_OK;` |
|        - | 3641 | `	}` |
|      835 | 3642 | `	switch(nErrType){` |
|      424 | 3643 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|       47 | 3644 | `	case E_WARNING: zErr = "Warning";     break;` |
|      372 | 3645 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|      ! 0 | 3646 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|      ! 0 | 3647 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|      ! 0 | 3648 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|      ! 0 | 3649 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|      ! 0 | 3650 | `	case 8192 /* E_DEPRECATED */: zErr = "Deprecated"; break;` |
|      ! 0 | 3651 | `	default:` |
|      ! 0 | 3652 | `		break;` |
|        - | 3653 | `	}` |
|      835 | 3654 | `	rc = SXRET_OK;` |
|        - | 3655 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|      835 | 3656 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|      835 | 3657 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|      835 | 3658 | `	va_start(ap,zFormat);` |
|      835 | 3659 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|      835 | 3660 | `	va_end(ap);` |
|      835 | 3661 | `	if( pFile ){` |
|      835 | 3662 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|      415 | 3663 | `	}` |
|        - | 3664 | `	/* Append a new line */` |
|      835 | 3665 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|      835 | 3666 | `	if( SyBlobLength(pWorker) > 0 ){` |
|        - | 3667 | `		/* Consume the generated error message */` |
|      835 | 3668 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|      415 | 3669 | `	}` |
|      835 | 3670 | `	return rc;` |
|      442 | 3671 | `}` |
|        - | 3672 |  |
