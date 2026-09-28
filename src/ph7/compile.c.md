# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2096/2236 lines (93.74%)

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
|      436 |   32 | `PH7_PRIVATE sxi32 GenStateGetLabel(ph7_gen_state *pGen,SyString *pName,ph7_vm_func *pFunc,Label **ppOut)` |
|        5 |   33 | `{` |
|        - |   34 | `	Label *aLabel;` |
|        - |   35 | `	sxu32 n;` |
|        - |   36 | `	/* Perform a linear scan on the label table */` |
|      441 |   37 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|     1605 |   38 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|     1331 |   39 | `		if( aLabel[n].pFunc == pFunc && SyStringCmp(&aLabel[n].sName,pName,SyMemcmp) == 0 ){` |
|        - |   40 | `			/* Jump destination found */` |
|      167 |   41 | `			if( ppOut ){` |
|      163 |   42 | `				*ppOut = &aLabel[n];` |
|       79 |   43 | `			}` |
|      167 |   44 | `			return SXRET_OK;` |
|        - |   45 | `		}` |
|      587 |   46 | `	}` |
|        - |   47 | `	/* No such destination */` |
|      279 |   48 | `	return SXERR_NOTFOUND;` |
|      223 |   49 | `}` |
|        - |   50 | `/*` |
|        - |   51 | ` * Fetch a block that correspond to the given criteria from the stack of` |
|        - |   52 | ` * compiled blocks.` |
|        - |   53 | ` * Return a pointer to that block on success. NULL otherwise.` |
|        - |   54 | ` */` |
|    46416 |   55 | `PH7_PRIVATE GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|        5 |   56 | `{` |
|    46421 |   57 | `	GenBlock *pBlock = pCurrent;` |
|   104084 |   58 | `	for(;;){` |
|   208173 |   59 | `		if( pBlock->iFlags & iBlockType ){` |
|    46403 |   60 | `			iCount--; /* Decrement nesting level */` |
|    46403 |   61 | `			if( iCount < 1 ){` |
|        - |   62 | `				/* Block meet with the desired criteria */` |
|    46371 |   63 | `				return pBlock;` |
|        - |   64 | `			}` |
|       16 |   65 | `		}` |
|        - |   66 | `		/* Point to the upper block */` |
|   161807 |   67 | `		pBlock = pBlock->pParent;` |
|   161807 |   68 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|        - |   69 | `			/* Forbidden */` |
|       28 |   70 | `			break;` |
|        - |   71 | `		}` |
|        5 |   72 | `	}` |
|        - |   73 | `	/* No such block */` |
|       53 |   74 | `	return 0;` |
|    23213 |   75 | `}` |
|        - |   76 | `/*` |
|        - |   77 | ` * Initialize a freshly allocated block instance.` |
|        - |   78 | ` */` |
|  1542808 |   79 | `static void GenStateInitBlock(` |
|        - |   80 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - |   81 | `	GenBlock *pBlock,    /* Target block */` |
|        - |   82 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|        - |   83 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|        - |   84 | `	void *pUserData      /* Upper layer private data */` |
|        - |   85 | `	)` |
|        5 |   86 | `{` |
|        - |   87 | `	/* Initialize block fields */` |
|  1542813 |   88 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  1542813 |   89 | `	pBlock->pUserData   = pUserData;` |
|  1542813 |   90 | `	pBlock->pGen        = pGen;` |
|  1542813 |   91 | `	pBlock->iFlags      = iType;` |
|  1542813 |   92 | `	pBlock->pParent     = 0;` |
|  1542813 |   93 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  1542813 |   94 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  1542813 |   95 | `}` |
|        - |   96 | `/*` |
|        - |   97 | ` * Allocate a new block instance.` |
|        - |   98 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|        - |   99 | ` * on success.Otherwise generate a compile-time error and abort` |
|        - |  100 | ` * processing on failure.` |
|        - |  101 | ` */` |
|  1537064 |  102 | `PH7_PRIVATE sxi32 GenStateEnterBlock(` |
|        - |  103 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - |  104 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|        - |  105 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|        - |  106 | `	void *pUserData,      /* Upper layer private data */` |
|        - |  107 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|        - |  108 | `	)` |
|        5 |  109 | `{` |
|        - |  110 | `	GenBlock *pBlock;` |
|        - |  111 | `	/* Allocate a new block instance */` |
|  1537069 |  112 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  1537069 |  113 | `	if( pBlock == 0 ){` |
|        - |  114 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |  115 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |  116 | `		 */` |
|      ! 0 |  117 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|        - |  118 | `		/* Abort processing immediately */` |
|      ! 0 |  119 | `		return SXERR_ABORT;` |
|        - |  120 | `	}` |
|        - |  121 | `	/* Zero the structure */` |
|  1537069 |  122 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  1537069 |  123 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|        - |  124 | `	/* Link to the parent block */` |
|  1537069 |  125 | `	pBlock->pParent = pGen->pCurrent;` |
|        - |  126 | `	/* A loop or switch gets an id, and remembers the loop it nests inside, so a goto's` |
|        - |  127 | `	 * and a label's positions can be compared after compilation (see aLoopParent). */` |
|  1537069 |  128 | `	if( iType & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|   136073 |  129 | `		sxu32 nParent = pGen->nCurLoopId;` |
|   136073 |  130 | `		pGen->nLoopId++;` |
|   136073 |  131 | `		SySetPut(&pGen->aLoopParent,(const void *)&nParent);` |
|   136073 |  132 | `		pBlock->nLoopId = pGen->nLoopId;` |
|   136073 |  133 | `		pBlock->nOuterLoopId = nParent;` |
|   136073 |  134 | `		pGen->nCurLoopId = pGen->nLoopId;` |
|    68034 |  135 | `	}` |
|        - |  136 | `	/* A try/catch/finally block gets a scope id, and remembers the scope it nests inside,` |
|        - |  137 | `	 * so the chain between any two points can be walked after compilation (aScope). Every` |
|        - |  138 | `	 * other block simply inherits the scope in effect. */` |
|  1537069 |  139 | `	pBlock->nOuterScopeId = pGen->nCurScopeId;` |
|  1537069 |  140 | `	pBlock->nScopeId = pGen->nCurScopeId;` |
|  1537069 |  141 | `	if( iType & GEN_BLOCK_EXCEPTION ){` |
|        - |  142 | `		GenScope sScope;` |
|     9011 |  143 | `		sScope.nParent = pGen->nCurScopeId;` |
|     9011 |  144 | `		sScope.pUserData = pUserData;` |
|     9011 |  145 | `		if( iType & GEN_BLOCK_FINALLY ){` |
|      309 |  146 | `			sScope.iKind = GEN_SCOPE_FINALLY;` |
|     8859 |  147 | `		}else if( iType & GEN_BLOCK_DETACHED ){` |
|     4181 |  148 | `			sScope.iKind = GEN_SCOPE_DETACHED;` |
|     2093 |  149 | `		}else{` |
|        - |  150 | `			/* A try. pUserData is its ph7_exception, which every try-block site passes at` |
|        - |  151 | `			 * ENTRY precisely so this can classify it. */` |
|     4531 |  152 | `			sScope.iKind = GenStateInlineTryCatch(pGen) ? GEN_SCOPE_TRY_INLINE : GEN_SCOPE_TRY;` |
|        - |  153 | `		}` |
|     9011 |  154 | `		if( SySetPut(&pGen->aScope,(const void *)&sScope) == SXRET_OK ){` |
|     9011 |  155 | `			pBlock->nScopeId = SySetUsed(&pGen->aScope);` |
|     9011 |  156 | `			pGen->nCurScopeId = pBlock->nScopeId;` |
|     4503 |  157 | `		}` |
|     4503 |  158 | `	}` |
|        - |  159 | `	/* Mark as the current block */` |
|  1537069 |  160 | `	pGen->pCurrent = pBlock;` |
|  1537069 |  161 | `	if( ppBlock ){` |
|        - |  162 | `		/* Write a pointer to the new instance */` |
|   728725 |  163 | `		*ppBlock = pBlock;` |
|   364360 |  164 | `	}` |
|  1537069 |  165 | `	return SXRET_OK;` |
|   768537 |  166 | `}` |
|        - |  167 | `/*` |
|        - |  168 | ` * Release block fields without freeing the whole instance.` |
|        - |  169 | ` */` |
|  1537056 |  170 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|        5 |  171 | `{` |
|  1537061 |  172 | `	SySetRelease(&pBlock->aPostContFix);` |
|  1537061 |  173 | `	SySetRelease(&pBlock->aJumpFix);` |
|  1537061 |  174 | `}` |
|        - |  175 | `/*` |
|        - |  176 | ` * Release a block.` |
|        - |  177 | ` */` |
|  1537052 |  178 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|        5 |  179 | `{` |
|  1537057 |  180 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  1537057 |  181 | `	GenStateReleaseBlock(&(*pBlock));` |
|        - |  182 | `	/* Free the instance */` |
|  1537057 |  183 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  1537057 |  184 | `}` |
|        - |  185 | `/*` |
|        - |  186 | ` * POP and release a block from the stack of compiled blocks.` |
|        - |  187 | ` */` |
|  1537052 |  188 | `PH7_PRIVATE sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|        5 |  189 | `{` |
|  1537057 |  190 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  1537057 |  191 | `	if( pBlock == 0 ){` |
|        - |  192 | `		/* No more block to pop */` |
|      ! 0 |  193 | `		return SXERR_EMPTY;` |
|        - |  194 | `	}` |
|  1537057 |  195 | `	if( pBlock->iFlags & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|   136063 |  196 | `		pGen->nCurLoopId = pBlock->nOuterLoopId;` |
|    68029 |  197 | `	}` |
|  1537057 |  198 | `	if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|     9011 |  199 | `		pGen->nCurScopeId = pBlock->nOuterScopeId;` |
|     4503 |  200 | `	}` |
|        - |  201 | `	/* Point to the upper block */` |
|  1537057 |  202 | `	pGen->pCurrent = pBlock->pParent;` |
|  1537057 |  203 | `	if( ppBlock ){` |
|        - |  204 | `		/* Write a pointer to the popped block */` |
|      ! 0 |  205 | `		*ppBlock = pBlock;` |
|      ! 0 |  206 | `	}else{` |
|        - |  207 | `		/* Safely release the block */` |
|  1537057 |  208 | `		GenStateFreeBlock(&(*pBlock));` |
|        - |  209 | `	}` |
|  1537057 |  210 | `	return SXRET_OK;` |
|   768531 |  211 | `}` |
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
|   162800 |  230 | `PH7_PRIVATE int GenStateUnconditionalTopLevel(ph7_gen_state *pGen)` |
|        5 |  231 | `{` |
|   162805 |  232 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   163009 |  233 | `	while( pBlock ){` |
|   163009 |  234 | `		if( pBlock->iFlags & (GEN_BLOCK_COND\|GEN_BLOCK_LOOP\|GEN_BLOCK_FUNC\|GEN_BLOCK_SWITCH\|GEN_BLOCK_EXCEPTION) ){` |
|      171 |  235 | `			return 0; /* conditional / nested */` |
|        - |  236 | `		}` |
|   162843 |  237 | `		if( pBlock->iFlags & GEN_BLOCK_GLOBAL ){` |
|   162639 |  238 | `			return 1; /* reached the global block with no conditional ancestor */` |
|        - |  239 | `		}` |
|      209 |  240 | `		pBlock = pBlock->pParent;` |
|        5 |  241 | `	}` |
|      ! 0 |  242 | `	return 1;` |
|    81405 |  243 | `}` |
|        - |  244 | `/*` |
|        - |  245 | ` * Guard a top-level function about to be installed. Same contract as the class` |
|        - |  246 | ` * guard above.` |
|        - |  247 | ` */` |
|   158378 |  248 | `PH7_PRIVATE sxi32 GenStateGuardFuncRedeclaration(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|        5 |  249 | `{` |
|        - |  250 | `	SyHashEntry *pEntry;` |
|   158383 |  251 | `	if( !GenStateUnconditionalTopLevel(pGen) ){` |
|      112 |  252 | `		return SXRET_OK;` |
|        - |  253 | `	}` |
|   158275 |  254 | `	pFunc->iFlags \|= VM_FUNC_BOUND;` |
|   158275 |  255 | `	if( pGen->pVm->bCompilingBuiltin ){` |
|   154985 |  256 | `		return SXRET_OK;` |
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
|     3295 |  274 | `	if( SyHashGet(&pGen->pVm->hHostFunction,(const void *)pFunc->sName.zString,pFunc->sName.nByte) ){` |
|        8 |  275 | `		PH7_GenCompileError(pGen,E_ERROR,pFunc->nLine,` |
|        2 |  276 | `			"Cannot redeclare function %z()",&pFunc->sName);` |
|        6 |  277 | `		return SXERR_ABORT;` |
|        - |  278 | `	}` |
|     3291 |  279 | `	pEntry = SyHashGet(&pGen->pVm->hFunction,(const void *)pFunc->sName.zString,pFunc->sName.nByte);` |
|     3291 |  280 | `	if( pEntry ){` |
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
|     3287 |  297 | `	return SXRET_OK;` |
|    79194 |  298 | `}` |
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
|   848710 |  309 | `PH7_PRIVATE sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|        5 |  310 | `{` |
|        - |  311 | `	JumpFixup sJumpFix;` |
|        - |  312 | `	sxi32 rc;` |
|        - |  313 | `	/* Init the JumpFixup structure */` |
|   848715 |  314 | `	sJumpFix.nJumpType = nJumpType;` |
|   848715 |  315 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|        - |  316 | `	/* Remember which bytecode array the emitted instruction lives in: the block whose` |
|        - |  317 | `	 * table this lands in may be resolved after a container swap (see JumpFixup). */` |
|   848715 |  318 | `	sJumpFix.pContainer = PH7_VmGetByteCodeContainer(pBlock->pGen->pVm);` |
|        - |  319 | `	/* Insert in the jump fixup table */` |
|   848715 |  320 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|   848715 |  321 | `	return rc;` |
|        5 |  322 | `}` |
|        - |  323 | `/*` |
|        - |  324 | ` * TRUE when the body being compiled has its try/catch/finally compiled INLINE` |
|        - |  325 | ` * (ROOT C, generator bodies) rather than into detached mini-programs.` |
|        - |  326 | ` */` |
|     4526 |  327 | `PH7_PRIVATE int GenStateInlineTryCatch(ph7_gen_state *pGen)` |
|        5 |  328 | `{` |
|     4531 |  329 | `	return pGen->bInGenerator && pGen->pVm->bInlineTryCatch;` |
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
|    46522 |  356 | `PH7_PRIVATE int GenStateJumpScope(ph7_gen_state *pGen,sxu32 nFrom,sxu32 nTo,int bEmitPops,` |
|        - |  357 | `	GenJumpScope *pScope)` |
|        5 |  358 | `{` |
|    46527 |  359 | `	GenScope *aScope = (GenScope *)SySetBasePtr(&pGen->aScope);` |
|    46527 |  360 | `	sxu32 nUsed = SySetUsed(&pGen->aScope);` |
|    46527 |  361 | `	sxu32 nCur = nFrom;` |
|    46527 |  362 | `	SyZero(pScope,sizeof(*pScope));` |
|    46641 |  363 | `	while( nCur != nTo ){` |
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
|    46523 |  394 | `	return TRUE;` |
|    23266 |  395 | `}` |
|        - |  396 | `/*` |
|        - |  397 | ` * Pick the jump opcode for a crossing described by GenStateJumpScope, and its iP1.` |
|        - |  398 | ` * Shared by break/continue (which know their target at emit time) and goto (which` |
|        - |  399 | ` * settles this in GenStateFixGoto, once the label fixes the counts).` |
|        - |  400 | ` */` |
|    46408 |  401 | `PH7_PRIVATE sxi32 GenStateScopeJumpOp(const GenJumpScope *pCross,sxi32 *piP1)` |
|        5 |  402 | `{` |
|    46413 |  403 | `	if( pCross->nDet > 0 \|\| pCross->nTry > 0 ){` |
|       84 |  404 | `		*piP1 = PH7_CATCH_JMP_P1(pCross->nDet,pCross->nTry);` |
|       84 |  405 | `		return PH7_OP_CATCH_JMP;` |
|        - |  406 | `	}` |
|    46333 |  407 | `	if( pCross->nInline > 0 ){` |
|       11 |  408 | `		*piP1 = (sxi32)pCross->nInline;` |
|       11 |  409 | `		return PH7_OP_SET_FINALLY_JMP;` |
|        - |  410 | `	}` |
|    46325 |  411 | `	*piP1 = 0;` |
|    46325 |  412 | `	return PH7_OP_JMP;` |
|    23209 |  413 | `}` |
|        - |  414 | `/*` |
|        - |  415 | ` * Resolve a recorded fixup to its VM instruction, in the container it was emitted` |
|        - |  416 | ` * into (see JumpFixup.pContainer) rather than whichever one is current now.` |
|        - |  417 | ` */` |
|   866102 |  418 | `PH7_PRIVATE VmInstr * GenStateFixupInstr(const JumpFixup *pFix)` |
|        5 |  419 | `{` |
|   866107 |  420 | `	return (VmInstr *)SySetAt(pFix->pContainer,pFix->nInstrIdx);` |
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
|  1213638 |  433 | `PH7_PRIVATE sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|        5 |  434 | `{` |
|        - |  435 | `	JumpFixup *aFix;` |
|        - |  436 | `	VmInstr *pInstr;` |
|        - |  437 | `	sxu32 nFixed;` |
|        - |  438 | `	sxu32 n;` |
|        - |  439 | `	/* Point to the jump fixup table */` |
|  1213643 |  440 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|        - |  441 | `	/* Fix the desired jumps */` |
|  2733867 |  442 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  1520229 |  443 | `		if( aFix[n].nJumpType < 0 ){` |
|        - |  444 | `			/* Already fixed */` |
|   526873 |  445 | `			continue;` |
|        - |  446 | `		}` |
|   993361 |  447 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|        - |  448 | `			/* Not of our interest */` |
|   144653 |  449 | `			continue;` |
|        - |  450 | `		}` |
|        - |  451 | `		/* Point to the instruction to fix */` |
|   848713 |  452 | `		pInstr = GenStateFixupInstr(&aFix[n]);` |
|   848713 |  453 | `		if( pInstr ){` |
|   848713 |  454 | `			pInstr->iP2 = nJumpDest;` |
|   848713 |  455 | `			nFixed++;` |
|        - |  456 | `			/* Mark as fixed */` |
|   848713 |  457 | `			aFix[n].nJumpType = -1;` |
|   424354 |  458 | `		}` |
|   424359 |  459 | `	}` |
|        - |  460 | `	/* Total number of fixed jumps */` |
|  1213643 |  461 | `	return nFixed;` |
|        5 |  462 | `}` |
|        - |  463 | `/*` |
|        - |  464 | ` * Fix a 'goto' now the jump destination is resolved.` |
|        - |  465 | ` * The goto statement can be used to jump to another section` |
|        - |  466 | ` * in the program.` |
|        - |  467 | ` * Refer to the routine responsible of compiling the goto` |
|        - |  468 | ` * statement for more information.` |
|        - |  469 | ` */` |
|   195146 |  470 | `PH7_PRIVATE sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|        5 |  471 | `{` |
|        - |  472 | `	JumpFixup *pJump,*aJumps;` |
|        - |  473 | `	GenJumpScope sCross;` |
|        - |  474 | `	Label *pLabel;` |
|        - |  475 | `	VmInstr *pInstr;` |
|        - |  476 | `	sxi32 rc;` |
|        - |  477 | `	sxu32 n;` |
|        - |  478 | `	/* Point to the goto table */` |
|   195151 |  479 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|        - |  480 | `	/* Fix */` |
|   195371 |  481 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
|      227 |  482 | `		pJump = &aJumps[n];` |
|        - |  483 | `		/* Extract the target label */` |
|        - |  484 | `		/* A label declared in ANOTHER function is not a destination: the lookup is keyed` |
|        - |  485 | `		 * on the goto's own function, so a same-named label elsewhere simply does not` |
|        - |  486 | `		 * answer and this reports php's undefined-label fatal. */` |
|      227 |  487 | `		rc = GenStateGetLabel(&(*pGen),&pJump->sLabel,pJump->pFunc,&pLabel);` |
|      227 |  488 | `		if( rc != SXRET_OK ){` |
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
|      163 |  500 | `		if( pLabel->nLoopId != 0 ){` |
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
|      163 |  527 | `		if( !GenStateJumpScope(&(*pGen),pJump->nScopeId,pLabel->nScopeId,FALSE,&sCross) ){` |
|        6 |  528 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,` |
|        - |  529 | `				"'goto' into a try, catch or finally block is disallowed");` |
|        6 |  530 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  531 | `				return SXERR_ABORT;` |
|        - |  532 | `			}` |
|        6 |  533 | `			continue;` |
|        - |  534 | `		}` |
|      159 |  535 | `		if( sCross.nFinally > 0 ){` |
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
|      157 |  552 | `		pInstr = GenStateFixupInstr(pJump);` |
|      157 |  553 | `		if( pInstr ){` |
|      157 |  554 | `			pInstr->iP2 = pLabel->nJumpDest;` |
|      157 |  555 | `			if( pInstr->iOp == PH7_OP_CATCH_JMP ){` |
|        - |  556 | `				/* Emitted as a structure-crossing jump because the goto sits inside a` |
|        - |  557 | `				 * try or a detached body. Now that the crossing is known it may well` |
|        - |  558 | `				 * turn out to leave nothing, and downgrade to a plain OP_JMP. */` |
|       47 |  559 | `				sxi32 iP1 = 0;` |
|       47 |  560 | `				pInstr->iOp = (sxu8)GenStateScopeJumpOp(&sCross,&iP1);` |
|       47 |  561 | `				pInstr->iP1 = iP1;` |
|       22 |  562 | `			}` |
|       76 |  563 | `		}` |
|       81 |  564 | `	}` |
|        - |  565 | `	/* php says nothing about a label nobody jumps to — the old "defined but not` |
|        - |  566 | `	 * referenced" warning was a PH7-ism with no counterpart in the oracle. */` |
|   195149 |  567 | `	return SXRET_OK;` |
|    97578 |  568 | `}` |
|        - |  569 | `/*` |
|        - |  570 | ` * Check if a given token value is installed in the literal table.` |
|        - |  571 | ` */` |
|  1543474 |  572 | `PH7_PRIVATE sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|        5 |  573 | `{` |
|        - |  574 | `	SyHashEntry *pEntry;` |
|  1543479 |  575 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  1543479 |  576 | `	if( pEntry == 0 ){` |
|   666599 |  577 | `		return SXERR_NOTFOUND;` |
|        - |  578 | `	}` |
|   876885 |  579 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|   876885 |  580 | `	return SXRET_OK;` |
|   771742 |  581 | `}` |
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
|   666594 |  592 | `PH7_PRIVATE sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|        5 |  593 | `{` |
|   666599 |  594 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|   666599 |  595 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   333297 |  596 | `	}` |
|   666599 |  597 | `	return SXRET_OK;` |
|        5 |  598 | `}` |
|        - |  599 | `/*` |
|        - |  600 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|        - |  601 | ` * in the constant table.` |
|        - |  602 | ` */` |
|   693694 |  603 | `PH7_PRIVATE ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|        5 |  604 | `{` |
|        - |  605 | `	ph7_value *pObj;` |
|   693699 |  606 | `	sxu32 nIdx = 0; /* cc warning */` |
|        - |  607 | `	/* Reserve a new constant */` |
|   693699 |  608 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   693699 |  609 | `	if( pObj == 0 ){` |
|      ! 0 |  610 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|      ! 0 |  611 | `		return 0;` |
|        - |  612 | `	}` |
|   693699 |  613 | `	*pIdx = nIdx;` |
|        - |  614 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|        - |  615 | `	 * the constant string iterals table [optimization purposes].` |
|        - |  616 | `	 */` |
|   693699 |  617 | `	return pObj;` |
|   346852 |  618 | `}` |
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
|   944442 |  633 | `PH7_PRIVATE void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|        5 |  634 | `{` |
|        - |  635 | `	VmCallArgMap *pMap;` |
|   944447 |  636 | `	if( !pGen->bStrictTypes ) return p3;` |
|      318 |  637 | `	if( p3 == 0 ){` |
|       44 |  638 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|       44 |  639 | `		if( pMap == 0 ) return 0;` |
|       44 |  640 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|       44 |  641 | `		p3 = (void *)pMap;` |
|       20 |  642 | `	}` |
|      318 |  643 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|      318 |  644 | `	return p3;` |
|   472226 |  645 | `}` |
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
|      184 |  665 | `PH7_PRIVATE int GenStateIsReservedConstant(SyString *pName)` |
|        5 |  666 | `{` |
|      189 |  667 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|       20 |  668 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|        3 |  669 | `			return TRUE;` |
|       18 |  670 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|        6 |  671 | `			return TRUE;` |
|        4 |  672 | `		}` |
|      178 |  673 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|       15 |  674 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|        3 |  675 | `			return TRUE;` |
|        - |  676 | `		}` |
|        5 |  677 | `	}` |
|        - |  678 | `	/* Not a reserved constant */` |
|      181 |  679 | `	return FALSE;` |
|       97 |  680 | `}` |
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
|  8236432 |  706 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|        5 |  707 | `{` |
|  8236437 |  708 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|        - |  709 | `	sxu32 nTarget;` |
|        - |  710 | `	sxu32 *aIdx;` |
|        - |  711 | `	sxu32 i;` |
|  8236437 |  712 | `	if( nCur <= nBaseline ){` |
|  8236303 |  713 | `		return;` |
|        - |  714 | `	}` |
|      139 |  715 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|      139 |  716 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|      281 |  717 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|      147 |  718 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|      147 |  719 | `		if( pInstr ){` |
|      147 |  720 | `			pInstr->iP2 = (sxi32)nTarget;` |
|       71 |  721 | `		}` |
|       76 |  722 | `	}` |
|      139 |  723 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
|  4118221 |  724 | `}` |
|        - |  725 |  |
|        - |  726 | `/*` |
|        - |  727 | `` * Does this call-argument node reach its target THROUGH a property? `$o->p`,`` |
|        - |  728 | ``  * `$this->m['k']`, `$o->a->b` all do; `$a['k']`, `$a[$i][$j]` and a plain `$var` `` |
|        - |  729 | ` * do not.` |
|        - |  730 | ` *` |
|        - |  731 | ` * Only the SUBSCRIPT spine is walked, because that is the only operator whose` |
|        - |  732 | `` * base is still part of the same lvalue: everything else (a call, a cast, `::`,`` |
|        - |  733 | `` * `?->`) either ends the path or is not writable through at all.`` |
|        - |  734 | ` */` |
|    18258 |  735 | `static int GenStateArgHasPropertyStep(ph7_expr_node *pNode)` |
|        5 |  736 | `{` |
|    18305 |  737 | `	while( pNode && pNode->pOp ){` |
|      125 |  738 | `		if( pNode->pOp->iOp == EXPR_OP_ARROW ){` |
|       76 |  739 | `			return 1;` |
|        - |  740 | `		}` |
|       51 |  741 | `		if( pNode->pOp->iOp != EXPR_OP_SUBSCRIPT ){` |
|        8 |  742 | `			return 0;` |
|        - |  743 | `		}` |
|       44 |  744 | `		pNode = pNode->pLeft;` |
|        2 |  745 | `	}` |
|    18183 |  746 | `	return 0;` |
|     9134 |  747 | `}` |
|        - |  748 | `/*` |
|        - |  749 | ` * By-reference out-parameters of builtin functions.` |
|        - |  750 | ` *` |
|        - |  751 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|        - |  752 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|        - |  753 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|        - |  754 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|        - |  755 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|        - |  756 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|        - |  757 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|        - |  758 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|        - |  759 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|        - |  760 | ` * creates it" behaviour).` |
|        - |  761 | ` *` |
|        - |  762 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|        - |  763 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|        - |  764 | ` */` |
|   853004 |  765 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|        5 |  766 | `{` |
|        - |  767 | `	static const struct {` |
|        - |  768 | `		const char *zName;` |
|        - |  769 | `		sxu32 nByte;` |
|        - |  770 | `		sxu32 mask;` |
|        - |  771 | `	} aByRef[] = {` |
|        - |  772 | `		{ "parse_str",              9, 1u<<1 },  /* &$result (apArg[1]) */` |
|        - |  773 | `		{ "settype",                7, 1u<<0 },  /* &$var    (apArg[0]) */` |
|        - |  774 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|        - |  775 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|        - |  776 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|        - |  777 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|        - |  778 | `		{ "flock",                  5, 1u<<2 },  /* &$would_block (apArg[2]) */` |
|        - |  779 | `		{ "getopt",                 6, 1u<<2 },  /* &$rest_index (apArg[2]) */` |
|        - |  780 | `		{ "is_callable",           11, 1u<<2 },  /* &$callable_name (apArg[2]) */` |
|        - |  781 | `		{ "similar_text",          12, 1u<<2 },  /* &$percent (apArg[2]) */` |
|        - |  782 | `		{ "str_replace",           11, 1u<<3 },  /* &$count  (apArg[3]) */` |
|        - |  783 | `		{ "str_ireplace",          12, 1u<<3 },  /* &$count  (apArg[3]) */` |
|        - |  784 | `		{ "fsockopen",              9, (1u<<2)\|(1u<<3) },  /* &$error_code, &$error_message */` |
|        - |  785 | `		{ "pfsockopen",            10, (1u<<2)\|(1u<<3) },  /* same */` |
|        - |  786 | `		{ "stream_socket_client",  20, (1u<<1)\|(1u<<2) },  /* &$error_code, &$error_message */` |
|        - |  787 | `		{ "stream_socket_server",  20, (1u<<1)\|(1u<<2) },  /* same pair */` |
|        - |  788 | `		{ "stream_socket_accept",  20, 1u<<2 },            /* &$peer_name (apArg[2]) */` |
|        - |  789 | `		{ "stream_select",         13, (1u<<0)\|(1u<<1)\|(1u<<2) }, /* &$read, &$write, &$except */` |
|        - |  790 | `		{ "stream_socket_recvfrom",22, 1u<<3 },            /* &$address (apArg[3]) */` |
|        - |  791 | `		{ "proc_open",              9, 1u<<2 },  /* &$pipes (apArg[2]) */` |
|        - |  792 | `		{ "exec",                   4, (1u<<1)\|(1u<<2) },  /* &$output, &$result_code */` |
|        - |  793 | `		{ "system",                 6, 1u<<1 },  /* &$result_code (apArg[1]) */` |
|        - |  794 | `		{ "passthru",               8, 1u<<1 },  /* &$result_code (apArg[1]) */` |
|        - |  795 | `		/* A by-ref VARIADIC tail: every actual from the third on is one of` |
|        - |  796 | ``		 * sscanf()'s `&...$vars`, so each is created rather than read. */`` |
|        - |  797 | `		{ "sscanf",                 6, ~((1u<<2) - 1u) },` |
|        - |  798 | `		{ "fscanf",                 6, ~((1u<<2) - 1u) },` |
|        - |  799 | `	};` |
|        - |  800 | `	sxu32 i;` |
|   853009 |  801 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|    40385 |  802 | `		return 0;` |
|        - |  803 | `	}` |
| 20609247 |  804 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
| 19821480 |  805 | `		if( pName->nByte == aByRef[i].nByte` |
| 10752891 |  806 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|    24867 |  807 | `			return aByRef[i].mask;` |
|        - |  808 | `		}` |
|  9898314 |  809 | `	}` |
|   787767 |  810 | `	return 0;` |
|   426507 |  811 | `}` |
|        - |  812 | `/*` |
|        - |  813 | ` * What may be passed by REFERENCE is decided from the argument's SHAPE, at compile` |
|        - |  814 | ` * time, exactly as php decides it (zend_compile_args -> zend_is_variable).` |
|        - |  815 | ` *` |
|        - |  816 | ` * php sorts every actual argument into three buckets:` |
|        - |  817 | ` *` |
|        - |  818 | ` *   GEN_ARG_LVALUE   a variable, an element, a property, a static property. It has a` |
|        - |  819 | ` *                    slot, so a by-ref parameter aliases it.` |
|        - |  820 | `` *   GEN_ARG_TEMPCALL the result of a call or of `new`. It has no slot, but php cannot`` |
|        - |  821 | ` *                    know at compile time whether the callee returns a reference, so it` |
|        - |  822 | ` *                    defers: E_NOTICE "Only variables should be passed by reference",` |
|        - |  823 | ` *                    then it operates on the temporary.` |
|        - |  824 | ` *   GEN_ARG_NONE     everything else — a literal, an operator/cast result, a class` |
|        - |  825 | `` *                    constant, `@$x`, `$o?->p`, an assignment. Binding one to a by-ref`` |
|        - |  826 | ` *                    parameter is a catchable Error at the CALL.` |
|        - |  827 | ` *` |
|        - |  828 | ` * Deciding it from the argument's runtime memobj instead does not work and was silently` |
|        - |  829 | ` * wrong in both directions: an arithmetic or concatenation result keeps its LEFT operand's` |
|        - |  830 | `` * slot index, so `f($i + 1)` with `function f(&$x)` aliased and overwrote `$i`; and a`` |
|        - |  831 | ` * builtin's by-ref row saw only "no slot", which a call result has too.` |
|        - |  832 | ` */` |
|        - |  833 | `#define GEN_ARG_LVALUE   0` |
|        - |  834 | `#define GEN_ARG_TEMPCALL 1` |
|        - |  835 | `#define GEN_ARG_NONE     2` |
|  1212422 |  836 | `static int GenStateArgShape(ph7_expr_node *pNode)` |
|        5 |  837 | `{` |
|  1212427 |  838 | `	if( pNode == 0 ){` |
|      ! 0 |  839 | `		return GEN_ARG_NONE;` |
|        - |  840 | `	}` |
|  1212427 |  841 | `	if( pNode->pOp == 0 ){` |
|        - |  842 | ``		/* A leaf: only the `$…` family is a variable. Everything else the parser`` |
|        - |  843 | ``		 * files here — a literal, an array/list constructor, a closure, `match`,`` |
|        - |  844 | ``		 * `clone` — is a temporary. */`` |
|   996245 |  845 | `		return pNode->xCode == PH7_CompileVariable ? GEN_ARG_LVALUE : GEN_ARG_NONE;` |
|        - |  846 | `	}` |
|   216187 |  847 | `	switch( pNode->pOp->iOp ){` |
|    13579 |  848 | `	case EXPR_OP_SUBSCRIPT: /* $a[k], and any base: php accepts g()[0] and C::m()[0] */` |
|        - |  849 | `	case EXPR_OP_ARROW:     /* $o->p */` |
|    27163 |  850 | `		return GEN_ARG_LVALUE;` |
|      414 |  851 | `	case EXPR_OP_DC:` |
|        - |  852 | ``		/* `C::$s` is a static property (an lvalue); `C::K` is a class constant and`` |
|        - |  853 | ``		 * `C::CASE` an enum case, neither of which php will bind. The right operand`` |
|        - |  854 | `		 * tells them apart. */` |
|     1247 |  855 | `		return ( pNode->pRight && pNode->pRight->pOp == 0` |
|      828 |  856 | `		      && pNode->pRight->xCode == PH7_CompileVariable )` |
|      828 |  857 | `			? GEN_ARG_LVALUE : GEN_ARG_NONE;` |
|    25615 |  858 | `	case EXPR_OP_FUNC_CALL:` |
|        - |  859 | `	case EXPR_OP_NEW:` |
|    51235 |  860 | `		return GEN_ARG_TEMPCALL;` |
|    68483 |  861 | `	default:` |
|        - |  862 | ``		/* Includes `?->` (php: "Cannot use nullsafe operator in write context"),`` |
|        - |  863 | ``		 * `@$x`, `$q = …`, `clone $o` and every arithmetic/logical operator. */`` |
|   136971 |  864 | `		return GEN_ARG_NONE;` |
|        - |  865 | `	}` |
|   606216 |  866 | `}` |
|        - |  867 | `/*` |
|        - |  868 | ` * Recover the bare global-builtin name from a call's callee node.` |
|        - |  869 | ` *` |
|        - |  870 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|        - |  871 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|        - |  872 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|        - |  873 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|        - |  874 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|        - |  875 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|        - |  876 | ` */` |
|  1663460 |  877 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|        5 |  878 | `{` |
|        - |  879 | `	SyToken *p, *pEnd;` |
|  1663465 |  880 | `	pOut->zString = 0;` |
|  1663465 |  881 | `	pOut->nByte = 0;` |
|  1663465 |  882 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|      ! 0 |  883 | `		return;` |
|        - |  884 | `	}` |
|  1663465 |  885 | `	p = pLeft->pStart;` |
|  1663465 |  886 | `	pEnd = pLeft->pEnd;` |
|        - |  887 | `	/* Optional single leading namespace separator (absolute path). */` |
|  1663465 |  888 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|      100 |  889 | `		p++;` |
|       48 |  890 | `	}` |
|  1663465 |  891 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|    50605 |  892 | `		return;` |
|        - |  893 | `	}` |
|        - |  894 | `	/* Must be a single component: nothing follows the name token. */` |
|  1612865 |  895 | `	if( p + 1 != pEnd ){` |
|      357 |  896 | `		return;` |
|        - |  897 | `	}` |
|  1612513 |  898 | `	*pOut = p->sData;` |
|   831735 |  899 | `}` |
|        - |  900 | `/*` |
|        - |  901 | `` * Is this expression node the bare variable `$this`?`` |
|        - |  902 | ` */` |
|   997398 |  903 | `PH7_PRIVATE int PH7_ExprNodeIsThis(ph7_expr_node *pNode)` |
|        5 |  904 | `{` |
|        - |  905 | `	SyToken *pTok;` |
|   997403 |  906 | `	if( pNode == 0 \|\| pNode->pOp != 0 \|\| pNode->xCode != PH7_CompileVariable ){` |
|   160317 |  907 | `		return 0;` |
|        - |  908 | `	}` |
|   837091 |  909 | `	pTok = pNode->pStart;` |
|   837091 |  910 | `	if( pTok == 0 \|\| pNode->pEnd == 0 \|\| pNode->pEnd < &pTok[2] ){` |
|      ! 0 |  911 | `		return 0;` |
|        - |  912 | `	}` |
|  1255634 |  913 | `	return (pTok[0].nType & PH7_TK_DOLLAR) != 0` |
|   837086 |  914 | `		&& (pTok[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|   837075 |  915 | `		&& pTok[1].sData.nByte == sizeof("this")-1` |
|  1255629 |  916 | `		&& SyMemcmp((const void *)pTok[1].sData.zString,(const void *)"this",sizeof("this")-1) == 0;` |
|   498704 |  917 | `}` |
|        - |  918 | `/*` |
|        - |  919 | ` * TRUE when codegen is inside a real FUNCTION body — php's` |
|        - |  920 | `` * `CG(active_op_array)->function_name`. A synthetic block (a match() arm's`` |
|        - |  921 | ` * throw-fixup) carries no ph7_vm_func and is not a scope.` |
|        - |  922 | ` */` |
|        2 |  923 | `static int GenStateInFunction(ph7_gen_state *pGen)` |
|        1 |  924 | `{` |
|        3 |  925 | `	GenBlock *pBlock = pGen->pCurrent;` |
|        9 |  926 | `	while( pBlock ){` |
|        7 |  927 | `		if( (pBlock->iFlags & GEN_BLOCK_FUNC) && pBlock->pUserData ){` |
|      ! 0 |  928 | `			return 1;` |
|        - |  929 | `		}` |
|        7 |  930 | `		pBlock = pBlock->pParent;` |
|        1 |  931 | `	}` |
|        3 |  932 | `	return 0;` |
|        2 |  933 | `}` |
|        - |  934 | `/*` |
|        - |  935 | `` * php's SPECIALIZED builtins — the list behind `Cannot use result of built-in`` |
|        - |  936 | `` * function in write context`.`` |
|        - |  937 | ` *` |
|        - |  938 | ` * The wording says "built-in function" but the rule is not about builtins: php` |
|        - |  939 | ` * refuses the write when the call was compiled to an OPCODE of its own rather` |
|        - |  940 | ` * than a real call, because a specialized opcode leaves a TMP where a call` |
|        - |  941 | `` * leaves a VAR (`zend_separate_if_call_and_write`). So `strlen("x")[0] = 1` and`` |
|        - |  942 | `` * `count([1])[0] = 1` are compile fatals while `array_values([1])[0] = 2`,`` |
|        - |  943 | `` * `str_split("ab")[0] = "z"` and `get_object_vars($o)["k"] = 2` all RUN — the`` |
|        - |  944 | `` * difference being php's `zend_try_compile_special_func_ex` table, reproduced`` |
|        - |  945 | ` * here name for name with the ARITY each entry demands.` |
|        - |  946 | ` *` |
|        - |  947 | ` * Six of php's names are deliberately absent, and only the last pair is a` |
|        - |  948 | ` * simplification — the other four are not refusals of php's at all:` |
|        - |  949 | `` *   `chr`/`ord` gate on BP_VAR_R, so they are never special in a WRITE context;`` |
|        - |  950 | `` *   `call_user_func`/`call_user_func_array` emit a REAL call, so their result is`` |
|        - |  951 | ` *     a VAR and php does not refuse a write through it either;` |
|        - |  952 | `` *   `in_array` and `array_slice` gate on the CONTENTS of a literal array`` |
|        - |  953 | `` *     argument and on a `func_get_args()`-shaped first argument — value-dependent`` |
|        - |  954 | ` *     shapes no program writes through, left out under §10. Leaving them out` |
|        - |  955 | ` *     ACCEPTS where php refuses, which is the direction that keeps running a` |
|        - |  956 | ` *     program php runs.` |
|        - |  957 | ` * Verified by sweeping every internal function of both engines at arities 0-3:` |
|        - |  958 | ` * the two specialized sets are identical, 27 names at the same arities.` |
|        - |  959 | ` */` |
|        - |  960 | `#define SPECFN_LITERAL_ARG0 0x01 /* php gives up unless argument #1 is a literal */` |
|        - |  961 | ``#define SPECFN_ANY_ARGS     0x02 /* …and `assert` is decided BEFORE php's unpack/named`` |
|        - |  962 | ``                                  * bail, so it stays special even for `assert(...$a)` */`` |
|        - |  963 | `#define SPECFN_IN_FUNC      0x04 /* php's gate reads CG(active_op_array)->function_name:` |
|        - |  964 | `                                  * at GLOBAL scope it emits a real call, whose runtime` |
|        - |  965 | `                                  * Error ("cannot be called from the global scope") is` |
|        - |  966 | `                                  * what the program actually gets */` |
|        - |  967 | ``#define SPECFN_FORMAT_ARG0  0x08 /* …and `sprintf` also needs php's format arithmetic`` |
|        - |  968 | `                                  * (implies SPECFN_LITERAL_ARG0) */` |
|        - |  969 | `static const struct {` |
|        - |  970 | `	const char *zName;` |
|        - |  971 | `	int nMinArg;   /* inclusive */` |
|        - |  972 | `	int nMaxArg;   /* inclusive; -1 = variadic */` |
|        - |  973 | `	int iFlags;` |
|        - |  974 | `} aSpecialFunc[] = {` |
|        - |  975 | `	{ "strlen",           1,  1, 0 },` |
|        - |  976 | `	{ "is_null",          1,  1, 0 },  { "is_bool",          1,  1, 0 },` |
|        - |  977 | `	{ "is_long",          1,  1, 0 },  { "is_int",           1,  1, 0 },` |
|        - |  978 | `	{ "is_integer",       1,  1, 0 },  { "is_float",         1,  1, 0 },` |
|        - |  979 | `	{ "is_double",        1,  1, 0 },  { "is_string",        1,  1, 0 },` |
|        - |  980 | `	{ "is_array",         1,  1, 0 },  { "is_object",        1,  1, 0 },` |
|        - |  981 | `	{ "is_resource",      1,  1, 0 },  { "is_scalar",        1,  1, 0 },` |
|        - |  982 | `	{ "boolval",          1,  1, 0 },  { "intval",           1,  1, 0 },` |
|        - |  983 | `	{ "floatval",         1,  1, 0 },  { "doubleval",        1,  1, 0 },` |
|        - |  984 | `	{ "strval",           1,  1, 0 },` |
|        - |  985 | `	{ "count",            1,  1, 0 },  { "sizeof",           1,  1, 0 },` |
|        - |  986 | `	{ "get_class",        0,  1, 0 },  { "get_called_class", 0,  0, 0 },` |
|        - |  987 | `	{ "gettype",          1,  1, 0 },` |
|        - |  988 | `	{ "func_num_args",    0,  0, SPECFN_IN_FUNC },` |
|        - |  989 | `	{ "func_get_args",    0,  0, SPECFN_IN_FUNC },` |
|        - |  990 | `	{ "array_key_exists", 2,  2, 0 },` |
|        - |  991 | `	{ "defined",          1,  1, SPECFN_LITERAL_ARG0 },` |
|        - |  992 | `	{ "sprintf",          1, -1, SPECFN_LITERAL_ARG0\|SPECFN_FORMAT_ARG0 },` |
|        - |  993 | `	/* php compiles assert() to its own opcode pair "independently of compiler` |
|        - |  994 | `	 * flags", in zend_compile_call BEFORE the special-func table is consulted —` |
|        - |  995 | `	 * so every arity counts and an unpacked argument does not exempt it. */` |
|        - |  996 | `	{ "assert",           0, -1, SPECFN_ANY_ARGS },` |
|        - |  997 | `};` |
|        - |  998 | `/*` |
|        - |  999 | ` * TRUE when this call node is one php compiles to an opcode of its own, so a` |
|        - | 1000 | ` * write THROUGH its result is php's built-in-function refusal. pName is the` |
|        - | 1001 | ` * callee's bare global name, already resolved by GenStateCallBuiltinName.` |
|        - | 1002 | ` */` |
|      136 | 1003 | `static int GenStateCallIsSpecialized(ph7_gen_state *pGen,ph7_expr_node *pCall,SyString *pName)` |
|        4 | 1004 | `{` |
|        - | 1005 | `	ph7_expr_node **apArg;` |
|        - | 1006 | `	sxu32 nArg, n;` |
|        - | 1007 | `	sxu32 i;` |
|      140 | 1008 | `	if( pName->nByte < 1 ){` |
|       35 | 1009 | `		return 0;` |
|        - | 1010 | `	}` |
|      106 | 1011 | `	apArg = (ph7_expr_node **)SySetBasePtr(&pCall->aNodeArgs);` |
|      106 | 1012 | `	nArg = SySetUsed(&pCall->aNodeArgs);` |
|     2818 | 1013 | `	for( i = 0 ; i < SX_ARRAYSIZE(aSpecialFunc) ; ++i ){` |
|        - | 1014 | `		SyString sEntry;` |
|     2728 | 1015 | `		SyStringInitFromBuf(&sEntry,aSpecialFunc[i].zName,SyStrlen(aSpecialFunc[i].zName));` |
|     2724 | 1016 | `		if( sEntry.nByte != pName->nByte` |
|     1564 | 1017 | `		 \|\| SyStrnicmp(sEntry.zString,pName->zString,pName->nByte) != 0 ){` |
|     2714 | 1018 | `			continue;` |
|        - | 1019 | `		}` |
|       12 | 1020 | `		if( (int)nArg < aSpecialFunc[i].nMinArg` |
|       16 | 1021 | `		 \|\| (aSpecialFunc[i].nMaxArg >= 0 && (int)nArg > aSpecialFunc[i].nMaxArg) ){` |
|       10 | 1022 | `			return 0;` |
|        - | 1023 | `		}` |
|        - | 1024 | `		/* php bails out of the whole table when any argument unpacks or is named` |
|        - | 1025 | ``		 * (`zend_args_contain_unpack_or_named`), so `strlen(...$a)[0] = 1` RUNS. */`` |
|       11 | 1026 | `		if( (aSpecialFunc[i].iFlags & SPECFN_ANY_ARGS) == 0 ){` |
|       17 | 1027 | `			for( n = 0 ; n < nArg ; ++n ){` |
|       11 | 1028 | `				if( apArg[n] && (apArg[n]->iFlags & (EXPR_NODE_SPREAD\|EXPR_NODE_NAMED_ARG)) ){` |
|        3 | 1029 | `					return 0;` |
|        - | 1030 | `				}` |
|        5 | 1031 | `			}` |
|        3 | 1032 | `		}` |
|        9 | 1033 | `		if( (aSpecialFunc[i].iFlags & SPECFN_IN_FUNC) && !GenStateInFunction(pGen) ){` |
|        3 | 1034 | `			return 0;` |
|        - | 1035 | `		}` |
|        - | 1036 | ``		/* `defined` and `sprintf` specialize only over a LITERAL first argument;`` |
|        - | 1037 | `		 * php gives up on a computed one and emits an ordinary call. */` |
|        6 | 1038 | `		if( aSpecialFunc[i].iFlags & SPECFN_LITERAL_ARG0 ){` |
|        2 | 1039 | `			if( nArg < 1 \|\| apArg[0] == 0 \|\| apArg[0]->pOp != 0` |
|        2 | 1040 | `			 \|\| apArg[0]->pStart == 0` |
|        3 | 1041 | `			 \|\| (apArg[0]->pStart->nType & (PH7_TK_SSTR\|PH7_TK_DSTR)) == 0 ){` |
|      ! 0 | 1042 | `				return 0;` |
|        - | 1043 | `			}` |
|        1 | 1044 | `		}` |
|        6 | 1045 | `		if( (aSpecialFunc[i].iFlags & SPECFN_FORMAT_ARG0) && nArg >= 1 && apArg[0] ){` |
|        - | 1046 | `			/* php's own sprintf gate, and it is arithmetic: a format under 256` |
|        - | 1047 | ``			 * bytes carrying nothing but `%s`, `%d` and `%%`, with exactly one`` |
|        - | 1048 | ``			 * VALUE per placeholder. `sprintf("a","b")` fails it (no placeholder,`` |
|        - | 1049 | `			 * one value) and compiles to an ordinary call, which is why the write` |
|        - | 1050 | `			 * through it RUNS. */` |
|        3 | 1051 | `			const SyString *pFmt = &apArg[0]->pStart->sData;` |
|        3 | 1052 | `			sxu32 nPlace = 0, k;` |
|        3 | 1053 | `			if( pFmt->nByte >= 256 ){` |
|      ! 0 | 1054 | `				return 0;` |
|        - | 1055 | `			}` |
|        - | 1056 | `			/* An escape or an interpolation makes php's argument something other` |
|        - | 1057 | `			 * than a plain literal; leave those to the ordinary call. */` |
|        7 | 1058 | `			for( k = 0 ; k < pFmt->nByte ; ++k ){` |
|        4 | 1059 | `				if( pFmt->zString[k] == '\\'` |
|        7 | 1060 | `				 \|\| ((apArg[0]->pStart->nType & PH7_TK_DSTR)` |
|        4 | 1061 | `				  && (pFmt->zString[k] == '$' \|\| pFmt->zString[k] == '{')) ){` |
|      ! 0 | 1062 | `					return 0;` |
|        - | 1063 | `				}` |
|        3 | 1064 | `			}` |
|        5 | 1065 | `			for( k = 0 ; k < pFmt->nByte ; ++k ){` |
|        3 | 1066 | `				if( pFmt->zString[k] != '%' ){` |
|      ! 0 | 1067 | `					continue;` |
|        - | 1068 | `				}` |
|        3 | 1069 | `				if( k + 1 >= pFmt->nByte ){` |
|      ! 0 | 1070 | `					return 0; /* a trailing '%' */` |
|        - | 1071 | `				}` |
|        3 | 1072 | `				k++;` |
|        3 | 1073 | `				if( pFmt->zString[k] == 's' \|\| pFmt->zString[k] == 'd' ){` |
|        3 | 1074 | `					nPlace++;` |
|        1 | 1075 | `				}else if( pFmt->zString[k] != '%' ){` |
|      ! 0 | 1076 | `					return 0; /* any other conversion */` |
|        - | 1077 | `				}` |
|        2 | 1078 | `			}` |
|        3 | 1079 | `			if( nPlace != nArg - 1 ){` |
|      ! 0 | 1080 | `				return 0;` |
|        - | 1081 | `			}` |
|        1 | 1082 | `		}` |
|        6 | 1083 | `		return 1;` |
|      ! 0 | 1084 | `	}` |
|       91 | 1085 | `	return 0;` |
|       72 | 1086 | `}` |
|        - | 1087 | `/*` |
|        - | 1088 | ` * The two write-target rules php decides at COMPILE time, in one place because` |
|        - | 1089 | ` * every write site has to make both of them.` |
|        - | 1090 | ` *` |
|        - | 1091 | `` * **`$this`** is not a variable a program may re-point: php refuses the`` |
|        - | 1092 | `` * assignment, the reference bind, a foreach/list target and `unset()` where they`` |
|        - | 1093 | `` * are WRITTEN. PHL performed all of them, so `$this = 5;` inside a method`` |
|        - | 1094 | ` * replaced the receiver with an int for the rest of the call and every later` |
|        - | 1095 | `` * `$this->x` failed somewhere else entirely.`` |
|        - | 1096 | ` *` |
|        - | 1097 | ``  * **A temporary** cannot be written THROUGH: `(new A)->p = 1` and `"s"->p = 1` `` |
|        - | 1098 | ` * modify an object/value that no longer exists after the statement, so php` |
|        - | 1099 | `` * refuses the whole chain — every write kind, including `+=`, `++`, `=&` and`` |
|        - | 1100 | `` * `unset()`. The base of the access chain decides: a variable and a userland`` |
|        - | 1101 | `` * CALL are writable (`f()->p = 1` is php-legal), a `new`, a literal and any`` |
|        - | 1102 | ` * other computed value are not, and an internal function's result gets php's own` |
|        - | 1103 | `` * separate wording — which is what `(clone $o)->p = 1` is, `clone` being a`` |
|        - | 1104 | ` * function in php 8.5.` |
|        - | 1105 | ` *` |
|        - | 1106 | ` * **A call is writable THROUGH but not writable INTO.** The distinction is` |
|        - | 1107 | `` * php's, and it is made in two different places: `zend_compile_var_inner` lets`` |
|        - | 1108 | `` * a call be the base of a chain, while `zend_ensure_writable_variable` refuses`` |
|        - | 1109 | ` * the call when it is the target ITSELF, with a wording that says which kind of` |
|        - | 1110 | `` * call it was. So `f()[0] = 5` compiles and `f() = 5` does not. The one write`` |
|        - | 1111 | `` * site that does not ask the second question is the SOURCE of `=&`, which is`` |
|        - | 1112 | `` * why `$r =& f()` is a runtime notice rather than a compile error.`` |
|        - | 1113 | ` */` |
|   997398 | 1114 | `PH7_PRIVATE sxi32 GenStateWriteTargetCheck(ph7_gen_state *pGen,ph7_expr_node *pTarget,int iCtx)` |
|        5 | 1115 | `{` |
|   997403 | 1116 | `	ph7_expr_node *pBase = pTarget;` |
|   997403 | 1117 | `	const char *zMsg = 0;` |
|        - | 1118 | `	sxi32 rc;` |
|   997403 | 1119 | `	if( pTarget == 0 ){` |
|      ! 0 | 1120 | `		return SXRET_OK;` |
|        - | 1121 | `	}` |
|   997403 | 1122 | `	if( PH7_ExprNodeIsThis(pTarget) ){` |
|       11 | 1123 | `		zMsg = (iCtx & PH7_WTC_UNSET) ? "Cannot unset $this" : "Cannot re-assign $this";` |
|   997397 | 1124 | `	}else if( pTarget->pOp && pTarget->pOp->iOp == EXPR_OP_FUNC_CALL` |
|    80057 | 1125 | `	       && (iCtx & PH7_WTC_REFSRC) == 0 ){` |
|        - | 1126 | ``		/* The target is the call itself (`f() = 5`, `f()++`, `unset(f())`,`` |
|        - | 1127 | ``		 * `foreach (… as f())`). php names the kind of call: a METHOD callee —`` |
|        - | 1128 | ``		 * `$o->m()`, `C::m()` — reports "method", everything else "function".`` |
|        - | 1129 | `		 * A PARENTHESISED member callee is php's variable-invocation` |
|        - | 1130 | ``		 * (`($o->p)()` calls the property's VALUE), which is an ordinary`` |
|        - | 1131 | `		 * function call, exactly the distinction the OP_CALL codegen makes. */` |
|       23 | 1132 | `		int bMethod = pTarget->pLeft` |
|       10 | 1133 | `			&& pTarget->pLeft->pOp` |
|        8 | 1134 | `			&& (pTarget->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|        5 | 1135 | `			 \|\| pTarget->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|        3 | 1136 | `			 \|\| pTarget->pLeft->pOp->iOp == EXPR_OP_DC)` |
|       15 | 1137 | `			&& (pTarget->pLeft->iFlags & EXPR_NODE_PARENS) == 0;` |
|       13 | 1138 | `		zMsg = bMethod` |
|        - | 1139 | `			? "Can't use method return value in write context"` |
|        5 | 1140 | `			: "Can't use function return value in write context";` |
|   997390 | 1141 | `	}else if( PH7_ExprContainsNullsafe(pTarget) ){` |
|        - | 1142 | ``		/* php asks this AFTER the call question (`$o?->m()++` is a method return`` |
|        - | 1143 | `` 		 * value, not a nullsafe chain) and BEFORE the base one (`(new A)?->p = 1` `` |
|        - | 1144 | `		 * is the nullsafe refusal, not the temporary). A reference SOURCE has its` |
|        - | 1145 | ``		 * own sentence for it. The `=`/`+=`/`unset()`/foreach paths screened this`` |
|        - | 1146 | ``		 * themselves; `++`/`--`, `??=` and `array(&…)` did not, so `$o?->p++` ran. */`` |
|        6 | 1147 | `		zMsg = (iCtx & PH7_WTC_REFSRC)` |
|        - | 1148 | `			? "Cannot take reference of a nullsafe chain"` |
|        2 | 1149 | `			: "Can't use nullsafe operator in write context";` |
|        4 | 1150 | `	}else{` |
|        - | 1151 | `		/* Walk to the base of the access chain; the links themselves are writable. */` |
|  1157743 | 1152 | `		while( pBase && pBase->pOp ){` |
|   160717 | 1153 | `			if( pBase->pOp->iOp == EXPR_OP_DC && !PH7_ExprNodeIsClassConst(pBase) ){` |
|        - | 1154 | `` 				/* A `::` left operand is a CLASS reference, not a value — `C::$s = 1` `` |
|        - | 1155 | ``				 * and even `(new C)::$s = 1` write class-level storage that outlives`` |
|        - | 1156 | `` 				 * any temporary, so the chain stops being about a base here. A `::` `` |
|        - | 1157 | ``				 * naming a CONSTANT is not storage, though: `A::K[0] = 5` subscripts`` |
|        - | 1158 | `				 * a COPY, so it falls through to the computed-base verdict below —` |
|        - | 1159 | `				 * php's "Cannot use temporary expression in write context", where` |
|        - | 1160 | `				 * PHL wrote into the copy and answered nothing. */` |
|      171 | 1161 | `				return SXRET_OK;` |
|        - | 1162 | `			}` |
|   160546 | 1163 | `			if( pBase->pOp->iOp != EXPR_OP_ARROW && pBase->pOp->iOp != EXPR_OP_NULLSAFE_ARROW` |
|   158195 | 1164 | `			 && pBase->pOp->iOp != EXPR_OP_SUBSCRIPT ){` |
|      189 | 1165 | `				break;` |
|        - | 1166 | `			}` |
|   160367 | 1167 | `			pBase = pBase->pLeft;` |
|        5 | 1168 | `		}` |
|   997215 | 1169 | `		if( pBase == 0 \|\| pBase == pTarget ){` |
|        - | 1170 | `			/* No chain: a non-variable target of its own is the caller's business` |
|        - | 1171 | `			 * (php reports its parse error / "Assignments can only happen to` |
|        - | 1172 | `			 * writable values" there, and so does PHL). */` |
|   837367 | 1173 | `			return SXRET_OK;` |
|        - | 1174 | `		}` |
|   159853 | 1175 | `		if( pBase->pOp == 0 ){` |
|   159709 | 1176 | `			if( pBase->xCode != PH7_CompileVariable ){` |
|      ! 0 | 1177 | `				zMsg = "Cannot use temporary expression in write context";` |
|        5 | 1178 | `			}` |
|    80001 | 1179 | `		}else if( pBase->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|        - | 1180 | `			/* php refuses a write through the result of a call it SPECIALIZED into` |
|        - | 1181 | `			 * an opcode — see aSpecialFunc above. The old test asked whether the` |
|        - | 1182 | `			 * name was a host function AT ALL, which would have refused every` |
|        - | 1183 | `			 * builtin (php specializes 28 of them), and asked it of the CALL node` |
|        - | 1184 | `			 * where GenStateCallBuiltinName wants the CALLEE node — so it never` |
|        - | 1185 | ``			 * matched anything and `clone` below was the only arm that ever fired.`` |
|        - | 1186 | `			 * The name table IS the resolution here: php looks the callee up in a` |
|        - | 1187 | `			 * function table that is fully populated at compile time, and PHL's is` |
|        - | 1188 | `			 * not — the ~650 core builtins register in PH7_VmMakeReady, which runs` |
|        - | 1189 | `			 * AFTER compilation (see the redeclaration guard near the top of this` |
|        - | 1190 | ``			 * file), so hHostFunction has no `strlen` to find. */`` |
|        - | 1191 | `			SyString sName;` |
|      140 | 1192 | `			GenStateCallBuiltinName(pBase->pLeft,&sName);` |
|      140 | 1193 | `			if( GenStateCallIsSpecialized(&(*pGen),pBase,&sName) ){` |
|        6 | 1194 | `				zMsg = "Cannot use result of built-in function in write context";` |
|        6 | 1195 | `			}` |
|       78 | 1196 | `		}else if( pBase->pOp->iOp == EXPR_OP_CLONE ){` |
|        - | 1197 | ``			/* php 8.5 implements `clone` AS a function, so a write through its result`` |
|        - | 1198 | `			 * takes the internal-function wording rather than the temporary one. */` |
|        3 | 1199 | `			zMsg = "Cannot use result of built-in function in write context";` |
|        2 | 1200 | `		}else{` |
|        - | 1201 | ``			/* `new`, and every other computed base. */`` |
|        8 | 1202 | `			zMsg = "Cannot use temporary expression in write context";` |
|        - | 1203 | `		}` |
|        - | 1204 | `	}` |
|   159875 | 1205 | `	if( zMsg == 0 ){` |
|   159841 | 1206 | `		return SXRET_OK;` |
|        - | 1207 | `	}` |
|       38 | 1208 | `	rc = PH7_GenCompileError(&(*pGen),E_ERROR,` |
|       34 | 1209 | `		pTarget->pStart ? pTarget->pStart->nLine : 0,"%s",zMsg);` |
|       38 | 1210 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_INVALID;` |
|   498704 | 1211 | `}` |
|        - | 1212 | `/*` |
|        - | 1213 | ` * What emitting a call's ARGUMENT LIST decided, handed back to the CALL codegen.` |
|        - | 1214 | ` * The arguments are emitted from their own routine because php evaluates them` |
|        - | 1215 | ` * AFTER the callee has been resolved, so this runs between the callee's emission` |
|        - | 1216 | ` * and the OP_CALL — see GenStateEmitCallArgs.` |
|        - | 1217 | ` */` |
|        - | 1218 | `typedef struct GenCallArgs GenCallArgs;` |
|        - | 1219 | `struct GenCallArgs {` |
|        - | 1220 | `	sxi32 iP1;      /* OP_CALL.iP1: the compile-time argument count */` |
|        - | 1221 | ``	sxu32 iP2;      /* OP_CALL.iP2: 1 if any argument unpacks (`...$a`) */`` |
|        - | 1222 | `	void *p3;       /* OP_CALL.p3: the VmCallArgMap, which may ALREADY carry the callee's` |
|        - | 1223 | `	                 * namespace qualification — the callee is emitted first now */` |
|        - | 1224 | ``	int bFcc;       /* First-class callable `f(...)`: no arguments, OP_LOAD_FCC follows */`` |
|        - | 1225 | ``	int bAnySpread; /* Any `...` argument (iP2 says the same; kept for the shape masks) */`` |
|        - | 1226 | `};` |
|        - | 1227 | `static sxi32 GenStateEmitCallArgs(ph7_gen_state *pGen,ph7_expr_node *pNode,sxi32 iFlags,` |
|        - | 1228 | `	GenCallArgs *pArgs);` |
|        - | 1229 | `/*` |
|        - | 1230 | ` * Generate bytecode for a given expression tree.` |
|        - | 1231 | ` * If something goes wrong while generating bytecode` |
|        - | 1232 | ` * for the expression tree (A very unlikely scenario)` |
|        - | 1233 | ` * this function takes care of generating the appropriate` |
|        - | 1234 | ` * error message.` |
|        - | 1235 | ` */` |
|  9433880 | 1236 | `static sxi32 GenStateEmitExprCode(` |
|        - | 1237 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - | 1238 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|        - | 1239 | `	sxi32 iFlags /* Control flags */` |
|        - | 1240 | `	)` |
|        5 | 1241 | `{` |
|        - | 1242 | `	VmInstr *pInstr;` |
|        - | 1243 | `	sxu32 nJmpIdx;` |
|  9433885 | 1244 | `	sxi32 iP1 = 0;` |
|  9433885 | 1245 | `	sxu32 iP2 = 0;` |
|  9433885 | 1246 | `	void *p3  = 0;` |
|        - | 1247 | `	sxi32 iVmOp;` |
|        - | 1248 | `	sxi32 rc;` |
|  9433885 | 1249 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
|  9433885 | 1250 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
|  9433885 | 1251 | `	sxu32 nRhsNsBase = 0;` |
|        - | 1252 | ``	/* Consumed here so it describes THIS node only — the direct operand of a `new` —`` |
|        - | 1253 | `	 * and never travels down into the operand's own sub-expressions. */` |
|  9433885 | 1254 | `	int bNewCallee = (iFlags & EXPR_FLAG_NEW_CALLEE) != 0;` |
|  9433885 | 1255 | `	iFlags &= ~EXPR_FLAG_NEW_CALLEE;` |
|  9433885 | 1256 | `	if( pNode->xCode ){` |
|        - | 1257 | `		SyToken *pTmpIn,*pTmpEnd;` |
|        - | 1258 | `		/* Compile node */` |
|  5831229 | 1259 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
|  5831229 | 1260 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
|  5831229 | 1261 | `		RE_SWAP_DELIMITER(pGen);` |
|  5831229 | 1262 | `		return rc;` |
|        - | 1263 | `	}` |
|  3602661 | 1264 | `	if( pNode->pOp == 0 ){` |
|      ! 0 | 1265 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|        - | 1266 | `			"Invalid expression node,PH7 is aborting compilation");` |
|      ! 0 | 1267 | `		return SXERR_ABORT;` |
|        - | 1268 | `	}` |
|  3602661 | 1269 | `	iVmOp = pNode->pOp->iVmOp;` |
|  3602661 | 1270 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|        - | 1271 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|        - | 1272 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|        - | 1273 | `		 * and later errors are still reported. */` |
|        3 | 1274 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|        - | 1275 | `			"The (unset) cast is no longer supported");` |
|        3 | 1276 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 1277 | `			return SXERR_ABORT;` |
|        - | 1278 | `		}` |
|        1 | 1279 | `	}` |
|  3602661 | 1280 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|      184 | 1281 | `		sxu32 nJmp = 0;` |
|        - | 1282 | `		sxu32 nNcNsBase;` |
|        - | 1283 | `		VmInstr *pInstrFix;` |
|        - | 1284 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|        - | 1285 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|        - | 1286 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|        - | 1287 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|        - | 1288 | `		 * stack slot carries a writable nIdx. */` |
|      184 | 1289 | `		if( pNode->pRight ){` |
|        - | 1290 | ``			/* `$a[] ??= v` READS its target before deciding to write, so php refuses the`` |
|        - | 1291 | ``			 * append form anywhere in that target's chain (`$a[][0] ??= v` too), even`` |
|        - | 1292 | ``			 * though the same tag makes every other `[]` on this path a legal write`` |
|        - | 1293 | ``			 * target. Only the container chain is walked — a `[]` inside an INDEX`` |
|        - | 1294 | `			 * expression is an ordinary read and the subscript codegen refuses it. */` |
|      184 | 1295 | `			ph7_expr_node *pTgt = pNode->pRight;` |
|      422 | 1296 | `			while( pTgt && pTgt->pOp && (pTgt->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|      106 | 1297 | `			      \|\| pTgt->pOp->iOp == EXPR_OP_ARROW \|\| pTgt->pOp->iOp == EXPR_OP_DC) ){` |
|      162 | 1298 | `				if( pTgt->pOp->iOp == EXPR_OP_SUBSCRIPT && SySetUsed(&pTgt->aNodeArgs) < 1 ){` |
|      ! 0 | 1299 | `					break;` |
|        - | 1300 | `				}` |
|      162 | 1301 | `				pTgt = pTgt->pLeft;` |
|        4 | 1302 | `			}` |
|      180 | 1303 | `			if( pTgt && pTgt->pOp && pTgt->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|        5 | 1304 | `			 && SySetUsed(&pTgt->aNodeArgs) < 1 ){` |
|      ! 0 | 1305 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,` |
|      ! 0 | 1306 | `					pNode->pRight->pStart ? pNode->pRight->pStart->nLine : 0,` |
|        - | 1307 | `					"Cannot use [] for reading");` |
|      ! 0 | 1308 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1309 | `			}` |
|        - | 1310 | `` 			/* …and only THEN the write-target rules, php's order: `strval(1)[] ??= 3` `` |
|        - | 1311 | ``			 * is the append refusal, not the specialized-builtin one. `??=` compiles`` |
|        - | 1312 | `			 * its own way and so never reached this check at all, which is why` |
|        - | 1313 | ``			 * `(new A)->p ??= 3` and `"lit"->p->q ??= 3` used to run. */`` |
|      184 | 1314 | `			rc = GenStateWriteTargetCheck(&(*pGen),pNode->pRight,0);` |
|      184 | 1315 | `			if( rc != SXRET_OK ){` |
|        3 | 1316 | `				return rc;` |
|        - | 1317 | `			}` |
|      181 | 1318 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      181 | 1319 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|      181 | 1320 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1321 | `				return rc;` |
|        - | 1322 | `			}` |
|      181 | 1323 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|        - | 1324 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|        - | 1325 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|        - | 1326 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|        - | 1327 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|        - | 1328 | `			 * the store, so the parent array does not need to be copied at` |
|        - | 1329 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|        - | 1330 | `			 * cascade for the actual write path stays correct. */` |
|      181 | 1331 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|      181 | 1332 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|      101 | 1333 | `				pInstrFix->iP2 = 3;` |
|       49 | 1334 | `			}` |
|       89 | 1335 | `		}` |
|        - | 1336 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|      181 | 1337 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|        - | 1338 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|      181 | 1339 | `		if( pNode->pLeft ){` |
|      181 | 1340 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|      181 | 1341 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|      181 | 1342 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1343 | `				return rc;` |
|        - | 1344 | `			}` |
|      181 | 1345 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|       89 | 1346 | `		}` |
|        - | 1347 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|      181 | 1348 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|        - | 1349 | `		/* Patch the short-circuit jump to land after the store. */` |
|      181 | 1350 | `		if( nJmp > 0 ){` |
|      181 | 1351 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|      181 | 1352 | `			if( pInstrFix ){` |
|      181 | 1353 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|       89 | 1354 | `			}` |
|       89 | 1355 | `		}` |
|      181 | 1356 | `		return SXRET_OK;` |
|        - | 1357 | `	}` |
|  3602481 | 1358 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|        - | 1359 | `		sxu32 nJz,nJmp;` |
|        - | 1360 | `		sxu32 nTernaryNsBase;` |
|        - | 1361 | `		/* Ternary operator require special handling */` |
|        - | 1362 | `		/* Phase#1: Compile the condition */` |
|    44223 | 1363 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    44223 | 1364 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    44223 | 1365 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1366 | `			return rc;` |
|        - | 1367 | `		}` |
|        - | 1368 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|        - | 1369 | `		 * compiling the condition must short-circuit to the end of the` |
|        - | 1370 | `		 * condition expression, not leak past the ternary. */` |
|    44223 | 1371 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    44223 | 1372 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    44223 | 1373 | `		if( pNode->pLeft ){` |
|        - | 1374 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|        - | 1375 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    44099 | 1376 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|        - | 1377 | `			/* Phase#3: Compile the 'then' expression  */` |
|    44099 | 1378 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    44099 | 1379 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    44099 | 1380 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1381 | `				return rc;` |
|        - | 1382 | `			}` |
|    44099 | 1383 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    22052 | 1384 | `		}else{` |
|        - | 1385 | `			/* Elvis operator: (expr) ?: (else)` |
|        - | 1386 | `			 * Duplicate condition so original value is the 'then' result.` |
|        - | 1387 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      128 | 1388 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      128 | 1389 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|        - | 1390 | `		}` |
|        - | 1391 | `		/* Phase#4: Emit the unconditional jump */` |
|    44223 | 1392 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|        - | 1393 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    44223 | 1394 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    44223 | 1395 | `		if( pInstr ){` |
|    44223 | 1396 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    22109 | 1397 | `		}` |
|    44223 | 1398 | `		if( !pNode->pLeft ){` |
|        - | 1399 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      128 | 1400 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       62 | 1401 | `		}` |
|        - | 1402 | `		/* Phase#6: Compile the 'else' expression */` |
|    44223 | 1403 | `		if( pNode->pRight ){` |
|    44223 | 1404 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    44223 | 1405 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    44223 | 1406 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1407 | `				return rc;` |
|        - | 1408 | `			}` |
|    44223 | 1409 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    22109 | 1410 | `		}` |
|    44223 | 1411 | `		if( nJmp > 0 ){` |
|        - | 1412 | `			/* Phase#7: Fix the unconditional jump */` |
|    44223 | 1413 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    44223 | 1414 | `			if( pInstr ){` |
|    44223 | 1415 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    22109 | 1416 | `			}` |
|    22109 | 1417 | `		}` |
|        - | 1418 | `		/* All done */` |
|    44223 | 1419 | `		return SXRET_OK;` |
|        - | 1420 | `	}` |
|  3558263 | 1421 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
|        - | 1422 | ``		/* PHP 8.5 pipe: `$lhs \|> $rhs` invokes the RHS callable with the LHS`` |
|        - | 1423 | ``		 * value as its sole argument [i.e. `$rhs($lhs)`]. Evaluate the LHS (the`` |
|        - | 1424 | `		 * argument) first, then the RHS callable, then emit a one-argument` |
|        - | 1425 | `		 * OP_CALL — the same stack shape the function-call path builds (the` |
|        - | 1426 | `		 * argument sits below the callee). The RHS is any callable expression:` |
|        - | 1427 | ``		 * an FCC `f(...)` (an OP_LOAD_FCC Closure), a closure variable, an`` |
|        - | 1428 | ``		 * `[obj,method]` pair, or a callable string. */`` |
|        - | 1429 | `		sxu32 nPipeNsBase;` |
|       27 | 1430 | `		sxi32 iOperandFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE\|EXPR_FLAG_RDONLY_LOAD);` |
|       27 | 1431 | `		if( pNode->pLeft == 0 \|\| pNode->pRight == 0 ){` |
|      ! 0 | 1432 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|        - | 1433 | `				"'\|>': Missing operand");` |
|      ! 0 | 1434 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1435 | `		}` |
|        - | 1436 | `		/* Argument: the LHS value. */` |
|       27 | 1437 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       27 | 1438 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iOperandFlags);` |
|       27 | 1439 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1440 | `			return rc;` |
|        - | 1441 | `		}` |
|       27 | 1442 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|        - | 1443 | `		/* Callable: the RHS. */` |
|       27 | 1444 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       27 | 1445 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iOperandFlags);` |
|       27 | 1446 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 1447 | `			return rc;` |
|        - | 1448 | `		}` |
|       27 | 1449 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|        - | 1450 | `		/* Invoke the callable with the single piped argument. */` |
|       27 | 1451 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|       27 | 1452 | `		return SXRET_OK;` |
|        - | 1453 | `	}` |
|  3558237 | 1454 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|        - | 1455 | `	/* Generate code for the left tree */` |
|  3558237 | 1456 | `	if( pNode->pLeft ){` |
|  3558209 | 1457 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        - | 1458 | `		GenCallArgs sArgs;` |
|  3558209 | 1459 | `		int bArgsEmitted = 0;` |
|  3558209 | 1460 | ``		sxu32 nNewClassInstr = 0; /* index+1 of a `new` operand's class-name push */`` |
|  3558209 | 1461 | `		SyZero(&sArgs,sizeof(sArgs));` |
|        - | 1462 | `		{` |
|        - | 1463 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|        - | 1464 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|        - | 1465 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|        - | 1466 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|        - | 1467 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|        - | 1468 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|        - | 1469 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|        - | 1470 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
|  3558209 | 1471 | `			sxi32 iLeftFlags = iFlags;` |
|        - | 1472 | `			/* The LHS chain whose subscript reads must be QUIET (LOAD_IDX iP2=8):` |
|        - | 1473 | ``			 * `??`'s left operand, and an isset()/empty() chain's intermediate`` |
|        - | 1474 | `			 * links -- php reads both silently and for the value. */` |
|  3558209 | 1475 | `			sxu32 nQuietLhsFirst = PH7_VmInstrLength(pGen->pVm);` |
|  3558209 | 1476 | `			int bQuietLhs = 0;` |
|        - | 1477 | `			/* D1 commit 2: a deferred element/property call arg records its lvalue chain, but` |
|        - | 1478 | `			 * that chain must be CONTIGUOUS. Only propagate DEFER_ARG to the base when the base` |
|        - | 1479 | `			 * is itself a continuable lvalue — a plain variable (an undefined base auto-defers),` |
|        - | 1480 | ``			 * another subscript, or a `->` member. If the base is anything else (most importantly`` |
|        - | 1481 | ``			 * a method CALL, e.g. `$o->items()->prop` or `$r->attributes->item(0)->nodeName`),`` |
|        - | 1482 | `			 * strip DEFER so that intermediate read is a NORMAL read, not a record-mode carrier. */` |
|  3558209 | 1483 | `			if( iLeftFlags & EXPR_FLAG_DEFER_ARG ){` |
|    30397 | 1484 | `				int bContinuable = pNode->pLeft` |
|    23137 | 1485 | `					&& ( (pNode->pLeft->pOp == 0 && pNode->pLeft->xCode == PH7_CompileVariable)` |
|     8269 | 1486 | `					  \|\| (pNode->pLeft->pOp != 0 && (pNode->pLeft->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|      607 | 1487 | `					                              \|\| pNode->pLeft->pOp->iOp == EXPR_OP_ARROW)) );` |
|    15201 | 1488 | `				if( !bContinuable ){` |
|      291 | 1489 | `					iLeftFlags &= ~EXPR_FLAG_DEFER_ARG;` |
|      143 | 1490 | `				}` |
|     7598 | 1491 | `			}` |
|        - | 1492 | `			/*` |
|        - | 1493 | `			 * An isset()/empty() CHAIN reads its intermediate links for their` |
|        - | 1494 | ``			 * VALUE, not for a truth. php walks `isset($o->a->b)` by fetching`` |
|        - | 1495 | ``			 * `$o->a` in BP_VAR_IS mode -- silent, but a real read that runs`` |
|        - | 1496 | `			 * __isset AND THEN __get (or offsetExists and then offsetGet) --` |
|        - | 1497 | `			 * and only the LAST link answers the isset question. PHL gave every` |
|        - | 1498 | `			 * link the terminal context, so the intermediate pushed a bool and` |
|        - | 1499 | `` 			 * the final `->b` was a property of `true`: `isset($model->rel->id)` `` |
|        - | 1500 | `			 * was FALSE for every class with accessors, and so was` |
|        - | 1501 | ``			 * `isset($container['k']['j'])` over ArrayAccess -- a silently wrong`` |
|        - | 1502 | `			 * guard, not a diagnostic.` |
|        - | 1503 | `			 *` |
|        - | 1504 | ``			 * The intermediate context is `??`'s (PH7_MEMBER_COALESCE for a`` |
|        - | 1505 | `			 * member, LOAD_IDX iP2=8 for a subscript, patched over the emitted` |
|        - | 1506 | `			 * range below), which is exactly "silent, and the value": EMPTY's` |
|        - | 1507 | `			 * would read a shade differently, since a class declaring __get with` |
|        - | 1508 | `			 * no __isset is read through __get for an intermediate link and is` |
|        - | 1509 | `			 * NOT for a terminal isset()/empty().` |
|        - | 1510 | `			 */` |
|  3558204 | 1511 | `			if( (iLeftFlags & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY))` |
|  1785299 | 1512 | `				&& pNode->pOp && pNode->pLeft && pNode->pLeft->pOp` |
|     6276 | 1513 | `				&& GEN_IS_ACCESS_OP(pNode->pOp->iOp)` |
|      156 | 1514 | `				&& GEN_IS_ACCESS_OP(pNode->pLeft->pOp->iOp) ){` |
|      113 | 1515 | `				iLeftFlags &= ~(EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY);` |
|      113 | 1516 | `				iLeftFlags \|= EXPR_FLAG_MEMBER_COALESCE\|EXPR_FLAG_QUIET_VAR;` |
|      113 | 1517 | `				bQuietLhs = 1;` |
|       54 | 1518 | `			}` |
|  3558204 | 1519 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
|  2920707 | 1520 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|  1141640 | 1521 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  1123006 | 1522 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|    38921 | 1523 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|  3538751 | 1524 | `			}else if( iLeftFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|        - | 1525 | ``				/* A SUBSCRIPT intermediate of an unset chain (`unset($a['k']['n'])`) keeps`` |
|        - | 1526 | `				 * the unset context — it must COW-separate the parent and must NOT vivify a` |
|        - | 1527 | `				 * missing key — but it is a READ of the container, not an unset of it. The` |
|        - | 1528 | ``				 * UNSET_BASE context says exactly that: `$a['k']` is loaded, where the`` |
|        - | 1529 | `				 * plain unset context would have removed the ELEMENT (and, for an` |
|        - | 1530 | `				 * ArrayAccess base, called offsetUnset() on the intermediate key). */` |
|      295 | 1531 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|      295 | 1532 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_UNSET_BASE;` |
|      145 | 1533 | `			}` |
|        - | 1534 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|        - | 1535 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|        - | 1536 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|        - | 1537 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|        - | 1538 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|        - | 1539 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|        - | 1540 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
|  3558204 | 1541 | `			if( pNode->pOp` |
|  5315506 | 1542 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
|  3536470 | 1543 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  3514665 | 1544 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|    47221 | 1545 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|    23608 | 1546 | `			}` |
|        - | 1547 | ``			/* `++`/`--` mutate their operand in place — the operand is a write`` |
|        - | 1548 | ``			 * lvalue exactly like a compound assign's (`$o->m[0]++` must tag the`` |
|        - | 1549 | ``			 * member base PH7_MEMBER_WRITE the way `$o->m[0] += 1` does: hooked`` |
|        - | 1550 | `			 * properties throw php's Indirect-modification Error, missing ones` |
|        - | 1551 | `			 * auto-vivify). The prec-18 site below handles the assign family;` |
|        - | 1552 | ``			 * `++`/`--` are unary, their operand is pLeft. */`` |
|  3558204 | 1553 | `			if( pNode->pOp` |
|  3558209 | 1554 | `				&& (pNode->pOp->iVmOp == PH7_OP_INCR \|\| pNode->pOp->iVmOp == PH7_OP_DECR) ){` |
|        - | 1555 | ``				/* `(new A)->p++` writes through a temporary exactly as `= 1` does. */`` |
|    52621 | 1556 | `				rc = GenStateWriteTargetCheck(&(*pGen),pNode->pLeft,0);` |
|    52621 | 1557 | `				if( rc != SXRET_OK ){` |
|       43 | 1558 | `					return rc;` |
|        - | 1559 | `				}` |
|    52615 | 1560 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE` |
|        - | 1561 | ``					\| EXPR_FLAG_RMW_LOAD /* php warns before seeding `$undef++` */;`` |
|    26305 | 1562 | `			}` |
|        - | 1563 | ``			/* The SOURCE of a `=&` (pLeft, the operands having been swapped in`` |
|        - | 1564 | `			 * parse.c) is compiled in WRITE context by php too —` |
|        - | 1565 | ``			 * `zend_compile_var(source, BP_VAR_W, 1)` — which is what makes`` |
|        - | 1566 | ``			 * `$r =& $undef` and `$r =& $a[5]` CREATE the thing they bind to,`` |
|        - | 1567 | `			 * silently. PHL READ it, so both warned about what was missing and then` |
|        - | 1568 | `			 * refused the bind outright, leaving $r undefined as well. No` |
|        - | 1569 | `			 * RMW_LOAD: a bind does not read the source's value, and no` |
|        - | 1570 | `			 * MEMBER_WRITE: a handler-backed native property has no pointer for` |
|        - | 1571 | ``			 * php to hand out either, so `$r =& $iv->s` must keep taking the`` |
|        - | 1572 | `			 * read COPY it takes in php. */` |
|  3558203 | 1573 | `			if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_REF ){` |
|      259 | 1574 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|      127 | 1575 | `			}` |
|        - | 1576 | ``			/* `??` reads its LEFT operand in isset-context: an undefined or`` |
|        - | 1577 | `			 * UNINITIALIZED typed PROPERTY must yield the default rather than a` |
|        - | 1578 | `			 * warning/Error (php). Tag a member-access LHS so its OP_MEMBER takes` |
|        - | 1579 | `			 * the silent-lookup path (iP2 = ISSET), which still loads a present` |
|        - | 1580 | `			 * value. A SUBSCRIPT LHS is left untagged: LOAD_IDX's ISSET mode means` |
|        - | 1581 | ``			 * offsetExists (a bool), but `$o[$k] ?? d` needs the offsetGet value —`` |
|        - | 1582 | `			 * that path is already handled correctly by OP_NULLC. */` |
|  3558203 | 1583 | `			if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC && pNode->pLeft ){` |
|        - | 1584 | ``				/* php reads the ENTIRE left operand of `??` in isset-context: no`` |
|        - | 1585 | ``				 * "Undefined variable" for `$x ?? d` OR for the base of`` |
|        - | 1586 | ``				 * `$x['k'] ?? d`. QUIET_VAR silences the variable read wherever it`` |
|        - | 1587 | `				 * sits in the chain. */` |
|      447 | 1588 | `				iLeftFlags \|= EXPR_FLAG_QUIET_VAR;` |
|      447 | 1589 | `				bQuietLhs = 1;` |
|      442 | 1590 | `				if( pNode->pLeft->pOp` |
|      522 | 1591 | `					&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|      308 | 1592 | `						\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|      242 | 1593 | `						\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|        - | 1594 | `					/* A member-access LHS additionally takes OP_MEMBER's SILENT` |
|        - | 1595 | `					 * lookup so an uninitialized typed property yields the default` |
|        - | 1596 | `					 * instead of an Error. It used to borrow isset()'s context for` |
|        - | 1597 | `					 * that, which is the same mistake the comment below records for` |
|        - | 1598 | `					 * subscripts: silence is shared, but isset() context makes every` |
|        - | 1599 | ``					 * ACCESSOR answer a truth, and `$o->p ?? d` needs the accessor's`` |
|        - | 1600 | ``					 * VALUE — so `??` has its own member context. A SUBSCRIPT LHS`` |
|        - | 1601 | `					 * still takes neither: LOAD_IDX's ISSET mode means offsetExists` |
|        - | 1602 | ``					 * (a bool), while `$o[$k] ?? d` needs the offsetGet value —`` |
|        - | 1603 | `					 * OP_NULLC already handles that path. */` |
|      138 | 1604 | `					iLeftFlags \|= EXPR_FLAG_MEMBER_COALESCE;` |
|       67 | 1605 | `				}` |
|      221 | 1606 | `			}` |
|  3558203 | 1607 | `			if( iVmOp == PH7_OP_ERR_CTRL ){` |
|        - | 1608 | `				/* '@' must suppress the diagnostics raised WHILE its operand runs, so` |
|        - | 1609 | `				 * open the window here; the trailing emit below closes it (iP1 = 0). */` |
|    18283 | 1610 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_ERR_CTRL,1,0,0,0);` |
|     9139 | 1611 | `			}` |
|  3558203 | 1612 | `			if( iVmOp == PH7_OP_NEW ){` |
|        - | 1613 | ``				/* Mark the direct operand so a call node under it (`new C($a)`) keeps the`` |
|        - | 1614 | `				 * emission order OP_NEW is assembled from — see the bNewCallee branch above. */` |
|    90531 | 1615 | `				iLeftFlags \|= EXPR_FLAG_NEW_CALLEE;` |
|    45263 | 1616 | `			}` |
|  3558203 | 1617 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags\|EXPR_FLAG_RDONLY_LOAD);` |
|  3558203 | 1618 | `			if( rc == SXRET_OK && bQuietLhs ){` |
|        - | 1619 | `				/* Mark EVERY subscript read in the quiet left chain (iP2=8).` |
|        - | 1620 | `				 * Peeking at the next instruction only catches the OUTERMOST` |
|        - | 1621 | ``				 * access, so `$d['x']['y'] ?? $v` still warned for the inner one;`` |
|        - | 1622 | `				 * and a forward scan would be unsound, since an unrelated sibling` |
|        - | 1623 | ``				 * access can sit immediately before a coalesce (`f($a['k'],`` |
|        - | 1624 | ``				 * $b['j'] ?? 1)`). The compiler knows the real extent, so it marks`` |
|        - | 1625 | `				 * the range it just emitted. Write-context codes (1/3/5) belong to` |
|        - | 1626 | ``				 * `??=` and keep their meaning. */`` |
|      555 | 1627 | `				sxu32 nEnd = PH7_VmInstrLength(pGen->pVm);` |
|        - | 1628 | `				sxu32 nAt;` |
|     2337 | 1629 | `				for( nAt = nQuietLhsFirst ; nAt < nEnd ; ++nAt ){` |
|     1787 | 1630 | `					VmInstr *pFix = PH7_VmGetInstr(pGen->pVm,nAt);` |
|     1787 | 1631 | `					if( pFix && pFix->iOp == PH7_OP_LOAD_IDX && pFix->iP2 == 0 ){` |
|      233 | 1632 | `						pFix->iP2 = 8;` |
|      114 | 1633 | `					}` |
|      896 | 1634 | `				}` |
|      275 | 1635 | `			}` |
|        - | 1636 | `		}` |
|  3558203 | 1637 | `		if( rc != SXRET_OK ){` |
|       67 | 1638 | `			return rc;` |
|        - | 1639 | `		}` |
|  3558141 | 1640 | `		if( !bIsChainOp ){` |
|        - | 1641 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|        - | 1642 | `			 * target the end of that LHS chain, which is right here. */` |
|  2407869 | 1643 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|  1203932 | 1644 | `		}` |
|  3558141 | 1645 | `		if( iVmOp == PH7_OP_CALL ){` |
|   853639 | 1646 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   853639 | 1647 | `			if( pInstr ){` |
|   853639 | 1648 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|   813303 | 1649 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|        - | 1650 | `					sxu32 nQual;` |
|   813303 | 1651 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|        - | 1652 | `					/* Prevent constant expansion but preserve the absolute flag` |
|        - | 1653 | `					 * so the later NEW handler (if any) can see it. */` |
|   813303 | 1654 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|        - | 1655 | `					/* Namespace-qualify the function name for CALL, unless the` |
|        - | 1656 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|        - | 1657 | `					 * imports — class imports must NOT affect function` |
|        - | 1658 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|        - | 1659 | `					 * before NEW; we store the original literal index in the` |
|        - | 1660 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|        - | 1661 | `					 * the unqualified name and re-qualify with class imports. */` |
|   813303 | 1662 | `					if( bAbsolute ){` |
|       83 | 1663 | `						pInstr->iP2 = (sxi32)nOrig;` |
|       44 | 1664 | `					}else{` |
|   813225 | 1665 | `						int fromImport = 0;` |
|   813225 | 1666 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|   813225 | 1667 | `						pInstr->iP2 = (sxi32)nQual;` |
|   813225 | 1668 | `						if( nQual != nOrig ){` |
|        - | 1669 | `							/* Record the original literal index in the arg map` |
|        - | 1670 | `							 * (NOT in the CALL's iP2 — that is the hasSpread` |
|        - | 1671 | `							 * flag) so the NEW handler can recover the` |
|        - | 1672 | `							 * unqualified name and re-qualify with CLASS` |
|        - | 1673 | `							 * imports. */` |
|      167 | 1674 | `							if( p3 == 0 ){` |
|      167 | 1675 | `								VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      162 | 1676 | `									&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|      167 | 1677 | `								if( pMap ){` |
|      167 | 1678 | `									SyZero(pMap, sizeof(VmCallArgMap));` |
|      167 | 1679 | `									p3 = (void *)pMap;` |
|       81 | 1680 | `								}` |
|       81 | 1681 | `							}` |
|      167 | 1682 | `							if( p3 ){` |
|      167 | 1683 | `								((VmCallArgMap *)p3)->nOrigNameLit = nOrig + 1;` |
|      167 | 1684 | `								if( !fromImport ){` |
|        - | 1685 | `									/* Mark as namespace-qualified */` |
|      143 | 1686 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|       69 | 1687 | `								}` |
|       81 | 1688 | `							}` |
|       81 | 1689 | `						}` |
|        - | 1690 | `					}` |
|   446990 | 1691 | `				}else if( (pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */` |
|    38010 | 1692 | `						&& !(pNode->pLeft && (pNode->pLeft->iFlags & EXPR_NODE_PARENS)))` |
|    22518 | 1693 | `					\|\| pInstr->iOp == PH7_OP_NEW ){` |
|        - | 1694 | `					/* Method call,flag that. But NOT when the callee was an explicitly` |
|        - | 1695 | ``					 * PARENTHESISED member access: `($o->p)(...)` / `($o::$p)(...)` invokes`` |
|        - | 1696 | `					 * the VALUE of the property (php's variable-invocation), so the OP_MEMBER` |
|        - | 1697 | `					 * must stay a plain property READ (leaving the callable on the stack for` |
|        - | 1698 | `					 * OP_CALL to invoke) rather than being rewritten into a method-name` |
|        - | 1699 | ``					 * resolution — the parens are exactly what distinguishes `($o->p)()` from`` |
|        - | 1700 | ``					 * the method call `$o->p()`. */`` |
|    35657 | 1701 | `					pInstr->iP2 = 1;` |
|        - | 1702 | ``					/* A static call with a DYNAMIC method name (`C::$m(...)`): the`` |
|        - | 1703 | ``					 * static-`::` codegen folded the variable NAME into OP_MEMBER->p3`` |
|        - | 1704 | ``					 * as if it were a static-PROPERTY read (`C::$m`), but in a CALL the`` |
|        - | 1705 | `					 * method name is the variable's VALUE. Rebuild the sequence` |
|        - | 1706 | `					 * [class, OP_LOAD $m -> value, OP_MEMBER(static, method)] so the` |
|        - | 1707 | `					 * dynamic name is read off the stack, matching the instance` |
|        - | 1708 | ``					 * (`$o->$m()`) path. iP1==1 marks a static member. */`` |
|    35657 | 1709 | `					if( pInstr->iOp == PH7_OP_MEMBER && pInstr->iP1 == 1 && pInstr->p3 ){` |
|       17 | 1710 | `						void *pDynName = pInstr->p3;` |
|       17 | 1711 | `						(void)PH7_VmPopInstr(pGen->pVm);` |
|       17 | 1712 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,pDynName,0);` |
|       17 | 1713 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_MEMBER,1,PH7_MEMBER_METHOD,0,0);` |
|        8 | 1714 | `					}` |
|    17826 | 1715 | `				}` |
|   426817 | 1716 | `			}` |
|        - | 1717 | `			/* The callee is resolved; NOW emit the arguments. php's order — the callee` |
|        - | 1718 | `			 * first, at its INIT_FCALL / INIT_METHOD_CALL, and the arguments only after` |
|        - | 1719 | ``			 * it — is what makes `(new C)->priv(boom())` report php's `Call to private`` |
|        - | 1720 | `` 			 * method` instead of whatever the argument threw, and what keeps `$o?->m(f())` `` |
|        - | 1721 | ``			 * from running `f()` on a null receiver. It also puts the callee in reach of`` |
|        - | 1722 | `			 * the argument ops one opcode EARLIER than OP_CALL.` |
|        - | 1723 | `			 *` |
|        - | 1724 | `			 * The stack that leaves here is therefore [callee][args…] — the mirror of the` |
|        - | 1725 | `			 * layout OP_CALL's whole dispatch is written against (the method-name pair` |
|        - | 1726 | `			 * below the arguments, the spread runs counted down from the top, the` |
|        - | 1727 | `			 * deferred-argument re-walk). OP_ROT_CALLEE turns the region back over just` |
|        - | 1728 | `			 * before the call, so nothing downstream of it changes. */` |
|   853639 | 1729 | `			if( !bArgsEmitted ){` |
|        - | 1730 | `				int bTwoSlot;` |
|   853639 | 1731 | `				sArgs.p3 = p3; /* the namespace map built just above, if any */` |
|        - | 1732 | `				/* A METHOD callee leaves TWO slots — [receiver][method name] — which` |
|        - | 1733 | `				 * OP_CALL reads as one callee (the receiver answers $this and the` |
|        - | 1734 | `				 * late-static-binding class); anything else leaves one. The instruction` |
|        - | 1735 | `				 * just emitted is what decides it. */` |
|   853639 | 1736 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   871481 | 1737 | `				bTwoSlot = pInstr && pInstr->iOp == PH7_OP_MEMBER` |
|   444659 | 1738 | `					&& pInstr->iP2 == PH7_MEMBER_METHOD` |
|        - | 1739 | `					/* …unless the member NAME was folded into p3 rather than pushed:` |
|        - | 1740 | `					 * that shape pushes the target alone, so the op leaves one slot,` |
|        - | 1741 | `					 * which is the same distinction vm_ops_oo.c makes before popping. */` |
|  1280451 | 1742 | `					&& pInstr->p3 == 0;` |
|        - | 1743 | `				/* Screen the callee HERE, where php screens it: an undefined function, a` |
|        - | 1744 | `				 * callable string/array naming nothing, a value that is not callable at` |
|        - | 1745 | `				 * all. A METHOD callee needs none — its OP_MEMBER just did it, against` |
|        - | 1746 | `				 * the entry it chose. An FCC needs none either: OP_LOAD_FCC runs the same` |
|        - | 1747 | `				 * screen, and nothing runs before it. And with NO arguments the call` |
|        - | 1748 | `				 * itself is already the first thing to happen, so there is nothing to` |
|        - | 1749 | `				 * order and no reason to pay for a second resolution. */` |
|        - | 1750 | `				{` |
|   853639 | 1751 | `					ph7_expr_node **apCallArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   853639 | 1752 | `					sxi32 nCallArg = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|   945472 | 1753 | `					int bNodeFcc = nCallArg == 1 && apCallArg[0]` |
|  1112959 | 1754 | `						&& (apCallArg[0]->iFlags & EXPR_NODE_FCC);` |
|   853639 | 1755 | `					if( bNewCallee ){` |
|        - | 1756 | ``						/* A `new`'s operand: the screen is OP_NEW itself, run with no`` |
|        - | 1757 | `						 * arguments on the stack (iP1 = -1). It asks every refusal the` |
|        - | 1758 | `						 * real pass asks and leaves the class name standing, so the two` |
|        - | 1759 | `						 * cannot disagree. Record where that push is — the NEW codegen` |
|        - | 1760 | `						 * used to find it one instruction behind the trailing OP_CALL,` |
|        - | 1761 | `						 * and the argument list now sits in between. */` |
|    88165 | 1762 | `						nNewClassInstr = PH7_VmInstrLength(pGen->pVm);` |
|    88165 | 1763 | `						if( nCallArg > 0 ){` |
|    85821 | 1764 | `							PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,-1,0,0,0);` |
|    42913 | 1765 | `						}` |
|   809559 | 1766 | `					}else if( nCallArg > 0 && !bTwoSlot && !bNodeFcc ){` |
|   717285 | 1767 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL_INIT,0,` |
|   717250 | 1768 | `							(p3 && ((VmCallArgMap *)p3)->bIsNamespaced) ? 1 : 0,0,0);` |
|   358625 | 1769 | `					}` |
|        - | 1770 | `				}` |
|   853639 | 1771 | `				rc = GenStateEmitCallArgs(&(*pGen),pNode,iFlags,&sArgs);` |
|   853639 | 1772 | `				if( rc != SXRET_OK ){` |
|       10 | 1773 | `					return rc;` |
|        - | 1774 | `				}` |
|   853631 | 1775 | `				iP1 = sArgs.iP1;` |
|   853631 | 1776 | `				iP2 = sArgs.iP2;` |
|   853631 | 1777 | `				p3  = sArgs.p3;` |
|   853631 | 1778 | `				bFcc = sArgs.bFcc;` |
|   853631 | 1779 | `				if( iP1 > 0 \|\| iP2 ){` |
|  1215818 | 1780 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_ROT_CALLEE,iP1,` |
|   810542 | 1781 | `						(iP2 ? PH7_ROT_SPREAD : 0) \| (bTwoSlot ? PH7_ROT_TWOSLOT : 0),0,0);` |
|   405271 | 1782 | `				}` |
|   853631 | 1783 | `				if( bNewCallee && nNewClassInstr > 0 ){` |
|    88165 | 1784 | `					if( p3 == 0 ){` |
|     2311 | 1785 | `						VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|     2306 | 1786 | `							&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|     2311 | 1787 | `						if( pMap ){` |
|     2311 | 1788 | `							SyZero(pMap,sizeof(VmCallArgMap));` |
|     2311 | 1789 | `							p3 = (void *)pMap;` |
|     1153 | 1790 | `						}` |
|     1153 | 1791 | `					}` |
|    88165 | 1792 | `					if( p3 ){` |
|    88165 | 1793 | `						((VmCallArgMap *)p3)->nNewClassInstr = nNewClassInstr;` |
|    44080 | 1794 | `					}` |
|    44080 | 1795 | `				}` |
|   426818 | 1796 | `			}` |
|  3131320 | 1797 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|        - | 1798 | `			ph7_expr_node **apNode;` |
|        - | 1799 | `			sxi32 n;` |
|   249427 | 1800 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|        - | 1801 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|        - | 1802 | `				\|EXPR_FLAG_LOAD_IDX_UNSET_BASE` |
|        - | 1803 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE` |
|        - | 1804 | `				\|EXPR_FLAG_MEMBER_COALESCE` |
|        - | 1805 | `				\|EXPR_FLAG_QUIET_VAR\|EXPR_FLAG_RMW_LOAD\|EXPR_FLAG_DEFER_ARG);` |
|        - | 1806 | `			/* Recurse and generate bytecodes for array index */` |
|   249427 | 1807 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   440229 | 1808 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   190807 | 1809 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   190807 | 1810 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   190807 | 1811 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 1812 | `					return rc;` |
|        - | 1813 | `				}` |
|        - | 1814 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   190807 | 1815 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|    95406 | 1816 | `			}` |
|   249427 | 1817 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   190807 | 1818 | `				iP1 = 1; /* Node have an index associated with it */` |
|    95406 | 1819 | `			}else{` |
|        - | 1820 | ``				/* `[]` names the element a WRITE is about to create, so php allows it`` |
|        - | 1821 | `				 * only where a write lands: an assignment target (plain, compound,` |
|        - | 1822 | ``				 * `=&`, a list()/foreach target) and a by-reference argument. Every`` |
|        - | 1823 | `				 * other placement is a COMPILE error there — PHL accepted them all and` |
|        - | 1824 | ``				 * answered NULL after a PH7-worded notice, so `$x = $a[];`,`` |
|        - | 1825 | ``				 * `isset($a[])` and `unset($a[][0])` were silent no-ops on source php`` |
|        - | 1826 | `				 * refuses to run. A call ARGUMENT is the one shape php also leaves to` |
|        - | 1827 | `				 * runtime (it cannot know the parameter's by-ref-ness at compile time),` |
|        - | 1828 | `				 * which is what DEFER_ARG marks. */` |
|    58625 | 1829 | `				if( iFlags & (EXPR_FLAG_LOAD_IDX_UNSET\|EXPR_FLAG_LOAD_IDX_UNSET_BASE) ){` |
|      ! 0 | 1830 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,` |
|      ! 0 | 1831 | `						pNode->pStart ? pNode->pStart->nLine : 0,` |
|        - | 1832 | `						"Cannot use [] for unsetting");` |
|      ! 0 | 1833 | `					return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1834 | `				}` |
|    58625 | 1835 | `				if( (iFlags & (EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_DEFER_ARG)) == 0 ){` |
|      ! 0 | 1836 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,` |
|      ! 0 | 1837 | `						pNode->pStart ? pNode->pStart->nLine : 0,` |
|        - | 1838 | `						"Cannot use [] for reading");` |
|      ! 0 | 1839 | `					return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 1840 | `				}` |
|        - | 1841 | `			}` |
|   249427 | 1842 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|        - | 1843 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|    12005 | 1844 | `				iP2 = 4;` |
|   243427 | 1845 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|        - | 1846 | `				/* offsetUnset for ArrayAccess; for an array, remove the ELEMENT. */` |
|      219 | 1847 | `				iP2 = 5;` |
|   237320 | 1848 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET_BASE ){` |
|        - | 1849 | `				/* An unset chain's intermediate container: read it, but with the` |
|        - | 1850 | `				 * unset context's COW-separate and no-vivify rules. */` |
|       26 | 1851 | `				iP2 = 10;` |
|   237201 | 1852 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|        - | 1853 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|        - | 1854 | `				 * short-circuit on missing keys without invoking offsetGet` |
|        - | 1855 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|       55 | 1856 | `				iP2 = 6;` |
|   237164 | 1857 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|        - | 1858 | `				/* Create an empty entry when the desired index is not found.` |
|        - | 1859 | ``				 * A read-modify-write target (`$a[k] += v`, `$a[k]++`) creates it`` |
|        - | 1860 | `				 * the same way but READS it first, so php warns about the missing` |
|        - | 1861 | `				 * key before seeding it — the RMW context says which of the two` |
|        - | 1862 | `				 * this is (VM_IDX_CTX_RMW). The flag rides the whole LHS chain, so` |
|        - | 1863 | `				 * an intermediate level gets it too, as php's BP_VAR_RW fetch does. */` |
|   157805 | 1864 | `				iP2 = (iFlags & EXPR_FLAG_RMW_LOAD) ? VM_IDX_CTX_RMW : 1;` |
|   158239 | 1865 | `			}else if( iFlags & EXPR_FLAG_DEFER_ARG ){` |
|        - | 1866 | `				/* D1 commit 2: deferred by-ref/by-value element arg. Behaves as a read but,` |
|        - | 1867 | `				 * on a lookup miss, records the lvalue path instead of warning; OP_CALL` |
|        - | 1868 | `				 * re-walks it in vivify (by-ref) or read+warn (by-value) mode. */` |
|    12795 | 1869 | `				iP2 = 9;` |
|     6400 | 1870 | `			}` |
|  2579796 | 1871 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|        - | 1872 | `			/* POP the left node — its answer is dropped exactly as a statement's is,` |
|        - | 1873 | `			 * so a #[\NoDiscard] callee warns for it too (php warns for every` |
|        - | 1874 | ``			 * element of a `for` clause list, not just the last). */`` |
|       14 | 1875 | `			GenStateMarkDiscardedCall(&(*pGen));` |
|       14 | 1876 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        6 | 1877 | `		}` |
|  1779064 | 1878 | `	}` |
|  3558161 | 1879 | `	rc = SXRET_OK;` |
|  3558161 | 1880 | `	nJmpIdx = 0;` |
|        - | 1881 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|        - | 1882 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|        - | 1883 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  3558161 | 1884 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|     3639 | 1885 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     3639 | 1886 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     3639 | 1887 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     3639 | 1888 | `			int isSpecial = 0;` |
|     3639 | 1889 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|     3491 | 1890 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|     3491 | 1891 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|     3486 | 1892 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     3295 | 1893 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     1655 | 1894 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      455 | 1895 | `					isSpecial = 1;` |
|      225 | 1896 | `				}` |
|     1780 | 1897 | `			}` |
|     3713 | 1898 | `			pInstr->iP1 = 0;` |
|        - | 1899 | ``			/* A leading `\` (NSSEP) makes the class name ABSOLUTE: it names the`` |
|        - | 1900 | `			 * global class, so it must NOT be re-qualified with the current namespace.` |
|        - | 1901 | ``			 * `\Closure::bind(...)` inside `namespace X` is Closure, not X\Closure.`` |
|        - | 1902 | ``			 * (Multi-component `\A\B::m` already resolves absolutely because its`` |
|        - | 1903 | ``			 * literal keeps a backslash; the single-component `\Closure` lost it.) */`` |
|        - | 1904 | `			{` |
|     5493 | 1905 | `				int bAbsolute = (pNode->pLeft && pNode->pLeft->pStart` |
|     5340 | 1906 | `					&& (pNode->pLeft->pStart->nType & PH7_TK_NSSEP));` |
|     3565 | 1907 | `				if( !isSpecial && !bAbsolute ){` |
|     3079 | 1908 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     1537 | 1909 | `				}` |
|        - | 1910 | `			}` |
|        - | 1911 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|        - | 1912 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|     3565 | 1913 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|     3115 | 1914 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|     3115 | 1915 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|      224 | 1916 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|      137 | 1917 | `					return SXRET_OK;` |
|        - | 1918 | `				}` |
|     1489 | 1919 | `			}` |
|     1714 | 1920 | `		}` |
|     1793 | 1921 | `	}` |
|        - | 1922 | `	/* Generate code for the right tree */` |
|  3558001 | 1923 | `	if( pNode->pRight ){` |
|  2012629 | 1924 | `		if( iVmOp == PH7_OP_LAND ){` |
|        - | 1925 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    98265 | 1926 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  1963499 | 1927 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|        - | 1928 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    69177 | 1929 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  1879783 | 1930 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|        - | 1931 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|      447 | 1932 | `			iVmOp = 0; /* No binary operator to emit */` |
|      447 | 1933 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  1845047 | 1934 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|        - | 1935 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|        - | 1936 | `			 * the entire containing postfix chain to null. The jump target is` |
|        - | 1937 | `			 * patched later by the innermost non-chain ancestor (or by` |
|        - | 1938 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|        - | 1939 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|        - | 1940 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|      147 | 1941 | `			sxu32 nNsJmp = 0;` |
|      147 | 1942 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|      147 | 1943 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  1844684 | 1944 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */` |
|  1429595 | 1945 | ``			\|\| pNode->pOp->iOp == EXPR_OP_REF /* `=&` reference bind */ ){`` |
|        - | 1946 | `` 			/* The lvalue is the RIGHT operand (prec-18 ops are right-associative; `=&` `` |
|        - | 1947 | `			 * swaps its operands in parse.c so its target is pRight too). Mark it a write` |
|        - | 1948 | `			 * target so a missing base (the container of a subscript-write, or a bare` |
|        - | 1949 | `` 			 * `$o->p`) is auto-created — PHP auto-vivifies on a plain write AND on a `=&` `` |
|        - | 1950 | ``			 * bind (`$a[0] =& $x` creates $a as [0 => &$x], it does not warn). */`` |
|        - | 1951 | `			/* php's compile-time write-target rules first ($this, a temporary base,` |
|        - | 1952 | `			 * the call that is the target itself). */` |
|   830295 | 1953 | `			rc = GenStateWriteTargetCheck(&(*pGen),pNode->pRight,0);` |
|   830295 | 1954 | `			if( rc != SXRET_OK ){` |
|       22 | 1955 | `				return rc;` |
|        - | 1956 | `			}` |
|   830277 | 1957 | `			if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|        - | 1958 | `				/* php compiles a reference SOURCE in write context too` |
|        - | 1959 | `` 				 * (`zend_compile_var(source, BP_VAR_W, 1)`), so `$r =& (new A)->p` `` |
|        - | 1960 | ``				 * and `$r =& (clone $o)->p` are the same two compile refusals a`` |
|        - | 1961 | `				 * write to them would be. Only the call-as-target question is not` |
|        - | 1962 | ``				 * asked here: `$r =& f()` is legal. The operands were swapped in`` |
|        - | 1963 | `				 * parse.c, so the source is pLeft. */` |
|      257 | 1964 | `				rc = GenStateWriteTargetCheck(&(*pGen),pNode->pLeft,PH7_WTC_REFSRC);` |
|      257 | 1965 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 1966 | `					return rc;` |
|        - | 1967 | `				}` |
|      126 | 1968 | `			}` |
|   830277 | 1969 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|   830277 | 1970 | `			if( iVmOp != PH7_OP_STORE && pNode->pOp->iOp != EXPR_OP_REF ){` |
|        - | 1971 | ``				/* COMPOUND assignment (`.=`, `+=`, ...) READS the target first, so`` |
|        - | 1972 | ``				 * php warns when it is undefined and then seeds it; a plain `=` and a`` |
|        - | 1973 | ``				 * `=&` rebind write without reading the target and stay silent. */`` |
|    22797 | 1974 | `				iFlags \|= EXPR_FLAG_RMW_LOAD;` |
|    11396 | 1975 | `			}` |
|   415136 | 1976 | `		}` |
|  2012611 | 1977 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  2012611 | 1978 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_RDONLY_LOAD);` |
|  2012611 | 1979 | `		if( !bIsChainOp ){` |
|        - | 1980 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|        - | 1981 | `			 * operator instruction is emitted. */` |
|  1965527 | 1982 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|   982761 | 1983 | `		}` |
|  2012611 | 1984 | `		if( iVmOp == PH7_OP_STORE ){` |
|   807233 | 1985 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|   807186 | 1986 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|        - | 1987 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|        - | 1988 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|        - | 1989 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|        - | 1990 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|        - | 1991 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|        - | 1992 | `				 */` |
|      241 | 1993 | `				iVmOp = 0;` |
|   807115 | 1994 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|   806997 | 1995 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|        - | 1996 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|     1621 | 1997 | `					iP2 = 1;` |
|      813 | 1998 | `				}else{` |
|   805381 | 1999 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|        - | 2000 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|   157141 | 2001 | `						iVmOp = PH7_OP_STORE_IDX;` |
|   157141 | 2002 | `						iP1 = pInstr->iP1;` |
|    78573 | 2003 | `					}else{` |
|   648245 | 2004 | `						p3 = pInstr->p3;` |
|        - | 2005 | `					}` |
|        - | 2006 | `					/* POP the last dynamic load instruction */` |
|   805381 | 2007 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|        - | 2008 | `				}` |
|   403501 | 2009 | `			}` |
|  1608997 | 2010 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|        - | 2011 | `			/* php records at COMPILE time whether the reference SOURCE was written` |
|        - | 2012 | ``			 * as a CALL (ZEND_RETURNS_FUNCTION), so the bind can raise `Only`` |
|        - | 2013 | ``			 * variables should be assigned by reference` when the callee turns out`` |
|        - | 2014 | `` 			 * not to return by reference. It is the direct call only: `$r =& f()` `` |
|        - | 2015 | ``			 * warns where `$r =& f()[0]` and `$r =& f()->p` are silent. The operands`` |
|        - | 2016 | `			 * were swapped in parse.c, so the source is pLeft. */` |
|      252 | 2017 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
|      193 | 2018 | `			 && pNode->pLeft->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|       29 | 2019 | `				iP1 \|= PH7_STOREREF_CALLSRC;` |
|       13 | 2020 | `			}` |
|        - | 2021 | `			/* Peek first: a member LHS ($o->p =& $x, C::$s =& $x) keeps its` |
|        - | 2022 | `			 * OP_MEMBER in place (it resolves + stashes the target slot at` |
|        - | 2023 | `			 * runtime), unlike the variable/array shapes which fold their load` |
|        - | 2024 | `			 * away. Mirrors the normal member-store fold above (iP2 kept-MEMBER). */` |
|      257 | 2025 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      257 | 2026 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|        - | 2027 | `				/* Tag the member as a reference target and flag STORE_REF (iP2=1)` |
|        - | 2028 | `				 * to take the member-rebind path in the VM. */` |
|       52 | 2029 | `				pInstr->iP2 = PH7_MEMBER_REF_TARGET;` |
|       52 | 2030 | `				iP2 = 1;` |
|       27 | 2031 | `			}else{` |
|      207 | 2032 | `				pInstr = PH7_VmPopInstr(pGen->pVm);` |
|      207 | 2033 | `				if( pInstr ){` |
|      207 | 2034 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|        - | 2035 | `						/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|        - | 2036 | `						 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|        - | 2037 | `						 */` |
|       61 | 2038 | `						iVmOp = PH7_OP_STORE_IDX_REF;` |
|       61 | 2039 | `						iP1 = pInstr->iP1 \| (iP1 & PH7_STOREREF_CALLSRC);` |
|       61 | 2040 | `						iP2 = pInstr->iP2;` |
|       61 | 2041 | `						p3  = pInstr->p3;` |
|       32 | 2042 | `					}else{` |
|      149 | 2043 | `						p3 = pInstr->p3;` |
|        - | 2044 | `					}` |
|      101 | 2045 | `				}` |
|        - | 2046 | `			}` |
|      126 | 2047 | `		}` |
|  1006303 | 2048 | `	}` |
|  3557978 | 2049 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|    46451 | 2050 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|        - | 2051 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|        - | 2052 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|       85 | 2053 | `		iVmOp = 0;` |
|       40 | 2054 | `	}` |
|  3557983 | 2055 | `	if( iVmOp > 0 ){` |
|  3557213 | 2056 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|    52615 | 2057 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|        - | 2058 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|     5805 | 2059 | `				iP1 = 1;` |
|     2905 | 2060 | `			}` |
|  3530908 | 2061 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|        - | 2062 | `			/* Namespace-qualify the class name for NEW */ {` |
|    90451 | 2063 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|    90451 | 2064 | `				VmInstr *pCallInstr = 0;` |
|    90451 | 2065 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|    88165 | 2066 | `					VmCallArgMap *pNewMap = (VmCallArgMap *)pPeek->p3;` |
|    88165 | 2067 | `					pCallInstr = pPeek;` |
|        - | 2068 | `` 					/* The class-name push sits one instruction back only when this `new` `` |
|        - | 2069 | `					 * takes no arguments; with an argument list the reorder puts the whole` |
|        - | 2070 | `					 * list (and its screen and rotation) in between, so the call node` |
|        - | 2071 | `					 * recorded where the push is. */` |
|   132245 | 2072 | `					pPeek = (pNewMap && pNewMap->nNewClassInstr > 0)` |
|    88160 | 2073 | `						? PH7_VmGetInstr(pGen->pVm,pNewMap->nNewClassInstr - 1)` |
|    44080 | 2074 | `						: PH7_VmPeekNextInstr(pGen->pVm);` |
|    44080 | 2075 | `				}` |
|    90451 | 2076 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|    90397 | 2077 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|        - | 2078 | `					sxu32 nLitForClass;` |
|    90397 | 2079 | `					VmCallArgMap *pCallNsMap = pCallInstr ? (VmCallArgMap *)pCallInstr->p3 : 0;` |
|        - | 2080 | `					/* If the CALL handler qualified the name with FUNCTION` |
|        - | 2081 | `					 * imports, recover the original literal (recorded in the` |
|        - | 2082 | `					 * arg map — OP_CALL's iP2 is the hasSpread flag, and` |
|        - | 2083 | `` 					 * misreading it as a literal index made `new C(...$args)` `` |
|        - | 2084 | `					 * fatal with "Class ' ' is not defined") and re-qualify` |
|        - | 2085 | `					 * with class imports. */` |
|    90397 | 2086 | `					if( pCallNsMap && pCallNsMap->nOrigNameLit > 0 ){` |
|       65 | 2087 | `						nLitForClass = pCallNsMap->nOrigNameLit - 1;` |
|       35 | 2088 | `					}else{` |
|    90337 | 2089 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|        - | 2090 | `					}` |
|    90397 | 2091 | `					pPeek->iP1 = 0;` |
|    90397 | 2092 | `					if( !bAbsolute ){` |
|        - | 2093 | `						/* self/static/parent are resolved at runtime against the` |
|        - | 2094 | `						 * current class — never namespace-qualify them (else` |
|        - | 2095 | ``						 * `new self` in namespace N becomes "N\self"). Mirrors the`` |
|        - | 2096 | `						 * instanceof (IS_A) guard below. */` |
|    90341 | 2097 | `						ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nLitForClass);` |
|    90341 | 2098 | `						int isSpecialNew = 0;` |
|    90341 | 2099 | `						if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|    90341 | 2100 | `							const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|    90341 | 2101 | `							sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|    90336 | 2102 | `							if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    90506 | 2103 | `								(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    45342 | 2104 | `								(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|       46 | 2105 | `								isSpecialNew = 1;` |
|       22 | 2106 | `							}` |
|    45168 | 2107 | `						}` |
|    90341 | 2108 | `						if( isSpecialNew ){` |
|       46 | 2109 | `							pPeek->iP2 = (sxi32)nLitForClass;` |
|       24 | 2110 | `						}else{` |
|    90297 | 2111 | `							pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|        - | 2112 | `						}` |
|    45173 | 2113 | `					}else{` |
|       61 | 2114 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|        - | 2115 | `					}` |
|    45196 | 2116 | `				}` |
|        - | 2117 | `			}` |
|    90451 | 2118 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    90451 | 2119 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|        - | 2120 | `				VmInstr *pPrev;` |
|        - | 2121 | `				int bPrevMember;` |
|    88165 | 2122 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|        - | 2123 | `				/* "Was the callee a MEMBER access?" — which, once the reorder puts a` |
|        - | 2124 | `				 * rotation between the callee and its call, is the question the rotation` |
|        - | 2125 | `				 * already answers (a method callee is the two-slot one). */` |
|   132245 | 2126 | `				bPrevMember = pPrev && (pPrev->iOp == PH7_OP_ROT_CALLEE` |
|    85816 | 2127 | `					? (pPrev->iP2 & PH7_ROT_TWOSLOT) != 0` |
|     2344 | 2128 | `					: pPrev->iOp == PH7_OP_MEMBER);` |
|    88165 | 2129 | `				if( !bPrevMember ){` |
|        - | 2130 | `					/* Pop the call instruction, preserve named-arg map and` |
|        - | 2131 | `					 * the hasSpread flag (OP_NEW consumes the spread` |
|        - | 2132 | `					 * accumulator exactly like OP_CALL would have). */` |
|    88165 | 2133 | `					iP1 = pInstr->iP1;` |
|    88165 | 2134 | `					iP2 = pInstr->iP2;` |
|    88165 | 2135 | `					if( pInstr->p3 ){` |
|    88165 | 2136 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|    44080 | 2137 | `					}` |
|    88165 | 2138 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    44080 | 2139 | `				}` |
|    44085 | 2140 | `			}` |
|  3459380 | 2141 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|        - | 2142 | `			/* instanceof: right operand is a class name, not a constant.` |
|        - | 2143 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|    11989 | 2144 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    11989 | 2145 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    11989 | 2146 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    11989 | 2147 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|    11989 | 2148 | `				int isSpecialIs = 0;` |
|    11989 | 2149 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|    11989 | 2150 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|    11989 | 2151 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|    11984 | 2152 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    11987 | 2153 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     5992 | 2154 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|       12 | 2155 | `						isSpecialIs = 1;` |
|        5 | 2156 | `					}` |
|     5992 | 2157 | `				}` |
|    11989 | 2158 | `				pInstr->iP1 = 0;` |
|    11989 | 2159 | `				if( !isSpecialIs && !bAbsolute ){` |
|    11953 | 2160 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     5974 | 2161 | `				}` |
|     5997 | 2162 | `			}` |
|  3408165 | 2163 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|        - | 2164 | `			/* Prevent constant expansion for member/property names.` |
|        - | 2165 | `			 * The right child (member name) was just compiled — its LOADC` |
|        - | 2166 | `			 * should not trigger constant lookup. */` |
|    47089 | 2167 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    47089 | 2168 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    46565 | 2169 | `				pInstr->iP1 = 0;` |
|    23280 | 2170 | `			}` |
|    47089 | 2171 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|        - | 2172 | `				/* Static member access,remember that */` |
|     3479 | 2173 | `				iP1 = 1;` |
|     3479 | 2174 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     3479 | 2175 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|      411 | 2176 | `					p3 = pInstr->p3;` |
|        - | 2177 | ``					/* A `$`-form (`C::$s`, `C::$$x`, `C::${$e}`) is a STATIC PROPERTY`` |
|        - | 2178 | `					 * access, never a constant. A LITERAL name folds into p3 (non-zero)` |
|        - | 2179 | ``					 * and the exec side reads it there; a DYNAMIC name (`$$x`/`${$e}`)`` |
|        - | 2180 | `					 * leaves p3==0 with the computed name on the stack — the SAME shape` |
|        - | 2181 | ``					 * as a bareword constant `C::C`. Mark iP1=2 so exec still routes it`` |
|        - | 2182 | `					 * to the property table (hAttr), not the constant table (hConst). */` |
|      411 | 2183 | `					if( p3 == 0 ){` |
|        8 | 2184 | `						iP1 = 2;` |
|        3 | 2185 | `					}` |
|      411 | 2186 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|      203 | 2187 | `				}` |
|     1737 | 2188 | `			}` |
|        - | 2189 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|        - | 2190 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|        - | 2191 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|        - | 2192 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|    47089 | 2193 | `			if( iP2 == PH7_MEMBER_READ ){` |
|    47089 | 2194 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|      118 | 2195 | `					iP2 = PH7_MEMBER_UNSET;` |
|    47032 | 2196 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|      250 | 2197 | `					iP2 = PH7_MEMBER_ISSET;` |
|    46852 | 2198 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|       45 | 2199 | `					iP2 = PH7_MEMBER_EMPTY;` |
|    46708 | 2200 | `				}else if( iFlags & EXPR_FLAG_MEMBER_COALESCE ){` |
|      233 | 2201 | `					iP2 = PH7_MEMBER_COALESCE;` |
|    46573 | 2202 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|        - | 2203 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|     2207 | 2204 | `					iP2 = PH7_MEMBER_WRITE;` |
|    45358 | 2205 | `				}else if( iFlags & EXPR_FLAG_DEFER_ARG ){` |
|        - | 2206 | `					/* D1 commit 2: deferred by-ref/by-value property arg ($o->p). */` |
|     2411 | 2207 | `					iP2 = PH7_MEMBER_DEFPATH;` |
|     1203 | 2208 | `				}` |
|    23542 | 2209 | `			}` |
|    23542 | 2210 | `		}` |
|        - | 2211 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|        - | 2212 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|        - | 2213 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|        - | 2214 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|        - | 2215 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
|  3557213 | 2216 | `		if( bFcc ){` |
|      258 | 2217 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|        - | 2218 | `			/* php's global fallback applies to a first-class callable exactly as it does` |
|        - | 2219 | ``			 * to the call it stands for: inside a namespace, `strlen(...)` is the global`` |
|        - | 2220 | `			 * function when the current namespace has none. The callee's literal was` |
|        - | 2221 | `			 * namespace-qualified above and the arg map that records it is dropped here` |
|        - | 2222 | `			 * (an FCC has no arguments), so carry the one bit the resolution needs in the` |
|        - | 2223 | ``			 * instruction itself — without it `strlen(...)` in a namespaced file was`` |
|        - | 2224 | ``			 * `Call to undefined function A\B\strlen()`, a fatal on ordinary php. */`` |
|      258 | 2225 | `			iP2 = (p3 && ((VmCallArgMap *)p3)->bIsNamespaced) ? 1 : 0;` |
|      258 | 2226 | `			p3 = 0;` |
|      258 | 2227 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      258 | 2228 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER && pInstr->iP2 == PH7_MEMBER_METHOD ){` |
|        - | 2229 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|        - | 2230 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|        - | 2231 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|        - | 2232 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|      148 | 2233 | `				void *pMemberName = pInstr->p3;` |
|      148 | 2234 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|      148 | 2235 | `				if( pMemberName ){` |
|      ! 0 | 2236 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|      ! 0 | 2237 | `				}` |
|      148 | 2238 | `				iP1 = 2;` |
|       76 | 2239 | `			}else{` |
|        - | 2240 | `				/* Only a METHOD member is the callee's NAME. A parenthesised PROPERTY read` |
|        - | 2241 | ``				 * (`($o->cb)(...)`, `(C::$cb)(...)`) is php's variable-invocation: the member`` |
|        - | 2242 | `				 * op stays, its VALUE is the callable, and this is the iP1=1 wrap. Dropping it` |
|        - | 2243 | `				 * here read the property NAME as a method name and answered` |
|        - | 2244 | ``				 * `Call to undefined method H::cb()` for a closure the object was holding —`` |
|        - | 2245 | `				 * the CALL codegen above already made the distinction (it leaves the member a` |
|        - | 2246 | `				 * plain read for a parenthesised callee) and this branch undid it. */` |
|      113 | 2247 | `				iP1 = 1;` |
|        - | 2248 | `			}` |
|      127 | 2249 | `		}` |
|        - | 2250 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|        - | 2251 | `		 * This is the primary emit path for user-visible calls. */` |
|  3557213 | 2252 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|   943823 | 2253 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|   471909 | 2254 | `		}` |
|        - | 2255 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  3557213 | 2256 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  1778604 | 2257 | `	}` |
|  3557983 | 2258 | `	if( nJmpIdx > 0 ){` |
|        - | 2259 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   167879 | 2260 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   167879 | 2261 | `		if( pInstr ){` |
|   167879 | 2262 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    83937 | 2263 | `		}` |
|    83937 | 2264 | `	}` |
|  3557983 | 2265 | `	return rc;` |
|  4716931 | 2266 | `}` |
|        - | 2267 | `/*` |
|        - | 2268 | ` * Emit a call's ARGUMENT LIST, and decide everything about it the OP_CALL then carries:` |
|        - | 2269 | ` * the count, the unpack flag, the named-argument / assert-source / argument-shape map.` |
|        - | 2270 | ` *` |
|        - | 2271 | ` * Split out of GenStateEmitExprCode because php resolves a callee BEFORE it evaluates` |
|        - | 2272 | ` * the arguments, so this now runs AFTER the callee sub-tree has been emitted (the one` |
|        - | 2273 | `` * exception is a `new`'s constructor list — see EXPR_FLAG_NEW_CALLEE). It is otherwise`` |
|        - | 2274 | ` * the same code, and reads only the node: nothing here inspects the instructions the` |
|        - | 2275 | ` * callee left behind.` |
|        - | 2276 | ` *` |
|        - | 2277 | ` * pArgs->p3 may arrive non-NULL — the callee's own namespace qualification builds the` |
|        - | 2278 | ` * VmCallArgMap first now — and every allocation site below reuses it.` |
|        - | 2279 | ` */` |
|   853634 | 2280 | `static sxi32 GenStateEmitCallArgs(` |
|        - | 2281 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - | 2282 | `	ph7_expr_node *pNode, /* The call node */` |
|        - | 2283 | `	sxi32 iFlags,         /* Control flags of the call site */` |
|        - | 2284 | `	GenCallArgs *pArgs    /* OUT: what the OP_CALL needs */` |
|        - | 2285 | `	)` |
|        5 | 2286 | `{` |
|   853639 | 2287 | `	void *p3 = pArgs->p3;` |
|   853639 | 2288 | `	sxi32 iP1 = 0;` |
|   853639 | 2289 | `	sxu32 iP2 = 0;` |
|   853639 | 2290 | `	int bFcc = 0;` |
|        - | 2291 | `	sxi32 rc;` |
|        - | 2292 | `	ph7_expr_node **apNode;` |
|   853639 | 2293 | `	int hasSpread = 0;` |
|   853639 | 2294 | `	int hasNamed = 0;` |
|   853639 | 2295 | `	sxu32 byRefMask = 0;` |
|        - | 2296 | `	sxi32 nArgs;` |
|        - | 2297 | `	sxi32 n;` |
|   853639 | 2298 | `	int bAnySpread = 0;` |
|        - | 2299 | `	/* Recurse and generate bytecodes for function arguments */` |
|   853639 | 2300 | `	apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   853639 | 2301 | `	nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|        - | 2302 | ``	/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|        - | 2303 | `	 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|        - | 2304 | `	 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|   853639 | 2305 | `	if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|      258 | 2306 | `		bFcc = 1;` |
|      258 | 2307 | `		nArgs = 0;` |
|      127 | 2308 | `	}` |
|        - | 2309 | `	/* Validate argument order like php: no positional argument after a` |
|        - | 2310 | ``	 * named one OR after unpacking, and `name: ...$x` is a parse error. */`` |
|        - | 2311 | `	{` |
|   853639 | 2312 | `		int seenNamed = 0;` |
|   853639 | 2313 | `		int seenSpread = 0;` |
|  2066463 | 2314 | `		for( n = 0; n < nArgs; ++n ){` |
|  1212831 | 2315 | `			if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|      296 | 2316 | `				bAnySpread = 1;` |
|      296 | 2317 | `				seenSpread = 1;` |
|      296 | 2318 | `				if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      ! 0 | 2319 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|        - | 2320 | `						"syntax error, unexpected token \"...\"");` |
|      ! 0 | 2321 | `					return SXERR_SYNTAX;` |
|        4 | 2322 | `				}` |
|  1212685 | 2323 | `			}else if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      629 | 2324 | `				seenNamed = 1;` |
|      629 | 2325 | `				hasNamed = 1;` |
|  1212227 | 2326 | `			}else if( seenNamed ){` |
|        3 | 2327 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|        - | 2328 | `					"Cannot use positional argument after named argument");` |
|        3 | 2329 | `				return SXERR_SYNTAX;` |
|  1211913 | 2330 | `			}else if( seenSpread ){` |
|      ! 0 | 2331 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|        - | 2332 | `					"Cannot use positional argument after argument unpacking");` |
|      ! 0 | 2333 | `				return SXERR_SYNTAX;` |
|        - | 2334 | `			}` |
|   606417 | 2335 | `		}` |
|        - | 2336 | `	}` |
|        - | 2337 | `	/* Read-only load */` |
|   853637 | 2338 | `	iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|        - | 2339 | `	/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|        - | 2340 | ``	 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|        - | 2341 | `	 * objects dispatch to the right method (offsetExists for both;` |
|        - | 2342 | `	 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|   853637 | 2343 | `	if( pNode->pLeft && pNode->pLeft->pStart ){` |
|   853637 | 2344 | `		SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|  1309254 | 2345 | `		int bIsset = pCallName->nByte == 5` |
|   853632 | 2346 | `			&& SyStrnicmp(pCallName->zString,"isset",5) == 0;` |
|  1309254 | 2347 | `		int bEmpty = pCallName->nByte == 5` |
|   853632 | 2348 | `			&& SyStrnicmp(pCallName->zString,"empty",5) == 0;` |
|        - | 2349 | `		/* isset()/empty() are language CONSTRUCTS, not functions: php parses` |
|        - | 2350 | `		 * their argument list in the grammar and a missing operand is a parse` |
|        - | 2351 | `		 * error on the ')'. They compile through this ordinary call loop, which` |
|        - | 2352 | ``		 * never checked arity, so `empty()` quietly evaluated to true and`` |
|        - | 2353 | ``		 * `isset()` to false. (empty() also takes exactly one operand in php,`` |
|        - | 2354 | `		 * unlike isset(), which is variadic.) */` |
|   853637 | 2355 | `		if( (bIsset \|\| bEmpty) && nArgs < 1 ){` |
|        - | 2356 | `			/* php names the ')' itself as the unexpected token, so point at the` |
|        - | 2357 | `			 * node's last token rather than pGen->pIn (which has already moved` |
|        - | 2358 | `			 * past the call to the statement's ';'). */` |
|        5 | 2359 | `			SyToken *pTok = pNode->pEnd;` |
|        5 | 2360 | `			if( pTok && pTok > pNode->pStart && (pTok->nType & PH7_TK_RPAREN) == 0 ){` |
|      ! 0 | 2361 | `				pTok--;` |
|      ! 0 | 2362 | `			}` |
|        5 | 2363 | `			PH7_GenSyntaxError(&(*pGen),pTok,0);` |
|        5 | 2364 | `			return SXERR_ABORT;` |
|        - | 2365 | `		}` |
|   853633 | 2366 | `		if( bIsset ){` |
|    12319 | 2367 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|   847476 | 2368 | `		}else if( bEmpty ){` |
|      187 | 2369 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|       91 | 2370 | `		}` |
|        - | 2371 | `		/* Auto-vivify by-reference out-params of known builtins so an` |
|        - | 2372 | `		 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|        - | 2373 | `		 * $m never assigned) gets a real memobj slot for the builtin to` |
|        - | 2374 | `		 * write back through. Skipped when spread/named args are present:` |
|        - | 2375 | `		 * the compile-time positional index no longer maps to the` |
|        - | 2376 | `		 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|   853633 | 2377 | `		if( !bAnySpread && !hasNamed ){` |
|        - | 2378 | `			SyString sBuiltin;` |
|   853009 | 2379 | `			GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|   853009 | 2380 | `			byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|   426502 | 2381 | `		}` |
|   426814 | 2382 | `	}` |
|  2066453 | 2383 | `	for( n = 0 ; n < nArgs ; ++n ){` |
|  1212827 | 2384 | `		sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  1212827 | 2385 | `		sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|        - | 2386 | `		/* For a by-ref argument position, drop the read-only flag so the` |
|        - | 2387 | `		 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|        - | 2388 | `		 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|        - | 2389 | `		 * auto-vivifies its element and exposes a writable memobj slot for the` |
|        - | 2390 | `		 * builtin to write back through. A plain $var target is unaffected` |
|        - | 2391 | `		 * (iP1=0 either way).` |
|        - | 2392 | `		 *` |
|        - | 2393 | `		 * A PROPERTY target is the one shape this eager path cannot express, so it` |
|        - | 2394 | ``		 * is left to the deferred one below: what php's `FETCH_OBJ_W` does to`` |
|        - | 2395 | ``		 * `$o->p` is not what an ASSIGNMENT does to it — a missing property is`` |
|        - | 2396 | ``		 * CREATED, an overloaded one takes `Indirect modification of overloaded`` |
|        - | 2397 | ``		 * property` and is passed by VALUE (rather than reaching `__set`), and a`` |
|        - | 2398 | ``		 * non-object base is the catchable `Attempt to modify property`. The`` |
|        - | 2399 | `		 * deferred resolver already encodes all of that (VmBindPropByRef) and` |
|        - | 2400 | `		 * already reads a host function's by-ref mask, so routing the property` |
|        - | 2401 | ``		 * shapes through it is what makes `preg_match($p, $s, $this->matches)` —`` |
|        - | 2402 | `		 * the ordinary spelling — write anything at all. */` |
|  1212822 | 2403 | `		if( n < 31 && (byRefMask & (1u<<n))` |
|   615545 | 2404 | `		 && !GenStateArgHasPropertyStep(apNode[n]) ){` |
|    18189 | 2405 | `			iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|    18189 | 2406 | `			iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|     9092 | 2407 | `		}` |
|        - | 2408 | ``		/* D1: a plain `$var` argument may bind to a by-ref parameter whose signature`` |
|        - | 2409 | `		 * is unknown at compile time (forward reference, dynamic call, or method` |
|        - | 2410 | ``		 * dispatch — e.g. PHPUnit's `willReturnReference($undef)`). We used to clear`` |
|        - | 2411 | `		 * the read-only flag here so an undefined variable vivified a real slot the` |
|        - | 2412 | `		 * by-ref write-back could reach — but that also invented the variable as NULL` |
|        - | 2413 | `		 * in the caller when the parameter turned out by-VALUE, and suppressed php's` |
|        - | 2414 | ``		 * `Undefined variable $x` warning. Instead mark it DEFERRED: OP_LOAD leaves an`` |
|        - | 2415 | `		 * undefined variable uncreated and carries a lazy-lvalue marker, and OP_CALL` |
|        - | 2416 | `		 * materializes it ONLY for a by-ref parameter once the callee is resolved` |
|        - | 2417 | `		 * (VmResolveDeferredArgs). Excludes isset()/empty()/unset(), which compile` |
|        - | 2418 | `		 * through this same call loop but must NEVER create their operand, and` |
|        - | 2419 | `		 * spread args (whose elements have no positional index of their own). A NAMED` |
|        - | 2420 | `		 * arg defers too: it binds to the formal its NAME picks, which the resolver` |
|        - | 2421 | `		 * looks up through the call's own argument map — excluding it left` |
|        - | 2422 | ``		 * `r(x: $a['new'])` warning `Undefined array key` and passing NULL where php`` |
|        - | 2423 | ``		 * creates the element for the by-ref parameter `$x`.`` |
|        - | 2424 | `		 *` |
|        - | 2425 | `		 * D1 commit 2: the same reasoning extends to an array-element ($a["k"]) or` |
|        - | 2426 | `		 * property ($o->p) argument — a by-ref user-function parameter must vivify the` |
|        - | 2427 | `		 * element/property, a by-value one must warn and NOT vivify. Those nodes carry a` |
|        - | 2428 | `		 * subscript/arrow operator (pOp != 0). The DEFER flag rides down to the base LOAD` |
|        - | 2429 | `		 * (undefined base auto-defers via commit 1) and to the LOAD_IDX/MEMBER, which` |
|        - | 2430 | ``		 * record the lvalue path on a lookup miss. Static `::` and nullsafe `?->` stay`` |
|        - | 2431 | `		 * eager. */` |
|  1212822 | 2432 | `		if( (iFlags & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_LOAD_IDX_UNSET` |
|   606411 | 2433 | `		               \|EXPR_FLAG_MEMBER_COALESCE)) == 0` |
|  1206540 | 2434 | `		 && (iArgFlags & EXPR_FLAG_RDONLY_LOAD) /* not a known builtin by-ref slot (kept eager above) */` |
|  1191166 | 2435 | `		 && (apNode[n]->iFlags & EXPR_NODE_SPREAD) == 0` |
|  1277504 | 2436 | `		 && ( (apNode[n]->pOp == 0 && apNode[n]->xCode == PH7_CompileVariable)` |
|   859452 | 2437 | `		   \|\| (apNode[n]->pOp != 0 && (apNode[n]->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|   197488 | 2438 | `		                            \|\| apNode[n]->pOp->iOp == EXPR_OP_ARROW)) ) ){` |
|   659461 | 2439 | `			iArgFlags \|= EXPR_FLAG_DEFER_ARG;` |
|   329728 | 2440 | `		}` |
|  1212827 | 2441 | `		rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|  1212827 | 2442 | `		if( rc != SXRET_OK ){` |
|        3 | 2443 | `			return rc;` |
|        - | 2444 | `		}` |
|        - | 2445 | `		/* Each argument is an independent nullsafe scope. */` |
|  1212825 | 2446 | `		GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|  1212825 | 2447 | `		if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|        - | 2448 | `			/* Emit spread opcode to unpack this array argument. iP1 marks a` |
|        - | 2449 | ``			 * source php will unpack BY REFERENCE: only a plain `$var` (php`` |
|        - | 2450 | ``			 * fetches every other shape — `$a[0]`, `$o->p`, `C::$s`, a cast, a`` |
|        - | 2451 | `			 * call — as an R-value, so a by-ref parameter binds its elements in` |
|        - | 2452 | `			 * a temporary and the write-back is invisible). The expander needs` |
|        - | 2453 | `			 * the distinction because it carries each element's slot for the` |
|        - | 2454 | ``			 * by-ref binder; without it `r(...$a[0])` wrote through to the real`` |
|        - | 2455 | `			 * element, which php leaves alone. */` |
|      411 | 2456 | `			PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD,` |
|      292 | 2457 | `				(apNode[n]->pOp == 0 && apNode[n]->xCode == PH7_CompileVariable) ? 1 : 0,` |
|        - | 2458 | `				0, 0, 0);` |
|      296 | 2459 | `			hasSpread = 1;` |
|      146 | 2460 | `		}` |
|   606415 | 2461 | `	}` |
|        - | 2462 | `	/* Total number of given arguments */` |
|   853631 | 2463 | `	iP1 = nArgs;` |
|   853631 | 2464 | `	iP2 = hasSpread;` |
|        - | 2465 | `	/* Build VmCallArgMap if named arguments are present.` |
|        - | 2466 | `	 * Deep-copy name strings so they survive token stream cleanup. */` |
|   853631 | 2467 | `	if( hasNamed ){` |
|      385 | 2468 | `		sxu32 nStrBytes = 0;` |
|        - | 2469 | `		char *zBuf;` |
|     1153 | 2470 | `		for( n = 0; n < nArgs; ++n ){` |
|      773 | 2471 | `			if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      627 | 2472 | `				nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|      311 | 2473 | `			}` |
|      389 | 2474 | `		}` |
|        - | 2475 | `		{` |
|      385 | 2476 | `		sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|      385 | 2477 | `		VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      380 | 2478 | `			&pGen->pVm->sAllocator, mapSize);` |
|      385 | 2479 | `		if( pMap ){` |
|      385 | 2480 | `			SyZero(pMap, mapSize);` |
|      385 | 2481 | `			pMap->bHasNamed = 1;` |
|      385 | 2482 | `			pMap->nTotal = (sxu32)nArgs;` |
|      385 | 2483 | `			pMap->aNames = (SyString *)&pMap[1];` |
|      385 | 2484 | `			zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|     1153 | 2485 | `			for( n = 0; n < nArgs; ++n ){` |
|      773 | 2486 | `				if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|      627 | 2487 | `					sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|      627 | 2488 | `					SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|      627 | 2489 | `					SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|      627 | 2490 | `					zBuf += nb;` |
|      311 | 2491 | `				}` |
|        - | 2492 | `				/* else: aNames[n] remains {NULL, 0} for positional */` |
|      389 | 2493 | `			}` |
|      385 | 2494 | `			p3 = (void *)pMap;` |
|      190 | 2495 | `		}` |
|        - | 2496 | `		}` |
|      190 | 2497 | `	}` |
|        - | 2498 | `	/* assert(): php's compiler keeps a copy of the assertion's AST and a` |
|        - | 2499 | ``	 * failing assert reports its rendered SOURCE (`assert(1 == 2)`), not the`` |
|        - | 2500 | `	 * evaluated value. Render the first argument's token span` |
|        - | 2501 | `	 * into the call map so vm_builtin_assert can echo it. Only a DIRECT` |
|        - | 2502 | `	 * unqualified/absolute call qualifies — matching php, an indirect call` |
|        - | 2503 | `	 * (call_user_func, a callable variable) has no source text and its` |
|        - | 2504 | `	 * AssertionError carries an empty message. A spread first argument is` |
|        - | 2505 | `	 * skipped (its span is the unpacked array, not the assertion). */` |
|   853626 | 2506 | `	if( nArgs >= 1 && !bFcc` |
|   810547 | 2507 | `	 && (apNode[0]->iFlags & EXPR_NODE_SPREAD) == 0 ){` |
|        - | 2508 | `		SyString sCallee;` |
|   810325 | 2509 | `		GenStateCallBuiltinName(pNode->pLeft,&sCallee);` |
|   810320 | 2510 | `		if( sCallee.nByte == sizeof("assert")-1` |
|   517671 | 2511 | `		 && SyStrnicmp(sCallee.zString,"assert",sizeof("assert")-1) == 0 ){` |
|        - | 2512 | `			/* An operator root's pStart/pEnd name only the operator token` |
|        - | 2513 | ``			 * (`1 == 2` roots at `==`); the subtree walk recovers the whole`` |
|        - | 2514 | `			 * raw extent, re-adding parens the grouping pass consumed. */` |
|       67 | 2515 | `			SyToken *pSpanIn = 0;` |
|       67 | 2516 | `			SyToken *pSpanEnd = 0;` |
|        - | 2517 | `			SyBlob sSrc;` |
|       67 | 2518 | `			PH7_ExprSubtreeSpan(apNode[0],&pSpanIn,&pSpanEnd);` |
|       67 | 2519 | `			SyBlobInit(&sSrc,&pGen->pVm->sAllocator);` |
|       67 | 2520 | `			if( pSpanIn && pSpanEnd && pSpanIn < pSpanEnd ){` |
|       67 | 2521 | `				if( apNode[0]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|        - | 2522 | ``					/* php renders the name too: `assert(assertion: 1 == 2)`. */`` |
|        3 | 2523 | `					SyBlobAppend(&sSrc,apNode[0]->sArgName.zString,apNode[0]->sArgName.nByte);` |
|        3 | 2524 | `					SyBlobAppend(&sSrc,": ",2);` |
|        1 | 2525 | `				}` |
|       67 | 2526 | `				PH7_GenRenderAssertSpan(pGen,pSpanIn,pSpanEnd,&sSrc);` |
|       31 | 2527 | `			}` |
|       67 | 2528 | `			if( SyBlobLength(&sSrc) > 0 ){` |
|       98 | 2529 | `				char *zDup = (char *)SyMemBackendDup(&pGen->pVm->sAllocator,` |
|       62 | 2530 | `					SyBlobData(&sSrc),SyBlobLength(&sSrc));` |
|       67 | 2531 | `				if( zDup ){` |
|       67 | 2532 | `					if( p3 == 0 ){` |
|       65 | 2533 | `						VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|       60 | 2534 | `							&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|       65 | 2535 | `						if( pMap ){` |
|       65 | 2536 | `							SyZero(pMap,sizeof(VmCallArgMap));` |
|       65 | 2537 | `							p3 = (void *)pMap;` |
|       30 | 2538 | `						}` |
|       30 | 2539 | `					}` |
|       67 | 2540 | `					if( p3 ){` |
|       67 | 2541 | `						SyStringInitFromBuf(&((VmCallArgMap *)p3)->sAssertSrc,` |
|        - | 2542 | `							zDup,SyBlobLength(&sSrc));` |
|       31 | 2543 | `					}` |
|       31 | 2544 | `				}` |
|       31 | 2545 | `			}` |
|       67 | 2546 | `			SyBlobRelease(&sSrc);` |
|       31 | 2547 | `		}` |
|   405160 | 2548 | `	}` |
|        - | 2549 | `	/* Record each argument's compile-time SHAPE so the by-ref binders can` |
|        - | 2550 | `	 * refuse a non-variable where php refuses it — at the CALL, before the` |
|        - | 2551 | `	 * callee's ZPP runs. Skipped when the call SPREADS (one compile-time` |
|        - | 2552 | `	 * argument becomes N runtime slots, so the positions no longer line up)` |
|        - | 2553 | `	 * or when it carries more arguments than the masks can hold; a call` |
|        - | 2554 | `	 * without the flag keeps the old runtime nIdx test. Named arguments are` |
|        - | 2555 | `	 * fine: they change which FORMAL a slot binds to, not the slot's index. */` |
|   853631 | 2556 | `	if( !bAnySpread && nArgs > 0 && nArgs <= 31 && !bFcc ){` |
|   810269 | 2557 | `		sxu32 nNonLval = 0;` |
|   810269 | 2558 | `		sxu32 nTempCall = 0;` |
|  2022691 | 2559 | `		for( n = 0 ; n < nArgs ; ++n ){` |
|  1212427 | 2560 | `			int iShape = GenStateArgShape(apNode[n]);` |
|  1212427 | 2561 | `			if( iShape == GEN_ARG_NONE ){` |
|   470987 | 2562 | `				nNonLval \|= (1u << n);` |
|   976936 | 2563 | `			}else if( iShape == GEN_ARG_TEMPCALL ){` |
|    51235 | 2564 | `				nTempCall \|= (1u << n);` |
|    25615 | 2565 | `			}` |
|   606216 | 2566 | `		}` |
|   810269 | 2567 | `		if( p3 == 0 ){` |
|   809793 | 2568 | `			VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|   809788 | 2569 | `				&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|   809793 | 2570 | `			if( pMap ){` |
|   809793 | 2571 | `				SyZero(pMap,sizeof(VmCallArgMap));` |
|   809793 | 2572 | `				p3 = (void *)pMap;` |
|   404894 | 2573 | `			}` |
|   404894 | 2574 | `		}` |
|   810269 | 2575 | `		if( p3 ){` |
|   810269 | 2576 | `			((VmCallArgMap *)p3)->bArgShapes = 1;` |
|   810269 | 2577 | `			((VmCallArgMap *)p3)->nNonLvalMask = nNonLval;` |
|   810269 | 2578 | `			((VmCallArgMap *)p3)->nTempCallMask = nTempCall;` |
|   405132 | 2579 | `		}` |
|   405132 | 2580 | `	}` |
|   853631 | 2581 | `	pArgs->iP1 = iP1;` |
|   853631 | 2582 | `	pArgs->iP2 = iP2;` |
|   853631 | 2583 | `	pArgs->p3  = p3;` |
|   853631 | 2584 | `	pArgs->bFcc = bFcc;` |
|   853631 | 2585 | `	pArgs->bAnySpread = bAnySpread;` |
|   853631 | 2586 | `	return SXRET_OK;` |
|   426822 | 2587 | `}` |
|        - | 2588 | `/*` |
|        - | 2589 | ` * Compile a PHP expression.` |
|        - | 2590 | ` * According to the PHP language reference manual:` |
|        - | 2591 | ` *  Expressions are the most important building stones of PHP.` |
|        - | 2592 | ` *  In PHP, almost anything you write is an expression.` |
|        - | 2593 | ` *  The simplest yet most accurate way to define an expression` |
|        - | 2594 | ` *  is "anything that has a value".` |
|        - | 2595 | ` * If something goes wrong while compiling the expression,this` |
|        - | 2596 | ` * function takes care of generating the appropriate error` |
|        - | 2597 | ` * message.` |
|        - | 2598 | ` */` |
|        - | 2599 | `/*` |
|        - | 2600 | ` * Does this expression tree contain a comma OPERATOR node?` |
|        - | 2601 | ` *` |
|        - | 2602 | `` * PH7 shipped `,` as a lowest-precedence binary operator (IMP-0139-COMMA), so`` |
|        - | 2603 | `` * `(1, 2)` and `$x = (f(), $y)` compile and evaluate to the right operand.`` |
|        - | 2604 | ` * php 8 has no comma operator: its grammar only allows comma-separated` |
|        - | 2605 | ` * expression LISTS inside for(...) clauses (call arguments, array literals and` |
|        - | 2606 | ` * list() are split by the parser, never by this node). Accepting it changes the` |
|        - | 2607 | ` * meaning of source php rejects, which §10 classes as a bug — so every context` |
|        - | 2608 | ` * except for() now reports php's parse error.` |
|        - | 2609 | ` */` |
| 30885922 | 2610 | `static int GenStateTreeHasComma(ph7_expr_node *pNode)` |
|        5 | 2611 | `{` |
|        - | 2612 | `	ph7_expr_node **apArg;` |
|        - | 2613 | `	sxu32 n;` |
| 30885927 | 2614 | `	if( pNode == 0 ){` |
| 21793495 | 2615 | `		return 0;` |
|        - | 2616 | `	}` |
|  9092437 | 2617 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|        6 | 2618 | `		return 1;` |
|        - | 2619 | `	}` |
|  9092428 | 2620 | `	if( GenStateTreeHasComma(pNode->pLeft) \|\| GenStateTreeHasComma(pNode->pRight)` |
|  9092429 | 2621 | `	 \|\| GenStateTreeHasComma(pNode->pCond) ){` |
|        6 | 2622 | `		return 1;` |
|        - | 2623 | `	}` |
|  9092429 | 2624 | `	apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
| 10479097 | 2625 | `	for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; n++ ){` |
|  1386673 | 2626 | `		if( GenStateTreeHasComma(apArg[n]) ){` |
|      ! 0 | 2627 | `			return 1;` |
|        - | 2628 | `		}` |
|   693339 | 2629 | `	}` |
|  9092429 | 2630 | `	return 0;` |
| 15442966 | 2631 | `}` |
|  2326726 | 2632 | `PH7_PRIVATE sxi32 PH7_CompileExpr(` |
|        - | 2633 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 2634 | `	sxi32 iFlags,        /* Control flags */` |
|        - | 2635 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|        - | 2636 | `	)` |
|        5 | 2637 | `{` |
|        - | 2638 | `	ph7_expr_node *pRoot;` |
|        - | 2639 | `	SySet sExprNode;` |
|        - | 2640 | `	SyToken *pEnd;` |
|        - | 2641 | `	sxi32 nExpr;` |
|        - | 2642 | `	sxi32 iNest;` |
|        - | 2643 | `	sxi32 rc;` |
|        - | 2644 | `	sxu32 nNullsafeBase;` |
|        - | 2645 | `	/* Initialize worker variables */` |
|  2326731 | 2646 | `	nExpr = 0;` |
|  2326731 | 2647 | `	pRoot = 0;` |
|        - | 2648 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|        - | 2649 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  2326731 | 2650 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  2326731 | 2651 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  2326731 | 2652 | `	SySetAlloc(&sExprNode,0x10);` |
|  2326731 | 2653 | `	rc = SXRET_OK;` |
|        - | 2654 | `	/* Delimit the expression */` |
|  2326731 | 2655 | `	pEnd = pGen->pIn;` |
|  2326731 | 2656 | `	iNest = 0;` |
| 17466263 | 2657 | `	while( pEnd < pGen->pEnd ){` |
| 16476551 | 2658 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|        - | 2659 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|     6335 | 2660 | `			iNest++;` |
| 16473386 | 2661 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|     6345 | 2662 | `			iNest--;` |
| 16467051 | 2663 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  1348881 | 2664 | `			if( iNest <= 0 ){` |
|  1337019 | 2665 | `				break;` |
|        - | 2666 | `			}` |
|     5931 | 2667 | `		}` |
| 15139537 | 2668 | `		pEnd++;` |
|        5 | 2669 | `	}` |
|  2326731 | 2670 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|     2481 | 2671 | `		SyToken *pEnd2 = pGen->pIn;` |
|     2481 | 2672 | `		iNest = 0;` |
|        - | 2673 | `		/* Stop at the first comma */` |
|    18685 | 2674 | `		while( pEnd2 < pEnd ){` |
|    16227 | 2675 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|      327 | 2676 | `				iNest++;` |
|    16066 | 2677 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|      327 | 2678 | `				iNest--;` |
|    15744 | 2679 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|     6133 | 2680 | `				if( iNest <= 0 ){` |
|       21 | 2681 | `					break;` |
|        - | 2682 | `				}` |
|     3055 | 2683 | `			}` |
|    16209 | 2684 | `			pEnd2++;` |
|        5 | 2685 | `		}` |
|     2481 | 2686 | `		if( pEnd2 <pEnd ){` |
|       21 | 2687 | `			pEnd = pEnd2;` |
|        9 | 2688 | `		}` |
|     1238 | 2689 | `	}` |
|  2326731 | 2690 | `	if( pEnd > pGen->pIn ){` |
|  2326717 | 2691 | `		SyToken *pTmp = pGen->pEnd;` |
|        - | 2692 | `		/* Swap delimiter */` |
|  2326717 | 2693 | `		pGen->pEnd = pEnd;` |
|        - | 2694 | `		/* Try to get an expression tree */` |
|  2326717 | 2695 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  2326712 | 2696 | `		if( rc == SXRET_OK && pRoot && pGen->nCommaExprOk < 1` |
|  2274239 | 2697 | `		 && GenStateTreeHasComma(pRoot) ){` |
|        - | 2698 | `			/* php has no comma operator outside a for() clause */` |
|        6 | 2699 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pRoot->pStart->nLine,` |
|        - | 2700 | `				"syntax error, unexpected token \",\"");` |
|        6 | 2701 | `			pGen->pEnd = pTmp;` |
|        6 | 2702 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2703 | `				SySetRelease(&sExprNode);` |
|      ! 0 | 2704 | `				return SXERR_ABORT;` |
|        - | 2705 | `			}` |
|        6 | 2706 | `			pGen->pIn = pEnd;` |
|        6 | 2707 | `			SySetRelease(&sExprNode);` |
|        6 | 2708 | `			SySetTruncate(&pGen->aNullsafeJmp,nNullsafeBase);` |
|        6 | 2709 | `			return SXRET_OK;` |
|        - | 2710 | `		}` |
|  2326713 | 2711 | `		if( rc == SXRET_OK && pRoot ){` |
|  2326491 | 2712 | `			rc = SXRET_OK;` |
|  2326491 | 2713 | `			if( xTreeValidator ){` |
|        - | 2714 | `				/* Call the upper layer validator callback */` |
|   183639 | 2715 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    91817 | 2716 | `			}` |
|  2326491 | 2717 | `			if( rc != SXERR_ABORT ){` |
|        - | 2718 | `				/* Generate code for the given tree */` |
|  2326491 | 2719 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|        - | 2720 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|        - | 2721 | `				 * expression so they short-circuit to its end. */` |
|  2326491 | 2722 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|  1163243 | 2723 | `			}` |
|  2326491 | 2724 | `			nExpr = 1;` |
|  1163243 | 2725 | `		}` |
|        - | 2726 | `		/* Release the whole tree */` |
|  2326713 | 2727 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|        - | 2728 | `		/* Synchronize token stream */` |
|  2326713 | 2729 | `		pGen->pEnd = pTmp;` |
|  2326713 | 2730 | `		pGen->pIn  = pEnd;` |
|  2326713 | 2731 | `		if( rc == SXERR_ABORT ){` |
|       59 | 2732 | `			SySetRelease(&sExprNode);` |
|       59 | 2733 | `			return SXERR_ABORT;` |
|        - | 2734 | `		}` |
|  1163327 | 2735 | `	}` |
|  2326673 | 2736 | `	SySetRelease(&sExprNode);` |
|  2326673 | 2737 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|  1163368 | 2738 | `}` |
|        - | 2739 | `/*` |
|        - | 2740 | ` * Return a pointer to the node construct handler associated` |
|        - | 2741 | ` * with a given node type [i.e: string,integer,float,...].` |
|        - | 2742 | ` */` |
|  1272366 | 2743 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|        5 | 2744 | `{` |
|  1272371 | 2745 | `	if( nNodeType & PH7_TK_NUM ){` |
|        - | 2746 | `		/* Numeric literal: Either real or integer */` |
|   707431 | 2747 | `		return PH7_CompileNumLiteral;` |
|   564945 | 2748 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|        - | 2749 | `		/* Double quoted string */` |
|    62755 | 2750 | `		return PH7_CompileString;` |
|   502195 | 2751 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|        - | 2752 | `		/* Single quoted string */` |
|   502053 | 2753 | `		return PH7_CompileSimpleString;` |
|      147 | 2754 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|        - | 2755 | `		/* Heredoc */` |
|       80 | 2756 | `		return PH7_CompileHereDoc;` |
|       71 | 2757 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|        - | 2758 | `		/* Nowdoc */` |
|       58 | 2759 | `		return PH7_CompileNowDoc;` |
|       15 | 2760 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|        - | 2761 | `		/* Backtick quoted string */` |
|        3 | 2762 | `		return PH7_CompileBacktic;` |
|        - | 2763 | `	}` |
|       12 | 2764 | `	return 0;` |
|   636188 | 2765 | `}` |
|        - | 2766 | `/*` |
|        - | 2767 | ` * Tree validator for unset() arguments — php's write-target rules, then its` |
|        - | 2768 | ``  * "Can't use nullsafe operator in write context", then the grammar: `unset()` `` |
|        - | 2769 | `` * takes a `variable`, so `unset(GK)`, `unset("s")` and `unset(A::K)` are php`` |
|        - | 2770 | ` * PARSE errors where PHL let them reach the VM and answer with a PH7-ism.` |
|        - | 2771 | ` */` |
|      332 | 2772 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|        5 | 2773 | `{` |
|        - | 2774 | `	sxi32 rc;` |
|      337 | 2775 | `	rc = GenStateWriteTargetCheck(&(*pGen),pNode,PH7_WTC_UNSET);` |
|      337 | 2776 | `	if( rc != SXRET_OK ){` |
|        3 | 2777 | `		return rc;` |
|        - | 2778 | `	}` |
|      335 | 2779 | `	if( PH7_ExprContainsNullsafe(pNode) ){` |
|      ! 0 | 2780 | `		rc = PH7_GenCompileError(pGen,E_ERROR,` |
|      ! 0 | 2781 | `			pNode ? pNode->pStart->nLine : 1,` |
|        - | 2782 | `			"Can't use nullsafe operator in write context");` |
|      ! 0 | 2783 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 2784 | `	}` |
|      335 | 2785 | `	if( pNode && PH7_ExprIsModifiableValue(pNode) == FALSE ){` |
|        6 | 2786 | `		return PH7_ExprOperandNotAVariable(pGen,pNode);` |
|        - | 2787 | `	}` |
|      331 | 2788 | `	return SXRET_OK;` |
|      171 | 2789 | `}` |
|        - | 2790 | `/*` |
|        - | 2791 | ` * Compile an unset() statement.` |
|        - | 2792 | ` * unset($var, $arr[$key], ...);` |
|        - | 2793 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|        - | 2794 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|        - | 2795 | ` * parent array before extracting the element to unset.` |
|        - | 2796 | ` */` |
|     3342 | 2797 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|        5 | 2798 | `{` |
|     3347 | 2799 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|     3347 | 2800 | `	sxu32 nIdx = 0;` |
|        - | 2801 | `	SyString sName;` |
|        - | 2802 | `	sxi32 rc;` |
|        - | 2803 | `	/* Jump the 'unset' keyword */` |
|     3347 | 2804 | `	pGen->pIn++;` |
|        - | 2805 | `	/* Save delimiter */` |
|     3347 | 2806 | `	pTmp = pGen->pEnd;` |
|        - | 2807 | `	/* Skip optional opening parenthesis and find the matching close */` |
|     3347 | 2808 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|     3347 | 2809 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|        - | 2810 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|        - | 2811 | `		SyToken *pClose;` |
|     3347 | 2812 | `		pGen->pIn++;   /* Skip '(' */` |
|     3347 | 2813 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|     3347 | 2814 | `		pEnd = pClose; /* Stop at ')' */` |
|     1671 | 2815 | `	}` |
|     3347 | 2816 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|        - | 2817 | `	/* Resolve the 'unset' builtin name once */` |
|     3347 | 2818 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|      539 | 2819 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      539 | 2820 | `		if( pObj == 0 ){` |
|      ! 0 | 2821 | `			return SXERR_ABORT;` |
|        - | 2822 | `		}` |
|      539 | 2823 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      539 | 2824 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      267 | 2825 | `	}` |
|        - | 2826 | `	/* Compile each comma-separated argument */` |
|    11633 | 2827 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|     8293 | 2828 | `		if( pGen->pIn < pNext ){` |
|        - | 2829 | `			/*` |
|        - | 2830 | ``			 * A PLAIN variable (`unset($x)`, exactly two tokens: '$' and the name) drops a`` |
|        - | 2831 | `			 * single NAME binding, which the unset() builtin cannot express — all it ever` |
|        - | 2832 | `			 * receives is the value's slot index, and unsetting the slot destroys whatever` |
|        - | 2833 | `			 * else aliases it. Emit OP_UNSET_VAR with the name instead. Subscripts and` |
|        - | 2834 | ``			 * properties (`unset($a[k])`, `unset($o->p)`) keep the existing path, which`` |
|        - | 2835 | `			 * already removes just the element/property.` |
|        - | 2836 | `			 */` |
|     8288 | 2837 | `			if( &pGen->pIn[2] == pNext` |
|     8122 | 2838 | `				&& (pGen->pIn[0].nType & PH7_TK_DOLLAR)` |
|     7961 | 2839 | `				&& (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|        - | 2840 | `				SyString *pVarName;` |
|        - | 2841 | ``				/* php refuses `unset($this)` where it is written. The tree validator`` |
|        - | 2842 | `				 * cannot see it — this fast path never builds a tree. */` |
|     7954 | 2843 | `				if( pGen->pIn[1].sData.nByte == sizeof("this")-1` |
|     4260 | 2844 | `				 && SyMemcmp((const void *)pGen->pIn[1].sData.zString,` |
|      278 | 2845 | `				             (const void *)"this",sizeof("this")-1) == 0 ){` |
|        3 | 2846 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 2847 | `						"Cannot unset $this");` |
|        3 | 2848 | `					return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|        - | 2849 | `				}` |
|    11933 | 2850 | `				char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|     7952 | 2851 | `					pGen->pIn[1].sData.zString,pGen->pIn[1].sData.nByte);` |
|     7957 | 2852 | `				pVarName = (SyString *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(SyString));` |
|     7957 | 2853 | `				if( zDup == 0 \|\| pVarName == 0 ){` |
|      ! 0 | 2854 | `					PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|        - | 2855 | `						"Fatal, PH7 is running out of memory");` |
|      ! 0 | 2856 | `					return SXERR_ABORT;` |
|        - | 2857 | `				}` |
|     7957 | 2858 | `				SyStringInitFromBuf(pVarName,zDup,pGen->pIn[1].sData.nByte);` |
|     7957 | 2859 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_UNSET_VAR,0,0,pVarName,0);` |
|     7957 | 2860 | `				pGen->pIn = pNext;` |
|     7957 | 2861 | `				if( pGen->pIn < pEnd ){` |
|     4927 | 2862 | `					pGen->pIn++; /* Jump the trailing comma */` |
|     2461 | 2863 | `				}` |
|     7957 | 2864 | `				continue;` |
|        - | 2865 | `			}` |
|      339 | 2866 | `			pGen->pEnd = pNext;` |
|      339 | 2867 | `			rc = PH7_CompileExpr(&(*pGen),` |
|        - | 2868 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|        - | 2869 | `				GenStateUnsetValidator);` |
|      339 | 2870 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 2871 | `				return SXERR_ABORT;` |
|        - | 2872 | `			}` |
|      339 | 2873 | `			if( rc != SXERR_EMPTY ){` |
|        - | 2874 | `				/* Emit call for this single argument */` |
|      337 | 2875 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|      337 | 2876 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|      337 | 2877 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      166 | 2878 | `			}` |
|      167 | 2879 | `		}` |
|        - | 2880 | `		/* Jump trailing commas */` |
|      365 | 2881 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|       29 | 2882 | `			pNext++;` |
|        3 | 2883 | `		}` |
|      339 | 2884 | `		pGen->pIn = pNext;` |
|        5 | 2885 | `	}` |
|        - | 2886 | `	/* Skip past the closing ')' if present */` |
|     3345 | 2887 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|     3345 | 2888 | `		pGen->pIn++;` |
|     1670 | 2889 | `	}` |
|        - | 2890 | `	/* Restore token stream */` |
|     3345 | 2891 | `	pGen->pEnd = pTmp;` |
|     3345 | 2892 | `	return SXRET_OK;` |
|     1676 | 2893 | `}` |
|        - | 2894 | `/*` |
|        - | 2895 | ` * PHP Language construct table.` |
|        - | 2896 | ` */` |
|        - | 2897 | `static const LangConstruct aLangConstruct[] = {` |
|        - | 2898 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|        - | 2899 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|        - | 2900 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|        - | 2901 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|        - | 2902 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|        - | 2903 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|        - | 2904 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|        - | 2905 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|        - | 2906 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|        - | 2907 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|        - | 2908 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|        - | 2909 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|        - | 2910 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|        - | 2911 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|        - | 2912 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|        - | 2913 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|        - | 2914 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|        - | 2915 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|        - | 2916 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|        - | 2917 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|        - | 2918 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|        - | 2919 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|        - | 2920 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|        - | 2921 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|        - | 2922 | `};` |
|        - | 2923 | `/*` |
|        - | 2924 | ` * Return a pointer to the statement handler routine associated` |
|        - | 2925 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|        - | 2926 | ` */` |
|  1149720 | 2927 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|        - | 2928 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|        - | 2929 | `	SyToken *pLookahed  /* Look-ahead token */` |
|        - | 2930 | `	)` |
|        5 | 2931 | `{` |
|  1149725 | 2932 | `	sxu32 n = 0;` |
|  3397357 | 2933 | `	for(;;){` |
|  6794719 | 2934 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|     4955 | 2935 | `			break;` |
|        - | 2936 | `		}` |
|  6789769 | 2937 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|  1144775 | 2938 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|      ! 0 | 2939 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|      ! 0 | 2940 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|        - | 2941 | `					/* 'static' (class context),return null */` |
|      ! 0 | 2942 | `					return 0;` |
|        - | 2943 | `				}` |
|      ! 0 | 2944 | `			}` |
|  1144770 | 2945 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|       58 | 2946 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|       36 | 2947 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|        - | 2948 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|        3 | 2949 | `				return 0;` |
|        - | 2950 | `			}` |
|        - | 2951 | `			/* Return a pointer to the handler.` |
|        - | 2952 | `			*/` |
|  1144773 | 2953 | `			return aLangConstruct[n].xConstruct;` |
|        - | 2954 | `		}` |
|  5644999 | 2955 | `		n++;` |
|        5 | 2956 | `	}` |
|     4955 | 2957 | `	if( pLookahed ){` |
|     4955 | 2958 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|      219 | 2959 | `			return PH7_CompileClassInterface;` |
|     4741 | 2960 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|     3725 | 2961 | `			return PH7_CompileClass;` |
|     1021 | 2962 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      213 | 2963 | `			return PH7_CompileTrait;` |
|        - | 2964 | `		}` |
|        - | 2965 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|        - | 2966 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|        - | 2967 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|        - | 2968 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|      404 | 2969 | `	}` |
|        - | 2970 | `	/* Not a language construct */` |
|      813 | 2971 | `	return 0;` |
|   574865 | 2972 | `}` |
|        - | 2973 | `/*` |
|        - | 2974 | ` * Check if the given keyword is in fact a PHP language construct.` |
|        - | 2975 | ` * Return TRUE on success. FALSE otheriwse.` |
|        - | 2976 | ` */` |
|      810 | 2977 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|        5 | 2978 | `{` |
|        - | 2979 | `	int rc;` |
|      815 | 2980 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|      815 | 2981 | `	if( rc == FALSE ){` |
|      574 | 2982 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|      468 | 2983 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|        - | 2984 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|        - | 2985 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|        - | 2986 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|        - | 2987 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|        - | 2988 | `			*/` |
|        - | 2989 | `			){` |
|      571 | 2990 | `				rc = TRUE;` |
|      283 | 2991 | `		}` |
|      287 | 2992 | `	}` |
|      815 | 2993 | `	return rc;` |
|        5 | 2994 | `}` |
|        - | 2995 | `/*` |
|        - | 2996 | ` * Compile a PHP chunk.` |
|        - | 2997 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|        - | 2998 | ` * takes care of generating the appropriate error message.` |
|        - | 2999 | ` */` |
|        - | 3000 | `/*` |
|        - | 3001 | ` * Update pGen->sPendingDoc for the statement whose first token is` |
|        - | 3002 | ` * pGen->pIn: when a docblock trivia is keyed to that token's index in` |
|        - | 3003 | ` * the chunk token set it becomes the pending docblock. An existing` |
|        - | 3004 | ` * pending docblock is LEFT in place otherwise: Zend keeps the last-seen` |
|        - | 3005 | ` * doc comment until a declaration consumes it, so a docblock survives` |
|        - | 3006 | ` * intervening non-declaration statements.` |
|        - | 3007 | ` */` |
|  2065586 | 3008 | `PH7_PRIVATE void GenStateSetPendingDoc(ph7_gen_state *pGen)` |
|        5 | 3009 | `{` |
|  2065591 | 3010 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|  2065591 | 3011 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|  2065591 | 3012 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|        - | 3013 | `	sxu32 nIdx, n;` |
|  2065586 | 3014 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|     8761 | 3015 | `	 \|\| pGen->pIn < pBase \|\| pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|        - | 3016 | `		/* Re-tokenized substream (string interpolation, synthesized code):` |
|        - | 3017 | `		 * indexes do not map to the sidecar */` |
|  2056835 | 3018 | `		return;` |
|        - | 3019 | `	}` |
|     8761 | 3020 | `	nIdx = (sxu32)(pGen->pIn - pBase);` |
|        - | 3021 | `	/* Attributes must be adjacent to their declaration (unlike docblocks):` |
|        - | 3022 | `	 * reset at every boundary, then collect the groups keyed to this token. */` |
|     8761 | 3023 | `	SySetReset(&pGen->aPendingAttrs);` |
|    40189 | 3024 | `	for( n = 0 ; n < nT ; n++ ){` |
|    31433 | 3025 | `		if( aT[n].nTokIdx != nIdx ){` |
|    30671 | 3026 | `			continue;` |
|        - | 3027 | `		}` |
|      767 | 3028 | `		if( aT[n].iKind == PH7_TRIVIA_DOC ){` |
|      191 | 3029 | `			pGen->sPendingDoc = aT[n].sText;` |
|      674 | 3030 | `		}else if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|      581 | 3031 | `			SySetPut(&pGen->aPendingAttrs,(const void *)&aT[n]);` |
|      288 | 3032 | `		}` |
|      386 | 3033 | `	}` |
|  1032798 | 3034 | `}` |
|        - | 3035 | `/*` |
|        - | 3036 | ` * Hand the pending docblock (if any) to a declaration: duplicate it into` |
|        - | 3037 | ` * the VM allocator (the raw script buffer dies after compilation) and` |
|        - | 3038 | ` * clear the pending slot so sibling declarations do not inherit it.` |
|        - | 3039 | ` */` |
|   174050 | 3040 | `PH7_PRIVATE void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut)` |
|        5 | 3041 | `{` |
|        - | 3042 | `	char *zDup;` |
|   174055 | 3043 | `	if( SyStringLength(&pGen->sPendingDoc) < 1 ){` |
|   173891 | 3044 | `		return;` |
|        - | 3045 | `	}` |
|      251 | 3046 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       82 | 3047 | `		SyStringData(&pGen->sPendingDoc),SyStringLength(&pGen->sPendingDoc));` |
|      169 | 3048 | `	if( zDup ){` |
|      169 | 3049 | `		SyStringInitFromBuf(pOut,zDup,SyStringLength(&pGen->sPendingDoc));` |
|       82 | 3050 | `	}` |
|      169 | 3051 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|    87030 | 3052 | `}` |
|        - | 3053 | `/*` |
|        - | 3054 | ` * Compile one recorded #[...] attribute group (the span between the group` |
|        - | 3055 | ` * delimiters) into ph7_attribute records appended to pOut. The span is` |
|        - | 3056 | ` * duplicated into the VM allocator FIRST (compiled bytecode and interned` |
|        - | 3057 | ` * names may point into the token text, which must outlive the raw script` |
|        - | 3058 | ` * buffer), then re-tokenized on its own. Each argument expression compiles` |
|        - | 3059 | ` * with the container-swap idiom into its own OP_DONE-terminated set,` |
|        - | 3060 | ` * evaluated lazily at ReflectionAttribute time (PHP semantics).` |
|        - | 3061 | ` */` |
|      620 | 3062 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut)` |
|        5 | 3063 | `{` |
|        - | 3064 | `	SySet *pToken;` |
|        - | 3065 | `	SyToken *pIn, *pEnd, *pSavedIn, *pSavedEnd;` |
|        - | 3066 | `	char *zSpan;` |
|      625 | 3067 | `	sxi32 rc = SXRET_OK;` |
|      625 | 3068 | `	if( SyStringLength(&pTrivia->sText) < 1 ){` |
|      ! 0 | 3069 | `		return SXRET_OK;` |
|        - | 3070 | `	}` |
|      935 | 3071 | `	zSpan = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      310 | 3072 | `		SyStringData(&pTrivia->sText),SyStringLength(&pTrivia->sText));` |
|      625 | 3073 | `	if( zSpan == 0 ){` |
|      ! 0 | 3074 | `		return SXRET_OK;` |
|        - | 3075 | `	}` |
|        - | 3076 | `	/* The token set must outlive compilation too: interned operands may` |
|        - | 3077 | `	 * reference token payloads. Pool-allocated, never released — bounded by` |
|        - | 3078 | `	 * the number of attribute declarations in the program. */` |
|      625 | 3079 | `	pToken = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      625 | 3080 | `	if( pToken == 0 ){` |
|      ! 0 | 3081 | `		return SXRET_OK;` |
|        - | 3082 | `	}` |
|      625 | 3083 | `	SySetInit(pToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      625 | 3084 | `	PH7_TokenizePHP(zSpan,SyStringLength(&pTrivia->sText),pTrivia->nLine,pToken,0);` |
|      625 | 3085 | `	pIn = (SyToken *)SySetBasePtr(pToken);` |
|      625 | 3086 | `	pEnd = &pIn[SySetUsed(pToken)];` |
|      625 | 3087 | `	pSavedIn = pGen->pIn;` |
|      625 | 3088 | `	pSavedEnd = pGen->pEnd;` |
|      629 | 3089 | `	while( pIn < pEnd ){` |
|        - | 3090 | `		ph7_attribute sAttr;` |
|        - | 3091 | `		SyBlob sFQN;` |
|      629 | 3092 | `		int bAbsolute = 0;` |
|      629 | 3093 | `		SyZero(&sAttr,sizeof(sAttr));` |
|      629 | 3094 | `		SySetInit(&sAttr.aArgs,&pGen->pVm->sAllocator,sizeof(ph7_attr_arg));` |
|      629 | 3095 | `		sAttr.nLine = pIn->nLine;` |
|      629 | 3096 | `		if( pIn->nType & PH7_TK_NSSEP ){` |
|      467 | 3097 | `			bAbsolute = 1;` |
|      467 | 3098 | `			pIn++;` |
|      231 | 3099 | `		}` |
|      629 | 3100 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|        - | 3101 | ``		/* `#[namespace\Attr]` — the current namespace, absolute from there. */`` |
|      629 | 3102 | `		if( !bAbsolute && GenStateNsRelPrefix(pGen,&pIn,pEnd,&sFQN) ){` |
|      ! 0 | 3103 | `			bAbsolute = 1;` |
|      ! 0 | 3104 | `		}` |
|      629 | 3105 | `		while( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      629 | 3106 | `			SyBlobAppend(&sFQN,pIn->sData.zString,pIn->sData.nByte);` |
|      629 | 3107 | `			pIn++;` |
|      629 | 3108 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|      ! 0 | 3109 | `				SyBlobAppend(&sFQN,"\\",1);` |
|      ! 0 | 3110 | `				pIn++;` |
|      ! 0 | 3111 | `				continue;` |
|        - | 3112 | `			}` |
|      629 | 3113 | `			break;` |
|      ! 0 | 3114 | `		}` |
|      629 | 3115 | `		if( SyBlobLength(&sFQN) < 1 ){` |
|        - | 3116 | `			/* Malformed group: stop quietly (the group was inert trivia before` |
|        - | 3117 | `			 * this feature; never turn it into a new fatal) */` |
|      ! 0 | 3118 | `			SyBlobRelease(&sFQN);` |
|      ! 0 | 3119 | `			break;` |
|        - | 3120 | `		}` |
|        - | 3121 | `		/* Resolve to an FQN: absolute names verbatim; else use-import alias,` |
|        - | 3122 | `		 * else current-namespace prefix (PHP attribute name resolution) */` |
|        - | 3123 | `		{` |
|      629 | 3124 | `			const char *zName = (const char *)SyBlobData(&sFQN);` |
|      629 | 3125 | `			sxu32 nName = SyBlobLength(&sFQN);` |
|      629 | 3126 | `			char *zDup = 0;` |
|      629 | 3127 | `			if( !bAbsolute ){` |
|      165 | 3128 | `				SyHashEntry *pImp = SyHashGet(&pGen->hUseImports,(const void *)zName,nName);` |
|      165 | 3129 | `				if( pImp ){` |
|        3 | 3130 | `					const char *zFqn = (const char *)pImp->pUserData;` |
|        3 | 3131 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFqn,SyStrlen(zFqn));` |
|        3 | 3132 | `					if( zDup ){` |
|        3 | 3133 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyStrlen(zDup));` |
|        2 | 3134 | `					}` |
|      164 | 3135 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|        - | 3136 | `					SyBlob sTmp;` |
|        3 | 3137 | `					SyBlobInit(&sTmp,&pGen->pVm->sAllocator);` |
|        3 | 3138 | `					SyBlobAppend(&sTmp,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|        3 | 3139 | `					SyBlobAppend(&sTmp,"\\",1);` |
|        3 | 3140 | `					SyBlobAppend(&sTmp,zName,nName);` |
|        4 | 3141 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        2 | 3142 | `						(const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|        3 | 3143 | `					if( zDup ){` |
|        3 | 3144 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyBlobLength(&sTmp));` |
|        1 | 3145 | `					}` |
|        3 | 3146 | `					SyBlobRelease(&sTmp);` |
|        1 | 3147 | `				}` |
|       81 | 3148 | `			}` |
|      629 | 3149 | `			if( SyStringLength(&sAttr.sName) < 1 ){` |
|      625 | 3150 | `				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|      625 | 3151 | `				if( zDup ){` |
|      625 | 3152 | `					SyStringInitFromBuf(&sAttr.sName,zDup,nName);` |
|      310 | 3153 | `				}` |
|      310 | 3154 | `			}` |
|        - | 3155 | `		}` |
|      629 | 3156 | `		SyBlobRelease(&sFQN);` |
|      629 | 3157 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|        - | 3158 | `			SyToken *pArgsEnd;` |
|      110 | 3159 | `			pIn++;` |
|      110 | 3160 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pArgsEnd);` |
|      290 | 3161 | `			while( pIn < pArgsEnd ){` |
|      184 | 3162 | `				SyToken *pArgStart = pIn, *pArgStop = pIn;` |
|      184 | 3163 | `				sxi32 iDepth = 0;` |
|        - | 3164 | `				ph7_attr_arg sArgRec;` |
|      632 | 3165 | `				while( pArgStop < pArgsEnd ){` |
|      528 | 3166 | `					if( pArgStop->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|       29 | 3167 | `						iDepth++;` |
|      514 | 3168 | `					}else if( pArgStop->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|       29 | 3169 | `						iDepth--;` |
|      486 | 3170 | `					}else if( (pArgStop->nType & PH7_TK_COMMA) && iDepth == 0 ){` |
|       77 | 3171 | `						break;` |
|        - | 3172 | `					}` |
|      452 | 3173 | `					pArgStop++;` |
|        4 | 3174 | `				}` |
|      184 | 3175 | `				SyZero(&sArgRec,sizeof(sArgRec));` |
|      184 | 3176 | `				SySetInit(&sArgRec.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      180 | 3177 | `				if( pArgStart < pArgStop && (pArgStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|      126 | 3178 | `				 && &pArgStart[1] < pArgStop && (pArgStart[1].nType & PH7_TK_COLON) ){` |
|       41 | 3179 | `					char *zN = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       13 | 3180 | `						pArgStart->sData.zString,pArgStart->sData.nByte);` |
|       28 | 3181 | `					if( zN ){` |
|       28 | 3182 | `						SyStringInitFromBuf(&sArgRec.sName,zN,pArgStart->sData.nByte);` |
|       13 | 3183 | `					}` |
|       28 | 3184 | `					pArgStart += 2;` |
|       13 | 3185 | `				}` |
|      184 | 3186 | `				if( pArgStart < pArgStop ){` |
|        - | 3187 | `					SySet *pInstrContainer;` |
|      184 | 3188 | `					pGen->pIn = pArgStart;` |
|      184 | 3189 | `					pGen->pEnd = pArgStop;` |
|      184 | 3190 | `					pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      184 | 3191 | `					PH7_VmSetByteCodeContainer(pGen->pVm,&sArgRec.aByteCode);` |
|      184 | 3192 | `					rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      184 | 3193 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      184 | 3194 | `					PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      184 | 3195 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3196 | `						pGen->pIn = pSavedIn;` |
|      ! 0 | 3197 | `						pGen->pEnd = pSavedEnd;` |
|      ! 0 | 3198 | `						return SXERR_ABORT;` |
|        - | 3199 | `					}` |
|      184 | 3200 | `					SySetPut(&sAttr.aArgs,(const void *)&sArgRec);` |
|       90 | 3201 | `				}` |
|      184 | 3202 | `				pIn = pArgStop;` |
|      184 | 3203 | `				if( pIn < pArgsEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|       77 | 3204 | `					pIn++;` |
|       38 | 3205 | `				}` |
|        4 | 3206 | `			}` |
|      110 | 3207 | `			pIn = (pArgsEnd < pEnd) ? &pArgsEnd[1] : pEnd;` |
|       53 | 3208 | `		}` |
|      629 | 3209 | `		SySetPut(pOut,(const void *)&sAttr);` |
|      629 | 3210 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        5 | 3211 | `			pIn++;` |
|        5 | 3212 | `			continue;` |
|        - | 3213 | `		}` |
|      625 | 3214 | `		break;` |
|      ! 0 | 3215 | `	}` |
|      625 | 3216 | `	pGen->pIn = pSavedIn;` |
|      625 | 3217 | `	pGen->pEnd = pSavedEnd;` |
|      625 | 3218 | `	return SXRET_OK;` |
|      315 | 3219 | `}` |
|        - | 3220 | `/*` |
|        - | 3221 | ` * Hand the pending attribute groups (if any) to a declaration: compile` |
|        - | 3222 | ` * every recorded group into pOut and clear the pending list.` |
|        - | 3223 | ` */` |
|   174058 | 3224 | `PH7_PRIVATE sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut)` |
|        5 | 3225 | `{` |
|   174063 | 3226 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|        - | 3227 | `	sxu32 n;` |
|        - | 3228 | `	sxi32 rc;` |
|   174639 | 3229 | `	for( n = 0 ; n < SySetUsed(&pGen->aPendingAttrs) ; n++ ){` |
|      581 | 3230 | `		rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|      581 | 3231 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3232 | `			return SXERR_ABORT;` |
|        - | 3233 | `		}` |
|      293 | 3234 | `	}` |
|   174063 | 3235 | `	SySetReset(&pGen->aPendingAttrs);` |
|   174063 | 3236 | `	return SXRET_OK;` |
|    87034 | 3237 | `}` |
|        - | 3238 | `/*` |
|        - | 3239 | ` * Compile the attribute groups keyed to the given token (a parameter's` |
|        - | 3240 | ` * first token inside a signature) into pOut. Parameters are parsed from` |
|        - | 3241 | ` * the main token stream, so the sidecar indexes map directly.` |
|        - | 3242 | ` */` |
|   299888 | 3243 | `PH7_PRIVATE sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut)` |
|        5 | 3244 | `{` |
|   299893 | 3245 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|   299893 | 3246 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|   299893 | 3247 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|        - | 3248 | `	sxu32 nIdx, n;` |
|        - | 3249 | `	sxi32 rc;` |
|   299888 | 3250 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|     1103 | 3251 | `	 \|\| pTok < pBase \|\| pTok >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|   298795 | 3252 | `		return SXRET_OK;` |
|        - | 3253 | `	}` |
|     1103 | 3254 | `	nIdx = (sxu32)(pTok - pBase);` |
|     4783 | 3255 | `	for( n = 0 ; n < nT ; n++ ){` |
|     3685 | 3256 | `		if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|       48 | 3257 | `			rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|       48 | 3258 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3259 | `				return SXERR_ABORT;` |
|        - | 3260 | `			}` |
|       22 | 3261 | `		}` |
|     1845 | 3262 | `	}` |
|     1103 | 3263 | `	return SXRET_OK;` |
|   149949 | 3264 | `}` |
|        - | 3265 | `/*` |
|        - | 3266 | ` * ---------------------------------------------------------------------------` |
|        - | 3267 | ` * Where php's OWN attributes may be written.` |
|        - | 3268 | ` *` |
|        - | 3269 | ` * php's seven internal attribute classes each carry a target mask and a` |
|        - | 3270 | ` * validator, and the engine runs them where the declaration COMPILES: a` |
|        - | 3271 | `` * misplaced `#[\Attribute]`, `#[\Override]` or `#[\NoDiscard]` is a fatal at`` |
|        - | 3272 | ` * the line it sits on, before anything else in the file runs. A USERLAND` |
|        - | 3273 | ` * attribute is different — php checks its mask only when someone asks for it,` |
|        - | 3274 | `` * at `newInstance()` — so this table is closed on purpose and unknown names go`` |
|        - | 3275 | ` * unchecked, which is php's behaviour and not an omission.` |
|        - | 3276 | ` *` |
|        - | 3277 | ` * The masks are the same seven the classes declare (see VmInstallAttributes);` |
|        - | 3278 | ` * they are repeated here because the compiler runs before any class exists.` |
|        - | 3279 | ` * None of the seven is IS_REPEATABLE, so a second one is php's own refusal.` |
|        - | 3280 | ` * ---------------------------------------------------------------------------` |
|        - | 3281 | ` */` |
|        - | 3282 | `/* php's Attribute::TARGET_* names, in bit order (TARGET_CLASS is bit 0). */` |
|        - | 3283 | `static const char *const azGenAttrTarget[] = {` |
|        - | 3284 | `	"class","function","method","property","class constant","parameter","constant"` |
|        - | 3285 | `};` |
|        - | 3286 | `static const struct {` |
|        - | 3287 | `	const char *zName;` |
|        - | 3288 | `	int iMask;` |
|        - | 3289 | `} aGenInternalAttr[] = {` |
|        - | 3290 | `	{ "Attribute",              1  },` |
|        - | 3291 | `	{ "Deprecated",             87 },` |
|        - | 3292 | `	{ "AllowDynamicProperties", 1  },` |
|        - | 3293 | `	{ "SensitiveParameter",     32 },` |
|        - | 3294 | `	{ "ReturnTypeWillChange",   4  },` |
|        - | 3295 | `	{ "Override",               12 },` |
|        - | 3296 | `	{ "NoDiscard",              6  },` |
|        - | 3297 | `};` |
|        - | 3298 | `/*` |
|        - | 3299 | ` * The extra validator php gives three of them, asked only once the target is` |
|        - | 3300 | ` * known to be a CLASS: the mask says "a class" and these say WHICH kinds.` |
|        - | 3301 | ` * Answers php's noun for the refused kind, or 0 when the class is acceptable.` |
|        - | 3302 | ` */` |
|       80 | 3303 | `static const char * GenStateAttrClassRefusal(const char *zAttr,sxi32 iFlags)` |
|        4 | 3304 | `{` |
|        - | 3305 | `	/* zAttr is a row of aGenInternalAttr, so an exact compare is the whole test. */` |
|       84 | 3306 | `	int bAttr = SyStrncmp(zAttr,"Attribute",sizeof("Attribute")) == 0;` |
|       84 | 3307 | `	int bDyn  = SyStrncmp(zAttr,"AllowDynamicProperties",sizeof("AllowDynamicProperties")) == 0;` |
|       84 | 3308 | `	int bDep  = SyStrncmp(zAttr,"Deprecated",sizeof("Deprecated")) == 0;` |
|       84 | 3309 | `	if( !bAttr && !bDyn && !bDep ){` |
|      ! 0 | 3310 | `		return 0;` |
|        - | 3311 | `	}` |
|       84 | 3312 | `	if( iFlags & PH7_CLASS_INTERFACE ){ return "interface"; }` |
|       78 | 3313 | `	if( iFlags & PH7_CLASS_ENUM ){ return "enum"; }` |
|       72 | 3314 | `	if( iFlags & PH7_CLASS_TRAIT ){` |
|        - | 3315 | `		/* php 8.5 DOES mark a deprecated trait; the other two refuse one. */` |
|        7 | 3316 | `		return bDep ? 0 : "trait";` |
|        - | 3317 | `	}` |
|       66 | 3318 | `	if( bAttr ){` |
|        - | 3319 | `		/* An attribute class must be instantiable. */` |
|       47 | 3320 | `		return (iFlags & PH7_CLASS_ABSTRACT) ? "abstract class" : 0;` |
|        - | 3321 | `	}` |
|       21 | 3322 | `	if( bDyn ){` |
|        - | 3323 | `		/* A readonly class has no dynamic property to allow. */` |
|       17 | 3324 | `		return (iFlags & PH7_CLASS_READONLY) ? "readonly class" : 0;` |
|        - | 3325 | `	}` |
|        5 | 3326 | `	return "class";   /* #[\Deprecated] on any other class kind */` |
|       44 | 3327 | `}` |
|        - | 3328 | `/*` |
|        - | 3329 | ` * Validate one declaration's attribute set against php's placement rules.` |
|        - | 3330 | ` *` |
|        - | 3331 | ` * iTarget is the single Attribute::TARGET_* bit php NAMES for this declaration` |
|        - | 3332 | ` * and iAccept the mask it accepts, which differ in exactly one place: a PROMOTED` |
|        - | 3333 | ` * constructor parameter is a parameter and a property both, so it takes either` |
|        - | 3334 | ` * bit while still reporting "parameter". pClassName/iClassFlags describe the` |
|        - | 3335 | ` * subject when the target is a class (0 and 0 otherwise).` |
|        - | 3336 | ` */` |
|   473852 | 3337 | `PH7_PRIVATE sxi32 GenStateCheckAttrPlacement(ph7_gen_state *pGen,SySet *pAttrs,` |
|        - | 3338 | `	int iTarget,int iAccept,const SyString *pClassName,sxi32 iClassFlags)` |
|        5 | 3339 | `{` |
|   473857 | 3340 | `	ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|        - | 3341 | `	sxu32 n,k;` |
|   474399 | 3342 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; ++n ){` |
|      623 | 3343 | `		SyString *pName = &aAttr[n].sName;` |
|        - | 3344 | `		sxu32 iRow;` |
|     3473 | 3345 | `		for( iRow = 0 ; iRow < SX_ARRAYSIZE(aGenInternalAttr) ; ++iRow ){` |
|     3346 | 3346 | `			if( pName->nByte == (sxu32)SyStrlen(aGenInternalAttr[iRow].zName)` |
|     2032 | 3347 | `			 && SyStrnicmp(pName->zString,aGenInternalAttr[iRow].zName,pName->nByte) == 0 ){` |
|      501 | 3348 | `				break;` |
|        - | 3349 | `			}` |
|     1430 | 3350 | `		}` |
|      623 | 3351 | `		if( iRow >= SX_ARRAYSIZE(aGenInternalAttr) ){` |
|      125 | 3352 | `			continue;   /* a userland attribute: judged at newInstance(), not here */` |
|        - | 3353 | `		}` |
|      501 | 3354 | `		if( (aGenInternalAttr[iRow].iMask & iAccept) == 0 ){` |
|        - | 3355 | `			SyBlob sAllowed;` |
|        - | 3356 | `			int iBit;` |
|        - | 3357 | `			sxi32 rc;` |
|       47 | 3358 | `			SyBlobInit(&sAllowed,&pGen->pVm->sAllocator);` |
|      369 | 3359 | `			for( iBit = 0 ; iBit < (int)SX_ARRAYSIZE(azGenAttrTarget) ; iBit++ ){` |
|      323 | 3360 | `				if( (aGenInternalAttr[iRow].iMask & (1 << iBit)) == 0 ){` |
|      239 | 3361 | `					continue;` |
|        - | 3362 | `				}` |
|       85 | 3363 | `				if( SyBlobLength(&sAllowed) > 0 ){` |
|       39 | 3364 | `					SyBlobAppend(&sAllowed,", ",sizeof(", ")-1);` |
|       19 | 3365 | `				}` |
|      127 | 3366 | `				SyBlobAppend(&sAllowed,azGenAttrTarget[iBit],` |
|       84 | 3367 | `					(sxu32)SyStrlen(azGenAttrTarget[iBit]));` |
|       43 | 3368 | `			}` |
|       47 | 3369 | `			SyBlobAppend(&sAllowed,"",sizeof(char));   /* NUL for the %s below */` |
|      171 | 3370 | `			for( iBit = 0 ; iBit < (int)SX_ARRAYSIZE(azGenAttrTarget) ; iBit++ ){` |
|      171 | 3371 | `				if( iTarget == (1 << iBit) ){` |
|       47 | 3372 | `					break;` |
|        - | 3373 | `				}` |
|       63 | 3374 | `			}` |
|       70 | 3375 | `			rc = PH7_GenCompileError(pGen,E_ERROR,aAttr[n].nLine,` |
|       23 | 3376 | `				"Attribute \"%z\" cannot target %s (allowed targets: %s)",pName,` |
|       23 | 3377 | `				iBit < (int)SX_ARRAYSIZE(azGenAttrTarget) ? azGenAttrTarget[iBit] : "",` |
|       23 | 3378 | `				SyBlobData(&sAllowed));` |
|       47 | 3379 | `			SyBlobRelease(&sAllowed);` |
|       47 | 3380 | `			return rc;` |
|        - | 3381 | `		}` |
|        - | 3382 | `		/* ...then repetition, which is what php checks second: the FIRST of a` |
|        - | 3383 | `		 * misplaced pair reports its target instead. */` |
|      455 | 3384 | `		for( k = 0 ; k < n ; ++k ){` |
|        6 | 3385 | `			if( aAttr[k].sName.nByte == pName->nByte` |
|        7 | 3386 | `			 && SyStrnicmp(aAttr[k].sName.zString,pName->zString,pName->nByte) == 0 ){` |
|       10 | 3387 | `				return PH7_GenCompileError(pGen,E_ERROR,aAttr[n].nLine,` |
|        3 | 3388 | `					"Attribute \"%z\" must not be repeated",pName);` |
|        - | 3389 | `			}` |
|      ! 0 | 3390 | `		}` |
|      449 | 3391 | `		if( iTarget == 1 && pClassName ){` |
|      124 | 3392 | `			const char *zRefused = GenStateAttrClassRefusal(aGenInternalAttr[iRow].zName,` |
|       40 | 3393 | `				iClassFlags);` |
|       84 | 3394 | `			if( zRefused ){` |
|       37 | 3395 | `				return PH7_GenCompileError(pGen,E_ERROR,aAttr[n].nLine,` |
|       24 | 3396 | `					"Cannot apply #[\\%s] to %s %z",aGenInternalAttr[iRow].zName,` |
|       12 | 3397 | `					zRefused,pClassName);` |
|        - | 3398 | `			}` |
|       28 | 3399 | `		}` |
|      215 | 3400 | `	}` |
|   473781 | 3401 | `	return SXRET_OK;` |
|   236931 | 3402 | `}` |
|        - | 3403 | `/*` |
|        - | 3404 | `` * php 8.5's `(void)` cast is a STATEMENT prefix, not an expression operator:`` |
|        - | 3405 | `` * `$x = (void) f();` and `return (void) f();` are parse errors there too, and`` |
|        - | 3406 | ` * the only thing it does is say that dropping the answer is DELIBERATE, which` |
|        - | 3407 | ` * silences a #[\NoDiscard] callee. The lexer already assembled the three tokens` |
|        - | 3408 | ` * into one (PH7_TK_VOID_CAST); this consumes it and answers 1.` |
|        - | 3409 | ` */` |
|   909224 | 3410 | `PH7_PRIVATE int GenStateTakeVoidCast(ph7_gen_state *pGen)` |
|        5 | 3411 | `{` |
|   909229 | 3412 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_VOID_CAST) ){` |
|       19 | 3413 | `		pGen->pIn++;` |
|       19 | 3414 | `		return 1;` |
|        - | 3415 | `	}` |
|   909211 | 3416 | `	return 0;` |
|   454617 | 3417 | `}` |
|        - | 3418 | `/*` |
|        - | 3419 | `` * php's grammar takes a `(void)` cast at the head of an expression STATEMENT and`` |
|        - | 3420 | `` * at the head of each element of a `for` clause list — `for ((void) f(), $i = 0;`` |
|        - | 3421 | `` * $i < 1; $i++, (void) g())` is all valid, while `for ($i = (void) f();;)` is`` |
|        - | 3422 | ` * not. The statement head is consumed by GenStateTakeVoidCast; a clause is one` |
|        - | 3423 | ` * expression with comma operators in it, so its element heads are marked HERE,` |
|        - | 3424 | ` * before it compiles: the token becomes the no-op cast operator parse.c declares,` |
|        - | 3425 | `` * and every other `(void)` in the clause stays unrecognized, which is php's own`` |
|        - | 3426 | ` * refusal. Nothing is moved or removed — the token stream is shared with the` |
|        - | 3427 | ` * rest of the file.` |
|        - | 3428 | ` */` |
|   104514 | 3429 | `PH7_PRIVATE int GenStateEnableClauseVoidCasts(ph7_gen_state *pGen,int bLastToo)` |
|        5 | 3430 | `{` |
|   104519 | 3431 | `	SyToken *pTok = pGen->pIn,*pLastMark = 0;` |
|   104519 | 3432 | `	int iDepth = 0,bHead = 1,bCommaAfter = 0;` |
|   597087 | 3433 | `	for( ; pTok < pGen->pEnd ; pTok++ ){` |
|   562249 | 3434 | `		if( pTok->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|    17253 | 3435 | `			iDepth++;` |
|   553625 | 3436 | `		}else if( pTok->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|    17253 | 3437 | `			iDepth--;` |
|   536377 | 3438 | `		}else if( iDepth == 0 && (pTok->nType & PH7_TK_SEMI) ){` |
|        - | 3439 | `			/* The three clauses share one token range (only the post one is` |
|        - | 3440 | `			 * delimited), so this scan stops where its own clause does. */` |
|    34843 | 3441 | `			break;` |
|   458077 | 3442 | `		}else if( iDepth == 0 && (pTok->nType & PH7_TK_COMMA) ){` |
|       14 | 3443 | `			bHead = 1;` |
|       14 | 3444 | `			bCommaAfter = 1;` |
|       14 | 3445 | `			continue;` |
|   458065 | 3446 | `		}else if( iDepth == 0 && bHead && (pTok->nType & PH7_TK_VOID_CAST) ){` |
|        9 | 3447 | `			pTok->nType \|= PH7_TK_OP;` |
|        9 | 3448 | `			pTok->pUserData = (void *)PH7_ExprExtractOperator(&pTok->sData,0);` |
|        9 | 3449 | `			pLastMark = pTok;` |
|        9 | 3450 | `			bCommaAfter = 0;` |
|        4 | 3451 | `		}` |
|   492561 | 3452 | `		bHead = 0;` |
|   246283 | 3453 | `	}` |
|        - | 3454 | `	/* The CONDITION clause's last element is the condition VALUE, so php refuses a` |
|        - | 3455 | ``	 * `(void)` on that one and only that one: `for (;(void) f();)` is a parse error`` |
|        - | 3456 | ``	 * where `for (;(void) f(), $i < 1;)` is fine. */`` |
|   104519 | 3457 | `	if( !bLastToo && pLastMark && !bCommaAfter ){` |
|        3 | 3458 | `		pLastMark->nType &= ~(sxu32)PH7_TK_OP;` |
|        3 | 3459 | `		pLastMark->pUserData = 0;` |
|        3 | 3460 | `		return 1;` |
|        - | 3461 | `	}` |
|   104517 | 3462 | `	return 0;` |
|    52262 | 3463 | `}` |
|        - | 3464 | `/*` |
|        - | 3465 | ` * The statement is about to throw its expression's value away. When that value` |
|        - | 3466 | ` * came straight out of a CALL, mark the call: php's !RETURN_VALUE_USED, which is` |
|        - | 3467 | `` * what a #[\NoDiscard] callee reads. `f() + 1;` drops the ADD's result, not the`` |
|        - | 3468 | ` * call's, so only the last instruction is looked at.` |
|        - | 3469 | ` */` |
|   978700 | 3470 | `PH7_PRIVATE void GenStateMarkDiscardedCall(ph7_gen_state *pGen)` |
|        5 | 3471 | `{` |
|   978705 | 3472 | `	VmInstr *pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   978705 | 3473 | `	if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|   106041 | 3474 | `		pInstr->bDiscard = 1;` |
|    53018 | 3475 | `	}` |
|   978705 | 3476 | `}` |
|  1834412 | 3477 | `PH7_PRIVATE sxi32 GenStateCompileChunk(` |
|        - | 3478 | `	ph7_gen_state *pGen, /* Code generator state */` |
|        - | 3479 | `	sxi32 iFlags         /* Compile flags */` |
|        - | 3480 | `	)` |
|        5 | 3481 | `{` |
|        - | 3482 | `	ProcLangConstruct xCons;` |
|        - | 3483 | `	sxi32 rc;` |
|  1834417 | 3484 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|  1169790 | 3485 | `	for(;;){` |
|  2087001 | 3486 | `		int bStmtIsDeclare = 0;` |
|  2087001 | 3487 | `		if( pGen->pIn >= pGen->pEnd ){` |
|        - | 3488 | `			/* No more input to process */` |
|    28307 | 3489 | `			break;` |
|        - | 3490 | `		}` |
|        - | 3491 | `		/* Bind a directly-preceding docblock to this statement */` |
|  2058699 | 3492 | `		GenStateSetPendingDoc(&(*pGen));` |
|  2058699 | 3493 | `		if( SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|        - | 3494 | `			/* php: a statement-position attribute group must be followed by a` |
|        - | 3495 | ``			 * declaration (function/class-like/const) — `#[A] $x = 1;` is a`` |
|        - | 3496 | `` 			 * parse error, never a silent discard. `static`/`fn`/`function` `` |
|        - | 3497 | ``			 * cover bare closure-expression statements; `readonly`/`enum` are`` |
|        - | 3498 | `			 * context-sensitive IDs handled by the modified-class/enum scans. */` |
|      330 | 3499 | `			int bAttrTarget = 0;` |
|      326 | 3500 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd)` |
|      325 | 3501 | `			 \|\| GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|       17 | 3502 | `				bAttrTarget = 1;` |
|      322 | 3503 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|      314 | 3504 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|      310 | 3505 | `				if( nKw == PH7_TKWRD_FUNCTION \|\| nKw == PH7_TKWRD_CLASS` |
|       66 | 3506 | `				 \|\| nKw == PH7_TKWRD_INTERFACE \|\| nKw == PH7_TKWRD_TRAIT` |
|       11 | 3507 | `				 \|\| nKw == PH7_TKWRD_ABSTRACT \|\| nKw == PH7_TKWRD_FINAL` |
|        8 | 3508 | `				 \|\| nKw == PH7_TKWRD_CONST \|\| nKw == PH7_TKWRD_STATIC` |
|        4 | 3509 | `				 \|\| nKw == PH7_TKWRD_FN ){` |
|      314 | 3510 | `					bAttrTarget = 1;` |
|      155 | 3511 | `				}` |
|      155 | 3512 | `			}` |
|      330 | 3513 | `			if( !bAttrTarget ){` |
|      ! 0 | 3514 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3515 | `					"syntax error, unexpected token \"%z\" after attribute group; expecting a declaration",` |
|      ! 0 | 3516 | `					&pGen->pIn->sData);` |
|      ! 0 | 3517 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3518 | `					break;` |
|        - | 3519 | `				}` |
|      ! 0 | 3520 | `				SySetReset(&pGen->aPendingAttrs);` |
|      ! 0 | 3521 | `			}` |
|      163 | 3522 | `		}` |
|        - | 3523 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|        - | 3524 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  2058699 | 3525 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  1149859 | 3526 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|  1149859 | 3527 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|       61 | 3528 | `				bStmtIsDeclare = 1;` |
|       28 | 3529 | `			}` |
|   574927 | 3530 | `		}` |
|  2058699 | 3531 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|        - | 3532 | `			/* Any non-declare top-level statement locks the strict_types` |
|        - | 3533 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|   252605 | 3534 | `			pGen->bStrictTypesLocked = 1;` |
|   126300 | 3535 | `		}` |
|  2058699 | 3536 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|        - | 3537 | `			/* Compile block */` |
|       59 | 3538 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|       59 | 3539 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 3540 | `				break;` |
|        - | 3541 | `			}` |
|       32 | 3542 | `		}else{` |
|  2058645 | 3543 | `			xCons = 0;` |
|  2058645 | 3544 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|        - | 3545 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|        - | 3546 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|        - | 3547 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|      169 | 3548 | `				xCons = PH7_CompileClassModifiers;` |
|  2058563 | 3549 | `			}else if( GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|        - | 3550 | ``				/* `enum Name …` (PHP 8.1) — `enum` is a context-sensitive ID,`` |
|        - | 3551 | `				 * so it is detected here rather than the keyword dispatcher. */` |
|      125 | 3552 | `				xCons = PH7_CompileEnum;` |
|  2058421 | 3553 | `			}else if( GenStateIsNsRelName(pGen->pIn,pGen->pEnd) ){` |
|        - | 3554 | ``				/* A statement that STARTS with php's `namespace\X` name operator`` |
|        - | 3555 | ``				 * (`namespace\Cee::m();`) is an expression, not a namespace`` |
|        - | 3556 | ``				 * DECLARATION — the glued `\` is what tells the two apart. */`` |
|        7 | 3557 | `				xCons = 0;` |
|  2058358 | 3558 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|  1149725 | 3559 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        - | 3560 | `				/* Try to extract a language construct handler */` |
|  1149725 | 3561 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|  1149725 | 3562 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|       13 | 3563 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|        - | 3564 | `						"Syntax error: Unexpected keyword '%z'",` |
|        8 | 3565 | `						&pGen->pIn->sData);` |
|        9 | 3566 | `					if( rc == SXERR_ABORT ){` |
|      ! 0 | 3567 | `						break;` |
|        - | 3568 | `					}` |
|        - | 3569 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|        - | 3570 | `					 * this erroneous statement.` |
|        - | 3571 | `					 */` |
|        9 | 3572 | `					xCons = PH7_ErrorRecover;` |
|        4 | 3573 | `				}` |
|  1483495 | 3574 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|   101615 | 3575 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|        - | 3576 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|      219 | 3577 | `				xCons = PH7_CompileLabel;` |
|      107 | 3578 | `			}` |
|  2058645 | 3579 | `			if( xCons == 0 ){` |
|        - | 3580 | `				/* Assume an expression an try to compile it. A leading php 8.5` |
|        - | 3581 | ``				 * `(void)` cast is consumed here — statement head is one of the two`` |
|        - | 3582 | `				 * places its grammar takes one — and says the answer is dropped` |
|        - | 3583 | `				 * DELIBERATELY, so the call below is not marked. */` |
|   909229 | 3584 | `				int bVoid = GenStateTakeVoidCast(&(*pGen));` |
|   909229 | 3585 | `				if( bVoid && pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|        - | 3586 | `` 					/* php's grammar wants an expression after the cast: `(void);` `` |
|        - | 3587 | ``					 * is `syntax error, unexpected token ";"` there. */`` |
|        3 | 3588 | `					rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|        - | 3589 | `						"syntax error, unexpected token \";\"");` |
|        2 | 3590 | `				}else{` |
|   909227 | 3591 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   909227 | 3592 | `					if( rc != SXERR_EMPTY ){` |
|   909035 | 3593 | `						if( !bVoid ){` |
|   909021 | 3594 | `							GenStateMarkDiscardedCall(&(*pGen));` |
|   454508 | 3595 | `						}` |
|        - | 3596 | `						/* Pop l-value */` |
|   909035 | 3597 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|   454515 | 3598 | `					}` |
|        - | 3599 | `				}` |
|   454617 | 3600 | `			}else{` |
|        - | 3601 | `				/* Go compile the sucker */` |
|  1149421 | 3602 | `				rc = xCons(&(*pGen));` |
|        - | 3603 | `			}` |
|  2058645 | 3604 | `			if( rc == SXERR_ABORT ){` |
|        - | 3605 | `				/* Request to abort compilation */` |
|       81 | 3606 | `				break;` |
|        - | 3607 | `			}` |
|        - | 3608 | `		}` |
|        - | 3609 | `		/* Ignore trailing semi-colons ';' */` |
|  3403407 | 3610 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  1344789 | 3611 | `			pGen->pIn++;` |
|        5 | 3612 | `		}` |
|  2058623 | 3613 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|        - | 3614 | `			/* Compile a single statement and return */` |
|  1806039 | 3615 | `			break;` |
|        - | 3616 | `		}` |
|        - | 3617 | `		/* LOOP ONE */` |
|        - | 3618 | `		/* LOOP TWO */` |
|        - | 3619 | `		/* LOOP THREE */` |
|        - | 3620 | `		/* LOOP FOUR */` |
|        5 | 3621 | `	}` |
|        - | 3622 | `	/* Return compilation status */` |
|  1834417 | 3623 | `	return rc;` |
|        5 | 3624 | `}` |
|        - | 3625 | `/*` |
|        - | 3626 | ` * Compile a Raw PHP chunk.` |
|        - | 3627 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|        - | 3628 | ` * takes care of generating the appropriate error message.` |
|        - | 3629 | ` */` |
|    28380 | 3630 | `static sxi32 PH7_CompilePHP(` |
|        - | 3631 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|        - | 3632 | `	SySet *pTokenSet,     /* Token set */` |
|        - | 3633 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|        - | 3634 | `	)` |
|        5 | 3635 | `{` |
|    28385 | 3636 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|        - | 3637 | `	sxi32 rc;` |
|        - | 3638 | `	/* Reset the token set (and its trivia sidecar) */` |
|    28385 | 3639 | `	SySetReset(&(*pTokenSet));` |
|    28385 | 3640 | `	SySetReset(&pGen->aTrivia);` |
|        - | 3641 | `	/* Mark as the default token set */` |
|    28385 | 3642 | `	pGen->pTokenSet = &(*pTokenSet);` |
|        - | 3643 | `	/* Advance the stream cursor */` |
|    28385 | 3644 | `	pGen->pRawIn++;` |
|        - | 3645 | `	/* Tokenize the PHP chunk first */` |
|    28385 | 3646 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet),&pGen->aTrivia);` |
|        - | 3647 | `	/* Point to the head and tail of the token stream. */` |
|    28385 | 3648 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|    28385 | 3649 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|    28385 | 3650 | `	if( is_expr ){` |
|      ! 0 | 3651 | `		rc = SXERR_EMPTY;` |
|      ! 0 | 3652 | `		if( pGen->pIn < pGen->pEnd ){` |
|        - | 3653 | `			/* A simple expression,compile it */` |
|      ! 0 | 3654 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|      ! 0 | 3655 | `		}` |
|        - | 3656 | `		/* Emit the DONE instruction */` |
|      ! 0 | 3657 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|      ! 0 | 3658 | `		return SXRET_OK;` |
|        - | 3659 | `	}` |
|    28385 | 3660 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|        - | 3661 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|        - | 3662 | `		/*` |
|        - | 3663 | `		 * Shortcut syntax for the 'echo' language construct.` |
|        - | 3664 | `		 * According to the PHP reference manual:` |
|        - | 3665 | `		 *  echo() also has a shortcut syntax, where you can` |
|        - | 3666 | `		 *  immediately follow` |
|        - | 3667 | `		 *  the opening tag with an equals sign as follows:` |
|        - | 3668 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|        - | 3669 | `		 * Symisc extension:` |
|        - | 3670 | `		 *   This short syntax works with all PHP opening` |
|        - | 3671 | `		 *   tags unlike the default PHP engine that handle` |
|        - | 3672 | `		 *   only short tag.` |
|        - | 3673 | `		 */` |
|        - | 3674 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|        3 | 3675 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|        3 | 3676 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|        3 | 3677 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|        - | 3678 | `		/* This synthesized echo is compiled as an EXPRESSION, which is otherwise a` |
|        - | 3679 | `		 * parse error; allow it for the duration of this one compile. */` |
|        3 | 3680 | `		pGen->nExprEchoOk++;` |
|        3 | 3681 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|        3 | 3682 | `		pGen->nExprEchoOk--;` |
|        3 | 3683 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 3684 | `			return SXERR_ABORT;` |
|        - | 3685 | `		}` |
|        3 | 3686 | `		if( rc != SXERR_EMPTY ){` |
|        3 | 3687 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        1 | 3688 | `		}` |
|        3 | 3689 | `		return SXRET_OK;` |
|        - | 3690 | `	}` |
|        - | 3691 | `	/* Compile the PHP chunk */` |
|    28383 | 3692 | `	rc = GenStateCompileChunk(pGen,0);` |
|        - | 3693 | `	/* Fix exceptions jumps */` |
|    28383 | 3694 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|        - | 3695 | `	/* Fix gotos now, the jump destination is resolved */` |
|    28383 | 3696 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|        3 | 3697 | `		rc = SXERR_ABORT;` |
|        1 | 3698 | `	}` |
|        - | 3699 | `	/* Reset container */` |
|    28383 | 3700 | `	SySetReset(&pGen->aGoto);` |
|    28383 | 3701 | `	SySetReset(&pGen->aLabel);` |
|    28383 | 3702 | `	SySetReset(&pGen->aNullsafeJmp);` |
|        - | 3703 | `	/* Compilation result */` |
|    28383 | 3704 | `	return rc;` |
|    14195 | 3705 | `}` |
|        - | 3706 | `/*` |
|        - | 3707 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|        - | 3708 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|        - | 3709 | ` * This is the only compile interface exported from this file.` |
|        - | 3710 | ` */` |
|    32152 | 3711 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|        - | 3712 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|        - | 3713 | `	SyString *pScript,  /* Script to compile */` |
|        - | 3714 | `	sxi32 iFlags        /* Compile flags */` |
|        - | 3715 | `	)` |
|        5 | 3716 | `{` |
|        - | 3717 | `	SySet aPhpToken,aRawToken;` |
|        - | 3718 | `	ph7_gen_state *pCodeGen;` |
|        - | 3719 | `	ph7_value *pRawObj;` |
|        - | 3720 | `	sxu32 nObjIdx;` |
|        - | 3721 | `	sxi32 nRawObj;` |
|        - | 3722 | `	int is_expr;` |
|        - | 3723 | `	sxi8 bSavedStrict;` |
|        - | 3724 | `	sxi8 bSavedStrictLocked;` |
|        - | 3725 | `	SyToken *pSavedIn,*pSavedEnd;` |
|        - | 3726 | `	sxi32 rc;` |
|    32157 | 3727 | `	sxu32 nBaseLine = 1;` |
|    32157 | 3728 | `	if( pScript->nByte < 1 ){` |
|        - | 3729 | `		/* Nothing to compile */` |
|      ! 0 | 3730 | `		return PH7_OK;` |
|        - | 3731 | `	}` |
|        - | 3732 | `	/* php skips a "#!" shebang on the first line of a CLI script: consume it` |
|        - | 3733 | `	 * (including its newline) so it is not echoed as inline text, and bump the` |
|        - | 3734 | `	 * base line to 2 so the code below still reports php-matching line numbers. */` |
|    32157 | 3735 | `	if( pScript->nByte >= 2 && pScript->zString[0]=='#' && pScript->zString[1]=='!' ){` |
|        3 | 3736 | `		const char *z = pScript->zString;` |
|        3 | 3737 | `		const char *zEnd = &z[pScript->nByte];` |
|       39 | 3738 | `		while( z < zEnd && z[0] != '\n' ){ z++; }` |
|        3 | 3739 | `		if( z < zEnd ){ z++; } /* consume the newline too */` |
|        3 | 3740 | `		pScript->nByte -= (sxu32)(z - pScript->zString);` |
|        3 | 3741 | `		pScript->zString = z;` |
|        3 | 3742 | `		nBaseLine = 2;` |
|        3 | 3743 | `		if( pScript->nByte < 1 ){` |
|      ! 0 | 3744 | `			return PH7_OK;` |
|        - | 3745 | `		}` |
|        1 | 3746 | `	}` |
|        - | 3747 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|        - | 3748 | `	 * file's flags so include/require restore them on return. */` |
|    32157 | 3749 | `	pCodeGen = &pVm->sCodeGen;` |
|        - | 3750 | ``	/* aPhpToken below is a LOCAL token set: once it is released at `cleanup`, any`` |
|        - | 3751 | `	 * pGen->pIn/pEnd still pointing into it DANGLE. PH7_VmEmitInstr reads pIn to stamp` |
|        - | 3752 | `	 * each instruction's source line, and instructions are still emitted after this` |
|        - | 3753 | `	 * function returns (VmEvalChunk's trailing OP_DONE) -- a use-after-free ASan caught` |
|        - | 3754 | `	 * immediately. Save the caller's cursor and restore it on the way out, so an` |
|        - | 3755 | `	 * enclosing compile keeps its (live) tokens and the top level goes back to NULL. */` |
|    32157 | 3756 | `	pSavedIn = pCodeGen->pIn;` |
|    32157 | 3757 | `	pSavedEnd = pCodeGen->pEnd;` |
|    32157 | 3758 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|    32157 | 3759 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|    32157 | 3760 | `	pCodeGen->bStrictTypes = 0;` |
|    32157 | 3761 | `	pCodeGen->bStrictTypesLocked = 0;` |
|        - | 3762 | `	/* Initialize the tokens containers */` |
|    32157 | 3763 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|    32157 | 3764 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|    32157 | 3765 | `	SySetAlloc(&aPhpToken,0xc0);` |
|    32157 | 3766 | `	is_expr = 0;` |
|    32157 | 3767 | `	if( iFlags & PH7_PHP_ONLY ){` |
|        - | 3768 | `		SyToken sTmp;` |
|        - | 3769 | `		/* PHP only: -*/` |
|    15643 | 3770 | `		sTmp.nLine = 1;` |
|    15643 | 3771 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|    15643 | 3772 | `		sTmp.pUserData = 0;` |
|    15643 | 3773 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|    15643 | 3774 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|    15643 | 3775 | `		if( iFlags & PH7_PHP_EXPR ){` |
|        - | 3776 | `			/* A simple PHP expression */` |
|      ! 0 | 3777 | `			is_expr = 1;` |
|      ! 0 | 3778 | `		}` |
|     7824 | 3779 | `	}else{` |
|        - | 3780 | `		/* Tokenize raw text */` |
|    16519 | 3781 | `		SySetAlloc(&aRawToken,32);` |
|    16519 | 3782 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken,nBaseLine);` |
|        - | 3783 | `	}` |
|        - | 3784 | `	/* Process high-level tokens */` |
|    32157 | 3785 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|    32157 | 3786 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|    32157 | 3787 | `	rc = PH7_OK;` |
|    32157 | 3788 | `	if( is_expr ){` |
|        - | 3789 | `		/* Compile the expression */` |
|      ! 0 | 3790 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|      ! 0 | 3791 | `		goto cleanup;` |
|        - | 3792 | `	}` |
|    32157 | 3793 | `	nObjIdx = 0;` |
|        - | 3794 | `	/* Start the compilation process */` |
|    24337 | 3795 | `	for(;;){` |
|    76981 | 3796 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|    32079 | 3797 | `			break; /* No more tokens to process */` |
|        - | 3798 | `		}` |
|    44907 | 3799 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|        - | 3800 | `			/* Compile the PHP chunk */` |
|    28385 | 3801 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|    28385 | 3802 | `			if( rc == SXERR_ABORT ){` |
|       83 | 3803 | `				break;` |
|        - | 3804 | `			}` |
|    28307 | 3805 | `			continue;` |
|        - | 3806 | `		}` |
|        - | 3807 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|    16527 | 3808 | `		nRawObj = 0;` |
|    33049 | 3809 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|        - | 3810 | `			/* Consume the raw chunk without any processing */` |
|    16527 | 3811 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|    16527 | 3812 | `			if( pRawObj == 0 ){` |
|      ! 0 | 3813 | `				rc = SXERR_MEM;` |
|      ! 0 | 3814 | `				break;` |
|        - | 3815 | `			}` |
|        - | 3816 | `			/* Mark as constant and emit the load constant instruction */` |
|    16527 | 3817 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|    16527 | 3818 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|    16527 | 3819 | `			++nRawObj;` |
|    16527 | 3820 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|        5 | 3821 | `		}` |
|    16527 | 3822 | `		if( nRawObj > 0 ){` |
|        - | 3823 | `			/* Emit the consume instruction */` |
|    16527 | 3824 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     8261 | 3825 | `		}` |
|    16081 | 3826 | `	}` |
|    16076 | 3827 | `cleanup:` |
|        - | 3828 | `	/* Drop the cursor into the token set BEFORE the set is freed (see above). */` |
|    32157 | 3829 | `	pCodeGen->pIn = pSavedIn;` |
|    32157 | 3830 | `	pCodeGen->pEnd = pSavedEnd;` |
|    32157 | 3831 | `	SySetRelease(&aRawToken);` |
|    32157 | 3832 | `	SySetRelease(&aPhpToken);` |
|        - | 3833 | `	/* Restore outer file's strict_types scope */` |
|    32157 | 3834 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|    32157 | 3835 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|    32157 | 3836 | `	return rc;` |
|    16081 | 3837 | `}` |
|        - | 3838 | `/*` |
|        - | 3839 | ` * Utility routines.Initialize the code generator.` |
|        - | 3840 | ` */` |
|     5740 | 3841 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|        - | 3842 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 3843 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|        - | 3844 | `	void *pErrData     /* Last argument to xErr() */` |
|        - | 3845 | `	)` |
|        5 | 3846 | `{` |
|     5745 | 3847 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|        - | 3848 | `	/* Zero the structure */` |
|     5745 | 3849 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|        - | 3850 | `	/* Initial state */` |
|     5745 | 3851 | `	pGen->pVm  = &(*pVm);` |
|     5745 | 3852 | `	pGen->xErr = xErr;` |
|     5745 | 3853 | `	pGen->pErrData = pErrData;` |
|     5745 | 3854 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|     5745 | 3855 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|     5745 | 3856 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|     5745 | 3857 | `	SySetInit(&pGen->aScope,&pVm->sAllocator,sizeof(GenScope));` |
|     5745 | 3858 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|     5745 | 3859 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|     5745 | 3860 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|     5745 | 3861 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|     5745 | 3862 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|     5745 | 3863 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|        - | 3864 | `	/* Error log buffer */` |
|     5745 | 3865 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|        - | 3866 | `	/* General purpose working buffer */` |
|     5745 | 3867 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|        - | 3868 | `	/* Namespace state */` |
|     5745 | 3869 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|     5745 | 3870 | `	GenStateInitUseImports(&(*pGen),&(*pVm));` |
|     5745 | 3871 | `	GenStateInitSeenSymbols(&(*pGen),&(*pVm));` |
|        - | 3872 | `	/* Create the global scope */` |
|     5745 | 3873 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|        - | 3874 | `	/* Point to the global scope */` |
|     5745 | 3875 | `	pGen->pCurrent = &pGen->sGlobal;` |
|     5745 | 3876 | `	return SXRET_OK;` |
|        5 | 3877 | `}` |
|        - | 3878 | `/*` |
|        - | 3879 | ` * Utility routines. Reset the code generator to it's initial state.` |
|        - | 3880 | ` */` |
|    37110 | 3881 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|        - | 3882 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 3883 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|        - | 3884 | `	void *pErrData     /* Last argument to xErr() */` |
|        - | 3885 | `	)` |
|        5 | 3886 | `{` |
|    37115 | 3887 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|        - | 3888 | `	GenBlock *pBlock,*pParent;` |
|        - | 3889 | `	/* Reset state */` |
|    37115 | 3890 | `	SySetReset(&pGen->aLabel);` |
|    37115 | 3891 | `	SySetReset(&pGen->aGoto);` |
|    37115 | 3892 | `	SySetReset(&pGen->aNullsafeJmp);` |
|    37115 | 3893 | `	SySetReset(&pGen->aTrivia);` |
|    37115 | 3894 | `	SySetReset(&pGen->aPendingAttrs);` |
|    37115 | 3895 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|    37115 | 3896 | `	SyBlobRelease(&pGen->sErrBuf);` |
|    37115 | 3897 | `	SyBlobRelease(&pGen->sWorker);` |
|    37115 | 3898 | `	SyBlobRelease(&pGen->sNamespace);` |
|    37115 | 3899 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    37115 | 3900 | `	GenStateResetUseImports(&(*pGen),&(*pVm));` |
|        - | 3901 | `	/* A fresh compile unit has declared nothing yet. */` |
|    37115 | 3902 | `	GenStateResetSeenSymbols(&(*pGen),&(*pVm));` |
|        - | 3903 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|        - | 3904 | `	 * They intern variable names and literal strings that are referenced by` |
|        - | 3905 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|        - | 3906 | `	 * Releasing them would either leak the interned strings or require freeing` |
|        - | 3907 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|        - | 3908 | `	 * number of unique names, which is acceptable. */` |
|        - | 3909 | `	/* Point to the global scope */` |
|    37115 | 3910 | `	pBlock = pGen->pCurrent;` |
|    37115 | 3911 | `	while( pBlock->pParent != 0 ){` |
|      ! 0 | 3912 | `		pParent = pBlock->pParent;` |
|      ! 0 | 3913 | `		GenStateFreeBlock(pBlock);` |
|      ! 0 | 3914 | `		pBlock = pParent;` |
|      ! 0 | 3915 | `	}` |
|    37115 | 3916 | `	pGen->xErr = xErr;` |
|    37115 | 3917 | `	pGen->pErrData = pErrData;` |
|    37115 | 3918 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    37115 | 3919 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|    37115 | 3920 | `	pGen->pIn = pGen->pEnd = 0;` |
|    37115 | 3921 | `	pGen->nErr = 0;` |
|        - | 3922 | `	/* Clear the class-body context (a prior compile aborted mid-class-body would` |
|        - | 3923 | `	 * otherwise leave these live for the next eval/include on this VM). */` |
|    37115 | 3924 | `	pGen->pCurClass = 0;` |
|    37115 | 3925 | `	pGen->iInMemberDefault = 0;` |
|    37115 | 3926 | `	return SXRET_OK;` |
|        5 | 3927 | `}` |
|        - | 3928 | `/*` |
|        - | 3929 | ` * Save the code generator's compile-position state and hand the live generator a` |
|        - | 3930 | ` * fresh, empty one for a NESTED compilation unit.` |
|        - | 3931 | ` *` |
|        - | 3932 | ` * A require/include normally runs at execution time, when no compile is in flight,` |
|        - | 3933 | ` * so VmEvalChunk can call PH7_ResetCodeGenerator to wipe the generator. But an` |
|        - | 3934 | `` * autoload can fire in the MIDDLE of compiling a class: resolving `class Child`` |
|        - | 3935 | `` * extends Base` calls PH7_VmExtractClass, which triggers the autoloader, whose`` |
|        - | 3936 | `` * body `require`s Base's file — a nested compile while the outer one is mid-token.`` |
|        - | 3937 | ` * PH7_ResetCodeGenerator would then blow away the outer compile's cursor` |
|        - | 3938 | ` * (pIn/pEnd), current block, use-imports, labels, etc.; the outer parse resumes` |
|        - | 3939 | ` * with pIn == NULL and reports a bogus "Expected '{' after class 'Child'". (Common` |
|        - | 3940 | ` * in every real framework: Composer autoloads parent classes on demand.)` |
|        - | 3941 | ` *` |
|        - | 3942 | ` * This snapshots the position/scope fields into *pSaved (a caller-stack` |
|        - | 3943 | ` * ph7_gen_state used purely as storage) and re-initializes the live generator's` |
|        - | 3944 | ` * position containers to FRESH, empty ones WITHOUT releasing the outer's (the` |
|        - | 3945 | ` * snapshot now owns those). hLiteral/hNumLiteral/hVar are intentionally left LIVE` |
|        - | 3946 | ` * and shared — compiled bytecode interns names/literals into them and they grow` |
|        - | 3947 | ` * monotonically (exactly why PH7_ResetCodeGenerator keeps them); restoring an old` |
|        - | 3948 | ` * copy of those headers after a nested grow would use-after-free the bucket array.` |
|        - | 3949 | ` */` |
|        4 | 3950 | `PH7_PRIVATE void PH7_CompilerSaveState(ph7_vm *pVm,ph7_gen_state *pSaved,ProcConsumer xErr,void *pErrData)` |
|        1 | 3951 | `{` |
|        5 | 3952 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|        - | 3953 | `	/* Shallow-copy every field; the position containers below are then replaced` |
|        - | 3954 | `	 * with fresh ones on the live struct, so *pSaved keeps the outer's memory. */` |
|        5 | 3955 | `	*pSaved = *pGen;` |
|        5 | 3956 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|        5 | 3957 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|        5 | 3958 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|        5 | 3959 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|        5 | 3960 | `	SySetInit(&pGen->aScope,&pVm->sAllocator,sizeof(GenScope));` |
|        5 | 3961 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|        5 | 3962 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|        5 | 3963 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|        5 | 3964 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|        5 | 3965 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|        5 | 3966 | `	GenStateInitUseImports(&(*pGen),&(*pVm));` |
|        5 | 3967 | `	GenStateInitSeenSymbols(&(*pGen),&(*pVm));` |
|        - | 3968 | `	/* Fresh global scope for the nested unit (address of the embedded sGlobal is` |
|        - | 3969 | `	 * stable, so any outer block still parented to it stays valid across restore). */` |
|        5 | 3970 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(pVm),0);` |
|        5 | 3971 | `	pGen->pCurrent = &pGen->sGlobal;` |
|        5 | 3972 | `	pGen->pIn = pGen->pEnd = 0;` |
|        5 | 3973 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|        5 | 3974 | `	pGen->pTokenSet = 0;` |
|        5 | 3975 | `	pGen->nErr = 0;` |
|        5 | 3976 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|        5 | 3977 | `	pGen->nCommaExprOk = 0;` |
|        5 | 3978 | `	pGen->zClauseCloser = 0;` |
|        5 | 3979 | `	pGen->bInGenerator = 0;` |
|        5 | 3980 | `	pGen->bStrictTypes = 0;` |
|        5 | 3981 | `	pGen->bStrictTypesLocked = 0;` |
|        - | 3982 | `	/* The nested unit is a fresh top-level compile: it is not lexically inside the` |
|        - | 3983 | `	 * outer's class body nor its member default, so a __TRAIT__ in the nested file` |
|        - | 3984 | `	 * must not inherit the outer's trait. (Restore below carries the outer's values` |
|        - | 3985 | `	 * back, so only the nested unit sees these zeros.) */` |
|        5 | 3986 | `	pGen->pCurClass = 0;` |
|        5 | 3987 | `	pGen->iInMemberDefault = 0;` |
|        5 | 3988 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|        5 | 3989 | `	pGen->xErr = xErr;` |
|        5 | 3990 | `	pGen->pErrData = pErrData;` |
|        5 | 3991 | `}` |
|        - | 3992 | `/*` |
|        - | 3993 | ` * Restore the outer compile-position state saved by PH7_CompilerSaveState,` |
|        - | 3994 | ` * releasing the nested unit's position containers first. The shared` |
|        - | 3995 | ` * hLiteral/hNumLiteral/hVar tables (grown by the nested compile) are carried` |
|        - | 3996 | ` * forward, NOT rolled back to the snapshot's stale headers.` |
|        - | 3997 | ` */` |
|        4 | 3998 | `PH7_PRIVATE void PH7_CompilerRestoreState(ph7_vm *pVm,ph7_gen_state *pSaved)` |
|        1 | 3999 | `{` |
|        5 | 4000 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|        - | 4001 | `	GenBlock *pBlock,*pParent;` |
|        - | 4002 | `	SyHash hVar,hLiteral,hNumLiteral;` |
|        - | 4003 | `	/* Free any nested blocks left open (e.g. an aborted nested compile), then the` |
|        - | 4004 | `	 * nested global block's own fixup sets. */` |
|        5 | 4005 | `	pBlock = pGen->pCurrent;` |
|        5 | 4006 | `	while( pBlock && pBlock->pParent != 0 ){` |
|      ! 0 | 4007 | `		pParent = pBlock->pParent;` |
|      ! 0 | 4008 | `		GenStateFreeBlock(pBlock);` |
|      ! 0 | 4009 | `		pBlock = pParent;` |
|      ! 0 | 4010 | `	}` |
|        5 | 4011 | `	GenStateReleaseBlock(&pGen->sGlobal);` |
|        - | 4012 | `	/* Release the nested unit's position containers. */` |
|        5 | 4013 | `	SySetRelease(&pGen->aLabel);` |
|        5 | 4014 | `	SySetRelease(&pGen->aGoto);` |
|        5 | 4015 | `	SySetRelease(&pGen->aNullsafeJmp);` |
|        5 | 4016 | `	SySetRelease(&pGen->aLoopParent);` |
|        5 | 4017 | `	SySetRelease(&pGen->aScope);` |
|        5 | 4018 | `	SySetRelease(&pGen->aTrivia);` |
|        5 | 4019 | `	SySetRelease(&pGen->aPendingAttrs);` |
|        5 | 4020 | `	SyBlobRelease(&pGen->sWorker);` |
|        5 | 4021 | `	SyBlobRelease(&pGen->sErrBuf);` |
|        5 | 4022 | `	SyBlobRelease(&pGen->sNamespace);` |
|        5 | 4023 | `	SyHashRelease(&pGen->hUseImports);` |
|        5 | 4024 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|        5 | 4025 | `	SyHashRelease(&pGen->hUseConstImports);` |
|        5 | 4026 | `	GenStateReleaseSeenSymbols(&(*pGen));` |
|        - | 4027 | `	/* Preserve the (possibly grown) shared intern tables across the restore. */` |
|        5 | 4028 | `	hVar = pGen->hVar;` |
|        5 | 4029 | `	hLiteral = pGen->hLiteral;` |
|        5 | 4030 | `	hNumLiteral = pGen->hNumLiteral;` |
|        5 | 4031 | `	*pGen = *pSaved;` |
|        5 | 4032 | `	pGen->hVar = hVar;` |
|        5 | 4033 | `	pGen->hLiteral = hLiteral;` |
|        5 | 4034 | `	pGen->hNumLiteral = hNumLiteral;` |
|        5 | 4035 | `}` |
|        - | 4036 | `/*` |
|        - | 4037 | ` * Raise php's parse error for an unexpected token: E_PARSE with the exact text` |
|        - | 4038 | ` * php's parser prints, e.g.` |
|        - | 4039 | ` *` |
|        - | 4040 | ` *   syntax error, unexpected token ";", expecting "{"` |
|        - | 4041 | ` *   syntax error, unexpected identifier "invalid", expecting "("` |
|        - | 4042 | ` *   syntax error, unexpected end of file` |
|        - | 4043 | ` *` |
|        - | 4044 | ` * php names the token by CLASS, not just by text: an identifier, a variable and` |
|        - | 4045 | ` * a number each get their own noun, while everything else (keywords, operators,` |
|        - | 4046 | ` * punctuation) is a "token". zExpecting is the optional ", expecting ..." tail` |
|        - | 4047 | ` * — pass NULL when the site cannot say what it wanted (php often can't either).` |
|        - | 4048 | ` * pTok == NULL means the input ran out: "unexpected end of file".` |
|        - | 4049 | ` *` |
|        - | 4050 | ` * PHL's hand-written recursive-descent parser has no bison expectation sets, so` |
|        - | 4051 | ` * a site can only claim an "expecting" clause it genuinely knows; every clause` |
|        - | 4052 | ` * emitted here was verified against php 8.5.7 for the construct in question.` |
|        - | 4053 | ` */` |
|      246 | 4054 | `PH7_PRIVATE sxi32 PH7_GenSyntaxError(` |
|        - | 4055 | `	ph7_gen_state *pGen,   /* Code generator state */` |
|        - | 4056 | `	SyToken *pTok,         /* Offending token, or NULL for end of file */` |
|        - | 4057 | `	const char *zExpecting /* ", expecting <this>" tail, or NULL */` |
|        - | 4058 | `	)` |
|        5 | 4059 | `{` |
|        - | 4060 | ``	/* php's noun for the offending token. An ALPHA-stream operator (`and`, `or`,`` |
|        - | 4061 | ``	 * `xor`, `new`, `clone`, `instanceof`) is lexed ID\|OP here but php calls it a`` |
|        - | 4062 | `	 * TOKEN, like every other reserved word — only a real identifier gets the` |
|        - | 4063 | `	 * "identifier" noun. */` |
|      251 | 4064 | `	const char *zNoun = "token";` |
|        - | 4065 | `	sxu32 nLine;` |
|      251 | 4066 | `	if( pTok == 0 && pGen->pTokenSet ){` |
|        - | 4067 | `		/* The caller ran out of tokens inside its own slice — but a statement's slice stops` |
|        - | 4068 | `		 * BEFORE its terminator, so the token php actually names (typically the ';') is the` |
|        - | 4069 | `		 * one sitting just past the slice, still inside the chunk's token stream. Reach for` |
|        - | 4070 | `		 * it before concluding "end of file". */` |
|       99 | 4071 | `		SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|       99 | 4072 | `		SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|       99 | 4073 | `		if( pGen->pEnd >= pBase && pGen->pEnd < pStreamEnd ){` |
|       92 | 4074 | `			pTok = pGen->pEnd;` |
|       44 | 4075 | `		}` |
|       47 | 4076 | `	}` |
|      251 | 4077 | `	nLine = pTok ? pTok->nLine : (pGen->pIn > (SyToken *)SySetBasePtr(pGen->pTokenSet) ? pGen->pIn[-1].nLine : 1);` |
|      251 | 4078 | `	if( pTok == 0 ){` |
|       12 | 4079 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        3 | 4080 | `			zExpecting ? "syntax error, unexpected end of file, expecting %s"` |
|        - | 4081 | `			           : "syntax error, unexpected end of file",` |
|        3 | 4082 | `			zExpecting);` |
|        - | 4083 | `	}` |
|      245 | 4084 | `	if( (pTok->nType & PH7_TK_ID) && (pTok->nType & PH7_TK_OP) == 0 ){` |
|       22 | 4085 | `		zNoun = "identifier";` |
|      236 | 4086 | `	}else if( pTok->nType & PH7_TK_DOLLAR ){` |
|        8 | 4087 | `		zNoun = "variable";` |
|        - | 4088 | `		/* The '$' is its own token and carries only "$" as text; the NAME is the` |
|        - | 4089 | ``		 * token after it. php names the whole variable, so `$x` was being reported`` |
|        - | 4090 | ``		 * as the nameless `variable "$"`. Stitch the two back together. */`` |
|        8 | 4091 | `		if( pGen->pTokenSet ){` |
|        8 | 4092 | `			SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        8 | 4093 | `			SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|        8 | 4094 | `			SyToken *pName = &pTok[1];` |
|        6 | 4095 | `			if( pTok >= pBase && pName < pStreamEnd` |
|        6 | 4096 | `				&& (pName->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|        8 | 4097 | `				&& pName->sData.nByte > 0 ){` |
|        8 | 4098 | `				SyBlobReset(&pGen->sWorker);` |
|        8 | 4099 | `				SyBlobAppend(&pGen->sWorker,"$",sizeof(char));` |
|        8 | 4100 | `				SyBlobAppend(&pGen->sWorker,pName->sData.zString,pName->sData.nByte);` |
|        - | 4101 | `				{` |
|        - | 4102 | `					SyString sVar;` |
|        8 | 4103 | `					SyStringInitFromBuf(&sVar,SyBlobData(&pGen->sWorker),` |
|        - | 4104 | `						SyBlobLength(&pGen->sWorker));` |
|        8 | 4105 | `					if( zExpecting ){` |
|       11 | 4106 | `						return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        - | 4107 | `							"syntax error, unexpected %s \"%z\", expecting %s",` |
|        3 | 4108 | `							zNoun,&sVar,zExpecting);` |
|        - | 4109 | `					}` |
|      ! 0 | 4110 | `					return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|      ! 0 | 4111 | `						"syntax error, unexpected %s \"%z\"",zNoun,&sVar);` |
|        - | 4112 | `				}` |
|        - | 4113 | `			}` |
|      ! 0 | 4114 | `		}` |
|      221 | 4115 | `	}else if( pTok->nType & PH7_TK_INTEGER ){` |
|       28 | 4116 | `		zNoun = "integer";` |
|      209 | 4117 | `	}else if( pTok->nType & PH7_TK_REAL ){` |
|      ! 0 | 4118 | `		zNoun = "float";` |
|      ! 0 | 4119 | `	}` |
|      239 | 4120 | `	if( zExpecting ){` |
|      164 | 4121 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|       53 | 4122 | `			"syntax error, unexpected %s \"%z\", expecting %s",zNoun,&pTok->sData,zExpecting);` |
|        - | 4123 | `	}` |
|      197 | 4124 | `	return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|       64 | 4125 | `		"syntax error, unexpected %s \"%z\"",zNoun,&pTok->sData);` |
|      128 | 4126 | `}` |
|        - | 4127 | `/*` |
|        - | 4128 | ` * Generate a compile-time error message.` |
|        - | 4129 | ` * If the error count limit is reached (usually 15 error message)` |
|        - | 4130 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|        - | 4131 | ` * abort compilation immediately.` |
|        - | 4132 | ` */` |
|     1068 | 4133 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|        5 | 4134 | `{` |
|     1073 | 4135 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|     1073 | 4136 | `	const char *zErr = "Error";` |
|        - | 4137 | `	SyString *pFile;` |
|        - | 4138 | `	va_list ap;` |
|        - | 4139 | `	sxi32 rc;` |
|        - | 4140 | `	/* Reset the working buffer */` |
|     1073 | 4141 | `	SyBlobReset(pWorker);` |
|        - | 4142 | `	/* Peek the processed file path if available */` |
|     1073 | 4143 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|     1073 | 4144 | `	if( nErrType == E_ERROR \|\| nErrType == E_PARSE ){` |
|        - | 4145 | `		/* Increment the error counter. A PARSE error is every bit as fatal as an` |
|        - | 4146 | `		 * E_ERROR one: php compiles nothing, runs nothing and exits 255. Counting` |
|        - | 4147 | `		 * only E_ERROR let a parse error print its diagnostic and then fall through` |
|        - | 4148 | `		 * into execution with a 0 exit status. */` |
|     1031 | 4149 | `		pGen->nErr++;` |
|     1031 | 4150 | `		if( pGen->nErr > 15 ){` |
|        - | 4151 | `			/* Error count limit reached */` |
|        5 | 4152 | `			if( pGen->xErr ){` |
|        5 | 4153 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|        5 | 4154 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|        5 | 4155 | `				if( pFile ){` |
|        5 | 4156 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|        2 | 4157 | `				}` |
|        5 | 4158 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|        5 | 4159 | `				if( SyBlobLength(pWorker) > 0 ){` |
|        5 | 4160 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|        2 | 4161 | `				}` |
|        2 | 4162 | `			}` |
|        - | 4163 | `			/* Abort immediately */` |
|        5 | 4164 | `			return SXERR_ABORT;` |
|        - | 4165 | `		}` |
|      511 | 4166 | `	}` |
|     1069 | 4167 | `	if( pGen->xErr == 0 ){` |
|        - | 4168 | `		/* No consumer — but keep the BARE message in the error buffer so a caller` |
|        - | 4169 | `		 * that needs the text can read it back. eval() compiles with logging off` |
|        - | 4170 | `		 * (a parse error there is php's catchable ParseError, not a printed` |
|        - | 4171 | `		 * diagnostic) and needs exactly this string for the exception message. */` |
|       50 | 4172 | `		va_start(ap,zFormat);` |
|       50 | 4173 | `		SyBlobFormatAp(pWorker,zFormat,ap);` |
|       50 | 4174 | `		va_end(ap);` |
|       50 | 4175 | `		return SXRET_OK;` |
|        - | 4176 | `	}` |
|     1021 | 4177 | `	switch(nErrType){` |
|      582 | 4178 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|       47 | 4179 | `	case E_WARNING: zErr = "Warning";     break;` |
|      400 | 4180 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|      ! 0 | 4181 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|      ! 0 | 4182 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|      ! 0 | 4183 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|      ! 0 | 4184 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|      ! 0 | 4185 | `	case 8192 /* E_DEPRECATED */: zErr = "Deprecated"; break;` |
|      ! 0 | 4186 | `	default:` |
|      ! 0 | 4187 | `		break;` |
|        - | 4188 | `	}` |
|     1021 | 4189 | `	rc = SXRET_OK;` |
|        - | 4190 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|     1021 | 4191 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|     1021 | 4192 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|     1021 | 4193 | `	va_start(ap,zFormat);` |
|     1021 | 4194 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|     1021 | 4195 | `	va_end(ap);` |
|     1021 | 4196 | `	if( pFile ){` |
|     1021 | 4197 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|      508 | 4198 | `	}` |
|        - | 4199 | `	/* Append a new line */` |
|     1021 | 4200 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|     1021 | 4201 | `	if( SyBlobLength(pWorker) > 0 ){` |
|        - | 4202 | `		/* Consume the generated error message */` |
|     1021 | 4203 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|      508 | 4204 | `	}` |
|     1021 | 4205 | `	return rc;` |
|      539 | 4206 | `}` |
|        - | 4207 |  |
