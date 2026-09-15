# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1492/1621 lines (92.04%)

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
|         - |   18 | ` * Check if the given name refer to a valid label.` |
|         - |   19 | ` * Return SXRET_OK and write a pointer to that label on success.` |
|         - |   20 | ` * Any other return value indicates no such label.` |
|         - |   21 | ` */` |
|       148 |   22 | `static sxi32 GenStateGetLabel(ph7_gen_state *pGen,SyString *pName,Label **ppOut)` |
|         5 |   23 | `{` |
|         - |   24 | `	Label *aLabel;` |
|         - |   25 | `	sxu32 n;` |
|         - |   26 | `	/* Perform a linear scan on the label table */` |
|       153 |   27 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|       333 |   28 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|       277 |   29 | `		if( SyStringCmp(&aLabel[n].sName,pName,SyMemcmp) == 0 ){` |
|         - |   30 | `			/* Jump destination found */` |
|        97 |   31 | `			aLabel[n].bRef = TRUE;` |
|        97 |   32 | `			if( ppOut ){` |
|        97 |   33 | `				*ppOut = &aLabel[n];` |
|        46 |   34 | `			}` |
|        97 |   35 | `			return SXRET_OK;` |
|         - |   36 | `		}` |
|        93 |   37 | `	}` |
|         - |   38 | `	/* No such destination */` |
|        60 |   39 | `	return SXERR_NOTFOUND;` |
|        79 |   40 | `}` |
|         - |   41 | `/*` |
|         - |   42 | ` * Fetch a block that correspond to the given criteria from the stack of` |
|         - |   43 | ` * compiled blocks.` |
|         - |   44 | ` * Return a pointer to that block on success. NULL otherwise.` |
|         - |   45 | ` */` |
|    178664 |   46 | `PH7_PRIVATE GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|         5 |   47 | `{` |
|    178669 |   48 | `	GenBlock *pBlock = pCurrent;` |
|    361067 |   49 | `	for(;;){` |
|    722139 |   50 | `		if( pBlock->iFlags & iBlockType ){` |
|    178669 |   51 | `			iCount--; /* Decrement nesting level */` |
|    178669 |   52 | `			if( iCount < 1 ){` |
|         - |   53 | `				/* Block meet with the desired criteria */` |
|    178643 |   54 | `				return pBlock;` |
|         - |   55 | `			}` |
|        13 |   56 | `		}` |
|         - |   57 | `		/* Point to the upper block */` |
|    543501 |   58 | `		pBlock = pBlock->pParent;` |
|    543501 |   59 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|         - |   60 | `			/* Forbidden */` |
|        17 |   61 | `			break;` |
|         - |   62 | `		}` |
|         5 |   63 | `	}` |
|         - |   64 | `	/* No such block */` |
|        30 |   65 | `	return 0;` |
|     89337 |   66 | `}` |
|         - |   67 | `/*` |
|         - |   68 | ` * Initialize a freshly allocated block instance.` |
|         - |   69 | ` */` |
|  13097008 |   70 | `static void GenStateInitBlock(` |
|         - |   71 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |   72 | `	GenBlock *pBlock,    /* Target block */` |
|         - |   73 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   74 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|         - |   75 | `	void *pUserData      /* Upper layer private data */` |
|         - |   76 | `	)` |
|         5 |   77 | `{` |
|         - |   78 | `	/* Initialize block fields */` |
|  13097013 |   79 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  13097013 |   80 | `	pBlock->pUserData   = pUserData;` |
|  13097013 |   81 | `	pBlock->pGen        = pGen;` |
|  13097013 |   82 | `	pBlock->iFlags      = iType;` |
|  13097013 |   83 | `	pBlock->pParent     = 0;` |
|  13097013 |   84 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  13097013 |   85 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  13097013 |   86 | `}` |
|         - |   87 | `/*` |
|         - |   88 | ` * Allocate a new block instance.` |
|         - |   89 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|         - |   90 | ` * on success.Otherwise generate a compile-time error and abort` |
|         - |   91 | ` * processing on failure.` |
|         - |   92 | ` */` |
|  13093124 |   93 | `PH7_PRIVATE sxi32 GenStateEnterBlock(` |
|         - |   94 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |   95 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   96 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|         - |   97 | `	void *pUserData,      /* Upper layer private data */` |
|         - |   98 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|         - |   99 | `	)` |
|         5 |  100 | `{` |
|         - |  101 | `	GenBlock *pBlock;` |
|         - |  102 | `	/* Allocate a new block instance */` |
|  13093129 |  103 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  13093129 |  104 | `	if( pBlock == 0 ){` |
|         - |  105 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |  106 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - |  107 | `		 */` |
|       ! 0 |  108 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|         - |  109 | `		/* Abort processing immediately */` |
|       ! 0 |  110 | `		return SXERR_ABORT;` |
|         - |  111 | `	}` |
|         - |  112 | `	/* Zero the structure */` |
|  13093129 |  113 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  13093129 |  114 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|         - |  115 | `	/* Link to the parent block */` |
|  13093129 |  116 | `	pBlock->pParent = pGen->pCurrent;` |
|         - |  117 | `	/* A loop or switch gets an id, and remembers the loop it nests inside, so a goto's` |
|         - |  118 | `	 * and a label's positions can be compared after compilation (see aLoopParent). */` |
|  13093129 |  119 | `	if( iType & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    540479 |  120 | `		sxu32 nParent = pGen->nCurLoopId;` |
|    540479 |  121 | `		pGen->nLoopId++;` |
|    540479 |  122 | `		SySetPut(&pGen->aLoopParent,(const void *)&nParent);` |
|    540479 |  123 | `		pBlock->nLoopId = pGen->nLoopId;` |
|    540479 |  124 | `		pBlock->nOuterLoopId = nParent;` |
|    540479 |  125 | `		pGen->nCurLoopId = pGen->nLoopId;` |
|    270237 |  126 | `	}` |
|         - |  127 | `	/* Mark as the current block */` |
|  13093129 |  128 | `	pGen->pCurrent = pBlock;` |
|  13093129 |  129 | `	if( ppBlock ){` |
|         - |  130 | `		/* Write a pointer to the new instance */` |
|   6296889 |  131 | `		*ppBlock = pBlock;` |
|   3148442 |  132 | `	}` |
|  13093129 |  133 | `	return SXRET_OK;` |
|   6546567 |  134 | `}` |
|         - |  135 | `/*` |
|         - |  136 | ` * Release block fields without freeing the whole instance.` |
|         - |  137 | ` */` |
|  13093116 |  138 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|         5 |  139 | `{` |
|  13093121 |  140 | `	SySetRelease(&pBlock->aPostContFix);` |
|  13093121 |  141 | `	SySetRelease(&pBlock->aJumpFix);` |
|  13093121 |  142 | `}` |
|         - |  143 | `/*` |
|         - |  144 | ` * Release a block.` |
|         - |  145 | ` */` |
|  13093112 |  146 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|         5 |  147 | `{` |
|  13093117 |  148 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  13093117 |  149 | `	GenStateReleaseBlock(&(*pBlock));` |
|         - |  150 | `	/* Free the instance */` |
|  13093117 |  151 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  13093117 |  152 | `}` |
|         - |  153 | `/*` |
|         - |  154 | ` * POP and release a block from the stack of compiled blocks.` |
|         - |  155 | ` */` |
|  13093112 |  156 | `PH7_PRIVATE sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|         5 |  157 | `{` |
|  13093117 |  158 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  13093117 |  159 | `	if( pBlock == 0 ){` |
|         - |  160 | `		/* No more block to pop */` |
|       ! 0 |  161 | `		return SXERR_EMPTY;` |
|         - |  162 | `	}` |
|  13093117 |  163 | `	if( pBlock->iFlags & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    540471 |  164 | `		pGen->nCurLoopId = pBlock->nOuterLoopId;` |
|    270233 |  165 | `	}` |
|         - |  166 | `	/* Point to the upper block */` |
|  13093117 |  167 | `	pGen->pCurrent = pBlock->pParent;` |
|  13093117 |  168 | `	if( ppBlock ){` |
|         - |  169 | `		/* Write a pointer to the popped block */` |
|       ! 0 |  170 | `		*ppBlock = pBlock;` |
|       ! 0 |  171 | `	}else{` |
|         - |  172 | `		/* Safely release the block */` |
|  13093117 |  173 | `		GenStateFreeBlock(&(*pBlock));` |
|         - |  174 | `	}` |
|  13093117 |  175 | `	return SXRET_OK;` |
|   6546561 |  176 | `}` |
|         - |  177 | `/*` |
|         - |  178 | ` * PHP-parity redeclaration guard.` |
|         - |  179 | ` *` |
|         - |  180 | ` * PHP raises a fatal "Cannot redeclare ..." when a class/interface/trait/enum` |
|         - |  181 | ` * or a function is declared a second time. PHL hoists every declaration into` |
|         - |  182 | `` * the VM at compile time (so `if(false){class C{}}` already makes C exist), and`` |
|         - |  183 | ` * historically it silently *overwrote* duplicates. We reproduce PHP for the` |
|         - |  184 | ` * case that matters and that real code hits: a declaration that is` |
|         - |  185 | ` * UNCONDITIONAL and at file top level, whose name is already bound by another` |
|         - |  186 | ` * unconditional top-level declaration (or by a builtin). Conditional` |
|         - |  187 | ` * declarations (inside if/loops/switch/try or nested in a function) are left` |
|         - |  188 | `` * hoisting as before, so the `if(!class_exists('C')){class C{}}` and`` |
|         - |  189 | `` * `if(false){class C{}} class C{}` guard idioms keep working.`` |
|         - |  190 | ` *` |
|         - |  191 | ` * Included files compile at include time (i.e. at run time relative to the main` |
|         - |  192 | ` * script), so this compile-time check surfaces the fatal at the same moment PHP` |
|         - |  193 | ` * does for the cross-include case too.` |
|         - |  194 | ` */` |
|   1043402 |  195 | `PH7_PRIVATE int GenStateUnconditionalTopLevel(ph7_gen_state *pGen)` |
|         5 |  196 | `{` |
|   1043407 |  197 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   1047391 |  198 | `	while( pBlock ){` |
|   1047391 |  199 | `		if( pBlock->iFlags & (GEN_BLOCK_COND\|GEN_BLOCK_LOOP\|GEN_BLOCK_FUNC\|GEN_BLOCK_SWITCH\|GEN_BLOCK_EXCEPTION) ){` |
|        77 |  200 | `			return 0; /* conditional / nested */` |
|         - |  201 | `		}` |
|   1047319 |  202 | `		if( pBlock->iFlags & GEN_BLOCK_GLOBAL ){` |
|   1043335 |  203 | `			return 1; /* reached the global block with no conditional ancestor */` |
|         - |  204 | `		}` |
|      3989 |  205 | `		pBlock = pBlock->pParent;` |
|         5 |  206 | `	}` |
|       ! 0 |  207 | `	return 1;` |
|    521706 |  208 | `}` |
|         - |  209 | `/*` |
|         - |  210 | ` * Guard a top-level function about to be installed. Same contract as the class` |
|         - |  211 | ` * guard above.` |
|         - |  212 | ` */` |
|    521326 |  213 | `PH7_PRIVATE sxi32 GenStateGuardFuncRedeclaration(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|         5 |  214 | `{` |
|         - |  215 | `	SyHashEntry *pEntry;` |
|    521331 |  216 | `	if( !GenStateUnconditionalTopLevel(pGen) ){` |
|        53 |  217 | `		return SXRET_OK;` |
|         - |  218 | `	}` |
|    521281 |  219 | `	pFunc->iFlags \|= VM_FUNC_BOUND;` |
|    521281 |  220 | `	if( pGen->pVm->bCompilingBuiltin ){` |
|    519925 |  221 | `		return SXRET_OK;` |
|         - |  222 | `	}` |
|         - |  223 | ``	/* NOTE: a userland function shadowing a C builtin (e.g. `function strlen(){}`)`` |
|         - |  224 | `	 * is NOT caught here — the C builtins register in PH7_VmMakeReady, after user` |
|         - |  225 | `	 * code has compiled, so hHostFunction is still empty at this point. Prelude` |
|         - |  226 | `	 * functions (ini_get, ...) and every builtin CLASS compile earlier and ARE` |
|         - |  227 | `	 * guarded. Redeclaring a C builtin function stays a known divergence. */` |
|      1361 |  228 | `	pEntry = SyHashGet(&pGen->pVm->hFunction,(const void *)pFunc->sName.zString,pFunc->sName.nByte);` |
|      1361 |  229 | `	if( pEntry ){` |
|         9 |  230 | `		ph7_vm_func *pPrev = (ph7_vm_func *)pEntry->pUserData;` |
|        11 |  231 | `		while( pPrev ){` |
|         9 |  232 | `			if( pPrev->iFlags & VM_FUNC_BOUND ){` |
|         6 |  233 | `				if( pPrev->sFile.nByte > 0 ){` |
|         4 |  234 | `					PH7_GenCompileError(pGen,E_ERROR,pFunc->nLine,` |
|         - |  235 | `						"Cannot redeclare function %z() (previously declared in %.*s:%u)",` |
|         1 |  236 | `						&pFunc->sName,pPrev->sFile.nByte,pPrev->sFile.zString,pPrev->nLine);` |
|         2 |  237 | `				}else{` |
|         4 |  238 | `					PH7_GenCompileError(pGen,E_ERROR,pFunc->nLine,` |
|         1 |  239 | `						"Cannot redeclare function %z()",&pFunc->sName);` |
|         - |  240 | `				}` |
|         6 |  241 | `				return SXERR_ABORT;` |
|         - |  242 | `			}` |
|         3 |  243 | `			pPrev = pPrev->pNextName;` |
|         1 |  244 | `		}` |
|         1 |  245 | `	}` |
|      1357 |  246 | `	return SXRET_OK;` |
|    260668 |  247 | `}` |
|         - |  248 | `/*` |
|         - |  249 | ` * Emit a forward jump.` |
|         - |  250 | ` * Notes on forward jumps` |
|         - |  251 | ` *  Compilation of some PHP constructs such as if,for,while and the logical or` |
|         - |  252 | ` *  (\|\|) and logical and (&&) operators in expressions requires the` |
|         - |  253 | ` *  generation of forward jumps.` |
|         - |  254 | ` *  Since the destination PC target of these jumps isn't known when the jumps` |
|         - |  255 | ` *  are emitted, we record each forward jump in an instance of the following` |
|         - |  256 | ` *  structure. Those jumps are fixed later when the jump destination is resolved.` |
|         - |  257 | ` */` |
|   4728554 |  258 | `PH7_PRIVATE sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|         5 |  259 | `{` |
|         - |  260 | `	JumpFixup sJumpFix;` |
|         - |  261 | `	sxi32 rc;` |
|         - |  262 | `	/* Init the JumpFixup structure */` |
|   4728559 |  263 | `	sJumpFix.nJumpType = nJumpType;` |
|   4728559 |  264 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|         - |  265 | `	/* Insert in the jump fixup table */` |
|   4728559 |  266 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|   4728559 |  267 | `	return rc;` |
|         5 |  268 | `}` |
|         - |  269 | `/*` |
|         - |  270 | ` * Fix a forward jump now the jump destination is resolved.` |
|         - |  271 | ` * Return the total number of fixed jumps.` |
|         - |  272 | ` * Notes on forward jumps:` |
|         - |  273 | ` *  Compilation of some PHP constructs such as if,for,while and the logical or` |
|         - |  274 | ` *  (\|\|) and logical and (&&) operators in expressions requires the` |
|         - |  275 | ` *  generation of forward jumps.` |
|         - |  276 | ` *  Since the destination PC target of these jumps isn't known when the jumps` |
|         - |  277 | ` *  are emitted, we record each forward jump in an instance of the following` |
|         - |  278 | ` *  structure.Those jumps are fixed later when the jump destination is resolved.` |
|         - |  279 | ` */` |
|   9102328 |  280 | `PH7_PRIVATE sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|         5 |  281 | `{` |
|         - |  282 | `	JumpFixup *aFix;` |
|         - |  283 | `	VmInstr *pInstr;` |
|         - |  284 | `	sxu32 nFixed;` |
|         - |  285 | `	sxu32 n;` |
|         - |  286 | `	/* Point to the jump fixup table */` |
|   9102333 |  287 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|         - |  288 | `	/* Fix the desired jumps */` |
|  19052605 |  289 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|   9950277 |  290 | `		if( aFix[n].nJumpType < 0 ){` |
|         - |  291 | `			/* Already fixed */` |
|   3715459 |  292 | `			continue;` |
|         - |  293 | `		}` |
|   6234823 |  294 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|         - |  295 | `			/* Not of our interest */` |
|   1506271 |  296 | `			continue;` |
|         - |  297 | `		}` |
|         - |  298 | `		/* Point to the instruction to fix */` |
|   4728557 |  299 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|   4728557 |  300 | `		if( pInstr ){` |
|   4728557 |  301 | `			pInstr->iP2 = nJumpDest;` |
|   4728557 |  302 | `			nFixed++;` |
|         - |  303 | `			/* Mark as fixed */` |
|   4728557 |  304 | `			aFix[n].nJumpType = -1;` |
|   2364276 |  305 | `		}` |
|   2364281 |  306 | `	}` |
|         - |  307 | `	/* Total number of fixed jumps */` |
|   9102333 |  308 | `	return nFixed;` |
|         5 |  309 | `}` |
|         - |  310 | `/*` |
|         - |  311 | ` * Fix a 'goto' now the jump destination is resolved.` |
|         - |  312 | ` * The goto statement can be used to jump to another section` |
|         - |  313 | ` * in the program.` |
|         - |  314 | ` * Refer to the routine responsible of compiling the goto` |
|         - |  315 | ` * statement for more information.` |
|         - |  316 | ` */` |
|   3370000 |  317 | `PH7_PRIVATE sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|         5 |  318 | `{` |
|         - |  319 | `	JumpFixup *pJump,*aJumps;` |
|         - |  320 | `	Label *pLabel;` |
|         - |  321 | `	VmInstr *pInstr;` |
|         - |  322 | `	sxi32 rc;` |
|         - |  323 | `	sxu32 n;` |
|         - |  324 | `	/* Point to the goto table */` |
|   3370005 |  325 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|         - |  326 | `	/* Fix */` |
|   3370151 |  327 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
|       153 |  328 | `		pJump = &aJumps[n];` |
|         - |  329 | `		/* Extract the target label */` |
|       153 |  330 | `		rc = GenStateGetLabel(&(*pGen),&pJump->sLabel,&pLabel);` |
|       153 |  331 | `		if( rc != SXRET_OK ){` |
|         - |  332 | `			/* No such label */` |
|        60 |  333 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"'goto' to undefined label '%z'",&pJump->sLabel);` |
|        60 |  334 | `			if( rc == SXERR_ABORT ){` |
|         3 |  335 | `				return SXERR_ABORT;` |
|         - |  336 | `			}` |
|        58 |  337 | `			continue;` |
|         - |  338 | `		}` |
|         - |  339 | `		/* php's one goto restriction: you may not jump INTO a loop or a switch. The label` |
|         - |  340 | `		 * is inside one exactly when it carries a loop id; that is legal only if the same` |
|         - |  341 | `		 * loop also encloses the goto, i.e. the label's loop is the goto's loop or one of` |
|         - |  342 | `		 * its ancestors. Walk up from the goto's loop looking for the label's. */` |
|        97 |  343 | `		if( pLabel->nLoopId != 0 ){` |
|       ! 0 |  344 | `			sxu32 *aParent = (sxu32 *)SySetBasePtr(&pGen->aLoopParent);` |
|       ! 0 |  345 | `			sxu32 nCur = pJump->nLoopId;` |
|       ! 0 |  346 | `			int bInside = 0;` |
|       ! 0 |  347 | `			while( nCur != 0 ){` |
|       ! 0 |  348 | `				if( nCur == pLabel->nLoopId ){` |
|       ! 0 |  349 | `					bInside = 1;` |
|       ! 0 |  350 | `					break;` |
|         - |  351 | `				}` |
|       ! 0 |  352 | `				nCur = (nCur <= SySetUsed(&pGen->aLoopParent)) ? aParent[nCur - 1] : 0;` |
|       ! 0 |  353 | `			}` |
|       ! 0 |  354 | `			if( !bInside ){` |
|       ! 0 |  355 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,` |
|         - |  356 | `					"'goto' into loop or switch statement is disallowed");` |
|       ! 0 |  357 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  358 | `					return SXERR_ABORT;` |
|         - |  359 | `				}` |
|       ! 0 |  360 | `				continue;` |
|         - |  361 | `			}` |
|       ! 0 |  362 | `		}` |
|         - |  363 | `		/* Make sure the target label is reachable */` |
|        97 |  364 | `		if( pLabel->pFunc != pJump->pFunc ){` |
|        11 |  365 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"'goto' to undefined label '%z'",&pJump->sLabel);` |
|        11 |  366 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  367 | `				return SXERR_ABORT;` |
|         - |  368 | `			}` |
|         4 |  369 | `		}` |
|         - |  370 | `		/* Fix the jump now the destination is resolved */` |
|        97 |  371 | `		pInstr = PH7_VmGetInstr(pGen->pVm,pJump->nInstrIdx);` |
|        97 |  372 | `		if( pInstr ){` |
|        97 |  373 | `			pInstr->iP2 = pLabel->nJumpDest;` |
|        46 |  374 | `		}` |
|        51 |  375 | `	}` |
|         - |  376 | `	/* php says nothing about a label nobody jumps to — the old "defined but not` |
|         - |  377 | `	 * referenced" warning was a PH7-ism with no counterpart in the oracle. */` |
|   3370003 |  378 | `	return SXRET_OK;` |
|   1685005 |  379 | `}` |
|         - |  380 | `/*` |
|         - |  381 | ` * Check if a given token value is installed in the literal table.` |
|         - |  382 | ` */` |
|  16881100 |  383 | `PH7_PRIVATE sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|         5 |  384 | `{` |
|         - |  385 | `	SyHashEntry *pEntry;` |
|  16881105 |  386 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  16881105 |  387 | `	if( pEntry == 0 ){` |
|   4500399 |  388 | `		return SXERR_NOTFOUND;` |
|         - |  389 | `	}` |
|  12380711 |  390 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  12380711 |  391 | `	return SXRET_OK;` |
|   8440555 |  392 | `}` |
|         - |  393 | `/*` |
|         - |  394 | ` * Install a given constant index in the literal table.` |
|         - |  395 | ` * In order to be installed, the ph7_value must be of type string.` |
|         - |  396 | ` *` |
|         - |  397 | ` * NOTE: empty strings are deliberately omitted here.  The VM reserves a` |
|         - |  398 | ` * single shared constant for "" during initialization (pVm->nEmptyStringIdx)` |
|         - |  399 | ` * and the compiler emits a LOADC referencing that slot whenever an empty` |
|         - |  400 | ` * literal is encountered.  This keeps the literal hash from growing when` |
|         - |  401 | ` * many "" literals appear in user code.` |
|         - |  402 | ` */` |
|   4500394 |  403 | `PH7_PRIVATE sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|         5 |  404 | `{` |
|   4500399 |  405 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|   4500399 |  406 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   2250197 |  407 | `	}` |
|   4500399 |  408 | `	return SXRET_OK;` |
|         5 |  409 | `}` |
|         - |  410 | `/*` |
|         - |  411 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|         - |  412 | ` * in the constant table.` |
|         - |  413 | ` */` |
|   3816490 |  414 | `PH7_PRIVATE ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|         5 |  415 | `{` |
|         - |  416 | `	ph7_value *pObj;` |
|   3816495 |  417 | `	sxu32 nIdx = 0; /* cc warning */` |
|         - |  418 | `	/* Reserve a new constant */` |
|   3816495 |  419 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   3816495 |  420 | `	if( pObj == 0 ){` |
|       ! 0 |  421 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  422 | `		return 0;` |
|         - |  423 | `	}` |
|   3816495 |  424 | `	*pIdx = nIdx;` |
|         - |  425 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|         - |  426 | `	 * the constant string iterals table [optimization purposes].` |
|         - |  427 | `	 */` |
|   3816495 |  428 | `	return pObj;` |
|   1908250 |  429 | `}` |
|         - |  430 | `/*` |
|         - |  431 | ` * Implementation of the PHP language constructs.` |
|         - |  432 | ` */` |
|         - |  433 | `/*` |
|         - |  434 | ` * Ensure the about-to-be-emitted CALL/NEW opcode carries a VmCallArgMap` |
|         - |  435 | ` * that reflects the caller file's strict_types mode. Returns the (possibly` |
|         - |  436 | ` * newly allocated and zero-initialized) map pointer. In weak-mode files` |
|         - |  437 | ` * this is a no-op and the caller's p3 is returned unchanged.` |
|         - |  438 | ` *` |
|         - |  439 | ` * NOTE: on allocation failure the call reverts to weak semantics rather` |
|         - |  440 | ` * than aborting compilation — out-of-memory during a map allocation is` |
|         - |  441 | ` * vanishingly unlikely and silently dropping to weak mode matches the` |
|         - |  442 | ` * surrounding callsites' zero-check fallback pattern.` |
|         - |  443 | ` */` |
|   7941844 |  444 | `PH7_PRIVATE void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|         5 |  445 | `{` |
|         - |  446 | `	VmCallArgMap *pMap;` |
|   7941849 |  447 | `	if( !pGen->bStrictTypes ) return p3;` |
|        58 |  448 | `	if( p3 == 0 ){` |
|        54 |  449 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|        54 |  450 | `		if( pMap == 0 ) return 0;` |
|        54 |  451 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|        54 |  452 | `		p3 = (void *)pMap;` |
|        25 |  453 | `	}` |
|        58 |  454 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|        58 |  455 | `	return p3;` |
|   3970927 |  456 | `}` |
|         - |  457 | `/* Forward declaration */` |
|         - |  458 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut);` |
|         - |  459 | `/* Forward declarations */` |
|         - |  460 | `/*` |
|         - |  461 | ` * Recover from a compile-time error. In other words synchronize` |
|         - |  462 | ` * the token stream cursor with the first semi-colon seen.` |
|         - |  463 | ` */` |
|         8 |  464 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|         1 |  465 | `{` |
|         - |  466 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|        17 |  467 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|         9 |  468 | `		pGen->pIn++;` |
|         1 |  469 | `	}` |
|         9 |  470 | `	return SXRET_OK;` |
|         1 |  471 | `}` |
|         - |  472 | `/*` |
|         - |  473 | ` * Check if the given identifier name is reserved or not.` |
|         - |  474 | ` * Return TRUE if reserved.FALSE otherwise.` |
|         - |  475 | ` */` |
|    341680 |  476 | `PH7_PRIVATE int GenStateIsReservedConstant(SyString *pName)` |
|         5 |  477 | `{` |
|    341685 |  478 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      3929 |  479 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|         3 |  480 | `			return TRUE;` |
|      3927 |  481 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|         6 |  482 | `			return TRUE;` |
|         5 |  483 | `		}` |
|    339720 |  484 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|      7785 |  485 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|         3 |  486 | `			return TRUE;` |
|         - |  487 | `		}` |
|      3889 |  488 | `	}` |
|         - |  489 | `	/* Not a reserved constant */` |
|    341677 |  490 | `	return FALSE;` |
|    170845 |  491 | `}` |
|         - |  492 | `/*` |
|         - |  493 | ` * Chain operators participate in a postfix member-access chain.` |
|         - |  494 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|         - |  495 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|         - |  496 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|         - |  497 | ` */` |
|         - |  498 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|         - |  499 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|         - |  500 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|         - |  501 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|         - |  502 |  |
|         - |  503 | `/*` |
|         - |  504 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|         - |  505 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|         - |  506 | ` * patched entries from the pending set.` |
|         - |  507 | ` */` |
|  53952954 |  508 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|         5 |  509 | `{` |
|  53952959 |  510 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|         - |  511 | `	sxu32 nTarget;` |
|         - |  512 | `	sxu32 *aIdx;` |
|         - |  513 | `	sxu32 i;` |
|  53952959 |  514 | `	if( nCur <= nBaseline ){` |
|  53952863 |  515 | `		return;` |
|         - |  516 | `	}` |
|        99 |  517 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|        99 |  518 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|       203 |  519 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|       107 |  520 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|       107 |  521 | `		if( pInstr ){` |
|       107 |  522 | `			pInstr->iP2 = (sxi32)nTarget;` |
|        52 |  523 | `		}` |
|        55 |  524 | `	}` |
|        99 |  525 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
|  26976482 |  526 | `}` |
|         - |  527 |  |
|         - |  528 | `/*` |
|         - |  529 | ` * By-reference out-parameters of builtin functions.` |
|         - |  530 | ` *` |
|         - |  531 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|         - |  532 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|         - |  533 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|         - |  534 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|         - |  535 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|         - |  536 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|         - |  537 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|         - |  538 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|         - |  539 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|         - |  540 | ` * creates it" behaviour).` |
|         - |  541 | ` *` |
|         - |  542 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|         - |  543 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|         - |  544 | ` */` |
|   7053524 |  545 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|         5 |  546 | `{` |
|         - |  547 | `	static const struct {` |
|         - |  548 | `		const char *zName;` |
|         - |  549 | `		sxu32 nByte;` |
|         - |  550 | `		sxu32 mask;` |
|         - |  551 | `	} aByRef[] = {` |
|         - |  552 | `		{ "parse_str",              9, 1u<<1 },  /* &$result (apArg[1]) */` |
|         - |  553 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|         - |  554 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|         - |  555 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|         - |  556 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|         - |  557 | `		{ "similar_text",          12, 1u<<2 },  /* &$percent (apArg[2]) */` |
|         - |  558 | `		{ "fsockopen",              9, (1u<<2)\|(1u<<3) },  /* &$error_code, &$error_message */` |
|         - |  559 | `		{ "pfsockopen",            10, (1u<<2)\|(1u<<3) },  /* same */` |
|         - |  560 | `		{ "stream_socket_client",  20, (1u<<1)\|(1u<<2) },  /* &$error_code, &$error_message */` |
|         - |  561 | `		{ "proc_open",              9, 1u<<2 },  /* &$pipes (apArg[2]) */` |
|         - |  562 | `	};` |
|         - |  563 | `	sxu32 i;` |
|   7053529 |  564 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|   1805795 |  565 | `		return 0;` |
|         - |  566 | `	}` |
|  57242747 |  567 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
|  52053374 |  568 | `		if( pName->nByte == aByRef[i].nByte` |
|  27541607 |  569 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|     58371 |  570 | `			return aByRef[i].mask;` |
|         - |  571 | `		}` |
|  25997509 |  572 | `	}` |
|   5189373 |  573 | `	return 0;` |
|   3526767 |  574 | `}` |
|         - |  575 | `/*` |
|         - |  576 | ` * Recover the bare global-builtin name from a call's callee node.` |
|         - |  577 | ` *` |
|         - |  578 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|         - |  579 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|         - |  580 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|         - |  581 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|         - |  582 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|         - |  583 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|         - |  584 | ` */` |
|   7053524 |  585 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|         5 |  586 | `{` |
|         - |  587 | `	SyToken *p, *pEnd;` |
|   7053529 |  588 | `	pOut->zString = 0;` |
|   7053529 |  589 | `	pOut->nByte = 0;` |
|   7053529 |  590 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|       ! 0 |  591 | `		return;` |
|         - |  592 | `	}` |
|   7053529 |  593 | `	p = pLeft->pStart;` |
|   7053529 |  594 | `	pEnd = pLeft->pEnd;` |
|         - |  595 | `	/* Optional single leading namespace separator (absolute path). */` |
|   7053529 |  596 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|      3923 |  597 | `		p++;` |
|      1959 |  598 | `	}` |
|   7053529 |  599 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|   1805749 |  600 | `		return;` |
|         - |  601 | `	}` |
|         - |  602 | `	/* Must be a single component: nothing follows the name token. */` |
|   5247785 |  603 | `	if( p + 1 != pEnd ){` |
|        51 |  604 | `		return;` |
|         - |  605 | `	}` |
|   5247739 |  606 | `	*pOut = p->sData;` |
|   3526767 |  607 | `}` |
|         - |  608 | `/*` |
|         - |  609 | ` * Generate bytecode for a given expression tree.` |
|         - |  610 | ` * If something goes wrong while generating bytecode` |
|         - |  611 | ` * for the expression tree (A very unlikely scenario)` |
|         - |  612 | ` * this function takes care of generating the appropriate` |
|         - |  613 | ` * error message.` |
|         - |  614 | ` */` |
|  76073186 |  615 | `static sxi32 GenStateEmitExprCode(` |
|         - |  616 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |  617 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|         - |  618 | `	sxi32 iFlags /* Control flags */` |
|         - |  619 | `	)` |
|         5 |  620 | `{` |
|         - |  621 | `	VmInstr *pInstr;` |
|         - |  622 | `	sxu32 nJmpIdx;` |
|  76073191 |  623 | `	sxi32 iP1 = 0;` |
|  76073191 |  624 | `	sxu32 iP2 = 0;` |
|  76073191 |  625 | `	void *p3  = 0;` |
|         - |  626 | `	sxi32 iVmOp;` |
|         - |  627 | `	sxi32 rc;` |
|  76073191 |  628 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
|  76073191 |  629 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
|  76073191 |  630 | `	sxu32 nRhsNsBase = 0;` |
|  76073191 |  631 | `	if( pNode->xCode ){` |
|         - |  632 | `		SyToken *pTmpIn,*pTmpEnd;` |
|         - |  633 | `		/* Compile node */` |
|  45914225 |  634 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
|  45914225 |  635 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
|  45914225 |  636 | `		RE_SWAP_DELIMITER(pGen);` |
|  45914225 |  637 | `		return rc;` |
|         - |  638 | `	}` |
|  30158971 |  639 | `	if( pNode->pOp == 0 ){` |
|       ! 0 |  640 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - |  641 | `			"Invalid expression node,PH7 is aborting compilation");` |
|       ! 0 |  642 | `		return SXERR_ABORT;` |
|         - |  643 | `	}` |
|  30158971 |  644 | `	iVmOp = pNode->pOp->iVmOp;` |
|  30158971 |  645 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|         - |  646 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|         - |  647 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|         - |  648 | `		 * and later errors are still reported. */` |
|         3 |  649 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - |  650 | `			"The (unset) cast is no longer supported");` |
|         3 |  651 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  652 | `			return SXERR_ABORT;` |
|         - |  653 | `		}` |
|         1 |  654 | `	}` |
|  30158971 |  655 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        93 |  656 | `		sxu32 nJmp = 0;` |
|         - |  657 | `		sxu32 nNcNsBase;` |
|         - |  658 | `		VmInstr *pInstrFix;` |
|         - |  659 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|         - |  660 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|         - |  661 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|         - |  662 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|         - |  663 | `		 * stack slot carries a writable nIdx. */` |
|        93 |  664 | `		if( pNode->pRight ){` |
|        93 |  665 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        93 |  666 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|        93 |  667 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  668 | `				return rc;` |
|         - |  669 | `			}` |
|        93 |  670 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|         - |  671 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|         - |  672 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|         - |  673 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|         - |  674 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|         - |  675 | `			 * the store, so the parent array does not need to be copied at` |
|         - |  676 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|         - |  677 | `			 * cascade for the actual write path stays correct. */` |
|        93 |  678 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|        93 |  679 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|        33 |  680 | `				pInstrFix->iP2 = 3;` |
|        15 |  681 | `			}` |
|        45 |  682 | `		}` |
|         - |  683 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|        93 |  684 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|         - |  685 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|        93 |  686 | `		if( pNode->pLeft ){` |
|        93 |  687 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        93 |  688 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|        93 |  689 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  690 | `				return rc;` |
|         - |  691 | `			}` |
|        93 |  692 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|        45 |  693 | `		}` |
|         - |  694 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|        93 |  695 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|         - |  696 | `		/* Patch the short-circuit jump to land after the store. */` |
|        93 |  697 | `		if( nJmp > 0 ){` |
|        93 |  698 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|        93 |  699 | `			if( pInstrFix ){` |
|        93 |  700 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|        45 |  701 | `			}` |
|        45 |  702 | `		}` |
|        93 |  703 | `		return SXRET_OK;` |
|         - |  704 | `	}` |
|  30158881 |  705 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|         - |  706 | `		sxu32 nJz,nJmp;` |
|         - |  707 | `		sxu32 nTernaryNsBase;` |
|         - |  708 | `		/* Ternary operator require special handling */` |
|         - |  709 | `		/* Phase#1: Compile the condition */` |
|    503825 |  710 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    503825 |  711 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    503825 |  712 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  713 | `			return rc;` |
|         - |  714 | `		}` |
|         - |  715 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|         - |  716 | `		 * compiling the condition must short-circuit to the end of the` |
|         - |  717 | `		 * condition expression, not leak past the ternary. */` |
|    503825 |  718 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    503825 |  719 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    503825 |  720 | `		if( pNode->pLeft ){` |
|         - |  721 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|         - |  722 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    499875 |  723 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - |  724 | `			/* Phase#3: Compile the 'then' expression  */` |
|    499875 |  725 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    499875 |  726 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    499875 |  727 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  728 | `				return rc;` |
|         - |  729 | `			}` |
|    499875 |  730 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    249940 |  731 | `		}else{` |
|         - |  732 | `			/* Elvis operator: (expr) ?: (else)` |
|         - |  733 | `			 * Duplicate condition so original value is the 'then' result.` |
|         - |  734 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      3955 |  735 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      3955 |  736 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - |  737 | `		}` |
|         - |  738 | `		/* Phase#4: Emit the unconditional jump */` |
|    503825 |  739 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|         - |  740 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    503825 |  741 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    503825 |  742 | `		if( pInstr ){` |
|    503825 |  743 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    251910 |  744 | `		}` |
|    503825 |  745 | `		if( !pNode->pLeft ){` |
|         - |  746 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      3955 |  747 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      1975 |  748 | `		}` |
|         - |  749 | `		/* Phase#6: Compile the 'else' expression */` |
|    503825 |  750 | `		if( pNode->pRight ){` |
|    503825 |  751 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    503825 |  752 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    503825 |  753 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  754 | `				return rc;` |
|         - |  755 | `			}` |
|    503825 |  756 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    251910 |  757 | `		}` |
|    503825 |  758 | `		if( nJmp > 0 ){` |
|         - |  759 | `			/* Phase#7: Fix the unconditional jump */` |
|    503825 |  760 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    503825 |  761 | `			if( pInstr ){` |
|    503825 |  762 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    251910 |  763 | `			}` |
|    251910 |  764 | `		}` |
|         - |  765 | `		/* All done */` |
|    503825 |  766 | `		return SXRET_OK;` |
|         - |  767 | `	}` |
|  29655061 |  768 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
|         - |  769 | ``		/* PHP 8.5 pipe: `$lhs \|> $rhs` invokes the RHS callable with the LHS`` |
|         - |  770 | ``		 * value as its sole argument [i.e. `$rhs($lhs)`]. Evaluate the LHS (the`` |
|         - |  771 | `		 * argument) first, then the RHS callable, then emit a one-argument` |
|         - |  772 | `		 * OP_CALL — the same stack shape the function-call path builds (the` |
|         - |  773 | `		 * argument sits below the callee). The RHS is any callable expression:` |
|         - |  774 | ``		 * an FCC `f(...)` (an OP_LOAD_FCC Closure), a closure variable, an`` |
|         - |  775 | ``		 * `[obj,method]` pair, or a callable string. */`` |
|         - |  776 | `		sxu32 nPipeNsBase;` |
|        27 |  777 | `		sxi32 iOperandFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE\|EXPR_FLAG_RDONLY_LOAD);` |
|        27 |  778 | `		if( pNode->pLeft == 0 \|\| pNode->pRight == 0 ){` |
|       ! 0 |  779 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - |  780 | `				"'\|>': Missing operand");` |
|       ! 0 |  781 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  782 | `		}` |
|         - |  783 | `		/* Argument: the LHS value. */` |
|        27 |  784 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 |  785 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iOperandFlags);` |
|        27 |  786 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  787 | `			return rc;` |
|         - |  788 | `		}` |
|        27 |  789 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - |  790 | `		/* Callable: the RHS. */` |
|        27 |  791 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 |  792 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iOperandFlags);` |
|        27 |  793 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  794 | `			return rc;` |
|         - |  795 | `		}` |
|        27 |  796 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - |  797 | `		/* Invoke the callable with the single piped argument. */` |
|        27 |  798 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        27 |  799 | `		return SXRET_OK;` |
|         - |  800 | `	}` |
|  29655035 |  801 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|         - |  802 | `	/* Generate code for the left tree */` |
|  29655035 |  803 | `	if( pNode->pLeft ){` |
|  29600903 |  804 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  29600903 |  805 | `		if( iVmOp == PH7_OP_CALL ){` |
|         - |  806 | `			ph7_expr_node **apNode;` |
|   7057749 |  807 | `			int hasSpread = 0;` |
|   7057749 |  808 | `			int hasNamed = 0;` |
|   7057749 |  809 | `			int bAnySpread = 0;` |
|   7057749 |  810 | `			sxu32 byRefMask = 0;` |
|         - |  811 | `			sxi32 nArgs;` |
|         - |  812 | `			sxi32 n;` |
|         - |  813 | `			/* Recurse and generate bytecodes for function arguments */` |
|   7057749 |  814 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   7057749 |  815 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|         - |  816 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|         - |  817 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|         - |  818 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|   7057749 |  819 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|        81 |  820 | `				bFcc = 1;` |
|        81 |  821 | `				nArgs = 0;` |
|        40 |  822 | `			}` |
|         - |  823 | `			/* Validate argument order like php: no positional argument after a` |
|         - |  824 | ``			 * named one OR after unpacking, and `name: ...$x` is a parse error. */`` |
|         - |  825 | `			{` |
|   7057749 |  826 | `				int seenNamed = 0;` |
|   7057749 |  827 | `				int seenSpread = 0;` |
|  15072169 |  828 | `				for( n = 0; n < nArgs; ++n ){` |
|   8014427 |  829 | `					if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|      4081 |  830 | `						bAnySpread = 1;` |
|      4081 |  831 | `						seenSpread = 1;` |
|      4081 |  832 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       ! 0 |  833 | `							rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - |  834 | `								"syntax error, unexpected token \"...\"");` |
|       ! 0 |  835 | `							return SXERR_SYNTAX;` |
|         5 |  836 | `						}` |
|   8012389 |  837 | `					}else if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       300 |  838 | `						seenNamed = 1;` |
|       300 |  839 | `						hasNamed = 1;` |
|   8010203 |  840 | `					}else if( seenNamed ){` |
|         3 |  841 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - |  842 | `							"Cannot use positional argument after named argument");` |
|         3 |  843 | `						return SXERR_SYNTAX;` |
|   8010053 |  844 | `					}else if( seenSpread ){` |
|       ! 0 |  845 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - |  846 | `							"Cannot use positional argument after argument unpacking");` |
|       ! 0 |  847 | `						return SXERR_SYNTAX;` |
|         - |  848 | `					}` |
|   4007215 |  849 | `				}` |
|         - |  850 | `			}` |
|         - |  851 | `			/* Read-only load */` |
|   7057747 |  852 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|         - |  853 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|         - |  854 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|         - |  855 | `			 * objects dispatch to the right method (offsetExists for both;` |
|         - |  856 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|   7057747 |  857 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|   7057747 |  858 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|   7057742 |  859 | `				if( pCallName->nByte == 5` |
|   3922202 |  860 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|    326329 |  861 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|   6894585 |  862 | `				}else if( pCallName->nByte == 5` |
|   3595878 |  863 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|       117 |  864 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|        56 |  865 | `				}` |
|         - |  866 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|         - |  867 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|         - |  868 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|         - |  869 | `				 * write back through. Skipped when spread/named args are present:` |
|         - |  870 | `				 * the compile-time positional index no longer maps to the` |
|         - |  871 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|   7057747 |  872 | `				if( !bAnySpread && !hasNamed ){` |
|         - |  873 | `					SyString sBuiltin;` |
|   7053529 |  874 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|   7053529 |  875 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|   3526762 |  876 | `				}` |
|   3528871 |  877 | `			}` |
|  15072165 |  878 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|   8014423 |  879 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   8014423 |  880 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|         - |  881 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|         - |  882 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|         - |  883 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|         - |  884 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|         - |  885 | `				 * builtin to write back through. A plain $var target is unaffected` |
|         - |  886 | `				 * (iP1=0 either way). */` |
|   8014423 |  887 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|     38891 |  888 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|     38891 |  889 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|     19443 |  890 | `				}` |
|         - |  891 | ``				/* Slice 21: a plain `$var` argument may bind to a USER-function by-ref`` |
|         - |  892 | `				 * parameter whose signature is unknown at compile time (forward` |
|         - |  893 | `				 * reference, dynamic call, or method dispatch — e.g. PHPUnit's` |
|         - |  894 | ``				 * `willReturnReference($undef)`). Reserve a real memobj slot for it so an`` |
|         - |  895 | `				 * UNDEFINED variable vivifies and the by-ref write-back reaches the caller` |
|         - |  896 | `				 * (php). A by-value parameter still receives a copy; the only divergence` |
|         - |  897 | `				 * is that an undefined variable passed BY VALUE is created as NULL in the` |
|         - |  898 | `				 * caller (recorded in NEWPLAN §2). Excludes isset()/empty()/unset(), which` |
|         - |  899 | `				 * compile through this same call loop but must NEVER create their operand,` |
|         - |  900 | `				 * and named/spread args (positional-index and by-ref semantics don't apply). */` |
|   8014418 |  901 | `				if( (iFlags & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_LOAD_IDX_UNSET)) == 0` |
|   7851188 |  902 | `				 && apNode[n]->pOp == 0 && apNode[n]->xCode == PH7_CompileVariable` |
|   4114668 |  903 | `				 && (apNode[n]->iFlags & (EXPR_NODE_NAMED_ARG\|EXPR_NODE_SPREAD)) == 0 ){` |
|   3362715 |  904 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|   1681355 |  905 | `				}` |
|   8014423 |  906 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|   8014423 |  907 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  908 | `					return rc;` |
|         - |  909 | `				}` |
|         - |  910 | `				/* Each argument is an independent nullsafe scope. */` |
|   8014423 |  911 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|   8014423 |  912 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|         - |  913 | `					/* Emit spread opcode to unpack this array argument */` |
|      4081 |  914 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      4081 |  915 | `					hasSpread = 1;` |
|      2038 |  916 | `				}` |
|   4007214 |  917 | `			}` |
|         - |  918 | `			/* Total number of given arguments */` |
|   7057747 |  919 | `			iP1 = nArgs;` |
|   7057747 |  920 | `			iP2 = hasSpread;` |
|         - |  921 | `			/* Build VmCallArgMap if named arguments are present.` |
|         - |  922 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|   7057747 |  923 | `			if( hasNamed ){` |
|       190 |  924 | `				sxu32 nStrBytes = 0;` |
|         - |  925 | `				char *zBuf;` |
|       562 |  926 | `				for( n = 0; n < nArgs; ++n ){` |
|       376 |  927 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       298 |  928 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|       147 |  929 | `					}` |
|       190 |  930 | `				}` |
|         - |  931 | `				{` |
|       190 |  932 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|       190 |  933 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|       186 |  934 | `					&pGen->pVm->sAllocator, mapSize);` |
|       190 |  935 | `				if( pMap ){` |
|       190 |  936 | `					SyZero(pMap, mapSize);` |
|       190 |  937 | `					pMap->bHasNamed = 1;` |
|       190 |  938 | `					pMap->nTotal = (sxu32)nArgs;` |
|       190 |  939 | `					pMap->aNames = (SyString *)&pMap[1];` |
|       190 |  940 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|       562 |  941 | `					for( n = 0; n < nArgs; ++n ){` |
|       376 |  942 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       298 |  943 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|       298 |  944 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|       298 |  945 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|       298 |  946 | `							zBuf += nb;` |
|       147 |  947 | `						}` |
|         - |  948 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|       190 |  949 | `					}` |
|       190 |  950 | `					p3 = (void *)pMap;` |
|        93 |  951 | `				}` |
|         - |  952 | `				}` |
|        93 |  953 | `			}` |
|         - |  954 | `			/* Remove stale flags now */` |
|   7057747 |  955 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|   3528871 |  956 | `		}` |
|         - |  957 | `		{` |
|         - |  958 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|         - |  959 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|         - |  960 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|         - |  961 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|         - |  962 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|         - |  963 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|         - |  964 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|         - |  965 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
|  29600901 |  966 | `			sxi32 iLeftFlags = iFlags;` |
|  29600901 |  967 | `			sxu32 nNullcLhsFirst = PH7_VmInstrLength(pGen->pVm);` |
|  29600901 |  968 | `			int bNullcLhs = 0;` |
|  29600896 |  969 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
|  24034646 |  970 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|   9234224 |  971 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|   7949395 |  972 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   2779789 |  973 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|   1389892 |  974 | `			}` |
|         - |  975 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|         - |  976 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|         - |  977 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|         - |  978 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|         - |  979 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|         - |  980 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|         - |  981 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
|  29600896 |  982 | `			if( pNode->pOp` |
|  41615788 |  983 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
|  26815387 |  984 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  24029826 |  985 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|   5991219 |  986 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|   2995607 |  987 | `			}` |
|         - |  988 | ``			/* `++`/`--` mutate their operand in place — the operand is a write`` |
|         - |  989 | ``			 * lvalue exactly like a compound assign's (`$o->m[0]++` must tag the`` |
|         - |  990 | ``			 * member base PH7_MEMBER_WRITE the way `$o->m[0] += 1` does: hooked`` |
|         - |  991 | `			 * properties throw php's Indirect-modification Error, missing ones` |
|         - |  992 | `			 * auto-vivify). The prec-18 site below handles the assign family;` |
|         - |  993 | ``			 * `++`/`--` are unary, their operand is pLeft. */`` |
|  29600896 |  994 | `			if( pNode->pOp` |
|  29600901 |  995 | `				&& (pNode->pOp->iVmOp == PH7_OP_INCR \|\| pNode->pOp->iVmOp == PH7_OP_DECR) ){` |
|    217725 |  996 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE` |
|         - |  997 | ``					\| EXPR_FLAG_RMW_LOAD /* php warns before seeding `$undef++` */;`` |
|    108860 |  998 | `			}` |
|         - |  999 | ``			/* `??` reads its LEFT operand in isset-context: an undefined or`` |
|         - | 1000 | `			 * UNINITIALIZED typed PROPERTY must yield the default rather than a` |
|         - | 1001 | `			 * warning/Error (php). Tag a member-access LHS so its OP_MEMBER takes` |
|         - | 1002 | `			 * the silent-lookup path (iP2 = ISSET), which still loads a present` |
|         - | 1003 | `			 * value. A SUBSCRIPT LHS is left untagged: LOAD_IDX's ISSET mode means` |
|         - | 1004 | ``			 * offsetExists (a bool), but `$o[$k] ?? d` needs the offsetGet value —`` |
|         - | 1005 | `			 * that path is already handled correctly by OP_NULLC. */` |
|  29600901 | 1006 | `			if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC && pNode->pLeft ){` |
|         - | 1007 | ``				/* php reads the ENTIRE left operand of `??` in isset-context: no`` |
|         - | 1008 | ``				 * "Undefined variable" for `$x ?? d` OR for the base of`` |
|         - | 1009 | ``				 * `$x['k'] ?? d`. QUIET_VAR silences the variable read wherever it`` |
|         - | 1010 | `				 * sits in the chain. */` |
|     58389 | 1011 | `				iLeftFlags \|= EXPR_FLAG_QUIET_VAR;` |
|     58389 | 1012 | `				bNullcLhs = 1;` |
|     58384 | 1013 | `				if( pNode->pLeft->pOp` |
|     87512 | 1014 | `					&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|     58326 | 1015 | `						\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|     58309 | 1016 | `						\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|         - | 1017 | `					/* A member-access LHS additionally takes OP_MEMBER's silent` |
|         - | 1018 | `					 * lookup (iP2 = ISSET) so an uninitialized typed property` |
|         - | 1019 | `					 * yields the default instead of an Error. A SUBSCRIPT LHS must` |
|         - | 1020 | `					 * NOT: LOAD_IDX's ISSET mode means offsetExists (a bool), while` |
|         - | 1021 | ``					 * `$o[$k] ?? d` needs the offsetGet value — OP_NULLC already`` |
|         - | 1022 | `					 * handles that path. */` |
|        37 | 1023 | `					iLeftFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|        18 | 1024 | `				}` |
|     29192 | 1025 | `			}` |
|  29600901 | 1026 | `			if( iVmOp == PH7_OP_ERR_CTRL ){` |
|         - | 1027 | `				/* '@' must suppress the diagnostics raised WHILE its operand runs, so` |
|         - | 1028 | `				 * open the window here; the trailing emit below closes it (iP1 = 0). */` |
|     15761 | 1029 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_ERR_CTRL,1,0,0,0);` |
|      7878 | 1030 | `			}` |
|  29600901 | 1031 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags\|EXPR_FLAG_RDONLY_LOAD);` |
|  29600901 | 1032 | `			if( rc == SXRET_OK && bNullcLhs ){` |
|         - | 1033 | ``				/* Mark EVERY subscript read in the `??` left chain quiet (iP2=8).`` |
|         - | 1034 | `				 * Peeking at the next instruction only catches the OUTERMOST` |
|         - | 1035 | ``				 * access, so `$d['x']['y'] ?? $v` still warned for the inner one;`` |
|         - | 1036 | `				 * and a forward scan would be unsound, since an unrelated sibling` |
|         - | 1037 | ``				 * access can sit immediately before a coalesce (`f($a['k'],`` |
|         - | 1038 | ``				 * $b['j'] ?? 1)`). The compiler knows the real extent, so it marks`` |
|         - | 1039 | `				 * the range it just emitted. Write-context codes (1/3/5) belong to` |
|         - | 1040 | ``				 * `??=` and keep their meaning. */`` |
|     58389 | 1041 | `				sxu32 nEnd = PH7_VmInstrLength(pGen->pVm);` |
|         - | 1042 | `				sxu32 nAt;` |
|    326667 | 1043 | `				for( nAt = nNullcLhsFirst ; nAt < nEnd ; ++nAt ){` |
|    268283 | 1044 | `					VmInstr *pFix = PH7_VmGetInstr(pGen->pVm,nAt);` |
|    268283 | 1045 | `					if( pFix && pFix->iOp == PH7_OP_LOAD_IDX && pFix->iP2 == 0 ){` |
|     58291 | 1046 | `						pFix->iP2 = 8;` |
|     29143 | 1047 | `					}` |
|    134144 | 1048 | `				}` |
|     29192 | 1049 | `			}` |
|         - | 1050 | `		}` |
|  29600901 | 1051 | `		if( rc != SXRET_OK ){` |
|        34 | 1052 | `			return rc;` |
|         - | 1053 | `		}` |
|  29600871 | 1054 | `		if( !bIsChainOp ){` |
|         - | 1055 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|         - | 1056 | `			 * target the end of that LHS chain, which is right here. */` |
|  13525941 | 1057 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|   6762968 | 1058 | `		}` |
|  29600871 | 1059 | `		if( iVmOp == PH7_OP_CALL ){` |
|   7057747 | 1060 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   7057747 | 1061 | `			if( pInstr ){` |
|   7057747 | 1062 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|   5248047 | 1063 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|         - | 1064 | `					sxu32 nQual;` |
|   5248047 | 1065 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 1066 | `					/* Prevent constant expansion but preserve the absolute flag` |
|         - | 1067 | `					 * so the later NEW handler (if any) can see it. */` |
|   5248047 | 1068 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|         - | 1069 | `					/* Namespace-qualify the function name for CALL, unless the` |
|         - | 1070 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|         - | 1071 | `					 * imports — class imports must NOT affect function` |
|         - | 1072 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|         - | 1073 | `					 * before NEW; we store the original literal index in the` |
|         - | 1074 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|         - | 1075 | `					 * the unqualified name and re-qualify with class imports. */` |
|   5248047 | 1076 | `					if( bAbsolute ){` |
|      3923 | 1077 | `						pInstr->iP2 = (sxi32)nOrig;` |
|      1964 | 1078 | `					}else{` |
|   5244129 | 1079 | `						int fromImport = 0;` |
|   5244129 | 1080 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|   5244129 | 1081 | `						pInstr->iP2 = (sxi32)nQual;` |
|   5244129 | 1082 | `						if( nQual != nOrig ){` |
|         - | 1083 | `							/* Record the original literal index in the arg map` |
|         - | 1084 | `							 * (NOT in the CALL's iP2 — that is the hasSpread` |
|         - | 1085 | `							 * flag) so the NEW handler can recover the` |
|         - | 1086 | `							 * unqualified name and re-qualify with CLASS` |
|         - | 1087 | `							 * imports. */` |
|       103 | 1088 | `							if( p3 == 0 ){` |
|       103 | 1089 | `								VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|        98 | 1090 | `									&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|       103 | 1091 | `								if( pMap ){` |
|       103 | 1092 | `									SyZero(pMap, sizeof(VmCallArgMap));` |
|       103 | 1093 | `									p3 = (void *)pMap;` |
|        49 | 1094 | `								}` |
|        49 | 1095 | `							}` |
|       103 | 1096 | `							if( p3 ){` |
|       103 | 1097 | `								((VmCallArgMap *)p3)->nOrigNameLit = nOrig + 1;` |
|       103 | 1098 | `								if( !fromImport ){` |
|         - | 1099 | `									/* Mark as namespace-qualified */` |
|        93 | 1100 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|        44 | 1101 | `								}` |
|        49 | 1102 | `							}` |
|        49 | 1103 | `						}` |
|         - | 1104 | `					}` |
|   4433726 | 1105 | `				}else if( (pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */` |
|   1799646 | 1106 | `						&& !(pNode->pLeft && (pNode->pLeft->iFlags & EXPR_NODE_PARENS)))` |
|    914911 | 1107 | `					\|\| pInstr->iOp == PH7_OP_NEW ){` |
|         - | 1108 | `					/* Method call,flag that. But NOT when the callee was an explicitly` |
|         - | 1109 | ``					 * PARENTHESISED member access: `($o->p)(...)` / `($o::$p)(...)` invokes`` |
|         - | 1110 | `					 * the VALUE of the property (php's variable-invocation), so the OP_MEMBER` |
|         - | 1111 | `					 * must stay a plain property READ (leaving the callable on the stack for` |
|         - | 1112 | `					 * OP_CALL to invoke) rather than being rewritten into a method-name` |
|         - | 1113 | ``					 * resolution — the parens are exactly what distinguishes `($o->p)()` from`` |
|         - | 1114 | ``					 * the method call `$o->p()`. */`` |
|   1789599 | 1115 | `					pInstr->iP2 = 1;` |
|         - | 1116 | ``					/* A static call with a DYNAMIC method name (`C::$m(...)`): the`` |
|         - | 1117 | ``					 * static-`::` codegen folded the variable NAME into OP_MEMBER->p3`` |
|         - | 1118 | ``					 * as if it were a static-PROPERTY read (`C::$m`), but in a CALL the`` |
|         - | 1119 | `					 * method name is the variable's VALUE. Rebuild the sequence` |
|         - | 1120 | `					 * [class, OP_LOAD $m -> value, OP_MEMBER(static, method)] so the` |
|         - | 1121 | `					 * dynamic name is read off the stack, matching the instance` |
|         - | 1122 | ``					 * (`$o->$m()`) path. iP1==1 marks a static member. */`` |
|   1789599 | 1123 | `					if( pInstr->iOp == PH7_OP_MEMBER && pInstr->iP1 == 1 && pInstr->p3 ){` |
|        11 | 1124 | `						void *pDynName = pInstr->p3;` |
|        11 | 1125 | `						(void)PH7_VmPopInstr(pGen->pVm);` |
|        11 | 1126 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,pDynName,0);` |
|        11 | 1127 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_MEMBER,1,PH7_MEMBER_METHOD,0,0);` |
|         5 | 1128 | `					}` |
|    894797 | 1129 | `				}` |
|   3528876 | 1130 | `			}` |
|  26072000 | 1131 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|         - | 1132 | `			ph7_expr_node **apNode;` |
|         - | 1133 | `			sxi32 n;` |
|   3025979 | 1134 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|         - | 1135 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|         - | 1136 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE` |
|         - | 1137 | `				\|EXPR_FLAG_QUIET_VAR\|EXPR_FLAG_RMW_LOAD);` |
|         - | 1138 | `			/* Recurse and generate bytecodes for array index */` |
|   3025979 | 1139 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   5814835 | 1140 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   2788861 | 1141 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   2788861 | 1142 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   2788861 | 1143 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 1144 | `					return rc;` |
|         - | 1145 | `				}` |
|         - | 1146 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   2788861 | 1147 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   1394433 | 1148 | `			}` |
|   3025979 | 1149 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   2788861 | 1150 | `				iP1 = 1; /* Node have an index associated with it */` |
|   1394428 | 1151 | `			}` |
|   3025979 | 1152 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|         - | 1153 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|    372765 | 1154 | `				iP2 = 4;` |
|   2839599 | 1155 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|         - | 1156 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|         - | 1157 | `				 * so the trailing unset() builtin can drop the slot. */` |
|     23361 | 1158 | `				iP2 = 5;` |
|   2641541 | 1159 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|         - | 1160 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|         - | 1161 | `				 * short-circuit on missing keys without invoking offsetGet` |
|         - | 1162 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|        29 | 1163 | `				iP2 = 6;` |
|   2629851 | 1164 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|         - | 1165 | `				/* Create an empty entry when the desired index is not found */` |
|    555759 | 1166 | `				iP2 = 1;` |
|    277882 | 1167 | `			}` |
|  21030142 | 1168 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         - | 1169 | `			/* POP the left node */` |
|         5 | 1170 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         2 | 1171 | `		}` |
|  14800433 | 1172 | `	}` |
|  29655003 | 1173 | `	rc = SXRET_OK;` |
|  29655003 | 1174 | `	nJmpIdx = 0;` |
|         - | 1175 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|         - | 1176 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|         - | 1177 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  29655003 | 1178 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|    466665 | 1179 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    466665 | 1180 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    466665 | 1181 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    466665 | 1182 | `			int isSpecial = 0;` |
|    466665 | 1183 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|    373493 | 1184 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|    373493 | 1185 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|    373488 | 1186 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    348186 | 1187 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    188641 | 1188 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|    147639 | 1189 | `					isSpecial = 1;` |
|     73817 | 1190 | `				}` |
|    210037 | 1191 | `			}` |
|    513251 | 1192 | `			pInstr->iP1 = 0;` |
|         - | 1193 | ``			/* A leading `\` (NSSEP) makes the class name ABSOLUTE: it names the`` |
|         - | 1194 | `			 * global class, so it must NOT be re-qualified with the current namespace.` |
|         - | 1195 | ``			 * `\Closure::bind(...)` inside `namespace X` is Closure, not X\Closure.`` |
|         - | 1196 | ``			 * (Multi-component `\A\B::m` already resolves absolutely because its`` |
|         - | 1197 | ``			 * literal keeps a backslash; the single-component `\Closure` lost it.) */`` |
|         - | 1198 | `			{` |
|    723288 | 1199 | `				int bAbsolute = (pNode->pLeft && pNode->pLeft->pStart` |
|    630111 | 1200 | `					&& (pNode->pLeft->pStart->nType & PH7_TK_NSSEP));` |
|    420079 | 1201 | `				if( !isSpecial && !bAbsolute ){` |
|    272427 | 1202 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|    136211 | 1203 | `				}` |
|         - | 1204 | `			}` |
|         - | 1205 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|         - | 1206 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|    420079 | 1207 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|    272445 | 1208 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|    272445 | 1209 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|        78 | 1210 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|        78 | 1211 | `					return SXRET_OK;` |
|         - | 1212 | `				}` |
|    136183 | 1213 | `			}` |
|    210000 | 1214 | `		}` |
|    303145 | 1215 | `	}` |
|         - | 1216 | `	/* Generate code for the right tree */` |
|  29608361 | 1217 | `	if( pNode->pRight ){` |
|  17005007 | 1218 | `		if( iVmOp == PH7_OP_LAND ){` |
|         - | 1219 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    450587 | 1220 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  16779716 | 1221 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|         - | 1222 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    326083 | 1223 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  16391386 | 1224 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|         - | 1225 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     58389 | 1226 | `			iVmOp = 0; /* No binary operator to emit */` |
|     58389 | 1227 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  16199207 | 1228 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|         - | 1229 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|         - | 1230 | `			 * the entire containing postfix chain to null. The jump target is` |
|         - | 1231 | `			 * patched later by the innermost non-chain ancestor (or by` |
|         - | 1232 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|         - | 1233 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|         - | 1234 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|       107 | 1235 | `			sxu32 nNsJmp = 0;` |
|       107 | 1236 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|       107 | 1237 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  16169911 | 1238 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|         - | 1239 | `			/* The lvalue is the RIGHT operand (these ops are right-associative). Mark it a write` |
|         - | 1240 | ``			 * target so a missing member (the base of a subscript-write, or a bare `$o->p`) is`` |
|         - | 1241 | `			 * auto-created — PHP auto-vivifies on write. */` |
|   5215213 | 1242 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|   5215213 | 1243 | `			if( iVmOp != PH7_OP_STORE ){` |
|         - | 1244 | ``				/* COMPOUND assignment (`.=`, `+=`, ...) READS the target first, so`` |
|         - | 1245 | `` 				 * php warns when it is undefined and then seeds it; a plain `=` `` |
|         - | 1246 | `				 * writes without reading and stays silent. */` |
|    450435 | 1247 | `				iFlags \|= EXPR_FLAG_RMW_LOAD;` |
|    225215 | 1248 | `			}` |
|   2607604 | 1249 | `		}` |
|  17005007 | 1250 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  17005007 | 1251 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_RDONLY_LOAD);` |
|  17005007 | 1252 | `		if( !bIsChainOp ){` |
|         - | 1253 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|         - | 1254 | `			 * operator instruction is emitted. */` |
|  11013867 | 1255 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|   5506931 | 1256 | `		}` |
|  17005007 | 1257 | `		if( iVmOp == PH7_OP_STORE ){` |
|   4764783 | 1258 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|   4764746 | 1259 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|         - | 1260 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|         - | 1261 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|         - | 1262 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|         - | 1263 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|         - | 1264 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|         - | 1265 | `				 */` |
|        91 | 1266 | `				iVmOp = 0;` |
|   4764740 | 1267 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|   4764697 | 1268 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 1269 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|    920335 | 1270 | `					iP2 = 1;` |
|    460170 | 1271 | `				}else{` |
|   3844367 | 1272 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 1273 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|    536259 | 1274 | `						iVmOp = PH7_OP_STORE_IDX;` |
|    536259 | 1275 | `						iP1 = pInstr->iP1;` |
|    268132 | 1276 | `					}else{` |
|   3308113 | 1277 | `						p3 = pInstr->p3;` |
|         - | 1278 | `					}` |
|         - | 1279 | `					/* POP the last dynamic load instruction */` |
|   3844367 | 1280 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         - | 1281 | `				}` |
|   2382351 | 1282 | `			}` |
|  14622618 | 1283 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|         - | 1284 | `			/* Peek first: a member LHS ($o->p =& $x, C::$s =& $x) keeps its` |
|         - | 1285 | `			 * OP_MEMBER in place (it resolves + stashes the target slot at` |
|         - | 1286 | `			 * runtime), unlike the variable/array shapes which fold their load` |
|         - | 1287 | `			 * away. Mirrors the normal member-store fold above (iP2 kept-MEMBER). */` |
|        73 | 1288 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|        73 | 1289 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 1290 | `				/* Tag the member as a reference target and flag STORE_REF (iP2=1)` |
|         - | 1291 | `				 * to take the member-rebind path in the VM. */` |
|        11 | 1292 | `				pInstr->iP2 = PH7_MEMBER_REF_TARGET;` |
|        11 | 1293 | `				iP2 = 1;` |
|         6 | 1294 | `			}else{` |
|        63 | 1295 | `				pInstr = PH7_VmPopInstr(pGen->pVm);` |
|        63 | 1296 | `				if( pInstr ){` |
|        63 | 1297 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 1298 | `						/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|         - | 1299 | `						 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|         - | 1300 | `						 */` |
|        19 | 1301 | `						iVmOp = PH7_OP_STORE_IDX_REF;` |
|        19 | 1302 | `						iP1 = pInstr->iP1;` |
|        19 | 1303 | `						iP2 = pInstr->iP2;` |
|        19 | 1304 | `						p3  = pInstr->p3;` |
|        10 | 1305 | `					}else{` |
|        45 | 1306 | `						p3 = pInstr->p3;` |
|         - | 1307 | `					}` |
|        30 | 1308 | `				}` |
|         - | 1309 | `			}` |
|        35 | 1310 | `		}` |
|   8502501 | 1311 | `	}` |
|  29608356 | 1312 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|    432508 | 1313 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|         - | 1314 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|         - | 1315 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|        34 | 1316 | `		iVmOp = 0;` |
|        15 | 1317 | `	}` |
|  29608361 | 1318 | `	if( iVmOp > 0 ){` |
|  29549857 | 1319 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|    217725 | 1320 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|         - | 1321 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|     15553 | 1322 | `				iP1 = 1;` |
|      7779 | 1323 | `			}` |
|  29440997 | 1324 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|         - | 1325 | `			/* Namespace-qualify the class name for NEW */ {` |
|    868239 | 1326 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|    868239 | 1327 | `				VmInstr *pCallInstr = 0;` |
|    868239 | 1328 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|    856399 | 1329 | `					pCallInstr = pPeek;` |
|    856399 | 1330 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    428197 | 1331 | `				}` |
|    868239 | 1332 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|    852711 | 1333 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 1334 | `					sxu32 nLitForClass;` |
|    852711 | 1335 | `					VmCallArgMap *pCallNsMap = pCallInstr ? (VmCallArgMap *)pCallInstr->p3 : 0;` |
|         - | 1336 | `					/* If the CALL handler qualified the name with FUNCTION` |
|         - | 1337 | `					 * imports, recover the original literal (recorded in the` |
|         - | 1338 | `					 * arg map — OP_CALL's iP2 is the hasSpread flag, and` |
|         - | 1339 | `` 					 * misreading it as a literal index made `new C(...$args)` `` |
|         - | 1340 | `					 * fatal with "Class ' ' is not defined") and re-qualify` |
|         - | 1341 | `					 * with class imports. */` |
|    852711 | 1342 | `					if( pCallNsMap && pCallNsMap->nOrigNameLit > 0 ){` |
|        55 | 1343 | `						nLitForClass = pCallNsMap->nOrigNameLit - 1;` |
|        30 | 1344 | `					}else{` |
|    852661 | 1345 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|         - | 1346 | `					}` |
|    852711 | 1347 | `					pPeek->iP1 = 0;` |
|    852711 | 1348 | `					if( !bAbsolute ){` |
|         - | 1349 | `						/* self/static/parent are resolved at runtime against the` |
|         - | 1350 | `						 * current class — never namespace-qualify them (else` |
|         - | 1351 | ``						 * `new self` in namespace N becomes "N\self"). Mirrors the`` |
|         - | 1352 | `						 * instanceof (IS_A) guard below. */` |
|    848803 | 1353 | `						ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nLitForClass);` |
|    848803 | 1354 | `						int isSpecialNew = 0;` |
|    848803 | 1355 | `						if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|    833675 | 1356 | `							const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|    833675 | 1357 | `							sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|    833670 | 1358 | `							if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    837399 | 1359 | `								(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    416783 | 1360 | `								(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      7799 | 1361 | `								isSpecialNew = 1;` |
|      3897 | 1362 | `							}` |
|    420617 | 1363 | `						}` |
|    856367 | 1364 | `						if( isSpecialNew ){` |
|      7799 | 1365 | `							pPeek->iP2 = (sxi32)nLitForClass;` |
|      3902 | 1366 | `						}else{` |
|    833445 | 1367 | `							pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|         - | 1368 | `						}` |
|    420622 | 1369 | `					}else{` |
|      3913 | 1370 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|         - | 1371 | `					}` |
|    422571 | 1372 | `				}` |
|         - | 1373 | `			}` |
|    860675 | 1374 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    860675 | 1375 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|         - | 1376 | `				VmInstr *pPrev;` |
|    856399 | 1377 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|    856399 | 1378 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|         - | 1379 | `					/* Pop the call instruction, preserve named-arg map and` |
|         - | 1380 | `					 * the hasSpread flag (OP_NEW consumes the spread` |
|         - | 1381 | `					 * accumulator exactly like OP_CALL would have). */` |
|    856399 | 1382 | `					iP1 = pInstr->iP1;` |
|    856399 | 1383 | `					iP2 = pInstr->iP2;` |
|    856399 | 1384 | `					if( pInstr->p3 ){` |
|        65 | 1385 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|        30 | 1386 | `					}` |
|    856399 | 1387 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    428197 | 1388 | `				}` |
|    428202 | 1389 | `			}` |
|  28894238 | 1390 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|         - | 1391 | `			/* instanceof: right operand is a class name, not a constant.` |
|         - | 1392 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     81793 | 1393 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     81793 | 1394 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     81793 | 1395 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     81793 | 1396 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     81793 | 1397 | `				int isSpecialIs = 0;` |
|     81793 | 1398 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     81793 | 1399 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     81793 | 1400 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     81788 | 1401 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     81791 | 1402 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     40894 | 1403 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|        12 | 1404 | `						isSpecialIs = 1;` |
|         5 | 1405 | `					}` |
|     40894 | 1406 | `				}` |
|     81793 | 1407 | `				pInstr->iP1 = 0;` |
|     81793 | 1408 | `				if( !isSpecialIs && !bAbsolute ){` |
|     81773 | 1409 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     40884 | 1410 | `				}` |
|     40899 | 1411 | `			}` |
|  28423009 | 1412 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|         - | 1413 | `			/* Prevent constant expansion for member/property names.` |
|         - | 1414 | `			 * The right child (member name) was just compiled — its LOADC` |
|         - | 1415 | `			 * should not trigger constant lookup. */` |
|   5991145 | 1416 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   5991145 | 1417 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|   5750483 | 1418 | `				pInstr->iP1 = 0;` |
|   2875239 | 1419 | `			}` |
|   5991145 | 1420 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|         - | 1421 | `				/* Static member access,remember that */` |
|    420023 | 1422 | `				iP1 = 1;` |
|    420023 | 1423 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    420023 | 1424 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|    236773 | 1425 | `					p3 = pInstr->p3;` |
|    236773 | 1426 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    118384 | 1427 | `				}` |
|    210009 | 1428 | `			}` |
|         - | 1429 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|         - | 1430 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|         - | 1431 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|         - | 1432 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|   5991145 | 1433 | `			if( iP2 == PH7_MEMBER_READ ){` |
|   5991145 | 1434 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|        42 | 1435 | `					iP2 = PH7_MEMBER_UNSET;` |
|   5991125 | 1436 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|     69983 | 1437 | `					iP2 = PH7_MEMBER_ISSET;` |
|   5956116 | 1438 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|        17 | 1439 | `					iP2 = PH7_MEMBER_EMPTY;` |
|   5921119 | 1440 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|         - | 1441 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|   1114521 | 1442 | `					iP2 = PH7_MEMBER_WRITE;` |
|    557258 | 1443 | `				}` |
|   2995570 | 1444 | `			}` |
|   2995570 | 1445 | `		}` |
|         - | 1446 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|         - | 1447 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|         - | 1448 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|         - | 1449 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|         - | 1450 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
|  29542293 | 1451 | `		if( bFcc ){` |
|        81 | 1452 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|        81 | 1453 | `			iP2 = 0;` |
|        81 | 1454 | `			p3 = 0;` |
|        81 | 1455 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|        81 | 1456 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 1457 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|         - | 1458 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|         - | 1459 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|         - | 1460 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|        37 | 1461 | `				void *pMemberName = pInstr->p3;` |
|        37 | 1462 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|        37 | 1463 | `				if( pMemberName ){` |
|       ! 0 | 1464 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|       ! 0 | 1465 | `				}` |
|        37 | 1466 | `				iP1 = 2;` |
|        19 | 1467 | `			}else{` |
|        45 | 1468 | `				iP1 = 1;` |
|         - | 1469 | `			}` |
|        40 | 1470 | `		}` |
|         - | 1471 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|         - | 1472 | `		 * This is the primary emit path for user-visible calls. */` |
|  29542293 | 1473 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|   7918337 | 1474 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|   3959166 | 1475 | `		}` |
|         - | 1476 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  29542293 | 1477 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  14771144 | 1478 | `	}` |
|  29600797 | 1479 | `	if( nJmpIdx > 0 ){` |
|         - | 1480 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|    835049 | 1481 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|    835049 | 1482 | `		if( pInstr ){` |
|    835049 | 1483 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    417522 | 1484 | `		}` |
|    417522 | 1485 | `	}` |
|  29600797 | 1486 | `	return rc;` |
|  38009532 | 1487 | `}` |
|         - | 1488 | `/*` |
|         - | 1489 | ` * Compile a PHP expression.` |
|         - | 1490 | ` * According to the PHP language reference manual:` |
|         - | 1491 | ` *  Expressions are the most important building stones of PHP.` |
|         - | 1492 | ` *  In PHP, almost anything you write is an expression.` |
|         - | 1493 | ` *  The simplest yet most accurate way to define an expression` |
|         - | 1494 | ` *  is "anything that has a value".` |
|         - | 1495 | ` * If something goes wrong while compiling the expression,this` |
|         - | 1496 | ` * function takes care of generating the appropriate error` |
|         - | 1497 | ` * message.` |
|         - | 1498 | ` */` |
|         - | 1499 | `/*` |
|         - | 1500 | ` * Does this expression tree contain a comma OPERATOR node?` |
|         - | 1501 | ` *` |
|         - | 1502 | `` * PH7 shipped `,` as a lowest-precedence binary operator (IMP-0139-COMMA), so`` |
|         - | 1503 | `` * `(1, 2)` and `$x = (f(), $y)` compile and evaluate to the right operand.`` |
|         - | 1504 | ` * php 8 has no comma operator: its grammar only allows comma-separated` |
|         - | 1505 | ` * expression LISTS inside for(...) clauses (call arguments, array literals and` |
|         - | 1506 | ` * list() are split by the parser, never by this node). Accepting it changes the` |
|         - | 1507 | ` * meaning of source php rejects, which §10 classes as a bug — so every context` |
|         - | 1508 | ` * except for() now reports php's parse error.` |
|         - | 1509 | ` */` |
| 252350636 | 1510 | `static int GenStateTreeHasComma(ph7_expr_node *pNode)` |
|         5 | 1511 | `{` |
|         - | 1512 | `	ph7_expr_node **apArg;` |
|         - | 1513 | `	sxu32 n;` |
| 252350641 | 1514 | `	if( pNode == 0 ){` |
| 177411279 | 1515 | `		return 0;` |
|         - | 1516 | `	}` |
|  74939367 | 1517 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         6 | 1518 | `		return 1;` |
|         - | 1519 | `	}` |
|  74939358 | 1520 | `	if( GenStateTreeHasComma(pNode->pLeft) \|\| GenStateTreeHasComma(pNode->pRight)` |
|  74939359 | 1521 | `	 \|\| GenStateTreeHasComma(pNode->pCond) ){` |
|         6 | 1522 | `		return 1;` |
|         - | 1523 | `	}` |
|  74939359 | 1524 | `	apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  85719433 | 1525 | `	for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; n++ ){` |
|  10780079 | 1526 | `		if( GenStateTreeHasComma(apArg[n]) ){` |
|       ! 0 | 1527 | `			return 1;` |
|         - | 1528 | `		}` |
|   5390042 | 1529 | `	}` |
|  74939359 | 1530 | `	return 0;` |
| 126175323 | 1531 | `}` |
|  17125612 | 1532 | `PH7_PRIVATE sxi32 PH7_CompileExpr(` |
|         - | 1533 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 1534 | `	sxi32 iFlags,        /* Control flags */` |
|         - | 1535 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|         - | 1536 | `	)` |
|         5 | 1537 | `{` |
|         - | 1538 | `	ph7_expr_node *pRoot;` |
|         - | 1539 | `	SySet sExprNode;` |
|         - | 1540 | `	SyToken *pEnd;` |
|         - | 1541 | `	sxi32 nExpr;` |
|         - | 1542 | `	sxi32 iNest;` |
|         - | 1543 | `	sxi32 rc;` |
|         - | 1544 | `	sxu32 nNullsafeBase;` |
|         - | 1545 | `	/* Initialize worker variables */` |
|  17125617 | 1546 | `	nExpr = 0;` |
|  17125617 | 1547 | `	pRoot = 0;` |
|         - | 1548 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|         - | 1549 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  17125617 | 1550 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  17125617 | 1551 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  17125617 | 1552 | `	SySetAlloc(&sExprNode,0x10);` |
|  17125617 | 1553 | `	rc = SXRET_OK;` |
|         - | 1554 | `	/* Delimit the expression */` |
|  17125617 | 1555 | `	pEnd = pGen->pIn;` |
|  17125617 | 1556 | `	iNest = 0;` |
| 134065519 | 1557 | `	while( pEnd < pGen->pEnd ){` |
| 127480969 | 1558 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 1559 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|      4727 | 1560 | `			iNest++;` |
| 127478608 | 1561 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|      4737 | 1562 | `			iNest--;` |
| 127473881 | 1563 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  10541945 | 1564 | `			if( iNest <= 0 ){` |
|  10541067 | 1565 | `				break;` |
|         - | 1566 | `			}` |
|       439 | 1567 | `		}` |
| 116939907 | 1568 | `		pEnd++;` |
|         5 | 1569 | `	}` |
|  17125617 | 1570 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|    738337 | 1571 | `		SyToken *pEnd2 = pGen->pIn;` |
|    738337 | 1572 | `		iNest = 0;` |
|         - | 1573 | `		/* Stop at the first comma */` |
|   1621345 | 1574 | `		while( pEnd2 < pEnd ){` |
|    883015 | 1575 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|     50557 | 1576 | `				iNest++;` |
|    857739 | 1577 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|     50557 | 1578 | `				iNest--;` |
|    807187 | 1579 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      6061 | 1580 | `				if( iNest <= 0 ){` |
|         3 | 1581 | `					break;` |
|         - | 1582 | `				}` |
|      3027 | 1583 | `			}` |
|    883013 | 1584 | `			pEnd2++;` |
|         5 | 1585 | `		}` |
|    738337 | 1586 | `		if( pEnd2 <pEnd ){` |
|         3 | 1587 | `			pEnd = pEnd2;` |
|         1 | 1588 | `		}` |
|    369166 | 1589 | `	}` |
|  17125617 | 1590 | `	if( pEnd > pGen->pIn ){` |
|  17102329 | 1591 | `		SyToken *pTmp = pGen->pEnd;` |
|         - | 1592 | `		/* Swap delimiter */` |
|  17102329 | 1593 | `		pGen->pEnd = pEnd;` |
|         - | 1594 | `		/* Try to get an expression tree */` |
|  17102329 | 1595 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  17102324 | 1596 | `		if( rc == SXRET_OK && pRoot && pGen->nCommaExprOk < 1` |
|  16927325 | 1597 | `		 && GenStateTreeHasComma(pRoot) ){` |
|         - | 1598 | `			/* php has no comma operator outside a for() clause */` |
|         6 | 1599 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pRoot->pStart->nLine,` |
|         - | 1600 | `				"syntax error, unexpected token \",\"");` |
|         6 | 1601 | `			pGen->pEnd = pTmp;` |
|         6 | 1602 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 1603 | `				SySetRelease(&sExprNode);` |
|       ! 0 | 1604 | `				return SXERR_ABORT;` |
|         - | 1605 | `			}` |
|         6 | 1606 | `			pGen->pIn = pEnd;` |
|         6 | 1607 | `			SySetRelease(&sExprNode);` |
|         6 | 1608 | `			SySetTruncate(&pGen->aNullsafeJmp,nNullsafeBase);` |
|         6 | 1609 | `			return SXRET_OK;` |
|         - | 1610 | `		}` |
|  17102325 | 1611 | `		if( rc == SXRET_OK && pRoot ){` |
|  17102145 | 1612 | `			rc = SXRET_OK;` |
|  17102145 | 1613 | `			if( xTreeValidator ){` |
|         - | 1614 | `				/* Call the upper layer validator callback */` |
|   1034017 | 1615 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    517006 | 1616 | `			}` |
|  17102145 | 1617 | `			if( rc != SXERR_ABORT ){` |
|         - | 1618 | `				/* Generate code for the given tree */` |
|  17102145 | 1619 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|         - | 1620 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|         - | 1621 | `				 * expression so they short-circuit to its end. */` |
|  17102145 | 1622 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|   8551070 | 1623 | `			}` |
|  17102145 | 1624 | `			nExpr = 1;` |
|   8551070 | 1625 | `		}` |
|         - | 1626 | `		/* Release the whole tree */` |
|  17102325 | 1627 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|         - | 1628 | `		/* Synchronize token stream */` |
|  17102325 | 1629 | `		pGen->pEnd = pTmp;` |
|  17102325 | 1630 | `		pGen->pIn  = pEnd;` |
|  17102325 | 1631 | `		if( rc == SXERR_ABORT ){` |
|        18 | 1632 | `			SySetRelease(&sExprNode);` |
|        18 | 1633 | `			return SXERR_ABORT;` |
|         - | 1634 | `		}` |
|   8551153 | 1635 | `	}` |
|  17125599 | 1636 | `	SySetRelease(&sExprNode);` |
|  17125599 | 1637 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|   8562811 | 1638 | `}` |
|         - | 1639 | `/*` |
|         - | 1640 | ` * Return a pointer to the node construct handler associated` |
|         - | 1641 | ` * with a given node type [i.e: string,integer,float,...].` |
|         - | 1642 | ` */` |
|   9511964 | 1643 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|         5 | 1644 | `{` |
|   9511969 | 1645 | `	if( nNodeType & PH7_TK_NUM ){` |
|         - | 1646 | `		/* Numeric literal: Either real or integer */` |
|   3825349 | 1647 | `		return PH7_CompileNumLiteral;` |
|   5686625 | 1648 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|         - | 1649 | `		/* Double quoted string */` |
|    124999 | 1650 | `		return PH7_CompileString;` |
|   5561631 | 1651 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|         - | 1652 | `		/* Single quoted string */` |
|   5561507 | 1653 | `		return PH7_CompileSimpleString;` |
|       129 | 1654 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|         - | 1655 | `		/* Heredoc */` |
|        73 | 1656 | `		return PH7_CompileHereDoc;` |
|        60 | 1657 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|         - | 1658 | `		/* Nowdoc */` |
|        56 | 1659 | `		return PH7_CompileNowDoc;` |
|         5 | 1660 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|         - | 1661 | `		/* Backtick quoted string */` |
|         3 | 1662 | `		return PH7_CompileBacktic;` |
|         - | 1663 | `	}` |
|         3 | 1664 | `	return 0;` |
|   4755987 | 1665 | `}` |
|         - | 1666 | `/*` |
|         - | 1667 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|         - | 1668 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|         - | 1669 | ` * in write context" parse error.` |
|         - | 1670 | ` */` |
|     23398 | 1671 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|         5 | 1672 | `{` |
|         - | 1673 | `	sxi32 rc;` |
|     23403 | 1674 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|     23401 | 1675 | `		return SXRET_OK;` |
|         - | 1676 | `	}` |
|         5 | 1677 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|         2 | 1678 | `		pNode ? pNode->pStart->nLine : 1,` |
|         - | 1679 | `		"Can't use nullsafe operator in write context");` |
|         3 | 1680 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|     11704 | 1681 | `}` |
|         - | 1682 | `/*` |
|         - | 1683 | ` * Compile an unset() statement.` |
|         - | 1684 | ` * unset($var, $arr[$key], ...);` |
|         - | 1685 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|         - | 1686 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|         - | 1687 | ` * parent array before extracting the element to unset.` |
|         - | 1688 | ` */` |
|     26110 | 1689 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|         5 | 1690 | `{` |
|     26115 | 1691 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|     26115 | 1692 | `	sxu32 nIdx = 0;` |
|         - | 1693 | `	SyString sName;` |
|         - | 1694 | `	sxi32 rc;` |
|         - | 1695 | `	/* Jump the 'unset' keyword */` |
|     26115 | 1696 | `	pGen->pIn++;` |
|         - | 1697 | `	/* Save delimiter */` |
|     26115 | 1698 | `	pTmp = pGen->pEnd;` |
|         - | 1699 | `	/* Skip optional opening parenthesis and find the matching close */` |
|     26115 | 1700 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|     26115 | 1701 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 1702 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|         - | 1703 | `		SyToken *pClose;` |
|     26115 | 1704 | `		pGen->pIn++;   /* Skip '(' */` |
|     26115 | 1705 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|     26115 | 1706 | `		pEnd = pClose; /* Stop at ')' */` |
|     13055 | 1707 | `	}` |
|     26115 | 1708 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|         - | 1709 | `	/* Resolve the 'unset' builtin name once */` |
|     26115 | 1710 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|      3885 | 1711 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3885 | 1712 | `		if( pObj == 0 ){` |
|       ! 0 | 1713 | `			return SXERR_ABORT;` |
|         - | 1714 | `		}` |
|      3885 | 1715 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      3885 | 1716 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      1940 | 1717 | `	}` |
|         - | 1718 | `	/* Compile each comma-separated argument */` |
|     56105 | 1719 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|     29995 | 1720 | `		if( pGen->pIn < pNext ){` |
|         - | 1721 | `			/*` |
|         - | 1722 | ``			 * A PLAIN variable (`unset($x)`, exactly two tokens: '$' and the name) drops a`` |
|         - | 1723 | `			 * single NAME binding, which the unset() builtin cannot express — all it ever` |
|         - | 1724 | `			 * receives is the value's slot index, and unsetting the slot destroys whatever` |
|         - | 1725 | `			 * else aliases it. Emit OP_UNSET_VAR with the name instead. Subscripts and` |
|         - | 1726 | ``			 * properties (`unset($a[k])`, `unset($o->p)`) keep the existing path, which`` |
|         - | 1727 | `			 * already removes just the element/property.` |
|         - | 1728 | `			 */` |
|     29990 | 1729 | `			if( &pGen->pIn[2] == pNext` |
|     18291 | 1730 | `				&& (pGen->pIn[0].nType & PH7_TK_DOLLAR)` |
|      6597 | 1731 | `				&& (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         - | 1732 | `				SyString *pVarName;` |
|      9890 | 1733 | `				char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      6590 | 1734 | `					pGen->pIn[1].sData.zString,pGen->pIn[1].sData.nByte);` |
|      6595 | 1735 | `				pVarName = (SyString *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(SyString));` |
|      6595 | 1736 | `				if( zDup == 0 \|\| pVarName == 0 ){` |
|       ! 0 | 1737 | `					PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - | 1738 | `						"Fatal, PH7 is running out of memory");` |
|       ! 0 | 1739 | `					return SXERR_ABORT;` |
|         - | 1740 | `				}` |
|      6595 | 1741 | `				SyStringInitFromBuf(pVarName,zDup,pGen->pIn[1].sData.nByte);` |
|      6595 | 1742 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_UNSET_VAR,0,0,pVarName,0);` |
|      6595 | 1743 | `				pGen->pIn = pNext;` |
|      6595 | 1744 | `				if( pGen->pIn < pEnd ){` |
|      3881 | 1745 | `					pGen->pIn++; /* Jump the trailing comma */` |
|      1938 | 1746 | `				}` |
|      6595 | 1747 | `				continue;` |
|         - | 1748 | `			}` |
|     23405 | 1749 | `			pGen->pEnd = pNext;` |
|     23405 | 1750 | `			rc = PH7_CompileExpr(&(*pGen),` |
|         - | 1751 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|         - | 1752 | `				GenStateUnsetValidator);` |
|     23405 | 1753 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 1754 | `				return SXERR_ABORT;` |
|         - | 1755 | `			}` |
|     23405 | 1756 | `			if( rc != SXERR_EMPTY ){` |
|         - | 1757 | `				/* Emit call for this single argument */` |
|     23403 | 1758 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     23403 | 1759 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|     23403 | 1760 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     11699 | 1761 | `			}` |
|     11700 | 1762 | `		}` |
|         - | 1763 | `		/* Jump trailing commas */` |
|     23411 | 1764 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|         7 | 1765 | `			pNext++;` |
|         1 | 1766 | `		}` |
|     23405 | 1767 | `		pGen->pIn = pNext;` |
|         5 | 1768 | `	}` |
|         - | 1769 | `	/* Skip past the closing ')' if present */` |
|     26115 | 1770 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|     26115 | 1771 | `		pGen->pIn++;` |
|     13055 | 1772 | `	}` |
|         - | 1773 | `	/* Restore token stream */` |
|     26115 | 1774 | `	pGen->pEnd = pTmp;` |
|     26115 | 1775 | `	return SXRET_OK;` |
|     13060 | 1776 | `}` |
|         - | 1777 | `/*` |
|         - | 1778 | ` * PHP Language construct table.` |
|         - | 1779 | ` */` |
|         - | 1780 | `static const LangConstruct aLangConstruct[] = {` |
|         - | 1781 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|         - | 1782 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|         - | 1783 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|         - | 1784 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|         - | 1785 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|         - | 1786 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|         - | 1787 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|         - | 1788 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|         - | 1789 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|         - | 1790 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|         - | 1791 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|         - | 1792 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|         - | 1793 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|         - | 1794 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|         - | 1795 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|         - | 1796 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|         - | 1797 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|         - | 1798 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|         - | 1799 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|         - | 1800 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|         - | 1801 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|         - | 1802 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|         - | 1803 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|         - | 1804 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|         - | 1805 | `};` |
|         - | 1806 | `/*` |
|         - | 1807 | ` * Return a pointer to the statement handler routine associated` |
|         - | 1808 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|         - | 1809 | ` */` |
|   8452276 | 1810 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|         - | 1811 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|         - | 1812 | `	SyToken *pLookahed  /* Look-ahead token */` |
|         - | 1813 | `	)` |
|         5 | 1814 | `{` |
|   8452281 | 1815 | `	sxu32 n = 0;` |
|  34133938 | 1816 | `	for(;;){` |
|  68267881 | 1817 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|    534091 | 1818 | `			break;` |
|         - | 1819 | `		}` |
|  67733795 | 1820 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|   7918195 | 1821 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|       ! 0 | 1822 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|       ! 0 | 1823 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|         - | 1824 | `					/* 'static' (class context),return null */` |
|       ! 0 | 1825 | `					return 0;` |
|         - | 1826 | `				}` |
|       ! 0 | 1827 | `			}` |
|   7918190 | 1828 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|     11654 | 1829 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|      5834 | 1830 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|         - | 1831 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|         3 | 1832 | `				return 0;` |
|         - | 1833 | `			}` |
|         - | 1834 | `			/* Return a pointer to the handler.` |
|         - | 1835 | `			*/` |
|   7918193 | 1836 | `			return aLangConstruct[n].xConstruct;` |
|         - | 1837 | `		}` |
|  59815605 | 1838 | `		n++;` |
|         5 | 1839 | `	}` |
|    534091 | 1840 | `	if( pLookahed ){` |
|    534091 | 1841 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|     69969 | 1842 | `			return PH7_CompileClassInterface;` |
|    464127 | 1843 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|    397583 | 1844 | `			return PH7_CompileClass;` |
|     66549 | 1845 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      7865 | 1846 | `			return PH7_CompileTrait;` |
|         - | 1847 | `		}` |
|         - | 1848 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|         - | 1849 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|         - | 1850 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|         - | 1851 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     29342 | 1852 | `	}` |
|         - | 1853 | `	/* Not a language construct */` |
|     58689 | 1854 | `	return 0;` |
|   4226143 | 1855 | `}` |
|         - | 1856 | `/*` |
|         - | 1857 | ` * Check if the given keyword is in fact a PHP language construct.` |
|         - | 1858 | ` * Return TRUE on success. FALSE otheriwse.` |
|         - | 1859 | ` */` |
|     58686 | 1860 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|         5 | 1861 | `{` |
|         - | 1862 | `	int rc;` |
|     58691 | 1863 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     58691 | 1864 | `	if( rc == FALSE ){` |
|     58584 | 1865 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|     15890 | 1866 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|         - | 1867 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|         - | 1868 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|         - | 1869 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|         - | 1870 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|         - | 1871 | `			*/` |
|         - | 1872 | `			){` |
|     58581 | 1873 | `				rc = TRUE;` |
|     29288 | 1874 | `		}` |
|     29292 | 1875 | `	}` |
|     58691 | 1876 | `	return rc;` |
|         5 | 1877 | `}` |
|         - | 1878 | `/*` |
|         - | 1879 | ` * Compile a PHP chunk.` |
|         - | 1880 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 1881 | ` * takes care of generating the appropriate error message.` |
|         - | 1882 | ` */` |
|         - | 1883 | `/*` |
|         - | 1884 | ` * Update pGen->sPendingDoc for the statement whose first token is` |
|         - | 1885 | ` * pGen->pIn: when a docblock trivia is keyed to that token's index in` |
|         - | 1886 | ` * the chunk token set it becomes the pending docblock. An existing` |
|         - | 1887 | ` * pending docblock is LEFT in place otherwise: Zend keeps the last-seen` |
|         - | 1888 | ` * doc comment until a declaration consumes it, so a docblock survives` |
|         - | 1889 | ` * intervening non-declaration statements.` |
|         - | 1890 | ` */` |
|  18139260 | 1891 | `PH7_PRIVATE void GenStateSetPendingDoc(ph7_gen_state *pGen)` |
|         5 | 1892 | `{` |
|  18139265 | 1893 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|  18139265 | 1894 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|  18139265 | 1895 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 1896 | `	sxu32 nIdx, n;` |
|  18139260 | 1897 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|   3421091 | 1898 | `	 \|\| pGen->pIn < pBase \|\| pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|         - | 1899 | `		/* Re-tokenized substream (string interpolation, synthesized code):` |
|         - | 1900 | `		 * indexes do not map to the sidecar */` |
|  14718181 | 1901 | `		return;` |
|         - | 1902 | `	}` |
|   3421089 | 1903 | `	nIdx = (sxu32)(pGen->pIn - pBase);` |
|         - | 1904 | `	/* Attributes must be adjacent to their declaration (unlike docblocks):` |
|         - | 1905 | `	 * reset at every boundary, then collect the groups keyed to this token. */` |
|   3421089 | 1906 | `	SySetReset(&pGen->aPendingAttrs);` |
|  10265017 | 1907 | `	for( n = 0 ; n < nT ; n++ ){` |
|   6843933 | 1908 | `		if( aT[n].nTokIdx != nIdx ){` |
|   6836001 | 1909 | `			continue;` |
|         - | 1910 | `		}` |
|      7937 | 1911 | `		if( aT[n].iKind == PH7_TRIVIA_DOC ){` |
|        29 | 1912 | `			pGen->sPendingDoc = aT[n].sText;` |
|      7925 | 1913 | `		}else if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|      7913 | 1914 | `			SySetPut(&pGen->aPendingAttrs,(const void *)&aT[n]);` |
|      3954 | 1915 | `		}` |
|      3971 | 1916 | `	}` |
|   9069635 | 1917 | `}` |
|         - | 1918 | `/*` |
|         - | 1919 | ` * Hand the pending docblock (if any) to a declaration: duplicate it into` |
|         - | 1920 | ` * the VM allocator (the raw script buffer dies after compilation) and` |
|         - | 1921 | ` * clear the pending slot so sibling declarations do not inherit it.` |
|         - | 1922 | ` */` |
|   4880676 | 1923 | `PH7_PRIVATE void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut)` |
|         5 | 1924 | `{` |
|         - | 1925 | `	char *zDup;` |
|   4880681 | 1926 | `	if( SyStringLength(&pGen->sPendingDoc) < 1 ){` |
|   4880661 | 1927 | `		return;` |
|         - | 1928 | `	}` |
|        35 | 1929 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        10 | 1930 | `		SyStringData(&pGen->sPendingDoc),SyStringLength(&pGen->sPendingDoc));` |
|        25 | 1931 | `	if( zDup ){` |
|        25 | 1932 | `		SyStringInitFromBuf(pOut,zDup,SyStringLength(&pGen->sPendingDoc));` |
|        10 | 1933 | `	}` |
|        25 | 1934 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|   2440343 | 1935 | `}` |
|         - | 1936 | `/*` |
|         - | 1937 | ` * Compile one recorded #[...] attribute group (the span between the group` |
|         - | 1938 | ` * delimiters) into ph7_attribute records appended to pOut. The span is` |
|         - | 1939 | ` * duplicated into the VM allocator FIRST (compiled bytecode and interned` |
|         - | 1940 | ` * names may point into the token text, which must outlive the raw script` |
|         - | 1941 | ` * buffer), then re-tokenized on its own. Each argument expression compiles` |
|         - | 1942 | ` * with the container-swap idiom into its own OP_DONE-terminated set,` |
|         - | 1943 | ` * evaluated lazily at ReflectionAttribute time (PHP semantics).` |
|         - | 1944 | ` */` |
|      7922 | 1945 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut)` |
|         5 | 1946 | `{` |
|         - | 1947 | `	SySet *pToken;` |
|         - | 1948 | `	SyToken *pIn, *pEnd, *pSavedIn, *pSavedEnd;` |
|         - | 1949 | `	char *zSpan;` |
|      7927 | 1950 | `	sxi32 rc = SXRET_OK;` |
|      7927 | 1951 | `	if( SyStringLength(&pTrivia->sText) < 1 ){` |
|       ! 0 | 1952 | `		return SXRET_OK;` |
|         - | 1953 | `	}` |
|     11888 | 1954 | `	zSpan = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      3961 | 1955 | `		SyStringData(&pTrivia->sText),SyStringLength(&pTrivia->sText));` |
|      7927 | 1956 | `	if( zSpan == 0 ){` |
|       ! 0 | 1957 | `		return SXRET_OK;` |
|         - | 1958 | `	}` |
|         - | 1959 | `	/* The token set must outlive compilation too: interned operands may` |
|         - | 1960 | `	 * reference token payloads. Pool-allocated, never released — bounded by` |
|         - | 1961 | `	 * the number of attribute declarations in the program. */` |
|      7927 | 1962 | `	pToken = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      7927 | 1963 | `	if( pToken == 0 ){` |
|       ! 0 | 1964 | `		return SXRET_OK;` |
|         - | 1965 | `	}` |
|      7927 | 1966 | `	SySetInit(pToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      7927 | 1967 | `	PH7_TokenizePHP(zSpan,SyStringLength(&pTrivia->sText),pTrivia->nLine,pToken,0);` |
|      7927 | 1968 | `	pIn = (SyToken *)SySetBasePtr(pToken);` |
|      7927 | 1969 | `	pEnd = &pIn[SySetUsed(pToken)];` |
|      7927 | 1970 | `	pSavedIn = pGen->pIn;` |
|      7927 | 1971 | `	pSavedEnd = pGen->pEnd;` |
|      7931 | 1972 | `	while( pIn < pEnd ){` |
|         - | 1973 | `		ph7_attribute sAttr;` |
|         - | 1974 | `		SyBlob sFQN;` |
|      7931 | 1975 | `		int bAbsolute = 0;` |
|      7931 | 1976 | `		SyZero(&sAttr,sizeof(sAttr));` |
|      7931 | 1977 | `		SySetInit(&sAttr.aArgs,&pGen->pVm->sAllocator,sizeof(ph7_attr_arg));` |
|      7931 | 1978 | `		sAttr.nLine = pIn->nLine;` |
|      7931 | 1979 | `		if( pIn->nType & PH7_TK_NSSEP ){` |
|        75 | 1980 | `			bAbsolute = 1;` |
|        75 | 1981 | `			pIn++;` |
|        35 | 1982 | `		}` |
|      7931 | 1983 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      7931 | 1984 | `		while( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      7931 | 1985 | `			SyBlobAppend(&sFQN,pIn->sData.zString,pIn->sData.nByte);` |
|      7931 | 1986 | `			pIn++;` |
|      7931 | 1987 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       ! 0 | 1988 | `				SyBlobAppend(&sFQN,"\\",1);` |
|       ! 0 | 1989 | `				pIn++;` |
|       ! 0 | 1990 | `				continue;` |
|         - | 1991 | `			}` |
|      7931 | 1992 | `			break;` |
|       ! 0 | 1993 | `		}` |
|      7931 | 1994 | `		if( SyBlobLength(&sFQN) < 1 ){` |
|         - | 1995 | `			/* Malformed group: stop quietly (the group was inert trivia before` |
|         - | 1996 | `			 * this feature; never turn it into a new fatal) */` |
|       ! 0 | 1997 | `			SyBlobRelease(&sFQN);` |
|       ! 0 | 1998 | `			break;` |
|         - | 1999 | `		}` |
|         - | 2000 | `		/* Resolve to an FQN: absolute names verbatim; else use-import alias,` |
|         - | 2001 | `		 * else current-namespace prefix (PHP attribute name resolution) */` |
|         - | 2002 | `		{` |
|      7931 | 2003 | `			const char *zName = (const char *)SyBlobData(&sFQN);` |
|      7931 | 2004 | `			sxu32 nName = SyBlobLength(&sFQN);` |
|      7931 | 2005 | `			char *zDup = 0;` |
|      7931 | 2006 | `			if( !bAbsolute ){` |
|      7861 | 2007 | `				SyHashEntry *pImp = SyHashGet(&pGen->hUseImports,(const void *)zName,nName);` |
|      7861 | 2008 | `				if( pImp ){` |
|       ! 0 | 2009 | `					const char *zFqn = (const char *)pImp->pUserData;` |
|       ! 0 | 2010 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFqn,SyStrlen(zFqn));` |
|       ! 0 | 2011 | `					if( zDup ){` |
|       ! 0 | 2012 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyStrlen(zDup));` |
|       ! 0 | 2013 | `					}` |
|      7861 | 2014 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         - | 2015 | `					SyBlob sTmp;` |
|       ! 0 | 2016 | `					SyBlobInit(&sTmp,&pGen->pVm->sAllocator);` |
|       ! 0 | 2017 | `					SyBlobAppend(&sTmp,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       ! 0 | 2018 | `					SyBlobAppend(&sTmp,"\\",1);` |
|       ! 0 | 2019 | `					SyBlobAppend(&sTmp,zName,nName);` |
|       ! 0 | 2020 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       ! 0 | 2021 | `						(const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|       ! 0 | 2022 | `					if( zDup ){` |
|       ! 0 | 2023 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyBlobLength(&sTmp));` |
|       ! 0 | 2024 | `					}` |
|       ! 0 | 2025 | `					SyBlobRelease(&sTmp);` |
|       ! 0 | 2026 | `				}` |
|      3928 | 2027 | `			}` |
|      7931 | 2028 | `			if( SyStringLength(&sAttr.sName) < 1 ){` |
|      7931 | 2029 | `				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|      7931 | 2030 | `				if( zDup ){` |
|      7931 | 2031 | `					SyStringInitFromBuf(&sAttr.sName,zDup,nName);` |
|      3963 | 2032 | `				}` |
|      3963 | 2033 | `			}` |
|         - | 2034 | `		}` |
|      7931 | 2035 | `		SyBlobRelease(&sFQN);` |
|      7931 | 2036 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 2037 | `			SyToken *pArgsEnd;` |
|      7829 | 2038 | `			pIn++;` |
|      7829 | 2039 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pArgsEnd);` |
|     15667 | 2040 | `			while( pIn < pArgsEnd ){` |
|      7843 | 2041 | `				SyToken *pArgStart = pIn, *pArgStop = pIn;` |
|      7843 | 2042 | `				sxi32 iDepth = 0;` |
|         - | 2043 | `				ph7_attr_arg sArgRec;` |
|     77877 | 2044 | `				while( pArgStop < pArgsEnd ){` |
|     70055 | 2045 | `					if( pArgStop->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        11 | 2046 | `						iDepth++;` |
|     70050 | 2047 | `					}else if( pArgStop->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        11 | 2048 | `						iDepth--;` |
|     70040 | 2049 | `					}else if( (pArgStop->nType & PH7_TK_COMMA) && iDepth == 0 ){` |
|        17 | 2050 | `						break;` |
|         - | 2051 | `					}` |
|     70039 | 2052 | `					pArgStop++;` |
|         5 | 2053 | `				}` |
|      7843 | 2054 | `				SyZero(&sArgRec,sizeof(sArgRec));` |
|      7843 | 2055 | `				SySetInit(&sArgRec.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      7838 | 2056 | `				if( pArgStart < pArgStop && (pArgStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|      7820 | 2057 | `				 && &pArgStart[1] < pArgStop && (pArgStart[1].nType & PH7_TK_COLON) ){` |
|        28 | 2058 | `					char *zN = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|         9 | 2059 | `						pArgStart->sData.zString,pArgStart->sData.nByte);` |
|        19 | 2060 | `					if( zN ){` |
|        19 | 2061 | `						SyStringInitFromBuf(&sArgRec.sName,zN,pArgStart->sData.nByte);` |
|         9 | 2062 | `					}` |
|        19 | 2063 | `					pArgStart += 2;` |
|         9 | 2064 | `				}` |
|      7843 | 2065 | `				if( pArgStart < pArgStop ){` |
|         - | 2066 | `					SySet *pInstrContainer;` |
|      7843 | 2067 | `					pGen->pIn = pArgStart;` |
|      7843 | 2068 | `					pGen->pEnd = pArgStop;` |
|      7843 | 2069 | `					pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      7843 | 2070 | `					PH7_VmSetByteCodeContainer(pGen->pVm,&sArgRec.aByteCode);` |
|      7843 | 2071 | `					rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      7843 | 2072 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      7843 | 2073 | `					PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      7843 | 2074 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 2075 | `						pGen->pIn = pSavedIn;` |
|       ! 0 | 2076 | `						pGen->pEnd = pSavedEnd;` |
|       ! 0 | 2077 | `						return SXERR_ABORT;` |
|         - | 2078 | `					}` |
|      7843 | 2079 | `					SySetPut(&sAttr.aArgs,(const void *)&sArgRec);` |
|      3919 | 2080 | `				}` |
|      7843 | 2081 | `				pIn = pArgStop;` |
|      7843 | 2082 | `				if( pIn < pArgsEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        17 | 2083 | `					pIn++;` |
|         8 | 2084 | `				}` |
|         5 | 2085 | `			}` |
|      7829 | 2086 | `			pIn = (pArgsEnd < pEnd) ? &pArgsEnd[1] : pEnd;` |
|      3912 | 2087 | `		}` |
|      7931 | 2088 | `		SySetPut(pOut,(const void *)&sAttr);` |
|      7931 | 2089 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|         5 | 2090 | `			pIn++;` |
|         5 | 2091 | `			continue;` |
|         - | 2092 | `		}` |
|      7927 | 2093 | `		break;` |
|       ! 0 | 2094 | `	}` |
|      7927 | 2095 | `	pGen->pIn = pSavedIn;` |
|      7927 | 2096 | `	pGen->pEnd = pSavedEnd;` |
|      7927 | 2097 | `	return SXRET_OK;` |
|      3966 | 2098 | `}` |
|         - | 2099 | `/*` |
|         - | 2100 | ` * Hand the pending attribute groups (if any) to a declaration: compile` |
|         - | 2101 | ` * every recorded group into pOut and clear the pending list.` |
|         - | 2102 | ` */` |
|   4880680 | 2103 | `PH7_PRIVATE sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut)` |
|         5 | 2104 | `{` |
|   4880685 | 2105 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|         - | 2106 | `	sxu32 n;` |
|         - | 2107 | `	sxi32 rc;` |
|   4888593 | 2108 | `	for( n = 0 ; n < SySetUsed(&pGen->aPendingAttrs) ; n++ ){` |
|      7913 | 2109 | `		rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|      7913 | 2110 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 2111 | `			return SXERR_ABORT;` |
|         - | 2112 | `		}` |
|      3959 | 2113 | `	}` |
|   4880685 | 2114 | `	SySetReset(&pGen->aPendingAttrs);` |
|   4880685 | 2115 | `	return SXRET_OK;` |
|   2440345 | 2116 | `}` |
|         - | 2117 | `/*` |
|         - | 2118 | ` * Compile the attribute groups keyed to the given token (a parameter's` |
|         - | 2119 | ` * first token inside a signature) into pOut. Parameters are parsed from` |
|         - | 2120 | ` * the main token stream, so the sidecar indexes map directly.` |
|         - | 2121 | ` */` |
|   2455948 | 2122 | `PH7_PRIVATE sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut)` |
|         5 | 2123 | `{` |
|   2455953 | 2124 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|   2455953 | 2125 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|   2455953 | 2126 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 2127 | `	sxu32 nIdx, n;` |
|         - | 2128 | `	sxi32 rc;` |
|   2455948 | 2129 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|    555141 | 2130 | `	 \|\| pTok < pBase \|\| pTok >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|   1900817 | 2131 | `		return SXRET_OK;` |
|         - | 2132 | `	}` |
|    555141 | 2133 | `	nIdx = (sxu32)(pTok - pBase);` |
|   1665457 | 2134 | `	for( n = 0 ; n < nT ; n++ ){` |
|   1110321 | 2135 | `		if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|        16 | 2136 | `			rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|        16 | 2137 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 2138 | `				return SXERR_ABORT;` |
|         - | 2139 | `			}` |
|         7 | 2140 | `		}` |
|    555163 | 2141 | `	}` |
|    555141 | 2142 | `	return SXRET_OK;` |
|   1227979 | 2143 | `}` |
|  13308868 | 2144 | `PH7_PRIVATE sxi32 GenStateCompileChunk(` |
|         - | 2145 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 2146 | `	sxi32 iFlags         /* Compile flags */` |
|         - | 2147 | `	)` |
|         5 | 2148 | `{` |
|         - | 2149 | `	ProcLangConstruct xCons;` |
|         - | 2150 | `	sxi32 rc;` |
|  13308873 | 2151 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|   7735654 | 2152 | `	for(;;){` |
|  14390093 | 2153 | `		int bStmtIsDeclare = 0;` |
|  14390093 | 2154 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 2155 | `			/* No more input to process */` |
|     91425 | 2156 | `			break;` |
|         - | 2157 | `		}` |
|         - | 2158 | `		/* Bind a directly-preceding docblock to this statement */` |
|  14298673 | 2159 | `		GenStateSetPendingDoc(&(*pGen));` |
|  14298673 | 2160 | `		if( SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|         - | 2161 | `			/* php: a statement-position attribute group must be followed by a` |
|         - | 2162 | ``			 * declaration (function/class-like/const) — `#[A] $x = 1;` is a`` |
|         - | 2163 | `` 			 * parse error, never a silent discard. `static`/`fn`/`function` `` |
|         - | 2164 | ``			 * cover bare closure-expression statements; `readonly`/`enum` are`` |
|         - | 2165 | `			 * context-sensitive IDs handled by the modified-class/enum scans. */` |
|      7825 | 2166 | `			int bAttrTarget = 0;` |
|      7820 | 2167 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd)` |
|      3945 | 2168 | `			 \|\| GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|      7765 | 2169 | `				bAttrTarget = 1;` |
|      3941 | 2170 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|        61 | 2171 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        60 | 2172 | `				if( nKw == PH7_TKWRD_FUNCTION \|\| nKw == PH7_TKWRD_CLASS` |
|        16 | 2173 | `				 \|\| nKw == PH7_TKWRD_INTERFACE \|\| nKw == PH7_TKWRD_TRAIT` |
|         4 | 2174 | `				 \|\| nKw == PH7_TKWRD_ABSTRACT \|\| nKw == PH7_TKWRD_FINAL` |
|         4 | 2175 | `				 \|\| nKw == PH7_TKWRD_CONST \|\| nKw == PH7_TKWRD_STATIC` |
|         1 | 2176 | `				 \|\| nKw == PH7_TKWRD_FN ){` |
|        61 | 2177 | `					bAttrTarget = 1;` |
|        30 | 2178 | `				}` |
|        30 | 2179 | `			}` |
|      7825 | 2180 | `			if( !bAttrTarget ){` |
|       ! 0 | 2181 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 2182 | `					"syntax error, unexpected token \"%z\" after attribute group; expecting a declaration",` |
|       ! 0 | 2183 | `					&pGen->pIn->sData);` |
|       ! 0 | 2184 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 2185 | `					break;` |
|         - | 2186 | `				}` |
|       ! 0 | 2187 | `				SySetReset(&pGen->aPendingAttrs);` |
|       ! 0 | 2188 | `			}` |
|      3910 | 2189 | `		}` |
|         - | 2190 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|         - | 2191 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  14298673 | 2192 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   8495031 | 2193 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   8495031 | 2194 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|        49 | 2195 | `				bStmtIsDeclare = 1;` |
|        22 | 2196 | `			}` |
|   4247513 | 2197 | `		}` |
|  14298673 | 2198 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|         - | 2199 | `			/* Any non-declare top-level statement locks the strict_types` |
|         - | 2200 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|   1081211 | 2201 | `			pGen->bStrictTypesLocked = 1;` |
|    540603 | 2202 | `		}` |
|  14298673 | 2203 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 2204 | `			/* Compile block */` |
|      3925 | 2205 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      3925 | 2206 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 2207 | `				break;` |
|         - | 2208 | `			}` |
|      1965 | 2209 | `		}else{` |
|  14294753 | 2210 | `			xCons = 0;` |
|  14294753 | 2211 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|         - | 2212 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|         - | 2213 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|         - | 2214 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|     42781 | 2215 | `				xCons = PH7_CompileClassModifiers;` |
|  14273365 | 2216 | `			}else if( GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|         - | 2217 | ``				/* `enum Name …` (PHP 8.1) — `enum` is a context-sensitive ID,`` |
|         - | 2218 | `				 * so it is detected here rather than the keyword dispatcher. */` |
|      3921 | 2219 | `				xCons = PH7_CompileEnum;` |
|  14250019 | 2220 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   8452281 | 2221 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         - | 2222 | `				/* Try to extract a language construct handler */` |
|   8452281 | 2223 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|   8452281 | 2224 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|        13 | 2225 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 2226 | `						"Syntax error: Unexpected keyword '%z'",` |
|         8 | 2227 | `						&pGen->pIn->sData);` |
|         9 | 2228 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 2229 | `						break;` |
|         - | 2230 | `					}` |
|         - | 2231 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|         - | 2232 | `					 * this erroneous statement.` |
|         - | 2233 | `					 */` |
|         9 | 2234 | `					xCons = PH7_ErrorRecover;` |
|         4 | 2235 | `				}` |
|  10021923 | 2236 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|    428007 | 2237 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|         - | 2238 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|       117 | 2239 | `				xCons = PH7_CompileLabel;` |
|        56 | 2240 | `			}` |
|  14294753 | 2241 | `			if( xCons == 0 ){` |
|         - | 2242 | `				/* Assume an expression an try to compile it */` |
|   5854351 | 2243 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   5854351 | 2244 | `				if(  rc != SXERR_EMPTY ){` |
|         - | 2245 | `					/* Pop l-value */` |
|   5854201 | 2246 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|   2927098 | 2247 | `				}` |
|   2927178 | 2248 | `			}else{` |
|         - | 2249 | `				/* Go compile the sucker */` |
|   8440407 | 2250 | `				rc = xCons(&(*pGen));` |
|         - | 2251 | `			}` |
|  14294753 | 2252 | `			if( rc == SXERR_ABORT ){` |
|         - | 2253 | `				/* Request to abort compilation */` |
|        34 | 2254 | `				break;` |
|         - | 2255 | `			}` |
|         - | 2256 | `		}` |
|         - | 2257 | `		/* Ignore trailing semi-colons ';' */` |
|  24553633 | 2258 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  10254995 | 2259 | `			pGen->pIn++;` |
|         5 | 2260 | `		}` |
|  14298643 | 2261 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|         - | 2262 | `			/* Compile a single statement and return */` |
|  13217423 | 2263 | `			break;` |
|         - | 2264 | `		}` |
|         - | 2265 | `		/* LOOP ONE */` |
|         - | 2266 | `		/* LOOP TWO */` |
|         - | 2267 | `		/* LOOP THREE */` |
|         - | 2268 | `		/* LOOP FOUR */` |
|         5 | 2269 | `	}` |
|         - | 2270 | `	/* Return compilation status */` |
|  13308873 | 2271 | `	return rc;` |
|         5 | 2272 | `}` |
|         - | 2273 | `/*` |
|         - | 2274 | ` * Compile a Raw PHP chunk.` |
|         - | 2275 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 2276 | ` * takes care of generating the appropriate error message.` |
|         - | 2277 | ` */` |
|     91452 | 2278 | `static sxi32 PH7_CompilePHP(` |
|         - | 2279 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 2280 | `	SySet *pTokenSet,     /* Token set */` |
|         - | 2281 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|         - | 2282 | `	)` |
|         5 | 2283 | `{` |
|     91457 | 2284 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|         - | 2285 | `	sxi32 rc;` |
|         - | 2286 | `	/* Reset the token set (and its trivia sidecar) */` |
|     91457 | 2287 | `	SySetReset(&(*pTokenSet));` |
|     91457 | 2288 | `	SySetReset(&pGen->aTrivia);` |
|         - | 2289 | `	/* Mark as the default token set */` |
|     91457 | 2290 | `	pGen->pTokenSet = &(*pTokenSet);` |
|         - | 2291 | `	/* Advance the stream cursor */` |
|     91457 | 2292 | `	pGen->pRawIn++;` |
|         - | 2293 | `	/* Tokenize the PHP chunk first */` |
|     91457 | 2294 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet),&pGen->aTrivia);` |
|         - | 2295 | `	/* Point to the head and tail of the token stream. */` |
|     91457 | 2296 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     91457 | 2297 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|     91457 | 2298 | `	if( is_expr ){` |
|       ! 0 | 2299 | `		rc = SXERR_EMPTY;` |
|       ! 0 | 2300 | `		if( pGen->pIn < pGen->pEnd ){` |
|         - | 2301 | `			/* A simple expression,compile it */` |
|       ! 0 | 2302 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|       ! 0 | 2303 | `		}` |
|         - | 2304 | `		/* Emit the DONE instruction */` |
|       ! 0 | 2305 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       ! 0 | 2306 | `		return SXRET_OK;` |
|         - | 2307 | `	}` |
|     91457 | 2308 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|         - | 2309 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|         - | 2310 | `		/*` |
|         - | 2311 | `		 * Shortcut syntax for the 'echo' language construct.` |
|         - | 2312 | `		 * According to the PHP reference manual:` |
|         - | 2313 | `		 *  echo() also has a shortcut syntax, where you can` |
|         - | 2314 | `		 *  immediately follow` |
|         - | 2315 | `		 *  the opening tag with an equals sign as follows:` |
|         - | 2316 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|         - | 2317 | `		 * Symisc extension:` |
|         - | 2318 | `		 *   This short syntax works with all PHP opening` |
|         - | 2319 | `		 *   tags unlike the default PHP engine that handle` |
|         - | 2320 | `		 *   only short tag.` |
|         - | 2321 | `		 */` |
|         - | 2322 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|         3 | 2323 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|         3 | 2324 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|         3 | 2325 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|         3 | 2326 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|         3 | 2327 | `		if( rc != SXERR_EMPTY ){` |
|         3 | 2328 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         1 | 2329 | `		}` |
|         3 | 2330 | `		return SXRET_OK;` |
|         - | 2331 | `	}` |
|         - | 2332 | `	/* Compile the PHP chunk */` |
|     91455 | 2333 | `	rc = GenStateCompileChunk(pGen,0);` |
|         - | 2334 | `	/* Fix exceptions jumps */` |
|     91455 | 2335 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - | 2336 | `	/* Fix gotos now, the jump destination is resolved */` |
|     91455 | 2337 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|         3 | 2338 | `		rc = SXERR_ABORT;` |
|         1 | 2339 | `	}` |
|         - | 2340 | `	/* Reset container */` |
|     91455 | 2341 | `	SySetReset(&pGen->aGoto);` |
|     91455 | 2342 | `	SySetReset(&pGen->aLabel);` |
|     91455 | 2343 | `	SySetReset(&pGen->aNullsafeJmp);` |
|         - | 2344 | `	/* Compilation result */` |
|     91455 | 2345 | `	return rc;` |
|     45731 | 2346 | `}` |
|         - | 2347 | `/*` |
|         - | 2348 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|         - | 2349 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|         - | 2350 | ` * This is the only compile interface exported from this file.` |
|         - | 2351 | ` */` |
|     94598 | 2352 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|         - | 2353 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|         - | 2354 | `	SyString *pScript,  /* Script to compile */` |
|         - | 2355 | `	sxi32 iFlags        /* Compile flags */` |
|         - | 2356 | `	)` |
|         5 | 2357 | `{` |
|         - | 2358 | `	SySet aPhpToken,aRawToken;` |
|         - | 2359 | `	ph7_gen_state *pCodeGen;` |
|         - | 2360 | `	ph7_value *pRawObj;` |
|         - | 2361 | `	sxu32 nObjIdx;` |
|         - | 2362 | `	sxi32 nRawObj;` |
|         - | 2363 | `	int is_expr;` |
|         - | 2364 | `	sxi8 bSavedStrict;` |
|         - | 2365 | `	sxi8 bSavedStrictLocked;` |
|         - | 2366 | `	SyToken *pSavedIn,*pSavedEnd;` |
|         - | 2367 | `	sxi32 rc;` |
|     94603 | 2368 | `	sxu32 nBaseLine = 1;` |
|     94603 | 2369 | `	if( pScript->nByte < 1 ){` |
|         - | 2370 | `		/* Nothing to compile */` |
|       ! 0 | 2371 | `		return PH7_OK;` |
|         - | 2372 | `	}` |
|         - | 2373 | `	/* php skips a "#!" shebang on the first line of a CLI script: consume it` |
|         - | 2374 | `	 * (including its newline) so it is not echoed as inline text, and bump the` |
|         - | 2375 | `	 * base line to 2 so the code below still reports php-matching line numbers. */` |
|     94603 | 2376 | `	if( pScript->nByte >= 2 && pScript->zString[0]=='#' && pScript->zString[1]=='!' ){` |
|         3 | 2377 | `		const char *z = pScript->zString;` |
|         3 | 2378 | `		const char *zEnd = &z[pScript->nByte];` |
|        39 | 2379 | `		while( z < zEnd && z[0] != '\n' ){ z++; }` |
|         3 | 2380 | `		if( z < zEnd ){ z++; } /* consume the newline too */` |
|         3 | 2381 | `		pScript->nByte -= (sxu32)(z - pScript->zString);` |
|         3 | 2382 | `		pScript->zString = z;` |
|         3 | 2383 | `		nBaseLine = 2;` |
|         3 | 2384 | `		if( pScript->nByte < 1 ){` |
|       ! 0 | 2385 | `			return PH7_OK;` |
|         - | 2386 | `		}` |
|         1 | 2387 | `	}` |
|         - | 2388 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|         - | 2389 | `	 * file's flags so include/require restore them on return. */` |
|     94603 | 2390 | `	pCodeGen = &pVm->sCodeGen;` |
|         - | 2391 | ``	/* aPhpToken below is a LOCAL token set: once it is released at `cleanup`, any`` |
|         - | 2392 | `	 * pGen->pIn/pEnd still pointing into it DANGLE. PH7_VmEmitInstr reads pIn to stamp` |
|         - | 2393 | `	 * each instruction's source line, and instructions are still emitted after this` |
|         - | 2394 | `	 * function returns (VmEvalChunk's trailing OP_DONE) -- a use-after-free ASan caught` |
|         - | 2395 | `	 * immediately. Save the caller's cursor and restore it on the way out, so an` |
|         - | 2396 | `	 * enclosing compile keeps its (live) tokens and the top level goes back to NULL. */` |
|     94603 | 2397 | `	pSavedIn = pCodeGen->pIn;` |
|     94603 | 2398 | `	pSavedEnd = pCodeGen->pEnd;` |
|     94603 | 2399 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|     94603 | 2400 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|     94603 | 2401 | `	pCodeGen->bStrictTypes = 0;` |
|     94603 | 2402 | `	pCodeGen->bStrictTypesLocked = 0;` |
|         - | 2403 | `	/* Initialize the tokens containers */` |
|     94603 | 2404 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|     94603 | 2405 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|     94603 | 2406 | `	SySetAlloc(&aPhpToken,0xc0);` |
|     94603 | 2407 | `	is_expr = 0;` |
|     94603 | 2408 | `	if( iFlags & PH7_PHP_ONLY ){` |
|         - | 2409 | `		SyToken sTmp;` |
|         - | 2410 | `		/* PHP only: -*/` |
|     81605 | 2411 | `		sTmp.nLine = 1;` |
|     81605 | 2412 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|     81605 | 2413 | `		sTmp.pUserData = 0;` |
|     81605 | 2414 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|     81605 | 2415 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|     81605 | 2416 | `		if( iFlags & PH7_PHP_EXPR ){` |
|         - | 2417 | `			/* A simple PHP expression */` |
|       ! 0 | 2418 | `			is_expr = 1;` |
|       ! 0 | 2419 | `		}` |
|     40805 | 2420 | `	}else{` |
|         - | 2421 | `		/* Tokenize raw text */` |
|     13003 | 2422 | `		SySetAlloc(&aRawToken,32);` |
|     13003 | 2423 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken,nBaseLine);` |
|         - | 2424 | `	}` |
|         - | 2425 | `	/* Process high-level tokens */` |
|     94603 | 2426 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|     94603 | 2427 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|     94603 | 2428 | `	rc = PH7_OK;` |
|     94603 | 2429 | `	if( is_expr ){` |
|         - | 2430 | `		/* Compile the expression */` |
|       ! 0 | 2431 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|       ! 0 | 2432 | `		goto cleanup;` |
|         - | 2433 | `	}` |
|     94603 | 2434 | `	nObjIdx = 0;` |
|         - | 2435 | `	/* Each compilation unit starts in the global namespace.` |
|         - | 2436 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|         - | 2437 | `	 * preventing namespace bleeding across include()d files. */` |
|     94603 | 2438 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|         - | 2439 | `	/* Start the compilation process */` |
|     53803 | 2440 | `	for(;;){` |
|    199031 | 2441 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|     94571 | 2442 | `			break; /* No more tokens to process */` |
|         - | 2443 | `		}` |
|    104465 | 2444 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|         - | 2445 | `			/* Compile the PHP chunk */` |
|     91457 | 2446 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|     91457 | 2447 | `			if( rc == SXERR_ABORT ){` |
|        36 | 2448 | `				break;` |
|         - | 2449 | `			}` |
|     91425 | 2450 | `			continue;` |
|         - | 2451 | `		}` |
|         - | 2452 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|     13013 | 2453 | `		nRawObj = 0;` |
|     26021 | 2454 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|         - | 2455 | `			/* Consume the raw chunk without any processing */` |
|     13013 | 2456 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|     13013 | 2457 | `			if( pRawObj == 0 ){` |
|       ! 0 | 2458 | `				rc = SXERR_MEM;` |
|       ! 0 | 2459 | `				break;` |
|         - | 2460 | `			}` |
|         - | 2461 | `			/* Mark as constant and emit the load constant instruction */` |
|     13013 | 2462 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|     13013 | 2463 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     13013 | 2464 | `			++nRawObj;` |
|     13013 | 2465 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|         5 | 2466 | `		}` |
|     13013 | 2467 | `		if( nRawObj > 0 ){` |
|         - | 2468 | `			/* Emit the consume instruction */` |
|     13013 | 2469 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|      6504 | 2470 | `		}` |
|     47304 | 2471 | `	}` |
|     47299 | 2472 | `cleanup:` |
|         - | 2473 | `	/* Drop the cursor into the token set BEFORE the set is freed (see above). */` |
|     94603 | 2474 | `	pCodeGen->pIn = pSavedIn;` |
|     94603 | 2475 | `	pCodeGen->pEnd = pSavedEnd;` |
|     94603 | 2476 | `	SySetRelease(&aRawToken);` |
|     94603 | 2477 | `	SySetRelease(&aPhpToken);` |
|         - | 2478 | `	/* Restore outer file's strict_types scope */` |
|     94603 | 2479 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|     94603 | 2480 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|     94603 | 2481 | `	return rc;` |
|     47304 | 2482 | `}` |
|         - | 2483 | `/*` |
|         - | 2484 | ` * Utility routines.Initialize the code generator.` |
|         - | 2485 | ` */` |
|      3880 | 2486 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|         - | 2487 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 2488 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 2489 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 2490 | `	)` |
|         5 | 2491 | `{` |
|      3885 | 2492 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 2493 | `	/* Zero the structure */` |
|      3885 | 2494 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|         - | 2495 | `	/* Initial state */` |
|      3885 | 2496 | `	pGen->pVm  = &(*pVm);` |
|      3885 | 2497 | `	pGen->xErr = xErr;` |
|      3885 | 2498 | `	pGen->pErrData = pErrData;` |
|      3885 | 2499 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|      3885 | 2500 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|      3885 | 2501 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|      3885 | 2502 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|      3885 | 2503 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|      3885 | 2504 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3885 | 2505 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3885 | 2506 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|      3885 | 2507 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|         - | 2508 | `	/* Error log buffer */` |
|      3885 | 2509 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|         - | 2510 | `	/* General purpose working buffer */` |
|      3885 | 2511 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|         - | 2512 | `	/* Namespace state */` |
|      3885 | 2513 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|      3885 | 2514 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|      3885 | 2515 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|      3885 | 2516 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 2517 | `	/* Create the global scope */` |
|      3885 | 2518 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|         - | 2519 | `	/* Point to the global scope */` |
|      3885 | 2520 | `	pGen->pCurrent = &pGen->sGlobal;` |
|      3885 | 2521 | `	return SXRET_OK;` |
|         5 | 2522 | `}` |
|         - | 2523 | `/*` |
|         - | 2524 | ` * Utility routines. Reset the code generator to it's initial state.` |
|         - | 2525 | ` */` |
|     98006 | 2526 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|         - | 2527 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 2528 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 2529 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 2530 | `	)` |
|         5 | 2531 | `{` |
|     98011 | 2532 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 2533 | `	GenBlock *pBlock,*pParent;` |
|         - | 2534 | `	/* Reset state */` |
|     98011 | 2535 | `	SySetReset(&pGen->aLabel);` |
|     98011 | 2536 | `	SySetReset(&pGen->aGoto);` |
|     98011 | 2537 | `	SySetReset(&pGen->aNullsafeJmp);` |
|     98011 | 2538 | `	SySetReset(&pGen->aTrivia);` |
|     98011 | 2539 | `	SySetReset(&pGen->aPendingAttrs);` |
|     98011 | 2540 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|     98011 | 2541 | `	SyBlobRelease(&pGen->sErrBuf);` |
|     98011 | 2542 | `	SyBlobRelease(&pGen->sWorker);` |
|     98011 | 2543 | `	SyBlobRelease(&pGen->sNamespace);` |
|     98011 | 2544 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|     98011 | 2545 | `	SyHashRelease(&pGen->hUseImports);` |
|     98011 | 2546 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|     98011 | 2547 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     98011 | 2548 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|     98011 | 2549 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     98011 | 2550 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 2551 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|         - | 2552 | `	 * They intern variable names and literal strings that are referenced by` |
|         - | 2553 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|         - | 2554 | `	 * Releasing them would either leak the interned strings or require freeing` |
|         - | 2555 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|         - | 2556 | `	 * number of unique names, which is acceptable. */` |
|         - | 2557 | `	/* Point to the global scope */` |
|     98011 | 2558 | `	pBlock = pGen->pCurrent;` |
|     98011 | 2559 | `	while( pBlock->pParent != 0 ){` |
|       ! 0 | 2560 | `		pParent = pBlock->pParent;` |
|       ! 0 | 2561 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 2562 | `		pBlock = pParent;` |
|       ! 0 | 2563 | `	}` |
|     98011 | 2564 | `	pGen->xErr = xErr;` |
|     98011 | 2565 | `	pGen->pErrData = pErrData;` |
|     98011 | 2566 | `	pGen->pCurrent = &pGen->sGlobal;` |
|     98011 | 2567 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|     98011 | 2568 | `	pGen->pIn = pGen->pEnd = 0;` |
|     98011 | 2569 | `	pGen->nErr = 0;` |
|     98011 | 2570 | `	return SXRET_OK;` |
|         5 | 2571 | `}` |
|         - | 2572 | `/*` |
|         - | 2573 | ` * Save the code generator's compile-position state and hand the live generator a` |
|         - | 2574 | ` * fresh, empty one for a NESTED compilation unit.` |
|         - | 2575 | ` *` |
|         - | 2576 | ` * A require/include normally runs at execution time, when no compile is in flight,` |
|         - | 2577 | ` * so VmEvalChunk can call PH7_ResetCodeGenerator to wipe the generator. But an` |
|         - | 2578 | `` * autoload can fire in the MIDDLE of compiling a class: resolving `class Child`` |
|         - | 2579 | `` * extends Base` calls PH7_VmExtractClass, which triggers the autoloader, whose`` |
|         - | 2580 | `` * body `require`s Base's file — a nested compile while the outer one is mid-token.`` |
|         - | 2581 | ` * PH7_ResetCodeGenerator would then blow away the outer compile's cursor` |
|         - | 2582 | ` * (pIn/pEnd), current block, use-imports, labels, etc.; the outer parse resumes` |
|         - | 2583 | ` * with pIn == NULL and reports a bogus "Expected '{' after class 'Child'". (Common` |
|         - | 2584 | ` * in every real framework: Composer autoloads parent classes on demand.)` |
|         - | 2585 | ` *` |
|         - | 2586 | ` * This snapshots the position/scope fields into *pSaved (a caller-stack` |
|         - | 2587 | ` * ph7_gen_state used purely as storage) and re-initializes the live generator's` |
|         - | 2588 | ` * position containers to FRESH, empty ones WITHOUT releasing the outer's (the` |
|         - | 2589 | ` * snapshot now owns those). hLiteral/hNumLiteral/hVar are intentionally left LIVE` |
|         - | 2590 | ` * and shared — compiled bytecode interns names/literals into them and they grow` |
|         - | 2591 | ` * monotonically (exactly why PH7_ResetCodeGenerator keeps them); restoring an old` |
|         - | 2592 | ` * copy of those headers after a nested grow would use-after-free the bucket array.` |
|         - | 2593 | ` */` |
|         4 | 2594 | `PH7_PRIVATE void PH7_CompilerSaveState(ph7_vm *pVm,ph7_gen_state *pSaved,ProcConsumer xErr,void *pErrData)` |
|         1 | 2595 | `{` |
|         5 | 2596 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 2597 | `	/* Shallow-copy every field; the position containers below are then replaced` |
|         - | 2598 | `	 * with fresh ones on the live struct, so *pSaved keeps the outer's memory. */` |
|         5 | 2599 | `	*pSaved = *pGen;` |
|         5 | 2600 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|         5 | 2601 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|         5 | 2602 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|         5 | 2603 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|         5 | 2604 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|         5 | 2605 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|         5 | 2606 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|         5 | 2607 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|         5 | 2608 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|         5 | 2609 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|         5 | 2610 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|         5 | 2611 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 2612 | `	/* Fresh global scope for the nested unit (address of the embedded sGlobal is` |
|         - | 2613 | `	 * stable, so any outer block still parented to it stays valid across restore). */` |
|         5 | 2614 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(pVm),0);` |
|         5 | 2615 | `	pGen->pCurrent = &pGen->sGlobal;` |
|         5 | 2616 | `	pGen->pIn = pGen->pEnd = 0;` |
|         5 | 2617 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|         5 | 2618 | `	pGen->pTokenSet = 0;` |
|         5 | 2619 | `	pGen->nErr = 0;` |
|         5 | 2620 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|         5 | 2621 | `	pGen->nCommaExprOk = 0;` |
|         5 | 2622 | `	pGen->bInGenerator = 0;` |
|         5 | 2623 | `	pGen->bStrictTypes = 0;` |
|         5 | 2624 | `	pGen->bStrictTypesLocked = 0;` |
|         5 | 2625 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|         5 | 2626 | `	pGen->xErr = xErr;` |
|         5 | 2627 | `	pGen->pErrData = pErrData;` |
|         5 | 2628 | `}` |
|         - | 2629 | `/*` |
|         - | 2630 | ` * Restore the outer compile-position state saved by PH7_CompilerSaveState,` |
|         - | 2631 | ` * releasing the nested unit's position containers first. The shared` |
|         - | 2632 | ` * hLiteral/hNumLiteral/hVar tables (grown by the nested compile) are carried` |
|         - | 2633 | ` * forward, NOT rolled back to the snapshot's stale headers.` |
|         - | 2634 | ` */` |
|         4 | 2635 | `PH7_PRIVATE void PH7_CompilerRestoreState(ph7_vm *pVm,ph7_gen_state *pSaved)` |
|         1 | 2636 | `{` |
|         5 | 2637 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 2638 | `	GenBlock *pBlock,*pParent;` |
|         - | 2639 | `	SyHash hVar,hLiteral,hNumLiteral;` |
|         - | 2640 | `	/* Free any nested blocks left open (e.g. an aborted nested compile), then the` |
|         - | 2641 | `	 * nested global block's own fixup sets. */` |
|         5 | 2642 | `	pBlock = pGen->pCurrent;` |
|         5 | 2643 | `	while( pBlock && pBlock->pParent != 0 ){` |
|       ! 0 | 2644 | `		pParent = pBlock->pParent;` |
|       ! 0 | 2645 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 2646 | `		pBlock = pParent;` |
|       ! 0 | 2647 | `	}` |
|         5 | 2648 | `	GenStateReleaseBlock(&pGen->sGlobal);` |
|         - | 2649 | `	/* Release the nested unit's position containers. */` |
|         5 | 2650 | `	SySetRelease(&pGen->aLabel);` |
|         5 | 2651 | `	SySetRelease(&pGen->aGoto);` |
|         5 | 2652 | `	SySetRelease(&pGen->aNullsafeJmp);` |
|         5 | 2653 | `	SySetRelease(&pGen->aLoopParent);` |
|         5 | 2654 | `	SySetRelease(&pGen->aTrivia);` |
|         5 | 2655 | `	SySetRelease(&pGen->aPendingAttrs);` |
|         5 | 2656 | `	SyBlobRelease(&pGen->sWorker);` |
|         5 | 2657 | `	SyBlobRelease(&pGen->sErrBuf);` |
|         5 | 2658 | `	SyBlobRelease(&pGen->sNamespace);` |
|         5 | 2659 | `	SyHashRelease(&pGen->hUseImports);` |
|         5 | 2660 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|         5 | 2661 | `	SyHashRelease(&pGen->hUseConstImports);` |
|         - | 2662 | `	/* Preserve the (possibly grown) shared intern tables across the restore. */` |
|         5 | 2663 | `	hVar = pGen->hVar;` |
|         5 | 2664 | `	hLiteral = pGen->hLiteral;` |
|         5 | 2665 | `	hNumLiteral = pGen->hNumLiteral;` |
|         5 | 2666 | `	*pGen = *pSaved;` |
|         5 | 2667 | `	pGen->hVar = hVar;` |
|         5 | 2668 | `	pGen->hLiteral = hLiteral;` |
|         5 | 2669 | `	pGen->hNumLiteral = hNumLiteral;` |
|         5 | 2670 | `}` |
|         - | 2671 | `/*` |
|         - | 2672 | ` * Raise php's parse error for an unexpected token: E_PARSE with the exact text` |
|         - | 2673 | ` * php's parser prints, e.g.` |
|         - | 2674 | ` *` |
|         - | 2675 | ` *   syntax error, unexpected token ";", expecting "{"` |
|         - | 2676 | ` *   syntax error, unexpected identifier "invalid", expecting "("` |
|         - | 2677 | ` *   syntax error, unexpected end of file` |
|         - | 2678 | ` *` |
|         - | 2679 | ` * php names the token by CLASS, not just by text: an identifier, a variable and` |
|         - | 2680 | ` * a number each get their own noun, while everything else (keywords, operators,` |
|         - | 2681 | ` * punctuation) is a "token". zExpecting is the optional ", expecting ..." tail` |
|         - | 2682 | ` * — pass NULL when the site cannot say what it wanted (php often can't either).` |
|         - | 2683 | ` * pTok == NULL means the input ran out: "unexpected end of file".` |
|         - | 2684 | ` *` |
|         - | 2685 | ` * PHL's hand-written recursive-descent parser has no bison expectation sets, so` |
|         - | 2686 | ` * a site can only claim an "expecting" clause it genuinely knows; every clause` |
|         - | 2687 | ` * emitted here was verified against php 8.5.7 for the construct in question.` |
|         - | 2688 | ` */` |
|       178 | 2689 | `PH7_PRIVATE sxi32 PH7_GenSyntaxError(` |
|         - | 2690 | `	ph7_gen_state *pGen,   /* Code generator state */` |
|         - | 2691 | `	SyToken *pTok,         /* Offending token, or NULL for end of file */` |
|         - | 2692 | `	const char *zExpecting /* ", expecting <this>" tail, or NULL */` |
|         - | 2693 | `	)` |
|         5 | 2694 | `{` |
|       183 | 2695 | `	const char *zNoun = "token";` |
|         - | 2696 | `	sxu32 nLine;` |
|       183 | 2697 | `	if( pTok == 0 && pGen->pTokenSet ){` |
|         - | 2698 | `		/* The caller ran out of tokens inside its own slice — but a statement's slice stops` |
|         - | 2699 | `		 * BEFORE its terminator, so the token php actually names (typically the ';') is the` |
|         - | 2700 | `		 * one sitting just past the slice, still inside the chunk's token stream. Reach for` |
|         - | 2701 | `		 * it before concluding "end of file". */` |
|        90 | 2702 | `		SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        90 | 2703 | `		SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|        90 | 2704 | `		if( pGen->pEnd >= pBase && pGen->pEnd < pStreamEnd ){` |
|        90 | 2705 | `			pTok = pGen->pEnd;` |
|        43 | 2706 | `		}` |
|        43 | 2707 | `	}` |
|       183 | 2708 | `	nLine = pTok ? pTok->nLine : (pGen->pIn > (SyToken *)SySetBasePtr(pGen->pTokenSet) ? pGen->pIn[-1].nLine : 1);` |
|       183 | 2709 | `	if( pTok == 0 ){` |
|       ! 0 | 2710 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|       ! 0 | 2711 | `			zExpecting ? "syntax error, unexpected end of file, expecting %s"` |
|         - | 2712 | `			           : "syntax error, unexpected end of file",` |
|       ! 0 | 2713 | `			zExpecting);` |
|         - | 2714 | `	}` |
|       183 | 2715 | `	if( pTok->nType & PH7_TK_ID ){` |
|        15 | 2716 | `		zNoun = "identifier";` |
|       176 | 2717 | `	}else if( pTok->nType & PH7_TK_DOLLAR ){` |
|         6 | 2718 | `		zNoun = "variable";` |
|       168 | 2719 | `	}else if( pTok->nType & PH7_TK_INTEGER ){` |
|        24 | 2720 | `		zNoun = "integer";` |
|       156 | 2721 | `	}else if( pTok->nType & PH7_TK_REAL ){` |
|       ! 0 | 2722 | `		zNoun = "float";` |
|       ! 0 | 2723 | `	}` |
|       183 | 2724 | `	if( zExpecting ){` |
|       118 | 2725 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        38 | 2726 | `			"syntax error, unexpected %s \"%z\", expecting %s",zNoun,&pTok->sData,zExpecting);` |
|         - | 2727 | `	}` |
|       158 | 2728 | `	return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        51 | 2729 | `		"syntax error, unexpected %s \"%z\"",zNoun,&pTok->sData);` |
|        94 | 2730 | `}` |
|         - | 2731 | `/*` |
|         - | 2732 | ` * Generate a compile-time error message.` |
|         - | 2733 | ` * If the error count limit is reached (usually 15 error message)` |
|         - | 2734 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|         - | 2735 | ` * abort compilation immediately.` |
|         - | 2736 | ` */` |
|       672 | 2737 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|         5 | 2738 | `{` |
|       677 | 2739 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|       677 | 2740 | `	const char *zErr = "Error";` |
|         - | 2741 | `	SyString *pFile;` |
|         - | 2742 | `	va_list ap;` |
|         - | 2743 | `	sxi32 rc;` |
|         - | 2744 | `	/* Reset the working buffer */` |
|       677 | 2745 | `	SyBlobReset(pWorker);` |
|         - | 2746 | `	/* Peek the processed file path if available */` |
|       677 | 2747 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|       677 | 2748 | `	if( nErrType == E_ERROR \|\| nErrType == E_PARSE ){` |
|         - | 2749 | `		/* Increment the error counter. A PARSE error is every bit as fatal as an` |
|         - | 2750 | `		 * E_ERROR one: php compiles nothing, runs nothing and exits 255. Counting` |
|         - | 2751 | `		 * only E_ERROR let a parse error print its diagnostic and then fall through` |
|         - | 2752 | `		 * into execution with a 0 exit status. */` |
|       669 | 2753 | `		pGen->nErr++;` |
|       669 | 2754 | `		if( pGen->nErr > 15 ){` |
|         - | 2755 | `			/* Error count limit reached */` |
|         6 | 2756 | `			if( pGen->xErr ){` |
|         6 | 2757 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|         6 | 2758 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|         6 | 2759 | `				if( pFile ){` |
|         6 | 2760 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|         2 | 2761 | `				}` |
|         6 | 2762 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|         6 | 2763 | `				if( SyBlobLength(pWorker) > 0 ){` |
|         6 | 2764 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|         2 | 2765 | `				}` |
|         2 | 2766 | `			}` |
|         - | 2767 | `			/* Abort immediately */` |
|         6 | 2768 | `			return SXERR_ABORT;` |
|         - | 2769 | `		}` |
|       330 | 2770 | `	}` |
|       673 | 2771 | `	if( pGen->xErr == 0 ){` |
|         - | 2772 | `		/* No available error consumer,return immediately */` |
|         3 | 2773 | `		return SXRET_OK;` |
|         - | 2774 | `	}` |
|       671 | 2775 | `	switch(nErrType){` |
|       322 | 2776 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|        11 | 2777 | `	case E_WARNING: zErr = "Warning";     break;` |
|       344 | 2778 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       ! 0 | 2779 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|       ! 0 | 2780 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|       ! 0 | 2781 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|       ! 0 | 2782 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|       ! 0 | 2783 | `	case 8192 /* E_DEPRECATED */: zErr = "Deprecated"; break;` |
|       ! 0 | 2784 | `	default:` |
|       ! 0 | 2785 | `		break;` |
|         - | 2786 | `	}` |
|       671 | 2787 | `	rc = SXRET_OK;` |
|         - | 2788 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|       671 | 2789 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|       671 | 2790 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|       671 | 2791 | `	va_start(ap,zFormat);` |
|       671 | 2792 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|       671 | 2793 | `	va_end(ap);` |
|       671 | 2794 | `	if( pFile ){` |
|       671 | 2795 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       333 | 2796 | `	}` |
|         - | 2797 | `	/* Append a new line */` |
|       671 | 2798 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       671 | 2799 | `	if( SyBlobLength(pWorker) > 0 ){` |
|         - | 2800 | `		/* Consume the generated error message */` |
|       671 | 2801 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       333 | 2802 | `	}` |
|       671 | 2803 | `	return rc;` |
|       341 | 2804 | `}` |
|         - | 2805 |  |
