# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1584/1715 lines (92.36%)

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
|        59 |   39 | `	return SXERR_NOTFOUND;` |
|        79 |   40 | `}` |
|         - |   41 | `/*` |
|         - |   42 | ` * Fetch a block that correspond to the given criteria from the stack of` |
|         - |   43 | ` * compiled blocks.` |
|         - |   44 | ` * Return a pointer to that block on success. NULL otherwise.` |
|         - |   45 | ` */` |
|    194798 |   46 | `PH7_PRIVATE GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|         5 |   47 | `{` |
|    194803 |   48 | `	GenBlock *pBlock = pCurrent;` |
|    393554 |   49 | `	for(;;){` |
|    787113 |   50 | `		if( pBlock->iFlags & iBlockType ){` |
|    194779 |   51 | `			iCount--; /* Decrement nesting level */` |
|    194779 |   52 | `			if( iCount < 1 ){` |
|         - |   53 | `				/* Block meet with the desired criteria */` |
|    194753 |   54 | `				return pBlock;` |
|         - |   55 | `			}` |
|        13 |   56 | `		}` |
|         - |   57 | `		/* Point to the upper block */` |
|    592365 |   58 | `		pBlock = pBlock->pParent;` |
|    592365 |   59 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|         - |   60 | `			/* Forbidden */` |
|        29 |   61 | `			break;` |
|         - |   62 | `		}` |
|         5 |   63 | `	}` |
|         - |   64 | `	/* No such block */` |
|        54 |   65 | `	return 0;` |
|     97404 |   66 | `}` |
|         - |   67 | `/*` |
|         - |   68 | ` * Initialize a freshly allocated block instance.` |
|         - |   69 | ` */` |
|  14005884 |   70 | `static void GenStateInitBlock(` |
|         - |   71 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |   72 | `	GenBlock *pBlock,    /* Target block */` |
|         - |   73 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   74 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|         - |   75 | `	void *pUserData      /* Upper layer private data */` |
|         - |   76 | `	)` |
|         5 |   77 | `{` |
|         - |   78 | `	/* Initialize block fields */` |
|  14005889 |   79 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  14005889 |   80 | `	pBlock->pUserData   = pUserData;` |
|  14005889 |   81 | `	pBlock->pGen        = pGen;` |
|  14005889 |   82 | `	pBlock->iFlags      = iType;` |
|  14005889 |   83 | `	pBlock->pParent     = 0;` |
|  14005889 |   84 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  14005889 |   85 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  14005889 |   86 | `}` |
|         - |   87 | `/*` |
|         - |   88 | ` * Allocate a new block instance.` |
|         - |   89 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|         - |   90 | ` * on success.Otherwise generate a compile-time error and abort` |
|         - |   91 | ` * processing on failure.` |
|         - |   92 | ` */` |
|  14001740 |   93 | `PH7_PRIVATE sxi32 GenStateEnterBlock(` |
|         - |   94 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |   95 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   96 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|         - |   97 | `	void *pUserData,      /* Upper layer private data */` |
|         - |   98 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|         - |   99 | `	)` |
|         5 |  100 | `{` |
|         - |  101 | `	GenBlock *pBlock;` |
|         - |  102 | `	/* Allocate a new block instance */` |
|  14001745 |  103 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  14001745 |  104 | `	if( pBlock == 0 ){` |
|         - |  105 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |  106 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - |  107 | `		 */` |
|       ! 0 |  108 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|         - |  109 | `		/* Abort processing immediately */` |
|       ! 0 |  110 | `		return SXERR_ABORT;` |
|         - |  111 | `	}` |
|         - |  112 | `	/* Zero the structure */` |
|  14001745 |  113 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  14001745 |  114 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|         - |  115 | `	/* Link to the parent block */` |
|  14001745 |  116 | `	pBlock->pParent = pGen->pCurrent;` |
|         - |  117 | `	/* A loop or switch gets an id, and remembers the loop it nests inside, so a goto's` |
|         - |  118 | `	 * and a label's positions can be compared after compilation (see aLoopParent). */` |
|  14001745 |  119 | `	if( iType & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    572647 |  120 | `		sxu32 nParent = pGen->nCurLoopId;` |
|    572647 |  121 | `		pGen->nLoopId++;` |
|    572647 |  122 | `		SySetPut(&pGen->aLoopParent,(const void *)&nParent);` |
|    572647 |  123 | `		pBlock->nLoopId = pGen->nLoopId;` |
|    572647 |  124 | `		pBlock->nOuterLoopId = nParent;` |
|    572647 |  125 | `		pGen->nCurLoopId = pGen->nLoopId;` |
|    286321 |  126 | `	}` |
|         - |  127 | `	/* Mark as the current block */` |
|  14001745 |  128 | `	pGen->pCurrent = pBlock;` |
|  14001745 |  129 | `	if( ppBlock ){` |
|         - |  130 | `		/* Write a pointer to the new instance */` |
|   6732403 |  131 | `		*ppBlock = pBlock;` |
|   3366199 |  132 | `	}` |
|  14001745 |  133 | `	return SXRET_OK;` |
|   7000875 |  134 | `}` |
|         - |  135 | `/*` |
|         - |  136 | ` * Release block fields without freeing the whole instance.` |
|         - |  137 | ` */` |
|  14001734 |  138 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|         5 |  139 | `{` |
|  14001739 |  140 | `	SySetRelease(&pBlock->aPostContFix);` |
|  14001739 |  141 | `	SySetRelease(&pBlock->aJumpFix);` |
|  14001739 |  142 | `}` |
|         - |  143 | `/*` |
|         - |  144 | ` * Release a block.` |
|         - |  145 | ` */` |
|  14001730 |  146 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|         5 |  147 | `{` |
|  14001735 |  148 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  14001735 |  149 | `	GenStateReleaseBlock(&(*pBlock));` |
|         - |  150 | `	/* Free the instance */` |
|  14001735 |  151 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  14001735 |  152 | `}` |
|         - |  153 | `/*` |
|         - |  154 | ` * POP and release a block from the stack of compiled blocks.` |
|         - |  155 | ` */` |
|  14001730 |  156 | `PH7_PRIVATE sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|         5 |  157 | `{` |
|  14001735 |  158 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  14001735 |  159 | `	if( pBlock == 0 ){` |
|         - |  160 | `		/* No more block to pop */` |
|       ! 0 |  161 | `		return SXERR_EMPTY;` |
|         - |  162 | `	}` |
|  14001735 |  163 | `	if( pBlock->iFlags & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    572639 |  164 | `		pGen->nCurLoopId = pBlock->nOuterLoopId;` |
|    286317 |  165 | `	}` |
|         - |  166 | `	/* Point to the upper block */` |
|  14001735 |  167 | `	pGen->pCurrent = pBlock->pParent;` |
|  14001735 |  168 | `	if( ppBlock ){` |
|         - |  169 | `		/* Write a pointer to the popped block */` |
|       ! 0 |  170 | `		*ppBlock = pBlock;` |
|       ! 0 |  171 | `	}else{` |
|         - |  172 | `		/* Safely release the block */` |
|  14001735 |  173 | `		GenStateFreeBlock(&(*pBlock));` |
|         - |  174 | `	}` |
|  14001735 |  175 | `	return SXRET_OK;` |
|   7000870 |  176 | `}` |
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
|   1121970 |  195 | `PH7_PRIVATE int GenStateUnconditionalTopLevel(ph7_gen_state *pGen)` |
|         5 |  196 | `{` |
|   1121975 |  197 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   1126243 |  198 | `	while( pBlock ){` |
|   1126243 |  199 | `		if( pBlock->iFlags & (GEN_BLOCK_COND\|GEN_BLOCK_LOOP\|GEN_BLOCK_FUNC\|GEN_BLOCK_SWITCH\|GEN_BLOCK_EXCEPTION) ){` |
|        93 |  200 | `			return 0; /* conditional / nested */` |
|         - |  201 | `		}` |
|   1126155 |  202 | `		if( pBlock->iFlags & GEN_BLOCK_GLOBAL ){` |
|   1121887 |  203 | `			return 1; /* reached the global block with no conditional ancestor */` |
|         - |  204 | `		}` |
|      4273 |  205 | `		pBlock = pBlock->pParent;` |
|         5 |  206 | `	}` |
|       ! 0 |  207 | `	return 1;` |
|    560990 |  208 | `}` |
|         - |  209 | `/*` |
|         - |  210 | ` * Guard a top-level function about to be installed. Same contract as the class` |
|         - |  211 | ` * guard above.` |
|         - |  212 | ` */` |
|    556422 |  213 | `PH7_PRIVATE sxi32 GenStateGuardFuncRedeclaration(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|         5 |  214 | `{` |
|         - |  215 | `	SyHashEntry *pEntry;` |
|    556427 |  216 | `	if( !GenStateUnconditionalTopLevel(pGen) ){` |
|        56 |  217 | `		return SXRET_OK;` |
|         - |  218 | `	}` |
|    556375 |  219 | `	pFunc->iFlags \|= VM_FUNC_BOUND;` |
|    556375 |  220 | `	if( pGen->pVm->bCompilingBuiltin ){` |
|    554765 |  221 | `		return SXRET_OK;` |
|         - |  222 | `	}` |
|         - |  223 | ``	/* NOTE: a userland function shadowing a C builtin (e.g. `function strlen(){}`)`` |
|         - |  224 | `	 * is NOT caught here — the C builtins register in PH7_VmMakeReady, after user` |
|         - |  225 | `	 * code has compiled, so hHostFunction is still empty at this point. Prelude` |
|         - |  226 | `	 * functions (ini_get, ...) and every builtin CLASS compile earlier and ARE` |
|         - |  227 | `	 * guarded. Redeclaring a C builtin function stays a known divergence. */` |
|      1615 |  228 | `	pEntry = SyHashGet(&pGen->pVm->hFunction,(const void *)pFunc->sName.zString,pFunc->sName.nByte);` |
|      1615 |  229 | `	if( pEntry ){` |
|        12 |  230 | `		ph7_vm_func *pPrev = (ph7_vm_func *)pEntry->pUserData;` |
|        14 |  231 | `		while( pPrev ){` |
|        12 |  232 | `			if( pPrev->iFlags & VM_FUNC_BOUND ){` |
|         9 |  233 | `				if( pPrev->sFile.nByte > 0 ){` |
|         8 |  234 | `					PH7_GenCompileError(pGen,E_ERROR,pFunc->nLine,` |
|         - |  235 | `						"Cannot redeclare function %z() (previously declared in %.*s:%u)",` |
|         2 |  236 | `						&pFunc->sName,pPrev->sFile.nByte,pPrev->sFile.zString,pPrev->nLine);` |
|         4 |  237 | `				}else{` |
|         4 |  238 | `					PH7_GenCompileError(pGen,E_ERROR,pFunc->nLine,` |
|         1 |  239 | `						"Cannot redeclare function %z()",&pFunc->sName);` |
|         - |  240 | `				}` |
|         9 |  241 | `				return SXERR_ABORT;` |
|         - |  242 | `			}` |
|         3 |  243 | `			pPrev = pPrev->pNextName;` |
|         1 |  244 | `		}` |
|         1 |  245 | `	}` |
|      1609 |  246 | `	return SXRET_OK;` |
|    278216 |  247 | `}` |
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
|   5062258 |  258 | `PH7_PRIVATE sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|         5 |  259 | `{` |
|         - |  260 | `	JumpFixup sJumpFix;` |
|         - |  261 | `	sxi32 rc;` |
|         - |  262 | `	/* Init the JumpFixup structure */` |
|   5062263 |  263 | `	sJumpFix.nJumpType = nJumpType;` |
|   5062263 |  264 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|         - |  265 | `	/* Insert in the jump fixup table */` |
|   5062263 |  266 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|   5062263 |  267 | `	return rc;` |
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
|   9737468 |  280 | `PH7_PRIVATE sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|         5 |  281 | `{` |
|         - |  282 | `	JumpFixup *aFix;` |
|         - |  283 | `	VmInstr *pInstr;` |
|         - |  284 | `	sxu32 nFixed;` |
|         - |  285 | `	sxu32 n;` |
|         - |  286 | `	/* Point to the jump fixup table */` |
|   9737473 |  287 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|         - |  288 | `	/* Fix the desired jumps */` |
|  20379099 |  289 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  10641631 |  290 | `		if( aFix[n].nJumpType < 0 ){` |
|         - |  291 | `			/* Already fixed */` |
|   3972151 |  292 | `			continue;` |
|         - |  293 | `		}` |
|   6669485 |  294 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|         - |  295 | `			/* Not of our interest */` |
|   1607229 |  296 | `			continue;` |
|         - |  297 | `		}` |
|         - |  298 | `		/* Point to the instruction to fix */` |
|   5062261 |  299 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|   5062261 |  300 | `		if( pInstr ){` |
|   5062261 |  301 | `			pInstr->iP2 = nJumpDest;` |
|   5062261 |  302 | `			nFixed++;` |
|         - |  303 | `			/* Mark as fixed */` |
|   5062261 |  304 | `			aFix[n].nJumpType = -1;` |
|   2531128 |  305 | `		}` |
|   2531133 |  306 | `	}` |
|         - |  307 | `	/* Total number of fixed jumps */` |
|   9737473 |  308 | `	return nFixed;` |
|         5 |  309 | `}` |
|         - |  310 | `/*` |
|         - |  311 | ` * Fix a 'goto' now the jump destination is resolved.` |
|         - |  312 | ` * The goto statement can be used to jump to another section` |
|         - |  313 | ` * in the program.` |
|         - |  314 | ` * Refer to the routine responsible of compiling the goto` |
|         - |  315 | ` * statement for more information.` |
|         - |  316 | ` */` |
|   3595636 |  317 | `PH7_PRIVATE sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|         5 |  318 | `{` |
|         - |  319 | `	JumpFixup *pJump,*aJumps;` |
|         - |  320 | `	Label *pLabel;` |
|         - |  321 | `	VmInstr *pInstr;` |
|         - |  322 | `	sxi32 rc;` |
|         - |  323 | `	sxu32 n;` |
|         - |  324 | `	/* Point to the goto table */` |
|   3595641 |  325 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|         - |  326 | `	/* Fix */` |
|   3595787 |  327 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
|       153 |  328 | `		pJump = &aJumps[n];` |
|         - |  329 | `		/* Extract the target label */` |
|       153 |  330 | `		rc = GenStateGetLabel(&(*pGen),&pJump->sLabel,&pLabel);` |
|       153 |  331 | `		if( rc != SXRET_OK ){` |
|         - |  332 | `			/* No such label */` |
|        59 |  333 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"'goto' to undefined label '%z'",&pJump->sLabel);` |
|        59 |  334 | `			if( rc == SXERR_ABORT ){` |
|         3 |  335 | `				return SXERR_ABORT;` |
|         - |  336 | `			}` |
|        57 |  337 | `			continue;` |
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
|        10 |  365 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"'goto' to undefined label '%z'",&pJump->sLabel);` |
|        10 |  366 | `			if( rc == SXERR_ABORT ){` |
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
|   3595639 |  378 | `	return SXRET_OK;` |
|   1797823 |  379 | `}` |
|         - |  380 | `/*` |
|         - |  381 | ` * Check if a given token value is installed in the literal table.` |
|         - |  382 | ` */` |
|  18144120 |  383 | `PH7_PRIVATE sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|         5 |  384 | `{` |
|         - |  385 | `	SyHashEntry *pEntry;` |
|  18144125 |  386 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  18144125 |  387 | `	if( pEntry == 0 ){` |
|   4852365 |  388 | `		return SXERR_NOTFOUND;` |
|         - |  389 | `	}` |
|  13291765 |  390 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  13291765 |  391 | `	return SXRET_OK;` |
|   9072065 |  392 | `}` |
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
|   4852360 |  403 | `PH7_PRIVATE sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|         5 |  404 | `{` |
|   4852365 |  405 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|   4852365 |  406 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   2426180 |  407 | `	}` |
|   4852365 |  408 | `	return SXRET_OK;` |
|         5 |  409 | `}` |
|         - |  410 | `/*` |
|         - |  411 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|         - |  412 | ` * in the constant table.` |
|         - |  413 | ` */` |
|   4109774 |  414 | `PH7_PRIVATE ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|         5 |  415 | `{` |
|         - |  416 | `	ph7_value *pObj;` |
|   4109779 |  417 | `	sxu32 nIdx = 0; /* cc warning */` |
|         - |  418 | `	/* Reserve a new constant */` |
|   4109779 |  419 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   4109779 |  420 | `	if( pObj == 0 ){` |
|       ! 0 |  421 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  422 | `		return 0;` |
|         - |  423 | `	}` |
|   4109779 |  424 | `	*pIdx = nIdx;` |
|         - |  425 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|         - |  426 | `	 * the constant string iterals table [optimization purposes].` |
|         - |  427 | `	 */` |
|   4109779 |  428 | `	return pObj;` |
|   2054892 |  429 | `}` |
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
|   8530536 |  444 | `PH7_PRIVATE void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|         5 |  445 | `{` |
|         - |  446 | `	VmCallArgMap *pMap;` |
|   8530541 |  447 | `	if( !pGen->bStrictTypes ) return p3;` |
|        80 |  448 | `	if( p3 == 0 ){` |
|        72 |  449 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|        72 |  450 | `		if( pMap == 0 ) return 0;` |
|        72 |  451 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|        72 |  452 | `		p3 = (void *)pMap;` |
|        34 |  453 | `	}` |
|        80 |  454 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|        80 |  455 | `	return p3;` |
|   4265273 |  456 | `}` |
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
|       112 |  476 | `PH7_PRIVATE int GenStateIsReservedConstant(SyString *pName)` |
|         5 |  477 | `{` |
|       117 |  478 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|        14 |  479 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|         3 |  480 | `			return TRUE;` |
|        12 |  481 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|         6 |  482 | `			return TRUE;` |
|         2 |  483 | `		}` |
|       109 |  484 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|        13 |  485 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|         3 |  486 | `			return TRUE;` |
|         - |  487 | `		}` |
|         4 |  488 | `	}` |
|         - |  489 | `	/* Not a reserved constant */` |
|       109 |  490 | `	return FALSE;` |
|        61 |  491 | `}` |
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
|  58034920 |  508 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|         5 |  509 | `{` |
|  58034925 |  510 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|         - |  511 | `	sxu32 nTarget;` |
|         - |  512 | `	sxu32 *aIdx;` |
|         - |  513 | `	sxu32 i;` |
|  58034925 |  514 | `	if( nCur <= nBaseline ){` |
|  58034827 |  515 | `		return;` |
|         - |  516 | `	}` |
|       101 |  517 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|       101 |  518 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|       207 |  519 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|       109 |  520 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|       109 |  521 | `		if( pInstr ){` |
|       109 |  522 | `			pInstr->iP2 = (sxi32)nTarget;` |
|        53 |  523 | `		}` |
|        56 |  524 | `	}` |
|       101 |  525 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
|  29017465 |  526 | `}` |
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
|   7578366 |  545 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|         5 |  546 | `{` |
|         - |  547 | `	static const struct {` |
|         - |  548 | `		const char *zName;` |
|         - |  549 | `		sxu32 nByte;` |
|         - |  550 | `		sxu32 mask;` |
|         - |  551 | `	} aByRef[] = {` |
|         - |  552 | `		{ "parse_str",              9, 1u<<1 },  /* &$result (apArg[1]) */` |
|         - |  553 | `		{ "settype",                7, 1u<<0 },  /* &$var    (apArg[0]) */` |
|         - |  554 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|         - |  555 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|         - |  556 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|         - |  557 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|         - |  558 | `		{ "flock",                  5, 1u<<2 },  /* &$would_block (apArg[2]) */` |
|         - |  559 | `		{ "getopt",                 6, 1u<<2 },  /* &$rest_index (apArg[2]) */` |
|         - |  560 | `		{ "is_callable",           11, 1u<<2 },  /* &$callable_name (apArg[2]) */` |
|         - |  561 | `		{ "similar_text",          12, 1u<<2 },  /* &$percent (apArg[2]) */` |
|         - |  562 | `		{ "str_replace",           11, 1u<<3 },  /* &$count  (apArg[3]) */` |
|         - |  563 | `		{ "str_ireplace",          12, 1u<<3 },  /* &$count  (apArg[3]) */` |
|         - |  564 | `		{ "fsockopen",              9, (1u<<2)\|(1u<<3) },  /* &$error_code, &$error_message */` |
|         - |  565 | `		{ "pfsockopen",            10, (1u<<2)\|(1u<<3) },  /* same */` |
|         - |  566 | `		{ "stream_socket_client",  20, (1u<<1)\|(1u<<2) },  /* &$error_code, &$error_message */` |
|         - |  567 | `		{ "proc_open",              9, 1u<<2 },  /* &$pipes (apArg[2]) */` |
|         - |  568 | `	};` |
|         - |  569 | `	sxu32 i;` |
|   7578371 |  570 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|   1927225 |  571 | `		return 0;` |
|         - |  572 | `	}` |
|  95215881 |  573 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
|  89631456 |  574 | `		if( pName->nByte == aByRef[i].nByte` |
|  47693153 |  575 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|     66731 |  576 | `			return aByRef[i].mask;` |
|         - |  577 | `		}` |
|  44782370 |  578 | `	}` |
|   5584425 |  579 | `	return 0;` |
|   3789188 |  580 | `}` |
|         - |  581 | `/*` |
|         - |  582 | ` * Recover the bare global-builtin name from a call's callee node.` |
|         - |  583 | ` *` |
|         - |  584 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|         - |  585 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|         - |  586 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|         - |  587 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|         - |  588 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|         - |  589 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|         - |  590 | ` */` |
|  13505182 |  591 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|         5 |  592 | `{` |
|         - |  593 | `	SyToken *p, *pEnd;` |
|  13505187 |  594 | `	pOut->zString = 0;` |
|  13505187 |  595 | `	pOut->nByte = 0;` |
|  13505187 |  596 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|       ! 0 |  597 | `		return;` |
|         - |  598 | `	}` |
|  13505187 |  599 | `	p = pLeft->pStart;` |
|  13505187 |  600 | `	pEnd = pLeft->pEnd;` |
|         - |  601 | `	/* Optional single leading namespace separator (absolute path). */` |
|  13505187 |  602 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|      8347 |  603 | `		p++;` |
|      4171 |  604 | `	}` |
|  13505187 |  605 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|   2396791 |  606 | `		return;` |
|         - |  607 | `	}` |
|         - |  608 | `	/* Must be a single component: nothing follows the name token. */` |
|  11108401 |  609 | `	if( p + 1 != pEnd ){` |
|        69 |  610 | `		return;` |
|         - |  611 | `	}` |
|  11108337 |  612 | `	*pOut = p->sData;` |
|   6752596 |  613 | `}` |
|         - |  614 | `/*` |
|         - |  615 | ` * Generate bytecode for a given expression tree.` |
|         - |  616 | ` * If something goes wrong while generating bytecode` |
|         - |  617 | ` * for the expression tree (A very unlikely scenario)` |
|         - |  618 | ` * this function takes care of generating the appropriate` |
|         - |  619 | ` * error message.` |
|         - |  620 | ` */` |
|  81690446 |  621 | `static sxi32 GenStateEmitExprCode(` |
|         - |  622 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |  623 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|         - |  624 | `	sxi32 iFlags /* Control flags */` |
|         - |  625 | `	)` |
|         5 |  626 | `{` |
|         - |  627 | `	VmInstr *pInstr;` |
|         - |  628 | `	sxu32 nJmpIdx;` |
|  81690451 |  629 | `	sxi32 iP1 = 0;` |
|  81690451 |  630 | `	sxu32 iP2 = 0;` |
|  81690451 |  631 | `	void *p3  = 0;` |
|         - |  632 | `	sxi32 iVmOp;` |
|         - |  633 | `	sxi32 rc;` |
|  81690451 |  634 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
|  81690451 |  635 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
|  81690451 |  636 | `	sxu32 nRhsNsBase = 0;` |
|  81690451 |  637 | `	if( pNode->xCode ){` |
|         - |  638 | `		SyToken *pTmpIn,*pTmpEnd;` |
|         - |  639 | `		/* Compile node */` |
|  49284359 |  640 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
|  49284359 |  641 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
|  49284359 |  642 | `		RE_SWAP_DELIMITER(pGen);` |
|  49284359 |  643 | `		return rc;` |
|         - |  644 | `	}` |
|  32406097 |  645 | `	if( pNode->pOp == 0 ){` |
|       ! 0 |  646 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - |  647 | `			"Invalid expression node,PH7 is aborting compilation");` |
|       ! 0 |  648 | `		return SXERR_ABORT;` |
|         - |  649 | `	}` |
|  32406097 |  650 | `	iVmOp = pNode->pOp->iVmOp;` |
|  32406097 |  651 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|         - |  652 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|         - |  653 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|         - |  654 | `		 * and later errors are still reported. */` |
|         3 |  655 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - |  656 | `			"The (unset) cast is no longer supported");` |
|         3 |  657 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  658 | `			return SXERR_ABORT;` |
|         - |  659 | `		}` |
|         1 |  660 | `	}` |
|  32406097 |  661 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|        93 |  662 | `		sxu32 nJmp = 0;` |
|         - |  663 | `		sxu32 nNcNsBase;` |
|         - |  664 | `		VmInstr *pInstrFix;` |
|         - |  665 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|         - |  666 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|         - |  667 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|         - |  668 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|         - |  669 | `		 * stack slot carries a writable nIdx. */` |
|        93 |  670 | `		if( pNode->pRight ){` |
|        93 |  671 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        93 |  672 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|        93 |  673 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  674 | `				return rc;` |
|         - |  675 | `			}` |
|        93 |  676 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|         - |  677 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|         - |  678 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|         - |  679 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|         - |  680 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|         - |  681 | `			 * the store, so the parent array does not need to be copied at` |
|         - |  682 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|         - |  683 | `			 * cascade for the actual write path stays correct. */` |
|        93 |  684 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|        93 |  685 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|        33 |  686 | `				pInstrFix->iP2 = 3;` |
|        15 |  687 | `			}` |
|        45 |  688 | `		}` |
|         - |  689 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|        93 |  690 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|         - |  691 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|        93 |  692 | `		if( pNode->pLeft ){` |
|        93 |  693 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        93 |  694 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|        93 |  695 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  696 | `				return rc;` |
|         - |  697 | `			}` |
|        93 |  698 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|        45 |  699 | `		}` |
|         - |  700 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|        93 |  701 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|         - |  702 | `		/* Patch the short-circuit jump to land after the store. */` |
|        93 |  703 | `		if( nJmp > 0 ){` |
|        93 |  704 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|        93 |  705 | `			if( pInstrFix ){` |
|        93 |  706 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|        45 |  707 | `			}` |
|        45 |  708 | `		}` |
|        93 |  709 | `		return SXRET_OK;` |
|         - |  710 | `	}` |
|  32406007 |  711 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|         - |  712 | `		sxu32 nJz,nJmp;` |
|         - |  713 | `		sxu32 nTernaryNsBase;` |
|         - |  714 | `		/* Ternary operator require special handling */` |
|         - |  715 | `		/* Phase#1: Compile the condition */` |
|    549793 |  716 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    549793 |  717 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    549793 |  718 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  719 | `			return rc;` |
|         - |  720 | `		}` |
|         - |  721 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|         - |  722 | `		 * compiling the condition must short-circuit to the end of the` |
|         - |  723 | `		 * condition expression, not leak past the ternary. */` |
|    549793 |  724 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    549793 |  725 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    549793 |  726 | `		if( pNode->pLeft ){` |
|         - |  727 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|         - |  728 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    545581 |  729 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - |  730 | `			/* Phase#3: Compile the 'then' expression  */` |
|    545581 |  731 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    545581 |  732 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    545581 |  733 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  734 | `				return rc;` |
|         - |  735 | `			}` |
|    545581 |  736 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    272793 |  737 | `		}else{` |
|         - |  738 | `			/* Elvis operator: (expr) ?: (else)` |
|         - |  739 | `			 * Duplicate condition so original value is the 'then' result.` |
|         - |  740 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      4217 |  741 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      4217 |  742 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - |  743 | `		}` |
|         - |  744 | `		/* Phase#4: Emit the unconditional jump */` |
|    549793 |  745 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|         - |  746 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    549793 |  747 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    549793 |  748 | `		if( pInstr ){` |
|    549793 |  749 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    274894 |  750 | `		}` |
|    549793 |  751 | `		if( !pNode->pLeft ){` |
|         - |  752 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      4217 |  753 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      2106 |  754 | `		}` |
|         - |  755 | `		/* Phase#6: Compile the 'else' expression */` |
|    549793 |  756 | `		if( pNode->pRight ){` |
|    549793 |  757 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    549793 |  758 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    549793 |  759 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  760 | `				return rc;` |
|         - |  761 | `			}` |
|    549793 |  762 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    274894 |  763 | `		}` |
|    549793 |  764 | `		if( nJmp > 0 ){` |
|         - |  765 | `			/* Phase#7: Fix the unconditional jump */` |
|    549793 |  766 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    549793 |  767 | `			if( pInstr ){` |
|    549793 |  768 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    274894 |  769 | `			}` |
|    274894 |  770 | `		}` |
|         - |  771 | `		/* All done */` |
|    549793 |  772 | `		return SXRET_OK;` |
|         - |  773 | `	}` |
|  31856219 |  774 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
|         - |  775 | ``		/* PHP 8.5 pipe: `$lhs \|> $rhs` invokes the RHS callable with the LHS`` |
|         - |  776 | ``		 * value as its sole argument [i.e. `$rhs($lhs)`]. Evaluate the LHS (the`` |
|         - |  777 | `		 * argument) first, then the RHS callable, then emit a one-argument` |
|         - |  778 | `		 * OP_CALL — the same stack shape the function-call path builds (the` |
|         - |  779 | `		 * argument sits below the callee). The RHS is any callable expression:` |
|         - |  780 | ``		 * an FCC `f(...)` (an OP_LOAD_FCC Closure), a closure variable, an`` |
|         - |  781 | ``		 * `[obj,method]` pair, or a callable string. */`` |
|         - |  782 | `		sxu32 nPipeNsBase;` |
|        27 |  783 | `		sxi32 iOperandFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE\|EXPR_FLAG_RDONLY_LOAD);` |
|        27 |  784 | `		if( pNode->pLeft == 0 \|\| pNode->pRight == 0 ){` |
|       ! 0 |  785 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - |  786 | `				"'\|>': Missing operand");` |
|       ! 0 |  787 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  788 | `		}` |
|         - |  789 | `		/* Argument: the LHS value. */` |
|        27 |  790 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 |  791 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iOperandFlags);` |
|        27 |  792 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  793 | `			return rc;` |
|         - |  794 | `		}` |
|        27 |  795 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - |  796 | `		/* Callable: the RHS. */` |
|        27 |  797 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 |  798 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iOperandFlags);` |
|        27 |  799 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  800 | `			return rc;` |
|         - |  801 | `		}` |
|        27 |  802 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - |  803 | `		/* Invoke the callable with the single piped argument. */` |
|        27 |  804 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        27 |  805 | `		return SXRET_OK;` |
|         - |  806 | `	}` |
|  31856193 |  807 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|         - |  808 | `	/* Generate code for the left tree */` |
|  31856193 |  809 | `	if( pNode->pLeft ){` |
|  31798445 |  810 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  31798445 |  811 | `		if( iVmOp == PH7_OP_CALL ){` |
|         - |  812 | `			ph7_expr_node **apNode;` |
|   7582917 |  813 | `			int hasSpread = 0;` |
|   7582917 |  814 | `			int hasNamed = 0;` |
|   7582917 |  815 | `			int bAnySpread = 0;` |
|   7582917 |  816 | `			sxu32 byRefMask = 0;` |
|         - |  817 | `			sxi32 nArgs;` |
|         - |  818 | `			sxi32 n;` |
|         - |  819 | `			/* Recurse and generate bytecodes for function arguments */` |
|   7582917 |  820 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   7582917 |  821 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|         - |  822 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|         - |  823 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|         - |  824 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|   7582917 |  825 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|        95 |  826 | `				bFcc = 1;` |
|        95 |  827 | `				nArgs = 0;` |
|        46 |  828 | `			}` |
|         - |  829 | `			/* Validate argument order like php: no positional argument after a` |
|         - |  830 | ``			 * named one OR after unpacking, and `name: ...$x` is a parse error. */`` |
|         - |  831 | `			{` |
|   7582917 |  832 | `				int seenNamed = 0;` |
|   7582917 |  833 | `				int seenSpread = 0;` |
|  16196125 |  834 | `				for( n = 0; n < nArgs; ++n ){` |
|   8613215 |  835 | `					if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|      4345 |  836 | `						bAnySpread = 1;` |
|      4345 |  837 | `						seenSpread = 1;` |
|      4345 |  838 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       ! 0 |  839 | `							rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - |  840 | `								"syntax error, unexpected token \"...\"");` |
|       ! 0 |  841 | `							return SXERR_SYNTAX;` |
|         5 |  842 | `						}` |
|   8611045 |  843 | `					}else if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       374 |  844 | `						seenNamed = 1;` |
|       374 |  845 | `						hasNamed = 1;` |
|   8608690 |  846 | `					}else if( seenNamed ){` |
|         3 |  847 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - |  848 | `							"Cannot use positional argument after named argument");` |
|         3 |  849 | `						return SXERR_SYNTAX;` |
|   8608503 |  850 | `					}else if( seenSpread ){` |
|       ! 0 |  851 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - |  852 | `							"Cannot use positional argument after argument unpacking");` |
|       ! 0 |  853 | `						return SXERR_SYNTAX;` |
|         - |  854 | `					}` |
|   4306609 |  855 | `				}` |
|         - |  856 | `			}` |
|         - |  857 | `			/* Read-only load */` |
|   7582915 |  858 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|         - |  859 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|         - |  860 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|         - |  861 | `			 * objects dispatch to the right method (offsetExists for both;` |
|         - |  862 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|   7582915 |  863 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|   7582915 |  864 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|  11794117 |  865 | `				int bIsset = pCallName->nByte == 5` |
|   7582910 |  866 | `					&& SyStrnicmp(pCallName->zString,"isset",5) == 0;` |
|  11794117 |  867 | `				int bEmpty = pCallName->nByte == 5` |
|   7582910 |  868 | `					&& SyStrnicmp(pCallName->zString,"empty",5) == 0;` |
|         - |  869 | `				/* isset()/empty() are language CONSTRUCTS, not functions: php parses` |
|         - |  870 | `				 * their argument list in the grammar and a missing operand is a parse` |
|         - |  871 | `				 * error on the ')'. They compile through this ordinary call loop, which` |
|         - |  872 | ``				 * never checked arity, so `empty()` quietly evaluated to true and`` |
|         - |  873 | ``				 * `isset()` to false. (empty() also takes exactly one operand in php,`` |
|         - |  874 | `				 * unlike isset(), which is variadic.) */` |
|   7582915 |  875 | `				if( (bIsset \|\| bEmpty) && nArgs < 1 ){` |
|         - |  876 | `					/* php names the ')' itself as the unexpected token, so point at the` |
|         - |  877 | `					 * node's last token rather than pGen->pIn (which has already moved` |
|         - |  878 | `					 * past the call to the statement's ';'). */` |
|         5 |  879 | `					SyToken *pTok = pNode->pEnd;` |
|         5 |  880 | `					if( pTok && pTok > pNode->pStart && (pTok->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 |  881 | `						pTok--;` |
|       ! 0 |  882 | `					}` |
|         5 |  883 | `					PH7_GenSyntaxError(&(*pGen),pTok,0);` |
|         5 |  884 | `					return SXERR_ABORT;` |
|         - |  885 | `				}` |
|   7582911 |  886 | `				if( bIsset ){` |
|    348229 |  887 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|   7408799 |  888 | `				}else if( bEmpty ){` |
|       135 |  889 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|        65 |  890 | `				}` |
|         - |  891 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|         - |  892 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|         - |  893 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|         - |  894 | `				 * write back through. Skipped when spread/named args are present:` |
|         - |  895 | `				 * the compile-time positional index no longer maps to the` |
|         - |  896 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|   7582911 |  897 | `				if( !bAnySpread && !hasNamed ){` |
|         - |  898 | `					SyString sBuiltin;` |
|   7578371 |  899 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|   7578371 |  900 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|   3789183 |  901 | `				}` |
|   3791453 |  902 | `			}` |
|  16196115 |  903 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|   8613211 |  904 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   8613211 |  905 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|         - |  906 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|         - |  907 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|         - |  908 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|         - |  909 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|         - |  910 | `				 * builtin to write back through. A plain $var target is unaffected` |
|         - |  911 | `				 * (iP1=0 either way). */` |
|   8613211 |  912 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|     41623 |  913 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|     41623 |  914 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|     20809 |  915 | `				}` |
|         - |  916 | ``				/* D1: a plain `$var` argument may bind to a by-ref parameter whose signature`` |
|         - |  917 | `				 * is unknown at compile time (forward reference, dynamic call, or method` |
|         - |  918 | ``				 * dispatch — e.g. PHPUnit's `willReturnReference($undef)`). We used to clear`` |
|         - |  919 | `				 * the read-only flag here so an undefined variable vivified a real slot the` |
|         - |  920 | `				 * by-ref write-back could reach — but that also invented the variable as NULL` |
|         - |  921 | `				 * in the caller when the parameter turned out by-VALUE, and suppressed php's` |
|         - |  922 | ``				 * `Undefined variable $x` warning. Instead mark it DEFERRED: OP_LOAD leaves an`` |
|         - |  923 | `				 * undefined variable uncreated and carries a lazy-lvalue marker, and OP_CALL` |
|         - |  924 | `				 * materializes it ONLY for a by-ref parameter once the callee is resolved` |
|         - |  925 | `				 * (VmResolveDeferredArgs). Excludes isset()/empty()/unset(), which compile` |
|         - |  926 | `				 * through this same call loop but must NEVER create their operand, and` |
|         - |  927 | `				 * named/spread args (positional-index and by-ref semantics don't apply).` |
|         - |  928 | `				 *` |
|         - |  929 | `				 * D1 commit 2: the same reasoning extends to an array-element ($a["k"]) or` |
|         - |  930 | `				 * property ($o->p) argument — a by-ref user-function parameter must vivify the` |
|         - |  931 | `				 * element/property, a by-value one must warn and NOT vivify. Those nodes carry a` |
|         - |  932 | `				 * subscript/arrow operator (pOp != 0). The DEFER flag rides down to the base LOAD` |
|         - |  933 | `				 * (undefined base auto-defers via commit 1) and to the LOAD_IDX/MEMBER, which` |
|         - |  934 | ``				 * record the lvalue path on a lookup miss. Static `::` and nullsafe `?->` stay`` |
|         - |  935 | `				 * eager. */` |
|   8613206 |  936 | `				if( (iFlags & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_LOAD_IDX_UNSET)) == 0` |
|   8439016 |  937 | `				 && (iArgFlags & EXPR_FLAG_RDONLY_LOAD) /* not a known builtin by-ref slot (kept eager above) */` |
|   8244017 |  938 | `				 && (apNode[n]->iFlags & (EXPR_NODE_NAMED_ARG\|EXPR_NODE_SPREAD)) == 0` |
|   9533950 |  939 | `				 && ( (apNode[n]->pOp == 0 && apNode[n]->xCode == PH7_CompileVariable)` |
|   6430368 |  940 | `				   \|\| (apNode[n]->pOp != 0 && (apNode[n]->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|   2829151 |  941 | `				                            \|\| apNode[n]->pOp->iOp == EXPR_OP_ARROW)) ) ){` |
|   5468805 |  942 | `					iArgFlags \|= EXPR_FLAG_DEFER_ARG;` |
|   2734400 |  943 | `				}` |
|   8613211 |  944 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|   8613211 |  945 | `				if( rc != SXRET_OK ){` |
|         3 |  946 | `					return rc;` |
|         - |  947 | `				}` |
|         - |  948 | `				/* Each argument is an independent nullsafe scope. */` |
|   8613209 |  949 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|   8613209 |  950 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|         - |  951 | `					/* Emit spread opcode to unpack this array argument */` |
|      4345 |  952 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      4345 |  953 | `					hasSpread = 1;` |
|      2170 |  954 | `				}` |
|   4306607 |  955 | `			}` |
|         - |  956 | `			/* Total number of given arguments */` |
|   7582909 |  957 | `			iP1 = nArgs;` |
|   7582909 |  958 | `			iP2 = hasSpread;` |
|         - |  959 | `			/* Build VmCallArgMap if named arguments are present.` |
|         - |  960 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|   7582909 |  961 | `			if( hasNamed ){` |
|       248 |  962 | `				sxu32 nStrBytes = 0;` |
|         - |  963 | `				char *zBuf;` |
|       730 |  964 | `				for( n = 0; n < nArgs; ++n ){` |
|       486 |  965 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       372 |  966 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|       184 |  967 | `					}` |
|       245 |  968 | `				}` |
|         - |  969 | `				{` |
|       248 |  970 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|       248 |  971 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|       244 |  972 | `					&pGen->pVm->sAllocator, mapSize);` |
|       248 |  973 | `				if( pMap ){` |
|       248 |  974 | `					SyZero(pMap, mapSize);` |
|       248 |  975 | `					pMap->bHasNamed = 1;` |
|       248 |  976 | `					pMap->nTotal = (sxu32)nArgs;` |
|       248 |  977 | `					pMap->aNames = (SyString *)&pMap[1];` |
|       248 |  978 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|       730 |  979 | `					for( n = 0; n < nArgs; ++n ){` |
|       486 |  980 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       372 |  981 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|       372 |  982 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|       372 |  983 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|       372 |  984 | `							zBuf += nb;` |
|       184 |  985 | `						}` |
|         - |  986 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|       245 |  987 | `					}` |
|       248 |  988 | `					p3 = (void *)pMap;` |
|       122 |  989 | `				}` |
|         - |  990 | `				}` |
|       122 |  991 | `			}` |
|         - |  992 | `			/* assert(): php's compiler keeps a copy of the assertion's AST and a` |
|         - |  993 | ``			 * failing assert reports its rendered SOURCE (`assert(1 == 2)`), not the`` |
|         - |  994 | `			 * evaluated value. Render the first argument's token span` |
|         - |  995 | `			 * into the call map so vm_builtin_assert can echo it. Only a DIRECT` |
|         - |  996 | `			 * unqualified/absolute call qualifies — matching php, an indirect call` |
|         - |  997 | `			 * (call_user_func, a callable variable) has no source text and its` |
|         - |  998 | `			 * AssertionError carries an empty message. A spread first argument is` |
|         - |  999 | `			 * skipped (its span is the unpacked array, not the assertion). */` |
|   7582904 | 1000 | `			if( nArgs >= 1 && !bFcc` |
|   5931121 | 1001 | `			 && (apNode[0]->iFlags & EXPR_NODE_SPREAD) == 0 ){` |
|         - | 1002 | `				SyString sCallee;` |
|   5926821 | 1003 | `				GenStateCallBuiltinName(pNode->pLeft,&sCallee);` |
|   5926816 | 1004 | `				if( sCallee.nByte == sizeof("assert")-1` |
|   3270968 | 1005 | `				 && SyStrnicmp(sCallee.zString,"assert",sizeof("assert")-1) == 0 ){` |
|         - | 1006 | `					/* An operator root's pStart/pEnd name only the operator token` |
|         - | 1007 | ``					 * (`1 == 2` roots at `==`); the subtree walk recovers the whole`` |
|         - | 1008 | `					 * raw extent, re-adding parens the grouping pass consumed. */` |
|        67 | 1009 | `					SyToken *pSpanIn = 0;` |
|        67 | 1010 | `					SyToken *pSpanEnd = 0;` |
|         - | 1011 | `					SyBlob sSrc;` |
|        67 | 1012 | `					PH7_ExprSubtreeSpan(apNode[0],&pSpanIn,&pSpanEnd);` |
|        67 | 1013 | `					SyBlobInit(&sSrc,&pGen->pVm->sAllocator);` |
|        67 | 1014 | `					if( pSpanIn && pSpanEnd && pSpanIn < pSpanEnd ){` |
|        67 | 1015 | `						if( apNode[0]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|         - | 1016 | ``							/* php renders the name too: `assert(assertion: 1 == 2)`. */`` |
|         3 | 1017 | `							SyBlobAppend(&sSrc,apNode[0]->sArgName.zString,apNode[0]->sArgName.nByte);` |
|         3 | 1018 | `							SyBlobAppend(&sSrc,": ",2);` |
|         1 | 1019 | `						}` |
|        67 | 1020 | `						PH7_GenRenderAssertSpan(pGen,pSpanIn,pSpanEnd,&sSrc);` |
|        31 | 1021 | `					}` |
|        67 | 1022 | `					if( SyBlobLength(&sSrc) > 0 ){` |
|        98 | 1023 | `						char *zDup = (char *)SyMemBackendDup(&pGen->pVm->sAllocator,` |
|        62 | 1024 | `							SyBlobData(&sSrc),SyBlobLength(&sSrc));` |
|        67 | 1025 | `						if( zDup ){` |
|        67 | 1026 | `							if( p3 == 0 ){` |
|        65 | 1027 | `								VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|        60 | 1028 | `									&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|        65 | 1029 | `								if( pMap ){` |
|        65 | 1030 | `									SyZero(pMap,sizeof(VmCallArgMap));` |
|        65 | 1031 | `									p3 = (void *)pMap;` |
|        30 | 1032 | `								}` |
|        30 | 1033 | `							}` |
|        67 | 1034 | `							if( p3 ){` |
|        67 | 1035 | `								SyStringInitFromBuf(&((VmCallArgMap *)p3)->sAssertSrc,` |
|         - | 1036 | `									zDup,SyBlobLength(&sSrc));` |
|        31 | 1037 | `							}` |
|        31 | 1038 | `						}` |
|        31 | 1039 | `					}` |
|        67 | 1040 | `					SyBlobRelease(&sSrc);` |
|        31 | 1041 | `				}` |
|   2963408 | 1042 | `			}` |
|         - | 1043 | `			/* Remove stale flags now */` |
|   7582909 | 1044 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|   3791452 | 1045 | `		}` |
|         - | 1046 | `		{` |
|         - | 1047 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|         - | 1048 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|         - | 1049 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|         - | 1050 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|         - | 1051 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|         - | 1052 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|         - | 1053 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|         - | 1054 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
|  31798437 | 1055 | `			sxi32 iLeftFlags = iFlags;` |
|  31798437 | 1056 | `			sxu32 nNullcLhsFirst = PH7_VmInstrLength(pGen->pVm);` |
|  31798437 | 1057 | `			int bNullcLhs = 0;` |
|         - | 1058 | `			/* D1 commit 2: a deferred element/property call arg records its lvalue chain, but` |
|         - | 1059 | `			 * that chain must be CONTIGUOUS. Only propagate DEFER_ARG to the base when the base` |
|         - | 1060 | `			 * is itself a continuable lvalue — a plain variable (an undefined base auto-defers),` |
|         - | 1061 | ``			 * another subscript, or a `->` member. If the base is anything else (most importantly`` |
|         - | 1062 | ``			 * a method CALL, e.g. `$o->items()->prop` or `$r->attributes->item(0)->nodeName`),`` |
|         - | 1063 | `			 * strip DEFER so that intermediate read is a NORMAL read, not a record-mode carrier. */` |
|  31798437 | 1064 | `			if( iLeftFlags & EXPR_FLAG_DEFER_ARG ){` |
|   4041813 | 1065 | `				int bContinuable = pNode->pLeft` |
|   3095562 | 1066 | `					&& ( (pNode->pLeft->pOp == 0 && pNode->pLeft->xCode == PH7_CompileVariable)` |
|   1138863 | 1067 | `					  \|\| (pNode->pLeft->pOp != 0 && (pNode->pLeft->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|    115984 | 1068 | `					                              \|\| pNode->pLeft->pOp->iOp == EXPR_OP_ARROW)) );` |
|   2020909 | 1069 | `				if( !bContinuable ){` |
|        46 | 1070 | `					iLeftFlags &= ~EXPR_FLAG_DEFER_ARG;` |
|        22 | 1071 | `				}` |
|   1010452 | 1072 | `			}` |
|  31798432 | 1073 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
|  25863523 | 1074 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|   9964333 | 1075 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|   8593321 | 1076 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   2966255 | 1077 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|   1483125 | 1078 | `			}` |
|         - | 1079 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|         - | 1080 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|         - | 1081 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|         - | 1082 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|         - | 1083 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|         - | 1084 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|         - | 1085 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
|  31798432 | 1086 | `			if( pNode->pOp` |
|  44725324 | 1087 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
|  28826156 | 1088 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  25853827 | 1089 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|   6393067 | 1090 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|   3196531 | 1091 | `			}` |
|         - | 1092 | ``			/* `++`/`--` mutate their operand in place — the operand is a write`` |
|         - | 1093 | ``			 * lvalue exactly like a compound assign's (`$o->m[0]++` must tag the`` |
|         - | 1094 | ``			 * member base PH7_MEMBER_WRITE the way `$o->m[0] += 1` does: hooked`` |
|         - | 1095 | `			 * properties throw php's Indirect-modification Error, missing ones` |
|         - | 1096 | `			 * auto-vivify). The prec-18 site below handles the assign family;` |
|         - | 1097 | ``			 * `++`/`--` are unary, their operand is pLeft. */`` |
|  31798432 | 1098 | `			if( pNode->pOp` |
|  31798437 | 1099 | `				&& (pNode->pOp->iVmOp == PH7_OP_INCR \|\| pNode->pOp->iVmOp == PH7_OP_DECR) ){` |
|    232325 | 1100 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE` |
|         - | 1101 | ``					\| EXPR_FLAG_RMW_LOAD /* php warns before seeding `$undef++` */;`` |
|    116160 | 1102 | `			}` |
|         - | 1103 | ``			/* `??` reads its LEFT operand in isset-context: an undefined or`` |
|         - | 1104 | `			 * UNINITIALIZED typed PROPERTY must yield the default rather than a` |
|         - | 1105 | `			 * warning/Error (php). Tag a member-access LHS so its OP_MEMBER takes` |
|         - | 1106 | `			 * the silent-lookup path (iP2 = ISSET), which still loads a present` |
|         - | 1107 | `			 * value. A SUBSCRIPT LHS is left untagged: LOAD_IDX's ISSET mode means` |
|         - | 1108 | ``			 * offsetExists (a bool), but `$o[$k] ?? d` needs the offsetGet value —`` |
|         - | 1109 | `			 * that path is already handled correctly by OP_NULLC. */` |
|  31798437 | 1110 | `			if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC && pNode->pLeft ){` |
|         - | 1111 | ``				/* php reads the ENTIRE left operand of `??` in isset-context: no`` |
|         - | 1112 | ``				 * "Undefined variable" for `$x ?? d` OR for the base of`` |
|         - | 1113 | ``				 * `$x['k'] ?? d`. QUIET_VAR silences the variable read wherever it`` |
|         - | 1114 | `				 * sits in the chain. */` |
|     62311 | 1115 | `				iLeftFlags \|= EXPR_FLAG_QUIET_VAR;` |
|     62311 | 1116 | `				bNullcLhs = 1;` |
|     62306 | 1117 | `				if( pNode->pLeft->pOp` |
|     93379 | 1118 | `					&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|     62232 | 1119 | `						\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|     62215 | 1120 | `						\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|         - | 1121 | `					/* A member-access LHS additionally takes OP_MEMBER's silent` |
|         - | 1122 | `					 * lookup (iP2 = ISSET) so an uninitialized typed property` |
|         - | 1123 | `					 * yields the default instead of an Error. A SUBSCRIPT LHS must` |
|         - | 1124 | `					 * NOT: LOAD_IDX's ISSET mode means offsetExists (a bool), while` |
|         - | 1125 | ``					 * `$o[$k] ?? d` needs the offsetGet value — OP_NULLC already`` |
|         - | 1126 | `					 * handles that path. */` |
|        37 | 1127 | `					iLeftFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|        18 | 1128 | `				}` |
|     31153 | 1129 | `			}` |
|  31798437 | 1130 | `			if( iVmOp == PH7_OP_ERR_CTRL ){` |
|         - | 1131 | `				/* '@' must suppress the diagnostics raised WHILE its operand runs, so` |
|         - | 1132 | `				 * open the window here; the trailing emit below closes it (iP1 = 0). */` |
|     16825 | 1133 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_ERR_CTRL,1,0,0,0);` |
|      8410 | 1134 | `			}` |
|  31798437 | 1135 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags\|EXPR_FLAG_RDONLY_LOAD);` |
|  31798437 | 1136 | `			if( rc == SXRET_OK && bNullcLhs ){` |
|         - | 1137 | ``				/* Mark EVERY subscript read in the `??` left chain quiet (iP2=8).`` |
|         - | 1138 | `				 * Peeking at the next instruction only catches the OUTERMOST` |
|         - | 1139 | ``				 * access, so `$d['x']['y'] ?? $v` still warned for the inner one;`` |
|         - | 1140 | `				 * and a forward scan would be unsound, since an unrelated sibling` |
|         - | 1141 | ``				 * access can sit immediately before a coalesce (`f($a['k'],`` |
|         - | 1142 | ``				 * $b['j'] ?? 1)`). The compiler knows the real extent, so it marks`` |
|         - | 1143 | `				 * the range it just emitted. Write-context codes (1/3/5) belong to` |
|         - | 1144 | ``				 * `??=` and keep their meaning. */`` |
|     62311 | 1145 | `				sxu32 nEnd = PH7_VmInstrLength(pGen->pVm);` |
|         - | 1146 | `				sxu32 nAt;` |
|    348567 | 1147 | `				for( nAt = nNullcLhsFirst ; nAt < nEnd ; ++nAt ){` |
|    286261 | 1148 | `					VmInstr *pFix = PH7_VmGetInstr(pGen->pVm,nAt);` |
|    286261 | 1149 | `					if( pFix && pFix->iOp == PH7_OP_LOAD_IDX && pFix->iP2 == 0 ){` |
|     62197 | 1150 | `						pFix->iP2 = 8;` |
|     31096 | 1151 | `					}` |
|    143133 | 1152 | `				}` |
|     31153 | 1153 | `			}` |
|         - | 1154 | `		}` |
|  31798437 | 1155 | `		if( rc != SXRET_OK ){` |
|        32 | 1156 | `			return rc;` |
|         - | 1157 | `		}` |
|  31798409 | 1158 | `		if( !bIsChainOp ){` |
|         - | 1159 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|         - | 1160 | `			 * target the end of that LHS chain, which is right here. */` |
|  14593635 | 1161 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|   7296815 | 1162 | `		}` |
|  31798409 | 1163 | `		if( iVmOp == PH7_OP_CALL ){` |
|   7582909 | 1164 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   7582909 | 1165 | `			if( pInstr ){` |
|   7582909 | 1166 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|   5651519 | 1167 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|         - | 1168 | `					sxu32 nQual;` |
|   5651519 | 1169 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 1170 | `					/* Prevent constant expansion but preserve the absolute flag` |
|         - | 1171 | `					 * so the later NEW handler (if any) can see it. */` |
|   5651519 | 1172 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|         - | 1173 | `					/* Namespace-qualify the function name for CALL, unless the` |
|         - | 1174 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|         - | 1175 | `					 * imports — class imports must NOT affect function` |
|         - | 1176 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|         - | 1177 | `					 * before NEW; we store the original literal index in the` |
|         - | 1178 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|         - | 1179 | `					 * the unqualified name and re-qualify with class imports. */` |
|   5651519 | 1180 | `					if( bAbsolute ){` |
|      4185 | 1181 | `						pInstr->iP2 = (sxi32)nOrig;` |
|      2095 | 1182 | `					}else{` |
|   5647339 | 1183 | `						int fromImport = 0;` |
|   5647339 | 1184 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|   5647339 | 1185 | `						pInstr->iP2 = (sxi32)nQual;` |
|   5647339 | 1186 | `						if( nQual != nOrig ){` |
|         - | 1187 | `							/* Record the original literal index in the arg map` |
|         - | 1188 | `							 * (NOT in the CALL's iP2 — that is the hasSpread` |
|         - | 1189 | `							 * flag) so the NEW handler can recover the` |
|         - | 1190 | `							 * unqualified name and re-qualify with CLASS` |
|         - | 1191 | `							 * imports. */` |
|       103 | 1192 | `							if( p3 == 0 ){` |
|       103 | 1193 | `								VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|        98 | 1194 | `									&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|       103 | 1195 | `								if( pMap ){` |
|       103 | 1196 | `									SyZero(pMap, sizeof(VmCallArgMap));` |
|       103 | 1197 | `									p3 = (void *)pMap;` |
|        49 | 1198 | `								}` |
|        49 | 1199 | `							}` |
|       103 | 1200 | `							if( p3 ){` |
|       103 | 1201 | `								((VmCallArgMap *)p3)->nOrigNameLit = nOrig + 1;` |
|       103 | 1202 | `								if( !fromImport ){` |
|         - | 1203 | `									/* Mark as namespace-qualified */` |
|        93 | 1204 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|        44 | 1205 | `								}` |
|        49 | 1206 | `							}` |
|        49 | 1207 | `						}` |
|         - | 1208 | `					}` |
|   4757152 | 1209 | `				}else if( (pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */` |
|   1920558 | 1210 | `						&& !(pNode->pLeft && (pNode->pLeft->iFlags & EXPR_NODE_PARENS)))` |
|    976539 | 1211 | `					\|\| pInstr->iOp == PH7_OP_NEW ){` |
|         - | 1212 | `					/* Method call,flag that. But NOT when the callee was an explicitly` |
|         - | 1213 | ``					 * PARENTHESISED member access: `($o->p)(...)` / `($o::$p)(...)` invokes`` |
|         - | 1214 | `					 * the VALUE of the property (php's variable-invocation), so the OP_MEMBER` |
|         - | 1215 | `					 * must stay a plain property READ (leaving the callable on the stack for` |
|         - | 1216 | `					 * OP_CALL to invoke) rather than being rewritten into a method-name` |
|         - | 1217 | ``					 * resolution — the parens are exactly what distinguishes `($o->p)()` from`` |
|         - | 1218 | ``					 * the method call `$o->p()`. */`` |
|   1909723 | 1219 | `					pInstr->iP2 = 1;` |
|         - | 1220 | ``					/* A static call with a DYNAMIC method name (`C::$m(...)`): the`` |
|         - | 1221 | ``					 * static-`::` codegen folded the variable NAME into OP_MEMBER->p3`` |
|         - | 1222 | ``					 * as if it were a static-PROPERTY read (`C::$m`), but in a CALL the`` |
|         - | 1223 | `					 * method name is the variable's VALUE. Rebuild the sequence` |
|         - | 1224 | `					 * [class, OP_LOAD $m -> value, OP_MEMBER(static, method)] so the` |
|         - | 1225 | `					 * dynamic name is read off the stack, matching the instance` |
|         - | 1226 | ``					 * (`$o->$m()`) path. iP1==1 marks a static member. */`` |
|   1909723 | 1227 | `					if( pInstr->iOp == PH7_OP_MEMBER && pInstr->iP1 == 1 && pInstr->p3 ){` |
|        11 | 1228 | `						void *pDynName = pInstr->p3;` |
|        11 | 1229 | `						(void)PH7_VmPopInstr(pGen->pVm);` |
|        11 | 1230 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,pDynName,0);` |
|        11 | 1231 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_MEMBER,1,PH7_MEMBER_METHOD,0,0);` |
|         5 | 1232 | `					}` |
|    954859 | 1233 | `				}` |
|   3791457 | 1234 | `			}` |
|  28006957 | 1235 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|         - | 1236 | `			ph7_expr_node **apNode;` |
|         - | 1237 | `			sxi32 n;` |
|   3228813 | 1238 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|         - | 1239 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|         - | 1240 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE` |
|         - | 1241 | `				\|EXPR_FLAG_QUIET_VAR\|EXPR_FLAG_RMW_LOAD\|EXPR_FLAG_DEFER_ARG);` |
|         - | 1242 | `			/* Recurse and generate bytecodes for array index */` |
|   3228813 | 1243 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   6204605 | 1244 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   2975797 | 1245 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   2975797 | 1246 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   2975797 | 1247 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 1248 | `					return rc;` |
|         - | 1249 | `				}` |
|         - | 1250 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   2975797 | 1251 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   1487901 | 1252 | `			}` |
|   3228813 | 1253 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   2975797 | 1254 | `				iP1 = 1; /* Node have an index associated with it */` |
|   1487896 | 1255 | `			}` |
|   3228813 | 1256 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|         - | 1257 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|    397759 | 1258 | `				iP2 = 4;` |
|   3029936 | 1259 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|         - | 1260 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|         - | 1261 | `				 * so the trailing unset() builtin can drop the slot. */` |
|     24939 | 1262 | `				iP2 = 5;` |
|   2818592 | 1263 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|         - | 1264 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|         - | 1265 | `				 * short-circuit on missing keys without invoking offsetGet` |
|         - | 1266 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|        37 | 1267 | `				iP2 = 6;` |
|   2806109 | 1268 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|         - | 1269 | `				/* Create an empty entry when the desired index is not found */` |
|    593033 | 1270 | `				iP2 = 1;` |
|   2509579 | 1271 | `			}else if( iFlags & EXPR_FLAG_DEFER_ARG ){` |
|         - | 1272 | `				/* D1 commit 2: deferred by-ref/by-value element arg. Behaves as a read but,` |
|         - | 1273 | `				 * on a lookup miss, records the lvalue path instead of warning; OP_CALL` |
|         - | 1274 | `				 * re-walks it in vivify (by-ref) or read+warn (by-value) mode. */` |
|    430795 | 1275 | `				iP2 = 9;` |
|    215400 | 1276 | `			}` |
|  22601101 | 1277 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         - | 1278 | `			/* POP the left node */` |
|         5 | 1279 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         2 | 1280 | `		}` |
|  15899202 | 1281 | `	}` |
|  31856157 | 1282 | `	rc = SXRET_OK;` |
|  31856157 | 1283 | `	nJmpIdx = 0;` |
|         - | 1284 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|         - | 1285 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|         - | 1286 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  31856157 | 1287 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|    498089 | 1288 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    498089 | 1289 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    498089 | 1290 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    498089 | 1291 | `			int isSpecial = 0;` |
|    498089 | 1292 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|    398689 | 1293 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|    398689 | 1294 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|    398684 | 1295 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    371682 | 1296 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    201366 | 1297 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|    157531 | 1298 | `					isSpecial = 1;` |
|     78763 | 1299 | `				}` |
|    224192 | 1300 | `			}` |
|    547789 | 1301 | `			pInstr->iP1 = 0;` |
|         - | 1302 | ``			/* A leading `\` (NSSEP) makes the class name ABSOLUTE: it names the`` |
|         - | 1303 | `			 * global class, so it must NOT be re-qualified with the current namespace.` |
|         - | 1304 | ``			 * `\Closure::bind(...)` inside `namespace X` is Closure, not X\Closure.`` |
|         - | 1305 | ``			 * (Multi-component `\A\B::m` already resolves absolutely because its`` |
|         - | 1306 | ``			 * literal keeps a backslash; the single-component `\Closure` lost it.) */`` |
|         - | 1307 | `			{` |
|    771981 | 1308 | `				int bAbsolute = (pNode->pLeft && pNode->pLeft->pStart` |
|    672576 | 1309 | `					&& (pNode->pLeft->pStart->nType & PH7_TK_NSSEP));` |
|    448389 | 1310 | `				if( !isSpecial && !bAbsolute ){` |
|    290845 | 1311 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|    145420 | 1312 | `				}` |
|         - | 1313 | `			}` |
|         - | 1314 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|         - | 1315 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|    448389 | 1316 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|    290863 | 1317 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|    290863 | 1318 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|       166 | 1319 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|        92 | 1320 | `					return SXRET_OK;` |
|         - | 1321 | `				}` |
|    145385 | 1322 | `			}` |
|    224148 | 1323 | `		}` |
|    323518 | 1324 | `	}` |
|         - | 1325 | `	/* Generate code for the right tree */` |
|  31806389 | 1326 | `	if( pNode->pRight ){` |
|  18281317 | 1327 | `		if( iVmOp == PH7_OP_LAND ){` |
|         - | 1328 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    484901 | 1329 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  18038869 | 1330 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|         - | 1331 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    385149 | 1332 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  17603849 | 1333 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|         - | 1334 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     62311 | 1335 | `			iVmOp = 0; /* No binary operator to emit */` |
|     62311 | 1336 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  17380177 | 1337 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|         - | 1338 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|         - | 1339 | `			 * the entire containing postfix chain to null. The jump target is` |
|         - | 1340 | `			 * patched later by the innermost non-chain ancestor (or by` |
|         - | 1341 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|         - | 1342 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|         - | 1343 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|       109 | 1344 | `			sxu32 nNsJmp = 0;` |
|       109 | 1345 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|       109 | 1346 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  17348916 | 1347 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */` |
|  14560197 | 1348 | ``			\|\| pNode->pOp->iOp == EXPR_OP_REF /* `=&` reference bind */ ){`` |
|         - | 1349 | `` 			/* The lvalue is the RIGHT operand (prec-18 ops are right-associative; `=&` `` |
|         - | 1350 | `			 * swaps its operands in parse.c so its target is pRight too). Mark it a write` |
|         - | 1351 | `			 * target so a missing base (the container of a subscript-write, or a bare` |
|         - | 1352 | `` 			 * `$o->p`) is auto-created — PHP auto-vivifies on a plain write AND on a `=&` `` |
|         - | 1353 | ``			 * bind (`$a[0] =& $x` creates $a as [0 => &$x], it does not warn). */`` |
|   5577427 | 1354 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|   5577427 | 1355 | `			if( iVmOp != PH7_OP_STORE && pNode->pOp->iOp != EXPR_OP_REF ){` |
|         - | 1356 | ``				/* COMPOUND assignment (`.=`, `+=`, ...) READS the target first, so`` |
|         - | 1357 | ``				 * php warns when it is undefined and then seeds it; a plain `=` and a`` |
|         - | 1358 | ``				 * `=&` rebind write without reading the target and stay silent. */`` |
|    480625 | 1359 | `				iFlags \|= EXPR_FLAG_RMW_LOAD;` |
|    240310 | 1360 | `			}` |
|   2788711 | 1361 | `		}` |
|  18281317 | 1362 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  18281317 | 1363 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_RDONLY_LOAD);` |
|  18281317 | 1364 | `		if( !bIsChainOp ){` |
|         - | 1365 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|         - | 1366 | `			 * operator instruction is emitted. */` |
|  11888343 | 1367 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|   5944169 | 1368 | `		}` |
|  18281317 | 1369 | `		if( iVmOp == PH7_OP_STORE ){` |
|   5096721 | 1370 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|   5096678 | 1371 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|         - | 1372 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|         - | 1373 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|         - | 1374 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|         - | 1375 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|         - | 1376 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|         - | 1377 | `				 */` |
|       149 | 1378 | `				iVmOp = 0;` |
|   5096649 | 1379 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|   5096577 | 1380 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 1381 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|    981991 | 1382 | `					iP2 = 1;` |
|    490998 | 1383 | `				}else{` |
|   4114591 | 1384 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 1385 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|    572191 | 1386 | `						iVmOp = PH7_OP_STORE_IDX;` |
|    572191 | 1387 | `						iP1 = pInstr->iP1;` |
|    286098 | 1388 | `					}else{` |
|   3542405 | 1389 | `						p3 = pInstr->p3;` |
|         - | 1390 | `					}` |
|         - | 1391 | `					/* POP the last dynamic load instruction */` |
|   4114591 | 1392 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         - | 1393 | `				}` |
|   2548291 | 1394 | `			}` |
|  15732959 | 1395 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|         - | 1396 | `			/* Peek first: a member LHS ($o->p =& $x, C::$s =& $x) keeps its` |
|         - | 1397 | `			 * OP_MEMBER in place (it resolves + stashes the target slot at` |
|         - | 1398 | `			 * runtime), unlike the variable/array shapes which fold their load` |
|         - | 1399 | `			 * away. Mirrors the normal member-store fold above (iP2 kept-MEMBER). */` |
|        90 | 1400 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|        90 | 1401 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 1402 | `				/* Tag the member as a reference target and flag STORE_REF (iP2=1)` |
|         - | 1403 | `				 * to take the member-rebind path in the VM. */` |
|        11 | 1404 | `				pInstr->iP2 = PH7_MEMBER_REF_TARGET;` |
|        11 | 1405 | `				iP2 = 1;` |
|         6 | 1406 | `			}else{` |
|        80 | 1407 | `				pInstr = PH7_VmPopInstr(pGen->pVm);` |
|        80 | 1408 | `				if( pInstr ){` |
|        80 | 1409 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 1410 | `						/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|         - | 1411 | `						 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|         - | 1412 | `						 */` |
|        36 | 1413 | `						iVmOp = PH7_OP_STORE_IDX_REF;` |
|        36 | 1414 | `						iP1 = pInstr->iP1;` |
|        36 | 1415 | `						iP2 = pInstr->iP2;` |
|        36 | 1416 | `						p3  = pInstr->p3;` |
|        19 | 1417 | `					}else{` |
|        46 | 1418 | `						p3 = pInstr->p3;` |
|         - | 1419 | `					}` |
|        38 | 1420 | `				}` |
|         - | 1421 | `			}` |
|        43 | 1422 | `		}` |
|   9140656 | 1423 | `	}` |
|  31806384 | 1424 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|    463732 | 1425 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|         - | 1426 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|         - | 1427 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|        55 | 1428 | `		iVmOp = 0;` |
|        25 | 1429 | `	}` |
|  31806389 | 1430 | `	if( iVmOp > 0 ){` |
|  31743885 | 1431 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|    232325 | 1432 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|         - | 1433 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|     16595 | 1434 | `				iP1 = 1;` |
|      8300 | 1435 | `			}` |
|  31627725 | 1436 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|         - | 1437 | `			/* Namespace-qualify the class name for NEW */ {` |
|    930683 | 1438 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|    930683 | 1439 | `				VmInstr *pCallInstr = 0;` |
|    930683 | 1440 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|    917871 | 1441 | `					pCallInstr = pPeek;` |
|    917871 | 1442 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    458933 | 1443 | `				}` |
|    930683 | 1444 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|    914109 | 1445 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 1446 | `					sxu32 nLitForClass;` |
|    914109 | 1447 | `					VmCallArgMap *pCallNsMap = pCallInstr ? (VmCallArgMap *)pCallInstr->p3 : 0;` |
|         - | 1448 | `					/* If the CALL handler qualified the name with FUNCTION` |
|         - | 1449 | `					 * imports, recover the original literal (recorded in the` |
|         - | 1450 | `					 * arg map — OP_CALL's iP2 is the hasSpread flag, and` |
|         - | 1451 | `` 					 * misreading it as a literal index made `new C(...$args)` `` |
|         - | 1452 | `					 * fatal with "Class ' ' is not defined") and re-qualify` |
|         - | 1453 | `					 * with class imports. */` |
|    914109 | 1454 | `					if( pCallNsMap && pCallNsMap->nOrigNameLit > 0 ){` |
|        55 | 1455 | `						nLitForClass = pCallNsMap->nOrigNameLit - 1;` |
|        30 | 1456 | `					}else{` |
|    914059 | 1457 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|         - | 1458 | `					}` |
|    914109 | 1459 | `					pPeek->iP1 = 0;` |
|    914109 | 1460 | `					if( !bAbsolute ){` |
|         - | 1461 | `						/* self/static/parent are resolved at runtime against the` |
|         - | 1462 | `						 * current class — never namespace-qualify them (else` |
|         - | 1463 | ``						 * `new self` in namespace N becomes "N\self"). Mirrors the`` |
|         - | 1464 | `						 * instanceof (IS_A) guard below. */` |
|    909941 | 1465 | `						ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nLitForClass);` |
|    909941 | 1466 | `						int isSpecialNew = 0;` |
|    909941 | 1467 | `						if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|    893805 | 1468 | `							const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|    893805 | 1469 | `							sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|    893800 | 1470 | `							if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    897788 | 1471 | `								(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    446855 | 1472 | `								(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      8321 | 1473 | `								isSpecialNew = 1;` |
|      4158 | 1474 | `							}` |
|    450934 | 1475 | `						}` |
|    918009 | 1476 | `						if( isSpecialNew ){` |
|      8321 | 1477 | `							pPeek->iP2 = (sxi32)nLitForClass;` |
|      4163 | 1478 | `						}else{` |
|    893557 | 1479 | `							pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|         - | 1480 | `						}` |
|    450939 | 1481 | `					}else{` |
|      4173 | 1482 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|         - | 1483 | `					}` |
|    453018 | 1484 | `				}` |
|         - | 1485 | `			}` |
|    922615 | 1486 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    922615 | 1487 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|         - | 1488 | `				VmInstr *pPrev;` |
|    917871 | 1489 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|    917871 | 1490 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|         - | 1491 | `					/* Pop the call instruction, preserve named-arg map and` |
|         - | 1492 | `					 * the hasSpread flag (OP_NEW consumes the spread` |
|         - | 1493 | `					 * accumulator exactly like OP_CALL would have). */` |
|    917871 | 1494 | `					iP1 = pInstr->iP1;` |
|    917871 | 1495 | `					iP2 = pInstr->iP2;` |
|    917871 | 1496 | `					if( pInstr->p3 ){` |
|        65 | 1497 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|        30 | 1498 | `					}` |
|    917871 | 1499 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    458933 | 1500 | `				}` |
|    458938 | 1501 | `			}` |
|  31042192 | 1502 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|         - | 1503 | `			/* instanceof: right operand is a class name, not a constant.` |
|         - | 1504 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     87265 | 1505 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     87265 | 1506 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     87265 | 1507 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     87265 | 1508 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     87265 | 1509 | `				int isSpecialIs = 0;` |
|     87265 | 1510 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     87265 | 1511 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     87265 | 1512 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     87260 | 1513 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     87263 | 1514 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     43630 | 1515 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|        12 | 1516 | `						isSpecialIs = 1;` |
|         5 | 1517 | `					}` |
|     43630 | 1518 | `				}` |
|     87265 | 1519 | `				pInstr->iP1 = 0;` |
|     87265 | 1520 | `				if( !isSpecialIs && !bAbsolute ){` |
|     87239 | 1521 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     43617 | 1522 | `				}` |
|     43635 | 1523 | `			}` |
|  30537257 | 1524 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|         - | 1525 | `			/* Prevent constant expansion for member/property names.` |
|         - | 1526 | `			 * The right child (member name) was just compiled — its LOADC` |
|         - | 1527 | `			 * should not trigger constant lookup. */` |
|   6392979 | 1528 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   6392979 | 1529 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|   6136151 | 1530 | `				pInstr->iP1 = 0;` |
|   3068073 | 1531 | `			}` |
|   6392979 | 1532 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|         - | 1533 | `				/* Static member access,remember that */` |
|    448321 | 1534 | `				iP1 = 1;` |
|    448321 | 1535 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    448321 | 1536 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|    252679 | 1537 | `					p3 = pInstr->p3;` |
|         - | 1538 | ``					/* A `$`-form (`C::$s`, `C::$$x`, `C::${$e}`) is a STATIC PROPERTY`` |
|         - | 1539 | `					 * access, never a constant. A LITERAL name folds into p3 (non-zero)` |
|         - | 1540 | ``					 * and the exec side reads it there; a DYNAMIC name (`$$x`/`${$e}`)`` |
|         - | 1541 | `					 * leaves p3==0 with the computed name on the stack — the SAME shape` |
|         - | 1542 | ``					 * as a bareword constant `C::C`. Mark iP1=2 so exec still routes it`` |
|         - | 1543 | `					 * to the property table (hAttr), not the constant table (hConst). */` |
|    252679 | 1544 | `					if( p3 == 0 ){` |
|         7 | 1545 | `						iP1 = 2;` |
|         3 | 1546 | `					}` |
|    252679 | 1547 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    126337 | 1548 | `				}` |
|    224158 | 1549 | `			}` |
|         - | 1550 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|         - | 1551 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|         - | 1552 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|         - | 1553 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|   6392979 | 1554 | `			if( iP2 == PH7_MEMBER_READ ){` |
|   6392979 | 1555 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|        42 | 1556 | `					iP2 = PH7_MEMBER_UNSET;` |
|   6392959 | 1557 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|     74665 | 1558 | `					iP2 = PH7_MEMBER_ISSET;` |
|   6355609 | 1559 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|        17 | 1560 | `					iP2 = PH7_MEMBER_EMPTY;` |
|   6318271 | 1561 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|         - | 1562 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|   1189187 | 1563 | `					iP2 = PH7_MEMBER_WRITE;` |
|   5723672 | 1564 | `				}else if( iFlags & EXPR_FLAG_DEFER_ARG ){` |
|         - | 1565 | `					/* D1 commit 2: deferred by-ref/by-value property arg ($o->p). */` |
|   1590119 | 1566 | `					iP2 = PH7_MEMBER_DEFPATH;` |
|    795057 | 1567 | `				}` |
|   3196487 | 1568 | `			}` |
|   3196487 | 1569 | `		}` |
|         - | 1570 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|         - | 1571 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|         - | 1572 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|         - | 1573 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|         - | 1574 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
|  31735817 | 1575 | `		if( bFcc ){` |
|        95 | 1576 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|        95 | 1577 | `			iP2 = 0;` |
|        95 | 1578 | `			p3 = 0;` |
|        95 | 1579 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|        95 | 1580 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 1581 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|         - | 1582 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|         - | 1583 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|         - | 1584 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|        41 | 1585 | `				void *pMemberName = pInstr->p3;` |
|        41 | 1586 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|        41 | 1587 | `				if( pMemberName ){` |
|       ! 0 | 1588 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|       ! 0 | 1589 | `				}` |
|        41 | 1590 | `				iP1 = 2;` |
|        21 | 1591 | `			}else{` |
|        55 | 1592 | `				iP1 = 1;` |
|         - | 1593 | `			}` |
|        46 | 1594 | `		}` |
|         - | 1595 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|         - | 1596 | `		 * This is the primary emit path for user-visible calls. */` |
|  31735817 | 1597 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|   8505427 | 1598 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|   4252711 | 1599 | `		}` |
|         - | 1600 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  31735817 | 1601 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  15867906 | 1602 | `	}` |
|  31798321 | 1603 | `	if( nJmpIdx > 0 ){` |
|         - | 1604 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|    932351 | 1605 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|    932351 | 1606 | `		if( pInstr ){` |
|    932351 | 1607 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    466173 | 1608 | `		}` |
|    466173 | 1609 | `	}` |
|  31798321 | 1610 | `	return rc;` |
|  40816354 | 1611 | `}` |
|         - | 1612 | `/*` |
|         - | 1613 | ` * Compile a PHP expression.` |
|         - | 1614 | ` * According to the PHP language reference manual:` |
|         - | 1615 | ` *  Expressions are the most important building stones of PHP.` |
|         - | 1616 | ` *  In PHP, almost anything you write is an expression.` |
|         - | 1617 | ` *  The simplest yet most accurate way to define an expression` |
|         - | 1618 | ` *  is "anything that has a value".` |
|         - | 1619 | ` * If something goes wrong while compiling the expression,this` |
|         - | 1620 | ` * function takes care of generating the appropriate error` |
|         - | 1621 | ` * message.` |
|         - | 1622 | ` */` |
|         - | 1623 | `/*` |
|         - | 1624 | ` * Does this expression tree contain a comma OPERATOR node?` |
|         - | 1625 | ` *` |
|         - | 1626 | `` * PH7 shipped `,` as a lowest-precedence binary operator (IMP-0139-COMMA), so`` |
|         - | 1627 | `` * `(1, 2)` and `$x = (f(), $y)` compile and evaluate to the right operand.`` |
|         - | 1628 | ` * php 8 has no comma operator: its grammar only allows comma-separated` |
|         - | 1629 | ` * expression LISTS inside for(...) clauses (call arguments, array literals and` |
|         - | 1630 | ` * list() are split by the parser, never by this node). Accepting it changes the` |
|         - | 1631 | ` * meaning of source php rejects, which §10 classes as a bug — so every context` |
|         - | 1632 | ` * except for() now reports php's parse error.` |
|         - | 1633 | ` */` |
| 270951324 | 1634 | `static int GenStateTreeHasComma(ph7_expr_node *pNode)` |
|         5 | 1635 | `{` |
|         - | 1636 | `	ph7_expr_node **apArg;` |
|         - | 1637 | `	sxu32 n;` |
| 270951329 | 1638 | `	if( pNode == 0 ){` |
| 190470781 | 1639 | `		return 0;` |
|         - | 1640 | `	}` |
|  80480553 | 1641 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         6 | 1642 | `		return 1;` |
|         - | 1643 | `	}` |
|  80480544 | 1644 | `	if( GenStateTreeHasComma(pNode->pLeft) \|\| GenStateTreeHasComma(pNode->pRight)` |
|  80480545 | 1645 | `	 \|\| GenStateTreeHasComma(pNode->pCond) ){` |
|         6 | 1646 | `		return 1;` |
|         - | 1647 | `	}` |
|  80480545 | 1648 | `	apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  92044795 | 1649 | `	for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; n++ ){` |
|  11564255 | 1650 | `		if( GenStateTreeHasComma(apArg[n]) ){` |
|       ! 0 | 1651 | `			return 1;` |
|         - | 1652 | `		}` |
|   5782130 | 1653 | `	}` |
|  80480545 | 1654 | `	return 0;` |
| 135475667 | 1655 | `}` |
|  18343614 | 1656 | `PH7_PRIVATE sxi32 PH7_CompileExpr(` |
|         - | 1657 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 1658 | `	sxi32 iFlags,        /* Control flags */` |
|         - | 1659 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|         - | 1660 | `	)` |
|         5 | 1661 | `{` |
|         - | 1662 | `	ph7_expr_node *pRoot;` |
|         - | 1663 | `	SySet sExprNode;` |
|         - | 1664 | `	SyToken *pEnd;` |
|         - | 1665 | `	sxi32 nExpr;` |
|         - | 1666 | `	sxi32 iNest;` |
|         - | 1667 | `	sxi32 rc;` |
|         - | 1668 | `	sxu32 nNullsafeBase;` |
|         - | 1669 | `	/* Initialize worker variables */` |
|  18343619 | 1670 | `	nExpr = 0;` |
|  18343619 | 1671 | `	pRoot = 0;` |
|         - | 1672 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|         - | 1673 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  18343619 | 1674 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  18343619 | 1675 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  18343619 | 1676 | `	SySetAlloc(&sExprNode,0x10);` |
|  18343619 | 1677 | `	rc = SXRET_OK;` |
|         - | 1678 | `	/* Delimit the expression */` |
|  18343619 | 1679 | `	pEnd = pGen->pIn;` |
|  18343619 | 1680 | `	iNest = 0;` |
| 143958373 | 1681 | `	while( pEnd < pGen->pEnd ){` |
| 136897517 | 1682 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 1683 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|      5235 | 1684 | `			iNest++;` |
| 136894902 | 1685 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|      5245 | 1686 | `			iNest--;` |
| 136889667 | 1687 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  11283895 | 1688 | `			if( iNest <= 0 ){` |
|  11282763 | 1689 | `				break;` |
|         - | 1690 | `			}` |
|       566 | 1691 | `		}` |
| 125614759 | 1692 | `		pEnd++;` |
|         5 | 1693 | `	}` |
|  18343619 | 1694 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|    788077 | 1695 | `		SyToken *pEnd2 = pGen->pIn;` |
|    788077 | 1696 | `		iNest = 0;` |
|         - | 1697 | `		/* Stop at the first comma */` |
|   1729965 | 1698 | `		while( pEnd2 < pEnd ){` |
|    941911 | 1699 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|     53995 | 1700 | `				iNest++;` |
|    914916 | 1701 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|     53995 | 1702 | `				iNest--;` |
|    860926 | 1703 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      6097 | 1704 | `				if( iNest <= 0 ){` |
|        21 | 1705 | `					break;` |
|         - | 1706 | `				}` |
|      3037 | 1707 | `			}` |
|    941893 | 1708 | `			pEnd2++;` |
|         5 | 1709 | `		}` |
|    788077 | 1710 | `		if( pEnd2 <pEnd ){` |
|        21 | 1711 | `			pEnd = pEnd2;` |
|         9 | 1712 | `		}` |
|    394036 | 1713 | `	}` |
|  18343619 | 1714 | `	if( pEnd > pGen->pIn ){` |
|  18318765 | 1715 | `		SyToken *pTmp = pGen->pEnd;` |
|         - | 1716 | `		/* Swap delimiter */` |
|  18318765 | 1717 | `		pGen->pEnd = pEnd;` |
|         - | 1718 | `		/* Try to get an expression tree */` |
|  18318765 | 1719 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  18318760 | 1720 | `		if( rc == SXRET_OK && pRoot && pGen->nCommaExprOk < 1` |
|  18132018 | 1721 | `		 && GenStateTreeHasComma(pRoot) ){` |
|         - | 1722 | `			/* php has no comma operator outside a for() clause */` |
|         6 | 1723 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pRoot->pStart->nLine,` |
|         - | 1724 | `				"syntax error, unexpected token \",\"");` |
|         6 | 1725 | `			pGen->pEnd = pTmp;` |
|         6 | 1726 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 1727 | `				SySetRelease(&sExprNode);` |
|       ! 0 | 1728 | `				return SXERR_ABORT;` |
|         - | 1729 | `			}` |
|         6 | 1730 | `			pGen->pIn = pEnd;` |
|         6 | 1731 | `			SySetRelease(&sExprNode);` |
|         6 | 1732 | `			SySetTruncate(&pGen->aNullsafeJmp,nNullsafeBase);` |
|         6 | 1733 | `			return SXRET_OK;` |
|         - | 1734 | `		}` |
|  18318761 | 1735 | `		if( rc == SXRET_OK && pRoot ){` |
|  18318577 | 1736 | `			rc = SXRET_OK;` |
|  18318577 | 1737 | `			if( xTreeValidator ){` |
|         - | 1738 | `				/* Call the upper layer validator callback */` |
|   1103449 | 1739 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    551722 | 1740 | `			}` |
|  18318577 | 1741 | `			if( rc != SXERR_ABORT ){` |
|         - | 1742 | `				/* Generate code for the given tree */` |
|  18318577 | 1743 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|         - | 1744 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|         - | 1745 | `				 * expression so they short-circuit to its end. */` |
|  18318577 | 1746 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|   9159286 | 1747 | `			}` |
|  18318577 | 1748 | `			nExpr = 1;` |
|   9159286 | 1749 | `		}` |
|         - | 1750 | `		/* Release the whole tree */` |
|  18318761 | 1751 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|         - | 1752 | `		/* Synchronize token stream */` |
|  18318761 | 1753 | `		pGen->pEnd = pTmp;` |
|  18318761 | 1754 | `		pGen->pIn  = pEnd;` |
|  18318761 | 1755 | `		if( rc == SXERR_ABORT ){` |
|        24 | 1756 | `			SySetRelease(&sExprNode);` |
|        24 | 1757 | `			return SXERR_ABORT;` |
|         - | 1758 | `		}` |
|   9159368 | 1759 | `	}` |
|  18343595 | 1760 | `	SySetRelease(&sExprNode);` |
|  18343595 | 1761 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|   9171812 | 1762 | `}` |
|         - | 1763 | `/*` |
|         - | 1764 | ` * Return a pointer to the node construct handler associated` |
|         - | 1765 | ` * with a given node type [i.e: string,integer,float,...].` |
|         - | 1766 | ` */` |
|  10242374 | 1767 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|         5 | 1768 | `{` |
|  10242379 | 1769 | `	if( nNodeType & PH7_TK_NUM ){` |
|         - | 1770 | `		/* Numeric literal: Either real or integer */` |
|   4119265 | 1771 | `		return PH7_CompileNumLiteral;` |
|   6123119 | 1772 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|         - | 1773 | `		/* Double quoted string */` |
|    134317 | 1774 | `		return PH7_CompileString;` |
|   5988807 | 1775 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|         - | 1776 | `		/* Single quoted string */` |
|   5988683 | 1777 | `		return PH7_CompileSimpleString;` |
|       129 | 1778 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|         - | 1779 | `		/* Heredoc */` |
|        72 | 1780 | `		return PH7_CompileHereDoc;` |
|        60 | 1781 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|         - | 1782 | `		/* Nowdoc */` |
|        56 | 1783 | `		return PH7_CompileNowDoc;` |
|         6 | 1784 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|         - | 1785 | `		/* Backtick quoted string */` |
|         3 | 1786 | `		return PH7_CompileBacktic;` |
|         - | 1787 | `	}` |
|         3 | 1788 | `	return 0;` |
|   5121192 | 1789 | `}` |
|         - | 1790 | `/*` |
|         - | 1791 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|         - | 1792 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|         - | 1793 | ` * in write context" parse error.` |
|         - | 1794 | ` */` |
|     24976 | 1795 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|         5 | 1796 | `{` |
|         - | 1797 | `	sxi32 rc;` |
|     24981 | 1798 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|     24979 | 1799 | `		return SXRET_OK;` |
|         - | 1800 | `	}` |
|         5 | 1801 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|         2 | 1802 | `		pNode ? pNode->pStart->nLine : 1,` |
|         - | 1803 | `		"Can't use nullsafe operator in write context");` |
|         3 | 1804 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|     12493 | 1805 | `}` |
|         - | 1806 | `/*` |
|         - | 1807 | ` * Compile an unset() statement.` |
|         - | 1808 | ` * unset($var, $arr[$key], ...);` |
|         - | 1809 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|         - | 1810 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|         - | 1811 | ` * parent array before extracting the element to unset.` |
|         - | 1812 | ` */` |
|     27718 | 1813 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|         5 | 1814 | `{` |
|     27723 | 1815 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|     27723 | 1816 | `	sxu32 nIdx = 0;` |
|         - | 1817 | `	SyString sName;` |
|         - | 1818 | `	sxi32 rc;` |
|         - | 1819 | `	/* Jump the 'unset' keyword */` |
|     27723 | 1820 | `	pGen->pIn++;` |
|         - | 1821 | `	/* Save delimiter */` |
|     27723 | 1822 | `	pTmp = pGen->pEnd;` |
|         - | 1823 | `	/* Skip optional opening parenthesis and find the matching close */` |
|     27723 | 1824 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|     27723 | 1825 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 1826 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|         - | 1827 | `		SyToken *pClose;` |
|     27723 | 1828 | `		pGen->pIn++;   /* Skip '(' */` |
|     27723 | 1829 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|     27723 | 1830 | `		pEnd = pClose; /* Stop at ')' */` |
|     13859 | 1831 | `	}` |
|     27723 | 1832 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|         - | 1833 | `	/* Resolve the 'unset' builtin name once */` |
|     27723 | 1834 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|      4145 | 1835 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      4145 | 1836 | `		if( pObj == 0 ){` |
|       ! 0 | 1837 | `			return SXERR_ABORT;` |
|         - | 1838 | `		}` |
|      4145 | 1839 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      4145 | 1840 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      2070 | 1841 | `	}` |
|         - | 1842 | `	/* Compile each comma-separated argument */` |
|     59413 | 1843 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|     31695 | 1844 | `		if( pGen->pIn < pNext ){` |
|         - | 1845 | `			/*` |
|         - | 1846 | ``			 * A PLAIN variable (`unset($x)`, exactly two tokens: '$' and the name) drops a`` |
|         - | 1847 | `			 * single NAME binding, which the unset() builtin cannot express — all it ever` |
|         - | 1848 | `			 * receives is the value's slot index, and unsetting the slot destroys whatever` |
|         - | 1849 | `			 * else aliases it. Emit OP_UNSET_VAR with the name instead. Subscripts and` |
|         - | 1850 | ``			 * properties (`unset($a[k])`, `unset($o->p)`) keep the existing path, which`` |
|         - | 1851 | `			 * already removes just the element/property.` |
|         - | 1852 | `			 */` |
|     31690 | 1853 | `			if( &pGen->pIn[2] == pNext` |
|     19202 | 1854 | `				&& (pGen->pIn[0].nType & PH7_TK_DOLLAR)` |
|      6719 | 1855 | `				&& (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         - | 1856 | `				SyString *pVarName;` |
|     10073 | 1857 | `				char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      6712 | 1858 | `					pGen->pIn[1].sData.zString,pGen->pIn[1].sData.nByte);` |
|      6717 | 1859 | `				pVarName = (SyString *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(SyString));` |
|      6717 | 1860 | `				if( zDup == 0 \|\| pVarName == 0 ){` |
|       ! 0 | 1861 | `					PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - | 1862 | `						"Fatal, PH7 is running out of memory");` |
|       ! 0 | 1863 | `					return SXERR_ABORT;` |
|         - | 1864 | `				}` |
|      6717 | 1865 | `				SyStringInitFromBuf(pVarName,zDup,pGen->pIn[1].sData.nByte);` |
|      6717 | 1866 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_UNSET_VAR,0,0,pVarName,0);` |
|      6717 | 1867 | `				pGen->pIn = pNext;` |
|      6717 | 1868 | `				if( pGen->pIn < pEnd ){` |
|      3963 | 1869 | `					pGen->pIn++; /* Jump the trailing comma */` |
|      1979 | 1870 | `				}` |
|      6717 | 1871 | `				continue;` |
|         - | 1872 | `			}` |
|     24983 | 1873 | `			pGen->pEnd = pNext;` |
|     24983 | 1874 | `			rc = PH7_CompileExpr(&(*pGen),` |
|         - | 1875 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|         - | 1876 | `				GenStateUnsetValidator);` |
|     24983 | 1877 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 1878 | `				return SXERR_ABORT;` |
|         - | 1879 | `			}` |
|     24983 | 1880 | `			if( rc != SXERR_EMPTY ){` |
|         - | 1881 | `				/* Emit call for this single argument */` |
|     24981 | 1882 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     24981 | 1883 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|     24981 | 1884 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     12488 | 1885 | `			}` |
|     12489 | 1886 | `		}` |
|         - | 1887 | `		/* Jump trailing commas */` |
|     24999 | 1888 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|        18 | 1889 | `			pNext++;` |
|         2 | 1890 | `		}` |
|     24983 | 1891 | `		pGen->pIn = pNext;` |
|         5 | 1892 | `	}` |
|         - | 1893 | `	/* Skip past the closing ')' if present */` |
|     27723 | 1894 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|     27723 | 1895 | `		pGen->pIn++;` |
|     13859 | 1896 | `	}` |
|         - | 1897 | `	/* Restore token stream */` |
|     27723 | 1898 | `	pGen->pEnd = pTmp;` |
|     27723 | 1899 | `	return SXRET_OK;` |
|     13864 | 1900 | `}` |
|         - | 1901 | `/*` |
|         - | 1902 | ` * PHP Language construct table.` |
|         - | 1903 | ` */` |
|         - | 1904 | `static const LangConstruct aLangConstruct[] = {` |
|         - | 1905 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|         - | 1906 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|         - | 1907 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|         - | 1908 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|         - | 1909 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|         - | 1910 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|         - | 1911 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|         - | 1912 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|         - | 1913 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|         - | 1914 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|         - | 1915 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|         - | 1916 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|         - | 1917 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|         - | 1918 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|         - | 1919 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|         - | 1920 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|         - | 1921 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|         - | 1922 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|         - | 1923 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|         - | 1924 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|         - | 1925 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|         - | 1926 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|         - | 1927 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|         - | 1928 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|         - | 1929 | `};` |
|         - | 1930 | `/*` |
|         - | 1931 | ` * Return a pointer to the statement handler routine associated` |
|         - | 1932 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|         - | 1933 | ` */` |
|   9060792 | 1934 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|         - | 1935 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|         - | 1936 | `	SyToken *pLookahed  /* Look-ahead token */` |
|         - | 1937 | `	)` |
|         5 | 1938 | `{` |
|   9060797 | 1939 | `	sxu32 n = 0;` |
|  36659296 | 1940 | `	for(;;){` |
|  73318597 | 1941 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|    578391 | 1942 | `			break;` |
|         - | 1943 | `		}` |
|  72740211 | 1944 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|   8482411 | 1945 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|       ! 0 | 1946 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|       ! 0 | 1947 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|         - | 1948 | `					/* 'static' (class context),return null */` |
|       ! 0 | 1949 | `					return 0;` |
|         - | 1950 | `				}` |
|       ! 0 | 1951 | `			}` |
|   8482406 | 1952 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|     12436 | 1953 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|      6225 | 1954 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|         - | 1955 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|         3 | 1956 | `				return 0;` |
|         - | 1957 | `			}` |
|         - | 1958 | `			/* Return a pointer to the handler.` |
|         - | 1959 | `			*/` |
|   8482409 | 1960 | `			return aLangConstruct[n].xConstruct;` |
|         - | 1961 | `		}` |
|  64257805 | 1962 | `		n++;` |
|         5 | 1963 | `	}` |
|    578391 | 1964 | `	if( pLookahed ){` |
|    578391 | 1965 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|     74673 | 1966 | `			return PH7_CompileClassInterface;` |
|    503723 | 1967 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|    432653 | 1968 | `			return PH7_CompileClass;` |
|     71075 | 1969 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      8429 | 1970 | `			return PH7_CompileTrait;` |
|         - | 1971 | `		}` |
|         - | 1972 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|         - | 1973 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|         - | 1974 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|         - | 1975 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     31323 | 1976 | `	}` |
|         - | 1977 | `	/* Not a language construct */` |
|     62651 | 1978 | `	return 0;` |
|   4530401 | 1979 | `}` |
|         - | 1980 | `/*` |
|         - | 1981 | ` * Check if the given keyword is in fact a PHP language construct.` |
|         - | 1982 | ` * Return TRUE on success. FALSE otheriwse.` |
|         - | 1983 | ` */` |
|     62648 | 1984 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|         5 | 1985 | `{` |
|         - | 1986 | `	int rc;` |
|     62653 | 1987 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     62653 | 1988 | `	if( rc == FALSE ){` |
|     62516 | 1989 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|     16962 | 1990 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|         - | 1991 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|         - | 1992 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|         - | 1993 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|         - | 1994 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|         - | 1995 | `			*/` |
|         - | 1996 | `			){` |
|     62513 | 1997 | `				rc = TRUE;` |
|     31254 | 1998 | `		}` |
|     31258 | 1999 | `	}` |
|     62653 | 2000 | `	return rc;` |
|         5 | 2001 | `}` |
|         - | 2002 | `/*` |
|         - | 2003 | ` * Compile a PHP chunk.` |
|         - | 2004 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 2005 | ` * takes care of generating the appropriate error message.` |
|         - | 2006 | ` */` |
|         - | 2007 | `/*` |
|         - | 2008 | ` * Update pGen->sPendingDoc for the statement whose first token is` |
|         - | 2009 | ` * pGen->pIn: when a docblock trivia is keyed to that token's index in` |
|         - | 2010 | ` * the chunk token set it becomes the pending docblock. An existing` |
|         - | 2011 | ` * pending docblock is LEFT in place otherwise: Zend keeps the last-seen` |
|         - | 2012 | ` * doc comment until a declaration consumes it, so a docblock survives` |
|         - | 2013 | ` * intervening non-declaration statements.` |
|         - | 2014 | ` */` |
|  19419696 | 2015 | `PH7_PRIVATE void GenStateSetPendingDoc(ph7_gen_state *pGen)` |
|         5 | 2016 | `{` |
|  19419701 | 2017 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|  19419701 | 2018 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|  19419701 | 2019 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 2020 | `	sxu32 nIdx, n;` |
|  19419696 | 2021 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|   3679313 | 2022 | `	 \|\| pGen->pIn < pBase \|\| pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|         - | 2023 | `		/* Re-tokenized substream (string interpolation, synthesized code):` |
|         - | 2024 | `		 * indexes do not map to the sidecar */` |
|  15740395 | 2025 | `		return;` |
|         - | 2026 | `	}` |
|   3679311 | 2027 | `	nIdx = (sxu32)(pGen->pIn - pBase);` |
|         - | 2028 | `	/* Attributes must be adjacent to their declaration (unlike docblocks):` |
|         - | 2029 | `	 * reset at every boundary, then collect the groups keyed to this token. */` |
|   3679311 | 2030 | `	SySetReset(&pGen->aPendingAttrs);` |
|  11039683 | 2031 | `	for( n = 0 ; n < nT ; n++ ){` |
|   7360377 | 2032 | `		if( aT[n].nTokIdx != nIdx ){` |
|   7351921 | 2033 | `			continue;` |
|         - | 2034 | `		}` |
|      8461 | 2035 | `		if( aT[n].iKind == PH7_TRIVIA_DOC ){` |
|        29 | 2036 | `			pGen->sPendingDoc = aT[n].sText;` |
|      8449 | 2037 | `		}else if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|      8437 | 2038 | `			SySetPut(&pGen->aPendingAttrs,(const void *)&aT[n]);` |
|      4216 | 2039 | `		}` |
|      4233 | 2040 | `	}` |
|   9709853 | 2041 | `}` |
|         - | 2042 | `/*` |
|         - | 2043 | ` * Hand the pending docblock (if any) to a declaration: duplicate it into` |
|         - | 2044 | ` * the VM allocator (the raw script buffer dies after compilation) and` |
|         - | 2045 | ` * clear the pending slot so sibling declarations do not inherit it.` |
|         - | 2046 | ` */` |
|   5216740 | 2047 | `PH7_PRIVATE void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut)` |
|         5 | 2048 | `{` |
|         - | 2049 | `	char *zDup;` |
|   5216745 | 2050 | `	if( SyStringLength(&pGen->sPendingDoc) < 1 ){` |
|   5216725 | 2051 | `		return;` |
|         - | 2052 | `	}` |
|        35 | 2053 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        10 | 2054 | `		SyStringData(&pGen->sPendingDoc),SyStringLength(&pGen->sPendingDoc));` |
|        25 | 2055 | `	if( zDup ){` |
|        25 | 2056 | `		SyStringInitFromBuf(pOut,zDup,SyStringLength(&pGen->sPendingDoc));` |
|        10 | 2057 | `	}` |
|        25 | 2058 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|   2608375 | 2059 | `}` |
|         - | 2060 | `/*` |
|         - | 2061 | ` * Compile one recorded #[...] attribute group (the span between the group` |
|         - | 2062 | ` * delimiters) into ph7_attribute records appended to pOut. The span is` |
|         - | 2063 | ` * duplicated into the VM allocator FIRST (compiled bytecode and interned` |
|         - | 2064 | ` * names may point into the token text, which must outlive the raw script` |
|         - | 2065 | ` * buffer), then re-tokenized on its own. Each argument expression compiles` |
|         - | 2066 | ` * with the container-swap idiom into its own OP_DONE-terminated set,` |
|         - | 2067 | ` * evaluated lazily at ReflectionAttribute time (PHP semantics).` |
|         - | 2068 | ` */` |
|      8446 | 2069 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut)` |
|         5 | 2070 | `{` |
|         - | 2071 | `	SySet *pToken;` |
|         - | 2072 | `	SyToken *pIn, *pEnd, *pSavedIn, *pSavedEnd;` |
|         - | 2073 | `	char *zSpan;` |
|      8451 | 2074 | `	sxi32 rc = SXRET_OK;` |
|      8451 | 2075 | `	if( SyStringLength(&pTrivia->sText) < 1 ){` |
|       ! 0 | 2076 | `		return SXRET_OK;` |
|         - | 2077 | `	}` |
|     12674 | 2078 | `	zSpan = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      4223 | 2079 | `		SyStringData(&pTrivia->sText),SyStringLength(&pTrivia->sText));` |
|      8451 | 2080 | `	if( zSpan == 0 ){` |
|       ! 0 | 2081 | `		return SXRET_OK;` |
|         - | 2082 | `	}` |
|         - | 2083 | `	/* The token set must outlive compilation too: interned operands may` |
|         - | 2084 | `	 * reference token payloads. Pool-allocated, never released — bounded by` |
|         - | 2085 | `	 * the number of attribute declarations in the program. */` |
|      8451 | 2086 | `	pToken = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      8451 | 2087 | `	if( pToken == 0 ){` |
|       ! 0 | 2088 | `		return SXRET_OK;` |
|         - | 2089 | `	}` |
|      8451 | 2090 | `	SySetInit(pToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      8451 | 2091 | `	PH7_TokenizePHP(zSpan,SyStringLength(&pTrivia->sText),pTrivia->nLine,pToken,0);` |
|      8451 | 2092 | `	pIn = (SyToken *)SySetBasePtr(pToken);` |
|      8451 | 2093 | `	pEnd = &pIn[SySetUsed(pToken)];` |
|      8451 | 2094 | `	pSavedIn = pGen->pIn;` |
|      8451 | 2095 | `	pSavedEnd = pGen->pEnd;` |
|      8455 | 2096 | `	while( pIn < pEnd ){` |
|         - | 2097 | `		ph7_attribute sAttr;` |
|         - | 2098 | `		SyBlob sFQN;` |
|      8455 | 2099 | `		int bAbsolute = 0;` |
|      8455 | 2100 | `		SyZero(&sAttr,sizeof(sAttr));` |
|      8455 | 2101 | `		SySetInit(&sAttr.aArgs,&pGen->pVm->sAllocator,sizeof(ph7_attr_arg));` |
|      8455 | 2102 | `		sAttr.nLine = pIn->nLine;` |
|      8455 | 2103 | `		if( pIn->nType & PH7_TK_NSSEP ){` |
|        75 | 2104 | `			bAbsolute = 1;` |
|        75 | 2105 | `			pIn++;` |
|        35 | 2106 | `		}` |
|      8455 | 2107 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      8455 | 2108 | `		while( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      8455 | 2109 | `			SyBlobAppend(&sFQN,pIn->sData.zString,pIn->sData.nByte);` |
|      8455 | 2110 | `			pIn++;` |
|      8455 | 2111 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       ! 0 | 2112 | `				SyBlobAppend(&sFQN,"\\",1);` |
|       ! 0 | 2113 | `				pIn++;` |
|       ! 0 | 2114 | `				continue;` |
|         - | 2115 | `			}` |
|      8455 | 2116 | `			break;` |
|       ! 0 | 2117 | `		}` |
|      8455 | 2118 | `		if( SyBlobLength(&sFQN) < 1 ){` |
|         - | 2119 | `			/* Malformed group: stop quietly (the group was inert trivia before` |
|         - | 2120 | `			 * this feature; never turn it into a new fatal) */` |
|       ! 0 | 2121 | `			SyBlobRelease(&sFQN);` |
|       ! 0 | 2122 | `			break;` |
|         - | 2123 | `		}` |
|         - | 2124 | `		/* Resolve to an FQN: absolute names verbatim; else use-import alias,` |
|         - | 2125 | `		 * else current-namespace prefix (PHP attribute name resolution) */` |
|         - | 2126 | `		{` |
|      8455 | 2127 | `			const char *zName = (const char *)SyBlobData(&sFQN);` |
|      8455 | 2128 | `			sxu32 nName = SyBlobLength(&sFQN);` |
|      8455 | 2129 | `			char *zDup = 0;` |
|      8455 | 2130 | `			if( !bAbsolute ){` |
|      8385 | 2131 | `				SyHashEntry *pImp = SyHashGet(&pGen->hUseImports,(const void *)zName,nName);` |
|      8385 | 2132 | `				if( pImp ){` |
|       ! 0 | 2133 | `					const char *zFqn = (const char *)pImp->pUserData;` |
|       ! 0 | 2134 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFqn,SyStrlen(zFqn));` |
|       ! 0 | 2135 | `					if( zDup ){` |
|       ! 0 | 2136 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyStrlen(zDup));` |
|       ! 0 | 2137 | `					}` |
|      8385 | 2138 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         - | 2139 | `					SyBlob sTmp;` |
|       ! 0 | 2140 | `					SyBlobInit(&sTmp,&pGen->pVm->sAllocator);` |
|       ! 0 | 2141 | `					SyBlobAppend(&sTmp,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       ! 0 | 2142 | `					SyBlobAppend(&sTmp,"\\",1);` |
|       ! 0 | 2143 | `					SyBlobAppend(&sTmp,zName,nName);` |
|       ! 0 | 2144 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       ! 0 | 2145 | `						(const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|       ! 0 | 2146 | `					if( zDup ){` |
|       ! 0 | 2147 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyBlobLength(&sTmp));` |
|       ! 0 | 2148 | `					}` |
|       ! 0 | 2149 | `					SyBlobRelease(&sTmp);` |
|       ! 0 | 2150 | `				}` |
|      4190 | 2151 | `			}` |
|      8455 | 2152 | `			if( SyStringLength(&sAttr.sName) < 1 ){` |
|      8455 | 2153 | `				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|      8455 | 2154 | `				if( zDup ){` |
|      8455 | 2155 | `					SyStringInitFromBuf(&sAttr.sName,zDup,nName);` |
|      4225 | 2156 | `				}` |
|      4225 | 2157 | `			}` |
|         - | 2158 | `		}` |
|      8455 | 2159 | `		SyBlobRelease(&sFQN);` |
|      8455 | 2160 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 2161 | `			SyToken *pArgsEnd;` |
|      8349 | 2162 | `			pIn++;` |
|      8349 | 2163 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pArgsEnd);` |
|     16707 | 2164 | `			while( pIn < pArgsEnd ){` |
|      8363 | 2165 | `				SyToken *pArgStart = pIn, *pArgStop = pIn;` |
|      8363 | 2166 | `				sxi32 iDepth = 0;` |
|         - | 2167 | `				ph7_attr_arg sArgRec;` |
|     83077 | 2168 | `				while( pArgStop < pArgsEnd ){` |
|     74735 | 2169 | `					if( pArgStop->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        11 | 2170 | `						iDepth++;` |
|     74730 | 2171 | `					}else if( pArgStop->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        11 | 2172 | `						iDepth--;` |
|     74720 | 2173 | `					}else if( (pArgStop->nType & PH7_TK_COMMA) && iDepth == 0 ){` |
|        17 | 2174 | `						break;` |
|         - | 2175 | `					}` |
|     74719 | 2176 | `					pArgStop++;` |
|         5 | 2177 | `				}` |
|      8363 | 2178 | `				SyZero(&sArgRec,sizeof(sArgRec));` |
|      8363 | 2179 | `				SySetInit(&sArgRec.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      8358 | 2180 | `				if( pArgStart < pArgStop && (pArgStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|      8340 | 2181 | `				 && &pArgStart[1] < pArgStop && (pArgStart[1].nType & PH7_TK_COLON) ){` |
|        28 | 2182 | `					char *zN = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|         9 | 2183 | `						pArgStart->sData.zString,pArgStart->sData.nByte);` |
|        19 | 2184 | `					if( zN ){` |
|        19 | 2185 | `						SyStringInitFromBuf(&sArgRec.sName,zN,pArgStart->sData.nByte);` |
|         9 | 2186 | `					}` |
|        19 | 2187 | `					pArgStart += 2;` |
|         9 | 2188 | `				}` |
|      8363 | 2189 | `				if( pArgStart < pArgStop ){` |
|         - | 2190 | `					SySet *pInstrContainer;` |
|      8363 | 2191 | `					pGen->pIn = pArgStart;` |
|      8363 | 2192 | `					pGen->pEnd = pArgStop;` |
|      8363 | 2193 | `					pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      8363 | 2194 | `					PH7_VmSetByteCodeContainer(pGen->pVm,&sArgRec.aByteCode);` |
|      8363 | 2195 | `					rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      8363 | 2196 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      8363 | 2197 | `					PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      8363 | 2198 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 2199 | `						pGen->pIn = pSavedIn;` |
|       ! 0 | 2200 | `						pGen->pEnd = pSavedEnd;` |
|       ! 0 | 2201 | `						return SXERR_ABORT;` |
|         - | 2202 | `					}` |
|      8363 | 2203 | `					SySetPut(&sAttr.aArgs,(const void *)&sArgRec);` |
|      4179 | 2204 | `				}` |
|      8363 | 2205 | `				pIn = pArgStop;` |
|      8363 | 2206 | `				if( pIn < pArgsEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        17 | 2207 | `					pIn++;` |
|         8 | 2208 | `				}` |
|         5 | 2209 | `			}` |
|      8349 | 2210 | `			pIn = (pArgsEnd < pEnd) ? &pArgsEnd[1] : pEnd;` |
|      4172 | 2211 | `		}` |
|      8455 | 2212 | `		SySetPut(pOut,(const void *)&sAttr);` |
|      8455 | 2213 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|         5 | 2214 | `			pIn++;` |
|         5 | 2215 | `			continue;` |
|         - | 2216 | `		}` |
|      8451 | 2217 | `		break;` |
|       ! 0 | 2218 | `	}` |
|      8451 | 2219 | `	pGen->pIn = pSavedIn;` |
|      8451 | 2220 | `	pGen->pEnd = pSavedEnd;` |
|      8451 | 2221 | `	return SXRET_OK;` |
|      4228 | 2222 | `}` |
|         - | 2223 | `/*` |
|         - | 2224 | ` * Hand the pending attribute groups (if any) to a declaration: compile` |
|         - | 2225 | ` * every recorded group into pOut and clear the pending list.` |
|         - | 2226 | ` */` |
|   5216746 | 2227 | `PH7_PRIVATE sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut)` |
|         5 | 2228 | `{` |
|   5216751 | 2229 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|         - | 2230 | `	sxu32 n;` |
|         - | 2231 | `	sxi32 rc;` |
|   5225183 | 2232 | `	for( n = 0 ; n < SySetUsed(&pGen->aPendingAttrs) ; n++ ){` |
|      8437 | 2233 | `		rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|      8437 | 2234 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 2235 | `			return SXERR_ABORT;` |
|         - | 2236 | `		}` |
|      4221 | 2237 | `	}` |
|   5216751 | 2238 | `	SySetReset(&pGen->aPendingAttrs);` |
|   5216751 | 2239 | `	return SXRET_OK;` |
|   2608378 | 2240 | `}` |
|         - | 2241 | `/*` |
|         - | 2242 | ` * Compile the attribute groups keyed to the given token (a parameter's` |
|         - | 2243 | ` * first token inside a signature) into pOut. Parameters are parsed from` |
|         - | 2244 | ` * the main token stream, so the sidecar indexes map directly.` |
|         - | 2245 | ` */` |
|   2621032 | 2246 | `PH7_PRIVATE sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut)` |
|         5 | 2247 | `{` |
|   2621037 | 2248 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|   2621037 | 2249 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|   2621037 | 2250 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 2251 | `	sxu32 nIdx, n;` |
|         - | 2252 | `	sxi32 rc;` |
|   2621032 | 2253 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|    588197 | 2254 | `	 \|\| pTok < pBase \|\| pTok >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|   2032845 | 2255 | `		return SXRET_OK;` |
|         - | 2256 | `	}` |
|    588197 | 2257 | `	nIdx = (sxu32)(pTok - pBase);` |
|   1764625 | 2258 | `	for( n = 0 ; n < nT ; n++ ){` |
|   1176433 | 2259 | `		if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|        16 | 2260 | `			rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|        16 | 2261 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 2262 | `				return SXERR_ABORT;` |
|         - | 2263 | `			}` |
|         7 | 2264 | `		}` |
|    588219 | 2265 | `	}` |
|    588197 | 2266 | `	return SXRET_OK;` |
|   1310521 | 2267 | `}` |
|  14255202 | 2268 | `PH7_PRIVATE sxi32 GenStateCompileChunk(` |
|         - | 2269 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 2270 | `	sxi32 iFlags         /* Compile flags */` |
|         - | 2271 | `	)` |
|         5 | 2272 | `{` |
|         - | 2273 | `	ProcLangConstruct xCons;` |
|         - | 2274 | `	sxi32 rc;` |
|  14255207 | 2275 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|   8290865 | 2276 | `	for(;;){` |
|  15418471 | 2277 | `		int bStmtIsDeclare = 0;` |
|  15418471 | 2278 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 2279 | `			/* No more input to process */` |
|     97027 | 2280 | `			break;` |
|         - | 2281 | `		}` |
|         - | 2282 | `		/* Bind a directly-preceding docblock to this statement */` |
|  15321449 | 2283 | `		GenStateSetPendingDoc(&(*pGen));` |
|  15321449 | 2284 | `		if( SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|         - | 2285 | `			/* php: a statement-position attribute group must be followed by a` |
|         - | 2286 | ``			 * declaration (function/class-like/const) — `#[A] $x = 1;` is a`` |
|         - | 2287 | `` 			 * parse error, never a silent discard. `static`/`fn`/`function` `` |
|         - | 2288 | ``			 * cover bare closure-expression statements; `readonly`/`enum` are`` |
|         - | 2289 | `			 * context-sensitive IDs handled by the modified-class/enum scans. */` |
|      8349 | 2290 | `			int bAttrTarget = 0;` |
|      8344 | 2291 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd)` |
|      4209 | 2292 | `			 \|\| GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|      8285 | 2293 | `				bAttrTarget = 1;` |
|      4206 | 2294 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|        66 | 2295 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        64 | 2296 | `				if( nKw == PH7_TKWRD_FUNCTION \|\| nKw == PH7_TKWRD_CLASS` |
|        19 | 2297 | `				 \|\| nKw == PH7_TKWRD_INTERFACE \|\| nKw == PH7_TKWRD_TRAIT` |
|         6 | 2298 | `				 \|\| nKw == PH7_TKWRD_ABSTRACT \|\| nKw == PH7_TKWRD_FINAL` |
|         6 | 2299 | `				 \|\| nKw == PH7_TKWRD_CONST \|\| nKw == PH7_TKWRD_STATIC` |
|         2 | 2300 | `				 \|\| nKw == PH7_TKWRD_FN ){` |
|        66 | 2301 | `					bAttrTarget = 1;` |
|        32 | 2302 | `				}` |
|        32 | 2303 | `			}` |
|      8349 | 2304 | `			if( !bAttrTarget ){` |
|       ! 0 | 2305 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 2306 | `					"syntax error, unexpected token \"%z\" after attribute group; expecting a declaration",` |
|       ! 0 | 2307 | `					&pGen->pIn->sData);` |
|       ! 0 | 2308 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 2309 | `					break;` |
|         - | 2310 | `				}` |
|       ! 0 | 2311 | `				SySetReset(&pGen->aPendingAttrs);` |
|       ! 0 | 2312 | `			}` |
|      4172 | 2313 | `		}` |
|         - | 2314 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|         - | 2315 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  15321449 | 2316 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   9106419 | 2317 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   9106419 | 2318 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|        53 | 2319 | `				bStmtIsDeclare = 1;` |
|        24 | 2320 | `			}` |
|   4553207 | 2321 | `		}` |
|  15321449 | 2322 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|         - | 2323 | `			/* Any non-declare top-level statement locks the strict_types` |
|         - | 2324 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|   1163261 | 2325 | `			pGen->bStrictTypesLocked = 1;` |
|    581628 | 2326 | `		}` |
|  15321449 | 2327 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 2328 | `			/* Compile block */` |
|      4189 | 2329 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      4189 | 2330 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 2331 | `				break;` |
|         - | 2332 | `			}` |
|      2097 | 2333 | `		}else{` |
|  15317265 | 2334 | `			xCons = 0;` |
|  15317265 | 2335 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|         - | 2336 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|         - | 2337 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|         - | 2338 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|     45653 | 2339 | `				xCons = PH7_CompileClassModifiers;` |
|  15294441 | 2340 | `			}else if( GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|         - | 2341 | ``				/* `enum Name …` (PHP 8.1) — `enum` is a context-sensitive ID,`` |
|         - | 2342 | `				 * so it is detected here rather than the keyword dispatcher. */` |
|      4195 | 2343 | `				xCons = PH7_CompileEnum;` |
|  15269522 | 2344 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   9060797 | 2345 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         - | 2346 | `				/* Try to extract a language construct handler */` |
|   9060797 | 2347 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|   9060797 | 2348 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|        13 | 2349 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 2350 | `						"Syntax error: Unexpected keyword '%z'",` |
|         8 | 2351 | `						&pGen->pIn->sData);` |
|         9 | 2352 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 2353 | `						break;` |
|         - | 2354 | `					}` |
|         - | 2355 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|         - | 2356 | `					 * this erroneous statement.` |
|         - | 2357 | `					 */` |
|         9 | 2358 | `					xCons = PH7_ErrorRecover;` |
|         4 | 2359 | `				}` |
|  10737031 | 2360 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|    466431 | 2361 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|         - | 2362 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|       117 | 2363 | `				xCons = PH7_CompileLabel;` |
|        56 | 2364 | `			}` |
|  15317265 | 2365 | `			if( xCons == 0 ){` |
|         - | 2366 | `				/* Assume an expression an try to compile it */` |
|   6269163 | 2367 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   6269163 | 2368 | `				if(  rc != SXERR_EMPTY ){` |
|         - | 2369 | `					/* Pop l-value */` |
|   6269005 | 2370 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|   3134500 | 2371 | `				}` |
|   3134584 | 2372 | `			}else{` |
|         - | 2373 | `				/* Go compile the sucker */` |
|   9048107 | 2374 | `				rc = xCons(&(*pGen));` |
|         - | 2375 | `			}` |
|  15317265 | 2376 | `			if( rc == SXERR_ABORT ){` |
|         - | 2377 | `				/* Request to abort compilation */` |
|        44 | 2378 | `				break;` |
|         - | 2379 | `			}` |
|         - | 2380 | `		}` |
|         - | 2381 | `		/* Ignore trailing semi-colons ';' */` |
|  26306935 | 2382 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  10985531 | 2383 | `			pGen->pIn++;` |
|         5 | 2384 | `		}` |
|  15321409 | 2385 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|         - | 2386 | `			/* Compile a single statement and return */` |
|  14158145 | 2387 | `			break;` |
|         - | 2388 | `		}` |
|         - | 2389 | `		/* LOOP ONE */` |
|         - | 2390 | `		/* LOOP TWO */` |
|         - | 2391 | `		/* LOOP THREE */` |
|         - | 2392 | `		/* LOOP FOUR */` |
|         5 | 2393 | `	}` |
|         - | 2394 | `	/* Return compilation status */` |
|  14255207 | 2395 | `	return rc;` |
|         5 | 2396 | `}` |
|         - | 2397 | `/*` |
|         - | 2398 | ` * Compile a Raw PHP chunk.` |
|         - | 2399 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 2400 | ` * takes care of generating the appropriate error message.` |
|         - | 2401 | ` */` |
|     97064 | 2402 | `static sxi32 PH7_CompilePHP(` |
|         - | 2403 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 2404 | `	SySet *pTokenSet,     /* Token set */` |
|         - | 2405 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|         - | 2406 | `	)` |
|         5 | 2407 | `{` |
|     97069 | 2408 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|         - | 2409 | `	sxi32 rc;` |
|         - | 2410 | `	/* Reset the token set (and its trivia sidecar) */` |
|     97069 | 2411 | `	SySetReset(&(*pTokenSet));` |
|     97069 | 2412 | `	SySetReset(&pGen->aTrivia);` |
|         - | 2413 | `	/* Mark as the default token set */` |
|     97069 | 2414 | `	pGen->pTokenSet = &(*pTokenSet);` |
|         - | 2415 | `	/* Advance the stream cursor */` |
|     97069 | 2416 | `	pGen->pRawIn++;` |
|         - | 2417 | `	/* Tokenize the PHP chunk first */` |
|     97069 | 2418 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet),&pGen->aTrivia);` |
|         - | 2419 | `	/* Point to the head and tail of the token stream. */` |
|     97069 | 2420 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     97069 | 2421 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|     97069 | 2422 | `	if( is_expr ){` |
|       ! 0 | 2423 | `		rc = SXERR_EMPTY;` |
|       ! 0 | 2424 | `		if( pGen->pIn < pGen->pEnd ){` |
|         - | 2425 | `			/* A simple expression,compile it */` |
|       ! 0 | 2426 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|       ! 0 | 2427 | `		}` |
|         - | 2428 | `		/* Emit the DONE instruction */` |
|       ! 0 | 2429 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       ! 0 | 2430 | `		return SXRET_OK;` |
|         - | 2431 | `	}` |
|     97069 | 2432 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|         - | 2433 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|         - | 2434 | `		/*` |
|         - | 2435 | `		 * Shortcut syntax for the 'echo' language construct.` |
|         - | 2436 | `		 * According to the PHP reference manual:` |
|         - | 2437 | `		 *  echo() also has a shortcut syntax, where you can` |
|         - | 2438 | `		 *  immediately follow` |
|         - | 2439 | `		 *  the opening tag with an equals sign as follows:` |
|         - | 2440 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|         - | 2441 | `		 * Symisc extension:` |
|         - | 2442 | `		 *   This short syntax works with all PHP opening` |
|         - | 2443 | `		 *   tags unlike the default PHP engine that handle` |
|         - | 2444 | `		 *   only short tag.` |
|         - | 2445 | `		 */` |
|         - | 2446 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|         3 | 2447 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|         3 | 2448 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|         3 | 2449 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|         - | 2450 | `		/* This synthesized echo is compiled as an EXPRESSION, which is otherwise a` |
|         - | 2451 | `		 * parse error; allow it for the duration of this one compile. */` |
|         3 | 2452 | `		pGen->nExprEchoOk++;` |
|         3 | 2453 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|         3 | 2454 | `		pGen->nExprEchoOk--;` |
|         3 | 2455 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 2456 | `			return SXERR_ABORT;` |
|         - | 2457 | `		}` |
|         3 | 2458 | `		if( rc != SXERR_EMPTY ){` |
|         3 | 2459 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         1 | 2460 | `		}` |
|         3 | 2461 | `		return SXRET_OK;` |
|         - | 2462 | `	}` |
|         - | 2463 | `	/* Compile the PHP chunk */` |
|     97067 | 2464 | `	rc = GenStateCompileChunk(pGen,0);` |
|         - | 2465 | `	/* Fix exceptions jumps */` |
|     97067 | 2466 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - | 2467 | `	/* Fix gotos now, the jump destination is resolved */` |
|     97067 | 2468 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|         3 | 2469 | `		rc = SXERR_ABORT;` |
|         1 | 2470 | `	}` |
|         - | 2471 | `	/* Reset container */` |
|     97067 | 2472 | `	SySetReset(&pGen->aGoto);` |
|     97067 | 2473 | `	SySetReset(&pGen->aLabel);` |
|     97067 | 2474 | `	SySetReset(&pGen->aNullsafeJmp);` |
|         - | 2475 | `	/* Compilation result */` |
|     97067 | 2476 | `	return rc;` |
|     48537 | 2477 | `}` |
|         - | 2478 | `/*` |
|         - | 2479 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|         - | 2480 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|         - | 2481 | ` * This is the only compile interface exported from this file.` |
|         - | 2482 | ` */` |
|    100404 | 2483 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|         - | 2484 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|         - | 2485 | `	SyString *pScript,  /* Script to compile */` |
|         - | 2486 | `	sxi32 iFlags        /* Compile flags */` |
|         - | 2487 | `	)` |
|         5 | 2488 | `{` |
|         - | 2489 | `	SySet aPhpToken,aRawToken;` |
|         - | 2490 | `	ph7_gen_state *pCodeGen;` |
|         - | 2491 | `	ph7_value *pRawObj;` |
|         - | 2492 | `	sxu32 nObjIdx;` |
|         - | 2493 | `	sxi32 nRawObj;` |
|         - | 2494 | `	int is_expr;` |
|         - | 2495 | `	sxi8 bSavedStrict;` |
|         - | 2496 | `	sxi8 bSavedStrictLocked;` |
|         - | 2497 | `	SyToken *pSavedIn,*pSavedEnd;` |
|         - | 2498 | `	sxi32 rc;` |
|    100409 | 2499 | `	sxu32 nBaseLine = 1;` |
|    100409 | 2500 | `	if( pScript->nByte < 1 ){` |
|         - | 2501 | `		/* Nothing to compile */` |
|       ! 0 | 2502 | `		return PH7_OK;` |
|         - | 2503 | `	}` |
|         - | 2504 | `	/* php skips a "#!" shebang on the first line of a CLI script: consume it` |
|         - | 2505 | `	 * (including its newline) so it is not echoed as inline text, and bump the` |
|         - | 2506 | `	 * base line to 2 so the code below still reports php-matching line numbers. */` |
|    100409 | 2507 | `	if( pScript->nByte >= 2 && pScript->zString[0]=='#' && pScript->zString[1]=='!' ){` |
|         3 | 2508 | `		const char *z = pScript->zString;` |
|         3 | 2509 | `		const char *zEnd = &z[pScript->nByte];` |
|        39 | 2510 | `		while( z < zEnd && z[0] != '\n' ){ z++; }` |
|         3 | 2511 | `		if( z < zEnd ){ z++; } /* consume the newline too */` |
|         3 | 2512 | `		pScript->nByte -= (sxu32)(z - pScript->zString);` |
|         3 | 2513 | `		pScript->zString = z;` |
|         3 | 2514 | `		nBaseLine = 2;` |
|         3 | 2515 | `		if( pScript->nByte < 1 ){` |
|       ! 0 | 2516 | `			return PH7_OK;` |
|         - | 2517 | `		}` |
|         1 | 2518 | `	}` |
|         - | 2519 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|         - | 2520 | `	 * file's flags so include/require restore them on return. */` |
|    100409 | 2521 | `	pCodeGen = &pVm->sCodeGen;` |
|         - | 2522 | ``	/* aPhpToken below is a LOCAL token set: once it is released at `cleanup`, any`` |
|         - | 2523 | `	 * pGen->pIn/pEnd still pointing into it DANGLE. PH7_VmEmitInstr reads pIn to stamp` |
|         - | 2524 | `	 * each instruction's source line, and instructions are still emitted after this` |
|         - | 2525 | `	 * function returns (VmEvalChunk's trailing OP_DONE) -- a use-after-free ASan caught` |
|         - | 2526 | `	 * immediately. Save the caller's cursor and restore it on the way out, so an` |
|         - | 2527 | `	 * enclosing compile keeps its (live) tokens and the top level goes back to NULL. */` |
|    100409 | 2528 | `	pSavedIn = pCodeGen->pIn;` |
|    100409 | 2529 | `	pSavedEnd = pCodeGen->pEnd;` |
|    100409 | 2530 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|    100409 | 2531 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|    100409 | 2532 | `	pCodeGen->bStrictTypes = 0;` |
|    100409 | 2533 | `	pCodeGen->bStrictTypesLocked = 0;` |
|         - | 2534 | `	/* Initialize the tokens containers */` |
|    100409 | 2535 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|    100409 | 2536 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|    100409 | 2537 | `	SySetAlloc(&aPhpToken,0xc0);` |
|    100409 | 2538 | `	is_expr = 0;` |
|    100409 | 2539 | `	if( iFlags & PH7_PHP_ONLY ){` |
|         - | 2540 | `		SyToken sTmp;` |
|         - | 2541 | `		/* PHP only: -*/` |
|     87099 | 2542 | `		sTmp.nLine = 1;` |
|     87099 | 2543 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|     87099 | 2544 | `		sTmp.pUserData = 0;` |
|     87099 | 2545 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|     87099 | 2546 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|     87099 | 2547 | `		if( iFlags & PH7_PHP_EXPR ){` |
|         - | 2548 | `			/* A simple PHP expression */` |
|       ! 0 | 2549 | `			is_expr = 1;` |
|       ! 0 | 2550 | `		}` |
|     43552 | 2551 | `	}else{` |
|         - | 2552 | `		/* Tokenize raw text */` |
|     13315 | 2553 | `		SySetAlloc(&aRawToken,32);` |
|     13315 | 2554 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken,nBaseLine);` |
|         - | 2555 | `	}` |
|         - | 2556 | `	/* Process high-level tokens */` |
|    100409 | 2557 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|    100409 | 2558 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|    100409 | 2559 | `	rc = PH7_OK;` |
|    100409 | 2560 | `	if( is_expr ){` |
|         - | 2561 | `		/* Compile the expression */` |
|       ! 0 | 2562 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|       ! 0 | 2563 | `		goto cleanup;` |
|         - | 2564 | `	}` |
|    100409 | 2565 | `	nObjIdx = 0;` |
|         - | 2566 | `	/* Each compilation unit starts in the global namespace.` |
|         - | 2567 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|         - | 2568 | `	 * preventing namespace bleeding across include()d files. */` |
|    100409 | 2569 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|         - | 2570 | `	/* Start the compilation process */` |
|     56861 | 2571 | `	for(;;){` |
|    210749 | 2572 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|    100367 | 2573 | `			break; /* No more tokens to process */` |
|         - | 2574 | `		}` |
|    110387 | 2575 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|         - | 2576 | `			/* Compile the PHP chunk */` |
|     97069 | 2577 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|     97069 | 2578 | `			if( rc == SXERR_ABORT ){` |
|        46 | 2579 | `				break;` |
|         - | 2580 | `			}` |
|     97027 | 2581 | `			continue;` |
|         - | 2582 | `		}` |
|         - | 2583 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|     13323 | 2584 | `		nRawObj = 0;` |
|     26641 | 2585 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|         - | 2586 | `			/* Consume the raw chunk without any processing */` |
|     13323 | 2587 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|     13323 | 2588 | `			if( pRawObj == 0 ){` |
|       ! 0 | 2589 | `				rc = SXERR_MEM;` |
|       ! 0 | 2590 | `				break;` |
|         - | 2591 | `			}` |
|         - | 2592 | `			/* Mark as constant and emit the load constant instruction */` |
|     13323 | 2593 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|     13323 | 2594 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     13323 | 2595 | `			++nRawObj;` |
|     13323 | 2596 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|         5 | 2597 | `		}` |
|     13323 | 2598 | `		if( nRawObj > 0 ){` |
|         - | 2599 | `			/* Emit the consume instruction */` |
|     13323 | 2600 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|      6659 | 2601 | `		}` |
|     50207 | 2602 | `	}` |
|     50202 | 2603 | `cleanup:` |
|         - | 2604 | `	/* Drop the cursor into the token set BEFORE the set is freed (see above). */` |
|    100409 | 2605 | `	pCodeGen->pIn = pSavedIn;` |
|    100409 | 2606 | `	pCodeGen->pEnd = pSavedEnd;` |
|    100409 | 2607 | `	SySetRelease(&aRawToken);` |
|    100409 | 2608 | `	SySetRelease(&aPhpToken);` |
|         - | 2609 | `	/* Restore outer file's strict_types scope */` |
|    100409 | 2610 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|    100409 | 2611 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|    100409 | 2612 | `	return rc;` |
|     50207 | 2613 | `}` |
|         - | 2614 | `/*` |
|         - | 2615 | ` * Utility routines.Initialize the code generator.` |
|         - | 2616 | ` */` |
|      4140 | 2617 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|         - | 2618 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 2619 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 2620 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 2621 | `	)` |
|         5 | 2622 | `{` |
|      4145 | 2623 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 2624 | `	/* Zero the structure */` |
|      4145 | 2625 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|         - | 2626 | `	/* Initial state */` |
|      4145 | 2627 | `	pGen->pVm  = &(*pVm);` |
|      4145 | 2628 | `	pGen->xErr = xErr;` |
|      4145 | 2629 | `	pGen->pErrData = pErrData;` |
|      4145 | 2630 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|      4145 | 2631 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|      4145 | 2632 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|      4145 | 2633 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|      4145 | 2634 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|      4145 | 2635 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      4145 | 2636 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      4145 | 2637 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|      4145 | 2638 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|         - | 2639 | `	/* Error log buffer */` |
|      4145 | 2640 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|         - | 2641 | `	/* General purpose working buffer */` |
|      4145 | 2642 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|         - | 2643 | `	/* Namespace state */` |
|      4145 | 2644 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|      4145 | 2645 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|      4145 | 2646 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|      4145 | 2647 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 2648 | `	/* Create the global scope */` |
|      4145 | 2649 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|         - | 2650 | `	/* Point to the global scope */` |
|      4145 | 2651 | `	pGen->pCurrent = &pGen->sGlobal;` |
|      4145 | 2652 | `	return SXRET_OK;` |
|         5 | 2653 | `}` |
|         - | 2654 | `/*` |
|         - | 2655 | ` * Utility routines. Reset the code generator to it's initial state.` |
|         - | 2656 | ` */` |
|    104046 | 2657 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|         - | 2658 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 2659 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 2660 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 2661 | `	)` |
|         5 | 2662 | `{` |
|    104051 | 2663 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 2664 | `	GenBlock *pBlock,*pParent;` |
|         - | 2665 | `	/* Reset state */` |
|    104051 | 2666 | `	SySetReset(&pGen->aLabel);` |
|    104051 | 2667 | `	SySetReset(&pGen->aGoto);` |
|    104051 | 2668 | `	SySetReset(&pGen->aNullsafeJmp);` |
|    104051 | 2669 | `	SySetReset(&pGen->aTrivia);` |
|    104051 | 2670 | `	SySetReset(&pGen->aPendingAttrs);` |
|    104051 | 2671 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|    104051 | 2672 | `	SyBlobRelease(&pGen->sErrBuf);` |
|    104051 | 2673 | `	SyBlobRelease(&pGen->sWorker);` |
|    104051 | 2674 | `	SyBlobRelease(&pGen->sNamespace);` |
|    104051 | 2675 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    104051 | 2676 | `	SyHashRelease(&pGen->hUseImports);` |
|    104051 | 2677 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|    104051 | 2678 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|    104051 | 2679 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|    104051 | 2680 | `	SyHashRelease(&pGen->hUseConstImports);` |
|    104051 | 2681 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 2682 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|         - | 2683 | `	 * They intern variable names and literal strings that are referenced by` |
|         - | 2684 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|         - | 2685 | `	 * Releasing them would either leak the interned strings or require freeing` |
|         - | 2686 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|         - | 2687 | `	 * number of unique names, which is acceptable. */` |
|         - | 2688 | `	/* Point to the global scope */` |
|    104051 | 2689 | `	pBlock = pGen->pCurrent;` |
|    104051 | 2690 | `	while( pBlock->pParent != 0 ){` |
|       ! 0 | 2691 | `		pParent = pBlock->pParent;` |
|       ! 0 | 2692 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 2693 | `		pBlock = pParent;` |
|       ! 0 | 2694 | `	}` |
|    104051 | 2695 | `	pGen->xErr = xErr;` |
|    104051 | 2696 | `	pGen->pErrData = pErrData;` |
|    104051 | 2697 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    104051 | 2698 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|    104051 | 2699 | `	pGen->pIn = pGen->pEnd = 0;` |
|    104051 | 2700 | `	pGen->nErr = 0;` |
|         - | 2701 | `	/* Clear the class-body context (a prior compile aborted mid-class-body would` |
|         - | 2702 | `	 * otherwise leave these live for the next eval/include on this VM). */` |
|    104051 | 2703 | `	pGen->pCurClass = 0;` |
|    104051 | 2704 | `	pGen->iInMemberDefault = 0;` |
|    104051 | 2705 | `	return SXRET_OK;` |
|         5 | 2706 | `}` |
|         - | 2707 | `/*` |
|         - | 2708 | ` * Save the code generator's compile-position state and hand the live generator a` |
|         - | 2709 | ` * fresh, empty one for a NESTED compilation unit.` |
|         - | 2710 | ` *` |
|         - | 2711 | ` * A require/include normally runs at execution time, when no compile is in flight,` |
|         - | 2712 | ` * so VmEvalChunk can call PH7_ResetCodeGenerator to wipe the generator. But an` |
|         - | 2713 | `` * autoload can fire in the MIDDLE of compiling a class: resolving `class Child`` |
|         - | 2714 | `` * extends Base` calls PH7_VmExtractClass, which triggers the autoloader, whose`` |
|         - | 2715 | `` * body `require`s Base's file — a nested compile while the outer one is mid-token.`` |
|         - | 2716 | ` * PH7_ResetCodeGenerator would then blow away the outer compile's cursor` |
|         - | 2717 | ` * (pIn/pEnd), current block, use-imports, labels, etc.; the outer parse resumes` |
|         - | 2718 | ` * with pIn == NULL and reports a bogus "Expected '{' after class 'Child'". (Common` |
|         - | 2719 | ` * in every real framework: Composer autoloads parent classes on demand.)` |
|         - | 2720 | ` *` |
|         - | 2721 | ` * This snapshots the position/scope fields into *pSaved (a caller-stack` |
|         - | 2722 | ` * ph7_gen_state used purely as storage) and re-initializes the live generator's` |
|         - | 2723 | ` * position containers to FRESH, empty ones WITHOUT releasing the outer's (the` |
|         - | 2724 | ` * snapshot now owns those). hLiteral/hNumLiteral/hVar are intentionally left LIVE` |
|         - | 2725 | ` * and shared — compiled bytecode interns names/literals into them and they grow` |
|         - | 2726 | ` * monotonically (exactly why PH7_ResetCodeGenerator keeps them); restoring an old` |
|         - | 2727 | ` * copy of those headers after a nested grow would use-after-free the bucket array.` |
|         - | 2728 | ` */` |
|         4 | 2729 | `PH7_PRIVATE void PH7_CompilerSaveState(ph7_vm *pVm,ph7_gen_state *pSaved,ProcConsumer xErr,void *pErrData)` |
|         1 | 2730 | `{` |
|         5 | 2731 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 2732 | `	/* Shallow-copy every field; the position containers below are then replaced` |
|         - | 2733 | `	 * with fresh ones on the live struct, so *pSaved keeps the outer's memory. */` |
|         5 | 2734 | `	*pSaved = *pGen;` |
|         5 | 2735 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|         5 | 2736 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|         5 | 2737 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|         5 | 2738 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|         5 | 2739 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|         5 | 2740 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|         5 | 2741 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|         5 | 2742 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|         5 | 2743 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|         5 | 2744 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|         5 | 2745 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|         5 | 2746 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 2747 | `	/* Fresh global scope for the nested unit (address of the embedded sGlobal is` |
|         - | 2748 | `	 * stable, so any outer block still parented to it stays valid across restore). */` |
|         5 | 2749 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(pVm),0);` |
|         5 | 2750 | `	pGen->pCurrent = &pGen->sGlobal;` |
|         5 | 2751 | `	pGen->pIn = pGen->pEnd = 0;` |
|         5 | 2752 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|         5 | 2753 | `	pGen->pTokenSet = 0;` |
|         5 | 2754 | `	pGen->nErr = 0;` |
|         5 | 2755 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|         5 | 2756 | `	pGen->nCommaExprOk = 0;` |
|         5 | 2757 | `	pGen->zClauseCloser = 0;` |
|         5 | 2758 | `	pGen->bInGenerator = 0;` |
|         5 | 2759 | `	pGen->bStrictTypes = 0;` |
|         5 | 2760 | `	pGen->bStrictTypesLocked = 0;` |
|         - | 2761 | `	/* The nested unit is a fresh top-level compile: it is not lexically inside the` |
|         - | 2762 | `	 * outer's class body nor its member default, so a __TRAIT__ in the nested file` |
|         - | 2763 | `	 * must not inherit the outer's trait. (Restore below carries the outer's values` |
|         - | 2764 | `	 * back, so only the nested unit sees these zeros.) */` |
|         5 | 2765 | `	pGen->pCurClass = 0;` |
|         5 | 2766 | `	pGen->iInMemberDefault = 0;` |
|         5 | 2767 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|         5 | 2768 | `	pGen->xErr = xErr;` |
|         5 | 2769 | `	pGen->pErrData = pErrData;` |
|         5 | 2770 | `}` |
|         - | 2771 | `/*` |
|         - | 2772 | ` * Restore the outer compile-position state saved by PH7_CompilerSaveState,` |
|         - | 2773 | ` * releasing the nested unit's position containers first. The shared` |
|         - | 2774 | ` * hLiteral/hNumLiteral/hVar tables (grown by the nested compile) are carried` |
|         - | 2775 | ` * forward, NOT rolled back to the snapshot's stale headers.` |
|         - | 2776 | ` */` |
|         4 | 2777 | `PH7_PRIVATE void PH7_CompilerRestoreState(ph7_vm *pVm,ph7_gen_state *pSaved)` |
|         1 | 2778 | `{` |
|         5 | 2779 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 2780 | `	GenBlock *pBlock,*pParent;` |
|         - | 2781 | `	SyHash hVar,hLiteral,hNumLiteral;` |
|         - | 2782 | `	/* Free any nested blocks left open (e.g. an aborted nested compile), then the` |
|         - | 2783 | `	 * nested global block's own fixup sets. */` |
|         5 | 2784 | `	pBlock = pGen->pCurrent;` |
|         5 | 2785 | `	while( pBlock && pBlock->pParent != 0 ){` |
|       ! 0 | 2786 | `		pParent = pBlock->pParent;` |
|       ! 0 | 2787 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 2788 | `		pBlock = pParent;` |
|       ! 0 | 2789 | `	}` |
|         5 | 2790 | `	GenStateReleaseBlock(&pGen->sGlobal);` |
|         - | 2791 | `	/* Release the nested unit's position containers. */` |
|         5 | 2792 | `	SySetRelease(&pGen->aLabel);` |
|         5 | 2793 | `	SySetRelease(&pGen->aGoto);` |
|         5 | 2794 | `	SySetRelease(&pGen->aNullsafeJmp);` |
|         5 | 2795 | `	SySetRelease(&pGen->aLoopParent);` |
|         5 | 2796 | `	SySetRelease(&pGen->aTrivia);` |
|         5 | 2797 | `	SySetRelease(&pGen->aPendingAttrs);` |
|         5 | 2798 | `	SyBlobRelease(&pGen->sWorker);` |
|         5 | 2799 | `	SyBlobRelease(&pGen->sErrBuf);` |
|         5 | 2800 | `	SyBlobRelease(&pGen->sNamespace);` |
|         5 | 2801 | `	SyHashRelease(&pGen->hUseImports);` |
|         5 | 2802 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|         5 | 2803 | `	SyHashRelease(&pGen->hUseConstImports);` |
|         - | 2804 | `	/* Preserve the (possibly grown) shared intern tables across the restore. */` |
|         5 | 2805 | `	hVar = pGen->hVar;` |
|         5 | 2806 | `	hLiteral = pGen->hLiteral;` |
|         5 | 2807 | `	hNumLiteral = pGen->hNumLiteral;` |
|         5 | 2808 | `	*pGen = *pSaved;` |
|         5 | 2809 | `	pGen->hVar = hVar;` |
|         5 | 2810 | `	pGen->hLiteral = hLiteral;` |
|         5 | 2811 | `	pGen->hNumLiteral = hNumLiteral;` |
|         5 | 2812 | `}` |
|         - | 2813 | `/*` |
|         - | 2814 | ` * Raise php's parse error for an unexpected token: E_PARSE with the exact text` |
|         - | 2815 | ` * php's parser prints, e.g.` |
|         - | 2816 | ` *` |
|         - | 2817 | ` *   syntax error, unexpected token ";", expecting "{"` |
|         - | 2818 | ` *   syntax error, unexpected identifier "invalid", expecting "("` |
|         - | 2819 | ` *   syntax error, unexpected end of file` |
|         - | 2820 | ` *` |
|         - | 2821 | ` * php names the token by CLASS, not just by text: an identifier, a variable and` |
|         - | 2822 | ` * a number each get their own noun, while everything else (keywords, operators,` |
|         - | 2823 | ` * punctuation) is a "token". zExpecting is the optional ", expecting ..." tail` |
|         - | 2824 | ` * — pass NULL when the site cannot say what it wanted (php often can't either).` |
|         - | 2825 | ` * pTok == NULL means the input ran out: "unexpected end of file".` |
|         - | 2826 | ` *` |
|         - | 2827 | ` * PHL's hand-written recursive-descent parser has no bison expectation sets, so` |
|         - | 2828 | ` * a site can only claim an "expecting" clause it genuinely knows; every clause` |
|         - | 2829 | ` * emitted here was verified against php 8.5.7 for the construct in question.` |
|         - | 2830 | ` */` |
|       208 | 2831 | `PH7_PRIVATE sxi32 PH7_GenSyntaxError(` |
|         - | 2832 | `	ph7_gen_state *pGen,   /* Code generator state */` |
|         - | 2833 | `	SyToken *pTok,         /* Offending token, or NULL for end of file */` |
|         - | 2834 | `	const char *zExpecting /* ", expecting <this>" tail, or NULL */` |
|         - | 2835 | `	)` |
|         5 | 2836 | `{` |
|       213 | 2837 | `	const char *zNoun = "token";` |
|         - | 2838 | `	sxu32 nLine;` |
|       213 | 2839 | `	if( pTok == 0 && pGen->pTokenSet ){` |
|         - | 2840 | `		/* The caller ran out of tokens inside its own slice — but a statement's slice stops` |
|         - | 2841 | `		 * BEFORE its terminator, so the token php actually names (typically the ';') is the` |
|         - | 2842 | `		 * one sitting just past the slice, still inside the chunk's token stream. Reach for` |
|         - | 2843 | `		 * it before concluding "end of file". */` |
|        96 | 2844 | `		SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        96 | 2845 | `		SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|        96 | 2846 | `		if( pGen->pEnd >= pBase && pGen->pEnd < pStreamEnd ){` |
|        92 | 2847 | `			pTok = pGen->pEnd;` |
|        44 | 2848 | `		}` |
|        46 | 2849 | `	}` |
|       213 | 2850 | `	nLine = pTok ? pTok->nLine : (pGen->pIn > (SyToken *)SySetBasePtr(pGen->pTokenSet) ? pGen->pIn[-1].nLine : 1);` |
|       213 | 2851 | `	if( pTok == 0 ){` |
|         7 | 2852 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|         2 | 2853 | `			zExpecting ? "syntax error, unexpected end of file, expecting %s"` |
|         - | 2854 | `			           : "syntax error, unexpected end of file",` |
|         2 | 2855 | `			zExpecting);` |
|         - | 2856 | `	}` |
|       209 | 2857 | `	if( pTok->nType & PH7_TK_ID ){` |
|        22 | 2858 | `		zNoun = "identifier";` |
|       200 | 2859 | `	}else if( pTok->nType & PH7_TK_DOLLAR ){` |
|         9 | 2860 | `		zNoun = "variable";` |
|         - | 2861 | `		/* The '$' is its own token and carries only "$" as text; the NAME is the` |
|         - | 2862 | ``		 * token after it. php names the whole variable, so `$x` was being reported`` |
|         - | 2863 | ``		 * as the nameless `variable "$"`. Stitch the two back together. */`` |
|         9 | 2864 | `		if( pGen->pTokenSet ){` |
|         9 | 2865 | `			SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|         9 | 2866 | `			SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|         9 | 2867 | `			SyToken *pName = &pTok[1];` |
|         6 | 2868 | `			if( pTok >= pBase && pName < pStreamEnd` |
|         6 | 2869 | `				&& (pName->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|         9 | 2870 | `				&& pName->sData.nByte > 0 ){` |
|         9 | 2871 | `				SyBlobReset(&pGen->sWorker);` |
|         9 | 2872 | `				SyBlobAppend(&pGen->sWorker,"$",sizeof(char));` |
|         9 | 2873 | `				SyBlobAppend(&pGen->sWorker,pName->sData.zString,pName->sData.nByte);` |
|         - | 2874 | `				{` |
|         - | 2875 | `					SyString sVar;` |
|         9 | 2876 | `					SyStringInitFromBuf(&sVar,SyBlobData(&pGen->sWorker),` |
|         - | 2877 | `						SyBlobLength(&pGen->sWorker));` |
|         9 | 2878 | `					if( zExpecting ){` |
|        12 | 2879 | `						return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|         - | 2880 | `							"syntax error, unexpected %s \"%z\", expecting %s",` |
|         3 | 2881 | `							zNoun,&sVar,zExpecting);` |
|         - | 2882 | `					}` |
|       ! 0 | 2883 | `					return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|       ! 0 | 2884 | `						"syntax error, unexpected %s \"%z\"",zNoun,&sVar);` |
|         - | 2885 | `				}` |
|         - | 2886 | `			}` |
|       ! 0 | 2887 | `		}` |
|       185 | 2888 | `	}else if( pTok->nType & PH7_TK_INTEGER ){` |
|        27 | 2889 | `		zNoun = "integer";` |
|       173 | 2890 | `	}else if( pTok->nType & PH7_TK_REAL ){` |
|       ! 0 | 2891 | `		zNoun = "float";` |
|       ! 0 | 2892 | `	}` |
|       203 | 2893 | `	if( zExpecting ){` |
|       143 | 2894 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        46 | 2895 | `			"syntax error, unexpected %s \"%z\", expecting %s",zNoun,&pTok->sData,zExpecting);` |
|         - | 2896 | `	}` |
|       164 | 2897 | `	return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        53 | 2898 | `		"syntax error, unexpected %s \"%z\"",zNoun,&pTok->sData);` |
|       109 | 2899 | `}` |
|         - | 2900 | `/*` |
|         - | 2901 | ` * Generate a compile-time error message.` |
|         - | 2902 | ` * If the error count limit is reached (usually 15 error message)` |
|         - | 2903 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|         - | 2904 | ` * abort compilation immediately.` |
|         - | 2905 | ` */` |
|       700 | 2906 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|         5 | 2907 | `{` |
|       705 | 2908 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|       705 | 2909 | `	const char *zErr = "Error";` |
|         - | 2910 | `	SyString *pFile;` |
|         - | 2911 | `	va_list ap;` |
|         - | 2912 | `	sxi32 rc;` |
|         - | 2913 | `	/* Reset the working buffer */` |
|       705 | 2914 | `	SyBlobReset(pWorker);` |
|         - | 2915 | `	/* Peek the processed file path if available */` |
|       705 | 2916 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|       705 | 2917 | `	if( nErrType == E_ERROR \|\| nErrType == E_PARSE ){` |
|         - | 2918 | `		/* Increment the error counter. A PARSE error is every bit as fatal as an` |
|         - | 2919 | `		 * E_ERROR one: php compiles nothing, runs nothing and exits 255. Counting` |
|         - | 2920 | `		 * only E_ERROR let a parse error print its diagnostic and then fall through` |
|         - | 2921 | `		 * into execution with a 0 exit status. */` |
|       697 | 2922 | `		pGen->nErr++;` |
|       697 | 2923 | `		if( pGen->nErr > 15 ){` |
|         - | 2924 | `			/* Error count limit reached */` |
|         6 | 2925 | `			if( pGen->xErr ){` |
|         6 | 2926 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|         6 | 2927 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|         6 | 2928 | `				if( pFile ){` |
|         6 | 2929 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|         2 | 2930 | `				}` |
|         6 | 2931 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|         6 | 2932 | `				if( SyBlobLength(pWorker) > 0 ){` |
|         6 | 2933 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|         2 | 2934 | `				}` |
|         2 | 2935 | `			}` |
|         - | 2936 | `			/* Abort immediately */` |
|         6 | 2937 | `			return SXERR_ABORT;` |
|         - | 2938 | `		}` |
|       344 | 2939 | `	}` |
|       701 | 2940 | `	if( pGen->xErr == 0 ){` |
|         - | 2941 | `		/* No consumer — but keep the BARE message in the error buffer so a caller` |
|         - | 2942 | `		 * that needs the text can read it back. eval() compiles with logging off` |
|         - | 2943 | `		 * (a parse error there is php's catchable ParseError, not a printed` |
|         - | 2944 | `		 * diagnostic) and needs exactly this string for the exception message. */` |
|         5 | 2945 | `		va_start(ap,zFormat);` |
|         5 | 2946 | `		SyBlobFormatAp(pWorker,zFormat,ap);` |
|         5 | 2947 | `		va_end(ap);` |
|         5 | 2948 | `		return SXRET_OK;` |
|         - | 2949 | `	}` |
|       697 | 2950 | `	switch(nErrType){` |
|       324 | 2951 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|        11 | 2952 | `	case E_WARNING: zErr = "Warning";     break;` |
|       368 | 2953 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       ! 0 | 2954 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|       ! 0 | 2955 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|       ! 0 | 2956 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|       ! 0 | 2957 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|       ! 0 | 2958 | `	case 8192 /* E_DEPRECATED */: zErr = "Deprecated"; break;` |
|       ! 0 | 2959 | `	default:` |
|       ! 0 | 2960 | `		break;` |
|         - | 2961 | `	}` |
|       697 | 2962 | `	rc = SXRET_OK;` |
|         - | 2963 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|       697 | 2964 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|       697 | 2965 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|       697 | 2966 | `	va_start(ap,zFormat);` |
|       697 | 2967 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|       697 | 2968 | `	va_end(ap);` |
|       697 | 2969 | `	if( pFile ){` |
|       697 | 2970 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       346 | 2971 | `	}` |
|         - | 2972 | `	/* Append a new line */` |
|       697 | 2973 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       697 | 2974 | `	if( SyBlobLength(pWorker) > 0 ){` |
|         - | 2975 | `		/* Consume the generated error message */` |
|       697 | 2976 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       346 | 2977 | `	}` |
|       697 | 2978 | `	return rc;` |
|       355 | 2979 | `}` |
|         - | 2980 |  |
