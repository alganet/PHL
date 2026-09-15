# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1476/1604 lines (92.02%)

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
|       276 |   29 | `		if( SyStringCmp(&aLabel[n].sName,pName,SyMemcmp) == 0 ){` |
|         - |   30 | `			/* Jump destination found */` |
|        96 |   31 | `			aLabel[n].bRef = TRUE;` |
|        96 |   32 | `			if( ppOut ){` |
|        96 |   33 | `				*ppOut = &aLabel[n];` |
|        46 |   34 | `			}` |
|        96 |   35 | `			return SXRET_OK;` |
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
|    178756 |   46 | `PH7_PRIVATE GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|         5 |   47 | `{` |
|    178761 |   48 | `	GenBlock *pBlock = pCurrent;` |
|    361253 |   49 | `	for(;;){` |
|    722511 |   50 | `		if( pBlock->iFlags & iBlockType ){` |
|    178761 |   51 | `			iCount--; /* Decrement nesting level */` |
|    178761 |   52 | `			if( iCount < 1 ){` |
|         - |   53 | `				/* Block meet with the desired criteria */` |
|    178735 |   54 | `				return pBlock;` |
|         - |   55 | `			}` |
|        13 |   56 | `		}` |
|         - |   57 | `		/* Point to the upper block */` |
|    543781 |   58 | `		pBlock = pBlock->pParent;` |
|    543781 |   59 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|         - |   60 | `			/* Forbidden */` |
|        17 |   61 | `			break;` |
|         - |   62 | `		}` |
|         5 |   63 | `	}` |
|         - |   64 | `	/* No such block */` |
|        30 |   65 | `	return 0;` |
|     89383 |   66 | `}` |
|         - |   67 | `/*` |
|         - |   68 | ` * Initialize a freshly allocated block instance.` |
|         - |   69 | ` */` |
|  13103726 |   70 | `static void GenStateInitBlock(` |
|         - |   71 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |   72 | `	GenBlock *pBlock,    /* Target block */` |
|         - |   73 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   74 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|         - |   75 | `	void *pUserData      /* Upper layer private data */` |
|         - |   76 | `	)` |
|         5 |   77 | `{` |
|         - |   78 | `	/* Initialize block fields */` |
|  13103731 |   79 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  13103731 |   80 | `	pBlock->pUserData   = pUserData;` |
|  13103731 |   81 | `	pBlock->pGen        = pGen;` |
|  13103731 |   82 | `	pBlock->iFlags      = iType;` |
|  13103731 |   83 | `	pBlock->pParent     = 0;` |
|  13103731 |   84 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  13103731 |   85 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  13103731 |   86 | `}` |
|         - |   87 | `/*` |
|         - |   88 | ` * Allocate a new block instance.` |
|         - |   89 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|         - |   90 | ` * on success.Otherwise generate a compile-time error and abort` |
|         - |   91 | ` * processing on failure.` |
|         - |   92 | ` */` |
|  13099840 |   93 | `PH7_PRIVATE sxi32 GenStateEnterBlock(` |
|         - |   94 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |   95 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   96 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|         - |   97 | `	void *pUserData,      /* Upper layer private data */` |
|         - |   98 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|         - |   99 | `	)` |
|         5 |  100 | `{` |
|         - |  101 | `	GenBlock *pBlock;` |
|         - |  102 | `	/* Allocate a new block instance */` |
|  13099845 |  103 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  13099845 |  104 | `	if( pBlock == 0 ){` |
|         - |  105 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |  106 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - |  107 | `		 */` |
|       ! 0 |  108 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|         - |  109 | `		/* Abort processing immediately */` |
|       ! 0 |  110 | `		return SXERR_ABORT;` |
|         - |  111 | `	}` |
|         - |  112 | `	/* Zero the structure */` |
|  13099845 |  113 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  13099845 |  114 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|         - |  115 | `	/* Link to the parent block */` |
|  13099845 |  116 | `	pBlock->pParent = pGen->pCurrent;` |
|         - |  117 | `	/* A loop or switch gets an id, and remembers the loop it nests inside, so a goto's` |
|         - |  118 | `	 * and a label's positions can be compared after compilation (see aLoopParent). */` |
|  13099845 |  119 | `	if( iType & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    540749 |  120 | `		sxu32 nParent = pGen->nCurLoopId;` |
|    540749 |  121 | `		pGen->nLoopId++;` |
|    540749 |  122 | `		SySetPut(&pGen->aLoopParent,(const void *)&nParent);` |
|    540749 |  123 | `		pBlock->nLoopId = pGen->nLoopId;` |
|    540749 |  124 | `		pBlock->nOuterLoopId = nParent;` |
|    540749 |  125 | `		pGen->nCurLoopId = pGen->nLoopId;` |
|    270372 |  126 | `	}` |
|         - |  127 | `	/* Mark as the current block */` |
|  13099845 |  128 | `	pGen->pCurrent = pBlock;` |
|  13099845 |  129 | `	if( ppBlock ){` |
|         - |  130 | `		/* Write a pointer to the new instance */` |
|   6300123 |  131 | `		*ppBlock = pBlock;` |
|   3150059 |  132 | `	}` |
|  13099845 |  133 | `	return SXRET_OK;` |
|   6549925 |  134 | `}` |
|         - |  135 | `/*` |
|         - |  136 | ` * Release block fields without freeing the whole instance.` |
|         - |  137 | ` */` |
|  13099832 |  138 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|         5 |  139 | `{` |
|  13099837 |  140 | `	SySetRelease(&pBlock->aPostContFix);` |
|  13099837 |  141 | `	SySetRelease(&pBlock->aJumpFix);` |
|  13099837 |  142 | `}` |
|         - |  143 | `/*` |
|         - |  144 | ` * Release a block.` |
|         - |  145 | ` */` |
|  13099828 |  146 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|         5 |  147 | `{` |
|  13099833 |  148 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  13099833 |  149 | `	GenStateReleaseBlock(&(*pBlock));` |
|         - |  150 | `	/* Free the instance */` |
|  13099833 |  151 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  13099833 |  152 | `}` |
|         - |  153 | `/*` |
|         - |  154 | ` * POP and release a block from the stack of compiled blocks.` |
|         - |  155 | ` */` |
|  13099828 |  156 | `PH7_PRIVATE sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|         5 |  157 | `{` |
|  13099833 |  158 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  13099833 |  159 | `	if( pBlock == 0 ){` |
|         - |  160 | `		/* No more block to pop */` |
|       ! 0 |  161 | `		return SXERR_EMPTY;` |
|         - |  162 | `	}` |
|  13099833 |  163 | `	if( pBlock->iFlags & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    540741 |  164 | `		pGen->nCurLoopId = pBlock->nOuterLoopId;` |
|    270368 |  165 | `	}` |
|         - |  166 | `	/* Point to the upper block */` |
|  13099833 |  167 | `	pGen->pCurrent = pBlock->pParent;` |
|  13099833 |  168 | `	if( ppBlock ){` |
|         - |  169 | `		/* Write a pointer to the popped block */` |
|       ! 0 |  170 | `		*ppBlock = pBlock;` |
|       ! 0 |  171 | `	}else{` |
|         - |  172 | `		/* Safely release the block */` |
|  13099833 |  173 | `		GenStateFreeBlock(&(*pBlock));` |
|         - |  174 | `	}` |
|  13099833 |  175 | `	return SXRET_OK;` |
|   6549919 |  176 | `}` |
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
|   1043908 |  195 | `PH7_PRIVATE int GenStateUnconditionalTopLevel(ph7_gen_state *pGen)` |
|         5 |  196 | `{` |
|   1043913 |  197 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   1047899 |  198 | `	while( pBlock ){` |
|   1047899 |  199 | `		if( pBlock->iFlags & (GEN_BLOCK_COND\|GEN_BLOCK_LOOP\|GEN_BLOCK_FUNC\|GEN_BLOCK_SWITCH\|GEN_BLOCK_EXCEPTION) ){` |
|        77 |  200 | `			return 0; /* conditional / nested */` |
|         - |  201 | `		}` |
|   1047827 |  202 | `		if( pBlock->iFlags & GEN_BLOCK_GLOBAL ){` |
|   1043841 |  203 | `			return 1; /* reached the global block with no conditional ancestor */` |
|         - |  204 | `		}` |
|      3991 |  205 | `		pBlock = pBlock->pParent;` |
|         5 |  206 | `	}` |
|       ! 0 |  207 | `	return 1;` |
|    521959 |  208 | `}` |
|         - |  209 | `/*` |
|         - |  210 | ` * Guard a top-level function about to be installed. Same contract as the class` |
|         - |  211 | ` * guard above.` |
|         - |  212 | ` */` |
|    521590 |  213 | `PH7_PRIVATE sxi32 GenStateGuardFuncRedeclaration(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|         5 |  214 | `{` |
|         - |  215 | `	SyHashEntry *pEntry;` |
|    521595 |  216 | `	if( !GenStateUnconditionalTopLevel(pGen) ){` |
|        53 |  217 | `		return SXRET_OK;` |
|         - |  218 | `	}` |
|    521545 |  219 | `	pFunc->iFlags \|= VM_FUNC_BOUND;` |
|    521545 |  220 | `	if( pGen->pVm->bCompilingBuiltin ){` |
|    520193 |  221 | `		return SXRET_OK;` |
|         - |  222 | `	}` |
|         - |  223 | ``	/* NOTE: a userland function shadowing a C builtin (e.g. `function strlen(){}`)`` |
|         - |  224 | `	 * is NOT caught here — the C builtins register in PH7_VmMakeReady, after user` |
|         - |  225 | `	 * code has compiled, so hHostFunction is still empty at this point. Prelude` |
|         - |  226 | `	 * functions (ini_get, ...) and every builtin CLASS compile earlier and ARE` |
|         - |  227 | `	 * guarded. Redeclaring a C builtin function stays a known divergence. */` |
|      1357 |  228 | `	pEntry = SyHashGet(&pGen->pVm->hFunction,(const void *)pFunc->sName.zString,pFunc->sName.nByte);` |
|      1357 |  229 | `	if( pEntry ){` |
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
|      1353 |  246 | `	return SXRET_OK;` |
|    260800 |  247 | `}` |
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
|   4730982 |  258 | `PH7_PRIVATE sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|         5 |  259 | `{` |
|         - |  260 | `	JumpFixup sJumpFix;` |
|         - |  261 | `	sxi32 rc;` |
|         - |  262 | `	/* Init the JumpFixup structure */` |
|   4730987 |  263 | `	sJumpFix.nJumpType = nJumpType;` |
|   4730987 |  264 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|         - |  265 | `	/* Insert in the jump fixup table */` |
|   4730987 |  266 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|   4730987 |  267 | `	return rc;` |
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
|   9107022 |  280 | `PH7_PRIVATE sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|         5 |  281 | `{` |
|         - |  282 | `	JumpFixup *aFix;` |
|         - |  283 | `	VmInstr *pInstr;` |
|         - |  284 | `	sxu32 nFixed;` |
|         - |  285 | `	sxu32 n;` |
|         - |  286 | `	/* Point to the jump fixup table */` |
|   9107027 |  287 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|         - |  288 | `	/* Fix the desired jumps */` |
|  19062439 |  289 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|   9955417 |  290 | `		if( aFix[n].nJumpType < 0 ){` |
|         - |  291 | `			/* Already fixed */` |
|   3717395 |  292 | `			continue;` |
|         - |  293 | `		}` |
|   6238027 |  294 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|         - |  295 | `			/* Not of our interest */` |
|   1507047 |  296 | `			continue;` |
|         - |  297 | `		}` |
|         - |  298 | `		/* Point to the instruction to fix */` |
|   4730985 |  299 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|   4730985 |  300 | `		if( pInstr ){` |
|   4730985 |  301 | `			pInstr->iP2 = nJumpDest;` |
|   4730985 |  302 | `			nFixed++;` |
|         - |  303 | `			/* Mark as fixed */` |
|   4730985 |  304 | `			aFix[n].nJumpType = -1;` |
|   2365490 |  305 | `		}` |
|   2365495 |  306 | `	}` |
|         - |  307 | `	/* Total number of fixed jumps */` |
|   9107027 |  308 | `	return nFixed;` |
|         5 |  309 | `}` |
|         - |  310 | `/*` |
|         - |  311 | ` * Fix a 'goto' now the jump destination is resolved.` |
|         - |  312 | ` * The goto statement can be used to jump to another section` |
|         - |  313 | ` * in the program.` |
|         - |  314 | ` * Refer to the routine responsible of compiling the goto` |
|         - |  315 | ` * statement for more information.` |
|         - |  316 | ` */` |
|   3371726 |  317 | `PH7_PRIVATE sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|         5 |  318 | `{` |
|         - |  319 | `	JumpFixup *pJump,*aJumps;` |
|         - |  320 | `	Label *pLabel;` |
|         - |  321 | `	VmInstr *pInstr;` |
|         - |  322 | `	sxi32 rc;` |
|         - |  323 | `	sxu32 n;` |
|         - |  324 | `	/* Point to the goto table */` |
|   3371731 |  325 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|         - |  326 | `	/* Fix */` |
|   3371877 |  327 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|        96 |  343 | `		if( pLabel->nLoopId != 0 ){` |
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
|        96 |  364 | `		if( pLabel->pFunc != pJump->pFunc ){` |
|        11 |  365 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"'goto' to undefined label '%z'",&pJump->sLabel);` |
|        11 |  366 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  367 | `				return SXERR_ABORT;` |
|         - |  368 | `			}` |
|         4 |  369 | `		}` |
|         - |  370 | `		/* Fix the jump now the destination is resolved */` |
|        96 |  371 | `		pInstr = PH7_VmGetInstr(pGen->pVm,pJump->nInstrIdx);` |
|        96 |  372 | `		if( pInstr ){` |
|        96 |  373 | `			pInstr->iP2 = pLabel->nJumpDest;` |
|        46 |  374 | `		}` |
|        50 |  375 | `	}` |
|         - |  376 | `	/* php says nothing about a label nobody jumps to — the old "defined but not` |
|         - |  377 | `	 * referenced" warning was a PH7-ism with no counterpart in the oracle. */` |
|   3371729 |  378 | `	return SXRET_OK;` |
|   1685868 |  379 | `}` |
|         - |  380 | `/*` |
|         - |  381 | ` * Check if a given token value is installed in the literal table.` |
|         - |  382 | ` */` |
|  16889584 |  383 | `PH7_PRIVATE sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|         5 |  384 | `{` |
|         - |  385 | `	SyHashEntry *pEntry;` |
|  16889589 |  386 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  16889589 |  387 | `	if( pEntry == 0 ){` |
|   4502661 |  388 | `		return SXERR_NOTFOUND;` |
|         - |  389 | `	}` |
|  12386933 |  390 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  12386933 |  391 | `	return SXRET_OK;` |
|   8444797 |  392 | `}` |
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
|   4502656 |  403 | `PH7_PRIVATE sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|         5 |  404 | `{` |
|   4502661 |  405 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|   4502661 |  406 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   2251328 |  407 | `	}` |
|   4502661 |  408 | `	return SXRET_OK;` |
|         5 |  409 | `}` |
|         - |  410 | `/*` |
|         - |  411 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|         - |  412 | ` * in the constant table.` |
|         - |  413 | ` */` |
|   3818388 |  414 | `PH7_PRIVATE ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|         5 |  415 | `{` |
|         - |  416 | `	ph7_value *pObj;` |
|   3818393 |  417 | `	sxu32 nIdx = 0; /* cc warning */` |
|         - |  418 | `	/* Reserve a new constant */` |
|   3818393 |  419 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   3818393 |  420 | `	if( pObj == 0 ){` |
|       ! 0 |  421 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  422 | `		return 0;` |
|         - |  423 | `	}` |
|   3818393 |  424 | `	*pIdx = nIdx;` |
|         - |  425 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|         - |  426 | `	 * the constant string iterals table [optimization purposes].` |
|         - |  427 | `	 */` |
|   3818393 |  428 | `	return pObj;` |
|   1909199 |  429 | `}` |
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
|   7945790 |  444 | `PH7_PRIVATE void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|         5 |  445 | `{` |
|         - |  446 | `	VmCallArgMap *pMap;` |
|   7945795 |  447 | `	if( !pGen->bStrictTypes ) return p3;` |
|        58 |  448 | `	if( p3 == 0 ){` |
|        54 |  449 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|        54 |  450 | `		if( pMap == 0 ) return 0;` |
|        54 |  451 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|        54 |  452 | `		p3 = (void *)pMap;` |
|        25 |  453 | `	}` |
|        58 |  454 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|        58 |  455 | `	return p3;` |
|   3972900 |  456 | `}` |
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
|    341856 |  476 | `PH7_PRIVATE int GenStateIsReservedConstant(SyString *pName)` |
|         5 |  477 | `{` |
|    341861 |  478 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      3931 |  479 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|         3 |  480 | `			return TRUE;` |
|      3929 |  481 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|         6 |  482 | `			return TRUE;` |
|         5 |  483 | `		}` |
|    339895 |  484 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|      7789 |  485 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|         3 |  486 | `			return TRUE;` |
|         - |  487 | `		}` |
|      3891 |  488 | `	}` |
|         - |  489 | `	/* Not a reserved constant */` |
|    341853 |  490 | `	return FALSE;` |
|    170933 |  491 | `}` |
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
|  53980080 |  508 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|         5 |  509 | `{` |
|  53980085 |  510 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|         - |  511 | `	sxu32 nTarget;` |
|         - |  512 | `	sxu32 *aIdx;` |
|         - |  513 | `	sxu32 i;` |
|  53980085 |  514 | `	if( nCur <= nBaseline ){` |
|  53979989 |  515 | `		return;` |
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
|  26990045 |  526 | `}` |
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
|   7057040 |  545 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
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
|   7057045 |  564 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|   1806719 |  565 | `		return 0;` |
|         - |  566 | `	}` |
|  57271011 |  567 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
|  52079076 |  568 | `		if( pName->nByte == aByRef[i].nByte` |
|  27555211 |  569 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|     58401 |  570 | `			return aByRef[i].mask;` |
|         - |  571 | `		}` |
|  26010345 |  572 | `	}` |
|   5191935 |  573 | `	return 0;` |
|   3528525 |  574 | `}` |
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
|   7057040 |  585 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|         5 |  586 | `{` |
|         - |  587 | `	SyToken *p, *pEnd;` |
|   7057045 |  588 | `	pOut->zString = 0;` |
|   7057045 |  589 | `	pOut->nByte = 0;` |
|   7057045 |  590 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|       ! 0 |  591 | `		return;` |
|         - |  592 | `	}` |
|   7057045 |  593 | `	p = pLeft->pStart;` |
|   7057045 |  594 | `	pEnd = pLeft->pEnd;` |
|         - |  595 | `	/* Optional single leading namespace separator (absolute path). */` |
|   7057045 |  596 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|      3925 |  597 | `		p++;` |
|      1960 |  598 | `	}` |
|   7057045 |  599 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|   1806673 |  600 | `		return;` |
|         - |  601 | `	}` |
|         - |  602 | `	/* Must be a single component: nothing follows the name token. */` |
|   5250377 |  603 | `	if( p + 1 != pEnd ){` |
|        51 |  604 | `		return;` |
|         - |  605 | `	}` |
|   5250331 |  606 | `	*pOut = p->sData;` |
|   3528525 |  607 | `}` |
|         - |  608 | `/*` |
|         - |  609 | ` * Generate bytecode for a given expression tree.` |
|         - |  610 | ` * If something goes wrong while generating bytecode` |
|         - |  611 | ` * for the expression tree (A very unlikely scenario)` |
|         - |  612 | ` * this function takes care of generating the appropriate` |
|         - |  613 | ` * error message.` |
|         - |  614 | ` */` |
|  76111534 |  615 | `static sxi32 GenStateEmitExprCode(` |
|         - |  616 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |  617 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|         - |  618 | `	sxi32 iFlags /* Control flags */` |
|         - |  619 | `	)` |
|         5 |  620 | `{` |
|         - |  621 | `	VmInstr *pInstr;` |
|         - |  622 | `	sxu32 nJmpIdx;` |
|  76111539 |  623 | `	sxi32 iP1 = 0;` |
|  76111539 |  624 | `	sxu32 iP2 = 0;` |
|  76111539 |  625 | `	void *p3  = 0;` |
|         - |  626 | `	sxi32 iVmOp;` |
|         - |  627 | `	sxi32 rc;` |
|  76111539 |  628 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
|  76111539 |  629 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
|  76111539 |  630 | `	sxu32 nRhsNsBase = 0;` |
|  76111539 |  631 | `	if( pNode->xCode ){` |
|         - |  632 | `		SyToken *pTmpIn,*pTmpEnd;` |
|         - |  633 | `		/* Compile node */` |
|  45937325 |  634 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
|  45937325 |  635 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
|  45937325 |  636 | `		RE_SWAP_DELIMITER(pGen);` |
|  45937325 |  637 | `		return rc;` |
|         - |  638 | `	}` |
|  30174219 |  639 | `	if( pNode->pOp == 0 ){` |
|       ! 0 |  640 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - |  641 | `			"Invalid expression node,PH7 is aborting compilation");` |
|       ! 0 |  642 | `		return SXERR_ABORT;` |
|         - |  643 | `	}` |
|  30174219 |  644 | `	iVmOp = pNode->pOp->iVmOp;` |
|  30174219 |  645 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|         - |  646 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|         - |  647 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|         - |  648 | `		 * and later errors are still reported. */` |
|         3 |  649 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - |  650 | `			"The (unset) cast is no longer supported");` |
|         3 |  651 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  652 | `			return SXERR_ABORT;` |
|         - |  653 | `		}` |
|         1 |  654 | `	}` |
|  30174219 |  655 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
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
|  30174129 |  705 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|         - |  706 | `		sxu32 nJz,nJmp;` |
|         - |  707 | `		sxu32 nTernaryNsBase;` |
|         - |  708 | `		/* Ternary operator require special handling */` |
|         - |  709 | `		/* Phase#1: Compile the condition */` |
|    504083 |  710 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    504083 |  711 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    504083 |  712 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  713 | `			return rc;` |
|         - |  714 | `		}` |
|         - |  715 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|         - |  716 | `		 * compiling the condition must short-circuit to the end of the` |
|         - |  717 | `		 * condition expression, not leak past the ternary. */` |
|    504083 |  718 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    504083 |  719 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    504083 |  720 | `		if( pNode->pLeft ){` |
|         - |  721 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|         - |  722 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    500131 |  723 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - |  724 | `			/* Phase#3: Compile the 'then' expression  */` |
|    500131 |  725 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    500131 |  726 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    500131 |  727 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  728 | `				return rc;` |
|         - |  729 | `			}` |
|    500131 |  730 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    250068 |  731 | `		}else{` |
|         - |  732 | `			/* Elvis operator: (expr) ?: (else)` |
|         - |  733 | `			 * Duplicate condition so original value is the 'then' result.` |
|         - |  734 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      3957 |  735 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      3957 |  736 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - |  737 | `		}` |
|         - |  738 | `		/* Phase#4: Emit the unconditional jump */` |
|    504083 |  739 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|         - |  740 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    504083 |  741 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    504083 |  742 | `		if( pInstr ){` |
|    504083 |  743 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    252039 |  744 | `		}` |
|    504083 |  745 | `		if( !pNode->pLeft ){` |
|         - |  746 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      3957 |  747 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      1976 |  748 | `		}` |
|         - |  749 | `		/* Phase#6: Compile the 'else' expression */` |
|    504083 |  750 | `		if( pNode->pRight ){` |
|    504083 |  751 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    504083 |  752 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    504083 |  753 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  754 | `				return rc;` |
|         - |  755 | `			}` |
|    504083 |  756 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    252039 |  757 | `		}` |
|    504083 |  758 | `		if( nJmp > 0 ){` |
|         - |  759 | `			/* Phase#7: Fix the unconditional jump */` |
|    504083 |  760 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    504083 |  761 | `			if( pInstr ){` |
|    504083 |  762 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    252039 |  763 | `			}` |
|    252039 |  764 | `		}` |
|         - |  765 | `		/* All done */` |
|    504083 |  766 | `		return SXRET_OK;` |
|         - |  767 | `	}` |
|  29670051 |  768 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
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
|  29670025 |  801 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|         - |  802 | `	/* Generate code for the left tree */` |
|  29670025 |  803 | `	if( pNode->pLeft ){` |
|  29615865 |  804 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  29615865 |  805 | `		if( iVmOp == PH7_OP_CALL ){` |
|         - |  806 | `			ph7_expr_node **apNode;` |
|   7061267 |  807 | `			int hasSpread = 0;` |
|   7061267 |  808 | `			int hasNamed = 0;` |
|   7061267 |  809 | `			int bAnySpread = 0;` |
|   7061267 |  810 | `			sxu32 byRefMask = 0;` |
|         - |  811 | `			sxi32 nArgs;` |
|         - |  812 | `			sxi32 n;` |
|         - |  813 | `			/* Recurse and generate bytecodes for function arguments */` |
|   7061267 |  814 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   7061267 |  815 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|         - |  816 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|         - |  817 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|         - |  818 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|   7061267 |  819 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|        81 |  820 | `				bFcc = 1;` |
|        81 |  821 | `				nArgs = 0;` |
|        40 |  822 | `			}` |
|         - |  823 | `			/* Validate argument order like php: no positional argument after a` |
|         - |  824 | ``			 * named one OR after unpacking, and `name: ...$x` is a parse error. */`` |
|         - |  825 | `			{` |
|   7061267 |  826 | `				int seenNamed = 0;` |
|   7061267 |  827 | `				int seenSpread = 0;` |
|  15079671 |  828 | `				for( n = 0; n < nArgs; ++n ){` |
|   8018411 |  829 | `					if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|      4083 |  830 | `						bAnySpread = 1;` |
|      4083 |  831 | `						seenSpread = 1;` |
|      4083 |  832 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       ! 0 |  833 | `							rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - |  834 | `								"syntax error, unexpected token \"...\"");` |
|       ! 0 |  835 | `							return SXERR_SYNTAX;` |
|         5 |  836 | `						}` |
|   8016372 |  837 | `					}else if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       300 |  838 | `						seenNamed = 1;` |
|       300 |  839 | `						hasNamed = 1;` |
|   8014185 |  840 | `					}else if( seenNamed ){` |
|         3 |  841 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - |  842 | `							"Cannot use positional argument after named argument");` |
|         3 |  843 | `						return SXERR_SYNTAX;` |
|   8014035 |  844 | `					}else if( seenSpread ){` |
|       ! 0 |  845 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - |  846 | `							"Cannot use positional argument after argument unpacking");` |
|       ! 0 |  847 | `						return SXERR_SYNTAX;` |
|         - |  848 | `					}` |
|   4009207 |  849 | `				}` |
|         - |  850 | `			}` |
|         - |  851 | `			/* Read-only load */` |
|   7061265 |  852 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|         - |  853 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|         - |  854 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|         - |  855 | `			 * objects dispatch to the right method (offsetExists for both;` |
|         - |  856 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|   7061265 |  857 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|   7061265 |  858 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|   7061260 |  859 | `				if( pCallName->nByte == 5` |
|   3924150 |  860 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|    326479 |  861 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|   6898028 |  862 | `				}else if( pCallName->nByte == 5` |
|   3597676 |  863 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|       115 |  864 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|        55 |  865 | `				}` |
|         - |  866 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|         - |  867 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|         - |  868 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|         - |  869 | `				 * write back through. Skipped when spread/named args are present:` |
|         - |  870 | `				 * the compile-time positional index no longer maps to the` |
|         - |  871 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|   7061265 |  872 | `				if( !bAnySpread && !hasNamed ){` |
|         - |  873 | `					SyString sBuiltin;` |
|   7057045 |  874 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|   7057045 |  875 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|   3528520 |  876 | `				}` |
|   3530630 |  877 | `			}` |
|  15079667 |  878 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|   8018407 |  879 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   8018407 |  880 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|         - |  881 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|         - |  882 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|         - |  883 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|         - |  884 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|         - |  885 | `				 * builtin to write back through. A plain $var target is unaffected` |
|         - |  886 | `				 * (iP1=0 either way). */` |
|   8018407 |  887 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|     38911 |  888 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|     38911 |  889 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|     19453 |  890 | `				}` |
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
|   8018402 |  901 | `				if( (iFlags & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_LOAD_IDX_UNSET)) == 0` |
|   7855098 |  902 | `				 && apNode[n]->pOp == 0 && apNode[n]->xCode == PH7_CompileVariable` |
|   4116749 |  903 | `				 && (apNode[n]->iFlags & (EXPR_NODE_NAMED_ARG\|EXPR_NODE_SPREAD)) == 0 ){` |
|   3364423 |  904 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|   1682209 |  905 | `				}` |
|   8018407 |  906 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|   8018407 |  907 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  908 | `					return rc;` |
|         - |  909 | `				}` |
|         - |  910 | `				/* Each argument is an independent nullsafe scope. */` |
|   8018407 |  911 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|   8018407 |  912 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|         - |  913 | `					/* Emit spread opcode to unpack this array argument */` |
|      4083 |  914 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      4083 |  915 | `					hasSpread = 1;` |
|      2039 |  916 | `				}` |
|   4009206 |  917 | `			}` |
|         - |  918 | `			/* Total number of given arguments */` |
|   7061265 |  919 | `			iP1 = nArgs;` |
|   7061265 |  920 | `			iP2 = hasSpread;` |
|         - |  921 | `			/* Build VmCallArgMap if named arguments are present.` |
|         - |  922 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|   7061265 |  923 | `			if( hasNamed ){` |
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
|   7061265 |  955 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|   3530630 |  956 | `		}` |
|         - |  957 | `		{` |
|         - |  958 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|         - |  959 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|         - |  960 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|         - |  961 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|         - |  962 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|         - |  963 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|         - |  964 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|         - |  965 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
|  29615863 |  966 | `			sxi32 iLeftFlags = iFlags;` |
|  29615858 |  967 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
|  24046799 |  968 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|   9238896 |  969 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|   7953407 |  970 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   2781217 |  971 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|   1390606 |  972 | `			}` |
|         - |  973 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|         - |  974 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|         - |  975 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|         - |  976 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|         - |  977 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|         - |  978 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|         - |  979 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
|  29615858 |  980 | `			if( pNode->pOp` |
|  41636799 |  981 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
|  26828917 |  982 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  24041924 |  983 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|   5994299 |  984 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|   2997147 |  985 | `			}` |
|         - |  986 | ``			/* `++`/`--` mutate their operand in place — the operand is a write`` |
|         - |  987 | ``			 * lvalue exactly like a compound assign's (`$o->m[0]++` must tag the`` |
|         - |  988 | ``			 * member base PH7_MEMBER_WRITE the way `$o->m[0] += 1` does: hooked`` |
|         - |  989 | `			 * properties throw php's Indirect-modification Error, missing ones` |
|         - |  990 | `			 * auto-vivify). The prec-18 site below handles the assign family;` |
|         - |  991 | ``			 * `++`/`--` are unary, their operand is pLeft. */`` |
|  29615858 |  992 | `			if( pNode->pOp` |
|  29615863 |  993 | `				&& (pNode->pOp->iVmOp == PH7_OP_INCR \|\| pNode->pOp->iVmOp == PH7_OP_DECR) ){` |
|    217835 |  994 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|    108915 |  995 | `			}` |
|         - |  996 | ``			/* `??` reads its LEFT operand in isset-context: an undefined or`` |
|         - |  997 | `			 * UNINITIALIZED typed PROPERTY must yield the default rather than a` |
|         - |  998 | `			 * warning/Error (php). Tag a member-access LHS so its OP_MEMBER takes` |
|         - |  999 | `			 * the silent-lookup path (iP2 = ISSET), which still loads a present` |
|         - | 1000 | `			 * value. A SUBSCRIPT LHS is left untagged: LOAD_IDX's ISSET mode means` |
|         - | 1001 | ``			 * offsetExists (a bool), but `$o[$k] ?? d` needs the offsetGet value —`` |
|         - | 1002 | `			 * that path is already handled correctly by OP_NULLC. */` |
|  29615858 | 1003 | `			if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC` |
|  14837126 | 1004 | `				&& pNode->pLeft && pNode->pLeft->pOp` |
|     87529 | 1005 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|     58338 | 1006 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|     58321 | 1007 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|        37 | 1008 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|        18 | 1009 | `			}` |
|  29615863 | 1010 | `			if( iVmOp == PH7_OP_ERR_CTRL ){` |
|         - | 1011 | `				/* '@' must suppress the diagnostics raised WHILE its operand runs, so` |
|         - | 1012 | `				 * open the window here; the trailing emit below closes it (iP1 = 0). */` |
|     15769 | 1013 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_ERR_CTRL,1,0,0,0);` |
|      7882 | 1014 | `			}` |
|  29615863 | 1015 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags);` |
|         - | 1016 | `		}` |
|  29615863 | 1017 | `		if( rc != SXRET_OK ){` |
|        34 | 1018 | `			return rc;` |
|         - | 1019 | `		}` |
|  29615833 | 1020 | `		if( !bIsChainOp ){` |
|         - | 1021 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|         - | 1022 | `			 * target the end of that LHS chain, which is right here. */` |
|  13532789 | 1023 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|   6766392 | 1024 | `		}` |
|  29615833 | 1025 | `		if( iVmOp == PH7_OP_CALL ){` |
|   7061265 | 1026 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   7061265 | 1027 | `			if( pInstr ){` |
|   7061265 | 1028 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|   5250639 | 1029 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|         - | 1030 | `					sxu32 nQual;` |
|   5250639 | 1031 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 1032 | `					/* Prevent constant expansion but preserve the absolute flag` |
|         - | 1033 | `					 * so the later NEW handler (if any) can see it. */` |
|   5250639 | 1034 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|         - | 1035 | `					/* Namespace-qualify the function name for CALL, unless the` |
|         - | 1036 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|         - | 1037 | `					 * imports — class imports must NOT affect function` |
|         - | 1038 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|         - | 1039 | `					 * before NEW; we store the original literal index in the` |
|         - | 1040 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|         - | 1041 | `					 * the unqualified name and re-qualify with class imports. */` |
|   5250639 | 1042 | `					if( bAbsolute ){` |
|      3925 | 1043 | `						pInstr->iP2 = (sxi32)nOrig;` |
|      1965 | 1044 | `					}else{` |
|   5246719 | 1045 | `						int fromImport = 0;` |
|   5246719 | 1046 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|   5246719 | 1047 | `						pInstr->iP2 = (sxi32)nQual;` |
|   5246719 | 1048 | `						if( nQual != nOrig ){` |
|         - | 1049 | `							/* Record the original literal index in the arg map` |
|         - | 1050 | `							 * (NOT in the CALL's iP2 — that is the hasSpread` |
|         - | 1051 | `							 * flag) so the NEW handler can recover the` |
|         - | 1052 | `							 * unqualified name and re-qualify with CLASS` |
|         - | 1053 | `							 * imports. */` |
|       103 | 1054 | `							if( p3 == 0 ){` |
|       103 | 1055 | `								VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|        98 | 1056 | `									&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|       103 | 1057 | `								if( pMap ){` |
|       103 | 1058 | `									SyZero(pMap, sizeof(VmCallArgMap));` |
|       103 | 1059 | `									p3 = (void *)pMap;` |
|        49 | 1060 | `								}` |
|        49 | 1061 | `							}` |
|       103 | 1062 | `							if( p3 ){` |
|       103 | 1063 | `								((VmCallArgMap *)p3)->nOrigNameLit = nOrig + 1;` |
|       103 | 1064 | `								if( !fromImport ){` |
|         - | 1065 | `									/* Mark as namespace-qualified */` |
|        93 | 1066 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|        44 | 1067 | `								}` |
|        49 | 1068 | `							}` |
|        49 | 1069 | `						}` |
|         - | 1070 | `					}` |
|   4435948 | 1071 | `				}else if( (pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */` |
|   1800568 | 1072 | `						&& !(pNode->pLeft && (pNode->pLeft->iFlags & EXPR_NODE_PARENS)))` |
|    915378 | 1073 | `					\|\| pInstr->iOp == PH7_OP_NEW ){` |
|         - | 1074 | `					/* Method call,flag that. But NOT when the callee was an explicitly` |
|         - | 1075 | ``					 * PARENTHESISED member access: `($o->p)(...)` / `($o::$p)(...)` invokes`` |
|         - | 1076 | `					 * the VALUE of the property (php's variable-invocation), so the OP_MEMBER` |
|         - | 1077 | `					 * must stay a plain property READ (leaving the callable on the stack for` |
|         - | 1078 | `					 * OP_CALL to invoke) rather than being rewritten into a method-name` |
|         - | 1079 | ``					 * resolution — the parens are exactly what distinguishes `($o->p)()` from`` |
|         - | 1080 | ``					 * the method call `$o->p()`. */`` |
|   1790517 | 1081 | `					pInstr->iP2 = 1;` |
|         - | 1082 | ``					/* A static call with a DYNAMIC method name (`C::$m(...)`): the`` |
|         - | 1083 | ``					 * static-`::` codegen folded the variable NAME into OP_MEMBER->p3`` |
|         - | 1084 | ``					 * as if it were a static-PROPERTY read (`C::$m`), but in a CALL the`` |
|         - | 1085 | `					 * method name is the variable's VALUE. Rebuild the sequence` |
|         - | 1086 | `					 * [class, OP_LOAD $m -> value, OP_MEMBER(static, method)] so the` |
|         - | 1087 | `					 * dynamic name is read off the stack, matching the instance` |
|         - | 1088 | ``					 * (`$o->$m()`) path. iP1==1 marks a static member. */`` |
|   1790517 | 1089 | `					if( pInstr->iOp == PH7_OP_MEMBER && pInstr->iP1 == 1 && pInstr->p3 ){` |
|        11 | 1090 | `						void *pDynName = pInstr->p3;` |
|        11 | 1091 | `						(void)PH7_VmPopInstr(pGen->pVm);` |
|        11 | 1092 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,pDynName,0);` |
|        11 | 1093 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_MEMBER,1,PH7_MEMBER_METHOD,0,0);` |
|         5 | 1094 | `					}` |
|    895256 | 1095 | `				}` |
|   3530635 | 1096 | `			}` |
|  26085203 | 1097 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|         - | 1098 | `			ph7_expr_node **apNode;` |
|         - | 1099 | `			sxi32 n;` |
|   3027495 | 1100 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|         - | 1101 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|         - | 1102 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE);` |
|         - | 1103 | `			/* Recurse and generate bytecodes for array index */` |
|   3027495 | 1104 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   5817751 | 1105 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   2790261 | 1106 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   2790261 | 1107 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   2790261 | 1108 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 1109 | `					return rc;` |
|         - | 1110 | `				}` |
|         - | 1111 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   2790261 | 1112 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   1395133 | 1113 | `			}` |
|   3027495 | 1114 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   2790261 | 1115 | `				iP1 = 1; /* Node have an index associated with it */` |
|   1395128 | 1116 | `			}` |
|   3027495 | 1117 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|         - | 1118 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|    372953 | 1119 | `				iP2 = 4;` |
|   2841021 | 1120 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|         - | 1121 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|         - | 1122 | `				 * so the trailing unset() builtin can drop the slot. */` |
|     23373 | 1123 | `				iP2 = 5;` |
|   2642863 | 1124 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|         - | 1125 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|         - | 1126 | `				 * short-circuit on missing keys without invoking offsetGet` |
|         - | 1127 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|        29 | 1128 | `				iP2 = 6;` |
|   2631167 | 1129 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|         - | 1130 | `				/* Create an empty entry when the desired index is not found */` |
|    556039 | 1131 | `				iP2 = 1;` |
|    278022 | 1132 | `			}` |
|  21040828 | 1133 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         - | 1134 | `			/* POP the left node */` |
|         5 | 1135 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         2 | 1136 | `		}` |
|  14807914 | 1137 | `	}` |
|  29669993 | 1138 | `	rc = SXRET_OK;` |
|  29669993 | 1139 | `	nJmpIdx = 0;` |
|         - | 1140 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|         - | 1141 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|         - | 1142 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  29669993 | 1143 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|    466905 | 1144 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    466905 | 1145 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    466905 | 1146 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    466905 | 1147 | `			int isSpecial = 0;` |
|    466905 | 1148 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|    373685 | 1149 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|    373685 | 1150 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|    373680 | 1151 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    348365 | 1152 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    188738 | 1153 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|    147715 | 1154 | `					isSpecial = 1;` |
|     73855 | 1155 | `				}` |
|    210145 | 1156 | `			}` |
|    513515 | 1157 | `			pInstr->iP1 = 0;` |
|         - | 1158 | ``			/* A leading `\` (NSSEP) makes the class name ABSOLUTE: it names the`` |
|         - | 1159 | `			 * global class, so it must NOT be re-qualified with the current namespace.` |
|         - | 1160 | ``			 * `\Closure::bind(...)` inside `namespace X` is Closure, not X\Closure.`` |
|         - | 1161 | ``			 * (Multi-component `\A\B::m` already resolves absolutely because its`` |
|         - | 1162 | ``			 * literal keeps a backslash; the single-component `\Closure` lost it.) */`` |
|         - | 1163 | `			{` |
|    723660 | 1164 | `				int bAbsolute = (pNode->pLeft && pNode->pLeft->pStart` |
|    630435 | 1165 | `					&& (pNode->pLeft->pStart->nType & PH7_TK_NSSEP));` |
|    420295 | 1166 | `				if( !isSpecial && !bAbsolute ){` |
|    272567 | 1167 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|    136281 | 1168 | `				}` |
|         - | 1169 | `			}` |
|         - | 1170 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|         - | 1171 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|    420295 | 1172 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|    272585 | 1173 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|    272585 | 1174 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|        78 | 1175 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|        78 | 1176 | `					return SXRET_OK;` |
|         - | 1177 | `				}` |
|    136253 | 1178 | `			}` |
|    210108 | 1179 | `		}` |
|    303301 | 1180 | `	}` |
|         - | 1181 | `	/* Generate code for the right tree */` |
|  29623327 | 1182 | `	if( pNode->pRight ){` |
|  17013679 | 1183 | `		if( iVmOp == PH7_OP_LAND ){` |
|         - | 1184 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    450819 | 1185 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  16788272 | 1186 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|         - | 1187 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    326251 | 1188 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  16399742 | 1189 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|         - | 1190 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     58399 | 1191 | `			iVmOp = 0; /* No binary operator to emit */` |
|     58399 | 1192 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  16207474 | 1193 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|         - | 1194 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|         - | 1195 | `			 * the entire containing postfix chain to null. The jump target is` |
|         - | 1196 | `			 * patched later by the innermost non-chain ancestor (or by` |
|         - | 1197 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|         - | 1198 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|         - | 1199 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|       107 | 1200 | `			sxu32 nNsJmp = 0;` |
|       107 | 1201 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|       107 | 1202 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  16178173 | 1203 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|         - | 1204 | `			/* The lvalue is the RIGHT operand (these ops are right-associative). Mark it a write` |
|         - | 1205 | ``			 * target so a missing member (the base of a subscript-write, or a bare `$o->p`) is`` |
|         - | 1206 | `			 * auto-created — PHP auto-vivifies on write. */` |
|   5217845 | 1207 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|   2608920 | 1208 | `		}` |
|  17013679 | 1209 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  17013679 | 1210 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  17013679 | 1211 | `		if( !bIsChainOp ){` |
|         - | 1212 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|         - | 1213 | `			 * operator instruction is emitted. */` |
|  11019459 | 1214 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|   5509727 | 1215 | `		}` |
|  17013679 | 1216 | `		if( iVmOp == PH7_OP_STORE ){` |
|   4767187 | 1217 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|   4767150 | 1218 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|         - | 1219 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|         - | 1220 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|         - | 1221 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|         - | 1222 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|         - | 1223 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|         - | 1224 | `				 */` |
|        91 | 1225 | `				iVmOp = 0;` |
|   4767144 | 1226 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|   4767101 | 1227 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 1228 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|    920809 | 1229 | `					iP2 = 1;` |
|    460407 | 1230 | `				}else{` |
|   3846297 | 1231 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 1232 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|    536529 | 1233 | `						iVmOp = PH7_OP_STORE_IDX;` |
|    536529 | 1234 | `						iP1 = pInstr->iP1;` |
|    268267 | 1235 | `					}else{` |
|   3309773 | 1236 | `						p3 = pInstr->p3;` |
|         - | 1237 | `					}` |
|         - | 1238 | `					/* POP the last dynamic load instruction */` |
|   3846297 | 1239 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         - | 1240 | `				}` |
|   2383553 | 1241 | `			}` |
|  14630088 | 1242 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|         - | 1243 | `			/* Peek first: a member LHS ($o->p =& $x, C::$s =& $x) keeps its` |
|         - | 1244 | `			 * OP_MEMBER in place (it resolves + stashes the target slot at` |
|         - | 1245 | `			 * runtime), unlike the variable/array shapes which fold their load` |
|         - | 1246 | `			 * away. Mirrors the normal member-store fold above (iP2 kept-MEMBER). */` |
|        73 | 1247 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|        73 | 1248 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 1249 | `				/* Tag the member as a reference target and flag STORE_REF (iP2=1)` |
|         - | 1250 | `				 * to take the member-rebind path in the VM. */` |
|        11 | 1251 | `				pInstr->iP2 = PH7_MEMBER_REF_TARGET;` |
|        11 | 1252 | `				iP2 = 1;` |
|         6 | 1253 | `			}else{` |
|        63 | 1254 | `				pInstr = PH7_VmPopInstr(pGen->pVm);` |
|        63 | 1255 | `				if( pInstr ){` |
|        63 | 1256 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 1257 | `						/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|         - | 1258 | `						 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|         - | 1259 | `						 */` |
|        19 | 1260 | `						iVmOp = PH7_OP_STORE_IDX_REF;` |
|        19 | 1261 | `						iP1 = pInstr->iP1;` |
|        19 | 1262 | `						iP2 = pInstr->iP2;` |
|        19 | 1263 | `						p3  = pInstr->p3;` |
|        10 | 1264 | `					}else{` |
|        45 | 1265 | `						p3 = pInstr->p3;` |
|         - | 1266 | `					}` |
|        30 | 1267 | `				}` |
|         - | 1268 | `			}` |
|        35 | 1269 | `		}` |
|   8506837 | 1270 | `	}` |
|  29623322 | 1271 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|    432717 | 1272 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|         - | 1273 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|         - | 1274 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|        34 | 1275 | `		iVmOp = 0;` |
|        15 | 1276 | `	}` |
|  29623327 | 1277 | `	if( iVmOp > 0 ){` |
|  29564813 | 1278 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|    217835 | 1279 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|         - | 1280 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|     15561 | 1281 | `				iP1 = 1;` |
|      7783 | 1282 | `			}` |
|  29455898 | 1283 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|         - | 1284 | `			/* Namespace-qualify the class name for NEW */ {` |
|    868659 | 1285 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|    868659 | 1286 | `				VmInstr *pCallInstr = 0;` |
|    868659 | 1287 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|    856813 | 1288 | `					pCallInstr = pPeek;` |
|    856813 | 1289 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    428404 | 1290 | `				}` |
|    868659 | 1291 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|    853125 | 1292 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 1293 | `					sxu32 nLitForClass;` |
|    853125 | 1294 | `					VmCallArgMap *pCallNsMap = pCallInstr ? (VmCallArgMap *)pCallInstr->p3 : 0;` |
|         - | 1295 | `					/* If the CALL handler qualified the name with FUNCTION` |
|         - | 1296 | `					 * imports, recover the original literal (recorded in the` |
|         - | 1297 | `					 * arg map — OP_CALL's iP2 is the hasSpread flag, and` |
|         - | 1298 | `` 					 * misreading it as a literal index made `new C(...$args)` `` |
|         - | 1299 | `					 * fatal with "Class ' ' is not defined") and re-qualify` |
|         - | 1300 | `					 * with class imports. */` |
|    853125 | 1301 | `					if( pCallNsMap && pCallNsMap->nOrigNameLit > 0 ){` |
|        55 | 1302 | `						nLitForClass = pCallNsMap->nOrigNameLit - 1;` |
|        30 | 1303 | `					}else{` |
|    853075 | 1304 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|         - | 1305 | `					}` |
|    853125 | 1306 | `					pPeek->iP1 = 0;` |
|    853125 | 1307 | `					if( !bAbsolute ){` |
|         - | 1308 | `						/* self/static/parent are resolved at runtime against the` |
|         - | 1309 | `						 * current class — never namespace-qualify them (else` |
|         - | 1310 | ``						 * `new self` in namespace N becomes "N\self"). Mirrors the`` |
|         - | 1311 | `						 * instanceof (IS_A) guard below. */` |
|    849215 | 1312 | `						ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nLitForClass);` |
|    849215 | 1313 | `						int isSpecialNew = 0;` |
|    849215 | 1314 | `						if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|    834079 | 1315 | `							const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|    834079 | 1316 | `							sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|    834074 | 1317 | `							if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    837805 | 1318 | `								(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    416985 | 1319 | `								(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      7803 | 1320 | `								isSpecialNew = 1;` |
|      3899 | 1321 | `							}` |
|    420821 | 1322 | `						}` |
|    856783 | 1323 | `						if( isSpecialNew ){` |
|      7803 | 1324 | `							pPeek->iP2 = (sxi32)nLitForClass;` |
|      3904 | 1325 | `						}else{` |
|    833849 | 1326 | `							pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|         - | 1327 | `						}` |
|    420826 | 1328 | `					}else{` |
|      3915 | 1329 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|         - | 1330 | `					}` |
|    422776 | 1331 | `				}` |
|         - | 1332 | `			}` |
|    861091 | 1333 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    861091 | 1334 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|         - | 1335 | `				VmInstr *pPrev;` |
|    856813 | 1336 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|    856813 | 1337 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|         - | 1338 | `					/* Pop the call instruction, preserve named-arg map and` |
|         - | 1339 | `					 * the hasSpread flag (OP_NEW consumes the spread` |
|         - | 1340 | `					 * accumulator exactly like OP_CALL would have). */` |
|    856813 | 1341 | `					iP1 = pInstr->iP1;` |
|    856813 | 1342 | `					iP2 = pInstr->iP2;` |
|    856813 | 1343 | `					if( pInstr->p3 ){` |
|        65 | 1344 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|        30 | 1345 | `					}` |
|    856813 | 1346 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    428404 | 1347 | `				}` |
|    428409 | 1348 | `			}` |
|  28908872 | 1349 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|         - | 1350 | `			/* instanceof: right operand is a class name, not a constant.` |
|         - | 1351 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     81835 | 1352 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     81835 | 1353 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     81835 | 1354 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     81835 | 1355 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     81835 | 1356 | `				int isSpecialIs = 0;` |
|     81835 | 1357 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     81835 | 1358 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     81835 | 1359 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     81830 | 1360 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     81833 | 1361 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     40915 | 1362 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|        12 | 1363 | `						isSpecialIs = 1;` |
|         5 | 1364 | `					}` |
|     40915 | 1365 | `				}` |
|     81835 | 1366 | `				pInstr->iP1 = 0;` |
|     81835 | 1367 | `				if( !isSpecialIs && !bAbsolute ){` |
|     81815 | 1368 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     40905 | 1369 | `				}` |
|     40920 | 1370 | `			}` |
|  28437414 | 1371 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|         - | 1372 | `			/* Prevent constant expansion for member/property names.` |
|         - | 1373 | `			 * The right child (member name) was just compiled — its LOADC` |
|         - | 1374 | `			 * should not trigger constant lookup. */` |
|   5994225 | 1375 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   5994225 | 1376 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|   5753439 | 1377 | `				pInstr->iP1 = 0;` |
|   2876717 | 1378 | `			}` |
|   5994225 | 1379 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|         - | 1380 | `				/* Static member access,remember that */` |
|    420239 | 1381 | `				iP1 = 1;` |
|    420239 | 1382 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    420239 | 1383 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|    236895 | 1384 | `					p3 = pInstr->p3;` |
|    236895 | 1385 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    118445 | 1386 | `				}` |
|    210117 | 1387 | `			}` |
|         - | 1388 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|         - | 1389 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|         - | 1390 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|         - | 1391 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|   5994225 | 1392 | `			if( iP2 == PH7_MEMBER_READ ){` |
|   5994225 | 1393 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|        42 | 1394 | `					iP2 = PH7_MEMBER_UNSET;` |
|   5994205 | 1395 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|     70019 | 1396 | `					iP2 = PH7_MEMBER_ISSET;` |
|   5959178 | 1397 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|        17 | 1398 | `					iP2 = PH7_MEMBER_EMPTY;` |
|   5924163 | 1399 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|         - | 1400 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|   1115095 | 1401 | `					iP2 = PH7_MEMBER_WRITE;` |
|    557545 | 1402 | `				}` |
|   2997110 | 1403 | `			}` |
|   2997110 | 1404 | `		}` |
|         - | 1405 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|         - | 1406 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|         - | 1407 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|         - | 1408 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|         - | 1409 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
|  29557245 | 1410 | `		if( bFcc ){` |
|        81 | 1411 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|        81 | 1412 | `			iP2 = 0;` |
|        81 | 1413 | `			p3 = 0;` |
|        81 | 1414 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|        81 | 1415 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 1416 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|         - | 1417 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|         - | 1418 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|         - | 1419 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|        37 | 1420 | `				void *pMemberName = pInstr->p3;` |
|        37 | 1421 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|        37 | 1422 | `				if( pMemberName ){` |
|       ! 0 | 1423 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|       ! 0 | 1424 | `				}` |
|        37 | 1425 | `				iP1 = 2;` |
|        19 | 1426 | `			}else{` |
|        45 | 1427 | `				iP1 = 1;` |
|         - | 1428 | `			}` |
|        40 | 1429 | `		}` |
|         - | 1430 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|         - | 1431 | `		 * This is the primary emit path for user-visible calls. */` |
|  29557245 | 1432 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|   7922271 | 1433 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|   3961133 | 1434 | `		}` |
|         - | 1435 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  29557245 | 1436 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  14778620 | 1437 | `	}` |
|  29615759 | 1438 | `	if( nJmpIdx > 0 ){` |
|         - | 1439 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|    835459 | 1440 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|    835459 | 1441 | `		if( pInstr ){` |
|    835459 | 1442 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    417727 | 1443 | `		}` |
|    417727 | 1444 | `	}` |
|  29615759 | 1445 | `	return rc;` |
|  38028692 | 1446 | `}` |
|         - | 1447 | `/*` |
|         - | 1448 | ` * Compile a PHP expression.` |
|         - | 1449 | ` * According to the PHP language reference manual:` |
|         - | 1450 | ` *  Expressions are the most important building stones of PHP.` |
|         - | 1451 | ` *  In PHP, almost anything you write is an expression.` |
|         - | 1452 | ` *  The simplest yet most accurate way to define an expression` |
|         - | 1453 | ` *  is "anything that has a value".` |
|         - | 1454 | ` * If something goes wrong while compiling the expression,this` |
|         - | 1455 | ` * function takes care of generating the appropriate error` |
|         - | 1456 | ` * message.` |
|         - | 1457 | ` */` |
|         - | 1458 | `/*` |
|         - | 1459 | ` * Does this expression tree contain a comma OPERATOR node?` |
|         - | 1460 | ` *` |
|         - | 1461 | `` * PH7 shipped `,` as a lowest-precedence binary operator (IMP-0139-COMMA), so`` |
|         - | 1462 | `` * `(1, 2)` and `$x = (f(), $y)` compile and evaluate to the right operand.`` |
|         - | 1463 | ` * php 8 has no comma operator: its grammar only allows comma-separated` |
|         - | 1464 | ` * expression LISTS inside for(...) clauses (call arguments, array literals and` |
|         - | 1465 | ` * list() are split by the parser, never by this node). Accepting it changes the` |
|         - | 1466 | ` * meaning of source php rejects, which §10 classes as a bug — so every context` |
|         - | 1467 | ` * except for() now reports php's parse error.` |
|         - | 1468 | ` */` |
| 252477650 | 1469 | `static int GenStateTreeHasComma(ph7_expr_node *pNode)` |
|         5 | 1470 | `{` |
|         - | 1471 | `	ph7_expr_node **apArg;` |
|         - | 1472 | `	sxu32 n;` |
| 252477655 | 1473 | `	if( pNode == 0 ){` |
| 177500529 | 1474 | `		return 0;` |
|         - | 1475 | `	}` |
|  74977131 | 1476 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         6 | 1477 | `		return 1;` |
|         - | 1478 | `	}` |
|  74977122 | 1479 | `	if( GenStateTreeHasComma(pNode->pLeft) \|\| GenStateTreeHasComma(pNode->pRight)` |
|  74977123 | 1480 | `	 \|\| GenStateTreeHasComma(pNode->pCond) ){` |
|         6 | 1481 | `		return 1;` |
|         - | 1482 | `	}` |
|  74977123 | 1483 | `	apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  85762569 | 1484 | `	for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; n++ ){` |
|  10785451 | 1485 | `		if( GenStateTreeHasComma(apArg[n]) ){` |
|       ! 0 | 1486 | `			return 1;` |
|         - | 1487 | `		}` |
|   5392728 | 1488 | `	}` |
|  74977123 | 1489 | `	return 0;` |
| 126238830 | 1490 | `}` |
|  17134154 | 1491 | `PH7_PRIVATE sxi32 PH7_CompileExpr(` |
|         - | 1492 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 1493 | `	sxi32 iFlags,        /* Control flags */` |
|         - | 1494 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|         - | 1495 | `	)` |
|         5 | 1496 | `{` |
|         - | 1497 | `	ph7_expr_node *pRoot;` |
|         - | 1498 | `	SySet sExprNode;` |
|         - | 1499 | `	SyToken *pEnd;` |
|         - | 1500 | `	sxi32 nExpr;` |
|         - | 1501 | `	sxi32 iNest;` |
|         - | 1502 | `	sxi32 rc;` |
|         - | 1503 | `	sxu32 nNullsafeBase;` |
|         - | 1504 | `	/* Initialize worker variables */` |
|  17134159 | 1505 | `	nExpr = 0;` |
|  17134159 | 1506 | `	pRoot = 0;` |
|         - | 1507 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|         - | 1508 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  17134159 | 1509 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  17134159 | 1510 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  17134159 | 1511 | `	SySetAlloc(&sExprNode,0x10);` |
|  17134159 | 1512 | `	rc = SXRET_OK;` |
|         - | 1513 | `	/* Delimit the expression */` |
|  17134159 | 1514 | `	pEnd = pGen->pIn;` |
|  17134159 | 1515 | `	iNest = 0;` |
| 134132849 | 1516 | `	while( pEnd < pGen->pEnd ){` |
| 127545095 | 1517 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 1518 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|      4725 | 1519 | `			iNest++;` |
| 127542735 | 1520 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|      4735 | 1521 | `			iNest--;` |
| 127538010 | 1522 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  10547275 | 1523 | `			if( iNest <= 0 ){` |
|  10546405 | 1524 | `				break;` |
|         - | 1525 | `			}` |
|       435 | 1526 | `		}` |
| 116998695 | 1527 | `		pEnd++;` |
|         5 | 1528 | `	}` |
|  17134159 | 1529 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|    738679 | 1530 | `		SyToken *pEnd2 = pGen->pIn;` |
|    738679 | 1531 | `		iNest = 0;` |
|         - | 1532 | `		/* Stop at the first comma */` |
|   1622097 | 1533 | `		while( pEnd2 < pEnd ){` |
|    883425 | 1534 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|     50583 | 1535 | `				iNest++;` |
|    858136 | 1536 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|     50583 | 1537 | `				iNest--;` |
|    807558 | 1538 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      6061 | 1539 | `				if( iNest <= 0 ){` |
|         3 | 1540 | `					break;` |
|         - | 1541 | `				}` |
|      3027 | 1542 | `			}` |
|    883423 | 1543 | `			pEnd2++;` |
|         5 | 1544 | `		}` |
|    738679 | 1545 | `		if( pEnd2 <pEnd ){` |
|         3 | 1546 | `			pEnd = pEnd2;` |
|         1 | 1547 | `		}` |
|    369337 | 1548 | `	}` |
|  17134159 | 1549 | `	if( pEnd > pGen->pIn ){` |
|  17110859 | 1550 | `		SyToken *pTmp = pGen->pEnd;` |
|         - | 1551 | `		/* Swap delimiter */` |
|  17110859 | 1552 | `		pGen->pEnd = pEnd;` |
|         - | 1553 | `		/* Try to get an expression tree */` |
|  17110859 | 1554 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  17110854 | 1555 | `		if( rc == SXRET_OK && pRoot && pGen->nCommaExprOk < 1` |
|  16935765 | 1556 | `		 && GenStateTreeHasComma(pRoot) ){` |
|         - | 1557 | `			/* php has no comma operator outside a for() clause */` |
|         6 | 1558 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pRoot->pStart->nLine,` |
|         - | 1559 | `				"syntax error, unexpected token \",\"");` |
|         6 | 1560 | `			pGen->pEnd = pTmp;` |
|         6 | 1561 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 1562 | `				SySetRelease(&sExprNode);` |
|       ! 0 | 1563 | `				return SXERR_ABORT;` |
|         - | 1564 | `			}` |
|         6 | 1565 | `			pGen->pIn = pEnd;` |
|         6 | 1566 | `			SySetRelease(&sExprNode);` |
|         6 | 1567 | `			SySetTruncate(&pGen->aNullsafeJmp,nNullsafeBase);` |
|         6 | 1568 | `			return SXRET_OK;` |
|         - | 1569 | `		}` |
|  17110855 | 1570 | `		if( rc == SXRET_OK && pRoot ){` |
|  17110675 | 1571 | `			rc = SXRET_OK;` |
|  17110675 | 1572 | `			if( xTreeValidator ){` |
|         - | 1573 | `				/* Call the upper layer validator callback */` |
|   1034531 | 1574 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    517263 | 1575 | `			}` |
|  17110675 | 1576 | `			if( rc != SXERR_ABORT ){` |
|         - | 1577 | `				/* Generate code for the given tree */` |
|  17110675 | 1578 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|         - | 1579 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|         - | 1580 | `				 * expression so they short-circuit to its end. */` |
|  17110675 | 1581 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|   8555335 | 1582 | `			}` |
|  17110675 | 1583 | `			nExpr = 1;` |
|   8555335 | 1584 | `		}` |
|         - | 1585 | `		/* Release the whole tree */` |
|  17110855 | 1586 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|         - | 1587 | `		/* Synchronize token stream */` |
|  17110855 | 1588 | `		pGen->pEnd = pTmp;` |
|  17110855 | 1589 | `		pGen->pIn  = pEnd;` |
|  17110855 | 1590 | `		if( rc == SXERR_ABORT ){` |
|        18 | 1591 | `			SySetRelease(&sExprNode);` |
|        18 | 1592 | `			return SXERR_ABORT;` |
|         - | 1593 | `		}` |
|   8555418 | 1594 | `	}` |
|  17134141 | 1595 | `	SySetRelease(&sExprNode);` |
|  17134141 | 1596 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|   8567082 | 1597 | `}` |
|         - | 1598 | `/*` |
|         - | 1599 | ` * Return a pointer to the node construct handler associated` |
|         - | 1600 | ` * with a given node type [i.e: string,integer,float,...].` |
|         - | 1601 | ` */` |
|   9516638 | 1602 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|         5 | 1603 | `{` |
|   9516643 | 1604 | `	if( nNodeType & PH7_TK_NUM ){` |
|         - | 1605 | `		/* Numeric literal: Either real or integer */` |
|   3827251 | 1606 | `		return PH7_CompileNumLiteral;` |
|   5689397 | 1607 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|         - | 1608 | `		/* Double quoted string */` |
|    124995 | 1609 | `		return PH7_CompileString;` |
|   5564407 | 1610 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|         - | 1611 | `		/* Single quoted string */` |
|   5564283 | 1612 | `		return PH7_CompileSimpleString;` |
|       129 | 1613 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|         - | 1614 | `		/* Heredoc */` |
|        73 | 1615 | `		return PH7_CompileHereDoc;` |
|        60 | 1616 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|         - | 1617 | `		/* Nowdoc */` |
|        55 | 1618 | `		return PH7_CompileNowDoc;` |
|         6 | 1619 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|         - | 1620 | `		/* Backtick quoted string */` |
|         3 | 1621 | `		return PH7_CompileBacktic;` |
|         - | 1622 | `	}` |
|         3 | 1623 | `	return 0;` |
|   4758324 | 1624 | `}` |
|         - | 1625 | `/*` |
|         - | 1626 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|         - | 1627 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|         - | 1628 | ` * in write context" parse error.` |
|         - | 1629 | ` */` |
|     23410 | 1630 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|         5 | 1631 | `{` |
|         - | 1632 | `	sxi32 rc;` |
|     23415 | 1633 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|     23413 | 1634 | `		return SXRET_OK;` |
|         - | 1635 | `	}` |
|         5 | 1636 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|         2 | 1637 | `		pNode ? pNode->pStart->nLine : 1,` |
|         - | 1638 | `		"Can't use nullsafe operator in write context");` |
|         3 | 1639 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|     11710 | 1640 | `}` |
|         - | 1641 | `/*` |
|         - | 1642 | ` * Compile an unset() statement.` |
|         - | 1643 | ` * unset($var, $arr[$key], ...);` |
|         - | 1644 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|         - | 1645 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|         - | 1646 | ` * parent array before extracting the element to unset.` |
|         - | 1647 | ` */` |
|     26120 | 1648 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|         5 | 1649 | `{` |
|     26125 | 1650 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|     26125 | 1651 | `	sxu32 nIdx = 0;` |
|         - | 1652 | `	SyString sName;` |
|         - | 1653 | `	sxi32 rc;` |
|         - | 1654 | `	/* Jump the 'unset' keyword */` |
|     26125 | 1655 | `	pGen->pIn++;` |
|         - | 1656 | `	/* Save delimiter */` |
|     26125 | 1657 | `	pTmp = pGen->pEnd;` |
|         - | 1658 | `	/* Skip optional opening parenthesis and find the matching close */` |
|     26125 | 1659 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|     26125 | 1660 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 1661 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|         - | 1662 | `		SyToken *pClose;` |
|     26125 | 1663 | `		pGen->pIn++;   /* Skip '(' */` |
|     26125 | 1664 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|     26125 | 1665 | `		pEnd = pClose; /* Stop at ')' */` |
|     13060 | 1666 | `	}` |
|     26125 | 1667 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|         - | 1668 | `	/* Resolve the 'unset' builtin name once */` |
|     26125 | 1669 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|      3887 | 1670 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3887 | 1671 | `		if( pObj == 0 ){` |
|       ! 0 | 1672 | `			return SXERR_ABORT;` |
|         - | 1673 | `		}` |
|      3887 | 1674 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      3887 | 1675 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      1941 | 1676 | `	}` |
|         - | 1677 | `	/* Compile each comma-separated argument */` |
|     56125 | 1678 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|     30005 | 1679 | `		if( pGen->pIn < pNext ){` |
|         - | 1680 | `			/*` |
|         - | 1681 | ``			 * A PLAIN variable (`unset($x)`, exactly two tokens: '$' and the name) drops a`` |
|         - | 1682 | `			 * single NAME binding, which the unset() builtin cannot express — all it ever` |
|         - | 1683 | `			 * receives is the value's slot index, and unsetting the slot destroys whatever` |
|         - | 1684 | `			 * else aliases it. Emit OP_UNSET_VAR with the name instead. Subscripts and` |
|         - | 1685 | ``			 * properties (`unset($a[k])`, `unset($o->p)`) keep the existing path, which`` |
|         - | 1686 | `			 * already removes just the element/property.` |
|         - | 1687 | `			 */` |
|     30000 | 1688 | `			if( &pGen->pIn[2] == pNext` |
|     18295 | 1689 | `				&& (pGen->pIn[0].nType & PH7_TK_DOLLAR)` |
|      6595 | 1690 | `				&& (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         - | 1691 | `				SyString *pVarName;` |
|      9887 | 1692 | `				char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      6588 | 1693 | `					pGen->pIn[1].sData.zString,pGen->pIn[1].sData.nByte);` |
|      6593 | 1694 | `				pVarName = (SyString *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(SyString));` |
|      6593 | 1695 | `				if( zDup == 0 \|\| pVarName == 0 ){` |
|       ! 0 | 1696 | `					PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - | 1697 | `						"Fatal, PH7 is running out of memory");` |
|       ! 0 | 1698 | `					return SXERR_ABORT;` |
|         - | 1699 | `				}` |
|      6593 | 1700 | `				SyStringInitFromBuf(pVarName,zDup,pGen->pIn[1].sData.nByte);` |
|      6593 | 1701 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_UNSET_VAR,0,0,pVarName,0);` |
|      6593 | 1702 | `				pGen->pIn = pNext;` |
|      6593 | 1703 | `				if( pGen->pIn < pEnd ){` |
|      3881 | 1704 | `					pGen->pIn++; /* Jump the trailing comma */` |
|      1938 | 1705 | `				}` |
|      6593 | 1706 | `				continue;` |
|         - | 1707 | `			}` |
|     23417 | 1708 | `			pGen->pEnd = pNext;` |
|     23417 | 1709 | `			rc = PH7_CompileExpr(&(*pGen),` |
|         - | 1710 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|         - | 1711 | `				GenStateUnsetValidator);` |
|     23417 | 1712 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 1713 | `				return SXERR_ABORT;` |
|         - | 1714 | `			}` |
|     23417 | 1715 | `			if( rc != SXERR_EMPTY ){` |
|         - | 1716 | `				/* Emit call for this single argument */` |
|     23415 | 1717 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     23415 | 1718 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|     23415 | 1719 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     11705 | 1720 | `			}` |
|     11706 | 1721 | `		}` |
|         - | 1722 | `		/* Jump trailing commas */` |
|     23423 | 1723 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|         7 | 1724 | `			pNext++;` |
|         1 | 1725 | `		}` |
|     23417 | 1726 | `		pGen->pIn = pNext;` |
|         5 | 1727 | `	}` |
|         - | 1728 | `	/* Skip past the closing ')' if present */` |
|     26125 | 1729 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|     26125 | 1730 | `		pGen->pIn++;` |
|     13060 | 1731 | `	}` |
|         - | 1732 | `	/* Restore token stream */` |
|     26125 | 1733 | `	pGen->pEnd = pTmp;` |
|     26125 | 1734 | `	return SXRET_OK;` |
|     13065 | 1735 | `}` |
|         - | 1736 | `/*` |
|         - | 1737 | ` * PHP Language construct table.` |
|         - | 1738 | ` */` |
|         - | 1739 | `static const LangConstruct aLangConstruct[] = {` |
|         - | 1740 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|         - | 1741 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|         - | 1742 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|         - | 1743 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|         - | 1744 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|         - | 1745 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|         - | 1746 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|         - | 1747 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|         - | 1748 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|         - | 1749 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|         - | 1750 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|         - | 1751 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|         - | 1752 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|         - | 1753 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|         - | 1754 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|         - | 1755 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|         - | 1756 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|         - | 1757 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|         - | 1758 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|         - | 1759 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|         - | 1760 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|         - | 1761 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|         - | 1762 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|         - | 1763 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|         - | 1764 | `};` |
|         - | 1765 | `/*` |
|         - | 1766 | ` * Return a pointer to the statement handler routine associated` |
|         - | 1767 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|         - | 1768 | ` */` |
|   8456556 | 1769 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|         - | 1770 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|         - | 1771 | `	SyToken *pLookahed  /* Look-ahead token */` |
|         - | 1772 | `	)` |
|         5 | 1773 | `{` |
|   8456561 | 1774 | `	sxu32 n = 0;` |
|  34151026 | 1775 | `	for(;;){` |
|  68302057 | 1776 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|    534339 | 1777 | `			break;` |
|         - | 1778 | `		}` |
|  67767723 | 1779 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|   7922227 | 1780 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|       ! 0 | 1781 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|       ! 0 | 1782 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|         - | 1783 | `					/* 'static' (class context),return null */` |
|       ! 0 | 1784 | `					return 0;` |
|         - | 1785 | `				}` |
|       ! 0 | 1786 | `			}` |
|   7922222 | 1787 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|     11660 | 1788 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|      5837 | 1789 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|         - | 1790 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|         3 | 1791 | `				return 0;` |
|         - | 1792 | `			}` |
|         - | 1793 | `			/* Return a pointer to the handler.` |
|         - | 1794 | `			*/` |
|   7922225 | 1795 | `			return aLangConstruct[n].xConstruct;` |
|         - | 1796 | `		}` |
|  59845501 | 1797 | `		n++;` |
|         5 | 1798 | `	}` |
|    534339 | 1799 | `	if( pLookahed ){` |
|    534339 | 1800 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|     70005 | 1801 | `			return PH7_CompileClassInterface;` |
|    464339 | 1802 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|    397763 | 1803 | `			return PH7_CompileClass;` |
|     66581 | 1804 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      7867 | 1805 | `			return PH7_CompileTrait;` |
|         - | 1806 | `		}` |
|         - | 1807 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|         - | 1808 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|         - | 1809 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|         - | 1810 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     29357 | 1811 | `	}` |
|         - | 1812 | `	/* Not a language construct */` |
|     58719 | 1813 | `	return 0;` |
|   4228283 | 1814 | `}` |
|         - | 1815 | `/*` |
|         - | 1816 | ` * Check if the given keyword is in fact a PHP language construct.` |
|         - | 1817 | ` * Return TRUE on success. FALSE otheriwse.` |
|         - | 1818 | ` */` |
|     58716 | 1819 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|         5 | 1820 | `{` |
|         - | 1821 | `	int rc;` |
|     58721 | 1822 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     58721 | 1823 | `	if( rc == FALSE ){` |
|     58614 | 1824 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|     15898 | 1825 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|         - | 1826 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|         - | 1827 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|         - | 1828 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|         - | 1829 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|         - | 1830 | `			*/` |
|         - | 1831 | `			){` |
|     58611 | 1832 | `				rc = TRUE;` |
|     29303 | 1833 | `		}` |
|     29307 | 1834 | `	}` |
|     58721 | 1835 | `	return rc;` |
|         5 | 1836 | `}` |
|         - | 1837 | `/*` |
|         - | 1838 | ` * Compile a PHP chunk.` |
|         - | 1839 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 1840 | ` * takes care of generating the appropriate error message.` |
|         - | 1841 | ` */` |
|         - | 1842 | `/*` |
|         - | 1843 | ` * Update pGen->sPendingDoc for the statement whose first token is` |
|         - | 1844 | ` * pGen->pIn: when a docblock trivia is keyed to that token's index in` |
|         - | 1845 | ` * the chunk token set it becomes the pending docblock. An existing` |
|         - | 1846 | ` * pending docblock is LEFT in place otherwise: Zend keeps the last-seen` |
|         - | 1847 | ` * doc comment until a declaration consumes it, so a docblock survives` |
|         - | 1848 | ` * intervening non-declaration statements.` |
|         - | 1849 | ` */` |
|  18148404 | 1850 | `PH7_PRIVATE void GenStateSetPendingDoc(ph7_gen_state *pGen)` |
|         5 | 1851 | `{` |
|  18148409 | 1852 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|  18148409 | 1853 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|  18148409 | 1854 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 1855 | `	sxu32 nIdx, n;` |
|  18148404 | 1856 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|   3422853 | 1857 | `	 \|\| pGen->pIn < pBase \|\| pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|         - | 1858 | `		/* Re-tokenized substream (string interpolation, synthesized code):` |
|         - | 1859 | `		 * indexes do not map to the sidecar */` |
|  14725563 | 1860 | `		return;` |
|         - | 1861 | `	}` |
|   3422851 | 1862 | `	nIdx = (sxu32)(pGen->pIn - pBase);` |
|         - | 1863 | `	/* Attributes must be adjacent to their declaration (unlike docblocks):` |
|         - | 1864 | `	 * reset at every boundary, then collect the groups keyed to this token. */` |
|   3422851 | 1865 | `	SySetReset(&pGen->aPendingAttrs);` |
|  10270303 | 1866 | `	for( n = 0 ; n < nT ; n++ ){` |
|   6847457 | 1867 | `		if( aT[n].nTokIdx != nIdx ){` |
|   6839521 | 1868 | `			continue;` |
|         - | 1869 | `		}` |
|      7941 | 1870 | `		if( aT[n].iKind == PH7_TRIVIA_DOC ){` |
|        29 | 1871 | `			pGen->sPendingDoc = aT[n].sText;` |
|      7929 | 1872 | `		}else if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|      7917 | 1873 | `			SySetPut(&pGen->aPendingAttrs,(const void *)&aT[n]);` |
|      3956 | 1874 | `		}` |
|      3973 | 1875 | `	}` |
|   9074207 | 1876 | `}` |
|         - | 1877 | `/*` |
|         - | 1878 | ` * Hand the pending docblock (if any) to a declaration: duplicate it into` |
|         - | 1879 | ` * the VM allocator (the raw script buffer dies after compilation) and` |
|         - | 1880 | ` * clear the pending slot so sibling declarations do not inherit it.` |
|         - | 1881 | ` */` |
|   4883116 | 1882 | `PH7_PRIVATE void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut)` |
|         5 | 1883 | `{` |
|         - | 1884 | `	char *zDup;` |
|   4883121 | 1885 | `	if( SyStringLength(&pGen->sPendingDoc) < 1 ){` |
|   4883101 | 1886 | `		return;` |
|         - | 1887 | `	}` |
|        35 | 1888 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        10 | 1889 | `		SyStringData(&pGen->sPendingDoc),SyStringLength(&pGen->sPendingDoc));` |
|        25 | 1890 | `	if( zDup ){` |
|        25 | 1891 | `		SyStringInitFromBuf(pOut,zDup,SyStringLength(&pGen->sPendingDoc));` |
|        10 | 1892 | `	}` |
|        25 | 1893 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|   2441563 | 1894 | `}` |
|         - | 1895 | `/*` |
|         - | 1896 | ` * Compile one recorded #[...] attribute group (the span between the group` |
|         - | 1897 | ` * delimiters) into ph7_attribute records appended to pOut. The span is` |
|         - | 1898 | ` * duplicated into the VM allocator FIRST (compiled bytecode and interned` |
|         - | 1899 | ` * names may point into the token text, which must outlive the raw script` |
|         - | 1900 | ` * buffer), then re-tokenized on its own. Each argument expression compiles` |
|         - | 1901 | ` * with the container-swap idiom into its own OP_DONE-terminated set,` |
|         - | 1902 | ` * evaluated lazily at ReflectionAttribute time (PHP semantics).` |
|         - | 1903 | ` */` |
|      7926 | 1904 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut)` |
|         5 | 1905 | `{` |
|         - | 1906 | `	SySet *pToken;` |
|         - | 1907 | `	SyToken *pIn, *pEnd, *pSavedIn, *pSavedEnd;` |
|         - | 1908 | `	char *zSpan;` |
|      7931 | 1909 | `	sxi32 rc = SXRET_OK;` |
|      7931 | 1910 | `	if( SyStringLength(&pTrivia->sText) < 1 ){` |
|       ! 0 | 1911 | `		return SXRET_OK;` |
|         - | 1912 | `	}` |
|     11894 | 1913 | `	zSpan = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      3963 | 1914 | `		SyStringData(&pTrivia->sText),SyStringLength(&pTrivia->sText));` |
|      7931 | 1915 | `	if( zSpan == 0 ){` |
|       ! 0 | 1916 | `		return SXRET_OK;` |
|         - | 1917 | `	}` |
|         - | 1918 | `	/* The token set must outlive compilation too: interned operands may` |
|         - | 1919 | `	 * reference token payloads. Pool-allocated, never released — bounded by` |
|         - | 1920 | `	 * the number of attribute declarations in the program. */` |
|      7931 | 1921 | `	pToken = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      7931 | 1922 | `	if( pToken == 0 ){` |
|       ! 0 | 1923 | `		return SXRET_OK;` |
|         - | 1924 | `	}` |
|      7931 | 1925 | `	SySetInit(pToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      7931 | 1926 | `	PH7_TokenizePHP(zSpan,SyStringLength(&pTrivia->sText),pTrivia->nLine,pToken,0);` |
|      7931 | 1927 | `	pIn = (SyToken *)SySetBasePtr(pToken);` |
|      7931 | 1928 | `	pEnd = &pIn[SySetUsed(pToken)];` |
|      7931 | 1929 | `	pSavedIn = pGen->pIn;` |
|      7931 | 1930 | `	pSavedEnd = pGen->pEnd;` |
|      7935 | 1931 | `	while( pIn < pEnd ){` |
|         - | 1932 | `		ph7_attribute sAttr;` |
|         - | 1933 | `		SyBlob sFQN;` |
|      7935 | 1934 | `		int bAbsolute = 0;` |
|      7935 | 1935 | `		SyZero(&sAttr,sizeof(sAttr));` |
|      7935 | 1936 | `		SySetInit(&sAttr.aArgs,&pGen->pVm->sAllocator,sizeof(ph7_attr_arg));` |
|      7935 | 1937 | `		sAttr.nLine = pIn->nLine;` |
|      7935 | 1938 | `		if( pIn->nType & PH7_TK_NSSEP ){` |
|        75 | 1939 | `			bAbsolute = 1;` |
|        75 | 1940 | `			pIn++;` |
|        35 | 1941 | `		}` |
|      7935 | 1942 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      7935 | 1943 | `		while( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      7935 | 1944 | `			SyBlobAppend(&sFQN,pIn->sData.zString,pIn->sData.nByte);` |
|      7935 | 1945 | `			pIn++;` |
|      7935 | 1946 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       ! 0 | 1947 | `				SyBlobAppend(&sFQN,"\\",1);` |
|       ! 0 | 1948 | `				pIn++;` |
|       ! 0 | 1949 | `				continue;` |
|         - | 1950 | `			}` |
|      7935 | 1951 | `			break;` |
|       ! 0 | 1952 | `		}` |
|      7935 | 1953 | `		if( SyBlobLength(&sFQN) < 1 ){` |
|         - | 1954 | `			/* Malformed group: stop quietly (the group was inert trivia before` |
|         - | 1955 | `			 * this feature; never turn it into a new fatal) */` |
|       ! 0 | 1956 | `			SyBlobRelease(&sFQN);` |
|       ! 0 | 1957 | `			break;` |
|         - | 1958 | `		}` |
|         - | 1959 | `		/* Resolve to an FQN: absolute names verbatim; else use-import alias,` |
|         - | 1960 | `		 * else current-namespace prefix (PHP attribute name resolution) */` |
|         - | 1961 | `		{` |
|      7935 | 1962 | `			const char *zName = (const char *)SyBlobData(&sFQN);` |
|      7935 | 1963 | `			sxu32 nName = SyBlobLength(&sFQN);` |
|      7935 | 1964 | `			char *zDup = 0;` |
|      7935 | 1965 | `			if( !bAbsolute ){` |
|      7865 | 1966 | `				SyHashEntry *pImp = SyHashGet(&pGen->hUseImports,(const void *)zName,nName);` |
|      7865 | 1967 | `				if( pImp ){` |
|       ! 0 | 1968 | `					const char *zFqn = (const char *)pImp->pUserData;` |
|       ! 0 | 1969 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFqn,SyStrlen(zFqn));` |
|       ! 0 | 1970 | `					if( zDup ){` |
|       ! 0 | 1971 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyStrlen(zDup));` |
|       ! 0 | 1972 | `					}` |
|      7865 | 1973 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         - | 1974 | `					SyBlob sTmp;` |
|       ! 0 | 1975 | `					SyBlobInit(&sTmp,&pGen->pVm->sAllocator);` |
|       ! 0 | 1976 | `					SyBlobAppend(&sTmp,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       ! 0 | 1977 | `					SyBlobAppend(&sTmp,"\\",1);` |
|       ! 0 | 1978 | `					SyBlobAppend(&sTmp,zName,nName);` |
|       ! 0 | 1979 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       ! 0 | 1980 | `						(const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|       ! 0 | 1981 | `					if( zDup ){` |
|       ! 0 | 1982 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyBlobLength(&sTmp));` |
|       ! 0 | 1983 | `					}` |
|       ! 0 | 1984 | `					SyBlobRelease(&sTmp);` |
|       ! 0 | 1985 | `				}` |
|      3930 | 1986 | `			}` |
|      7935 | 1987 | `			if( SyStringLength(&sAttr.sName) < 1 ){` |
|      7935 | 1988 | `				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|      7935 | 1989 | `				if( zDup ){` |
|      7935 | 1990 | `					SyStringInitFromBuf(&sAttr.sName,zDup,nName);` |
|      3965 | 1991 | `				}` |
|      3965 | 1992 | `			}` |
|         - | 1993 | `		}` |
|      7935 | 1994 | `		SyBlobRelease(&sFQN);` |
|      7935 | 1995 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 1996 | `			SyToken *pArgsEnd;` |
|      7833 | 1997 | `			pIn++;` |
|      7833 | 1998 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pArgsEnd);` |
|     15675 | 1999 | `			while( pIn < pArgsEnd ){` |
|      7847 | 2000 | `				SyToken *pArgStart = pIn, *pArgStop = pIn;` |
|      7847 | 2001 | `				sxi32 iDepth = 0;` |
|         - | 2002 | `				ph7_attr_arg sArgRec;` |
|     77917 | 2003 | `				while( pArgStop < pArgsEnd ){` |
|     70091 | 2004 | `					if( pArgStop->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        11 | 2005 | `						iDepth++;` |
|     70086 | 2006 | `					}else if( pArgStop->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        11 | 2007 | `						iDepth--;` |
|     70076 | 2008 | `					}else if( (pArgStop->nType & PH7_TK_COMMA) && iDepth == 0 ){` |
|        17 | 2009 | `						break;` |
|         - | 2010 | `					}` |
|     70075 | 2011 | `					pArgStop++;` |
|         5 | 2012 | `				}` |
|      7847 | 2013 | `				SyZero(&sArgRec,sizeof(sArgRec));` |
|      7847 | 2014 | `				SySetInit(&sArgRec.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      7842 | 2015 | `				if( pArgStart < pArgStop && (pArgStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|      7824 | 2016 | `				 && &pArgStart[1] < pArgStop && (pArgStart[1].nType & PH7_TK_COLON) ){` |
|        28 | 2017 | `					char *zN = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|         9 | 2018 | `						pArgStart->sData.zString,pArgStart->sData.nByte);` |
|        19 | 2019 | `					if( zN ){` |
|        19 | 2020 | `						SyStringInitFromBuf(&sArgRec.sName,zN,pArgStart->sData.nByte);` |
|         9 | 2021 | `					}` |
|        19 | 2022 | `					pArgStart += 2;` |
|         9 | 2023 | `				}` |
|      7847 | 2024 | `				if( pArgStart < pArgStop ){` |
|         - | 2025 | `					SySet *pInstrContainer;` |
|      7847 | 2026 | `					pGen->pIn = pArgStart;` |
|      7847 | 2027 | `					pGen->pEnd = pArgStop;` |
|      7847 | 2028 | `					pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      7847 | 2029 | `					PH7_VmSetByteCodeContainer(pGen->pVm,&sArgRec.aByteCode);` |
|      7847 | 2030 | `					rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      7847 | 2031 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      7847 | 2032 | `					PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      7847 | 2033 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 2034 | `						pGen->pIn = pSavedIn;` |
|       ! 0 | 2035 | `						pGen->pEnd = pSavedEnd;` |
|       ! 0 | 2036 | `						return SXERR_ABORT;` |
|         - | 2037 | `					}` |
|      7847 | 2038 | `					SySetPut(&sAttr.aArgs,(const void *)&sArgRec);` |
|      3921 | 2039 | `				}` |
|      7847 | 2040 | `				pIn = pArgStop;` |
|      7847 | 2041 | `				if( pIn < pArgsEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        17 | 2042 | `					pIn++;` |
|         8 | 2043 | `				}` |
|         5 | 2044 | `			}` |
|      7833 | 2045 | `			pIn = (pArgsEnd < pEnd) ? &pArgsEnd[1] : pEnd;` |
|      3914 | 2046 | `		}` |
|      7935 | 2047 | `		SySetPut(pOut,(const void *)&sAttr);` |
|      7935 | 2048 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|         5 | 2049 | `			pIn++;` |
|         5 | 2050 | `			continue;` |
|         - | 2051 | `		}` |
|      7931 | 2052 | `		break;` |
|       ! 0 | 2053 | `	}` |
|      7931 | 2054 | `	pGen->pIn = pSavedIn;` |
|      7931 | 2055 | `	pGen->pEnd = pSavedEnd;` |
|      7931 | 2056 | `	return SXRET_OK;` |
|      3968 | 2057 | `}` |
|         - | 2058 | `/*` |
|         - | 2059 | ` * Hand the pending attribute groups (if any) to a declaration: compile` |
|         - | 2060 | ` * every recorded group into pOut and clear the pending list.` |
|         - | 2061 | ` */` |
|   4883120 | 2062 | `PH7_PRIVATE sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut)` |
|         5 | 2063 | `{` |
|   4883125 | 2064 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|         - | 2065 | `	sxu32 n;` |
|         - | 2066 | `	sxi32 rc;` |
|   4891037 | 2067 | `	for( n = 0 ; n < SySetUsed(&pGen->aPendingAttrs) ; n++ ){` |
|      7917 | 2068 | `		rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|      7917 | 2069 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 2070 | `			return SXERR_ABORT;` |
|         - | 2071 | `		}` |
|      3961 | 2072 | `	}` |
|   4883125 | 2073 | `	SySetReset(&pGen->aPendingAttrs);` |
|   4883125 | 2074 | `	return SXRET_OK;` |
|   2441565 | 2075 | `}` |
|         - | 2076 | `/*` |
|         - | 2077 | ` * Compile the attribute groups keyed to the given token (a parameter's` |
|         - | 2078 | ` * first token inside a signature) into pOut. Parameters are parsed from` |
|         - | 2079 | ` * the main token stream, so the sidecar indexes map directly.` |
|         - | 2080 | ` */` |
|   2457196 | 2081 | `PH7_PRIVATE sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut)` |
|         5 | 2082 | `{` |
|   2457201 | 2083 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|   2457201 | 2084 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|   2457201 | 2085 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 2086 | `	sxu32 nIdx, n;` |
|         - | 2087 | `	sxi32 rc;` |
|   2457196 | 2088 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|    555427 | 2089 | `	 \|\| pTok < pBase \|\| pTok >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|   1901779 | 2090 | `		return SXRET_OK;` |
|         - | 2091 | `	}` |
|    555427 | 2092 | `	nIdx = (sxu32)(pTok - pBase);` |
|   1666315 | 2093 | `	for( n = 0 ; n < nT ; n++ ){` |
|   1110893 | 2094 | `		if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|        16 | 2095 | `			rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|        16 | 2096 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 2097 | `				return SXERR_ABORT;` |
|         - | 2098 | `			}` |
|         7 | 2099 | `		}` |
|    555449 | 2100 | `	}` |
|    555427 | 2101 | `	return SXRET_OK;` |
|   1228603 | 2102 | `}` |
|  13315712 | 2103 | `PH7_PRIVATE sxi32 GenStateCompileChunk(` |
|         - | 2104 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 2105 | `	sxi32 iFlags         /* Compile flags */` |
|         - | 2106 | `	)` |
|         5 | 2107 | `{` |
|         - | 2108 | `	ProcLangConstruct xCons;` |
|         - | 2109 | `	sxi32 rc;` |
|  13315717 | 2110 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|   7739486 | 2111 | `	for(;;){` |
|  14397347 | 2112 | `		int bStmtIsDeclare = 0;` |
|  14397347 | 2113 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 2114 | `			/* No more input to process */` |
|     91471 | 2115 | `			break;` |
|         - | 2116 | `		}` |
|         - | 2117 | `		/* Bind a directly-preceding docblock to this statement */` |
|  14305881 | 2118 | `		GenStateSetPendingDoc(&(*pGen));` |
|  14305881 | 2119 | `		if( SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|         - | 2120 | `			/* php: a statement-position attribute group must be followed by a` |
|         - | 2121 | ``			 * declaration (function/class-like/const) — `#[A] $x = 1;` is a`` |
|         - | 2122 | `` 			 * parse error, never a silent discard. `static`/`fn`/`function` `` |
|         - | 2123 | ``			 * cover bare closure-expression statements; `readonly`/`enum` are`` |
|         - | 2124 | `			 * context-sensitive IDs handled by the modified-class/enum scans. */` |
|      7829 | 2125 | `			int bAttrTarget = 0;` |
|      7824 | 2126 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd)` |
|      3947 | 2127 | `			 \|\| GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|      7769 | 2128 | `				bAttrTarget = 1;` |
|      3943 | 2129 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|        61 | 2130 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        60 | 2131 | `				if( nKw == PH7_TKWRD_FUNCTION \|\| nKw == PH7_TKWRD_CLASS` |
|        16 | 2132 | `				 \|\| nKw == PH7_TKWRD_INTERFACE \|\| nKw == PH7_TKWRD_TRAIT` |
|         4 | 2133 | `				 \|\| nKw == PH7_TKWRD_ABSTRACT \|\| nKw == PH7_TKWRD_FINAL` |
|         4 | 2134 | `				 \|\| nKw == PH7_TKWRD_CONST \|\| nKw == PH7_TKWRD_STATIC` |
|         1 | 2135 | `				 \|\| nKw == PH7_TKWRD_FN ){` |
|        61 | 2136 | `					bAttrTarget = 1;` |
|        30 | 2137 | `				}` |
|        30 | 2138 | `			}` |
|      7829 | 2139 | `			if( !bAttrTarget ){` |
|       ! 0 | 2140 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 2141 | `					"syntax error, unexpected token \"%z\" after attribute group; expecting a declaration",` |
|       ! 0 | 2142 | `					&pGen->pIn->sData);` |
|       ! 0 | 2143 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 2144 | `					break;` |
|         - | 2145 | `				}` |
|       ! 0 | 2146 | `				SySetReset(&pGen->aPendingAttrs);` |
|       ! 0 | 2147 | `			}` |
|      3912 | 2148 | `		}` |
|         - | 2149 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|         - | 2150 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  14305881 | 2151 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   8499333 | 2152 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   8499333 | 2153 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|        49 | 2154 | `				bStmtIsDeclare = 1;` |
|        22 | 2155 | `			}` |
|   4249664 | 2156 | `		}` |
|  14305881 | 2157 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|         - | 2158 | `			/* Any non-declare top-level statement locks the strict_types` |
|         - | 2159 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|   1081621 | 2160 | `			pGen->bStrictTypesLocked = 1;` |
|    540808 | 2161 | `		}` |
|  14305881 | 2162 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 2163 | `			/* Compile block */` |
|      3927 | 2164 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      3927 | 2165 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 2166 | `				break;` |
|         - | 2167 | `			}` |
|      1966 | 2168 | `		}else{` |
|  14301959 | 2169 | `			xCons = 0;` |
|  14301959 | 2170 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|         - | 2171 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|         - | 2172 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|         - | 2173 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|     42803 | 2174 | `				xCons = PH7_CompileClassModifiers;` |
|  14280560 | 2175 | `			}else if( GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|         - | 2176 | ``				/* `enum Name …` (PHP 8.1) — `enum` is a context-sensitive ID,`` |
|         - | 2177 | `				 * so it is detected here rather than the keyword dispatcher. */` |
|      3923 | 2178 | `				xCons = PH7_CompileEnum;` |
|  14257202 | 2179 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   8456561 | 2180 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         - | 2181 | `				/* Try to extract a language construct handler */` |
|   8456561 | 2182 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|   8456561 | 2183 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|        13 | 2184 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 2185 | `						"Syntax error: Unexpected keyword '%z'",` |
|         8 | 2186 | `						&pGen->pIn->sData);` |
|         9 | 2187 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 2188 | `						break;` |
|         - | 2189 | `					}` |
|         - | 2190 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|         - | 2191 | `					 * this erroneous statement.` |
|         - | 2192 | `					 */` |
|         9 | 2193 | `					xCons = PH7_ErrorRecover;` |
|         4 | 2194 | `				}` |
|  10026965 | 2195 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|    428201 | 2196 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|         - | 2197 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|       117 | 2198 | `				xCons = PH7_CompileLabel;` |
|        56 | 2199 | `			}` |
|  14301959 | 2200 | `			if( xCons == 0 ){` |
|         - | 2201 | `				/* Assume an expression an try to compile it */` |
|   5857283 | 2202 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   5857283 | 2203 | `				if(  rc != SXERR_EMPTY ){` |
|         - | 2204 | `					/* Pop l-value */` |
|   5857133 | 2205 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|   2928564 | 2206 | `				}` |
|   2928644 | 2207 | `			}else{` |
|         - | 2208 | `				/* Go compile the sucker */` |
|   8444681 | 2209 | `				rc = xCons(&(*pGen));` |
|         - | 2210 | `			}` |
|  14301959 | 2211 | `			if( rc == SXERR_ABORT ){` |
|         - | 2212 | `				/* Request to abort compilation */` |
|        34 | 2213 | `				break;` |
|         - | 2214 | `			}` |
|         - | 2215 | `		}` |
|         - | 2216 | `		/* Ignore trailing semi-colons ';' */` |
|  24565993 | 2217 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  10260147 | 2218 | `			pGen->pIn++;` |
|         5 | 2219 | `		}` |
|  14305851 | 2220 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|         - | 2221 | `			/* Compile a single statement and return */` |
|  13224221 | 2222 | `			break;` |
|         - | 2223 | `		}` |
|         - | 2224 | `		/* LOOP ONE */` |
|         - | 2225 | `		/* LOOP TWO */` |
|         - | 2226 | `		/* LOOP THREE */` |
|         - | 2227 | `		/* LOOP FOUR */` |
|         5 | 2228 | `	}` |
|         - | 2229 | `	/* Return compilation status */` |
|  13315717 | 2230 | `	return rc;` |
|         5 | 2231 | `}` |
|         - | 2232 | `/*` |
|         - | 2233 | ` * Compile a Raw PHP chunk.` |
|         - | 2234 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 2235 | ` * takes care of generating the appropriate error message.` |
|         - | 2236 | ` */` |
|     91498 | 2237 | `static sxi32 PH7_CompilePHP(` |
|         - | 2238 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 2239 | `	SySet *pTokenSet,     /* Token set */` |
|         - | 2240 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|         - | 2241 | `	)` |
|         5 | 2242 | `{` |
|     91503 | 2243 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|         - | 2244 | `	sxi32 rc;` |
|         - | 2245 | `	/* Reset the token set (and its trivia sidecar) */` |
|     91503 | 2246 | `	SySetReset(&(*pTokenSet));` |
|     91503 | 2247 | `	SySetReset(&pGen->aTrivia);` |
|         - | 2248 | `	/* Mark as the default token set */` |
|     91503 | 2249 | `	pGen->pTokenSet = &(*pTokenSet);` |
|         - | 2250 | `	/* Advance the stream cursor */` |
|     91503 | 2251 | `	pGen->pRawIn++;` |
|         - | 2252 | `	/* Tokenize the PHP chunk first */` |
|     91503 | 2253 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet),&pGen->aTrivia);` |
|         - | 2254 | `	/* Point to the head and tail of the token stream. */` |
|     91503 | 2255 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     91503 | 2256 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|     91503 | 2257 | `	if( is_expr ){` |
|       ! 0 | 2258 | `		rc = SXERR_EMPTY;` |
|       ! 0 | 2259 | `		if( pGen->pIn < pGen->pEnd ){` |
|         - | 2260 | `			/* A simple expression,compile it */` |
|       ! 0 | 2261 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|       ! 0 | 2262 | `		}` |
|         - | 2263 | `		/* Emit the DONE instruction */` |
|       ! 0 | 2264 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       ! 0 | 2265 | `		return SXRET_OK;` |
|         - | 2266 | `	}` |
|     91503 | 2267 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|         - | 2268 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|         - | 2269 | `		/*` |
|         - | 2270 | `		 * Shortcut syntax for the 'echo' language construct.` |
|         - | 2271 | `		 * According to the PHP reference manual:` |
|         - | 2272 | `		 *  echo() also has a shortcut syntax, where you can` |
|         - | 2273 | `		 *  immediately follow` |
|         - | 2274 | `		 *  the opening tag with an equals sign as follows:` |
|         - | 2275 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|         - | 2276 | `		 * Symisc extension:` |
|         - | 2277 | `		 *   This short syntax works with all PHP opening` |
|         - | 2278 | `		 *   tags unlike the default PHP engine that handle` |
|         - | 2279 | `		 *   only short tag.` |
|         - | 2280 | `		 */` |
|         - | 2281 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|         3 | 2282 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|         3 | 2283 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|         3 | 2284 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|         3 | 2285 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|         3 | 2286 | `		if( rc != SXERR_EMPTY ){` |
|         3 | 2287 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         1 | 2288 | `		}` |
|         3 | 2289 | `		return SXRET_OK;` |
|         - | 2290 | `	}` |
|         - | 2291 | `	/* Compile the PHP chunk */` |
|     91501 | 2292 | `	rc = GenStateCompileChunk(pGen,0);` |
|         - | 2293 | `	/* Fix exceptions jumps */` |
|     91501 | 2294 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - | 2295 | `	/* Fix gotos now, the jump destination is resolved */` |
|     91501 | 2296 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|         3 | 2297 | `		rc = SXERR_ABORT;` |
|         1 | 2298 | `	}` |
|         - | 2299 | `	/* Reset container */` |
|     91501 | 2300 | `	SySetReset(&pGen->aGoto);` |
|     91501 | 2301 | `	SySetReset(&pGen->aLabel);` |
|     91501 | 2302 | `	SySetReset(&pGen->aNullsafeJmp);` |
|         - | 2303 | `	/* Compilation result */` |
|     91501 | 2304 | `	return rc;` |
|     45754 | 2305 | `}` |
|         - | 2306 | `/*` |
|         - | 2307 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|         - | 2308 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|         - | 2309 | ` * This is the only compile interface exported from this file.` |
|         - | 2310 | ` */` |
|     94648 | 2311 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|         - | 2312 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|         - | 2313 | `	SyString *pScript,  /* Script to compile */` |
|         - | 2314 | `	sxi32 iFlags        /* Compile flags */` |
|         - | 2315 | `	)` |
|         5 | 2316 | `{` |
|         - | 2317 | `	SySet aPhpToken,aRawToken;` |
|         - | 2318 | `	ph7_gen_state *pCodeGen;` |
|         - | 2319 | `	ph7_value *pRawObj;` |
|         - | 2320 | `	sxu32 nObjIdx;` |
|         - | 2321 | `	sxi32 nRawObj;` |
|         - | 2322 | `	int is_expr;` |
|         - | 2323 | `	sxi8 bSavedStrict;` |
|         - | 2324 | `	sxi8 bSavedStrictLocked;` |
|         - | 2325 | `	SyToken *pSavedIn,*pSavedEnd;` |
|         - | 2326 | `	sxi32 rc;` |
|     94653 | 2327 | `	sxu32 nBaseLine = 1;` |
|     94653 | 2328 | `	if( pScript->nByte < 1 ){` |
|         - | 2329 | `		/* Nothing to compile */` |
|       ! 0 | 2330 | `		return PH7_OK;` |
|         - | 2331 | `	}` |
|         - | 2332 | `	/* php skips a "#!" shebang on the first line of a CLI script: consume it` |
|         - | 2333 | `	 * (including its newline) so it is not echoed as inline text, and bump the` |
|         - | 2334 | `	 * base line to 2 so the code below still reports php-matching line numbers. */` |
|     94653 | 2335 | `	if( pScript->nByte >= 2 && pScript->zString[0]=='#' && pScript->zString[1]=='!' ){` |
|         3 | 2336 | `		const char *z = pScript->zString;` |
|         3 | 2337 | `		const char *zEnd = &z[pScript->nByte];` |
|        39 | 2338 | `		while( z < zEnd && z[0] != '\n' ){ z++; }` |
|         3 | 2339 | `		if( z < zEnd ){ z++; } /* consume the newline too */` |
|         3 | 2340 | `		pScript->nByte -= (sxu32)(z - pScript->zString);` |
|         3 | 2341 | `		pScript->zString = z;` |
|         3 | 2342 | `		nBaseLine = 2;` |
|         3 | 2343 | `		if( pScript->nByte < 1 ){` |
|       ! 0 | 2344 | `			return PH7_OK;` |
|         - | 2345 | `		}` |
|         1 | 2346 | `	}` |
|         - | 2347 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|         - | 2348 | `	 * file's flags so include/require restore them on return. */` |
|     94653 | 2349 | `	pCodeGen = &pVm->sCodeGen;` |
|         - | 2350 | ``	/* aPhpToken below is a LOCAL token set: once it is released at `cleanup`, any`` |
|         - | 2351 | `	 * pGen->pIn/pEnd still pointing into it DANGLE. PH7_VmEmitInstr reads pIn to stamp` |
|         - | 2352 | `	 * each instruction's source line, and instructions are still emitted after this` |
|         - | 2353 | `	 * function returns (VmEvalChunk's trailing OP_DONE) -- a use-after-free ASan caught` |
|         - | 2354 | `	 * immediately. Save the caller's cursor and restore it on the way out, so an` |
|         - | 2355 | `	 * enclosing compile keeps its (live) tokens and the top level goes back to NULL. */` |
|     94653 | 2356 | `	pSavedIn = pCodeGen->pIn;` |
|     94653 | 2357 | `	pSavedEnd = pCodeGen->pEnd;` |
|     94653 | 2358 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|     94653 | 2359 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|     94653 | 2360 | `	pCodeGen->bStrictTypes = 0;` |
|     94653 | 2361 | `	pCodeGen->bStrictTypesLocked = 0;` |
|         - | 2362 | `	/* Initialize the tokens containers */` |
|     94653 | 2363 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|     94653 | 2364 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|     94653 | 2365 | `	SySetAlloc(&aPhpToken,0xc0);` |
|     94653 | 2366 | `	is_expr = 0;` |
|     94653 | 2367 | `	if( iFlags & PH7_PHP_ONLY ){` |
|         - | 2368 | `		SyToken sTmp;` |
|         - | 2369 | `		/* PHP only: -*/` |
|     81647 | 2370 | `		sTmp.nLine = 1;` |
|     81647 | 2371 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|     81647 | 2372 | `		sTmp.pUserData = 0;` |
|     81647 | 2373 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|     81647 | 2374 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|     81647 | 2375 | `		if( iFlags & PH7_PHP_EXPR ){` |
|         - | 2376 | `			/* A simple PHP expression */` |
|       ! 0 | 2377 | `			is_expr = 1;` |
|       ! 0 | 2378 | `		}` |
|     40826 | 2379 | `	}else{` |
|         - | 2380 | `		/* Tokenize raw text */` |
|     13011 | 2381 | `		SySetAlloc(&aRawToken,32);` |
|     13011 | 2382 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken,nBaseLine);` |
|         - | 2383 | `	}` |
|         - | 2384 | `	/* Process high-level tokens */` |
|     94653 | 2385 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|     94653 | 2386 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|     94653 | 2387 | `	rc = PH7_OK;` |
|     94653 | 2388 | `	if( is_expr ){` |
|         - | 2389 | `		/* Compile the expression */` |
|       ! 0 | 2390 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|       ! 0 | 2391 | `		goto cleanup;` |
|         - | 2392 | `	}` |
|     94653 | 2393 | `	nObjIdx = 0;` |
|         - | 2394 | `	/* Each compilation unit starts in the global namespace.` |
|         - | 2395 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|         - | 2396 | `	 * preventing namespace bleeding across include()d files. */` |
|     94653 | 2397 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|         - | 2398 | `	/* Start the compilation process */` |
|     53832 | 2399 | `	for(;;){` |
|    199135 | 2400 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|     94621 | 2401 | `			break; /* No more tokens to process */` |
|         - | 2402 | `		}` |
|    104519 | 2403 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|         - | 2404 | `			/* Compile the PHP chunk */` |
|     91503 | 2405 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|     91503 | 2406 | `			if( rc == SXERR_ABORT ){` |
|        36 | 2407 | `				break;` |
|         - | 2408 | `			}` |
|     91471 | 2409 | `			continue;` |
|         - | 2410 | `		}` |
|         - | 2411 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|     13021 | 2412 | `		nRawObj = 0;` |
|     26037 | 2413 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|         - | 2414 | `			/* Consume the raw chunk without any processing */` |
|     13021 | 2415 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|     13021 | 2416 | `			if( pRawObj == 0 ){` |
|       ! 0 | 2417 | `				rc = SXERR_MEM;` |
|       ! 0 | 2418 | `				break;` |
|         - | 2419 | `			}` |
|         - | 2420 | `			/* Mark as constant and emit the load constant instruction */` |
|     13021 | 2421 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|     13021 | 2422 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     13021 | 2423 | `			++nRawObj;` |
|     13021 | 2424 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|         5 | 2425 | `		}` |
|     13021 | 2426 | `		if( nRawObj > 0 ){` |
|         - | 2427 | `			/* Emit the consume instruction */` |
|     13021 | 2428 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|      6508 | 2429 | `		}` |
|     47329 | 2430 | `	}` |
|     47324 | 2431 | `cleanup:` |
|         - | 2432 | `	/* Drop the cursor into the token set BEFORE the set is freed (see above). */` |
|     94653 | 2433 | `	pCodeGen->pIn = pSavedIn;` |
|     94653 | 2434 | `	pCodeGen->pEnd = pSavedEnd;` |
|     94653 | 2435 | `	SySetRelease(&aRawToken);` |
|     94653 | 2436 | `	SySetRelease(&aPhpToken);` |
|         - | 2437 | `	/* Restore outer file's strict_types scope */` |
|     94653 | 2438 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|     94653 | 2439 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|     94653 | 2440 | `	return rc;` |
|     47329 | 2441 | `}` |
|         - | 2442 | `/*` |
|         - | 2443 | ` * Utility routines.Initialize the code generator.` |
|         - | 2444 | ` */` |
|      3882 | 2445 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|         - | 2446 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 2447 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 2448 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 2449 | `	)` |
|         5 | 2450 | `{` |
|      3887 | 2451 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 2452 | `	/* Zero the structure */` |
|      3887 | 2453 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|         - | 2454 | `	/* Initial state */` |
|      3887 | 2455 | `	pGen->pVm  = &(*pVm);` |
|      3887 | 2456 | `	pGen->xErr = xErr;` |
|      3887 | 2457 | `	pGen->pErrData = pErrData;` |
|      3887 | 2458 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|      3887 | 2459 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|      3887 | 2460 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|      3887 | 2461 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|      3887 | 2462 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|      3887 | 2463 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3887 | 2464 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3887 | 2465 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|      3887 | 2466 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|         - | 2467 | `	/* Error log buffer */` |
|      3887 | 2468 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|         - | 2469 | `	/* General purpose working buffer */` |
|      3887 | 2470 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|         - | 2471 | `	/* Namespace state */` |
|      3887 | 2472 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|      3887 | 2473 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|      3887 | 2474 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|      3887 | 2475 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 2476 | `	/* Create the global scope */` |
|      3887 | 2477 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|         - | 2478 | `	/* Point to the global scope */` |
|      3887 | 2479 | `	pGen->pCurrent = &pGen->sGlobal;` |
|      3887 | 2480 | `	return SXRET_OK;` |
|         5 | 2481 | `}` |
|         - | 2482 | `/*` |
|         - | 2483 | ` * Utility routines. Reset the code generator to it's initial state.` |
|         - | 2484 | ` */` |
|     98058 | 2485 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|         - | 2486 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 2487 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 2488 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 2489 | `	)` |
|         5 | 2490 | `{` |
|     98063 | 2491 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 2492 | `	GenBlock *pBlock,*pParent;` |
|         - | 2493 | `	/* Reset state */` |
|     98063 | 2494 | `	SySetReset(&pGen->aLabel);` |
|     98063 | 2495 | `	SySetReset(&pGen->aGoto);` |
|     98063 | 2496 | `	SySetReset(&pGen->aNullsafeJmp);` |
|     98063 | 2497 | `	SySetReset(&pGen->aTrivia);` |
|     98063 | 2498 | `	SySetReset(&pGen->aPendingAttrs);` |
|     98063 | 2499 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|     98063 | 2500 | `	SyBlobRelease(&pGen->sErrBuf);` |
|     98063 | 2501 | `	SyBlobRelease(&pGen->sWorker);` |
|     98063 | 2502 | `	SyBlobRelease(&pGen->sNamespace);` |
|     98063 | 2503 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|     98063 | 2504 | `	SyHashRelease(&pGen->hUseImports);` |
|     98063 | 2505 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|     98063 | 2506 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     98063 | 2507 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|     98063 | 2508 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     98063 | 2509 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 2510 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|         - | 2511 | `	 * They intern variable names and literal strings that are referenced by` |
|         - | 2512 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|         - | 2513 | `	 * Releasing them would either leak the interned strings or require freeing` |
|         - | 2514 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|         - | 2515 | `	 * number of unique names, which is acceptable. */` |
|         - | 2516 | `	/* Point to the global scope */` |
|     98063 | 2517 | `	pBlock = pGen->pCurrent;` |
|     98063 | 2518 | `	while( pBlock->pParent != 0 ){` |
|       ! 0 | 2519 | `		pParent = pBlock->pParent;` |
|       ! 0 | 2520 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 2521 | `		pBlock = pParent;` |
|       ! 0 | 2522 | `	}` |
|     98063 | 2523 | `	pGen->xErr = xErr;` |
|     98063 | 2524 | `	pGen->pErrData = pErrData;` |
|     98063 | 2525 | `	pGen->pCurrent = &pGen->sGlobal;` |
|     98063 | 2526 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|     98063 | 2527 | `	pGen->pIn = pGen->pEnd = 0;` |
|     98063 | 2528 | `	pGen->nErr = 0;` |
|     98063 | 2529 | `	return SXRET_OK;` |
|         5 | 2530 | `}` |
|         - | 2531 | `/*` |
|         - | 2532 | ` * Save the code generator's compile-position state and hand the live generator a` |
|         - | 2533 | ` * fresh, empty one for a NESTED compilation unit.` |
|         - | 2534 | ` *` |
|         - | 2535 | ` * A require/include normally runs at execution time, when no compile is in flight,` |
|         - | 2536 | ` * so VmEvalChunk can call PH7_ResetCodeGenerator to wipe the generator. But an` |
|         - | 2537 | `` * autoload can fire in the MIDDLE of compiling a class: resolving `class Child`` |
|         - | 2538 | `` * extends Base` calls PH7_VmExtractClass, which triggers the autoloader, whose`` |
|         - | 2539 | `` * body `require`s Base's file — a nested compile while the outer one is mid-token.`` |
|         - | 2540 | ` * PH7_ResetCodeGenerator would then blow away the outer compile's cursor` |
|         - | 2541 | ` * (pIn/pEnd), current block, use-imports, labels, etc.; the outer parse resumes` |
|         - | 2542 | ` * with pIn == NULL and reports a bogus "Expected '{' after class 'Child'". (Common` |
|         - | 2543 | ` * in every real framework: Composer autoloads parent classes on demand.)` |
|         - | 2544 | ` *` |
|         - | 2545 | ` * This snapshots the position/scope fields into *pSaved (a caller-stack` |
|         - | 2546 | ` * ph7_gen_state used purely as storage) and re-initializes the live generator's` |
|         - | 2547 | ` * position containers to FRESH, empty ones WITHOUT releasing the outer's (the` |
|         - | 2548 | ` * snapshot now owns those). hLiteral/hNumLiteral/hVar are intentionally left LIVE` |
|         - | 2549 | ` * and shared — compiled bytecode interns names/literals into them and they grow` |
|         - | 2550 | ` * monotonically (exactly why PH7_ResetCodeGenerator keeps them); restoring an old` |
|         - | 2551 | ` * copy of those headers after a nested grow would use-after-free the bucket array.` |
|         - | 2552 | ` */` |
|         4 | 2553 | `PH7_PRIVATE void PH7_CompilerSaveState(ph7_vm *pVm,ph7_gen_state *pSaved,ProcConsumer xErr,void *pErrData)` |
|         1 | 2554 | `{` |
|         5 | 2555 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 2556 | `	/* Shallow-copy every field; the position containers below are then replaced` |
|         - | 2557 | `	 * with fresh ones on the live struct, so *pSaved keeps the outer's memory. */` |
|         5 | 2558 | `	*pSaved = *pGen;` |
|         5 | 2559 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|         5 | 2560 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|         5 | 2561 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|         5 | 2562 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|         5 | 2563 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|         5 | 2564 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|         5 | 2565 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|         5 | 2566 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|         5 | 2567 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|         5 | 2568 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|         5 | 2569 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|         5 | 2570 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 2571 | `	/* Fresh global scope for the nested unit (address of the embedded sGlobal is` |
|         - | 2572 | `	 * stable, so any outer block still parented to it stays valid across restore). */` |
|         5 | 2573 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(pVm),0);` |
|         5 | 2574 | `	pGen->pCurrent = &pGen->sGlobal;` |
|         5 | 2575 | `	pGen->pIn = pGen->pEnd = 0;` |
|         5 | 2576 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|         5 | 2577 | `	pGen->pTokenSet = 0;` |
|         5 | 2578 | `	pGen->nErr = 0;` |
|         5 | 2579 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|         5 | 2580 | `	pGen->nCommaExprOk = 0;` |
|         5 | 2581 | `	pGen->bInGenerator = 0;` |
|         5 | 2582 | `	pGen->bStrictTypes = 0;` |
|         5 | 2583 | `	pGen->bStrictTypesLocked = 0;` |
|         5 | 2584 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|         5 | 2585 | `	pGen->xErr = xErr;` |
|         5 | 2586 | `	pGen->pErrData = pErrData;` |
|         5 | 2587 | `}` |
|         - | 2588 | `/*` |
|         - | 2589 | ` * Restore the outer compile-position state saved by PH7_CompilerSaveState,` |
|         - | 2590 | ` * releasing the nested unit's position containers first. The shared` |
|         - | 2591 | ` * hLiteral/hNumLiteral/hVar tables (grown by the nested compile) are carried` |
|         - | 2592 | ` * forward, NOT rolled back to the snapshot's stale headers.` |
|         - | 2593 | ` */` |
|         4 | 2594 | `PH7_PRIVATE void PH7_CompilerRestoreState(ph7_vm *pVm,ph7_gen_state *pSaved)` |
|         1 | 2595 | `{` |
|         5 | 2596 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 2597 | `	GenBlock *pBlock,*pParent;` |
|         - | 2598 | `	SyHash hVar,hLiteral,hNumLiteral;` |
|         - | 2599 | `	/* Free any nested blocks left open (e.g. an aborted nested compile), then the` |
|         - | 2600 | `	 * nested global block's own fixup sets. */` |
|         5 | 2601 | `	pBlock = pGen->pCurrent;` |
|         5 | 2602 | `	while( pBlock && pBlock->pParent != 0 ){` |
|       ! 0 | 2603 | `		pParent = pBlock->pParent;` |
|       ! 0 | 2604 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 2605 | `		pBlock = pParent;` |
|       ! 0 | 2606 | `	}` |
|         5 | 2607 | `	GenStateReleaseBlock(&pGen->sGlobal);` |
|         - | 2608 | `	/* Release the nested unit's position containers. */` |
|         5 | 2609 | `	SySetRelease(&pGen->aLabel);` |
|         5 | 2610 | `	SySetRelease(&pGen->aGoto);` |
|         5 | 2611 | `	SySetRelease(&pGen->aNullsafeJmp);` |
|         5 | 2612 | `	SySetRelease(&pGen->aLoopParent);` |
|         5 | 2613 | `	SySetRelease(&pGen->aTrivia);` |
|         5 | 2614 | `	SySetRelease(&pGen->aPendingAttrs);` |
|         5 | 2615 | `	SyBlobRelease(&pGen->sWorker);` |
|         5 | 2616 | `	SyBlobRelease(&pGen->sErrBuf);` |
|         5 | 2617 | `	SyBlobRelease(&pGen->sNamespace);` |
|         5 | 2618 | `	SyHashRelease(&pGen->hUseImports);` |
|         5 | 2619 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|         5 | 2620 | `	SyHashRelease(&pGen->hUseConstImports);` |
|         - | 2621 | `	/* Preserve the (possibly grown) shared intern tables across the restore. */` |
|         5 | 2622 | `	hVar = pGen->hVar;` |
|         5 | 2623 | `	hLiteral = pGen->hLiteral;` |
|         5 | 2624 | `	hNumLiteral = pGen->hNumLiteral;` |
|         5 | 2625 | `	*pGen = *pSaved;` |
|         5 | 2626 | `	pGen->hVar = hVar;` |
|         5 | 2627 | `	pGen->hLiteral = hLiteral;` |
|         5 | 2628 | `	pGen->hNumLiteral = hNumLiteral;` |
|         5 | 2629 | `}` |
|         - | 2630 | `/*` |
|         - | 2631 | ` * Raise php's parse error for an unexpected token: E_PARSE with the exact text` |
|         - | 2632 | ` * php's parser prints, e.g.` |
|         - | 2633 | ` *` |
|         - | 2634 | ` *   syntax error, unexpected token ";", expecting "{"` |
|         - | 2635 | ` *   syntax error, unexpected identifier "invalid", expecting "("` |
|         - | 2636 | ` *   syntax error, unexpected end of file` |
|         - | 2637 | ` *` |
|         - | 2638 | ` * php names the token by CLASS, not just by text: an identifier, a variable and` |
|         - | 2639 | ` * a number each get their own noun, while everything else (keywords, operators,` |
|         - | 2640 | ` * punctuation) is a "token". zExpecting is the optional ", expecting ..." tail` |
|         - | 2641 | ` * — pass NULL when the site cannot say what it wanted (php often can't either).` |
|         - | 2642 | ` * pTok == NULL means the input ran out: "unexpected end of file".` |
|         - | 2643 | ` *` |
|         - | 2644 | ` * PHL's hand-written recursive-descent parser has no bison expectation sets, so` |
|         - | 2645 | ` * a site can only claim an "expecting" clause it genuinely knows; every clause` |
|         - | 2646 | ` * emitted here was verified against php 8.5.7 for the construct in question.` |
|         - | 2647 | ` */` |
|       178 | 2648 | `PH7_PRIVATE sxi32 PH7_GenSyntaxError(` |
|         - | 2649 | `	ph7_gen_state *pGen,   /* Code generator state */` |
|         - | 2650 | `	SyToken *pTok,         /* Offending token, or NULL for end of file */` |
|         - | 2651 | `	const char *zExpecting /* ", expecting <this>" tail, or NULL */` |
|         - | 2652 | `	)` |
|         5 | 2653 | `{` |
|       183 | 2654 | `	const char *zNoun = "token";` |
|         - | 2655 | `	sxu32 nLine;` |
|       183 | 2656 | `	if( pTok == 0 && pGen->pTokenSet ){` |
|         - | 2657 | `		/* The caller ran out of tokens inside its own slice — but a statement's slice stops` |
|         - | 2658 | `		 * BEFORE its terminator, so the token php actually names (typically the ';') is the` |
|         - | 2659 | `		 * one sitting just past the slice, still inside the chunk's token stream. Reach for` |
|         - | 2660 | `		 * it before concluding "end of file". */` |
|        90 | 2661 | `		SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        90 | 2662 | `		SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|        90 | 2663 | `		if( pGen->pEnd >= pBase && pGen->pEnd < pStreamEnd ){` |
|        90 | 2664 | `			pTok = pGen->pEnd;` |
|        43 | 2665 | `		}` |
|        43 | 2666 | `	}` |
|       183 | 2667 | `	nLine = pTok ? pTok->nLine : (pGen->pIn > (SyToken *)SySetBasePtr(pGen->pTokenSet) ? pGen->pIn[-1].nLine : 1);` |
|       183 | 2668 | `	if( pTok == 0 ){` |
|       ! 0 | 2669 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|       ! 0 | 2670 | `			zExpecting ? "syntax error, unexpected end of file, expecting %s"` |
|         - | 2671 | `			           : "syntax error, unexpected end of file",` |
|       ! 0 | 2672 | `			zExpecting);` |
|         - | 2673 | `	}` |
|       183 | 2674 | `	if( pTok->nType & PH7_TK_ID ){` |
|        17 | 2675 | `		zNoun = "identifier";` |
|       176 | 2676 | `	}else if( pTok->nType & PH7_TK_DOLLAR ){` |
|         6 | 2677 | `		zNoun = "variable";` |
|       168 | 2678 | `	}else if( pTok->nType & PH7_TK_INTEGER ){` |
|        24 | 2679 | `		zNoun = "integer";` |
|       156 | 2680 | `	}else if( pTok->nType & PH7_TK_REAL ){` |
|       ! 0 | 2681 | `		zNoun = "float";` |
|       ! 0 | 2682 | `	}` |
|       183 | 2683 | `	if( zExpecting ){` |
|       118 | 2684 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        38 | 2685 | `			"syntax error, unexpected %s \"%z\", expecting %s",zNoun,&pTok->sData,zExpecting);` |
|         - | 2686 | `	}` |
|       158 | 2687 | `	return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        51 | 2688 | `		"syntax error, unexpected %s \"%z\"",zNoun,&pTok->sData);` |
|        94 | 2689 | `}` |
|         - | 2690 | `/*` |
|         - | 2691 | ` * Generate a compile-time error message.` |
|         - | 2692 | ` * If the error count limit is reached (usually 15 error message)` |
|         - | 2693 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|         - | 2694 | ` * abort compilation immediately.` |
|         - | 2695 | ` */` |
|       674 | 2696 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|         5 | 2697 | `{` |
|       679 | 2698 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|       679 | 2699 | `	const char *zErr = "Error";` |
|         - | 2700 | `	SyString *pFile;` |
|         - | 2701 | `	va_list ap;` |
|         - | 2702 | `	sxi32 rc;` |
|         - | 2703 | `	/* Reset the working buffer */` |
|       679 | 2704 | `	SyBlobReset(pWorker);` |
|         - | 2705 | `	/* Peek the processed file path if available */` |
|       679 | 2706 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|       679 | 2707 | `	if( nErrType == E_ERROR \|\| nErrType == E_PARSE ){` |
|         - | 2708 | `		/* Increment the error counter. A PARSE error is every bit as fatal as an` |
|         - | 2709 | `		 * E_ERROR one: php compiles nothing, runs nothing and exits 255. Counting` |
|         - | 2710 | `		 * only E_ERROR let a parse error print its diagnostic and then fall through` |
|         - | 2711 | `		 * into execution with a 0 exit status. */` |
|       669 | 2712 | `		pGen->nErr++;` |
|       669 | 2713 | `		if( pGen->nErr > 15 ){` |
|         - | 2714 | `			/* Error count limit reached */` |
|         6 | 2715 | `			if( pGen->xErr ){` |
|         6 | 2716 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|         6 | 2717 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|         6 | 2718 | `				if( pFile ){` |
|         6 | 2719 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|         2 | 2720 | `				}` |
|         6 | 2721 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|         6 | 2722 | `				if( SyBlobLength(pWorker) > 0 ){` |
|         6 | 2723 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|         2 | 2724 | `				}` |
|         2 | 2725 | `			}` |
|         - | 2726 | `			/* Abort immediately */` |
|         6 | 2727 | `			return SXERR_ABORT;` |
|         - | 2728 | `		}` |
|       330 | 2729 | `	}` |
|       675 | 2730 | `	if( pGen->xErr == 0 ){` |
|         - | 2731 | `		/* No available error consumer,return immediately */` |
|         3 | 2732 | `		return SXRET_OK;` |
|         - | 2733 | `	}` |
|       673 | 2734 | `	switch(nErrType){` |
|       322 | 2735 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|         8 | 2736 | `	case E_WARNING: zErr = "Warning";     break;` |
|       344 | 2737 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|         6 | 2738 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|       ! 0 | 2739 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|       ! 0 | 2740 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|       ! 0 | 2741 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|       ! 0 | 2742 | `	case 8192 /* E_DEPRECATED */: zErr = "Deprecated"; break;` |
|       ! 0 | 2743 | `	default:` |
|       ! 0 | 2744 | `		break;` |
|         - | 2745 | `	}` |
|       673 | 2746 | `	rc = SXRET_OK;` |
|         - | 2747 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|       673 | 2748 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|       673 | 2749 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|       673 | 2750 | `	va_start(ap,zFormat);` |
|       673 | 2751 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|       673 | 2752 | `	va_end(ap);` |
|       673 | 2753 | `	if( pFile ){` |
|       673 | 2754 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       334 | 2755 | `	}` |
|         - | 2756 | `	/* Append a new line */` |
|       673 | 2757 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       673 | 2758 | `	if( SyBlobLength(pWorker) > 0 ){` |
|         - | 2759 | `		/* Consume the generated error message */` |
|       673 | 2760 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       334 | 2761 | `	}` |
|       673 | 2762 | `	return rc;` |
|       342 | 2763 | `}` |
|         - | 2764 |  |
