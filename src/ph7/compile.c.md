# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1524/1655 lines (92.08%)

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
|    177864 |   46 | `PH7_PRIVATE GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|         5 |   47 | `{` |
|    177869 |   48 | `	GenBlock *pBlock = pCurrent;` |
|    359413 |   49 | `	for(;;){` |
|    718831 |   50 | `		if( pBlock->iFlags & iBlockType ){` |
|    177845 |   51 | `			iCount--; /* Decrement nesting level */` |
|    177845 |   52 | `			if( iCount < 1 ){` |
|         - |   53 | `				/* Block meet with the desired criteria */` |
|    177819 |   54 | `				return pBlock;` |
|         - |   55 | `			}` |
|        13 |   56 | `		}` |
|         - |   57 | `		/* Point to the upper block */` |
|    541017 |   58 | `		pBlock = pBlock->pParent;` |
|    541017 |   59 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|         - |   60 | `			/* Forbidden */` |
|        28 |   61 | `			break;` |
|         - |   62 | `		}` |
|         5 |   63 | `	}` |
|         - |   64 | `	/* No such block */` |
|        53 |   65 | `	return 0;` |
|     88937 |   66 | `}` |
|         - |   67 | `/*` |
|         - |   68 | ` * Initialize a freshly allocated block instance.` |
|         - |   69 | ` */` |
|  13036378 |   70 | `static void GenStateInitBlock(` |
|         - |   71 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |   72 | `	GenBlock *pBlock,    /* Target block */` |
|         - |   73 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   74 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|         - |   75 | `	void *pUserData      /* Upper layer private data */` |
|         - |   76 | `	)` |
|         5 |   77 | `{` |
|         - |   78 | `	/* Initialize block fields */` |
|  13036383 |   79 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  13036383 |   80 | `	pBlock->pUserData   = pUserData;` |
|  13036383 |   81 | `	pBlock->pGen        = pGen;` |
|  13036383 |   82 | `	pBlock->iFlags      = iType;` |
|  13036383 |   83 | `	pBlock->pParent     = 0;` |
|  13036383 |   84 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  13036383 |   85 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  13036383 |   86 | `}` |
|         - |   87 | `/*` |
|         - |   88 | ` * Allocate a new block instance.` |
|         - |   89 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|         - |   90 | ` * on success.Otherwise generate a compile-time error and abort` |
|         - |   91 | ` * processing on failure.` |
|         - |   92 | ` */` |
|  13032512 |   93 | `PH7_PRIVATE sxi32 GenStateEnterBlock(` |
|         - |   94 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |   95 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   96 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|         - |   97 | `	void *pUserData,      /* Upper layer private data */` |
|         - |   98 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|         - |   99 | `	)` |
|         5 |  100 | `{` |
|         - |  101 | `	GenBlock *pBlock;` |
|         - |  102 | `	/* Allocate a new block instance */` |
|  13032517 |  103 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  13032517 |  104 | `	if( pBlock == 0 ){` |
|         - |  105 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |  106 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - |  107 | `		 */` |
|       ! 0 |  108 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|         - |  109 | `		/* Abort processing immediately */` |
|       ! 0 |  110 | `		return SXERR_ABORT;` |
|         - |  111 | `	}` |
|         - |  112 | `	/* Zero the structure */` |
|  13032517 |  113 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  13032517 |  114 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|         - |  115 | `	/* Link to the parent block */` |
|  13032517 |  116 | `	pBlock->pParent = pGen->pCurrent;` |
|         - |  117 | `	/* A loop or switch gets an id, and remembers the loop it nests inside, so a goto's` |
|         - |  118 | `	 * and a label's positions can be compared after compilation (see aLoopParent). */` |
|  13032517 |  119 | `	if( iType & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    538007 |  120 | `		sxu32 nParent = pGen->nCurLoopId;` |
|    538007 |  121 | `		pGen->nLoopId++;` |
|    538007 |  122 | `		SySetPut(&pGen->aLoopParent,(const void *)&nParent);` |
|    538007 |  123 | `		pBlock->nLoopId = pGen->nLoopId;` |
|    538007 |  124 | `		pBlock->nOuterLoopId = nParent;` |
|    538007 |  125 | `		pGen->nCurLoopId = pGen->nLoopId;` |
|    269001 |  126 | `	}` |
|         - |  127 | `	/* Mark as the current block */` |
|  13032517 |  128 | `	pGen->pCurrent = pBlock;` |
|  13032517 |  129 | `	if( ppBlock ){` |
|         - |  130 | `		/* Write a pointer to the new instance */` |
|   6267663 |  131 | `		*ppBlock = pBlock;` |
|   3133829 |  132 | `	}` |
|  13032517 |  133 | `	return SXRET_OK;` |
|   6516261 |  134 | `}` |
|         - |  135 | `/*` |
|         - |  136 | ` * Release block fields without freeing the whole instance.` |
|         - |  137 | ` */` |
|  13032506 |  138 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|         5 |  139 | `{` |
|  13032511 |  140 | `	SySetRelease(&pBlock->aPostContFix);` |
|  13032511 |  141 | `	SySetRelease(&pBlock->aJumpFix);` |
|  13032511 |  142 | `}` |
|         - |  143 | `/*` |
|         - |  144 | ` * Release a block.` |
|         - |  145 | ` */` |
|  13032502 |  146 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|         5 |  147 | `{` |
|  13032507 |  148 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  13032507 |  149 | `	GenStateReleaseBlock(&(*pBlock));` |
|         - |  150 | `	/* Free the instance */` |
|  13032507 |  151 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  13032507 |  152 | `}` |
|         - |  153 | `/*` |
|         - |  154 | ` * POP and release a block from the stack of compiled blocks.` |
|         - |  155 | ` */` |
|  13032502 |  156 | `PH7_PRIVATE sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|         5 |  157 | `{` |
|  13032507 |  158 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  13032507 |  159 | `	if( pBlock == 0 ){` |
|         - |  160 | `		/* No more block to pop */` |
|       ! 0 |  161 | `		return SXERR_EMPTY;` |
|         - |  162 | `	}` |
|  13032507 |  163 | `	if( pBlock->iFlags & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    537999 |  164 | `		pGen->nCurLoopId = pBlock->nOuterLoopId;` |
|    268997 |  165 | `	}` |
|         - |  166 | `	/* Point to the upper block */` |
|  13032507 |  167 | `	pGen->pCurrent = pBlock->pParent;` |
|  13032507 |  168 | `	if( ppBlock ){` |
|         - |  169 | `		/* Write a pointer to the popped block */` |
|       ! 0 |  170 | `		*ppBlock = pBlock;` |
|       ! 0 |  171 | `	}else{` |
|         - |  172 | `		/* Safely release the block */` |
|  13032507 |  173 | `		GenStateFreeBlock(&(*pBlock));` |
|         - |  174 | `	}` |
|  13032507 |  175 | `	return SXRET_OK;` |
|   6516256 |  176 | `}` |
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
|   1046332 |  195 | `PH7_PRIVATE int GenStateUnconditionalTopLevel(ph7_gen_state *pGen)` |
|         5 |  196 | `{` |
|   1046337 |  197 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   1050303 |  198 | `	while( pBlock ){` |
|   1050303 |  199 | `		if( pBlock->iFlags & (GEN_BLOCK_COND\|GEN_BLOCK_LOOP\|GEN_BLOCK_FUNC\|GEN_BLOCK_SWITCH\|GEN_BLOCK_EXCEPTION) ){` |
|        77 |  200 | `			return 0; /* conditional / nested */` |
|         - |  201 | `		}` |
|   1050231 |  202 | `		if( pBlock->iFlags & GEN_BLOCK_GLOBAL ){` |
|   1046265 |  203 | `			return 1; /* reached the global block with no conditional ancestor */` |
|         - |  204 | `		}` |
|      3971 |  205 | `		pBlock = pBlock->pParent;` |
|         5 |  206 | `	}` |
|       ! 0 |  207 | `	return 1;` |
|    523171 |  208 | `}` |
|         - |  209 | `/*` |
|         - |  210 | ` * Guard a top-level function about to be installed. Same contract as the class` |
|         - |  211 | ` * guard above.` |
|         - |  212 | ` */` |
|    518928 |  213 | `PH7_PRIVATE sxi32 GenStateGuardFuncRedeclaration(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|         5 |  214 | `{` |
|         - |  215 | `	SyHashEntry *pEntry;` |
|    518933 |  216 | `	if( !GenStateUnconditionalTopLevel(pGen) ){` |
|        52 |  217 | `		return SXRET_OK;` |
|         - |  218 | `	}` |
|    518883 |  219 | `	pFunc->iFlags \|= VM_FUNC_BOUND;` |
|    518883 |  220 | `	if( pGen->pVm->bCompilingBuiltin ){` |
|    517513 |  221 | `		return SXRET_OK;` |
|         - |  222 | `	}` |
|         - |  223 | ``	/* NOTE: a userland function shadowing a C builtin (e.g. `function strlen(){}`)`` |
|         - |  224 | `	 * is NOT caught here — the C builtins register in PH7_VmMakeReady, after user` |
|         - |  225 | `	 * code has compiled, so hHostFunction is still empty at this point. Prelude` |
|         - |  226 | `	 * functions (ini_get, ...) and every builtin CLASS compile earlier and ARE` |
|         - |  227 | `	 * guarded. Redeclaring a C builtin function stays a known divergence. */` |
|      1375 |  228 | `	pEntry = SyHashGet(&pGen->pVm->hFunction,(const void *)pFunc->sName.zString,pFunc->sName.nByte);` |
|      1375 |  229 | `	if( pEntry ){` |
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
|      1371 |  246 | `	return SXRET_OK;` |
|    259469 |  247 | `}` |
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
|   4706528 |  258 | `PH7_PRIVATE sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|         5 |  259 | `{` |
|         - |  260 | `	JumpFixup sJumpFix;` |
|         - |  261 | `	sxi32 rc;` |
|         - |  262 | `	/* Init the JumpFixup structure */` |
|   4706533 |  263 | `	sJumpFix.nJumpType = nJumpType;` |
|   4706533 |  264 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|         - |  265 | `	/* Insert in the jump fixup table */` |
|   4706533 |  266 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|   4706533 |  267 | `	return rc;` |
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
|   9059726 |  280 | `PH7_PRIVATE sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|         5 |  281 | `{` |
|         - |  282 | `	JumpFixup *aFix;` |
|         - |  283 | `	VmInstr *pInstr;` |
|         - |  284 | `	sxu32 nFixed;` |
|         - |  285 | `	sxu32 n;` |
|         - |  286 | `	/* Point to the jump fixup table */` |
|   9059731 |  287 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|         - |  288 | `	/* Fix the desired jumps */` |
|  18963587 |  289 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|   9903861 |  290 | `		if( aFix[n].nJumpType < 0 ){` |
|         - |  291 | `			/* Already fixed */` |
|   3697999 |  292 | `			continue;` |
|         - |  293 | `		}` |
|   6205867 |  294 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|         - |  295 | `			/* Not of our interest */` |
|   1499341 |  296 | `			continue;` |
|         - |  297 | `		}` |
|         - |  298 | `		/* Point to the instruction to fix */` |
|   4706531 |  299 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|   4706531 |  300 | `		if( pInstr ){` |
|   4706531 |  301 | `			pInstr->iP2 = nJumpDest;` |
|   4706531 |  302 | `			nFixed++;` |
|         - |  303 | `			/* Mark as fixed */` |
|   4706531 |  304 | `			aFix[n].nJumpType = -1;` |
|   2353263 |  305 | `		}` |
|   2353268 |  306 | `	}` |
|         - |  307 | `	/* Total number of fixed jumps */` |
|   9059731 |  308 | `	return nFixed;` |
|         5 |  309 | `}` |
|         - |  310 | `/*` |
|         - |  311 | ` * Fix a 'goto' now the jump destination is resolved.` |
|         - |  312 | ` * The goto statement can be used to jump to another section` |
|         - |  313 | ` * in the program.` |
|         - |  314 | ` * Refer to the routine responsible of compiling the goto` |
|         - |  315 | ` * statement for more information.` |
|         - |  316 | ` */` |
|   3354292 |  317 | `PH7_PRIVATE sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|         5 |  318 | `{` |
|         - |  319 | `	JumpFixup *pJump,*aJumps;` |
|         - |  320 | `	Label *pLabel;` |
|         - |  321 | `	VmInstr *pInstr;` |
|         - |  322 | `	sxi32 rc;` |
|         - |  323 | `	sxu32 n;` |
|         - |  324 | `	/* Point to the goto table */` |
|   3354297 |  325 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|         - |  326 | `	/* Fix */` |
|   3354443 |  327 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|        12 |  365 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"'goto' to undefined label '%z'",&pJump->sLabel);` |
|        12 |  366 | `			if( rc == SXERR_ABORT ){` |
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
|   3354295 |  378 | `	return SXRET_OK;` |
|   1677151 |  379 | `}` |
|         - |  380 | `/*` |
|         - |  381 | ` * Check if a given token value is installed in the literal table.` |
|         - |  382 | ` */` |
|  16803384 |  383 | `PH7_PRIVATE sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|         5 |  384 | `{` |
|         - |  385 | `	SyHashEntry *pEntry;` |
|  16803389 |  386 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  16803389 |  387 | `	if( pEntry == 0 ){` |
|   4479677 |  388 | `		return SXERR_NOTFOUND;` |
|         - |  389 | `	}` |
|  12323717 |  390 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  12323717 |  391 | `	return SXRET_OK;` |
|   8401697 |  392 | `}` |
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
|   4479672 |  403 | `PH7_PRIVATE sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|         5 |  404 | `{` |
|   4479677 |  405 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|   4479677 |  406 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   2239836 |  407 | `	}` |
|   4479677 |  408 | `	return SXRET_OK;` |
|         5 |  409 | `}` |
|         - |  410 | `/*` |
|         - |  411 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|         - |  412 | ` * in the constant table.` |
|         - |  413 | ` */` |
|   3799210 |  414 | `PH7_PRIVATE ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|         5 |  415 | `{` |
|         - |  416 | `	ph7_value *pObj;` |
|   3799215 |  417 | `	sxu32 nIdx = 0; /* cc warning */` |
|         - |  418 | `	/* Reserve a new constant */` |
|   3799215 |  419 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   3799215 |  420 | `	if( pObj == 0 ){` |
|       ! 0 |  421 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  422 | `		return 0;` |
|         - |  423 | `	}` |
|   3799215 |  424 | `	*pIdx = nIdx;` |
|         - |  425 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|         - |  426 | `	 * the constant string iterals table [optimization purposes].` |
|         - |  427 | `	 */` |
|   3799215 |  428 | `	return pObj;` |
|   1899610 |  429 | `}` |
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
|   7905570 |  444 | `PH7_PRIVATE void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|         5 |  445 | `{` |
|         - |  446 | `	VmCallArgMap *pMap;` |
|   7905575 |  447 | `	if( !pGen->bStrictTypes ) return p3;` |
|        58 |  448 | `	if( p3 == 0 ){` |
|        54 |  449 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|        54 |  450 | `		if( pMap == 0 ) return 0;` |
|        54 |  451 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|        54 |  452 | `		p3 = (void *)pMap;` |
|        25 |  453 | `	}` |
|        58 |  454 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|        58 |  455 | `	return p3;` |
|   3952790 |  456 | `}` |
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
|    340128 |  476 | `PH7_PRIVATE int GenStateIsReservedConstant(SyString *pName)` |
|         5 |  477 | `{` |
|    340133 |  478 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      3909 |  479 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|         3 |  480 | `			return TRUE;` |
|      3907 |  481 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|         5 |  482 | `			return TRUE;` |
|         5 |  483 | `		}` |
|    338178 |  484 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|      7753 |  485 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|         3 |  486 | `			return TRUE;` |
|         - |  487 | `		}` |
|      3873 |  488 | `	}` |
|         - |  489 | `	/* Not a reserved constant */` |
|    340125 |  490 | `	return FALSE;` |
|    170069 |  491 | `}` |
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
|  53705102 |  508 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|         5 |  509 | `{` |
|  53705107 |  510 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|         - |  511 | `	sxu32 nTarget;` |
|         - |  512 | `	sxu32 *aIdx;` |
|         - |  513 | `	sxu32 i;` |
|  53705107 |  514 | `	if( nCur <= nBaseline ){` |
|  53705011 |  515 | `		return;` |
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
|  26852556 |  526 | `}` |
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
|   7021348 |  545 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
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
|   7021353 |  564 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|   1797509 |  565 | `		return 0;` |
|         - |  566 | `	}` |
|  56982153 |  567 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
|  51816404 |  568 | `		if( pName->nByte == aByRef[i].nByte` |
|  27416280 |  569 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|     58105 |  570 | `			return aByRef[i].mask;` |
|         - |  571 | `		}` |
|  25879157 |  572 | `	}` |
|   5165749 |  573 | `	return 0;` |
|   3510679 |  574 | `}` |
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
|   7021348 |  585 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|         5 |  586 | `{` |
|         - |  587 | `	SyToken *p, *pEnd;` |
|   7021353 |  588 | `	pOut->zString = 0;` |
|   7021353 |  589 | `	pOut->nByte = 0;` |
|   7021353 |  590 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|       ! 0 |  591 | `		return;` |
|         - |  592 | `	}` |
|   7021353 |  593 | `	p = pLeft->pStart;` |
|   7021353 |  594 | `	pEnd = pLeft->pEnd;` |
|         - |  595 | `	/* Optional single leading namespace separator (absolute path). */` |
|   7021353 |  596 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|      3905 |  597 | `		p++;` |
|      1950 |  598 | `	}` |
|   7021353 |  599 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|   1797459 |  600 | `		return;` |
|         - |  601 | `	}` |
|         - |  602 | `	/* Must be a single component: nothing follows the name token. */` |
|   5223899 |  603 | `	if( p + 1 != pEnd ){` |
|        55 |  604 | `		return;` |
|         - |  605 | `	}` |
|   5223849 |  606 | `	*pOut = p->sData;` |
|   3510679 |  607 | `}` |
|         - |  608 | `/*` |
|         - |  609 | ` * Generate bytecode for a given expression tree.` |
|         - |  610 | ` * If something goes wrong while generating bytecode` |
|         - |  611 | ` * for the expression tree (A very unlikely scenario)` |
|         - |  612 | ` * this function takes care of generating the appropriate` |
|         - |  613 | ` * error message.` |
|         - |  614 | ` */` |
|  75723622 |  615 | `static sxi32 GenStateEmitExprCode(` |
|         - |  616 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |  617 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|         - |  618 | `	sxi32 iFlags /* Control flags */` |
|         - |  619 | `	)` |
|         5 |  620 | `{` |
|         - |  621 | `	VmInstr *pInstr;` |
|         - |  622 | `	sxu32 nJmpIdx;` |
|  75723627 |  623 | `	sxi32 iP1 = 0;` |
|  75723627 |  624 | `	sxu32 iP2 = 0;` |
|  75723627 |  625 | `	void *p3  = 0;` |
|         - |  626 | `	sxi32 iVmOp;` |
|         - |  627 | `	sxi32 rc;` |
|  75723627 |  628 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
|  75723627 |  629 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
|  75723627 |  630 | `	sxu32 nRhsNsBase = 0;` |
|  75723627 |  631 | `	if( pNode->xCode ){` |
|         - |  632 | `		SyToken *pTmpIn,*pTmpEnd;` |
|         - |  633 | `		/* Compile node */` |
|  45703659 |  634 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
|  45703659 |  635 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
|  45703659 |  636 | `		RE_SWAP_DELIMITER(pGen);` |
|  45703659 |  637 | `		return rc;` |
|         - |  638 | `	}` |
|  30019973 |  639 | `	if( pNode->pOp == 0 ){` |
|       ! 0 |  640 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - |  641 | `			"Invalid expression node,PH7 is aborting compilation");` |
|       ! 0 |  642 | `		return SXERR_ABORT;` |
|         - |  643 | `	}` |
|  30019973 |  644 | `	iVmOp = pNode->pOp->iVmOp;` |
|  30019973 |  645 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|         - |  646 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|         - |  647 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|         - |  648 | `		 * and later errors are still reported. */` |
|         3 |  649 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - |  650 | `			"The (unset) cast is no longer supported");` |
|         3 |  651 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  652 | `			return SXERR_ABORT;` |
|         - |  653 | `		}` |
|         1 |  654 | `	}` |
|  30019973 |  655 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
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
|  30019883 |  705 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|         - |  706 | `		sxu32 nJz,nJmp;` |
|         - |  707 | `		sxu32 nTernaryNsBase;` |
|         - |  708 | `		/* Ternary operator require special handling */` |
|         - |  709 | `		/* Phase#1: Compile the condition */` |
|    501479 |  710 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    501479 |  711 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    501479 |  712 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  713 | `			return rc;` |
|         - |  714 | `		}` |
|         - |  715 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|         - |  716 | `		 * compiling the condition must short-circuit to the end of the` |
|         - |  717 | `		 * condition expression, not leak past the ternary. */` |
|    501479 |  718 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    501479 |  719 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    501479 |  720 | `		if( pNode->pLeft ){` |
|         - |  721 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|         - |  722 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    497547 |  723 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - |  724 | `			/* Phase#3: Compile the 'then' expression  */` |
|    497547 |  725 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    497547 |  726 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    497547 |  727 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  728 | `				return rc;` |
|         - |  729 | `			}` |
|    497547 |  730 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    248776 |  731 | `		}else{` |
|         - |  732 | `			/* Elvis operator: (expr) ?: (else)` |
|         - |  733 | `			 * Duplicate condition so original value is the 'then' result.` |
|         - |  734 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      3937 |  735 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      3937 |  736 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - |  737 | `		}` |
|         - |  738 | `		/* Phase#4: Emit the unconditional jump */` |
|    501479 |  739 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|         - |  740 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    501479 |  741 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    501479 |  742 | `		if( pInstr ){` |
|    501479 |  743 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    250737 |  744 | `		}` |
|    501479 |  745 | `		if( !pNode->pLeft ){` |
|         - |  746 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      3937 |  747 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      1966 |  748 | `		}` |
|         - |  749 | `		/* Phase#6: Compile the 'else' expression */` |
|    501479 |  750 | `		if( pNode->pRight ){` |
|    501479 |  751 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    501479 |  752 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    501479 |  753 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  754 | `				return rc;` |
|         - |  755 | `			}` |
|    501479 |  756 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    250737 |  757 | `		}` |
|    501479 |  758 | `		if( nJmp > 0 ){` |
|         - |  759 | `			/* Phase#7: Fix the unconditional jump */` |
|    501479 |  760 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    501479 |  761 | `			if( pInstr ){` |
|    501479 |  762 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    250737 |  763 | `			}` |
|    250737 |  764 | `		}` |
|         - |  765 | `		/* All done */` |
|    501479 |  766 | `		return SXRET_OK;` |
|         - |  767 | `	}` |
|  29518409 |  768 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
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
|  29518383 |  801 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|         - |  802 | `	/* Generate code for the left tree */` |
|  29518383 |  803 | `	if( pNode->pLeft ){` |
|  29464503 |  804 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  29464503 |  805 | `		if( iVmOp == PH7_OP_CALL ){` |
|         - |  806 | `			ph7_expr_node **apNode;` |
|   7025561 |  807 | `			int hasSpread = 0;` |
|   7025561 |  808 | `			int hasNamed = 0;` |
|   7025561 |  809 | `			int bAnySpread = 0;` |
|   7025561 |  810 | `			sxu32 byRefMask = 0;` |
|         - |  811 | `			sxi32 nArgs;` |
|         - |  812 | `			sxi32 n;` |
|         - |  813 | `			/* Recurse and generate bytecodes for function arguments */` |
|   7025561 |  814 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   7025561 |  815 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|         - |  816 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|         - |  817 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|         - |  818 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|   7025561 |  819 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|        84 |  820 | `				bFcc = 1;` |
|        84 |  821 | `				nArgs = 0;` |
|        41 |  822 | `			}` |
|         - |  823 | `			/* Validate argument order like php: no positional argument after a` |
|         - |  824 | ``			 * named one OR after unpacking, and `name: ...$x` is a parse error. */`` |
|         - |  825 | `			{` |
|   7025561 |  826 | `				int seenNamed = 0;` |
|   7025561 |  827 | `				int seenSpread = 0;` |
|  15003531 |  828 | `				for( n = 0; n < nArgs; ++n ){` |
|   7977977 |  829 | `					if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|      4065 |  830 | `						bAnySpread = 1;` |
|      4065 |  831 | `						seenSpread = 1;` |
|      4065 |  832 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       ! 0 |  833 | `							rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - |  834 | `								"syntax error, unexpected token \"...\"");` |
|       ! 0 |  835 | `							return SXERR_SYNTAX;` |
|         5 |  836 | `						}` |
|   7975947 |  837 | `					}else if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       300 |  838 | `						seenNamed = 1;` |
|       300 |  839 | `						hasNamed = 1;` |
|   7973769 |  840 | `					}else if( seenNamed ){` |
|         3 |  841 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - |  842 | `							"Cannot use positional argument after named argument");` |
|         3 |  843 | `						return SXERR_SYNTAX;` |
|   7973619 |  844 | `					}else if( seenSpread ){` |
|       ! 0 |  845 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - |  846 | `							"Cannot use positional argument after argument unpacking");` |
|       ! 0 |  847 | `						return SXERR_SYNTAX;` |
|         - |  848 | `					}` |
|   3988990 |  849 | `				}` |
|         - |  850 | `			}` |
|         - |  851 | `			/* Read-only load */` |
|   7025559 |  852 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|         - |  853 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|         - |  854 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|         - |  855 | `			 * objects dispatch to the right method (offsetExists for both;` |
|         - |  856 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|   7025559 |  857 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|   7025559 |  858 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|  10929870 |  859 | `				int bIsset = pCallName->nByte == 5` |
|   7025554 |  860 | `					&& SyStrnicmp(pCallName->zString,"isset",5) == 0;` |
|  10929870 |  861 | `				int bEmpty = pCallName->nByte == 5` |
|   7025554 |  862 | `					&& SyStrnicmp(pCallName->zString,"empty",5) == 0;` |
|         - |  863 | `				/* isset()/empty() are language CONSTRUCTS, not functions: php parses` |
|         - |  864 | `				 * their argument list in the grammar and a missing operand is a parse` |
|         - |  865 | `				 * error on the ')'. They compile through this ordinary call loop, which` |
|         - |  866 | ``				 * never checked arity, so `empty()` quietly evaluated to true and`` |
|         - |  867 | ``				 * `isset()` to false. (empty() also takes exactly one operand in php,`` |
|         - |  868 | `				 * unlike isset(), which is variadic.) */` |
|   7025559 |  869 | `				if( (bIsset \|\| bEmpty) && nArgs < 1 ){` |
|         - |  870 | `					/* php names the ')' itself as the unexpected token, so point at the` |
|         - |  871 | `					 * node's last token rather than pGen->pIn (which has already moved` |
|         - |  872 | `					 * past the call to the statement's ';'). */` |
|         5 |  873 | `					SyToken *pTok = pNode->pEnd;` |
|         5 |  874 | `					if( pTok && pTok > pNode->pStart && (pTok->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 |  875 | `						pTok--;` |
|       ! 0 |  876 | `					}` |
|         5 |  877 | `					PH7_GenSyntaxError(&(*pGen),pTok,0);` |
|         5 |  878 | `					return SXERR_ABORT;` |
|         - |  879 | `				}` |
|   7025555 |  880 | `				if( bIsset ){` |
|    324841 |  881 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|   6863137 |  882 | `				}else if( bEmpty ){` |
|       129 |  883 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|        62 |  884 | `				}` |
|         - |  885 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|         - |  886 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|         - |  887 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|         - |  888 | `				 * write back through. Skipped when spread/named args are present:` |
|         - |  889 | `				 * the compile-time positional index no longer maps to the` |
|         - |  890 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|   7025555 |  891 | `				if( !bAnySpread && !hasNamed ){` |
|         - |  892 | `					SyString sBuiltin;` |
|   7021353 |  893 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|   7021353 |  894 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|   3510674 |  895 | `				}` |
|   3512775 |  896 | `			}` |
|  15003521 |  897 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|   7977973 |  898 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   7977973 |  899 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|         - |  900 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|         - |  901 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|         - |  902 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|         - |  903 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|         - |  904 | `				 * builtin to write back through. A plain $var target is unaffected` |
|         - |  905 | `				 * (iP1=0 either way). */` |
|   7977973 |  906 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|     38711 |  907 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|     38711 |  908 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|     19353 |  909 | `				}` |
|         - |  910 | ``				/* Slice 21: a plain `$var` argument may bind to a USER-function by-ref`` |
|         - |  911 | `				 * parameter whose signature is unknown at compile time (forward` |
|         - |  912 | `				 * reference, dynamic call, or method dispatch — e.g. PHPUnit's` |
|         - |  913 | ``				 * `willReturnReference($undef)`). Reserve a real memobj slot for it so an`` |
|         - |  914 | `				 * UNDEFINED variable vivifies and the by-ref write-back reaches the caller` |
|         - |  915 | `				 * (php). A by-value parameter still receives a copy; the only divergence` |
|         - |  916 | `				 * is that an undefined variable passed BY VALUE is created as NULL in the` |
|         - |  917 | `				 * caller (recorded in NEWPLAN §2). Excludes isset()/empty()/unset(), which` |
|         - |  918 | `				 * compile through this same call loop but must NEVER create their operand,` |
|         - |  919 | `				 * and named/spread args (positional-index and by-ref semantics don't apply). */` |
|   7977968 |  920 | `				if( (iFlags & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_LOAD_IDX_UNSET)) == 0` |
|   7815475 |  921 | `				 && apNode[n]->pOp == 0 && apNode[n]->xCode == PH7_CompileVariable` |
|   4095883 |  922 | `				 && (apNode[n]->iFlags & (EXPR_NODE_NAMED_ARG\|EXPR_NODE_SPREAD)) == 0 ){` |
|   3347283 |  923 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|   1673639 |  924 | `				}` |
|   7977973 |  925 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|   7977973 |  926 | `				if( rc != SXRET_OK ){` |
|         3 |  927 | `					return rc;` |
|         - |  928 | `				}` |
|         - |  929 | `				/* Each argument is an independent nullsafe scope. */` |
|   7977971 |  930 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|   7977971 |  931 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|         - |  932 | `					/* Emit spread opcode to unpack this array argument */` |
|      4065 |  933 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      4065 |  934 | `					hasSpread = 1;` |
|      2030 |  935 | `				}` |
|   3988988 |  936 | `			}` |
|         - |  937 | `			/* Total number of given arguments */` |
|   7025553 |  938 | `			iP1 = nArgs;` |
|   7025553 |  939 | `			iP2 = hasSpread;` |
|         - |  940 | `			/* Build VmCallArgMap if named arguments are present.` |
|         - |  941 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|   7025553 |  942 | `			if( hasNamed ){` |
|       190 |  943 | `				sxu32 nStrBytes = 0;` |
|         - |  944 | `				char *zBuf;` |
|       562 |  945 | `				for( n = 0; n < nArgs; ++n ){` |
|       376 |  946 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       298 |  947 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|       147 |  948 | `					}` |
|       190 |  949 | `				}` |
|         - |  950 | `				{` |
|       190 |  951 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|       190 |  952 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|       186 |  953 | `					&pGen->pVm->sAllocator, mapSize);` |
|       190 |  954 | `				if( pMap ){` |
|       190 |  955 | `					SyZero(pMap, mapSize);` |
|       190 |  956 | `					pMap->bHasNamed = 1;` |
|       190 |  957 | `					pMap->nTotal = (sxu32)nArgs;` |
|       190 |  958 | `					pMap->aNames = (SyString *)&pMap[1];` |
|       190 |  959 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|       562 |  960 | `					for( n = 0; n < nArgs; ++n ){` |
|       376 |  961 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       298 |  962 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|       298 |  963 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|       298 |  964 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|       298 |  965 | `							zBuf += nb;` |
|       147 |  966 | `						}` |
|         - |  967 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|       190 |  968 | `					}` |
|       190 |  969 | `					p3 = (void *)pMap;` |
|        93 |  970 | `				}` |
|         - |  971 | `				}` |
|        93 |  972 | `			}` |
|         - |  973 | `			/* Remove stale flags now */` |
|   7025553 |  974 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|   3512774 |  975 | `		}` |
|         - |  976 | `		{` |
|         - |  977 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|         - |  978 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|         - |  979 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|         - |  980 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|         - |  981 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|         - |  982 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|         - |  983 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|         - |  984 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
|  29464495 |  985 | `			sxi32 iLeftFlags = iFlags;` |
|  29464495 |  986 | `			sxu32 nNullcLhsFirst = PH7_VmInstrLength(pGen->pVm);` |
|  29464495 |  987 | `			int bNullcLhs = 0;` |
|  29464490 |  988 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
|  23923595 |  989 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|   9191376 |  990 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|   7912469 |  991 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   2766985 |  992 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|   1383490 |  993 | `			}` |
|         - |  994 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|         - |  995 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|         - |  996 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|         - |  997 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|         - |  998 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|         - |  999 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|         - | 1000 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
|  29464490 | 1001 | `			if( pNode->pOp` |
|  41424056 | 1002 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
|  26691858 | 1003 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  23919174 | 1004 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|   5963543 | 1005 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|   2981769 | 1006 | `			}` |
|         - | 1007 | ``			/* `++`/`--` mutate their operand in place — the operand is a write`` |
|         - | 1008 | ``			 * lvalue exactly like a compound assign's (`$o->m[0]++` must tag the`` |
|         - | 1009 | ``			 * member base PH7_MEMBER_WRITE the way `$o->m[0] += 1` does: hooked`` |
|         - | 1010 | `			 * properties throw php's Indirect-modification Error, missing ones` |
|         - | 1011 | `			 * auto-vivify). The prec-18 site below handles the assign family;` |
|         - | 1012 | ``			 * `++`/`--` are unary, their operand is pLeft. */`` |
|  29464490 | 1013 | `			if( pNode->pOp` |
|  29464495 | 1014 | `				&& (pNode->pOp->iVmOp == PH7_OP_INCR \|\| pNode->pOp->iVmOp == PH7_OP_DECR) ){` |
|    216733 | 1015 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE` |
|         - | 1016 | ``					\| EXPR_FLAG_RMW_LOAD /* php warns before seeding `$undef++` */;`` |
|    108364 | 1017 | `			}` |
|         - | 1018 | ``			/* `??` reads its LEFT operand in isset-context: an undefined or`` |
|         - | 1019 | `			 * UNINITIALIZED typed PROPERTY must yield the default rather than a` |
|         - | 1020 | `			 * warning/Error (php). Tag a member-access LHS so its OP_MEMBER takes` |
|         - | 1021 | `			 * the silent-lookup path (iP2 = ISSET), which still loads a present` |
|         - | 1022 | `			 * value. A SUBSCRIPT LHS is left untagged: LOAD_IDX's ISSET mode means` |
|         - | 1023 | ``			 * offsetExists (a bool), but `$o[$k] ?? d` needs the offsetGet value —`` |
|         - | 1024 | `			 * that path is already handled correctly by OP_NULLC. */` |
|  29464495 | 1025 | `			if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC && pNode->pLeft ){` |
|         - | 1026 | ``				/* php reads the ENTIRE left operand of `??` in isset-context: no`` |
|         - | 1027 | ``				 * "Undefined variable" for `$x ?? d` OR for the base of`` |
|         - | 1028 | ``				 * `$x['k'] ?? d`. QUIET_VAR silences the variable read wherever it`` |
|         - | 1029 | `				 * sits in the chain. */` |
|     58121 | 1030 | `				iLeftFlags \|= EXPR_FLAG_QUIET_VAR;` |
|     58121 | 1031 | `				bNullcLhs = 1;` |
|     58116 | 1032 | `				if( pNode->pLeft->pOp` |
|     87110 | 1033 | `					&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|     58058 | 1034 | `						\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|     58041 | 1035 | `						\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|         - | 1036 | `					/* A member-access LHS additionally takes OP_MEMBER's silent` |
|         - | 1037 | `					 * lookup (iP2 = ISSET) so an uninitialized typed property` |
|         - | 1038 | `					 * yields the default instead of an Error. A SUBSCRIPT LHS must` |
|         - | 1039 | `					 * NOT: LOAD_IDX's ISSET mode means offsetExists (a bool), while` |
|         - | 1040 | ``					 * `$o[$k] ?? d` needs the offsetGet value — OP_NULLC already`` |
|         - | 1041 | `					 * handles that path. */` |
|        37 | 1042 | `					iLeftFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|        18 | 1043 | `				}` |
|     29058 | 1044 | `			}` |
|  29464495 | 1045 | `			if( iVmOp == PH7_OP_ERR_CTRL ){` |
|         - | 1046 | `				/* '@' must suppress the diagnostics raised WHILE its operand runs, so` |
|         - | 1047 | `				 * open the window here; the trailing emit below closes it (iP1 = 0). */` |
|     15705 | 1048 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_ERR_CTRL,1,0,0,0);` |
|      7850 | 1049 | `			}` |
|  29464495 | 1050 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags\|EXPR_FLAG_RDONLY_LOAD);` |
|  29464495 | 1051 | `			if( rc == SXRET_OK && bNullcLhs ){` |
|         - | 1052 | ``				/* Mark EVERY subscript read in the `??` left chain quiet (iP2=8).`` |
|         - | 1053 | `				 * Peeking at the next instruction only catches the OUTERMOST` |
|         - | 1054 | ``				 * access, so `$d['x']['y'] ?? $v` still warned for the inner one;`` |
|         - | 1055 | `				 * and a forward scan would be unsound, since an unrelated sibling` |
|         - | 1056 | ``				 * access can sit immediately before a coalesce (`f($a['k'],`` |
|         - | 1057 | ``				 * $b['j'] ?? 1)`). The compiler knows the real extent, so it marks`` |
|         - | 1058 | `				 * the range it just emitted. Write-context codes (1/3/5) belong to` |
|         - | 1059 | ``				 * `??=` and keep their meaning. */`` |
|     58121 | 1060 | `				sxu32 nEnd = PH7_VmInstrLength(pGen->pVm);` |
|         - | 1061 | `				sxu32 nAt;` |
|    325163 | 1062 | `				for( nAt = nNullcLhsFirst ; nAt < nEnd ; ++nAt ){` |
|    267047 | 1063 | `					VmInstr *pFix = PH7_VmGetInstr(pGen->pVm,nAt);` |
|    267047 | 1064 | `					if( pFix && pFix->iOp == PH7_OP_LOAD_IDX && pFix->iP2 == 0 ){` |
|     58023 | 1065 | `						pFix->iP2 = 8;` |
|     29009 | 1066 | `					}` |
|    133526 | 1067 | `				}` |
|     29058 | 1068 | `			}` |
|         - | 1069 | `		}` |
|  29464495 | 1070 | `		if( rc != SXRET_OK ){` |
|        32 | 1071 | `			return rc;` |
|         - | 1072 | `		}` |
|  29464467 | 1073 | `		if( !bIsChainOp ){` |
|         - | 1074 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|         - | 1075 | `			 * target the end of that LHS chain, which is right here. */` |
|  13463321 | 1076 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|   6731658 | 1077 | `		}` |
|  29464467 | 1078 | `		if( iVmOp == PH7_OP_CALL ){` |
|   7025553 | 1079 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   7025553 | 1080 | `			if( pInstr ){` |
|   7025553 | 1081 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|   5224157 | 1082 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|         - | 1083 | `					sxu32 nQual;` |
|   5224157 | 1084 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 1085 | `					/* Prevent constant expansion but preserve the absolute flag` |
|         - | 1086 | `					 * so the later NEW handler (if any) can see it. */` |
|   5224157 | 1087 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|         - | 1088 | `					/* Namespace-qualify the function name for CALL, unless the` |
|         - | 1089 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|         - | 1090 | `					 * imports — class imports must NOT affect function` |
|         - | 1091 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|         - | 1092 | `					 * before NEW; we store the original literal index in the` |
|         - | 1093 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|         - | 1094 | `					 * the unqualified name and re-qualify with class imports. */` |
|   5224157 | 1095 | `					if( bAbsolute ){` |
|      3905 | 1096 | `						pInstr->iP2 = (sxi32)nOrig;` |
|      1955 | 1097 | `					}else{` |
|   5220257 | 1098 | `						int fromImport = 0;` |
|   5220257 | 1099 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|   5220257 | 1100 | `						pInstr->iP2 = (sxi32)nQual;` |
|   5220257 | 1101 | `						if( nQual != nOrig ){` |
|         - | 1102 | `							/* Record the original literal index in the arg map` |
|         - | 1103 | `							 * (NOT in the CALL's iP2 — that is the hasSpread` |
|         - | 1104 | `							 * flag) so the NEW handler can recover the` |
|         - | 1105 | `							 * unqualified name and re-qualify with CLASS` |
|         - | 1106 | `							 * imports. */` |
|       103 | 1107 | `							if( p3 == 0 ){` |
|       103 | 1108 | `								VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|        98 | 1109 | `									&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|       103 | 1110 | `								if( pMap ){` |
|       103 | 1111 | `									SyZero(pMap, sizeof(VmCallArgMap));` |
|       103 | 1112 | `									p3 = (void *)pMap;` |
|        49 | 1113 | `								}` |
|        49 | 1114 | `							}` |
|       103 | 1115 | `							if( p3 ){` |
|       103 | 1116 | `								((VmCallArgMap *)p3)->nOrigNameLit = nOrig + 1;` |
|       103 | 1117 | `								if( !fromImport ){` |
|         - | 1118 | `									/* Mark as namespace-qualified */` |
|        93 | 1119 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|        44 | 1120 | `								}` |
|        49 | 1121 | `							}` |
|        49 | 1122 | `						}` |
|         - | 1123 | `					}` |
|   4413477 | 1124 | `				}else if( (pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */` |
|   1791383 | 1125 | `						&& !(pNode->pLeft && (pNode->pLeft->iFlags & EXPR_NODE_PARENS)))` |
|    910720 | 1126 | `					\|\| pInstr->iOp == PH7_OP_NEW ){` |
|         - | 1127 | `					/* Method call,flag that. But NOT when the callee was an explicitly` |
|         - | 1128 | ``					 * PARENTHESISED member access: `($o->p)(...)` / `($o::$p)(...)` invokes`` |
|         - | 1129 | `					 * the VALUE of the property (php's variable-invocation), so the OP_MEMBER` |
|         - | 1130 | `					 * must stay a plain property READ (leaving the callable on the stack for` |
|         - | 1131 | `					 * OP_CALL to invoke) rather than being rewritten into a method-name` |
|         - | 1132 | ``					 * resolution — the parens are exactly what distinguishes `($o->p)()` from`` |
|         - | 1133 | ``					 * the method call `$o->p()`. */`` |
|   1781373 | 1134 | `					pInstr->iP2 = 1;` |
|         - | 1135 | ``					/* A static call with a DYNAMIC method name (`C::$m(...)`): the`` |
|         - | 1136 | ``					 * static-`::` codegen folded the variable NAME into OP_MEMBER->p3`` |
|         - | 1137 | ``					 * as if it were a static-PROPERTY read (`C::$m`), but in a CALL the`` |
|         - | 1138 | `					 * method name is the variable's VALUE. Rebuild the sequence` |
|         - | 1139 | `					 * [class, OP_LOAD $m -> value, OP_MEMBER(static, method)] so the` |
|         - | 1140 | `					 * dynamic name is read off the stack, matching the instance` |
|         - | 1141 | ``					 * (`$o->$m()`) path. iP1==1 marks a static member. */`` |
|   1781373 | 1142 | `					if( pInstr->iOp == PH7_OP_MEMBER && pInstr->iP1 == 1 && pInstr->p3 ){` |
|        11 | 1143 | `						void *pDynName = pInstr->p3;` |
|        11 | 1144 | `						(void)PH7_VmPopInstr(pGen->pVm);` |
|        11 | 1145 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,pDynName,0);` |
|        11 | 1146 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_MEMBER,1,PH7_MEMBER_METHOD,0,0);` |
|         5 | 1147 | `					}` |
|    890684 | 1148 | `				}` |
|   3512779 | 1149 | `			}` |
|  25951693 | 1150 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|         - | 1151 | `			ph7_expr_node **apNode;` |
|         - | 1152 | `			sxi32 n;` |
|   3012065 | 1153 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|         - | 1154 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|         - | 1155 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE` |
|         - | 1156 | `				\|EXPR_FLAG_QUIET_VAR\|EXPR_FLAG_RMW_LOAD);` |
|         - | 1157 | `			/* Recurse and generate bytecodes for array index */` |
|   3012065 | 1158 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   5788093 | 1159 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   2776033 | 1160 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   2776033 | 1161 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   2776033 | 1162 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 1163 | `					return rc;` |
|         - | 1164 | `				}` |
|         - | 1165 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   2776033 | 1166 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   1388019 | 1167 | `			}` |
|   3012065 | 1168 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   2776033 | 1169 | `				iP1 = 1; /* Node have an index associated with it */` |
|   1388014 | 1170 | `			}` |
|   3012065 | 1171 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|         - | 1172 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|    371053 | 1173 | `				iP2 = 4;` |
|   2826541 | 1174 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|         - | 1175 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|         - | 1176 | `				 * so the trailing unset() builtin can drop the slot. */` |
|     23255 | 1177 | `				iP2 = 5;` |
|   2629392 | 1178 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|         - | 1179 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|         - | 1180 | `				 * short-circuit on missing keys without invoking offsetGet` |
|         - | 1181 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|        35 | 1182 | `				iP2 = 6;` |
|   2617752 | 1183 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|         - | 1184 | `				/* Create an empty entry when the desired index is not found */` |
|    553205 | 1185 | `				iP2 = 1;` |
|    276605 | 1186 | `			}` |
|  20932889 | 1187 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         - | 1188 | `			/* POP the left node */` |
|         5 | 1189 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         2 | 1190 | `		}` |
|  14732231 | 1191 | `	}` |
|  29518347 | 1192 | `	rc = SXRET_OK;` |
|  29518347 | 1193 | `	nJmpIdx = 0;` |
|         - | 1194 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|         - | 1195 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|         - | 1196 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  29518347 | 1197 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|    464527 | 1198 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    464527 | 1199 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    464527 | 1200 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    464527 | 1201 | `			int isSpecial = 0;` |
|    464527 | 1202 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|    371787 | 1203 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|    371787 | 1204 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|    371782 | 1205 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    346595 | 1206 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    187778 | 1207 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|    146957 | 1208 | `					isSpecial = 1;` |
|     73476 | 1209 | `				}` |
|    209076 | 1210 | `			}` |
|    510897 | 1211 | `			pInstr->iP1 = 0;` |
|         - | 1212 | ``			/* A leading `\` (NSSEP) makes the class name ABSOLUTE: it names the`` |
|         - | 1213 | `			 * global class, so it must NOT be re-qualified with the current namespace.` |
|         - | 1214 | ``			 * `\Closure::bind(...)` inside `namespace X` is Closure, not X\Closure.`` |
|         - | 1215 | ``			 * (Multi-component `\A\B::m` already resolves absolutely because its`` |
|         - | 1216 | ``			 * literal keeps a backslash; the single-component `\Closure` lost it.) */`` |
|         - | 1217 | `			{` |
|    719973 | 1218 | `				int bAbsolute = (pNode->pLeft && pNode->pLeft->pStart` |
|    627228 | 1219 | `					&& (pNode->pLeft->pStart->nType & PH7_TK_NSSEP));` |
|    418157 | 1220 | `				if( !isSpecial && !bAbsolute ){` |
|    271187 | 1221 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|    135591 | 1222 | `				}` |
|         - | 1223 | `			}` |
|         - | 1224 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|         - | 1225 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|    418157 | 1226 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|    271205 | 1227 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|    271205 | 1228 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|        78 | 1229 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|        78 | 1230 | `					return SXRET_OK;` |
|         - | 1231 | `				}` |
|    135563 | 1232 | `			}` |
|    209039 | 1233 | `		}` |
|    301752 | 1234 | `	}` |
|         - | 1235 | `	/* Generate code for the right tree */` |
|  29471921 | 1236 | `	if( pNode->pRight ){` |
|  16926451 | 1237 | `		if( iVmOp == PH7_OP_LAND ){` |
|         - | 1238 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    448497 | 1239 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  16702205 | 1240 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|         - | 1241 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    324531 | 1242 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  16315696 | 1243 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|         - | 1244 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     58121 | 1245 | `			iVmOp = 0; /* No binary operator to emit */` |
|     58121 | 1246 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  16124427 | 1247 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|         - | 1248 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|         - | 1249 | `			 * the entire containing postfix chain to null. The jump target is` |
|         - | 1250 | `			 * patched later by the innermost non-chain ancestor (or by` |
|         - | 1251 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|         - | 1252 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|         - | 1253 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|       107 | 1254 | `			sxu32 nNsJmp = 0;` |
|       107 | 1255 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|       107 | 1256 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  16095265 | 1257 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|         - | 1258 | `			/* The lvalue is the RIGHT operand (these ops are right-associative). Mark it a write` |
|         - | 1259 | ``			 * target so a missing member (the base of a subscript-write, or a bare `$o->p`) is`` |
|         - | 1260 | `			 * auto-created — PHP auto-vivifies on write. */` |
|   5191167 | 1261 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|   5191167 | 1262 | `			if( iVmOp != PH7_OP_STORE ){` |
|         - | 1263 | ``				/* COMPOUND assignment (`.=`, `+=`, ...) READS the target first, so`` |
|         - | 1264 | `` 				 * php warns when it is undefined and then seeds it; a plain `=` `` |
|         - | 1265 | `				 * writes without reading and stays silent. */` |
|    448359 | 1266 | `				iFlags \|= EXPR_FLAG_RMW_LOAD;` |
|    224177 | 1267 | `			}` |
|   2595581 | 1268 | `		}` |
|  16926451 | 1269 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  16926451 | 1270 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_RDONLY_LOAD);` |
|  16926451 | 1271 | `		if( !bIsChainOp ){` |
|         - | 1272 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|         - | 1273 | `			 * operator instruction is emitted. */` |
|  10962987 | 1274 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|   5481491 | 1275 | `		}` |
|  16926451 | 1276 | `		if( iVmOp == PH7_OP_STORE ){` |
|   4742813 | 1277 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|   4742776 | 1278 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|         - | 1279 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|         - | 1280 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|         - | 1281 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|         - | 1282 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|         - | 1283 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|         - | 1284 | `				 */` |
|        99 | 1285 | `				iVmOp = 0;` |
|   4742766 | 1286 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|   4742719 | 1287 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 1288 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|    916069 | 1289 | `					iP2 = 1;` |
|    458037 | 1290 | `				}else{` |
|   3826655 | 1291 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 1292 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|    533795 | 1293 | `						iVmOp = PH7_OP_STORE_IDX;` |
|    533795 | 1294 | `						iP1 = pInstr->iP1;` |
|    266900 | 1295 | `					}else{` |
|   3292865 | 1296 | `						p3 = pInstr->p3;` |
|         - | 1297 | `					}` |
|         - | 1298 | `					/* POP the last dynamic load instruction */` |
|   3826655 | 1299 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         - | 1300 | `				}` |
|   2371362 | 1301 | `			}` |
|  14555047 | 1302 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|         - | 1303 | `			/* Peek first: a member LHS ($o->p =& $x, C::$s =& $x) keeps its` |
|         - | 1304 | `			 * OP_MEMBER in place (it resolves + stashes the target slot at` |
|         - | 1305 | `			 * runtime), unlike the variable/array shapes which fold their load` |
|         - | 1306 | `			 * away. Mirrors the normal member-store fold above (iP2 kept-MEMBER). */` |
|        73 | 1307 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|        73 | 1308 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 1309 | `				/* Tag the member as a reference target and flag STORE_REF (iP2=1)` |
|         - | 1310 | `				 * to take the member-rebind path in the VM. */` |
|        11 | 1311 | `				pInstr->iP2 = PH7_MEMBER_REF_TARGET;` |
|        11 | 1312 | `				iP2 = 1;` |
|         6 | 1313 | `			}else{` |
|        63 | 1314 | `				pInstr = PH7_VmPopInstr(pGen->pVm);` |
|        63 | 1315 | `				if( pInstr ){` |
|        63 | 1316 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 1317 | `						/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|         - | 1318 | `						 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|         - | 1319 | `						 */` |
|        19 | 1320 | `						iVmOp = PH7_OP_STORE_IDX_REF;` |
|        19 | 1321 | `						iP1 = pInstr->iP1;` |
|        19 | 1322 | `						iP2 = pInstr->iP2;` |
|        19 | 1323 | `						p3  = pInstr->p3;` |
|        10 | 1324 | `					}else{` |
|        45 | 1325 | `						p3 = pInstr->p3;` |
|         - | 1326 | `					}` |
|        30 | 1327 | `				}` |
|         - | 1328 | `			}` |
|        35 | 1329 | `		}` |
|   8463223 | 1330 | `	}` |
|  29471916 | 1331 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|    430512 | 1332 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|         - | 1333 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|         - | 1334 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|        34 | 1335 | `		iVmOp = 0;` |
|        15 | 1336 | `	}` |
|  29471921 | 1337 | `	if( iVmOp > 0 ){` |
|  29413677 | 1338 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|    216733 | 1339 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|         - | 1340 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|     15481 | 1341 | `				iP1 = 1;` |
|      7743 | 1342 | `			}` |
|  29305313 | 1343 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|         - | 1344 | `			/* Namespace-qualify the class name for NEW */ {` |
|    864231 | 1345 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|    864231 | 1346 | `				VmInstr *pCallInstr = 0;` |
|    864231 | 1347 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|    852447 | 1348 | `					pCallInstr = pPeek;` |
|    852447 | 1349 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    426221 | 1350 | `				}` |
|    864231 | 1351 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|    848775 | 1352 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 1353 | `					sxu32 nLitForClass;` |
|    848775 | 1354 | `					VmCallArgMap *pCallNsMap = pCallInstr ? (VmCallArgMap *)pCallInstr->p3 : 0;` |
|         - | 1355 | `					/* If the CALL handler qualified the name with FUNCTION` |
|         - | 1356 | `					 * imports, recover the original literal (recorded in the` |
|         - | 1357 | `					 * arg map — OP_CALL's iP2 is the hasSpread flag, and` |
|         - | 1358 | `` 					 * misreading it as a literal index made `new C(...$args)` `` |
|         - | 1359 | `					 * fatal with "Class ' ' is not defined") and re-qualify` |
|         - | 1360 | `					 * with class imports. */` |
|    848775 | 1361 | `					if( pCallNsMap && pCallNsMap->nOrigNameLit > 0 ){` |
|        55 | 1362 | `						nLitForClass = pCallNsMap->nOrigNameLit - 1;` |
|        30 | 1363 | `					}else{` |
|    848725 | 1364 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|         - | 1365 | `					}` |
|    848775 | 1366 | `					pPeek->iP1 = 0;` |
|    848775 | 1367 | `					if( !bAbsolute ){` |
|         - | 1368 | `						/* self/static/parent are resolved at runtime against the` |
|         - | 1369 | `						 * current class — never namespace-qualify them (else` |
|         - | 1370 | ``						 * `new self` in namespace N becomes "N\self"). Mirrors the`` |
|         - | 1371 | `						 * instanceof (IS_A) guard below. */` |
|    844885 | 1372 | `						ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nLitForClass);` |
|    844885 | 1373 | `						int isSpecialNew = 0;` |
|    844885 | 1374 | `						if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|    829829 | 1375 | `							const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|    829829 | 1376 | `							sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|    829824 | 1377 | `							if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    833535 | 1378 | `								(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    414860 | 1379 | `								(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      7763 | 1380 | `								isSpecialNew = 1;` |
|      3879 | 1381 | `							}` |
|    418676 | 1382 | `						}` |
|    852413 | 1383 | `						if( isSpecialNew ){` |
|      7763 | 1384 | `							pPeek->iP2 = (sxi32)nLitForClass;` |
|      3884 | 1385 | `						}else{` |
|    829599 | 1386 | `							pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|         - | 1387 | `						}` |
|    418681 | 1388 | `					}else{` |
|      3895 | 1389 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|         - | 1390 | `					}` |
|    420621 | 1391 | `				}` |
|         - | 1392 | `			}` |
|    856703 | 1393 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    856703 | 1394 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|         - | 1395 | `				VmInstr *pPrev;` |
|    852447 | 1396 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|    852447 | 1397 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|         - | 1398 | `					/* Pop the call instruction, preserve named-arg map and` |
|         - | 1399 | `					 * the hasSpread flag (OP_NEW consumes the spread` |
|         - | 1400 | `					 * accumulator exactly like OP_CALL would have). */` |
|    852447 | 1401 | `					iP1 = pInstr->iP1;` |
|    852447 | 1402 | `					iP2 = pInstr->iP2;` |
|    852447 | 1403 | `					if( pInstr->p3 ){` |
|        65 | 1404 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|        30 | 1405 | `					}` |
|    852447 | 1406 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    426221 | 1407 | `				}` |
|    426226 | 1408 | `			}` |
|  28761072 | 1409 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|         - | 1410 | `			/* instanceof: right operand is a class name, not a constant.` |
|         - | 1411 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     81415 | 1412 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     81415 | 1413 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     81415 | 1414 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     81415 | 1415 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     81415 | 1416 | `				int isSpecialIs = 0;` |
|     81415 | 1417 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     81415 | 1418 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     81415 | 1419 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     81410 | 1420 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     81413 | 1421 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     40705 | 1422 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|        12 | 1423 | `						isSpecialIs = 1;` |
|         5 | 1424 | `					}` |
|     40705 | 1425 | `				}` |
|     81415 | 1426 | `				pInstr->iP1 = 0;` |
|     81415 | 1427 | `				if( !isSpecialIs && !bAbsolute ){` |
|     81395 | 1428 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     40695 | 1429 | `				}` |
|     40710 | 1430 | `			}` |
|  28292018 | 1431 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|         - | 1432 | `			/* Prevent constant expansion for member/property names.` |
|         - | 1433 | `			 * The right child (member name) was just compiled — its LOADC` |
|         - | 1434 | `			 * should not trigger constant lookup. */` |
|   5963469 | 1435 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   5963469 | 1436 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|   5723923 | 1437 | `				pInstr->iP1 = 0;` |
|   2861959 | 1438 | `			}` |
|   5963469 | 1439 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|         - | 1440 | `				/* Static member access,remember that */` |
|    418101 | 1441 | `				iP1 = 1;` |
|    418101 | 1442 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    418101 | 1443 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|    235675 | 1444 | `					p3 = pInstr->p3;` |
|    235675 | 1445 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    117835 | 1446 | `				}` |
|    209048 | 1447 | `			}` |
|         - | 1448 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|         - | 1449 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|         - | 1450 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|         - | 1451 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|   5963469 | 1452 | `			if( iP2 == PH7_MEMBER_READ ){` |
|   5963469 | 1453 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|        42 | 1454 | `					iP2 = PH7_MEMBER_UNSET;` |
|   5963449 | 1455 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|     69659 | 1456 | `					iP2 = PH7_MEMBER_ISSET;` |
|   5928602 | 1457 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|        17 | 1458 | `					iP2 = PH7_MEMBER_EMPTY;` |
|   5893767 | 1459 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|         - | 1460 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|   1109355 | 1461 | `					iP2 = PH7_MEMBER_WRITE;` |
|    554675 | 1462 | `				}` |
|   2981732 | 1463 | `			}` |
|   2981732 | 1464 | `		}` |
|         - | 1465 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|         - | 1466 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|         - | 1467 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|         - | 1468 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|         - | 1469 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
|  29406149 | 1470 | `		if( bFcc ){` |
|        84 | 1471 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|        84 | 1472 | `			iP2 = 0;` |
|        84 | 1473 | `			p3 = 0;` |
|        84 | 1474 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|        84 | 1475 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 1476 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|         - | 1477 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|         - | 1478 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|         - | 1479 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|        37 | 1480 | `				void *pMemberName = pInstr->p3;` |
|        37 | 1481 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|        37 | 1482 | `				if( pMemberName ){` |
|       ! 0 | 1483 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|       ! 0 | 1484 | `				}` |
|        37 | 1485 | `				iP1 = 2;` |
|        19 | 1486 | `			}else{` |
|        48 | 1487 | `				iP1 = 1;` |
|         - | 1488 | `			}` |
|        41 | 1489 | `		}` |
|         - | 1490 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|         - | 1491 | `		 * This is the primary emit path for user-visible calls. */` |
|  29406149 | 1492 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|   7882169 | 1493 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|   3941082 | 1494 | `		}` |
|         - | 1495 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  29406149 | 1496 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  14703072 | 1497 | `	}` |
|  29464393 | 1498 | `	if( nJmpIdx > 0 ){` |
|         - | 1499 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|    831139 | 1500 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|    831139 | 1501 | `		if( pInstr ){` |
|    831139 | 1502 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    415567 | 1503 | `		}` |
|    415567 | 1504 | `	}` |
|  29464393 | 1505 | `	return rc;` |
|  37834876 | 1506 | `}` |
|         - | 1507 | `/*` |
|         - | 1508 | ` * Compile a PHP expression.` |
|         - | 1509 | ` * According to the PHP language reference manual:` |
|         - | 1510 | ` *  Expressions are the most important building stones of PHP.` |
|         - | 1511 | ` *  In PHP, almost anything you write is an expression.` |
|         - | 1512 | ` *  The simplest yet most accurate way to define an expression` |
|         - | 1513 | ` *  is "anything that has a value".` |
|         - | 1514 | ` * If something goes wrong while compiling the expression,this` |
|         - | 1515 | ` * function takes care of generating the appropriate error` |
|         - | 1516 | ` * message.` |
|         - | 1517 | ` */` |
|         - | 1518 | `/*` |
|         - | 1519 | ` * Does this expression tree contain a comma OPERATOR node?` |
|         - | 1520 | ` *` |
|         - | 1521 | `` * PH7 shipped `,` as a lowest-precedence binary operator (IMP-0139-COMMA), so`` |
|         - | 1522 | `` * `(1, 2)` and `$x = (f(), $y)` compile and evaluate to the right operand.`` |
|         - | 1523 | ` * php 8 has no comma operator: its grammar only allows comma-separated` |
|         - | 1524 | ` * expression LISTS inside for(...) clauses (call arguments, array literals and` |
|         - | 1525 | ` * list() are split by the parser, never by this node). Accepting it changes the` |
|         - | 1526 | ` * meaning of source php rejects, which §10 classes as a bug — so every context` |
|         - | 1527 | ` * except for() now reports php's parse error.` |
|         - | 1528 | ` */` |
| 251191806 | 1529 | `static int GenStateTreeHasComma(ph7_expr_node *pNode)` |
|         5 | 1530 | `{` |
|         - | 1531 | `	ph7_expr_node **apArg;` |
|         - | 1532 | `	sxu32 n;` |
| 251191811 | 1533 | `	if( pNode == 0 ){` |
| 176596847 | 1534 | `		return 0;` |
|         - | 1535 | `	}` |
|  74594969 | 1536 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         6 | 1537 | `		return 1;` |
|         - | 1538 | `	}` |
|  74594960 | 1539 | `	if( GenStateTreeHasComma(pNode->pLeft) \|\| GenStateTreeHasComma(pNode->pRight)` |
|  74594961 | 1540 | `	 \|\| GenStateTreeHasComma(pNode->pCond) ){` |
|         6 | 1541 | `		return 1;` |
|         - | 1542 | `	}` |
|  74594961 | 1543 | `	apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  85325867 | 1544 | `	for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; n++ ){` |
|  10730911 | 1545 | `		if( GenStateTreeHasComma(apArg[n]) ){` |
|       ! 0 | 1546 | `			return 1;` |
|         - | 1547 | `		}` |
|   5365458 | 1548 | `	}` |
|  74594961 | 1549 | `	return 0;` |
| 125595908 | 1550 | `}` |
|  17047456 | 1551 | `PH7_PRIVATE sxi32 PH7_CompileExpr(` |
|         - | 1552 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 1553 | `	sxi32 iFlags,        /* Control flags */` |
|         - | 1554 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|         - | 1555 | `	)` |
|         5 | 1556 | `{` |
|         - | 1557 | `	ph7_expr_node *pRoot;` |
|         - | 1558 | `	SySet sExprNode;` |
|         - | 1559 | `	SyToken *pEnd;` |
|         - | 1560 | `	sxi32 nExpr;` |
|         - | 1561 | `	sxi32 iNest;` |
|         - | 1562 | `	sxi32 rc;` |
|         - | 1563 | `	sxu32 nNullsafeBase;` |
|         - | 1564 | `	/* Initialize worker variables */` |
|  17047461 | 1565 | `	nExpr = 0;` |
|  17047461 | 1566 | `	pRoot = 0;` |
|         - | 1567 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|         - | 1568 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  17047461 | 1569 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  17047461 | 1570 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  17047461 | 1571 | `	SySetAlloc(&sExprNode,0x10);` |
|  17047461 | 1572 | `	rc = SXRET_OK;` |
|         - | 1573 | `	/* Delimit the expression */` |
|  17047461 | 1574 | `	pEnd = pGen->pIn;` |
|  17047461 | 1575 | `	iNest = 0;` |
| 133451905 | 1576 | `	while( pEnd < pGen->pEnd ){` |
| 126897207 | 1577 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 1578 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|      4741 | 1579 | `			iNest++;` |
| 126894839 | 1580 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|      4751 | 1581 | `			iNest--;` |
| 126890098 | 1582 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  10493691 | 1583 | `			if( iNest <= 0 ){` |
|  10492763 | 1584 | `				break;` |
|         - | 1585 | `			}` |
|       464 | 1586 | `		}` |
| 116404449 | 1587 | `		pEnd++;` |
|         5 | 1588 | `	}` |
|  17047461 | 1589 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|    734937 | 1590 | `		SyToken *pEnd2 = pGen->pIn;` |
|    734937 | 1591 | `		iNest = 0;` |
|         - | 1592 | `		/* Stop at the first comma */` |
|   1614001 | 1593 | `		while( pEnd2 < pEnd ){` |
|    879071 | 1594 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|     50337 | 1595 | `				iNest++;` |
|    853905 | 1596 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|     50337 | 1597 | `				iNest--;` |
|    803573 | 1598 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      6067 | 1599 | `				if( iNest <= 0 ){` |
|         3 | 1600 | `					break;` |
|         - | 1601 | `				}` |
|      3030 | 1602 | `			}` |
|    879069 | 1603 | `			pEnd2++;` |
|         5 | 1604 | `		}` |
|    734937 | 1605 | `		if( pEnd2 <pEnd ){` |
|         3 | 1606 | `			pEnd = pEnd2;` |
|         1 | 1607 | `		}` |
|    367466 | 1608 | `	}` |
|  17047461 | 1609 | `	if( pEnd > pGen->pIn ){` |
|  17024281 | 1610 | `		SyToken *pTmp = pGen->pEnd;` |
|         - | 1611 | `		/* Swap delimiter */` |
|  17024281 | 1612 | `		pGen->pEnd = pEnd;` |
|         - | 1613 | `		/* Try to get an expression tree */` |
|  17024281 | 1614 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  17024276 | 1615 | `		if( rc == SXRET_OK && pRoot && pGen->nCommaExprOk < 1` |
|  16850065 | 1616 | `		 && GenStateTreeHasComma(pRoot) ){` |
|         - | 1617 | `			/* php has no comma operator outside a for() clause */` |
|         6 | 1618 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pRoot->pStart->nLine,` |
|         - | 1619 | `				"syntax error, unexpected token \",\"");` |
|         6 | 1620 | `			pGen->pEnd = pTmp;` |
|         6 | 1621 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 1622 | `				SySetRelease(&sExprNode);` |
|       ! 0 | 1623 | `				return SXERR_ABORT;` |
|         - | 1624 | `			}` |
|         6 | 1625 | `			pGen->pIn = pEnd;` |
|         6 | 1626 | `			SySetRelease(&sExprNode);` |
|         6 | 1627 | `			SySetTruncate(&pGen->aNullsafeJmp,nNullsafeBase);` |
|         6 | 1628 | `			return SXRET_OK;` |
|         - | 1629 | `		}` |
|  17024277 | 1630 | `		if( rc == SXRET_OK && pRoot ){` |
|  17024093 | 1631 | `			rc = SXRET_OK;` |
|  17024093 | 1632 | `			if( xTreeValidator ){` |
|         - | 1633 | `				/* Call the upper layer validator callback */` |
|   1029261 | 1634 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    514628 | 1635 | `			}` |
|  17024093 | 1636 | `			if( rc != SXERR_ABORT ){` |
|         - | 1637 | `				/* Generate code for the given tree */` |
|  17024093 | 1638 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|         - | 1639 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|         - | 1640 | `				 * expression so they short-circuit to its end. */` |
|  17024093 | 1641 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|   8512044 | 1642 | `			}` |
|  17024093 | 1643 | `			nExpr = 1;` |
|   8512044 | 1644 | `		}` |
|         - | 1645 | `		/* Release the whole tree */` |
|  17024277 | 1646 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|         - | 1647 | `		/* Synchronize token stream */` |
|  17024277 | 1648 | `		pGen->pEnd = pTmp;` |
|  17024277 | 1649 | `		pGen->pIn  = pEnd;` |
|  17024277 | 1650 | `		if( rc == SXERR_ABORT ){` |
|        24 | 1651 | `			SySetRelease(&sExprNode);` |
|        24 | 1652 | `			return SXERR_ABORT;` |
|         - | 1653 | `		}` |
|   8512126 | 1654 | `	}` |
|  17047437 | 1655 | `	SySetRelease(&sExprNode);` |
|  17047437 | 1656 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|   8523733 | 1657 | `}` |
|         - | 1658 | `/*` |
|         - | 1659 | ` * Return a pointer to the node construct handler associated` |
|         - | 1660 | ` * with a given node type [i.e: string,integer,float,...].` |
|         - | 1661 | ` */` |
|   9468670 | 1662 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|         5 | 1663 | `{` |
|   9468675 | 1664 | `	if( nNodeType & PH7_TK_NUM ){` |
|         - | 1665 | `		/* Numeric literal: Either real or integer */` |
|   3808067 | 1666 | `		return PH7_CompileNumLiteral;` |
|   5660613 | 1667 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|         - | 1668 | `		/* Double quoted string */` |
|    124837 | 1669 | `		return PH7_CompileString;` |
|   5535781 | 1670 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|         - | 1671 | `		/* Single quoted string */` |
|   5535657 | 1672 | `		return PH7_CompileSimpleString;` |
|       129 | 1673 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|         - | 1674 | `		/* Heredoc */` |
|        73 | 1675 | `		return PH7_CompileHereDoc;` |
|        60 | 1676 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|         - | 1677 | `		/* Nowdoc */` |
|        55 | 1678 | `		return PH7_CompileNowDoc;` |
|         5 | 1679 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|         - | 1680 | `		/* Backtick quoted string */` |
|         3 | 1681 | `		return PH7_CompileBacktic;` |
|         - | 1682 | `	}` |
|         3 | 1683 | `	return 0;` |
|   4734340 | 1684 | `}` |
|         - | 1685 | `/*` |
|         - | 1686 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|         - | 1687 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|         - | 1688 | ` * in write context" parse error.` |
|         - | 1689 | ` */` |
|     23292 | 1690 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|         5 | 1691 | `{` |
|         - | 1692 | `	sxi32 rc;` |
|     23297 | 1693 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|     23295 | 1694 | `		return SXRET_OK;` |
|         - | 1695 | `	}` |
|         5 | 1696 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|         2 | 1697 | `		pNode ? pNode->pStart->nLine : 1,` |
|         - | 1698 | `		"Can't use nullsafe operator in write context");` |
|         3 | 1699 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|     11651 | 1700 | `}` |
|         - | 1701 | `/*` |
|         - | 1702 | ` * Compile an unset() statement.` |
|         - | 1703 | ` * unset($var, $arr[$key], ...);` |
|         - | 1704 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|         - | 1705 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|         - | 1706 | ` * parent array before extracting the element to unset.` |
|         - | 1707 | ` */` |
|     26008 | 1708 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|         5 | 1709 | `{` |
|     26013 | 1710 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|     26013 | 1711 | `	sxu32 nIdx = 0;` |
|         - | 1712 | `	SyString sName;` |
|         - | 1713 | `	sxi32 rc;` |
|         - | 1714 | `	/* Jump the 'unset' keyword */` |
|     26013 | 1715 | `	pGen->pIn++;` |
|         - | 1716 | `	/* Save delimiter */` |
|     26013 | 1717 | `	pTmp = pGen->pEnd;` |
|         - | 1718 | `	/* Skip optional opening parenthesis and find the matching close */` |
|     26013 | 1719 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|     26013 | 1720 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 1721 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|         - | 1722 | `		SyToken *pClose;` |
|     26013 | 1723 | `		pGen->pIn++;   /* Skip '(' */` |
|     26013 | 1724 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|     26013 | 1725 | `		pEnd = pClose; /* Stop at ')' */` |
|     13004 | 1726 | `	}` |
|     26013 | 1727 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|         - | 1728 | `	/* Resolve the 'unset' builtin name once */` |
|     26013 | 1729 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|      3867 | 1730 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3867 | 1731 | `		if( pObj == 0 ){` |
|       ! 0 | 1732 | `			return SXERR_ABORT;` |
|         - | 1733 | `		}` |
|      3867 | 1734 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      3867 | 1735 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      1931 | 1736 | `	}` |
|         - | 1737 | `	/* Compile each comma-separated argument */` |
|     55881 | 1738 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|     29873 | 1739 | `		if( pGen->pIn < pNext ){` |
|         - | 1740 | `			/*` |
|         - | 1741 | ``			 * A PLAIN variable (`unset($x)`, exactly two tokens: '$' and the name) drops a`` |
|         - | 1742 | `			 * single NAME binding, which the unset() builtin cannot express — all it ever` |
|         - | 1743 | `			 * receives is the value's slot index, and unsetting the slot destroys whatever` |
|         - | 1744 | `			 * else aliases it. Emit OP_UNSET_VAR with the name instead. Subscripts and` |
|         - | 1745 | ``			 * properties (`unset($a[k])`, `unset($o->p)`) keep the existing path, which`` |
|         - | 1746 | `			 * already removes just the element/property.` |
|         - | 1747 | `			 */` |
|     29868 | 1748 | `			if( &pGen->pIn[2] == pNext` |
|     18222 | 1749 | `				&& (pGen->pIn[0].nType & PH7_TK_DOLLAR)` |
|      6581 | 1750 | `				&& (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         - | 1751 | `				SyString *pVarName;` |
|      9866 | 1752 | `				char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      6574 | 1753 | `					pGen->pIn[1].sData.zString,pGen->pIn[1].sData.nByte);` |
|      6579 | 1754 | `				pVarName = (SyString *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(SyString));` |
|      6579 | 1755 | `				if( zDup == 0 \|\| pVarName == 0 ){` |
|       ! 0 | 1756 | `					PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - | 1757 | `						"Fatal, PH7 is running out of memory");` |
|       ! 0 | 1758 | `					return SXERR_ABORT;` |
|         - | 1759 | `				}` |
|      6579 | 1760 | `				SyStringInitFromBuf(pVarName,zDup,pGen->pIn[1].sData.nByte);` |
|      6579 | 1761 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_UNSET_VAR,0,0,pVarName,0);` |
|      6579 | 1762 | `				pGen->pIn = pNext;` |
|      6579 | 1763 | `				if( pGen->pIn < pEnd ){` |
|      3861 | 1764 | `					pGen->pIn++; /* Jump the trailing comma */` |
|      1928 | 1765 | `				}` |
|      6579 | 1766 | `				continue;` |
|         - | 1767 | `			}` |
|     23299 | 1768 | `			pGen->pEnd = pNext;` |
|     23299 | 1769 | `			rc = PH7_CompileExpr(&(*pGen),` |
|         - | 1770 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|         - | 1771 | `				GenStateUnsetValidator);` |
|     23299 | 1772 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 1773 | `				return SXERR_ABORT;` |
|         - | 1774 | `			}` |
|     23299 | 1775 | `			if( rc != SXERR_EMPTY ){` |
|         - | 1776 | `				/* Emit call for this single argument */` |
|     23297 | 1777 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     23297 | 1778 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|     23297 | 1779 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     11646 | 1780 | `			}` |
|     11647 | 1781 | `		}` |
|         - | 1782 | `		/* Jump trailing commas */` |
|     23305 | 1783 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|         7 | 1784 | `			pNext++;` |
|         1 | 1785 | `		}` |
|     23299 | 1786 | `		pGen->pIn = pNext;` |
|         5 | 1787 | `	}` |
|         - | 1788 | `	/* Skip past the closing ')' if present */` |
|     26013 | 1789 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|     26013 | 1790 | `		pGen->pIn++;` |
|     13004 | 1791 | `	}` |
|         - | 1792 | `	/* Restore token stream */` |
|     26013 | 1793 | `	pGen->pEnd = pTmp;` |
|     26013 | 1794 | `	return SXRET_OK;` |
|     13009 | 1795 | `}` |
|         - | 1796 | `/*` |
|         - | 1797 | ` * PHP Language construct table.` |
|         - | 1798 | ` */` |
|         - | 1799 | `static const LangConstruct aLangConstruct[] = {` |
|         - | 1800 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|         - | 1801 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|         - | 1802 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|         - | 1803 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|         - | 1804 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|         - | 1805 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|         - | 1806 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|         - | 1807 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|         - | 1808 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|         - | 1809 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|         - | 1810 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|         - | 1811 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|         - | 1812 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|         - | 1813 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|         - | 1814 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|         - | 1815 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|         - | 1816 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|         - | 1817 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|         - | 1818 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|         - | 1819 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|         - | 1820 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|         - | 1821 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|         - | 1822 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|         - | 1823 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|         - | 1824 | `};` |
|         - | 1825 | `/*` |
|         - | 1826 | ` * Return a pointer to the statement handler routine associated` |
|         - | 1827 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|         - | 1828 | ` */` |
|   8420728 | 1829 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|         - | 1830 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|         - | 1831 | `	SyToken *pLookahed  /* Look-ahead token */` |
|         - | 1832 | `	)` |
|         5 | 1833 | `{` |
|   8420733 | 1834 | `	sxu32 n = 0;` |
|  34073427 | 1835 | `	for(;;){` |
|  68146859 | 1836 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|    539373 | 1837 | `			break;` |
|         - | 1838 | `		}` |
|  67607491 | 1839 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|   7881365 | 1840 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|       ! 0 | 1841 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|       ! 0 | 1842 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|         - | 1843 | `					/* 'static' (class context),return null */` |
|       ! 0 | 1844 | `					return 0;` |
|         - | 1845 | `				}` |
|       ! 0 | 1846 | `			}` |
|   7881360 | 1847 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|     11600 | 1848 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|      5807 | 1849 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|         - | 1850 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|         3 | 1851 | `				return 0;` |
|         - | 1852 | `			}` |
|         - | 1853 | `			/* Return a pointer to the handler.` |
|         - | 1854 | `			*/` |
|   7881363 | 1855 | `			return aLangConstruct[n].xConstruct;` |
|         - | 1856 | `		}` |
|  59726131 | 1857 | `		n++;` |
|         5 | 1858 | `	}` |
|    539373 | 1859 | `	if( pLookahed ){` |
|    539373 | 1860 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|     69645 | 1861 | `			return PH7_CompileClassInterface;` |
|    469733 | 1862 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|    403487 | 1863 | `			return PH7_CompileClass;` |
|     66251 | 1864 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      7829 | 1865 | `			return PH7_CompileTrait;` |
|         - | 1866 | `		}` |
|         - | 1867 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|         - | 1868 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|         - | 1869 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|         - | 1870 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     29211 | 1871 | `	}` |
|         - | 1872 | `	/* Not a language construct */` |
|     58427 | 1873 | `	return 0;` |
|   4210369 | 1874 | `}` |
|         - | 1875 | `/*` |
|         - | 1876 | ` * Check if the given keyword is in fact a PHP language construct.` |
|         - | 1877 | ` * Return TRUE on success. FALSE otheriwse.` |
|         - | 1878 | ` */` |
|     58424 | 1879 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|         5 | 1880 | `{` |
|         - | 1881 | `	int rc;` |
|     58429 | 1882 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     58429 | 1883 | `	if( rc == FALSE ){` |
|     58314 | 1884 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|     15818 | 1885 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|         - | 1886 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|         - | 1887 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|         - | 1888 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|         - | 1889 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|         - | 1890 | `			*/` |
|         - | 1891 | `			){` |
|     58311 | 1892 | `				rc = TRUE;` |
|     29153 | 1893 | `		}` |
|     29157 | 1894 | `	}` |
|     58429 | 1895 | `	return rc;` |
|         5 | 1896 | `}` |
|         - | 1897 | `/*` |
|         - | 1898 | ` * Compile a PHP chunk.` |
|         - | 1899 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 1900 | ` * takes care of generating the appropriate error message.` |
|         - | 1901 | ` */` |
|         - | 1902 | `/*` |
|         - | 1903 | ` * Update pGen->sPendingDoc for the statement whose first token is` |
|         - | 1904 | ` * pGen->pIn: when a docblock trivia is keyed to that token's index in` |
|         - | 1905 | ` * the chunk token set it becomes the pending docblock. An existing` |
|         - | 1906 | ` * pending docblock is LEFT in place otherwise: Zend keeps the last-seen` |
|         - | 1907 | ` * doc comment until a declaration consumes it, so a docblock survives` |
|         - | 1908 | ` * intervening non-declaration statements.` |
|         - | 1909 | ` */` |
|  18063318 | 1910 | `PH7_PRIVATE void GenStateSetPendingDoc(ph7_gen_state *pGen)` |
|         5 | 1911 | `{` |
|  18063323 | 1912 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|  18063323 | 1913 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|  18063323 | 1914 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 1915 | `	sxu32 nIdx, n;` |
|  18063318 | 1916 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|   3413065 | 1917 | `	 \|\| pGen->pIn < pBase \|\| pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|         - | 1918 | `		/* Re-tokenized substream (string interpolation, synthesized code):` |
|         - | 1919 | `		 * indexes do not map to the sidecar */` |
|  14650265 | 1920 | `		return;` |
|         - | 1921 | `	}` |
|   3413063 | 1922 | `	nIdx = (sxu32)(pGen->pIn - pBase);` |
|         - | 1923 | `	/* Attributes must be adjacent to their declaration (unlike docblocks):` |
|         - | 1924 | `	 * reset at every boundary, then collect the groups keyed to this token. */` |
|   3413063 | 1925 | `	SySetReset(&pGen->aPendingAttrs);` |
|  10240939 | 1926 | `	for( n = 0 ; n < nT ; n++ ){` |
|   6827881 | 1927 | `		if( aT[n].nTokIdx != nIdx ){` |
|   6819985 | 1928 | `			continue;` |
|         - | 1929 | `		}` |
|      7901 | 1930 | `		if( aT[n].iKind == PH7_TRIVIA_DOC ){` |
|        29 | 1931 | `			pGen->sPendingDoc = aT[n].sText;` |
|      7889 | 1932 | `		}else if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|      7877 | 1933 | `			SySetPut(&pGen->aPendingAttrs,(const void *)&aT[n]);` |
|      3936 | 1934 | `		}` |
|      3953 | 1935 | `	}` |
|   9031664 | 1936 | `}` |
|         - | 1937 | `/*` |
|         - | 1938 | ` * Hand the pending docblock (if any) to a declaration: duplicate it into` |
|         - | 1939 | ` * the VM allocator (the raw script buffer dies after compilation) and` |
|         - | 1940 | ` * clear the pending slot so sibling declarations do not inherit it.` |
|         - | 1941 | ` */` |
|   4865882 | 1942 | `PH7_PRIVATE void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut)` |
|         5 | 1943 | `{` |
|         - | 1944 | `	char *zDup;` |
|   4865887 | 1945 | `	if( SyStringLength(&pGen->sPendingDoc) < 1 ){` |
|   4865867 | 1946 | `		return;` |
|         - | 1947 | `	}` |
|        35 | 1948 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        10 | 1949 | `		SyStringData(&pGen->sPendingDoc),SyStringLength(&pGen->sPendingDoc));` |
|        25 | 1950 | `	if( zDup ){` |
|        25 | 1951 | `		SyStringInitFromBuf(pOut,zDup,SyStringLength(&pGen->sPendingDoc));` |
|        10 | 1952 | `	}` |
|        25 | 1953 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|   2432946 | 1954 | `}` |
|         - | 1955 | `/*` |
|         - | 1956 | ` * Compile one recorded #[...] attribute group (the span between the group` |
|         - | 1957 | ` * delimiters) into ph7_attribute records appended to pOut. The span is` |
|         - | 1958 | ` * duplicated into the VM allocator FIRST (compiled bytecode and interned` |
|         - | 1959 | ` * names may point into the token text, which must outlive the raw script` |
|         - | 1960 | ` * buffer), then re-tokenized on its own. Each argument expression compiles` |
|         - | 1961 | ` * with the container-swap idiom into its own OP_DONE-terminated set,` |
|         - | 1962 | ` * evaluated lazily at ReflectionAttribute time (PHP semantics).` |
|         - | 1963 | ` */` |
|      7886 | 1964 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut)` |
|         5 | 1965 | `{` |
|         - | 1966 | `	SySet *pToken;` |
|         - | 1967 | `	SyToken *pIn, *pEnd, *pSavedIn, *pSavedEnd;` |
|         - | 1968 | `	char *zSpan;` |
|      7891 | 1969 | `	sxi32 rc = SXRET_OK;` |
|      7891 | 1970 | `	if( SyStringLength(&pTrivia->sText) < 1 ){` |
|       ! 0 | 1971 | `		return SXRET_OK;` |
|         - | 1972 | `	}` |
|     11834 | 1973 | `	zSpan = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      3943 | 1974 | `		SyStringData(&pTrivia->sText),SyStringLength(&pTrivia->sText));` |
|      7891 | 1975 | `	if( zSpan == 0 ){` |
|       ! 0 | 1976 | `		return SXRET_OK;` |
|         - | 1977 | `	}` |
|         - | 1978 | `	/* The token set must outlive compilation too: interned operands may` |
|         - | 1979 | `	 * reference token payloads. Pool-allocated, never released — bounded by` |
|         - | 1980 | `	 * the number of attribute declarations in the program. */` |
|      7891 | 1981 | `	pToken = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      7891 | 1982 | `	if( pToken == 0 ){` |
|       ! 0 | 1983 | `		return SXRET_OK;` |
|         - | 1984 | `	}` |
|      7891 | 1985 | `	SySetInit(pToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      7891 | 1986 | `	PH7_TokenizePHP(zSpan,SyStringLength(&pTrivia->sText),pTrivia->nLine,pToken,0);` |
|      7891 | 1987 | `	pIn = (SyToken *)SySetBasePtr(pToken);` |
|      7891 | 1988 | `	pEnd = &pIn[SySetUsed(pToken)];` |
|      7891 | 1989 | `	pSavedIn = pGen->pIn;` |
|      7891 | 1990 | `	pSavedEnd = pGen->pEnd;` |
|      7895 | 1991 | `	while( pIn < pEnd ){` |
|         - | 1992 | `		ph7_attribute sAttr;` |
|         - | 1993 | `		SyBlob sFQN;` |
|      7895 | 1994 | `		int bAbsolute = 0;` |
|      7895 | 1995 | `		SyZero(&sAttr,sizeof(sAttr));` |
|      7895 | 1996 | `		SySetInit(&sAttr.aArgs,&pGen->pVm->sAllocator,sizeof(ph7_attr_arg));` |
|      7895 | 1997 | `		sAttr.nLine = pIn->nLine;` |
|      7895 | 1998 | `		if( pIn->nType & PH7_TK_NSSEP ){` |
|        75 | 1999 | `			bAbsolute = 1;` |
|        75 | 2000 | `			pIn++;` |
|        35 | 2001 | `		}` |
|      7895 | 2002 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      7895 | 2003 | `		while( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      7895 | 2004 | `			SyBlobAppend(&sFQN,pIn->sData.zString,pIn->sData.nByte);` |
|      7895 | 2005 | `			pIn++;` |
|      7895 | 2006 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       ! 0 | 2007 | `				SyBlobAppend(&sFQN,"\\",1);` |
|       ! 0 | 2008 | `				pIn++;` |
|       ! 0 | 2009 | `				continue;` |
|         - | 2010 | `			}` |
|      7895 | 2011 | `			break;` |
|       ! 0 | 2012 | `		}` |
|      7895 | 2013 | `		if( SyBlobLength(&sFQN) < 1 ){` |
|         - | 2014 | `			/* Malformed group: stop quietly (the group was inert trivia before` |
|         - | 2015 | `			 * this feature; never turn it into a new fatal) */` |
|       ! 0 | 2016 | `			SyBlobRelease(&sFQN);` |
|       ! 0 | 2017 | `			break;` |
|         - | 2018 | `		}` |
|         - | 2019 | `		/* Resolve to an FQN: absolute names verbatim; else use-import alias,` |
|         - | 2020 | `		 * else current-namespace prefix (PHP attribute name resolution) */` |
|         - | 2021 | `		{` |
|      7895 | 2022 | `			const char *zName = (const char *)SyBlobData(&sFQN);` |
|      7895 | 2023 | `			sxu32 nName = SyBlobLength(&sFQN);` |
|      7895 | 2024 | `			char *zDup = 0;` |
|      7895 | 2025 | `			if( !bAbsolute ){` |
|      7825 | 2026 | `				SyHashEntry *pImp = SyHashGet(&pGen->hUseImports,(const void *)zName,nName);` |
|      7825 | 2027 | `				if( pImp ){` |
|       ! 0 | 2028 | `					const char *zFqn = (const char *)pImp->pUserData;` |
|       ! 0 | 2029 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFqn,SyStrlen(zFqn));` |
|       ! 0 | 2030 | `					if( zDup ){` |
|       ! 0 | 2031 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyStrlen(zDup));` |
|       ! 0 | 2032 | `					}` |
|      7825 | 2033 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         - | 2034 | `					SyBlob sTmp;` |
|       ! 0 | 2035 | `					SyBlobInit(&sTmp,&pGen->pVm->sAllocator);` |
|       ! 0 | 2036 | `					SyBlobAppend(&sTmp,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       ! 0 | 2037 | `					SyBlobAppend(&sTmp,"\\",1);` |
|       ! 0 | 2038 | `					SyBlobAppend(&sTmp,zName,nName);` |
|       ! 0 | 2039 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       ! 0 | 2040 | `						(const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|       ! 0 | 2041 | `					if( zDup ){` |
|       ! 0 | 2042 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyBlobLength(&sTmp));` |
|       ! 0 | 2043 | `					}` |
|       ! 0 | 2044 | `					SyBlobRelease(&sTmp);` |
|       ! 0 | 2045 | `				}` |
|      3910 | 2046 | `			}` |
|      7895 | 2047 | `			if( SyStringLength(&sAttr.sName) < 1 ){` |
|      7895 | 2048 | `				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|      7895 | 2049 | `				if( zDup ){` |
|      7895 | 2050 | `					SyStringInitFromBuf(&sAttr.sName,zDup,nName);` |
|      3945 | 2051 | `				}` |
|      3945 | 2052 | `			}` |
|         - | 2053 | `		}` |
|      7895 | 2054 | `		SyBlobRelease(&sFQN);` |
|      7895 | 2055 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 2056 | `			SyToken *pArgsEnd;` |
|      7793 | 2057 | `			pIn++;` |
|      7793 | 2058 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pArgsEnd);` |
|     15595 | 2059 | `			while( pIn < pArgsEnd ){` |
|      7807 | 2060 | `				SyToken *pArgStart = pIn, *pArgStop = pIn;` |
|      7807 | 2061 | `				sxi32 iDepth = 0;` |
|         - | 2062 | `				ph7_attr_arg sArgRec;` |
|     77517 | 2063 | `				while( pArgStop < pArgsEnd ){` |
|     69731 | 2064 | `					if( pArgStop->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        11 | 2065 | `						iDepth++;` |
|     69726 | 2066 | `					}else if( pArgStop->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        11 | 2067 | `						iDepth--;` |
|     69716 | 2068 | `					}else if( (pArgStop->nType & PH7_TK_COMMA) && iDepth == 0 ){` |
|        17 | 2069 | `						break;` |
|         - | 2070 | `					}` |
|     69715 | 2071 | `					pArgStop++;` |
|         5 | 2072 | `				}` |
|      7807 | 2073 | `				SyZero(&sArgRec,sizeof(sArgRec));` |
|      7807 | 2074 | `				SySetInit(&sArgRec.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      7802 | 2075 | `				if( pArgStart < pArgStop && (pArgStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|      7784 | 2076 | `				 && &pArgStart[1] < pArgStop && (pArgStart[1].nType & PH7_TK_COLON) ){` |
|        28 | 2077 | `					char *zN = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|         9 | 2078 | `						pArgStart->sData.zString,pArgStart->sData.nByte);` |
|        19 | 2079 | `					if( zN ){` |
|        19 | 2080 | `						SyStringInitFromBuf(&sArgRec.sName,zN,pArgStart->sData.nByte);` |
|         9 | 2081 | `					}` |
|        19 | 2082 | `					pArgStart += 2;` |
|         9 | 2083 | `				}` |
|      7807 | 2084 | `				if( pArgStart < pArgStop ){` |
|         - | 2085 | `					SySet *pInstrContainer;` |
|      7807 | 2086 | `					pGen->pIn = pArgStart;` |
|      7807 | 2087 | `					pGen->pEnd = pArgStop;` |
|      7807 | 2088 | `					pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      7807 | 2089 | `					PH7_VmSetByteCodeContainer(pGen->pVm,&sArgRec.aByteCode);` |
|      7807 | 2090 | `					rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      7807 | 2091 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      7807 | 2092 | `					PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      7807 | 2093 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 2094 | `						pGen->pIn = pSavedIn;` |
|       ! 0 | 2095 | `						pGen->pEnd = pSavedEnd;` |
|       ! 0 | 2096 | `						return SXERR_ABORT;` |
|         - | 2097 | `					}` |
|      7807 | 2098 | `					SySetPut(&sAttr.aArgs,(const void *)&sArgRec);` |
|      3901 | 2099 | `				}` |
|      7807 | 2100 | `				pIn = pArgStop;` |
|      7807 | 2101 | `				if( pIn < pArgsEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        17 | 2102 | `					pIn++;` |
|         8 | 2103 | `				}` |
|         5 | 2104 | `			}` |
|      7793 | 2105 | `			pIn = (pArgsEnd < pEnd) ? &pArgsEnd[1] : pEnd;` |
|      3894 | 2106 | `		}` |
|      7895 | 2107 | `		SySetPut(pOut,(const void *)&sAttr);` |
|      7895 | 2108 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|         5 | 2109 | `			pIn++;` |
|         5 | 2110 | `			continue;` |
|         - | 2111 | `		}` |
|      7891 | 2112 | `		break;` |
|       ! 0 | 2113 | `	}` |
|      7891 | 2114 | `	pGen->pIn = pSavedIn;` |
|      7891 | 2115 | `	pGen->pEnd = pSavedEnd;` |
|      7891 | 2116 | `	return SXRET_OK;` |
|      3948 | 2117 | `}` |
|         - | 2118 | `/*` |
|         - | 2119 | ` * Hand the pending attribute groups (if any) to a declaration: compile` |
|         - | 2120 | ` * every recorded group into pOut and clear the pending list.` |
|         - | 2121 | ` */` |
|   4865886 | 2122 | `PH7_PRIVATE sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut)` |
|         5 | 2123 | `{` |
|   4865891 | 2124 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|         - | 2125 | `	sxu32 n;` |
|         - | 2126 | `	sxi32 rc;` |
|   4873763 | 2127 | `	for( n = 0 ; n < SySetUsed(&pGen->aPendingAttrs) ; n++ ){` |
|      7877 | 2128 | `		rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|      7877 | 2129 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 2130 | `			return SXERR_ABORT;` |
|         - | 2131 | `		}` |
|      3941 | 2132 | `	}` |
|   4865891 | 2133 | `	SySetReset(&pGen->aPendingAttrs);` |
|   4865891 | 2134 | `	return SXRET_OK;` |
|   2432948 | 2135 | `}` |
|         - | 2136 | `/*` |
|         - | 2137 | ` * Compile the attribute groups keyed to the given token (a parameter's` |
|         - | 2138 | ` * first token inside a signature) into pOut. Parameters are parsed from` |
|         - | 2139 | ` * the main token stream, so the sidecar indexes map directly.` |
|         - | 2140 | ` */` |
|   2444644 | 2141 | `PH7_PRIVATE sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut)` |
|         5 | 2142 | `{` |
|   2444649 | 2143 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|   2444649 | 2144 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|   2444649 | 2145 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 2146 | `	sxu32 nIdx, n;` |
|         - | 2147 | `	sxi32 rc;` |
|   2444644 | 2148 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|    552575 | 2149 | `	 \|\| pTok < pBase \|\| pTok >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|   1892079 | 2150 | `		return SXRET_OK;` |
|         - | 2151 | `	}` |
|    552575 | 2152 | `	nIdx = (sxu32)(pTok - pBase);` |
|   1657759 | 2153 | `	for( n = 0 ; n < nT ; n++ ){` |
|   1105189 | 2154 | `		if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|        16 | 2155 | `			rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|        16 | 2156 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 2157 | `				return SXERR_ABORT;` |
|         - | 2158 | `			}` |
|         7 | 2159 | `		}` |
|    552597 | 2160 | `	}` |
|    552575 | 2161 | `	return SXRET_OK;` |
|   1222327 | 2162 | `}` |
|  13247096 | 2163 | `PH7_PRIVATE sxi32 GenStateCompileChunk(` |
|         - | 2164 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 2165 | `	sxi32 iFlags         /* Compile flags */` |
|         - | 2166 | `	)` |
|         5 | 2167 | `{` |
|         - | 2168 | `	ProcLangConstruct xCons;` |
|         - | 2169 | `	sxi32 rc;` |
|  13247101 | 2170 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|   7707792 | 2171 | `	for(;;){` |
|  14331345 | 2172 | `		int bStmtIsDeclare = 0;` |
|  14331345 | 2173 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 2174 | `			/* No more input to process */` |
|     90847 | 2175 | `			break;` |
|         - | 2176 | `		}` |
|         - | 2177 | `		/* Bind a directly-preceding docblock to this statement */` |
|  14240503 | 2178 | `		GenStateSetPendingDoc(&(*pGen));` |
|  14240503 | 2179 | `		if( SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|         - | 2180 | `			/* php: a statement-position attribute group must be followed by a` |
|         - | 2181 | ``			 * declaration (function/class-like/const) — `#[A] $x = 1;` is a`` |
|         - | 2182 | `` 			 * parse error, never a silent discard. `static`/`fn`/`function` `` |
|         - | 2183 | ``			 * cover bare closure-expression statements; `readonly`/`enum` are`` |
|         - | 2184 | `			 * context-sensitive IDs handled by the modified-class/enum scans. */` |
|      7789 | 2185 | `			int bAttrTarget = 0;` |
|      7784 | 2186 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd)` |
|      3927 | 2187 | `			 \|\| GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|      7729 | 2188 | `				bAttrTarget = 1;` |
|      3923 | 2189 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|        61 | 2190 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        60 | 2191 | `				if( nKw == PH7_TKWRD_FUNCTION \|\| nKw == PH7_TKWRD_CLASS` |
|        16 | 2192 | `				 \|\| nKw == PH7_TKWRD_INTERFACE \|\| nKw == PH7_TKWRD_TRAIT` |
|         4 | 2193 | `				 \|\| nKw == PH7_TKWRD_ABSTRACT \|\| nKw == PH7_TKWRD_FINAL` |
|         4 | 2194 | `				 \|\| nKw == PH7_TKWRD_CONST \|\| nKw == PH7_TKWRD_STATIC` |
|         1 | 2195 | `				 \|\| nKw == PH7_TKWRD_FN ){` |
|        61 | 2196 | `					bAttrTarget = 1;` |
|        30 | 2197 | `				}` |
|        30 | 2198 | `			}` |
|      7789 | 2199 | `			if( !bAttrTarget ){` |
|       ! 0 | 2200 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 2201 | `					"syntax error, unexpected token \"%z\" after attribute group; expecting a declaration",` |
|       ! 0 | 2202 | `					&pGen->pIn->sData);` |
|       ! 0 | 2203 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 2204 | `					break;` |
|         - | 2205 | `				}` |
|       ! 0 | 2206 | `				SySetReset(&pGen->aPendingAttrs);` |
|       ! 0 | 2207 | `			}` |
|      3892 | 2208 | `		}` |
|         - | 2209 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|         - | 2210 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  14240503 | 2211 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   8463285 | 2212 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   8463285 | 2213 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|        49 | 2214 | `				bStmtIsDeclare = 1;` |
|        22 | 2215 | `			}` |
|   4231640 | 2216 | `		}` |
|  14240503 | 2217 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|         - | 2218 | `			/* Any non-declare top-level statement locks the strict_types` |
|         - | 2219 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|   1084243 | 2220 | `			pGen->bStrictTypesLocked = 1;` |
|    542119 | 2221 | `		}` |
|  14240503 | 2222 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 2223 | `			/* Compile block */` |
|      3907 | 2224 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      3907 | 2225 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 2226 | `				break;` |
|         - | 2227 | `			}` |
|      1956 | 2228 | `		}else{` |
|  14236601 | 2229 | `			xCons = 0;` |
|  14236601 | 2230 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|         - | 2231 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|         - | 2232 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|         - | 2233 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|     42583 | 2234 | `				xCons = PH7_CompileClassModifiers;` |
|  14215312 | 2235 | `			}else if( GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|         - | 2236 | ``				/* `enum Name …` (PHP 8.1) — `enum` is a context-sensitive ID,`` |
|         - | 2237 | `				 * so it is detected here rather than the keyword dispatcher. */` |
|      3903 | 2238 | `				xCons = PH7_CompileEnum;` |
|  14192074 | 2239 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   8420733 | 2240 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         - | 2241 | `				/* Try to extract a language construct handler */` |
|   8420733 | 2242 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|   8420733 | 2243 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|        13 | 2244 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 2245 | `						"Syntax error: Unexpected keyword '%z'",` |
|         8 | 2246 | `						&pGen->pIn->sData);` |
|         9 | 2247 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 2248 | `						break;` |
|         - | 2249 | `					}` |
|         - | 2250 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|         - | 2251 | `					 * this erroneous statement.` |
|         - | 2252 | `					 */` |
|         9 | 2253 | `					xCons = PH7_ErrorRecover;` |
|         4 | 2254 | `				}` |
|   9979761 | 2255 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|    426363 | 2256 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|         - | 2257 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|       117 | 2258 | `				xCons = PH7_CompileLabel;` |
|        56 | 2259 | `			}` |
|  14236601 | 2260 | `			if( xCons == 0 ){` |
|         - | 2261 | `				/* Assume an expression an try to compile it */` |
|   5827701 | 2262 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   5827701 | 2263 | `				if(  rc != SXERR_EMPTY ){` |
|         - | 2264 | `					/* Pop l-value */` |
|   5827547 | 2265 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|   2913771 | 2266 | `				}` |
|   2913853 | 2267 | `			}else{` |
|         - | 2268 | `				/* Go compile the sucker */` |
|   8408905 | 2269 | `				rc = xCons(&(*pGen));` |
|         - | 2270 | `			}` |
|  14236601 | 2271 | `			if( rc == SXERR_ABORT ){` |
|         - | 2272 | `				/* Request to abort compilation */` |
|        42 | 2273 | `				break;` |
|         - | 2274 | `			}` |
|         - | 2275 | `		}` |
|         - | 2276 | `		/* Ignore trailing semi-colons ';' */` |
|  24448411 | 2277 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  10207951 | 2278 | `			pGen->pIn++;` |
|         5 | 2279 | `		}` |
|  14240465 | 2280 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|         - | 2281 | `			/* Compile a single statement and return */` |
|  13156221 | 2282 | `			break;` |
|         - | 2283 | `		}` |
|         - | 2284 | `		/* LOOP ONE */` |
|         - | 2285 | `		/* LOOP TWO */` |
|         - | 2286 | `		/* LOOP THREE */` |
|         - | 2287 | `		/* LOOP FOUR */` |
|         5 | 2288 | `	}` |
|         - | 2289 | `	/* Return compilation status */` |
|  13247101 | 2290 | `	return rc;` |
|         5 | 2291 | `}` |
|         - | 2292 | `/*` |
|         - | 2293 | ` * Compile a Raw PHP chunk.` |
|         - | 2294 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 2295 | ` * takes care of generating the appropriate error message.` |
|         - | 2296 | ` */` |
|     90882 | 2297 | `static sxi32 PH7_CompilePHP(` |
|         - | 2298 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 2299 | `	SySet *pTokenSet,     /* Token set */` |
|         - | 2300 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|         - | 2301 | `	)` |
|         5 | 2302 | `{` |
|     90887 | 2303 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|         - | 2304 | `	sxi32 rc;` |
|         - | 2305 | `	/* Reset the token set (and its trivia sidecar) */` |
|     90887 | 2306 | `	SySetReset(&(*pTokenSet));` |
|     90887 | 2307 | `	SySetReset(&pGen->aTrivia);` |
|         - | 2308 | `	/* Mark as the default token set */` |
|     90887 | 2309 | `	pGen->pTokenSet = &(*pTokenSet);` |
|         - | 2310 | `	/* Advance the stream cursor */` |
|     90887 | 2311 | `	pGen->pRawIn++;` |
|         - | 2312 | `	/* Tokenize the PHP chunk first */` |
|     90887 | 2313 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet),&pGen->aTrivia);` |
|         - | 2314 | `	/* Point to the head and tail of the token stream. */` |
|     90887 | 2315 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     90887 | 2316 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|     90887 | 2317 | `	if( is_expr ){` |
|       ! 0 | 2318 | `		rc = SXERR_EMPTY;` |
|       ! 0 | 2319 | `		if( pGen->pIn < pGen->pEnd ){` |
|         - | 2320 | `			/* A simple expression,compile it */` |
|       ! 0 | 2321 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|       ! 0 | 2322 | `		}` |
|         - | 2323 | `		/* Emit the DONE instruction */` |
|       ! 0 | 2324 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       ! 0 | 2325 | `		return SXRET_OK;` |
|         - | 2326 | `	}` |
|     90887 | 2327 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|         - | 2328 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|         - | 2329 | `		/*` |
|         - | 2330 | `		 * Shortcut syntax for the 'echo' language construct.` |
|         - | 2331 | `		 * According to the PHP reference manual:` |
|         - | 2332 | `		 *  echo() also has a shortcut syntax, where you can` |
|         - | 2333 | `		 *  immediately follow` |
|         - | 2334 | `		 *  the opening tag with an equals sign as follows:` |
|         - | 2335 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|         - | 2336 | `		 * Symisc extension:` |
|         - | 2337 | `		 *   This short syntax works with all PHP opening` |
|         - | 2338 | `		 *   tags unlike the default PHP engine that handle` |
|         - | 2339 | `		 *   only short tag.` |
|         - | 2340 | `		 */` |
|         - | 2341 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|         3 | 2342 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|         3 | 2343 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|         3 | 2344 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|         - | 2345 | `		/* This synthesized echo is compiled as an EXPRESSION, which is otherwise a` |
|         - | 2346 | `		 * parse error; allow it for the duration of this one compile. */` |
|         3 | 2347 | `		pGen->nExprEchoOk++;` |
|         3 | 2348 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|         3 | 2349 | `		pGen->nExprEchoOk--;` |
|         3 | 2350 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 2351 | `			return SXERR_ABORT;` |
|         - | 2352 | `		}` |
|         3 | 2353 | `		if( rc != SXERR_EMPTY ){` |
|         3 | 2354 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         1 | 2355 | `		}` |
|         3 | 2356 | `		return SXRET_OK;` |
|         - | 2357 | `	}` |
|         - | 2358 | `	/* Compile the PHP chunk */` |
|     90885 | 2359 | `	rc = GenStateCompileChunk(pGen,0);` |
|         - | 2360 | `	/* Fix exceptions jumps */` |
|     90885 | 2361 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - | 2362 | `	/* Fix gotos now, the jump destination is resolved */` |
|     90885 | 2363 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|         3 | 2364 | `		rc = SXERR_ABORT;` |
|         1 | 2365 | `	}` |
|         - | 2366 | `	/* Reset container */` |
|     90885 | 2367 | `	SySetReset(&pGen->aGoto);` |
|     90885 | 2368 | `	SySetReset(&pGen->aLabel);` |
|     90885 | 2369 | `	SySetReset(&pGen->aNullsafeJmp);` |
|         - | 2370 | `	/* Compilation result */` |
|     90885 | 2371 | `	return rc;` |
|     45446 | 2372 | `}` |
|         - | 2373 | `/*` |
|         - | 2374 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|         - | 2375 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|         - | 2376 | ` * This is the only compile interface exported from this file.` |
|         - | 2377 | ` */` |
|     94050 | 2378 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|         - | 2379 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|         - | 2380 | `	SyString *pScript,  /* Script to compile */` |
|         - | 2381 | `	sxi32 iFlags        /* Compile flags */` |
|         - | 2382 | `	)` |
|         5 | 2383 | `{` |
|         - | 2384 | `	SySet aPhpToken,aRawToken;` |
|         - | 2385 | `	ph7_gen_state *pCodeGen;` |
|         - | 2386 | `	ph7_value *pRawObj;` |
|         - | 2387 | `	sxu32 nObjIdx;` |
|         - | 2388 | `	sxi32 nRawObj;` |
|         - | 2389 | `	int is_expr;` |
|         - | 2390 | `	sxi8 bSavedStrict;` |
|         - | 2391 | `	sxi8 bSavedStrictLocked;` |
|         - | 2392 | `	SyToken *pSavedIn,*pSavedEnd;` |
|         - | 2393 | `	sxi32 rc;` |
|     94055 | 2394 | `	sxu32 nBaseLine = 1;` |
|     94055 | 2395 | `	if( pScript->nByte < 1 ){` |
|         - | 2396 | `		/* Nothing to compile */` |
|       ! 0 | 2397 | `		return PH7_OK;` |
|         - | 2398 | `	}` |
|         - | 2399 | `	/* php skips a "#!" shebang on the first line of a CLI script: consume it` |
|         - | 2400 | `	 * (including its newline) so it is not echoed as inline text, and bump the` |
|         - | 2401 | `	 * base line to 2 so the code below still reports php-matching line numbers. */` |
|     94055 | 2402 | `	if( pScript->nByte >= 2 && pScript->zString[0]=='#' && pScript->zString[1]=='!' ){` |
|         3 | 2403 | `		const char *z = pScript->zString;` |
|         3 | 2404 | `		const char *zEnd = &z[pScript->nByte];` |
|        39 | 2405 | `		while( z < zEnd && z[0] != '\n' ){ z++; }` |
|         3 | 2406 | `		if( z < zEnd ){ z++; } /* consume the newline too */` |
|         3 | 2407 | `		pScript->nByte -= (sxu32)(z - pScript->zString);` |
|         3 | 2408 | `		pScript->zString = z;` |
|         3 | 2409 | `		nBaseLine = 2;` |
|         3 | 2410 | `		if( pScript->nByte < 1 ){` |
|       ! 0 | 2411 | `			return PH7_OK;` |
|         - | 2412 | `		}` |
|         1 | 2413 | `	}` |
|         - | 2414 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|         - | 2415 | `	 * file's flags so include/require restore them on return. */` |
|     94055 | 2416 | `	pCodeGen = &pVm->sCodeGen;` |
|         - | 2417 | ``	/* aPhpToken below is a LOCAL token set: once it is released at `cleanup`, any`` |
|         - | 2418 | `	 * pGen->pIn/pEnd still pointing into it DANGLE. PH7_VmEmitInstr reads pIn to stamp` |
|         - | 2419 | `	 * each instruction's source line, and instructions are still emitted after this` |
|         - | 2420 | `	 * function returns (VmEvalChunk's trailing OP_DONE) -- a use-after-free ASan caught` |
|         - | 2421 | `	 * immediately. Save the caller's cursor and restore it on the way out, so an` |
|         - | 2422 | `	 * enclosing compile keeps its (live) tokens and the top level goes back to NULL. */` |
|     94055 | 2423 | `	pSavedIn = pCodeGen->pIn;` |
|     94055 | 2424 | `	pSavedEnd = pCodeGen->pEnd;` |
|     94055 | 2425 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|     94055 | 2426 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|     94055 | 2427 | `	pCodeGen->bStrictTypes = 0;` |
|     94055 | 2428 | `	pCodeGen->bStrictTypesLocked = 0;` |
|         - | 2429 | `	/* Initialize the tokens containers */` |
|     94055 | 2430 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|     94055 | 2431 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|     94055 | 2432 | `	SySetAlloc(&aPhpToken,0xc0);` |
|     94055 | 2433 | `	is_expr = 0;` |
|     94055 | 2434 | `	if( iFlags & PH7_PHP_ONLY ){` |
|         - | 2435 | `		SyToken sTmp;` |
|         - | 2436 | `		/* PHP only: -*/` |
|     81233 | 2437 | `		sTmp.nLine = 1;` |
|     81233 | 2438 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|     81233 | 2439 | `		sTmp.pUserData = 0;` |
|     81233 | 2440 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|     81233 | 2441 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|     81233 | 2442 | `		if( iFlags & PH7_PHP_EXPR ){` |
|         - | 2443 | `			/* A simple PHP expression */` |
|       ! 0 | 2444 | `			is_expr = 1;` |
|       ! 0 | 2445 | `		}` |
|     40619 | 2446 | `	}else{` |
|         - | 2447 | `		/* Tokenize raw text */` |
|     12827 | 2448 | `		SySetAlloc(&aRawToken,32);` |
|     12827 | 2449 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken,nBaseLine);` |
|         - | 2450 | `	}` |
|         - | 2451 | `	/* Process high-level tokens */` |
|     94055 | 2452 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|     94055 | 2453 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|     94055 | 2454 | `	rc = PH7_OK;` |
|     94055 | 2455 | `	if( is_expr ){` |
|         - | 2456 | `		/* Compile the expression */` |
|       ! 0 | 2457 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|       ! 0 | 2458 | `		goto cleanup;` |
|         - | 2459 | `	}` |
|     94055 | 2460 | `	nObjIdx = 0;` |
|         - | 2461 | `	/* Each compilation unit starts in the global namespace.` |
|         - | 2462 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|         - | 2463 | `	 * preventing namespace bleeding across include()d files. */` |
|     94055 | 2464 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|         - | 2465 | `	/* Start the compilation process */` |
|     53440 | 2466 | `	for(;;){` |
|    197727 | 2467 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|     94015 | 2468 | `			break; /* No more tokens to process */` |
|         - | 2469 | `		}` |
|    103717 | 2470 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|         - | 2471 | `			/* Compile the PHP chunk */` |
|     90887 | 2472 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|     90887 | 2473 | `			if( rc == SXERR_ABORT ){` |
|        44 | 2474 | `				break;` |
|         - | 2475 | `			}` |
|     90847 | 2476 | `			continue;` |
|         - | 2477 | `		}` |
|         - | 2478 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|     12835 | 2479 | `		nRawObj = 0;` |
|     25665 | 2480 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|         - | 2481 | `			/* Consume the raw chunk without any processing */` |
|     12835 | 2482 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|     12835 | 2483 | `			if( pRawObj == 0 ){` |
|       ! 0 | 2484 | `				rc = SXERR_MEM;` |
|       ! 0 | 2485 | `				break;` |
|         - | 2486 | `			}` |
|         - | 2487 | `			/* Mark as constant and emit the load constant instruction */` |
|     12835 | 2488 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|     12835 | 2489 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     12835 | 2490 | `			++nRawObj;` |
|     12835 | 2491 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|         5 | 2492 | `		}` |
|     12835 | 2493 | `		if( nRawObj > 0 ){` |
|         - | 2494 | `			/* Emit the consume instruction */` |
|     12835 | 2495 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|      6415 | 2496 | `		}` |
|     47030 | 2497 | `	}` |
|     47025 | 2498 | `cleanup:` |
|         - | 2499 | `	/* Drop the cursor into the token set BEFORE the set is freed (see above). */` |
|     94055 | 2500 | `	pCodeGen->pIn = pSavedIn;` |
|     94055 | 2501 | `	pCodeGen->pEnd = pSavedEnd;` |
|     94055 | 2502 | `	SySetRelease(&aRawToken);` |
|     94055 | 2503 | `	SySetRelease(&aPhpToken);` |
|         - | 2504 | `	/* Restore outer file's strict_types scope */` |
|     94055 | 2505 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|     94055 | 2506 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|     94055 | 2507 | `	return rc;` |
|     47030 | 2508 | `}` |
|         - | 2509 | `/*` |
|         - | 2510 | ` * Utility routines.Initialize the code generator.` |
|         - | 2511 | ` */` |
|      3862 | 2512 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|         - | 2513 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 2514 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 2515 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 2516 | `	)` |
|         5 | 2517 | `{` |
|      3867 | 2518 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 2519 | `	/* Zero the structure */` |
|      3867 | 2520 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|         - | 2521 | `	/* Initial state */` |
|      3867 | 2522 | `	pGen->pVm  = &(*pVm);` |
|      3867 | 2523 | `	pGen->xErr = xErr;` |
|      3867 | 2524 | `	pGen->pErrData = pErrData;` |
|      3867 | 2525 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|      3867 | 2526 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|      3867 | 2527 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|      3867 | 2528 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|      3867 | 2529 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|      3867 | 2530 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3867 | 2531 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3867 | 2532 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|      3867 | 2533 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|         - | 2534 | `	/* Error log buffer */` |
|      3867 | 2535 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|         - | 2536 | `	/* General purpose working buffer */` |
|      3867 | 2537 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|         - | 2538 | `	/* Namespace state */` |
|      3867 | 2539 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|      3867 | 2540 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|      3867 | 2541 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|      3867 | 2542 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 2543 | `	/* Create the global scope */` |
|      3867 | 2544 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|         - | 2545 | `	/* Point to the global scope */` |
|      3867 | 2546 | `	pGen->pCurrent = &pGen->sGlobal;` |
|      3867 | 2547 | `	return SXRET_OK;` |
|         5 | 2548 | `}` |
|         - | 2549 | `/*` |
|         - | 2550 | ` * Utility routines. Reset the code generator to it's initial state.` |
|         - | 2551 | ` */` |
|     97428 | 2552 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|         - | 2553 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 2554 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 2555 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 2556 | `	)` |
|         5 | 2557 | `{` |
|     97433 | 2558 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 2559 | `	GenBlock *pBlock,*pParent;` |
|         - | 2560 | `	/* Reset state */` |
|     97433 | 2561 | `	SySetReset(&pGen->aLabel);` |
|     97433 | 2562 | `	SySetReset(&pGen->aGoto);` |
|     97433 | 2563 | `	SySetReset(&pGen->aNullsafeJmp);` |
|     97433 | 2564 | `	SySetReset(&pGen->aTrivia);` |
|     97433 | 2565 | `	SySetReset(&pGen->aPendingAttrs);` |
|     97433 | 2566 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|     97433 | 2567 | `	SyBlobRelease(&pGen->sErrBuf);` |
|     97433 | 2568 | `	SyBlobRelease(&pGen->sWorker);` |
|     97433 | 2569 | `	SyBlobRelease(&pGen->sNamespace);` |
|     97433 | 2570 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|     97433 | 2571 | `	SyHashRelease(&pGen->hUseImports);` |
|     97433 | 2572 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|     97433 | 2573 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     97433 | 2574 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|     97433 | 2575 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     97433 | 2576 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 2577 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|         - | 2578 | `	 * They intern variable names and literal strings that are referenced by` |
|         - | 2579 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|         - | 2580 | `	 * Releasing them would either leak the interned strings or require freeing` |
|         - | 2581 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|         - | 2582 | `	 * number of unique names, which is acceptable. */` |
|         - | 2583 | `	/* Point to the global scope */` |
|     97433 | 2584 | `	pBlock = pGen->pCurrent;` |
|     97433 | 2585 | `	while( pBlock->pParent != 0 ){` |
|       ! 0 | 2586 | `		pParent = pBlock->pParent;` |
|       ! 0 | 2587 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 2588 | `		pBlock = pParent;` |
|       ! 0 | 2589 | `	}` |
|     97433 | 2590 | `	pGen->xErr = xErr;` |
|     97433 | 2591 | `	pGen->pErrData = pErrData;` |
|     97433 | 2592 | `	pGen->pCurrent = &pGen->sGlobal;` |
|     97433 | 2593 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|     97433 | 2594 | `	pGen->pIn = pGen->pEnd = 0;` |
|     97433 | 2595 | `	pGen->nErr = 0;` |
|     97433 | 2596 | `	return SXRET_OK;` |
|         5 | 2597 | `}` |
|         - | 2598 | `/*` |
|         - | 2599 | ` * Save the code generator's compile-position state and hand the live generator a` |
|         - | 2600 | ` * fresh, empty one for a NESTED compilation unit.` |
|         - | 2601 | ` *` |
|         - | 2602 | ` * A require/include normally runs at execution time, when no compile is in flight,` |
|         - | 2603 | ` * so VmEvalChunk can call PH7_ResetCodeGenerator to wipe the generator. But an` |
|         - | 2604 | `` * autoload can fire in the MIDDLE of compiling a class: resolving `class Child`` |
|         - | 2605 | `` * extends Base` calls PH7_VmExtractClass, which triggers the autoloader, whose`` |
|         - | 2606 | `` * body `require`s Base's file — a nested compile while the outer one is mid-token.`` |
|         - | 2607 | ` * PH7_ResetCodeGenerator would then blow away the outer compile's cursor` |
|         - | 2608 | ` * (pIn/pEnd), current block, use-imports, labels, etc.; the outer parse resumes` |
|         - | 2609 | ` * with pIn == NULL and reports a bogus "Expected '{' after class 'Child'". (Common` |
|         - | 2610 | ` * in every real framework: Composer autoloads parent classes on demand.)` |
|         - | 2611 | ` *` |
|         - | 2612 | ` * This snapshots the position/scope fields into *pSaved (a caller-stack` |
|         - | 2613 | ` * ph7_gen_state used purely as storage) and re-initializes the live generator's` |
|         - | 2614 | ` * position containers to FRESH, empty ones WITHOUT releasing the outer's (the` |
|         - | 2615 | ` * snapshot now owns those). hLiteral/hNumLiteral/hVar are intentionally left LIVE` |
|         - | 2616 | ` * and shared — compiled bytecode interns names/literals into them and they grow` |
|         - | 2617 | ` * monotonically (exactly why PH7_ResetCodeGenerator keeps them); restoring an old` |
|         - | 2618 | ` * copy of those headers after a nested grow would use-after-free the bucket array.` |
|         - | 2619 | ` */` |
|         4 | 2620 | `PH7_PRIVATE void PH7_CompilerSaveState(ph7_vm *pVm,ph7_gen_state *pSaved,ProcConsumer xErr,void *pErrData)` |
|         1 | 2621 | `{` |
|         5 | 2622 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 2623 | `	/* Shallow-copy every field; the position containers below are then replaced` |
|         - | 2624 | `	 * with fresh ones on the live struct, so *pSaved keeps the outer's memory. */` |
|         5 | 2625 | `	*pSaved = *pGen;` |
|         5 | 2626 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|         5 | 2627 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|         5 | 2628 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|         5 | 2629 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|         5 | 2630 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|         5 | 2631 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|         5 | 2632 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|         5 | 2633 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|         5 | 2634 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|         5 | 2635 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|         5 | 2636 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|         5 | 2637 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 2638 | `	/* Fresh global scope for the nested unit (address of the embedded sGlobal is` |
|         - | 2639 | `	 * stable, so any outer block still parented to it stays valid across restore). */` |
|         5 | 2640 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(pVm),0);` |
|         5 | 2641 | `	pGen->pCurrent = &pGen->sGlobal;` |
|         5 | 2642 | `	pGen->pIn = pGen->pEnd = 0;` |
|         5 | 2643 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|         5 | 2644 | `	pGen->pTokenSet = 0;` |
|         5 | 2645 | `	pGen->nErr = 0;` |
|         5 | 2646 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|         5 | 2647 | `	pGen->nCommaExprOk = 0;` |
|         5 | 2648 | `	pGen->zClauseCloser = 0;` |
|         5 | 2649 | `	pGen->bInGenerator = 0;` |
|         5 | 2650 | `	pGen->bStrictTypes = 0;` |
|         5 | 2651 | `	pGen->bStrictTypesLocked = 0;` |
|         5 | 2652 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|         5 | 2653 | `	pGen->xErr = xErr;` |
|         5 | 2654 | `	pGen->pErrData = pErrData;` |
|         5 | 2655 | `}` |
|         - | 2656 | `/*` |
|         - | 2657 | ` * Restore the outer compile-position state saved by PH7_CompilerSaveState,` |
|         - | 2658 | ` * releasing the nested unit's position containers first. The shared` |
|         - | 2659 | ` * hLiteral/hNumLiteral/hVar tables (grown by the nested compile) are carried` |
|         - | 2660 | ` * forward, NOT rolled back to the snapshot's stale headers.` |
|         - | 2661 | ` */` |
|         4 | 2662 | `PH7_PRIVATE void PH7_CompilerRestoreState(ph7_vm *pVm,ph7_gen_state *pSaved)` |
|         1 | 2663 | `{` |
|         5 | 2664 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 2665 | `	GenBlock *pBlock,*pParent;` |
|         - | 2666 | `	SyHash hVar,hLiteral,hNumLiteral;` |
|         - | 2667 | `	/* Free any nested blocks left open (e.g. an aborted nested compile), then the` |
|         - | 2668 | `	 * nested global block's own fixup sets. */` |
|         5 | 2669 | `	pBlock = pGen->pCurrent;` |
|         5 | 2670 | `	while( pBlock && pBlock->pParent != 0 ){` |
|       ! 0 | 2671 | `		pParent = pBlock->pParent;` |
|       ! 0 | 2672 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 2673 | `		pBlock = pParent;` |
|       ! 0 | 2674 | `	}` |
|         5 | 2675 | `	GenStateReleaseBlock(&pGen->sGlobal);` |
|         - | 2676 | `	/* Release the nested unit's position containers. */` |
|         5 | 2677 | `	SySetRelease(&pGen->aLabel);` |
|         5 | 2678 | `	SySetRelease(&pGen->aGoto);` |
|         5 | 2679 | `	SySetRelease(&pGen->aNullsafeJmp);` |
|         5 | 2680 | `	SySetRelease(&pGen->aLoopParent);` |
|         5 | 2681 | `	SySetRelease(&pGen->aTrivia);` |
|         5 | 2682 | `	SySetRelease(&pGen->aPendingAttrs);` |
|         5 | 2683 | `	SyBlobRelease(&pGen->sWorker);` |
|         5 | 2684 | `	SyBlobRelease(&pGen->sErrBuf);` |
|         5 | 2685 | `	SyBlobRelease(&pGen->sNamespace);` |
|         5 | 2686 | `	SyHashRelease(&pGen->hUseImports);` |
|         5 | 2687 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|         5 | 2688 | `	SyHashRelease(&pGen->hUseConstImports);` |
|         - | 2689 | `	/* Preserve the (possibly grown) shared intern tables across the restore. */` |
|         5 | 2690 | `	hVar = pGen->hVar;` |
|         5 | 2691 | `	hLiteral = pGen->hLiteral;` |
|         5 | 2692 | `	hNumLiteral = pGen->hNumLiteral;` |
|         5 | 2693 | `	*pGen = *pSaved;` |
|         5 | 2694 | `	pGen->hVar = hVar;` |
|         5 | 2695 | `	pGen->hLiteral = hLiteral;` |
|         5 | 2696 | `	pGen->hNumLiteral = hNumLiteral;` |
|         5 | 2697 | `}` |
|         - | 2698 | `/*` |
|         - | 2699 | ` * Raise php's parse error for an unexpected token: E_PARSE with the exact text` |
|         - | 2700 | ` * php's parser prints, e.g.` |
|         - | 2701 | ` *` |
|         - | 2702 | ` *   syntax error, unexpected token ";", expecting "{"` |
|         - | 2703 | ` *   syntax error, unexpected identifier "invalid", expecting "("` |
|         - | 2704 | ` *   syntax error, unexpected end of file` |
|         - | 2705 | ` *` |
|         - | 2706 | ` * php names the token by CLASS, not just by text: an identifier, a variable and` |
|         - | 2707 | ` * a number each get their own noun, while everything else (keywords, operators,` |
|         - | 2708 | ` * punctuation) is a "token". zExpecting is the optional ", expecting ..." tail` |
|         - | 2709 | ` * — pass NULL when the site cannot say what it wanted (php often can't either).` |
|         - | 2710 | ` * pTok == NULL means the input ran out: "unexpected end of file".` |
|         - | 2711 | ` *` |
|         - | 2712 | ` * PHL's hand-written recursive-descent parser has no bison expectation sets, so` |
|         - | 2713 | ` * a site can only claim an "expecting" clause it genuinely knows; every clause` |
|         - | 2714 | ` * emitted here was verified against php 8.5.7 for the construct in question.` |
|         - | 2715 | ` */` |
|       208 | 2716 | `PH7_PRIVATE sxi32 PH7_GenSyntaxError(` |
|         - | 2717 | `	ph7_gen_state *pGen,   /* Code generator state */` |
|         - | 2718 | `	SyToken *pTok,         /* Offending token, or NULL for end of file */` |
|         - | 2719 | `	const char *zExpecting /* ", expecting <this>" tail, or NULL */` |
|         - | 2720 | `	)` |
|         5 | 2721 | `{` |
|       213 | 2722 | `	const char *zNoun = "token";` |
|         - | 2723 | `	sxu32 nLine;` |
|       213 | 2724 | `	if( pTok == 0 && pGen->pTokenSet ){` |
|         - | 2725 | `		/* The caller ran out of tokens inside its own slice — but a statement's slice stops` |
|         - | 2726 | `		 * BEFORE its terminator, so the token php actually names (typically the ';') is the` |
|         - | 2727 | `		 * one sitting just past the slice, still inside the chunk's token stream. Reach for` |
|         - | 2728 | `		 * it before concluding "end of file". */` |
|        96 | 2729 | `		SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        96 | 2730 | `		SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|        96 | 2731 | `		if( pGen->pEnd >= pBase && pGen->pEnd < pStreamEnd ){` |
|        92 | 2732 | `			pTok = pGen->pEnd;` |
|        44 | 2733 | `		}` |
|        46 | 2734 | `	}` |
|       213 | 2735 | `	nLine = pTok ? pTok->nLine : (pGen->pIn > (SyToken *)SySetBasePtr(pGen->pTokenSet) ? pGen->pIn[-1].nLine : 1);` |
|       213 | 2736 | `	if( pTok == 0 ){` |
|         8 | 2737 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|         2 | 2738 | `			zExpecting ? "syntax error, unexpected end of file, expecting %s"` |
|         - | 2739 | `			           : "syntax error, unexpected end of file",` |
|         2 | 2740 | `			zExpecting);` |
|         - | 2741 | `	}` |
|       209 | 2742 | `	if( pTok->nType & PH7_TK_ID ){` |
|        23 | 2743 | `		zNoun = "identifier";` |
|       200 | 2744 | `	}else if( pTok->nType & PH7_TK_DOLLAR ){` |
|         8 | 2745 | `		zNoun = "variable";` |
|         - | 2746 | `		/* The '$' is its own token and carries only "$" as text; the NAME is the` |
|         - | 2747 | ``		 * token after it. php names the whole variable, so `$x` was being reported`` |
|         - | 2748 | ``		 * as the nameless `variable "$"`. Stitch the two back together. */`` |
|         8 | 2749 | `		if( pGen->pTokenSet ){` |
|         8 | 2750 | `			SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|         8 | 2751 | `			SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|         8 | 2752 | `			SyToken *pName = &pTok[1];` |
|         6 | 2753 | `			if( pTok >= pBase && pName < pStreamEnd` |
|         6 | 2754 | `				&& (pName->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|         8 | 2755 | `				&& pName->sData.nByte > 0 ){` |
|         8 | 2756 | `				SyBlobReset(&pGen->sWorker);` |
|         8 | 2757 | `				SyBlobAppend(&pGen->sWorker,"$",sizeof(char));` |
|         8 | 2758 | `				SyBlobAppend(&pGen->sWorker,pName->sData.zString,pName->sData.nByte);` |
|         - | 2759 | `				{` |
|         - | 2760 | `					SyString sVar;` |
|         8 | 2761 | `					SyStringInitFromBuf(&sVar,SyBlobData(&pGen->sWorker),` |
|         - | 2762 | `						SyBlobLength(&pGen->sWorker));` |
|         8 | 2763 | `					if( zExpecting ){` |
|        11 | 2764 | `						return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|         - | 2765 | `							"syntax error, unexpected %s \"%z\", expecting %s",` |
|         3 | 2766 | `							zNoun,&sVar,zExpecting);` |
|         - | 2767 | `					}` |
|       ! 0 | 2768 | `					return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|       ! 0 | 2769 | `						"syntax error, unexpected %s \"%z\"",zNoun,&sVar);` |
|         - | 2770 | `				}` |
|         - | 2771 | `			}` |
|       ! 0 | 2772 | `		}` |
|       185 | 2773 | `	}else if( pTok->nType & PH7_TK_INTEGER ){` |
|        27 | 2774 | `		zNoun = "integer";` |
|       173 | 2775 | `	}else if( pTok->nType & PH7_TK_REAL ){` |
|       ! 0 | 2776 | `		zNoun = "float";` |
|       ! 0 | 2777 | `	}` |
|       203 | 2778 | `	if( zExpecting ){` |
|       143 | 2779 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        46 | 2780 | `			"syntax error, unexpected %s \"%z\", expecting %s",zNoun,&pTok->sData,zExpecting);` |
|         - | 2781 | `	}` |
|       164 | 2782 | `	return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        53 | 2783 | `		"syntax error, unexpected %s \"%z\"",zNoun,&pTok->sData);` |
|       109 | 2784 | `}` |
|         - | 2785 | `/*` |
|         - | 2786 | ` * Generate a compile-time error message.` |
|         - | 2787 | ` * If the error count limit is reached (usually 15 error message)` |
|         - | 2788 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|         - | 2789 | ` * abort compilation immediately.` |
|         - | 2790 | ` */` |
|       686 | 2791 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|         5 | 2792 | `{` |
|       691 | 2793 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|       691 | 2794 | `	const char *zErr = "Error";` |
|         - | 2795 | `	SyString *pFile;` |
|         - | 2796 | `	va_list ap;` |
|         - | 2797 | `	sxi32 rc;` |
|         - | 2798 | `	/* Reset the working buffer */` |
|       691 | 2799 | `	SyBlobReset(pWorker);` |
|         - | 2800 | `	/* Peek the processed file path if available */` |
|       691 | 2801 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|       691 | 2802 | `	if( nErrType == E_ERROR \|\| nErrType == E_PARSE ){` |
|         - | 2803 | `		/* Increment the error counter. A PARSE error is every bit as fatal as an` |
|         - | 2804 | `		 * E_ERROR one: php compiles nothing, runs nothing and exits 255. Counting` |
|         - | 2805 | `		 * only E_ERROR let a parse error print its diagnostic and then fall through` |
|         - | 2806 | `		 * into execution with a 0 exit status. */` |
|       683 | 2807 | `		pGen->nErr++;` |
|       683 | 2808 | `		if( pGen->nErr > 15 ){` |
|         - | 2809 | `			/* Error count limit reached */` |
|         6 | 2810 | `			if( pGen->xErr ){` |
|         6 | 2811 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|         6 | 2812 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|         6 | 2813 | `				if( pFile ){` |
|         6 | 2814 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|         2 | 2815 | `				}` |
|         6 | 2816 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|         6 | 2817 | `				if( SyBlobLength(pWorker) > 0 ){` |
|         6 | 2818 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|         2 | 2819 | `				}` |
|         2 | 2820 | `			}` |
|         - | 2821 | `			/* Abort immediately */` |
|         6 | 2822 | `			return SXERR_ABORT;` |
|         - | 2823 | `		}` |
|       337 | 2824 | `	}` |
|       687 | 2825 | `	if( pGen->xErr == 0 ){` |
|         - | 2826 | `		/* No consumer — but keep the BARE message in the error buffer so a caller` |
|         - | 2827 | `		 * that needs the text can read it back. eval() compiles with logging off` |
|         - | 2828 | `		 * (a parse error there is php's catchable ParseError, not a printed` |
|         - | 2829 | `		 * diagnostic) and needs exactly this string for the exception message. */` |
|         5 | 2830 | `		va_start(ap,zFormat);` |
|         5 | 2831 | `		SyBlobFormatAp(pWorker,zFormat,ap);` |
|         5 | 2832 | `		va_end(ap);` |
|         5 | 2833 | `		return SXRET_OK;` |
|         - | 2834 | `	}` |
|       683 | 2835 | `	switch(nErrType){` |
|       310 | 2836 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|        11 | 2837 | `	case E_WARNING: zErr = "Warning";     break;` |
|       368 | 2838 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       ! 0 | 2839 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|       ! 0 | 2840 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|       ! 0 | 2841 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|       ! 0 | 2842 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|       ! 0 | 2843 | `	case 8192 /* E_DEPRECATED */: zErr = "Deprecated"; break;` |
|       ! 0 | 2844 | `	default:` |
|       ! 0 | 2845 | `		break;` |
|         - | 2846 | `	}` |
|       683 | 2847 | `	rc = SXRET_OK;` |
|         - | 2848 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|       683 | 2849 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|       683 | 2850 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|       683 | 2851 | `	va_start(ap,zFormat);` |
|       683 | 2852 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|       683 | 2853 | `	va_end(ap);` |
|       683 | 2854 | `	if( pFile ){` |
|       683 | 2855 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       339 | 2856 | `	}` |
|         - | 2857 | `	/* Append a new line */` |
|       683 | 2858 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       683 | 2859 | `	if( SyBlobLength(pWorker) > 0 ){` |
|         - | 2860 | `		/* Consume the generated error message */` |
|       683 | 2861 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       339 | 2862 | `	}` |
|       683 | 2863 | `	return rc;` |
|       348 | 2864 | `}` |
|         - | 2865 |  |
