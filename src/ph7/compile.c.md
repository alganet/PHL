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
|    178568 |   46 | `PH7_PRIVATE GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|         5 |   47 | `{` |
|    178573 |   48 | `	GenBlock *pBlock = pCurrent;` |
|    360873 |   49 | `	for(;;){` |
|    721751 |   50 | `		if( pBlock->iFlags & iBlockType ){` |
|    178573 |   51 | `			iCount--; /* Decrement nesting level */` |
|    178573 |   52 | `			if( iCount < 1 ){` |
|         - |   53 | `				/* Block meet with the desired criteria */` |
|    178547 |   54 | `				return pBlock;` |
|         - |   55 | `			}` |
|        13 |   56 | `		}` |
|         - |   57 | `		/* Point to the upper block */` |
|    543209 |   58 | `		pBlock = pBlock->pParent;` |
|    543209 |   59 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|         - |   60 | `			/* Forbidden */` |
|        16 |   61 | `			break;` |
|         - |   62 | `		}` |
|         5 |   63 | `	}` |
|         - |   64 | `	/* No such block */` |
|        29 |   65 | `	return 0;` |
|     89289 |   66 | `}` |
|         - |   67 | `/*` |
|         - |   68 | ` * Initialize a freshly allocated block instance.` |
|         - |   69 | ` */` |
|  13090134 |   70 | `static void GenStateInitBlock(` |
|         - |   71 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |   72 | `	GenBlock *pBlock,    /* Target block */` |
|         - |   73 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   74 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|         - |   75 | `	void *pUserData      /* Upper layer private data */` |
|         - |   76 | `	)` |
|         5 |   77 | `{` |
|         - |   78 | `	/* Initialize block fields */` |
|  13090139 |   79 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  13090139 |   80 | `	pBlock->pUserData   = pUserData;` |
|  13090139 |   81 | `	pBlock->pGen        = pGen;` |
|  13090139 |   82 | `	pBlock->iFlags      = iType;` |
|  13090139 |   83 | `	pBlock->pParent     = 0;` |
|  13090139 |   84 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  13090139 |   85 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  13090139 |   86 | `}` |
|         - |   87 | `/*` |
|         - |   88 | ` * Allocate a new block instance.` |
|         - |   89 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|         - |   90 | ` * on success.Otherwise generate a compile-time error and abort` |
|         - |   91 | ` * processing on failure.` |
|         - |   92 | ` */` |
|  13086252 |   93 | `PH7_PRIVATE sxi32 GenStateEnterBlock(` |
|         - |   94 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |   95 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   96 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|         - |   97 | `	void *pUserData,      /* Upper layer private data */` |
|         - |   98 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|         - |   99 | `	)` |
|         5 |  100 | `{` |
|         - |  101 | `	GenBlock *pBlock;` |
|         - |  102 | `	/* Allocate a new block instance */` |
|  13086257 |  103 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  13086257 |  104 | `	if( pBlock == 0 ){` |
|         - |  105 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |  106 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - |  107 | `		 */` |
|       ! 0 |  108 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|         - |  109 | `		/* Abort processing immediately */` |
|       ! 0 |  110 | `		return SXERR_ABORT;` |
|         - |  111 | `	}` |
|         - |  112 | `	/* Zero the structure */` |
|  13086257 |  113 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  13086257 |  114 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|         - |  115 | `	/* Link to the parent block */` |
|  13086257 |  116 | `	pBlock->pParent = pGen->pCurrent;` |
|         - |  117 | `	/* A loop or switch gets an id, and remembers the loop it nests inside, so a goto's` |
|         - |  118 | `	 * and a label's positions can be compared after compilation (see aLoopParent). */` |
|  13086257 |  119 | `	if( iType & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    540187 |  120 | `		sxu32 nParent = pGen->nCurLoopId;` |
|    540187 |  121 | `		pGen->nLoopId++;` |
|    540187 |  122 | `		SySetPut(&pGen->aLoopParent,(const void *)&nParent);` |
|    540187 |  123 | `		pBlock->nLoopId = pGen->nLoopId;` |
|    540187 |  124 | `		pBlock->nOuterLoopId = nParent;` |
|    540187 |  125 | `		pGen->nCurLoopId = pGen->nLoopId;` |
|    270091 |  126 | `	}` |
|         - |  127 | `	/* Mark as the current block */` |
|  13086257 |  128 | `	pGen->pCurrent = pBlock;` |
|  13086257 |  129 | `	if( ppBlock ){` |
|         - |  130 | `		/* Write a pointer to the new instance */` |
|   6293587 |  131 | `		*ppBlock = pBlock;` |
|   3146791 |  132 | `	}` |
|  13086257 |  133 | `	return SXRET_OK;` |
|   6543131 |  134 | `}` |
|         - |  135 | `/*` |
|         - |  136 | ` * Release block fields without freeing the whole instance.` |
|         - |  137 | ` */` |
|  13086244 |  138 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|         5 |  139 | `{` |
|  13086249 |  140 | `	SySetRelease(&pBlock->aPostContFix);` |
|  13086249 |  141 | `	SySetRelease(&pBlock->aJumpFix);` |
|  13086249 |  142 | `}` |
|         - |  143 | `/*` |
|         - |  144 | ` * Release a block.` |
|         - |  145 | ` */` |
|  13086240 |  146 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|         5 |  147 | `{` |
|  13086245 |  148 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  13086245 |  149 | `	GenStateReleaseBlock(&(*pBlock));` |
|         - |  150 | `	/* Free the instance */` |
|  13086245 |  151 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  13086245 |  152 | `}` |
|         - |  153 | `/*` |
|         - |  154 | ` * POP and release a block from the stack of compiled blocks.` |
|         - |  155 | ` */` |
|  13086240 |  156 | `PH7_PRIVATE sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|         5 |  157 | `{` |
|  13086245 |  158 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  13086245 |  159 | `	if( pBlock == 0 ){` |
|         - |  160 | `		/* No more block to pop */` |
|       ! 0 |  161 | `		return SXERR_EMPTY;` |
|         - |  162 | `	}` |
|  13086245 |  163 | `	if( pBlock->iFlags & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    540179 |  164 | `		pGen->nCurLoopId = pBlock->nOuterLoopId;` |
|    270087 |  165 | `	}` |
|         - |  166 | `	/* Point to the upper block */` |
|  13086245 |  167 | `	pGen->pCurrent = pBlock->pParent;` |
|  13086245 |  168 | `	if( ppBlock ){` |
|         - |  169 | `		/* Write a pointer to the popped block */` |
|       ! 0 |  170 | `		*ppBlock = pBlock;` |
|       ! 0 |  171 | `	}else{` |
|         - |  172 | `		/* Safely release the block */` |
|  13086245 |  173 | `		GenStateFreeBlock(&(*pBlock));` |
|         - |  174 | `	}` |
|  13086245 |  175 | `	return SXRET_OK;` |
|   6543125 |  176 | `}` |
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
|   1038950 |  195 | `PH7_PRIVATE int GenStateUnconditionalTopLevel(ph7_gen_state *pGen)` |
|         5 |  196 | `{` |
|   1038955 |  197 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   1042937 |  198 | `	while( pBlock ){` |
|   1042937 |  199 | `		if( pBlock->iFlags & (GEN_BLOCK_COND\|GEN_BLOCK_LOOP\|GEN_BLOCK_FUNC\|GEN_BLOCK_SWITCH\|GEN_BLOCK_EXCEPTION) ){` |
|        76 |  200 | `			return 0; /* conditional / nested */` |
|         - |  201 | `		}` |
|   1042865 |  202 | `		if( pBlock->iFlags & GEN_BLOCK_GLOBAL ){` |
|   1038883 |  203 | `			return 1; /* reached the global block with no conditional ancestor */` |
|         - |  204 | `		}` |
|      3987 |  205 | `		pBlock = pBlock->pParent;` |
|         5 |  206 | `	}` |
|       ! 0 |  207 | `	return 1;` |
|    519480 |  208 | `}` |
|         - |  209 | `/*` |
|         - |  210 | ` * Guard a top-level function about to be installed. Same contract as the class` |
|         - |  211 | ` * guard above.` |
|         - |  212 | ` */` |
|    521048 |  213 | `PH7_PRIVATE sxi32 GenStateGuardFuncRedeclaration(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|         5 |  214 | `{` |
|         - |  215 | `	SyHashEntry *pEntry;` |
|    521053 |  216 | `	if( !GenStateUnconditionalTopLevel(pGen) ){` |
|        52 |  217 | `		return SXRET_OK;` |
|         - |  218 | `	}` |
|    521003 |  219 | `	pFunc->iFlags \|= VM_FUNC_BOUND;` |
|    521003 |  220 | `	if( pGen->pVm->bCompilingBuiltin ){` |
|    519657 |  221 | `		return SXRET_OK;` |
|         - |  222 | `	}` |
|         - |  223 | ``	/* NOTE: a userland function shadowing a C builtin (e.g. `function strlen(){}`)`` |
|         - |  224 | `	 * is NOT caught here — the C builtins register in PH7_VmMakeReady, after user` |
|         - |  225 | `	 * code has compiled, so hHostFunction is still empty at this point. Prelude` |
|         - |  226 | `	 * functions (ini_get, ...) and every builtin CLASS compile earlier and ARE` |
|         - |  227 | `	 * guarded. Redeclaring a C builtin function stays a known divergence. */` |
|      1351 |  228 | `	pEntry = SyHashGet(&pGen->pVm->hFunction,(const void *)pFunc->sName.zString,pFunc->sName.nByte);` |
|      1351 |  229 | `	if( pEntry ){` |
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
|      1347 |  246 | `	return SXRET_OK;` |
|    260529 |  247 | `}` |
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
|   4726068 |  258 | `PH7_PRIVATE sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|         5 |  259 | `{` |
|         - |  260 | `	JumpFixup sJumpFix;` |
|         - |  261 | `	sxi32 rc;` |
|         - |  262 | `	/* Init the JumpFixup structure */` |
|   4726073 |  263 | `	sJumpFix.nJumpType = nJumpType;` |
|   4726073 |  264 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|         - |  265 | `	/* Insert in the jump fixup table */` |
|   4726073 |  266 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|   4726073 |  267 | `	return rc;` |
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
|   9097602 |  280 | `PH7_PRIVATE sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|         5 |  281 | `{` |
|         - |  282 | `	JumpFixup *aFix;` |
|         - |  283 | `	VmInstr *pInstr;` |
|         - |  284 | `	sxu32 nFixed;` |
|         - |  285 | `	sxu32 n;` |
|         - |  286 | `	/* Point to the jump fixup table */` |
|   9097607 |  287 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|         - |  288 | `	/* Fix the desired jumps */` |
|  19042725 |  289 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|   9945123 |  290 | `		if( aFix[n].nJumpType < 0 ){` |
|         - |  291 | `			/* Already fixed */` |
|   3713567 |  292 | `			continue;` |
|         - |  293 | `		}` |
|   6231561 |  294 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|         - |  295 | `			/* Not of our interest */` |
|   1505495 |  296 | `			continue;` |
|         - |  297 | `		}` |
|         - |  298 | `		/* Point to the instruction to fix */` |
|   4726071 |  299 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|   4726071 |  300 | `		if( pInstr ){` |
|   4726071 |  301 | `			pInstr->iP2 = nJumpDest;` |
|   4726071 |  302 | `			nFixed++;` |
|         - |  303 | `			/* Mark as fixed */` |
|   4726071 |  304 | `			aFix[n].nJumpType = -1;` |
|   2363033 |  305 | `		}` |
|   2363038 |  306 | `	}` |
|         - |  307 | `	/* Total number of fixed jumps */` |
|   9097607 |  308 | `	return nFixed;` |
|         5 |  309 | `}` |
|         - |  310 | `/*` |
|         - |  311 | ` * Fix a 'goto' now the jump destination is resolved.` |
|         - |  312 | ` * The goto statement can be used to jump to another section` |
|         - |  313 | ` * in the program.` |
|         - |  314 | ` * Refer to the routine responsible of compiling the goto` |
|         - |  315 | ` * statement for more information.` |
|         - |  316 | ` */` |
|   3368256 |  317 | `PH7_PRIVATE sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|         5 |  318 | `{` |
|         - |  319 | `	JumpFixup *pJump,*aJumps;` |
|         - |  320 | `	Label *pLabel;` |
|         - |  321 | `	VmInstr *pInstr;` |
|         - |  322 | `	sxi32 rc;` |
|         - |  323 | `	sxu32 n;` |
|         - |  324 | `	/* Point to the goto table */` |
|   3368261 |  325 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|         - |  326 | `	/* Fix */` |
|   3368407 |  327 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|   3368259 |  378 | `	return SXRET_OK;` |
|   1684133 |  379 | `}` |
|         - |  380 | `/*` |
|         - |  381 | ` * Check if a given token value is installed in the literal table.` |
|         - |  382 | ` */` |
|  16872184 |  383 | `PH7_PRIVATE sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|         5 |  384 | `{` |
|         - |  385 | `	SyHashEntry *pEntry;` |
|  16872189 |  386 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  16872189 |  387 | `	if( pEntry == 0 ){` |
|   4498017 |  388 | `		return SXERR_NOTFOUND;` |
|         - |  389 | `	}` |
|  12374177 |  390 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  12374177 |  391 | `	return SXRET_OK;` |
|   8436097 |  392 | `}` |
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
|   4498012 |  403 | `PH7_PRIVATE sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|         5 |  404 | `{` |
|   4498017 |  405 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|   4498017 |  406 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   2249006 |  407 | `	}` |
|   4498017 |  408 | `	return SXRET_OK;` |
|         5 |  409 | `}` |
|         - |  410 | `/*` |
|         - |  411 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|         - |  412 | ` * in the constant table.` |
|         - |  413 | ` */` |
|   3814448 |  414 | `PH7_PRIVATE ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|         5 |  415 | `{` |
|         - |  416 | `	ph7_value *pObj;` |
|   3814453 |  417 | `	sxu32 nIdx = 0; /* cc warning */` |
|         - |  418 | `	/* Reserve a new constant */` |
|   3814453 |  419 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   3814453 |  420 | `	if( pObj == 0 ){` |
|       ! 0 |  421 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  422 | `		return 0;` |
|         - |  423 | `	}` |
|   3814453 |  424 | `	*pIdx = nIdx;` |
|         - |  425 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|         - |  426 | `	 * the constant string iterals table [optimization purposes].` |
|         - |  427 | `	 */` |
|   3814453 |  428 | `	return pObj;` |
|   1907229 |  429 | `}` |
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
|   7937580 |  444 | `PH7_PRIVATE void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|         5 |  445 | `{` |
|         - |  446 | `	VmCallArgMap *pMap;` |
|   7937585 |  447 | `	if( !pGen->bStrictTypes ) return p3;` |
|        39 |  448 | `	if( p3 == 0 ){` |
|        35 |  449 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|        35 |  450 | `		if( pMap == 0 ) return 0;` |
|        35 |  451 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|        35 |  452 | `		p3 = (void *)pMap;` |
|        16 |  453 | `	}` |
|        39 |  454 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|        39 |  455 | `	return p3;` |
|   3968795 |  456 | `}` |
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
|    341504 |  476 | `PH7_PRIVATE int GenStateIsReservedConstant(SyString *pName)` |
|         5 |  477 | `{` |
|    341509 |  478 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      3927 |  479 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|         3 |  480 | `			return TRUE;` |
|      3925 |  481 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|         6 |  482 | `			return TRUE;` |
|         5 |  483 | `		}` |
|    339545 |  484 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|      7781 |  485 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|         3 |  486 | `			return TRUE;` |
|         - |  487 | `		}` |
|      3887 |  488 | `	}` |
|         - |  489 | `	/* Not a reserved constant */` |
|    341501 |  490 | `	return FALSE;` |
|    170757 |  491 | `}` |
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
|  53924436 |  508 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|         5 |  509 | `{` |
|  53924441 |  510 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|         - |  511 | `	sxu32 nTarget;` |
|         - |  512 | `	sxu32 *aIdx;` |
|         - |  513 | `	sxu32 i;` |
|  53924441 |  514 | `	if( nCur <= nBaseline ){` |
|  53924345 |  515 | `		return;` |
|         - |  516 | `	}` |
|       100 |  517 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|       100 |  518 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|       204 |  519 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|       108 |  520 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|       108 |  521 | `		if( pInstr ){` |
|       108 |  522 | `			pInstr->iP2 = (sxi32)nTarget;` |
|        52 |  523 | `		}` |
|        56 |  524 | `	}` |
|       100 |  525 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
|  26962223 |  526 | `}` |
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
|   7049758 |  545 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
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
|   7049763 |  564 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|   1804841 |  565 | `		return 0;` |
|         - |  566 | `	}` |
|  57212063 |  567 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
|  52025472 |  568 | `		if( pName->nByte == aByRef[i].nByte` |
|  27526842 |  569 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|     58341 |  570 | `			return aByRef[i].mask;` |
|         - |  571 | `		}` |
|  25983573 |  572 | `	}` |
|   5186591 |  573 | `	return 0;` |
|   3524884 |  574 | `}` |
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
|   7049758 |  585 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|         5 |  586 | `{` |
|         - |  587 | `	SyToken *p, *pEnd;` |
|   7049763 |  588 | `	pOut->zString = 0;` |
|   7049763 |  589 | `	pOut->nByte = 0;` |
|   7049763 |  590 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|       ! 0 |  591 | `		return;` |
|         - |  592 | `	}` |
|   7049763 |  593 | `	p = pLeft->pStart;` |
|   7049763 |  594 | `	pEnd = pLeft->pEnd;` |
|         - |  595 | `	/* Optional single leading namespace separator (absolute path). */` |
|   7049763 |  596 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|      3921 |  597 | `		p++;` |
|      1958 |  598 | `	}` |
|   7049763 |  599 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|   1804795 |  600 | `		return;` |
|         - |  601 | `	}` |
|         - |  602 | `	/* Must be a single component: nothing follows the name token. */` |
|   5244973 |  603 | `	if( p + 1 != pEnd ){` |
|        51 |  604 | `		return;` |
|         - |  605 | `	}` |
|   5244927 |  606 | `	*pOut = p->sData;` |
|   3524884 |  607 | `}` |
|         - |  608 | `/*` |
|         - |  609 | ` * Generate bytecode for a given expression tree.` |
|         - |  610 | ` * If something goes wrong while generating bytecode` |
|         - |  611 | ` * for the expression tree (A very unlikely scenario)` |
|         - |  612 | ` * this function takes care of generating the appropriate` |
|         - |  613 | ` * error message.` |
|         - |  614 | ` */` |
|  76033052 |  615 | `static sxi32 GenStateEmitExprCode(` |
|         - |  616 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |  617 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|         - |  618 | `	sxi32 iFlags /* Control flags */` |
|         - |  619 | `	)` |
|         5 |  620 | `{` |
|         - |  621 | `	VmInstr *pInstr;` |
|         - |  622 | `	sxu32 nJmpIdx;` |
|  76033057 |  623 | `	sxi32 iP1 = 0;` |
|  76033057 |  624 | `	sxu32 iP2 = 0;` |
|  76033057 |  625 | `	void *p3  = 0;` |
|         - |  626 | `	sxi32 iVmOp;` |
|         - |  627 | `	sxi32 rc;` |
|  76033057 |  628 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
|  76033057 |  629 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
|  76033057 |  630 | `	sxu32 nRhsNsBase = 0;` |
|  76033057 |  631 | `	if( pNode->xCode ){` |
|         - |  632 | `		SyToken *pTmpIn,*pTmpEnd;` |
|         - |  633 | `		/* Compile node */` |
|  45889935 |  634 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
|  45889935 |  635 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
|  45889935 |  636 | `		RE_SWAP_DELIMITER(pGen);` |
|  45889935 |  637 | `		return rc;` |
|         - |  638 | `	}` |
|  30143127 |  639 | `	if( pNode->pOp == 0 ){` |
|       ! 0 |  640 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - |  641 | `			"Invalid expression node,PH7 is aborting compilation");` |
|       ! 0 |  642 | `		return SXERR_ABORT;` |
|         - |  643 | `	}` |
|  30143127 |  644 | `	iVmOp = pNode->pOp->iVmOp;` |
|  30143127 |  645 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|         - |  646 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|         - |  647 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|         - |  648 | `		 * and later errors are still reported. */` |
|         3 |  649 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - |  650 | `			"The (unset) cast is no longer supported");` |
|         3 |  651 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  652 | `			return SXERR_ABORT;` |
|         - |  653 | `		}` |
|         1 |  654 | `	}` |
|  30143127 |  655 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
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
|  30143037 |  705 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|         - |  706 | `		sxu32 nJz,nJmp;` |
|         - |  707 | `		sxu32 nTernaryNsBase;` |
|         - |  708 | `		/* Ternary operator require special handling */` |
|         - |  709 | `		/* Phase#1: Compile the condition */` |
|    503567 |  710 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    503567 |  711 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    503567 |  712 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  713 | `			return rc;` |
|         - |  714 | `		}` |
|         - |  715 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|         - |  716 | `		 * compiling the condition must short-circuit to the end of the` |
|         - |  717 | `		 * condition expression, not leak past the ternary. */` |
|    503567 |  718 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    503567 |  719 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    503567 |  720 | `		if( pNode->pLeft ){` |
|         - |  721 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|         - |  722 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    499619 |  723 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - |  724 | `			/* Phase#3: Compile the 'then' expression  */` |
|    499619 |  725 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    499619 |  726 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    499619 |  727 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  728 | `				return rc;` |
|         - |  729 | `			}` |
|    499619 |  730 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    249812 |  731 | `		}else{` |
|         - |  732 | `			/* Elvis operator: (expr) ?: (else)` |
|         - |  733 | `			 * Duplicate condition so original value is the 'then' result.` |
|         - |  734 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      3953 |  735 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      3953 |  736 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - |  737 | `		}` |
|         - |  738 | `		/* Phase#4: Emit the unconditional jump */` |
|    503567 |  739 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|         - |  740 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    503567 |  741 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    503567 |  742 | `		if( pInstr ){` |
|    503567 |  743 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    251781 |  744 | `		}` |
|    503567 |  745 | `		if( !pNode->pLeft ){` |
|         - |  746 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      3953 |  747 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      1974 |  748 | `		}` |
|         - |  749 | `		/* Phase#6: Compile the 'else' expression */` |
|    503567 |  750 | `		if( pNode->pRight ){` |
|    503567 |  751 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    503567 |  752 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    503567 |  753 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  754 | `				return rc;` |
|         - |  755 | `			}` |
|    503567 |  756 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    251781 |  757 | `		}` |
|    503567 |  758 | `		if( nJmp > 0 ){` |
|         - |  759 | `			/* Phase#7: Fix the unconditional jump */` |
|    503567 |  760 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    503567 |  761 | `			if( pInstr ){` |
|    503567 |  762 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    251781 |  763 | `			}` |
|    251781 |  764 | `		}` |
|         - |  765 | `		/* All done */` |
|    503567 |  766 | `		return SXRET_OK;` |
|         - |  767 | `	}` |
|  29639475 |  768 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
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
|  29639449 |  801 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|         - |  802 | `	/* Generate code for the left tree */` |
|  29639449 |  803 | `	if( pNode->pLeft ){` |
|  29585343 |  804 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  29585343 |  805 | `		if( iVmOp == PH7_OP_CALL ){` |
|         - |  806 | `			ph7_expr_node **apNode;` |
|   7053971 |  807 | `			int hasSpread = 0;` |
|   7053971 |  808 | `			int hasNamed = 0;` |
|   7053971 |  809 | `			int bAnySpread = 0;` |
|   7053971 |  810 | `			sxu32 byRefMask = 0;` |
|         - |  811 | `			sxi32 nArgs;` |
|         - |  812 | `			sxi32 n;` |
|         - |  813 | `			/* Recurse and generate bytecodes for function arguments */` |
|   7053971 |  814 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   7053971 |  815 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|         - |  816 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|         - |  817 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|         - |  818 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|   7053971 |  819 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|        81 |  820 | `				bFcc = 1;` |
|        81 |  821 | `				nArgs = 0;` |
|        40 |  822 | `			}` |
|         - |  823 | `			/* Validate argument order like php: no positional argument after a` |
|         - |  824 | ``			 * named one OR after unpacking, and `name: ...$x` is a parse error. */`` |
|         - |  825 | `			{` |
|   7053971 |  826 | `				int seenNamed = 0;` |
|   7053971 |  827 | `				int seenSpread = 0;` |
|  15064099 |  828 | `				for( n = 0; n < nArgs; ++n ){` |
|   8010135 |  829 | `					if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|      4079 |  830 | `						bAnySpread = 1;` |
|      4079 |  831 | `						seenSpread = 1;` |
|      4079 |  832 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       ! 0 |  833 | `							rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - |  834 | `								"syntax error, unexpected token \"...\"");` |
|       ! 0 |  835 | `							return SXERR_SYNTAX;` |
|         5 |  836 | `						}` |
|   8008098 |  837 | `					}else if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       290 |  838 | `						seenNamed = 1;` |
|       290 |  839 | `						hasNamed = 1;` |
|   8005918 |  840 | `					}else if( seenNamed ){` |
|         3 |  841 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - |  842 | `							"Cannot use positional argument after named argument");` |
|         3 |  843 | `						return SXERR_SYNTAX;` |
|   8005773 |  844 | `					}else if( seenSpread ){` |
|       ! 0 |  845 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - |  846 | `							"Cannot use positional argument after argument unpacking");` |
|       ! 0 |  847 | `						return SXERR_SYNTAX;` |
|         - |  848 | `					}` |
|   4005069 |  849 | `				}` |
|         - |  850 | `			}` |
|         - |  851 | `			/* Read-only load */` |
|   7053969 |  852 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|         - |  853 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|         - |  854 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|         - |  855 | `			 * objects dispatch to the right method (offsetExists for both;` |
|         - |  856 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|   7053969 |  857 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|   7053969 |  858 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|   7053964 |  859 | `				if( pCallName->nByte == 5` |
|   3920098 |  860 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|    326143 |  861 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|   6890900 |  862 | `				}else if( pCallName->nByte == 5` |
|   3593960 |  863 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|       115 |  864 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|        55 |  865 | `				}` |
|         - |  866 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|         - |  867 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|         - |  868 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|         - |  869 | `				 * write back through. Skipped when spread/named args are present:` |
|         - |  870 | `				 * the compile-time positional index no longer maps to the` |
|         - |  871 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|   7053969 |  872 | `				if( !bAnySpread && !hasNamed ){` |
|         - |  873 | `					SyString sBuiltin;` |
|   7049763 |  874 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|   7049763 |  875 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|   3524879 |  876 | `				}` |
|   3526982 |  877 | `			}` |
|  15064095 |  878 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|   8010131 |  879 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   8010131 |  880 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|         - |  881 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|         - |  882 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|         - |  883 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|         - |  884 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|         - |  885 | `				 * builtin to write back through. A plain $var target is unaffected` |
|         - |  886 | `				 * (iP1=0 either way). */` |
|   8010131 |  887 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|     38871 |  888 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|     38871 |  889 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|     19433 |  890 | `				}` |
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
|   8010126 |  901 | `				if( (iFlags & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_LOAD_IDX_UNSET)) == 0` |
|   7846990 |  902 | `				 && apNode[n]->pOp == 0 && apNode[n]->xCode == PH7_CompileVariable` |
|   4112489 |  903 | `				 && (apNode[n]->iFlags & (EXPR_NODE_NAMED_ARG\|EXPR_NODE_SPREAD)) == 0 ){` |
|   3360939 |  904 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|   1680467 |  905 | `				}` |
|   8010131 |  906 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|   8010131 |  907 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  908 | `					return rc;` |
|         - |  909 | `				}` |
|         - |  910 | `				/* Each argument is an independent nullsafe scope. */` |
|   8010131 |  911 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|   8010131 |  912 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|         - |  913 | `					/* Emit spread opcode to unpack this array argument */` |
|      4079 |  914 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      4079 |  915 | `					hasSpread = 1;` |
|      2037 |  916 | `				}` |
|   4005068 |  917 | `			}` |
|         - |  918 | `			/* Total number of given arguments */` |
|   7053969 |  919 | `			iP1 = nArgs;` |
|   7053969 |  920 | `			iP2 = hasSpread;` |
|         - |  921 | `			/* Build VmCallArgMap if named arguments are present.` |
|         - |  922 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|   7053969 |  923 | `			if( hasNamed ){` |
|       179 |  924 | `				sxu32 nStrBytes = 0;` |
|         - |  925 | `				char *zBuf;` |
|       539 |  926 | `				for( n = 0; n < nArgs; ++n ){` |
|       363 |  927 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       287 |  928 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|       142 |  929 | `					}` |
|       183 |  930 | `				}` |
|         - |  931 | `				{` |
|       179 |  932 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|       179 |  933 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|       176 |  934 | `					&pGen->pVm->sAllocator, mapSize);` |
|       179 |  935 | `				if( pMap ){` |
|       179 |  936 | `					SyZero(pMap, mapSize);` |
|       179 |  937 | `					pMap->bHasNamed = 1;` |
|       179 |  938 | `					pMap->nTotal = (sxu32)nArgs;` |
|       179 |  939 | `					pMap->aNames = (SyString *)&pMap[1];` |
|       179 |  940 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|       539 |  941 | `					for( n = 0; n < nArgs; ++n ){` |
|       363 |  942 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       287 |  943 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|       287 |  944 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|       287 |  945 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|       287 |  946 | `							zBuf += nb;` |
|       142 |  947 | `						}` |
|         - |  948 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|       183 |  949 | `					}` |
|       179 |  950 | `					p3 = (void *)pMap;` |
|        88 |  951 | `				}` |
|         - |  952 | `				}` |
|        88 |  953 | `			}` |
|         - |  954 | `			/* Remove stale flags now */` |
|   7053969 |  955 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|   3526982 |  956 | `		}` |
|         - |  957 | `		{` |
|         - |  958 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|         - |  959 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|         - |  960 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|         - |  961 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|         - |  962 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|         - |  963 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|         - |  964 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|         - |  965 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
|  29585341 |  966 | `			sxi32 iLeftFlags = iFlags;` |
|  29585336 |  967 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
|  24022026 |  968 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|   9229384 |  969 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|   7945227 |  970 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   2778337 |  971 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|   1389166 |  972 | `			}` |
|         - |  973 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|         - |  974 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|         - |  975 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|         - |  976 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|         - |  977 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|         - |  978 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|         - |  979 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
|  29585336 |  980 | `			if( pNode->pOp` |
|  41593894 |  981 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
|  26801273 |  982 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  24017158 |  983 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|   5988111 |  984 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|   2994053 |  985 | `			}` |
|         - |  986 | ``			/* `++`/`--` mutate their operand in place — the operand is a write`` |
|         - |  987 | ``			 * lvalue exactly like a compound assign's (`$o->m[0]++` must tag the`` |
|         - |  988 | ``			 * member base PH7_MEMBER_WRITE the way `$o->m[0] += 1` does: hooked`` |
|         - |  989 | `			 * properties throw php's Indirect-modification Error, missing ones` |
|         - |  990 | `			 * auto-vivify). The prec-18 site below handles the assign family;` |
|         - |  991 | ``			 * `++`/`--` are unary, their operand is pLeft. */`` |
|  29585336 |  992 | `			if( pNode->pOp` |
|  29585341 |  993 | `				&& (pNode->pOp->iVmOp == PH7_OP_INCR \|\| pNode->pOp->iVmOp == PH7_OP_DECR) ){` |
|    217607 |  994 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|    108801 |  995 | `			}` |
|         - |  996 | ``			/* `??` reads its LEFT operand in isset-context: an undefined or`` |
|         - |  997 | `			 * UNINITIALIZED typed PROPERTY must yield the default rather than a` |
|         - |  998 | `			 * warning/Error (php). Tag a member-access LHS so its OP_MEMBER takes` |
|         - |  999 | `			 * the silent-lookup path (iP2 = ISSET), which still loads a present` |
|         - | 1000 | `			 * value. A SUBSCRIPT LHS is left untagged: LOAD_IDX's ISSET mode means` |
|         - | 1001 | ``			 * offsetExists (a bool), but `$o[$k] ?? d` needs the offsetGet value —`` |
|         - | 1002 | `			 * that path is already handled correctly by OP_NULLC. */` |
|  29585336 | 1003 | `			if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC` |
|  14821835 | 1004 | `				&& pNode->pLeft && pNode->pLeft->pOp` |
|     87439 | 1005 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|     58278 | 1006 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|     58261 | 1007 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|        37 | 1008 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|        18 | 1009 | `			}` |
|  29585341 | 1010 | `			if( iVmOp == PH7_OP_ERR_CTRL ){` |
|         - | 1011 | `				/* '@' must suppress the diagnostics raised WHILE its operand runs, so` |
|         - | 1012 | `				 * open the window here; the trailing emit below closes it (iP1 = 0). */` |
|     15753 | 1013 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_ERR_CTRL,1,0,0,0);` |
|      7874 | 1014 | `			}` |
|  29585341 | 1015 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags);` |
|         - | 1016 | `		}` |
|  29585341 | 1017 | `		if( rc != SXRET_OK ){` |
|        34 | 1018 | `			return rc;` |
|         - | 1019 | `		}` |
|  29585311 | 1020 | `		if( !bIsChainOp ){` |
|         - | 1021 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|         - | 1022 | `			 * target the end of that LHS chain, which is right here. */` |
|  13518863 | 1023 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|   6759429 | 1024 | `		}` |
|  29585311 | 1025 | `		if( iVmOp == PH7_OP_CALL ){` |
|   7053969 | 1026 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   7053969 | 1027 | `			if( pInstr ){` |
|   7053969 | 1028 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|   5245225 | 1029 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|         - | 1030 | `					sxu32 nQual;` |
|   5245225 | 1031 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 1032 | `					/* Prevent constant expansion but preserve the absolute flag` |
|         - | 1033 | `					 * so the later NEW handler (if any) can see it. */` |
|   5245225 | 1034 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|         - | 1035 | `					/* Namespace-qualify the function name for CALL, unless the` |
|         - | 1036 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|         - | 1037 | `					 * imports — class imports must NOT affect function` |
|         - | 1038 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|         - | 1039 | `					 * before NEW; we store the original literal index in the` |
|         - | 1040 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|         - | 1041 | `					 * the unqualified name and re-qualify with class imports. */` |
|   5245225 | 1042 | `					if( bAbsolute ){` |
|      3921 | 1043 | `						pInstr->iP2 = (sxi32)nOrig;` |
|      1963 | 1044 | `					}else{` |
|   5241309 | 1045 | `						int fromImport = 0;` |
|   5241309 | 1046 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|   5241309 | 1047 | `						pInstr->iP2 = (sxi32)nQual;` |
|   5241309 | 1048 | `						if( nQual != nOrig ){` |
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
|   4431359 | 1071 | `				}else if( (pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */` |
|   1798697 | 1072 | `						&& !(pNode->pLeft && (pNode->pLeft->iFlags & EXPR_NODE_PARENS)))` |
|    914426 | 1073 | `					\|\| pInstr->iOp == PH7_OP_NEW ){` |
|         - | 1074 | `					/* Method call,flag that. But NOT when the callee was an explicitly` |
|         - | 1075 | ``					 * PARENTHESISED member access: `($o->p)(...)` / `($o::$p)(...)` invokes`` |
|         - | 1076 | `					 * the VALUE of the property (php's variable-invocation), so the OP_MEMBER` |
|         - | 1077 | `					 * must stay a plain property READ (leaving the callable on the stack for` |
|         - | 1078 | `					 * OP_CALL to invoke) rather than being rewritten into a method-name` |
|         - | 1079 | ``					 * resolution — the parens are exactly what distinguishes `($o->p)()` from`` |
|         - | 1080 | ``					 * the method call `$o->p()`. */`` |
|   1788657 | 1081 | `					pInstr->iP2 = 1;` |
|         - | 1082 | ``					/* A static call with a DYNAMIC method name (`C::$m(...)`): the`` |
|         - | 1083 | ``					 * static-`::` codegen folded the variable NAME into OP_MEMBER->p3`` |
|         - | 1084 | ``					 * as if it were a static-PROPERTY read (`C::$m`), but in a CALL the`` |
|         - | 1085 | `					 * method name is the variable's VALUE. Rebuild the sequence` |
|         - | 1086 | `					 * [class, OP_LOAD $m -> value, OP_MEMBER(static, method)] so the` |
|         - | 1087 | `					 * dynamic name is read off the stack, matching the instance` |
|         - | 1088 | ``					 * (`$o->$m()`) path. iP1==1 marks a static member. */`` |
|   1788657 | 1089 | `					if( pInstr->iOp == PH7_OP_MEMBER && pInstr->iP1 == 1 && pInstr->p3 ){` |
|        11 | 1090 | `						void *pDynName = pInstr->p3;` |
|        11 | 1091 | `						(void)PH7_VmPopInstr(pGen->pVm);` |
|        11 | 1092 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,pDynName,0);` |
|        11 | 1093 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_MEMBER,1,PH7_MEMBER_METHOD,0,0);` |
|         5 | 1094 | `					}` |
|    894326 | 1095 | `				}` |
|   3526987 | 1096 | `			}` |
|  26058329 | 1097 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|         - | 1098 | `			ph7_expr_node **apNode;` |
|         - | 1099 | `			sxi32 n;` |
|   3024383 | 1100 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|         - | 1101 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|         - | 1102 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE);` |
|         - | 1103 | `			/* Recurse and generate bytecodes for array index */` |
|   3024383 | 1104 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   5811771 | 1105 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   2787393 | 1106 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   2787393 | 1107 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   2787393 | 1108 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 1109 | `					return rc;` |
|         - | 1110 | `				}` |
|         - | 1111 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   2787393 | 1112 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   1393699 | 1113 | `			}` |
|   3024383 | 1114 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   2787393 | 1115 | `				iP1 = 1; /* Node have an index associated with it */` |
|   1393694 | 1116 | `			}` |
|   3024383 | 1117 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|         - | 1118 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|    372569 | 1119 | `				iP2 = 4;` |
|   2838101 | 1120 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|         - | 1121 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|         - | 1122 | `				 * so the trailing unset() builtin can drop the slot. */` |
|     23349 | 1123 | `				iP2 = 5;` |
|   2640147 | 1124 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|         - | 1125 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|         - | 1126 | `				 * short-circuit on missing keys without invoking offsetGet` |
|         - | 1127 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|        29 | 1128 | `				iP2 = 6;` |
|   2628463 | 1129 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|         - | 1130 | `				/* Create an empty entry when the desired index is not found */` |
|    555467 | 1131 | `				iP2 = 1;` |
|    277736 | 1132 | `			}` |
|  21019158 | 1133 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         - | 1134 | `			/* POP the left node */` |
|         5 | 1135 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         2 | 1136 | `		}` |
|  14792653 | 1137 | `	}` |
|  29639417 | 1138 | `	rc = SXRET_OK;` |
|  29639417 | 1139 | `	nJmpIdx = 0;` |
|         - | 1140 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|         - | 1141 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|         - | 1142 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  29639417 | 1143 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|    466425 | 1144 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    466425 | 1145 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    466425 | 1146 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    466425 | 1147 | `			int isSpecial = 0;` |
|    466425 | 1148 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|    373301 | 1149 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|    373301 | 1150 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|    373296 | 1151 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    348007 | 1152 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    188544 | 1153 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|    147563 | 1154 | `					isSpecial = 1;` |
|     73779 | 1155 | `				}` |
|    209929 | 1156 | `			}` |
|    512987 | 1157 | `			pInstr->iP1 = 0;` |
|         - | 1158 | ``			/* A leading `\` (NSSEP) makes the class name ABSOLUTE: it names the`` |
|         - | 1159 | `			 * global class, so it must NOT be re-qualified with the current namespace.` |
|         - | 1160 | ``			 * `\Closure::bind(...)` inside `namespace X` is Closure, not X\Closure.`` |
|         - | 1161 | ``			 * (Multi-component `\A\B::m` already resolves absolutely because its`` |
|         - | 1162 | ``			 * literal keeps a backslash; the single-component `\Closure` lost it.) */`` |
|         - | 1163 | `			{` |
|    722916 | 1164 | `				int bAbsolute = (pNode->pLeft && pNode->pLeft->pStart` |
|    629787 | 1165 | `					&& (pNode->pLeft->pStart->nType & PH7_TK_NSSEP));` |
|    419863 | 1166 | `				if( !isSpecial && !bAbsolute ){` |
|    272287 | 1167 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|    136141 | 1168 | `				}` |
|         - | 1169 | `			}` |
|         - | 1170 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|         - | 1171 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|    419863 | 1172 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|    272305 | 1173 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|    272305 | 1174 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|        78 | 1175 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|        78 | 1176 | `					return SXRET_OK;` |
|         - | 1177 | `				}` |
|    136113 | 1178 | `			}` |
|    209892 | 1179 | `		}` |
|    302989 | 1180 | `	}` |
|         - | 1181 | `	/* Generate code for the right tree */` |
|  29592799 | 1182 | `	if( pNode->pRight ){` |
|  16996155 | 1183 | `		if( iVmOp == PH7_OP_LAND ){` |
|         - | 1184 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    450359 | 1185 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  16770978 | 1186 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|         - | 1187 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    325919 | 1188 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  16382844 | 1189 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|         - | 1190 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     58339 | 1191 | `			iVmOp = 0; /* No binary operator to emit */` |
|     58339 | 1192 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  16190772 | 1193 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|         - | 1194 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|         - | 1195 | `			 * the entire containing postfix chain to null. The jump target is` |
|         - | 1196 | `			 * patched later by the innermost non-chain ancestor (or by` |
|         - | 1197 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|         - | 1198 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|         - | 1199 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|       108 | 1200 | `			sxu32 nNsJmp = 0;` |
|       108 | 1201 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|       108 | 1202 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  16161501 | 1203 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|         - | 1204 | `			/* The lvalue is the RIGHT operand (these ops are right-associative). Mark it a write` |
|         - | 1205 | ``			 * target so a missing member (the base of a subscript-write, or a bare `$o->p`) is`` |
|         - | 1206 | `			 * auto-created — PHP auto-vivifies on write. */` |
|   5212469 | 1207 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|   2606232 | 1208 | `		}` |
|  16996155 | 1209 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  16996155 | 1210 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|  16996155 | 1211 | `		if( !bIsChainOp ){` |
|         - | 1212 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|         - | 1213 | `			 * operator instruction is emitted. */` |
|  11008123 | 1214 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|   5504059 | 1215 | `		}` |
|  16996155 | 1216 | `		if( iVmOp == PH7_OP_STORE ){` |
|   4762275 | 1217 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|   4762238 | 1218 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|         - | 1219 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|         - | 1220 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|         - | 1221 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|         - | 1222 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|         - | 1223 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|         - | 1224 | `				 */` |
|        91 | 1225 | `				iVmOp = 0;` |
|   4762232 | 1226 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|   4762189 | 1227 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 1228 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|    919861 | 1229 | `					iP2 = 1;` |
|    459933 | 1230 | `				}else{` |
|   3842333 | 1231 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 1232 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|    535977 | 1233 | `						iVmOp = PH7_OP_STORE_IDX;` |
|    535977 | 1234 | `						iP1 = pInstr->iP1;` |
|    267991 | 1235 | `					}else{` |
|   3306361 | 1236 | `						p3 = pInstr->p3;` |
|         - | 1237 | `					}` |
|         - | 1238 | `					/* POP the last dynamic load instruction */` |
|   3842333 | 1239 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         - | 1240 | `				}` |
|   2381097 | 1241 | `			}` |
|  14615020 | 1242 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|         - | 1243 | `			/* Peek first: a member LHS ($o->p =& $x, C::$s =& $x) keeps its` |
|         - | 1244 | `			 * OP_MEMBER in place (it resolves + stashes the target slot at` |
|         - | 1245 | `			 * runtime), unlike the variable/array shapes which fold their load` |
|         - | 1246 | `			 * away. Mirrors the normal member-store fold above (iP2 kept-MEMBER). */` |
|        72 | 1247 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|        72 | 1248 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 1249 | `				/* Tag the member as a reference target and flag STORE_REF (iP2=1)` |
|         - | 1250 | `				 * to take the member-rebind path in the VM. */` |
|        11 | 1251 | `				pInstr->iP2 = PH7_MEMBER_REF_TARGET;` |
|        11 | 1252 | `				iP2 = 1;` |
|         6 | 1253 | `			}else{` |
|        62 | 1254 | `				pInstr = PH7_VmPopInstr(pGen->pVm);` |
|        62 | 1255 | `				if( pInstr ){` |
|        62 | 1256 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 1257 | `						/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|         - | 1258 | `						 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|         - | 1259 | `						 */` |
|        19 | 1260 | `						iVmOp = PH7_OP_STORE_IDX_REF;` |
|        19 | 1261 | `						iP1 = pInstr->iP1;` |
|        19 | 1262 | `						iP2 = pInstr->iP2;` |
|        19 | 1263 | `						p3  = pInstr->p3;` |
|        10 | 1264 | `					}else{` |
|        44 | 1265 | `						p3 = pInstr->p3;` |
|         - | 1266 | `					}` |
|        30 | 1267 | `				}` |
|         - | 1268 | `			}` |
|        35 | 1269 | `		}` |
|   8498075 | 1270 | `	}` |
|  29592794 | 1271 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|    432269 | 1272 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|         - | 1273 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|         - | 1274 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|        34 | 1275 | `		iVmOp = 0;` |
|        15 | 1276 | `	}` |
|  29592799 | 1277 | `	if( iVmOp > 0 ){` |
|  29534345 | 1278 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|    217607 | 1279 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|         - | 1280 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|     15545 | 1281 | `				iP1 = 1;` |
|      7775 | 1282 | `			}` |
|  29425544 | 1283 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|         - | 1284 | `			/* Namespace-qualify the class name for NEW */ {` |
|    867763 | 1285 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|    867763 | 1286 | `				VmInstr *pCallInstr = 0;` |
|    867763 | 1287 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|    855929 | 1288 | `					pCallInstr = pPeek;` |
|    855929 | 1289 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    427962 | 1290 | `				}` |
|    867763 | 1291 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|    852245 | 1292 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 1293 | `					sxu32 nLitForClass;` |
|    852245 | 1294 | `					VmCallArgMap *pCallNsMap = pCallInstr ? (VmCallArgMap *)pCallInstr->p3 : 0;` |
|         - | 1295 | `					/* If the CALL handler qualified the name with FUNCTION` |
|         - | 1296 | `					 * imports, recover the original literal (recorded in the` |
|         - | 1297 | `					 * arg map — OP_CALL's iP2 is the hasSpread flag, and` |
|         - | 1298 | `` 					 * misreading it as a literal index made `new C(...$args)` `` |
|         - | 1299 | `					 * fatal with "Class ' ' is not defined") and re-qualify` |
|         - | 1300 | `					 * with class imports. */` |
|    852245 | 1301 | `					if( pCallNsMap && pCallNsMap->nOrigNameLit > 0 ){` |
|        55 | 1302 | `						nLitForClass = pCallNsMap->nOrigNameLit - 1;` |
|        30 | 1303 | `					}else{` |
|    852195 | 1304 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|         - | 1305 | `					}` |
|    852245 | 1306 | `					pPeek->iP1 = 0;` |
|    852245 | 1307 | `					if( !bAbsolute ){` |
|         - | 1308 | `						/* self/static/parent are resolved at runtime against the` |
|         - | 1309 | `						 * current class — never namespace-qualify them (else` |
|         - | 1310 | ``						 * `new self` in namespace N becomes "N\self"). Mirrors the`` |
|         - | 1311 | `						 * instanceof (IS_A) guard below. */` |
|    848339 | 1312 | `						ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nLitForClass);` |
|    848339 | 1313 | `						int isSpecialNew = 0;` |
|    848339 | 1314 | `						if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|    833215 | 1315 | `							const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|    833215 | 1316 | `							sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|    833210 | 1317 | `							if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    836939 | 1318 | `								(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    416554 | 1319 | `								(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      7795 | 1320 | `								isSpecialNew = 1;` |
|      3895 | 1321 | `							}` |
|    420386 | 1322 | `						}` |
|    855901 | 1323 | `						if( isSpecialNew ){` |
|      7795 | 1324 | `							pPeek->iP2 = (sxi32)nLitForClass;` |
|      3900 | 1325 | `						}else{` |
|    832987 | 1326 | `							pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|         - | 1327 | `						}` |
|    420391 | 1328 | `					}else{` |
|      3911 | 1329 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|         - | 1330 | `					}` |
|    422339 | 1331 | `				}` |
|         - | 1332 | `			}` |
|    860201 | 1333 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    860201 | 1334 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|         - | 1335 | `				VmInstr *pPrev;` |
|    855929 | 1336 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|    855929 | 1337 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|         - | 1338 | `					/* Pop the call instruction, preserve named-arg map and` |
|         - | 1339 | `					 * the hasSpread flag (OP_NEW consumes the spread` |
|         - | 1340 | `					 * accumulator exactly like OP_CALL would have). */` |
|    855929 | 1341 | `					iP1 = pInstr->iP1;` |
|    855929 | 1342 | `					iP2 = pInstr->iP2;` |
|    855929 | 1343 | `					if( pInstr->p3 ){` |
|        65 | 1344 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|        30 | 1345 | `					}` |
|    855929 | 1346 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    427962 | 1347 | `				}` |
|    427967 | 1348 | `			}` |
|  28879083 | 1349 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|         - | 1350 | `			/* instanceof: right operand is a class name, not a constant.` |
|         - | 1351 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     81751 | 1352 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     81751 | 1353 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     81751 | 1354 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     81751 | 1355 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     81751 | 1356 | `				int isSpecialIs = 0;` |
|     81751 | 1357 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     81751 | 1358 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     81751 | 1359 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     81746 | 1360 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     81749 | 1361 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     40873 | 1362 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|        12 | 1363 | `						isSpecialIs = 1;` |
|         5 | 1364 | `					}` |
|     40873 | 1365 | `				}` |
|     81751 | 1366 | `				pInstr->iP1 = 0;` |
|     81751 | 1367 | `				if( !isSpecialIs && !bAbsolute ){` |
|     81731 | 1368 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     40863 | 1369 | `				}` |
|     40878 | 1370 | `			}` |
|  28408112 | 1371 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|         - | 1372 | `			/* Prevent constant expansion for member/property names.` |
|         - | 1373 | `			 * The right child (member name) was just compiled — its LOADC` |
|         - | 1374 | `			 * should not trigger constant lookup. */` |
|   5988037 | 1375 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   5988037 | 1376 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|   5747499 | 1377 | `				pInstr->iP1 = 0;` |
|   2873747 | 1378 | `			}` |
|   5988037 | 1379 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|         - | 1380 | `				/* Static member access,remember that */` |
|    419807 | 1381 | `				iP1 = 1;` |
|    419807 | 1382 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    419807 | 1383 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|    236651 | 1384 | `					p3 = pInstr->p3;` |
|    236651 | 1385 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    118323 | 1386 | `				}` |
|    209901 | 1387 | `			}` |
|         - | 1388 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|         - | 1389 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|         - | 1390 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|         - | 1391 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|   5988037 | 1392 | `			if( iP2 == PH7_MEMBER_READ ){` |
|   5988037 | 1393 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|        42 | 1394 | `					iP2 = PH7_MEMBER_UNSET;` |
|   5988017 | 1395 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|     69947 | 1396 | `					iP2 = PH7_MEMBER_ISSET;` |
|   5953026 | 1397 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|        17 | 1398 | `					iP2 = PH7_MEMBER_EMPTY;` |
|   5918047 | 1399 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|         - | 1400 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|   1113947 | 1401 | `					iP2 = PH7_MEMBER_WRITE;` |
|    556971 | 1402 | `				}` |
|   2994016 | 1403 | `			}` |
|   2994016 | 1404 | `		}` |
|         - | 1405 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|         - | 1406 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|         - | 1407 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|         - | 1408 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|         - | 1409 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
|  29526783 | 1410 | `		if( bFcc ){` |
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
|  29526783 | 1432 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|   7914085 | 1433 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|   3957040 | 1434 | `		}` |
|         - | 1435 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  29526783 | 1436 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  14763389 | 1437 | `	}` |
|  29585237 | 1438 | `	if( nJmpIdx > 0 ){` |
|         - | 1439 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|    834607 | 1440 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|    834607 | 1441 | `		if( pInstr ){` |
|    834607 | 1442 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    417301 | 1443 | `		}` |
|    417301 | 1444 | `	}` |
|  29585237 | 1445 | `	return rc;` |
|  37989478 | 1446 | `}` |
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
| 252217380 | 1469 | `static int GenStateTreeHasComma(ph7_expr_node *pNode)` |
|         5 | 1470 | `{` |
|         - | 1471 | `	ph7_expr_node **apArg;` |
|         - | 1472 | `	sxu32 n;` |
| 252217385 | 1473 | `	if( pNode == 0 ){` |
| 177317535 | 1474 | `		return 0;` |
|         - | 1475 | `	}` |
|  74899855 | 1476 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         6 | 1477 | `		return 1;` |
|         - | 1478 | `	}` |
|  74899846 | 1479 | `	if( GenStateTreeHasComma(pNode->pLeft) \|\| GenStateTreeHasComma(pNode->pRight)` |
|  74899847 | 1480 | `	 \|\| GenStateTreeHasComma(pNode->pCond) ){` |
|         6 | 1481 | `		return 1;` |
|         - | 1482 | `	}` |
|  74899847 | 1483 | `	apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  85674173 | 1484 | `	for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; n++ ){` |
|  10774331 | 1485 | `		if( GenStateTreeHasComma(apArg[n]) ){` |
|       ! 0 | 1486 | `			return 1;` |
|         - | 1487 | `		}` |
|   5387168 | 1488 | `	}` |
|  74899847 | 1489 | `	return 0;` |
| 126108695 | 1490 | `}` |
|  17116438 | 1491 | `PH7_PRIVATE sxi32 PH7_CompileExpr(` |
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
|  17116443 | 1505 | `	nExpr = 0;` |
|  17116443 | 1506 | `	pRoot = 0;` |
|         - | 1507 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|         - | 1508 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  17116443 | 1509 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  17116443 | 1510 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  17116443 | 1511 | `	SySetAlloc(&sExprNode,0x10);` |
|  17116443 | 1512 | `	rc = SXRET_OK;` |
|         - | 1513 | `	/* Delimit the expression */` |
|  17116443 | 1514 | `	pEnd = pGen->pIn;` |
|  17116443 | 1515 | `	iNest = 0;` |
| 133994473 | 1516 | `	while( pEnd < pGen->pEnd ){` |
| 127413553 | 1517 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 1518 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|      4719 | 1519 | `			iNest++;` |
| 127411196 | 1520 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|      4729 | 1521 | `			iNest--;` |
| 127406477 | 1522 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  10536393 | 1523 | `			if( iNest <= 0 ){` |
|  10535523 | 1524 | `				break;` |
|         - | 1525 | `			}` |
|       435 | 1526 | `		}` |
| 116878035 | 1527 | `		pEnd++;` |
|         5 | 1528 | `	}` |
|  17116443 | 1529 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|    737919 | 1530 | `		SyToken *pEnd2 = pGen->pIn;` |
|    737919 | 1531 | `		iNest = 0;` |
|         - | 1532 | `		/* Stop at the first comma */` |
|   1620441 | 1533 | `		while( pEnd2 < pEnd ){` |
|    882529 | 1534 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|     50531 | 1535 | `				iNest++;` |
|    857266 | 1536 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|     50531 | 1537 | `				iNest--;` |
|    806740 | 1538 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      6061 | 1539 | `				if( iNest <= 0 ){` |
|         3 | 1540 | `					break;` |
|         - | 1541 | `				}` |
|      3027 | 1542 | `			}` |
|    882527 | 1543 | `			pEnd2++;` |
|         5 | 1544 | `		}` |
|    737919 | 1545 | `		if( pEnd2 <pEnd ){` |
|         3 | 1546 | `			pEnd = pEnd2;` |
|         1 | 1547 | `		}` |
|    368957 | 1548 | `	}` |
|  17116443 | 1549 | `	if( pEnd > pGen->pIn ){` |
|  17093167 | 1550 | `		SyToken *pTmp = pGen->pEnd;` |
|         - | 1551 | `		/* Swap delimiter */` |
|  17093167 | 1552 | `		pGen->pEnd = pEnd;` |
|         - | 1553 | `		/* Try to get an expression tree */` |
|  17093167 | 1554 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  17093162 | 1555 | `		if( rc == SXRET_OK && pRoot && pGen->nCommaExprOk < 1` |
|  16918257 | 1556 | `		 && GenStateTreeHasComma(pRoot) ){` |
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
|  17093163 | 1570 | `		if( rc == SXRET_OK && pRoot ){` |
|  17092981 | 1571 | `			rc = SXRET_OK;` |
|  17092981 | 1572 | `			if( xTreeValidator ){` |
|         - | 1573 | `				/* Call the upper layer validator callback */` |
|   1033453 | 1574 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    516724 | 1575 | `			}` |
|  17092981 | 1576 | `			if( rc != SXERR_ABORT ){` |
|         - | 1577 | `				/* Generate code for the given tree */` |
|  17092981 | 1578 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|         - | 1579 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|         - | 1580 | `				 * expression so they short-circuit to its end. */` |
|  17092981 | 1581 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|   8546488 | 1582 | `			}` |
|  17092981 | 1583 | `			nExpr = 1;` |
|   8546488 | 1584 | `		}` |
|         - | 1585 | `		/* Release the whole tree */` |
|  17093163 | 1586 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|         - | 1587 | `		/* Synchronize token stream */` |
|  17093163 | 1588 | `		pGen->pEnd = pTmp;` |
|  17093163 | 1589 | `		pGen->pIn  = pEnd;` |
|  17093163 | 1590 | `		if( rc == SXERR_ABORT ){` |
|        17 | 1591 | `			SySetRelease(&sExprNode);` |
|        17 | 1592 | `			return SXERR_ABORT;` |
|         - | 1593 | `		}` |
|   8546572 | 1594 | `	}` |
|  17116425 | 1595 | `	SySetRelease(&sExprNode);` |
|  17116425 | 1596 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|   8558224 | 1597 | `}` |
|         - | 1598 | `/*` |
|         - | 1599 | ` * Return a pointer to the node construct handler associated` |
|         - | 1600 | ` * with a given node type [i.e: string,integer,float,...].` |
|         - | 1601 | ` */` |
|   9506828 | 1602 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|         5 | 1603 | `{` |
|   9506833 | 1604 | `	if( nNodeType & PH7_TK_NUM ){` |
|         - | 1605 | `		/* Numeric literal: Either real or integer */` |
|   3823305 | 1606 | `		return PH7_CompileNumLiteral;` |
|   5683533 | 1607 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|         - | 1608 | `		/* Double quoted string */` |
|    124851 | 1609 | `		return PH7_CompileString;` |
|   5558687 | 1610 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|         - | 1611 | `		/* Single quoted string */` |
|   5558563 | 1612 | `		return PH7_CompileSimpleString;` |
|       129 | 1613 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|         - | 1614 | `		/* Heredoc */` |
|        73 | 1615 | `		return PH7_CompileHereDoc;` |
|        61 | 1616 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|         - | 1617 | `		/* Nowdoc */` |
|        56 | 1618 | `		return PH7_CompileNowDoc;` |
|         6 | 1619 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|         - | 1620 | `		/* Backtick quoted string */` |
|         3 | 1621 | `		return PH7_CompileBacktic;` |
|         - | 1622 | `	}` |
|         3 | 1623 | `	return 0;` |
|   4753419 | 1624 | `}` |
|         - | 1625 | `/*` |
|         - | 1626 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|         - | 1627 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|         - | 1628 | ` * in write context" parse error.` |
|         - | 1629 | ` */` |
|     23386 | 1630 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|         5 | 1631 | `{` |
|         - | 1632 | `	sxi32 rc;` |
|     23391 | 1633 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|     23389 | 1634 | `		return SXRET_OK;` |
|         - | 1635 | `	}` |
|         5 | 1636 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|         2 | 1637 | `		pNode ? pNode->pStart->nLine : 1,` |
|         - | 1638 | `		"Can't use nullsafe operator in write context");` |
|         3 | 1639 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|     11698 | 1640 | `}` |
|         - | 1641 | `/*` |
|         - | 1642 | ` * Compile an unset() statement.` |
|         - | 1643 | ` * unset($var, $arr[$key], ...);` |
|         - | 1644 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|         - | 1645 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|         - | 1646 | ` * parent array before extracting the element to unset.` |
|         - | 1647 | ` */` |
|     26096 | 1648 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|         5 | 1649 | `{` |
|     26101 | 1650 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|     26101 | 1651 | `	sxu32 nIdx = 0;` |
|         - | 1652 | `	SyString sName;` |
|         - | 1653 | `	sxi32 rc;` |
|         - | 1654 | `	/* Jump the 'unset' keyword */` |
|     26101 | 1655 | `	pGen->pIn++;` |
|         - | 1656 | `	/* Save delimiter */` |
|     26101 | 1657 | `	pTmp = pGen->pEnd;` |
|         - | 1658 | `	/* Skip optional opening parenthesis and find the matching close */` |
|     26101 | 1659 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|     26101 | 1660 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 1661 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|         - | 1662 | `		SyToken *pClose;` |
|     26101 | 1663 | `		pGen->pIn++;   /* Skip '(' */` |
|     26101 | 1664 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|     26101 | 1665 | `		pEnd = pClose; /* Stop at ')' */` |
|     13048 | 1666 | `	}` |
|     26101 | 1667 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|         - | 1668 | `	/* Resolve the 'unset' builtin name once */` |
|     26101 | 1669 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|      3883 | 1670 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3883 | 1671 | `		if( pObj == 0 ){` |
|       ! 0 | 1672 | `			return SXERR_ABORT;` |
|         - | 1673 | `		}` |
|      3883 | 1674 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      3883 | 1675 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      1939 | 1676 | `	}` |
|         - | 1677 | `	/* Compile each comma-separated argument */` |
|     56077 | 1678 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|     29981 | 1679 | `		if( pGen->pIn < pNext ){` |
|         - | 1680 | `			/*` |
|         - | 1681 | ``			 * A PLAIN variable (`unset($x)`, exactly two tokens: '$' and the name) drops a`` |
|         - | 1682 | `			 * single NAME binding, which the unset() builtin cannot express — all it ever` |
|         - | 1683 | `			 * receives is the value's slot index, and unsetting the slot destroys whatever` |
|         - | 1684 | `			 * else aliases it. Emit OP_UNSET_VAR with the name instead. Subscripts and` |
|         - | 1685 | ``			 * properties (`unset($a[k])`, `unset($o->p)`) keep the existing path, which`` |
|         - | 1686 | `			 * already removes just the element/property.` |
|         - | 1687 | `			 */` |
|     29976 | 1688 | `			if( &pGen->pIn[2] == pNext` |
|     18283 | 1689 | `				&& (pGen->pIn[0].nType & PH7_TK_DOLLAR)` |
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
|     23393 | 1708 | `			pGen->pEnd = pNext;` |
|     23393 | 1709 | `			rc = PH7_CompileExpr(&(*pGen),` |
|         - | 1710 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|         - | 1711 | `				GenStateUnsetValidator);` |
|     23393 | 1712 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 1713 | `				return SXERR_ABORT;` |
|         - | 1714 | `			}` |
|     23393 | 1715 | `			if( rc != SXERR_EMPTY ){` |
|         - | 1716 | `				/* Emit call for this single argument */` |
|     23391 | 1717 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     23391 | 1718 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|     23391 | 1719 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     11693 | 1720 | `			}` |
|     11694 | 1721 | `		}` |
|         - | 1722 | `		/* Jump trailing commas */` |
|     23399 | 1723 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|         7 | 1724 | `			pNext++;` |
|         1 | 1725 | `		}` |
|     23393 | 1726 | `		pGen->pIn = pNext;` |
|         5 | 1727 | `	}` |
|         - | 1728 | `	/* Skip past the closing ')' if present */` |
|     26101 | 1729 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|     26101 | 1730 | `		pGen->pIn++;` |
|     13048 | 1731 | `	}` |
|         - | 1732 | `	/* Restore token stream */` |
|     26101 | 1733 | `	pGen->pEnd = pTmp;` |
|     26101 | 1734 | `	return SXRET_OK;` |
|     13053 | 1735 | `}` |
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
|   8443916 | 1769 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|         - | 1770 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|         - | 1771 | `	SyToken *pLookahed  /* Look-ahead token */` |
|         - | 1772 | `	)` |
|         5 | 1773 | `{` |
|   8443921 | 1774 | `	sxu32 n = 0;` |
|  34067116 | 1775 | `	for(;;){` |
|  68134237 | 1776 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|    529909 | 1777 | `			break;` |
|         - | 1778 | `		}` |
|  67604333 | 1779 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|   7914017 | 1780 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|       ! 0 | 1781 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|       ! 0 | 1782 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|         - | 1783 | `					/* 'static' (class context),return null */` |
|       ! 0 | 1784 | `					return 0;` |
|         - | 1785 | `				}` |
|       ! 0 | 1786 | `			}` |
|   7914012 | 1787 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|     11648 | 1788 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|      5831 | 1789 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|         - | 1790 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|         3 | 1791 | `				return 0;` |
|         - | 1792 | `			}` |
|         - | 1793 | `			/* Return a pointer to the handler.` |
|         - | 1794 | `			*/` |
|   7914015 | 1795 | `			return aLangConstruct[n].xConstruct;` |
|         - | 1796 | `		}` |
|  59690321 | 1797 | `		n++;` |
|         5 | 1798 | `	}` |
|    529909 | 1799 | `	if( pLookahed ){` |
|    529909 | 1800 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|     69933 | 1801 | `			return PH7_CompileClassInterface;` |
|    459981 | 1802 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|    393475 | 1803 | `			return PH7_CompileClass;` |
|     66511 | 1804 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      7859 | 1805 | `			return PH7_CompileTrait;` |
|         - | 1806 | `		}` |
|         - | 1807 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|         - | 1808 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|         - | 1809 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|         - | 1810 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     29326 | 1811 | `	}` |
|         - | 1812 | `	/* Not a language construct */` |
|     58657 | 1813 | `	return 0;` |
|   4221963 | 1814 | `}` |
|         - | 1815 | `/*` |
|         - | 1816 | ` * Check if the given keyword is in fact a PHP language construct.` |
|         - | 1817 | ` * Return TRUE on success. FALSE otheriwse.` |
|         - | 1818 | ` */` |
|     58654 | 1819 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|         5 | 1820 | `{` |
|         - | 1821 | `	int rc;` |
|     58659 | 1822 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     58659 | 1823 | `	if( rc == FALSE ){` |
|     58552 | 1824 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|     15880 | 1825 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|         - | 1826 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|         - | 1827 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|         - | 1828 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|         - | 1829 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|         - | 1830 | `			*/` |
|         - | 1831 | `			){` |
|     58549 | 1832 | `				rc = TRUE;` |
|     29272 | 1833 | `		}` |
|     29276 | 1834 | `	}` |
|     58659 | 1835 | `	return rc;` |
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
|  18125780 | 1850 | `PH7_PRIVATE void GenStateSetPendingDoc(ph7_gen_state *pGen)` |
|         5 | 1851 | `{` |
|  18125785 | 1852 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|  18125785 | 1853 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|  18125785 | 1854 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 1855 | `	sxu32 nIdx, n;` |
|  18125780 | 1856 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|   3415435 | 1857 | `	 \|\| pGen->pIn < pBase \|\| pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|         - | 1858 | `		/* Re-tokenized substream (string interpolation, synthesized code):` |
|         - | 1859 | `		 * indexes do not map to the sidecar */` |
|  14710357 | 1860 | `		return;` |
|         - | 1861 | `	}` |
|   3415433 | 1862 | `	nIdx = (sxu32)(pGen->pIn - pBase);` |
|         - | 1863 | `	/* Attributes must be adjacent to their declaration (unlike docblocks):` |
|         - | 1864 | `	 * reset at every boundary, then collect the groups keyed to this token. */` |
|   3415433 | 1865 | `	SySetReset(&pGen->aPendingAttrs);` |
|  10248049 | 1866 | `	for( n = 0 ; n < nT ; n++ ){` |
|   6832621 | 1867 | `		if( aT[n].nTokIdx != nIdx ){` |
|   6824693 | 1868 | `			continue;` |
|         - | 1869 | `		}` |
|      7933 | 1870 | `		if( aT[n].iKind == PH7_TRIVIA_DOC ){` |
|        29 | 1871 | `			pGen->sPendingDoc = aT[n].sText;` |
|      7921 | 1872 | `		}else if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|      7909 | 1873 | `			SySetPut(&pGen->aPendingAttrs,(const void *)&aT[n]);` |
|      3952 | 1874 | `		}` |
|      3969 | 1875 | `	}` |
|   9062895 | 1876 | `}` |
|         - | 1877 | `/*` |
|         - | 1878 | ` * Hand the pending docblock (if any) to a declaration: duplicate it into` |
|         - | 1879 | ` * the VM allocator (the raw script buffer dies after compilation) and` |
|         - | 1880 | ` * clear the pending slot so sibling declarations do not inherit it.` |
|         - | 1881 | ` */` |
|   4874202 | 1882 | `PH7_PRIVATE void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut)` |
|         5 | 1883 | `{` |
|         - | 1884 | `	char *zDup;` |
|   4874207 | 1885 | `	if( SyStringLength(&pGen->sPendingDoc) < 1 ){` |
|   4874187 | 1886 | `		return;` |
|         - | 1887 | `	}` |
|        35 | 1888 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        10 | 1889 | `		SyStringData(&pGen->sPendingDoc),SyStringLength(&pGen->sPendingDoc));` |
|        25 | 1890 | `	if( zDup ){` |
|        25 | 1891 | `		SyStringInitFromBuf(pOut,zDup,SyStringLength(&pGen->sPendingDoc));` |
|        10 | 1892 | `	}` |
|        25 | 1893 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|   2437106 | 1894 | `}` |
|         - | 1895 | `/*` |
|         - | 1896 | ` * Compile one recorded #[...] attribute group (the span between the group` |
|         - | 1897 | ` * delimiters) into ph7_attribute records appended to pOut. The span is` |
|         - | 1898 | ` * duplicated into the VM allocator FIRST (compiled bytecode and interned` |
|         - | 1899 | ` * names may point into the token text, which must outlive the raw script` |
|         - | 1900 | ` * buffer), then re-tokenized on its own. Each argument expression compiles` |
|         - | 1901 | ` * with the container-swap idiom into its own OP_DONE-terminated set,` |
|         - | 1902 | ` * evaluated lazily at ReflectionAttribute time (PHP semantics).` |
|         - | 1903 | ` */` |
|      7918 | 1904 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut)` |
|         5 | 1905 | `{` |
|         - | 1906 | `	SySet *pToken;` |
|         - | 1907 | `	SyToken *pIn, *pEnd, *pSavedIn, *pSavedEnd;` |
|         - | 1908 | `	char *zSpan;` |
|      7923 | 1909 | `	sxi32 rc = SXRET_OK;` |
|      7923 | 1910 | `	if( SyStringLength(&pTrivia->sText) < 1 ){` |
|       ! 0 | 1911 | `		return SXRET_OK;` |
|         - | 1912 | `	}` |
|     11882 | 1913 | `	zSpan = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      3959 | 1914 | `		SyStringData(&pTrivia->sText),SyStringLength(&pTrivia->sText));` |
|      7923 | 1915 | `	if( zSpan == 0 ){` |
|       ! 0 | 1916 | `		return SXRET_OK;` |
|         - | 1917 | `	}` |
|         - | 1918 | `	/* The token set must outlive compilation too: interned operands may` |
|         - | 1919 | `	 * reference token payloads. Pool-allocated, never released — bounded by` |
|         - | 1920 | `	 * the number of attribute declarations in the program. */` |
|      7923 | 1921 | `	pToken = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      7923 | 1922 | `	if( pToken == 0 ){` |
|       ! 0 | 1923 | `		return SXRET_OK;` |
|         - | 1924 | `	}` |
|      7923 | 1925 | `	SySetInit(pToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      7923 | 1926 | `	PH7_TokenizePHP(zSpan,SyStringLength(&pTrivia->sText),pTrivia->nLine,pToken,0);` |
|      7923 | 1927 | `	pIn = (SyToken *)SySetBasePtr(pToken);` |
|      7923 | 1928 | `	pEnd = &pIn[SySetUsed(pToken)];` |
|      7923 | 1929 | `	pSavedIn = pGen->pIn;` |
|      7923 | 1930 | `	pSavedEnd = pGen->pEnd;` |
|      7927 | 1931 | `	while( pIn < pEnd ){` |
|         - | 1932 | `		ph7_attribute sAttr;` |
|         - | 1933 | `		SyBlob sFQN;` |
|      7927 | 1934 | `		int bAbsolute = 0;` |
|      7927 | 1935 | `		SyZero(&sAttr,sizeof(sAttr));` |
|      7927 | 1936 | `		SySetInit(&sAttr.aArgs,&pGen->pVm->sAllocator,sizeof(ph7_attr_arg));` |
|      7927 | 1937 | `		sAttr.nLine = pIn->nLine;` |
|      7927 | 1938 | `		if( pIn->nType & PH7_TK_NSSEP ){` |
|        75 | 1939 | `			bAbsolute = 1;` |
|        75 | 1940 | `			pIn++;` |
|        35 | 1941 | `		}` |
|      7927 | 1942 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      7927 | 1943 | `		while( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      7927 | 1944 | `			SyBlobAppend(&sFQN,pIn->sData.zString,pIn->sData.nByte);` |
|      7927 | 1945 | `			pIn++;` |
|      7927 | 1946 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       ! 0 | 1947 | `				SyBlobAppend(&sFQN,"\\",1);` |
|       ! 0 | 1948 | `				pIn++;` |
|       ! 0 | 1949 | `				continue;` |
|         - | 1950 | `			}` |
|      7927 | 1951 | `			break;` |
|       ! 0 | 1952 | `		}` |
|      7927 | 1953 | `		if( SyBlobLength(&sFQN) < 1 ){` |
|         - | 1954 | `			/* Malformed group: stop quietly (the group was inert trivia before` |
|         - | 1955 | `			 * this feature; never turn it into a new fatal) */` |
|       ! 0 | 1956 | `			SyBlobRelease(&sFQN);` |
|       ! 0 | 1957 | `			break;` |
|         - | 1958 | `		}` |
|         - | 1959 | `		/* Resolve to an FQN: absolute names verbatim; else use-import alias,` |
|         - | 1960 | `		 * else current-namespace prefix (PHP attribute name resolution) */` |
|         - | 1961 | `		{` |
|      7927 | 1962 | `			const char *zName = (const char *)SyBlobData(&sFQN);` |
|      7927 | 1963 | `			sxu32 nName = SyBlobLength(&sFQN);` |
|      7927 | 1964 | `			char *zDup = 0;` |
|      7927 | 1965 | `			if( !bAbsolute ){` |
|      7857 | 1966 | `				SyHashEntry *pImp = SyHashGet(&pGen->hUseImports,(const void *)zName,nName);` |
|      7857 | 1967 | `				if( pImp ){` |
|       ! 0 | 1968 | `					const char *zFqn = (const char *)pImp->pUserData;` |
|       ! 0 | 1969 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFqn,SyStrlen(zFqn));` |
|       ! 0 | 1970 | `					if( zDup ){` |
|       ! 0 | 1971 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyStrlen(zDup));` |
|       ! 0 | 1972 | `					}` |
|      7857 | 1973 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
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
|      3926 | 1986 | `			}` |
|      7927 | 1987 | `			if( SyStringLength(&sAttr.sName) < 1 ){` |
|      7927 | 1988 | `				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|      7927 | 1989 | `				if( zDup ){` |
|      7927 | 1990 | `					SyStringInitFromBuf(&sAttr.sName,zDup,nName);` |
|      3961 | 1991 | `				}` |
|      3961 | 1992 | `			}` |
|         - | 1993 | `		}` |
|      7927 | 1994 | `		SyBlobRelease(&sFQN);` |
|      7927 | 1995 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 1996 | `			SyToken *pArgsEnd;` |
|      7825 | 1997 | `			pIn++;` |
|      7825 | 1998 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pArgsEnd);` |
|     15659 | 1999 | `			while( pIn < pArgsEnd ){` |
|      7839 | 2000 | `				SyToken *pArgStart = pIn, *pArgStop = pIn;` |
|      7839 | 2001 | `				sxi32 iDepth = 0;` |
|         - | 2002 | `				ph7_attr_arg sArgRec;` |
|     77837 | 2003 | `				while( pArgStop < pArgsEnd ){` |
|     70019 | 2004 | `					if( pArgStop->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        11 | 2005 | `						iDepth++;` |
|     70014 | 2006 | `					}else if( pArgStop->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        11 | 2007 | `						iDepth--;` |
|     70004 | 2008 | `					}else if( (pArgStop->nType & PH7_TK_COMMA) && iDepth == 0 ){` |
|        17 | 2009 | `						break;` |
|         - | 2010 | `					}` |
|     70003 | 2011 | `					pArgStop++;` |
|         5 | 2012 | `				}` |
|      7839 | 2013 | `				SyZero(&sArgRec,sizeof(sArgRec));` |
|      7839 | 2014 | `				SySetInit(&sArgRec.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      7834 | 2015 | `				if( pArgStart < pArgStop && (pArgStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|      7816 | 2016 | `				 && &pArgStart[1] < pArgStop && (pArgStart[1].nType & PH7_TK_COLON) ){` |
|        28 | 2017 | `					char *zN = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|         9 | 2018 | `						pArgStart->sData.zString,pArgStart->sData.nByte);` |
|        19 | 2019 | `					if( zN ){` |
|        19 | 2020 | `						SyStringInitFromBuf(&sArgRec.sName,zN,pArgStart->sData.nByte);` |
|         9 | 2021 | `					}` |
|        19 | 2022 | `					pArgStart += 2;` |
|         9 | 2023 | `				}` |
|      7839 | 2024 | `				if( pArgStart < pArgStop ){` |
|         - | 2025 | `					SySet *pInstrContainer;` |
|      7839 | 2026 | `					pGen->pIn = pArgStart;` |
|      7839 | 2027 | `					pGen->pEnd = pArgStop;` |
|      7839 | 2028 | `					pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      7839 | 2029 | `					PH7_VmSetByteCodeContainer(pGen->pVm,&sArgRec.aByteCode);` |
|      7839 | 2030 | `					rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      7839 | 2031 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      7839 | 2032 | `					PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      7839 | 2033 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 2034 | `						pGen->pIn = pSavedIn;` |
|       ! 0 | 2035 | `						pGen->pEnd = pSavedEnd;` |
|       ! 0 | 2036 | `						return SXERR_ABORT;` |
|         - | 2037 | `					}` |
|      7839 | 2038 | `					SySetPut(&sAttr.aArgs,(const void *)&sArgRec);` |
|      3917 | 2039 | `				}` |
|      7839 | 2040 | `				pIn = pArgStop;` |
|      7839 | 2041 | `				if( pIn < pArgsEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        17 | 2042 | `					pIn++;` |
|         8 | 2043 | `				}` |
|         5 | 2044 | `			}` |
|      7825 | 2045 | `			pIn = (pArgsEnd < pEnd) ? &pArgsEnd[1] : pEnd;` |
|      3910 | 2046 | `		}` |
|      7927 | 2047 | `		SySetPut(pOut,(const void *)&sAttr);` |
|      7927 | 2048 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|         5 | 2049 | `			pIn++;` |
|         5 | 2050 | `			continue;` |
|         - | 2051 | `		}` |
|      7923 | 2052 | `		break;` |
|       ! 0 | 2053 | `	}` |
|      7923 | 2054 | `	pGen->pIn = pSavedIn;` |
|      7923 | 2055 | `	pGen->pEnd = pSavedEnd;` |
|      7923 | 2056 | `	return SXRET_OK;` |
|      3964 | 2057 | `}` |
|         - | 2058 | `/*` |
|         - | 2059 | ` * Hand the pending attribute groups (if any) to a declaration: compile` |
|         - | 2060 | ` * every recorded group into pOut and clear the pending list.` |
|         - | 2061 | ` */` |
|   4874206 | 2062 | `PH7_PRIVATE sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut)` |
|         5 | 2063 | `{` |
|   4874211 | 2064 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|         - | 2065 | `	sxu32 n;` |
|         - | 2066 | `	sxi32 rc;` |
|   4882115 | 2067 | `	for( n = 0 ; n < SySetUsed(&pGen->aPendingAttrs) ; n++ ){` |
|      7909 | 2068 | `		rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|      7909 | 2069 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 2070 | `			return SXERR_ABORT;` |
|         - | 2071 | `		}` |
|      3957 | 2072 | `	}` |
|   4874211 | 2073 | `	SySetReset(&pGen->aPendingAttrs);` |
|   4874211 | 2074 | `	return SXRET_OK;` |
|   2437108 | 2075 | `}` |
|         - | 2076 | `/*` |
|         - | 2077 | ` * Compile the attribute groups keyed to the given token (a parameter's` |
|         - | 2078 | ` * first token inside a signature) into pOut. Parameters are parsed from` |
|         - | 2079 | ` * the main token stream, so the sidecar indexes map directly.` |
|         - | 2080 | ` */` |
|   2454656 | 2081 | `PH7_PRIVATE sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut)` |
|         5 | 2082 | `{` |
|   2454661 | 2083 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|   2454661 | 2084 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|   2454661 | 2085 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 2086 | `	sxu32 nIdx, n;` |
|         - | 2087 | `	sxi32 rc;` |
|   2454656 | 2088 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|    554855 | 2089 | `	 \|\| pTok < pBase \|\| pTok >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|   1899811 | 2090 | `		return SXRET_OK;` |
|         - | 2091 | `	}` |
|    554855 | 2092 | `	nIdx = (sxu32)(pTok - pBase);` |
|   1664599 | 2093 | `	for( n = 0 ; n < nT ; n++ ){` |
|   1109749 | 2094 | `		if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|        16 | 2095 | `			rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|        16 | 2096 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 2097 | `				return SXERR_ABORT;` |
|         - | 2098 | `			}` |
|         7 | 2099 | `		}` |
|    554877 | 2100 | `	}` |
|    554855 | 2101 | `	return SXRET_OK;` |
|   1227333 | 2102 | `}` |
|  13301952 | 2103 | `PH7_PRIVATE sxi32 GenStateCompileChunk(` |
|         - | 2104 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 2105 | `	sxi32 iFlags         /* Compile flags */` |
|         - | 2106 | `	)` |
|         5 | 2107 | `{` |
|         - | 2108 | `	ProcLangConstruct xCons;` |
|         - | 2109 | `	sxi32 rc;` |
|  13301957 | 2110 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|   7727616 | 2111 | `	for(;;){` |
|  14378597 | 2112 | `		int bStmtIsDeclare = 0;` |
|  14378597 | 2113 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 2114 | `			/* No more input to process */` |
|     91387 | 2115 | `			break;` |
|         - | 2116 | `		}` |
|         - | 2117 | `		/* Bind a directly-preceding docblock to this statement */` |
|  14287215 | 2118 | `		GenStateSetPendingDoc(&(*pGen));` |
|  14287215 | 2119 | `		if( SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|         - | 2120 | `			/* php: a statement-position attribute group must be followed by a` |
|         - | 2121 | ``			 * declaration (function/class-like/const) — `#[A] $x = 1;` is a`` |
|         - | 2122 | `` 			 * parse error, never a silent discard. `static`/`fn`/`function` `` |
|         - | 2123 | ``			 * cover bare closure-expression statements; `readonly`/`enum` are`` |
|         - | 2124 | `			 * context-sensitive IDs handled by the modified-class/enum scans. */` |
|      7821 | 2125 | `			int bAttrTarget = 0;` |
|      7816 | 2126 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd)` |
|      3943 | 2127 | `			 \|\| GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|      7761 | 2128 | `				bAttrTarget = 1;` |
|      3939 | 2129 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|        61 | 2130 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        60 | 2131 | `				if( nKw == PH7_TKWRD_FUNCTION \|\| nKw == PH7_TKWRD_CLASS` |
|        16 | 2132 | `				 \|\| nKw == PH7_TKWRD_INTERFACE \|\| nKw == PH7_TKWRD_TRAIT` |
|         4 | 2133 | `				 \|\| nKw == PH7_TKWRD_ABSTRACT \|\| nKw == PH7_TKWRD_FINAL` |
|         4 | 2134 | `				 \|\| nKw == PH7_TKWRD_CONST \|\| nKw == PH7_TKWRD_STATIC` |
|         1 | 2135 | `				 \|\| nKw == PH7_TKWRD_FN ){` |
|        61 | 2136 | `					bAttrTarget = 1;` |
|        30 | 2137 | `				}` |
|        30 | 2138 | `			}` |
|      7821 | 2139 | `			if( !bAttrTarget ){` |
|       ! 0 | 2140 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 2141 | `					"syntax error, unexpected token \"%z\" after attribute group; expecting a declaration",` |
|       ! 0 | 2142 | `					&pGen->pIn->sData);` |
|       ! 0 | 2143 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 2144 | `					break;` |
|         - | 2145 | `				}` |
|       ! 0 | 2146 | `				SySetReset(&pGen->aPendingAttrs);` |
|       ! 0 | 2147 | `			}` |
|      3908 | 2148 | `		}` |
|         - | 2149 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|         - | 2150 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  14287215 | 2151 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   8486649 | 2152 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   8486649 | 2153 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|        47 | 2154 | `				bStmtIsDeclare = 1;` |
|        21 | 2155 | `			}` |
|   4243322 | 2156 | `		}` |
|  14287215 | 2157 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|         - | 2158 | `			/* Any non-declare top-level statement locks the strict_types` |
|         - | 2159 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|   1076633 | 2160 | `			pGen->bStrictTypesLocked = 1;` |
|    538314 | 2161 | `		}` |
|  14287215 | 2162 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 2163 | `			/* Compile block */` |
|      3923 | 2164 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      3923 | 2165 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 2166 | `				break;` |
|         - | 2167 | `			}` |
|      1964 | 2168 | `		}else{` |
|  14283297 | 2169 | `			xCons = 0;` |
|  14283297 | 2170 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|         - | 2171 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|         - | 2172 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|         - | 2173 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|     42759 | 2174 | `				xCons = PH7_CompileClassModifiers;` |
|  14261920 | 2175 | `			}else if( GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|         - | 2176 | ``				/* `enum Name …` (PHP 8.1) — `enum` is a context-sensitive ID,`` |
|         - | 2177 | `				 * so it is detected here rather than the keyword dispatcher. */` |
|      3919 | 2178 | `				xCons = PH7_CompileEnum;` |
|  14238586 | 2179 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   8443921 | 2180 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         - | 2181 | `				/* Try to extract a language construct handler */` |
|   8443921 | 2182 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|   8443921 | 2183 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
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
|  10014671 | 2195 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|    427759 | 2196 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|         - | 2197 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|       117 | 2198 | `				xCons = PH7_CompileLabel;` |
|        56 | 2199 | `			}` |
|  14283297 | 2200 | `			if( xCons == 0 ){` |
|         - | 2201 | `				/* Assume an expression an try to compile it */` |
|   5851247 | 2202 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   5851247 | 2203 | `				if(  rc != SXERR_EMPTY ){` |
|         - | 2204 | `					/* Pop l-value */` |
|   5851097 | 2205 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|   2925546 | 2206 | `				}` |
|   2925626 | 2207 | `			}else{` |
|         - | 2208 | `				/* Go compile the sucker */` |
|   8432055 | 2209 | `				rc = xCons(&(*pGen));` |
|         - | 2210 | `			}` |
|  14283297 | 2211 | `			if( rc == SXERR_ABORT ){` |
|         - | 2212 | `				/* Request to abort compilation */` |
|        34 | 2213 | `				break;` |
|         - | 2214 | `			}` |
|         - | 2215 | `		}` |
|         - | 2216 | `		/* Ignore trailing semi-colons ';' */` |
|  24536733 | 2217 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  10249553 | 2218 | `			pGen->pIn++;` |
|         5 | 2219 | `		}` |
|  14287185 | 2220 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|         - | 2221 | `			/* Compile a single statement and return */` |
|  13210545 | 2222 | `			break;` |
|         - | 2223 | `		}` |
|         - | 2224 | `		/* LOOP ONE */` |
|         - | 2225 | `		/* LOOP TWO */` |
|         - | 2226 | `		/* LOOP THREE */` |
|         - | 2227 | `		/* LOOP FOUR */` |
|         5 | 2228 | `	}` |
|         - | 2229 | `	/* Return compilation status */` |
|  13301957 | 2230 | `	return rc;` |
|         5 | 2231 | `}` |
|         - | 2232 | `/*` |
|         - | 2233 | ` * Compile a Raw PHP chunk.` |
|         - | 2234 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 2235 | ` * takes care of generating the appropriate error message.` |
|         - | 2236 | ` */` |
|     91414 | 2237 | `static sxi32 PH7_CompilePHP(` |
|         - | 2238 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 2239 | `	SySet *pTokenSet,     /* Token set */` |
|         - | 2240 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|         - | 2241 | `	)` |
|         5 | 2242 | `{` |
|     91419 | 2243 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|         - | 2244 | `	sxi32 rc;` |
|         - | 2245 | `	/* Reset the token set (and its trivia sidecar) */` |
|     91419 | 2246 | `	SySetReset(&(*pTokenSet));` |
|     91419 | 2247 | `	SySetReset(&pGen->aTrivia);` |
|         - | 2248 | `	/* Mark as the default token set */` |
|     91419 | 2249 | `	pGen->pTokenSet = &(*pTokenSet);` |
|         - | 2250 | `	/* Advance the stream cursor */` |
|     91419 | 2251 | `	pGen->pRawIn++;` |
|         - | 2252 | `	/* Tokenize the PHP chunk first */` |
|     91419 | 2253 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet),&pGen->aTrivia);` |
|         - | 2254 | `	/* Point to the head and tail of the token stream. */` |
|     91419 | 2255 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     91419 | 2256 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|     91419 | 2257 | `	if( is_expr ){` |
|       ! 0 | 2258 | `		rc = SXERR_EMPTY;` |
|       ! 0 | 2259 | `		if( pGen->pIn < pGen->pEnd ){` |
|         - | 2260 | `			/* A simple expression,compile it */` |
|       ! 0 | 2261 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|       ! 0 | 2262 | `		}` |
|         - | 2263 | `		/* Emit the DONE instruction */` |
|       ! 0 | 2264 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       ! 0 | 2265 | `		return SXRET_OK;` |
|         - | 2266 | `	}` |
|     91419 | 2267 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
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
|     91417 | 2292 | `	rc = GenStateCompileChunk(pGen,0);` |
|         - | 2293 | `	/* Fix exceptions jumps */` |
|     91417 | 2294 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - | 2295 | `	/* Fix gotos now, the jump destination is resolved */` |
|     91417 | 2296 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|         3 | 2297 | `		rc = SXERR_ABORT;` |
|         1 | 2298 | `	}` |
|         - | 2299 | `	/* Reset container */` |
|     91417 | 2300 | `	SySetReset(&pGen->aGoto);` |
|     91417 | 2301 | `	SySetReset(&pGen->aLabel);` |
|     91417 | 2302 | `	SySetReset(&pGen->aNullsafeJmp);` |
|         - | 2303 | `	/* Compilation result */` |
|     91417 | 2304 | `	return rc;` |
|     45712 | 2305 | `}` |
|         - | 2306 | `/*` |
|         - | 2307 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|         - | 2308 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|         - | 2309 | ` * This is the only compile interface exported from this file.` |
|         - | 2310 | ` */` |
|     94564 | 2311 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
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
|     94569 | 2327 | `	sxu32 nBaseLine = 1;` |
|     94569 | 2328 | `	if( pScript->nByte < 1 ){` |
|         - | 2329 | `		/* Nothing to compile */` |
|       ! 0 | 2330 | `		return PH7_OK;` |
|         - | 2331 | `	}` |
|         - | 2332 | `	/* php skips a "#!" shebang on the first line of a CLI script: consume it` |
|         - | 2333 | `	 * (including its newline) so it is not echoed as inline text, and bump the` |
|         - | 2334 | `	 * base line to 2 so the code below still reports php-matching line numbers. */` |
|     94569 | 2335 | `	if( pScript->nByte >= 2 && pScript->zString[0]=='#' && pScript->zString[1]=='!' ){` |
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
|     94569 | 2349 | `	pCodeGen = &pVm->sCodeGen;` |
|         - | 2350 | ``	/* aPhpToken below is a LOCAL token set: once it is released at `cleanup`, any`` |
|         - | 2351 | `	 * pGen->pIn/pEnd still pointing into it DANGLE. PH7_VmEmitInstr reads pIn to stamp` |
|         - | 2352 | `	 * each instruction's source line, and instructions are still emitted after this` |
|         - | 2353 | `	 * function returns (VmEvalChunk's trailing OP_DONE) -- a use-after-free ASan caught` |
|         - | 2354 | `	 * immediately. Save the caller's cursor and restore it on the way out, so an` |
|         - | 2355 | `	 * enclosing compile keeps its (live) tokens and the top level goes back to NULL. */` |
|     94569 | 2356 | `	pSavedIn = pCodeGen->pIn;` |
|     94569 | 2357 | `	pSavedEnd = pCodeGen->pEnd;` |
|     94569 | 2358 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|     94569 | 2359 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|     94569 | 2360 | `	pCodeGen->bStrictTypes = 0;` |
|     94569 | 2361 | `	pCodeGen->bStrictTypesLocked = 0;` |
|         - | 2362 | `	/* Initialize the tokens containers */` |
|     94569 | 2363 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|     94569 | 2364 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|     94569 | 2365 | `	SySetAlloc(&aPhpToken,0xc0);` |
|     94569 | 2366 | `	is_expr = 0;` |
|     94569 | 2367 | `	if( iFlags & PH7_PHP_ONLY ){` |
|         - | 2368 | `		SyToken sTmp;` |
|         - | 2369 | `		/* PHP only: -*/` |
|     81563 | 2370 | `		sTmp.nLine = 1;` |
|     81563 | 2371 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|     81563 | 2372 | `		sTmp.pUserData = 0;` |
|     81563 | 2373 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|     81563 | 2374 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|     81563 | 2375 | `		if( iFlags & PH7_PHP_EXPR ){` |
|         - | 2376 | `			/* A simple PHP expression */` |
|       ! 0 | 2377 | `			is_expr = 1;` |
|       ! 0 | 2378 | `		}` |
|     40784 | 2379 | `	}else{` |
|         - | 2380 | `		/* Tokenize raw text */` |
|     13011 | 2381 | `		SySetAlloc(&aRawToken,32);` |
|     13011 | 2382 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken,nBaseLine);` |
|         - | 2383 | `	}` |
|         - | 2384 | `	/* Process high-level tokens */` |
|     94569 | 2385 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|     94569 | 2386 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|     94569 | 2387 | `	rc = PH7_OK;` |
|     94569 | 2388 | `	if( is_expr ){` |
|         - | 2389 | `		/* Compile the expression */` |
|       ! 0 | 2390 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|       ! 0 | 2391 | `		goto cleanup;` |
|         - | 2392 | `	}` |
|     94569 | 2393 | `	nObjIdx = 0;` |
|         - | 2394 | `	/* Each compilation unit starts in the global namespace.` |
|         - | 2395 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|         - | 2396 | `	 * preventing namespace bleeding across include()d files. */` |
|     94569 | 2397 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|         - | 2398 | `	/* Start the compilation process */` |
|     53790 | 2399 | `	for(;;){` |
|    198967 | 2400 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|     94537 | 2401 | `			break; /* No more tokens to process */` |
|         - | 2402 | `		}` |
|    104435 | 2403 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|         - | 2404 | `			/* Compile the PHP chunk */` |
|     91419 | 2405 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|     91419 | 2406 | `			if( rc == SXERR_ABORT ){` |
|        36 | 2407 | `				break;` |
|         - | 2408 | `			}` |
|     91387 | 2409 | `			continue;` |
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
|     47287 | 2430 | `	}` |
|     47282 | 2431 | `cleanup:` |
|         - | 2432 | `	/* Drop the cursor into the token set BEFORE the set is freed (see above). */` |
|     94569 | 2433 | `	pCodeGen->pIn = pSavedIn;` |
|     94569 | 2434 | `	pCodeGen->pEnd = pSavedEnd;` |
|     94569 | 2435 | `	SySetRelease(&aRawToken);` |
|     94569 | 2436 | `	SySetRelease(&aPhpToken);` |
|         - | 2437 | `	/* Restore outer file's strict_types scope */` |
|     94569 | 2438 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|     94569 | 2439 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|     94569 | 2440 | `	return rc;` |
|     47287 | 2441 | `}` |
|         - | 2442 | `/*` |
|         - | 2443 | ` * Utility routines.Initialize the code generator.` |
|         - | 2444 | ` */` |
|      3878 | 2445 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|         - | 2446 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 2447 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 2448 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 2449 | `	)` |
|         5 | 2450 | `{` |
|      3883 | 2451 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 2452 | `	/* Zero the structure */` |
|      3883 | 2453 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|         - | 2454 | `	/* Initial state */` |
|      3883 | 2455 | `	pGen->pVm  = &(*pVm);` |
|      3883 | 2456 | `	pGen->xErr = xErr;` |
|      3883 | 2457 | `	pGen->pErrData = pErrData;` |
|      3883 | 2458 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|      3883 | 2459 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|      3883 | 2460 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|      3883 | 2461 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|      3883 | 2462 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|      3883 | 2463 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3883 | 2464 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3883 | 2465 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|      3883 | 2466 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|         - | 2467 | `	/* Error log buffer */` |
|      3883 | 2468 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|         - | 2469 | `	/* General purpose working buffer */` |
|      3883 | 2470 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|         - | 2471 | `	/* Namespace state */` |
|      3883 | 2472 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|      3883 | 2473 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|      3883 | 2474 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|      3883 | 2475 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 2476 | `	/* Create the global scope */` |
|      3883 | 2477 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|         - | 2478 | `	/* Point to the global scope */` |
|      3883 | 2479 | `	pGen->pCurrent = &pGen->sGlobal;` |
|      3883 | 2480 | `	return SXRET_OK;` |
|         5 | 2481 | `}` |
|         - | 2482 | `/*` |
|         - | 2483 | ` * Utility routines. Reset the code generator to it's initial state.` |
|         - | 2484 | ` */` |
|     97968 | 2485 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|         - | 2486 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 2487 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 2488 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 2489 | `	)` |
|         5 | 2490 | `{` |
|     97973 | 2491 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 2492 | `	GenBlock *pBlock,*pParent;` |
|         - | 2493 | `	/* Reset state */` |
|     97973 | 2494 | `	SySetReset(&pGen->aLabel);` |
|     97973 | 2495 | `	SySetReset(&pGen->aGoto);` |
|     97973 | 2496 | `	SySetReset(&pGen->aNullsafeJmp);` |
|     97973 | 2497 | `	SySetReset(&pGen->aTrivia);` |
|     97973 | 2498 | `	SySetReset(&pGen->aPendingAttrs);` |
|     97973 | 2499 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|     97973 | 2500 | `	SyBlobRelease(&pGen->sErrBuf);` |
|     97973 | 2501 | `	SyBlobRelease(&pGen->sWorker);` |
|     97973 | 2502 | `	SyBlobRelease(&pGen->sNamespace);` |
|     97973 | 2503 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|     97973 | 2504 | `	SyHashRelease(&pGen->hUseImports);` |
|     97973 | 2505 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|     97973 | 2506 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     97973 | 2507 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|     97973 | 2508 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     97973 | 2509 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 2510 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|         - | 2511 | `	 * They intern variable names and literal strings that are referenced by` |
|         - | 2512 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|         - | 2513 | `	 * Releasing them would either leak the interned strings or require freeing` |
|         - | 2514 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|         - | 2515 | `	 * number of unique names, which is acceptable. */` |
|         - | 2516 | `	/* Point to the global scope */` |
|     97973 | 2517 | `	pBlock = pGen->pCurrent;` |
|     97973 | 2518 | `	while( pBlock->pParent != 0 ){` |
|       ! 0 | 2519 | `		pParent = pBlock->pParent;` |
|       ! 0 | 2520 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 2521 | `		pBlock = pParent;` |
|       ! 0 | 2522 | `	}` |
|     97973 | 2523 | `	pGen->xErr = xErr;` |
|     97973 | 2524 | `	pGen->pErrData = pErrData;` |
|     97973 | 2525 | `	pGen->pCurrent = &pGen->sGlobal;` |
|     97973 | 2526 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|     97973 | 2527 | `	pGen->pIn = pGen->pEnd = 0;` |
|     97973 | 2528 | `	pGen->nErr = 0;` |
|     97973 | 2529 | `	return SXRET_OK;` |
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
|       180 | 2648 | `PH7_PRIVATE sxi32 PH7_GenSyntaxError(` |
|         - | 2649 | `	ph7_gen_state *pGen,   /* Code generator state */` |
|         - | 2650 | `	SyToken *pTok,         /* Offending token, or NULL for end of file */` |
|         - | 2651 | `	const char *zExpecting /* ", expecting <this>" tail, or NULL */` |
|         - | 2652 | `	)` |
|         5 | 2653 | `{` |
|       185 | 2654 | `	const char *zNoun = "token";` |
|         - | 2655 | `	sxu32 nLine;` |
|       185 | 2656 | `	if( pTok == 0 && pGen->pTokenSet ){` |
|         - | 2657 | `		/* The caller ran out of tokens inside its own slice — but a statement's slice stops` |
|         - | 2658 | `		 * BEFORE its terminator, so the token php actually names (typically the ';') is the` |
|         - | 2659 | `		 * one sitting just past the slice, still inside the chunk's token stream. Reach for` |
|         - | 2660 | `		 * it before concluding "end of file". */` |
|        92 | 2661 | `		SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        92 | 2662 | `		SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|        92 | 2663 | `		if( pGen->pEnd >= pBase && pGen->pEnd < pStreamEnd ){` |
|        92 | 2664 | `			pTok = pGen->pEnd;` |
|        44 | 2665 | `		}` |
|        44 | 2666 | `	}` |
|       185 | 2667 | `	nLine = pTok ? pTok->nLine : (pGen->pIn > (SyToken *)SySetBasePtr(pGen->pTokenSet) ? pGen->pIn[-1].nLine : 1);` |
|       185 | 2668 | `	if( pTok == 0 ){` |
|       ! 0 | 2669 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|       ! 0 | 2670 | `			zExpecting ? "syntax error, unexpected end of file, expecting %s"` |
|         - | 2671 | `			           : "syntax error, unexpected end of file",` |
|       ! 0 | 2672 | `			zExpecting);` |
|         - | 2673 | `	}` |
|       185 | 2674 | `	if( pTok->nType & PH7_TK_ID ){` |
|        16 | 2675 | `		zNoun = "identifier";` |
|       178 | 2676 | `	}else if( pTok->nType & PH7_TK_DOLLAR ){` |
|         6 | 2677 | `		zNoun = "variable";` |
|       170 | 2678 | `	}else if( pTok->nType & PH7_TK_INTEGER ){` |
|        23 | 2679 | `		zNoun = "integer";` |
|       158 | 2680 | `	}else if( pTok->nType & PH7_TK_REAL ){` |
|       ! 0 | 2681 | `		zNoun = "float";` |
|       ! 0 | 2682 | `	}` |
|       185 | 2683 | `	if( zExpecting ){` |
|       118 | 2684 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        38 | 2685 | `			"syntax error, unexpected %s \"%z\", expecting %s",zNoun,&pTok->sData,zExpecting);` |
|         - | 2686 | `	}` |
|       161 | 2687 | `	return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        52 | 2688 | `		"syntax error, unexpected %s \"%z\"",zNoun,&pTok->sData);` |
|        95 | 2689 | `}` |
|         - | 2690 | `/*` |
|         - | 2691 | ` * Generate a compile-time error message.` |
|         - | 2692 | ` * If the error count limit is reached (usually 15 error message)` |
|         - | 2693 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|         - | 2694 | ` * abort compilation immediately.` |
|         - | 2695 | ` */` |
|       678 | 2696 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|         5 | 2697 | `{` |
|       683 | 2698 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|       683 | 2699 | `	const char *zErr = "Error";` |
|         - | 2700 | `	SyString *pFile;` |
|         - | 2701 | `	va_list ap;` |
|         - | 2702 | `	sxi32 rc;` |
|         - | 2703 | `	/* Reset the working buffer */` |
|       683 | 2704 | `	SyBlobReset(pWorker);` |
|         - | 2705 | `	/* Peek the processed file path if available */` |
|       683 | 2706 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|       683 | 2707 | `	if( nErrType == E_ERROR \|\| nErrType == E_PARSE ){` |
|         - | 2708 | `		/* Increment the error counter. A PARSE error is every bit as fatal as an` |
|         - | 2709 | `		 * E_ERROR one: php compiles nothing, runs nothing and exits 255. Counting` |
|         - | 2710 | `		 * only E_ERROR let a parse error print its diagnostic and then fall through` |
|         - | 2711 | `		 * into execution with a 0 exit status. */` |
|       673 | 2712 | `		pGen->nErr++;` |
|       673 | 2713 | `		if( pGen->nErr > 15 ){` |
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
|       332 | 2729 | `	}` |
|       679 | 2730 | `	if( pGen->xErr == 0 ){` |
|         - | 2731 | `		/* No available error consumer,return immediately */` |
|         3 | 2732 | `		return SXRET_OK;` |
|         - | 2733 | `	}` |
|       677 | 2734 | `	switch(nErrType){` |
|       322 | 2735 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|         8 | 2736 | `	case E_WARNING: zErr = "Warning";     break;` |
|       348 | 2737 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|         6 | 2738 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|       ! 0 | 2739 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|       ! 0 | 2740 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|       ! 0 | 2741 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|       ! 0 | 2742 | `	case 8192 /* E_DEPRECATED */: zErr = "Deprecated"; break;` |
|       ! 0 | 2743 | `	default:` |
|       ! 0 | 2744 | `		break;` |
|         - | 2745 | `	}` |
|       677 | 2746 | `	rc = SXRET_OK;` |
|         - | 2747 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|       677 | 2748 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|       677 | 2749 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|       677 | 2750 | `	va_start(ap,zFormat);` |
|       677 | 2751 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|       677 | 2752 | `	va_end(ap);` |
|       677 | 2753 | `	if( pFile ){` |
|       677 | 2754 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       336 | 2755 | `	}` |
|         - | 2756 | `	/* Append a new line */` |
|       677 | 2757 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       677 | 2758 | `	if( SyBlobLength(pWorker) > 0 ){` |
|         - | 2759 | `		/* Consume the generated error message */` |
|       677 | 2760 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       336 | 2761 | `	}` |
|       677 | 2762 | `	return rc;` |
|       344 | 2763 | `}` |
|         - | 2764 |  |
