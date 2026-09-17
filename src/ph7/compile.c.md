# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1524/1654 lines (92.14%)

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
|    179520 |   46 | `PH7_PRIVATE GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|         5 |   47 | `{` |
|    179525 |   48 | `	GenBlock *pBlock = pCurrent;` |
|    362761 |   49 | `	for(;;){` |
|    725527 |   50 | `		if( pBlock->iFlags & iBlockType ){` |
|    179501 |   51 | `			iCount--; /* Decrement nesting level */` |
|    179501 |   52 | `			if( iCount < 1 ){` |
|         - |   53 | `				/* Block meet with the desired criteria */` |
|    179475 |   54 | `				return pBlock;` |
|         - |   55 | `			}` |
|        13 |   56 | `		}` |
|         - |   57 | `		/* Point to the upper block */` |
|    546057 |   58 | `		pBlock = pBlock->pParent;` |
|    546057 |   59 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|         - |   60 | `			/* Forbidden */` |
|        29 |   61 | `			break;` |
|         - |   62 | `		}` |
|         5 |   63 | `	}` |
|         - |   64 | `	/* No such block */` |
|        54 |   65 | `	return 0;` |
|     89765 |   66 | `}` |
|         - |   67 | `/*` |
|         - |   68 | ` * Initialize a freshly allocated block instance.` |
|         - |   69 | ` */` |
|  13157890 |   70 | `static void GenStateInitBlock(` |
|         - |   71 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |   72 | `	GenBlock *pBlock,    /* Target block */` |
|         - |   73 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   74 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|         - |   75 | `	void *pUserData      /* Upper layer private data */` |
|         - |   76 | `	)` |
|         5 |   77 | `{` |
|         - |   78 | `	/* Initialize block fields */` |
|  13157895 |   79 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  13157895 |   80 | `	pBlock->pUserData   = pUserData;` |
|  13157895 |   81 | `	pBlock->pGen        = pGen;` |
|  13157895 |   82 | `	pBlock->iFlags      = iType;` |
|  13157895 |   83 | `	pBlock->pParent     = 0;` |
|  13157895 |   84 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  13157895 |   85 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  13157895 |   86 | `}` |
|         - |   87 | `/*` |
|         - |   88 | ` * Allocate a new block instance.` |
|         - |   89 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|         - |   90 | ` * on success.Otherwise generate a compile-time error and abort` |
|         - |   91 | ` * processing on failure.` |
|         - |   92 | ` */` |
|  13153988 |   93 | `PH7_PRIVATE sxi32 GenStateEnterBlock(` |
|         - |   94 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |   95 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   96 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|         - |   97 | `	void *pUserData,      /* Upper layer private data */` |
|         - |   98 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|         - |   99 | `	)` |
|         5 |  100 | `{` |
|         - |  101 | `	GenBlock *pBlock;` |
|         - |  102 | `	/* Allocate a new block instance */` |
|  13153993 |  103 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  13153993 |  104 | `	if( pBlock == 0 ){` |
|         - |  105 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |  106 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - |  107 | `		 */` |
|       ! 0 |  108 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|         - |  109 | `		/* Abort processing immediately */` |
|       ! 0 |  110 | `		return SXERR_ABORT;` |
|         - |  111 | `	}` |
|         - |  112 | `	/* Zero the structure */` |
|  13153993 |  113 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  13153993 |  114 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|         - |  115 | `	/* Link to the parent block */` |
|  13153993 |  116 | `	pBlock->pParent = pGen->pCurrent;` |
|         - |  117 | `	/* A loop or switch gets an id, and remembers the loop it nests inside, so a goto's` |
|         - |  118 | `	 * and a label's positions can be compared after compilation (see aLoopParent). */` |
|  13153993 |  119 | `	if( iType & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    542999 |  120 | `		sxu32 nParent = pGen->nCurLoopId;` |
|    542999 |  121 | `		pGen->nLoopId++;` |
|    542999 |  122 | `		SySetPut(&pGen->aLoopParent,(const void *)&nParent);` |
|    542999 |  123 | `		pBlock->nLoopId = pGen->nLoopId;` |
|    542999 |  124 | `		pBlock->nOuterLoopId = nParent;` |
|    542999 |  125 | `		pGen->nCurLoopId = pGen->nLoopId;` |
|    271497 |  126 | `	}` |
|         - |  127 | `	/* Mark as the current block */` |
|  13153993 |  128 | `	pGen->pCurrent = pBlock;` |
|  13153993 |  129 | `	if( ppBlock ){` |
|         - |  130 | `		/* Write a pointer to the new instance */` |
|   6326101 |  131 | `		*ppBlock = pBlock;` |
|   3163048 |  132 | `	}` |
|  13153993 |  133 | `	return SXRET_OK;` |
|   6576999 |  134 | `}` |
|         - |  135 | `/*` |
|         - |  136 | ` * Release block fields without freeing the whole instance.` |
|         - |  137 | ` */` |
|  13153978 |  138 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|         5 |  139 | `{` |
|  13153983 |  140 | `	SySetRelease(&pBlock->aPostContFix);` |
|  13153983 |  141 | `	SySetRelease(&pBlock->aJumpFix);` |
|  13153983 |  142 | `}` |
|         - |  143 | `/*` |
|         - |  144 | ` * Release a block.` |
|         - |  145 | ` */` |
|  13153974 |  146 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|         5 |  147 | `{` |
|  13153979 |  148 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  13153979 |  149 | `	GenStateReleaseBlock(&(*pBlock));` |
|         - |  150 | `	/* Free the instance */` |
|  13153979 |  151 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  13153979 |  152 | `}` |
|         - |  153 | `/*` |
|         - |  154 | ` * POP and release a block from the stack of compiled blocks.` |
|         - |  155 | ` */` |
|  13153974 |  156 | `PH7_PRIVATE sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|         5 |  157 | `{` |
|  13153979 |  158 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  13153979 |  159 | `	if( pBlock == 0 ){` |
|         - |  160 | `		/* No more block to pop */` |
|       ! 0 |  161 | `		return SXERR_EMPTY;` |
|         - |  162 | `	}` |
|  13153979 |  163 | `	if( pBlock->iFlags & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    542991 |  164 | `		pGen->nCurLoopId = pBlock->nOuterLoopId;` |
|    271493 |  165 | `	}` |
|         - |  166 | `	/* Point to the upper block */` |
|  13153979 |  167 | `	pGen->pCurrent = pBlock->pParent;` |
|  13153979 |  168 | `	if( ppBlock ){` |
|         - |  169 | `		/* Write a pointer to the popped block */` |
|       ! 0 |  170 | `		*ppBlock = pBlock;` |
|       ! 0 |  171 | `	}else{` |
|         - |  172 | `		/* Safely release the block */` |
|  13153979 |  173 | `		GenStateFreeBlock(&(*pBlock));` |
|         - |  174 | `	}` |
|  13153979 |  175 | `	return SXRET_OK;` |
|   6576992 |  176 | `}` |
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
|   1056052 |  195 | `PH7_PRIVATE int GenStateUnconditionalTopLevel(ph7_gen_state *pGen)` |
|         5 |  196 | `{` |
|   1056057 |  197 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   1060059 |  198 | `	while( pBlock ){` |
|   1060059 |  199 | `		if( pBlock->iFlags & (GEN_BLOCK_COND\|GEN_BLOCK_LOOP\|GEN_BLOCK_FUNC\|GEN_BLOCK_SWITCH\|GEN_BLOCK_EXCEPTION) ){` |
|        77 |  200 | `			return 0; /* conditional / nested */` |
|         - |  201 | `		}` |
|   1059987 |  202 | `		if( pBlock->iFlags & GEN_BLOCK_GLOBAL ){` |
|   1055985 |  203 | `			return 1; /* reached the global block with no conditional ancestor */` |
|         - |  204 | `		}` |
|      4007 |  205 | `		pBlock = pBlock->pParent;` |
|         5 |  206 | `	}` |
|       ! 0 |  207 | `	return 1;` |
|    528031 |  208 | `}` |
|         - |  209 | `/*` |
|         - |  210 | ` * Guard a top-level function about to be installed. Same contract as the class` |
|         - |  211 | ` * guard above.` |
|         - |  212 | ` */` |
|    523752 |  213 | `PH7_PRIVATE sxi32 GenStateGuardFuncRedeclaration(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|         5 |  214 | `{` |
|         - |  215 | `	SyHashEntry *pEntry;` |
|    523757 |  216 | `	if( !GenStateUnconditionalTopLevel(pGen) ){` |
|        52 |  217 | `		return SXRET_OK;` |
|         - |  218 | `	}` |
|    523707 |  219 | `	pFunc->iFlags \|= VM_FUNC_BOUND;` |
|    523707 |  220 | `	if( pGen->pVm->bCompilingBuiltin ){` |
|    522337 |  221 | `		return SXRET_OK;` |
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
|    261881 |  247 | `}` |
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
|   4750456 |  258 | `PH7_PRIVATE sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|         5 |  259 | `{` |
|         - |  260 | `	JumpFixup sJumpFix;` |
|         - |  261 | `	sxi32 rc;` |
|         - |  262 | `	/* Init the JumpFixup structure */` |
|   4750461 |  263 | `	sJumpFix.nJumpType = nJumpType;` |
|   4750461 |  264 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|         - |  265 | `	/* Insert in the jump fixup table */` |
|   4750461 |  266 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|   4750461 |  267 | `	return rc;` |
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
|   9144318 |  280 | `PH7_PRIVATE sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|         5 |  281 | `{` |
|         - |  282 | `	JumpFixup *aFix;` |
|         - |  283 | `	VmInstr *pInstr;` |
|         - |  284 | `	sxu32 nFixed;` |
|         - |  285 | `	sxu32 n;` |
|         - |  286 | `	/* Point to the jump fixup table */` |
|   9144323 |  287 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|         - |  288 | `	/* Fix the desired jumps */` |
|  19140625 |  289 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|   9996307 |  290 | `		if( aFix[n].nJumpType < 0 ){` |
|         - |  291 | `			/* Already fixed */` |
|   3732549 |  292 | `			continue;` |
|         - |  293 | `		}` |
|   6263763 |  294 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|         - |  295 | `			/* Not of our interest */` |
|   1513309 |  296 | `			continue;` |
|         - |  297 | `		}` |
|         - |  298 | `		/* Point to the instruction to fix */` |
|   4750459 |  299 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|   4750459 |  300 | `		if( pInstr ){` |
|   4750459 |  301 | `			pInstr->iP2 = nJumpDest;` |
|   4750459 |  302 | `			nFixed++;` |
|         - |  303 | `			/* Mark as fixed */` |
|   4750459 |  304 | `			aFix[n].nJumpType = -1;` |
|   2375227 |  305 | `		}` |
|   2375232 |  306 | `	}` |
|         - |  307 | `	/* Total number of fixed jumps */` |
|   9144323 |  308 | `	return nFixed;` |
|         5 |  309 | `}` |
|         - |  310 | `/*` |
|         - |  311 | ` * Fix a 'goto' now the jump destination is resolved.` |
|         - |  312 | ` * The goto statement can be used to jump to another section` |
|         - |  313 | ` * in the program.` |
|         - |  314 | ` * Refer to the routine responsible of compiling the goto` |
|         - |  315 | ` * statement for more information.` |
|         - |  316 | ` */` |
|   3385536 |  317 | `PH7_PRIVATE sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|         5 |  318 | `{` |
|         - |  319 | `	JumpFixup *pJump,*aJumps;` |
|         - |  320 | `	Label *pLabel;` |
|         - |  321 | `	VmInstr *pInstr;` |
|         - |  322 | `	sxi32 rc;` |
|         - |  323 | `	sxu32 n;` |
|         - |  324 | `	/* Point to the goto table */` |
|   3385541 |  325 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|         - |  326 | `	/* Fix */` |
|   3385687 |  327 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|   3385539 |  378 | `	return SXRET_OK;` |
|   1692773 |  379 | `}` |
|         - |  380 | `/*` |
|         - |  381 | ` * Check if a given token value is installed in the literal table.` |
|         - |  382 | ` */` |
|  16959858 |  383 | `PH7_PRIVATE sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|         5 |  384 | `{` |
|         - |  385 | `	SyHashEntry *pEntry;` |
|  16959863 |  386 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  16959863 |  387 | `	if( pEntry == 0 ){` |
|   4521433 |  388 | `		return SXERR_NOTFOUND;` |
|         - |  389 | `	}` |
|  12438435 |  390 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  12438435 |  391 | `	return SXRET_OK;` |
|   8479934 |  392 | `}` |
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
|   4521428 |  403 | `PH7_PRIVATE sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|         5 |  404 | `{` |
|   4521433 |  405 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|   4521433 |  406 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   2260714 |  407 | `	}` |
|   4521433 |  408 | `	return SXRET_OK;` |
|         5 |  409 | `}` |
|         - |  410 | `/*` |
|         - |  411 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|         - |  412 | ` * in the constant table.` |
|         - |  413 | ` */` |
|   3834318 |  414 | `PH7_PRIVATE ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|         5 |  415 | `{` |
|         - |  416 | `	ph7_value *pObj;` |
|   3834323 |  417 | `	sxu32 nIdx = 0; /* cc warning */` |
|         - |  418 | `	/* Reserve a new constant */` |
|   3834323 |  419 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   3834323 |  420 | `	if( pObj == 0 ){` |
|       ! 0 |  421 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  422 | `		return 0;` |
|         - |  423 | `	}` |
|   3834323 |  424 | `	*pIdx = nIdx;` |
|         - |  425 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|         - |  426 | `	 * the constant string iterals table [optimization purposes].` |
|         - |  427 | `	 */` |
|   3834323 |  428 | `	return pObj;` |
|   1917164 |  429 | `}` |
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
|   7979022 |  444 | `PH7_PRIVATE void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|         5 |  445 | `{` |
|         - |  446 | `	VmCallArgMap *pMap;` |
|   7979027 |  447 | `	if( !pGen->bStrictTypes ) return p3;` |
|        58 |  448 | `	if( p3 == 0 ){` |
|        54 |  449 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|        54 |  450 | `		if( pMap == 0 ) return 0;` |
|        54 |  451 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|        54 |  452 | `		p3 = (void *)pMap;` |
|        25 |  453 | `	}` |
|        58 |  454 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|        58 |  455 | `	return p3;` |
|   3989516 |  456 | `}` |
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
|    343296 |  476 | `PH7_PRIVATE int GenStateIsReservedConstant(SyString *pName)` |
|         5 |  477 | `{` |
|    343301 |  478 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      3945 |  479 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|         3 |  480 | `			return TRUE;` |
|      3943 |  481 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|         5 |  482 | `			return TRUE;` |
|         5 |  483 | `		}` |
|    341328 |  484 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|      7825 |  485 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|         3 |  486 | `			return TRUE;` |
|         - |  487 | `		}` |
|      3909 |  488 | `	}` |
|         - |  489 | `	/* Not a reserved constant */` |
|    343293 |  490 | `	return FALSE;` |
|    171653 |  491 | `}` |
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
|  54204144 |  508 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|         5 |  509 | `{` |
|  54204149 |  510 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|         - |  511 | `	sxu32 nTarget;` |
|         - |  512 | `	sxu32 *aIdx;` |
|         - |  513 | `	sxu32 i;` |
|  54204149 |  514 | `	if( nCur <= nBaseline ){` |
|  54204053 |  515 | `		return;` |
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
|  27102077 |  526 | `}` |
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
|   7086592 |  545 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
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
|   7086597 |  564 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|   1814207 |  565 | `		return 0;` |
|         - |  566 | `	}` |
|  57511695 |  567 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
|  52297940 |  568 | `		if( pName->nByte == aByRef[i].nByte` |
|  27671056 |  569 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|     58645 |  570 | `			return aByRef[i].mask;` |
|         - |  571 | `		}` |
|  26119655 |  572 | `	}` |
|   5213755 |  573 | `	return 0;` |
|   3543301 |  574 | `}` |
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
|   7086592 |  585 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|         5 |  586 | `{` |
|         - |  587 | `	SyToken *p, *pEnd;` |
|   7086597 |  588 | `	pOut->zString = 0;` |
|   7086597 |  589 | `	pOut->nByte = 0;` |
|   7086597 |  590 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|       ! 0 |  591 | `		return;` |
|         - |  592 | `	}` |
|   7086597 |  593 | `	p = pLeft->pStart;` |
|   7086597 |  594 | `	pEnd = pLeft->pEnd;` |
|         - |  595 | `	/* Optional single leading namespace separator (absolute path). */` |
|   7086597 |  596 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|      3941 |  597 | `		p++;` |
|      1968 |  598 | `	}` |
|   7086597 |  599 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|   1814157 |  600 | `		return;` |
|         - |  601 | `	}` |
|         - |  602 | `	/* Must be a single component: nothing follows the name token. */` |
|   5272445 |  603 | `	if( p + 1 != pEnd ){` |
|        55 |  604 | `		return;` |
|         - |  605 | `	}` |
|   5272395 |  606 | `	*pOut = p->sData;` |
|   3543301 |  607 | `}` |
|         - |  608 | `/*` |
|         - |  609 | ` * Generate bytecode for a given expression tree.` |
|         - |  610 | ` * If something goes wrong while generating bytecode` |
|         - |  611 | ` * for the expression tree (A very unlikely scenario)` |
|         - |  612 | ` * this function takes care of generating the appropriate` |
|         - |  613 | ` * error message.` |
|         - |  614 | ` */` |
|  76427508 |  615 | `static sxi32 GenStateEmitExprCode(` |
|         - |  616 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |  617 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|         - |  618 | `	sxi32 iFlags /* Control flags */` |
|         - |  619 | `	)` |
|         5 |  620 | `{` |
|         - |  621 | `	VmInstr *pInstr;` |
|         - |  622 | `	sxu32 nJmpIdx;` |
|  76427513 |  623 | `	sxi32 iP1 = 0;` |
|  76427513 |  624 | `	sxu32 iP2 = 0;` |
|  76427513 |  625 | `	void *p3  = 0;` |
|         - |  626 | `	sxi32 iVmOp;` |
|         - |  627 | `	sxi32 rc;` |
|  76427513 |  628 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
|  76427513 |  629 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
|  76427513 |  630 | `	sxu32 nRhsNsBase = 0;` |
|  76427513 |  631 | `	if( pNode->xCode ){` |
|         - |  632 | `		SyToken *pTmpIn,*pTmpEnd;` |
|         - |  633 | `		/* Compile node */` |
|  46128281 |  634 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
|  46128281 |  635 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
|  46128281 |  636 | `		RE_SWAP_DELIMITER(pGen);` |
|  46128281 |  637 | `		return rc;` |
|         - |  638 | `	}` |
|  30299237 |  639 | `	if( pNode->pOp == 0 ){` |
|       ! 0 |  640 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - |  641 | `			"Invalid expression node,PH7 is aborting compilation");` |
|       ! 0 |  642 | `		return SXERR_ABORT;` |
|         - |  643 | `	}` |
|  30299237 |  644 | `	iVmOp = pNode->pOp->iVmOp;` |
|  30299237 |  645 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|         - |  646 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|         - |  647 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|         - |  648 | `		 * and later errors are still reported. */` |
|         3 |  649 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - |  650 | `			"The (unset) cast is no longer supported");` |
|         3 |  651 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  652 | `			return SXERR_ABORT;` |
|         - |  653 | `		}` |
|         1 |  654 | `	}` |
|  30299237 |  655 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
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
|  30299147 |  705 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|         - |  706 | `		sxu32 nJz,nJmp;` |
|         - |  707 | `		sxu32 nTernaryNsBase;` |
|         - |  708 | `		/* Ternary operator require special handling */` |
|         - |  709 | `		/* Phase#1: Compile the condition */` |
|    506113 |  710 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    506113 |  711 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    506113 |  712 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  713 | `			return rc;` |
|         - |  714 | `		}` |
|         - |  715 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|         - |  716 | `		 * compiling the condition must short-circuit to the end of the` |
|         - |  717 | `		 * condition expression, not leak past the ternary. */` |
|    506113 |  718 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    506113 |  719 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    506113 |  720 | `		if( pNode->pLeft ){` |
|         - |  721 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|         - |  722 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    502145 |  723 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - |  724 | `			/* Phase#3: Compile the 'then' expression  */` |
|    502145 |  725 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    502145 |  726 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    502145 |  727 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  728 | `				return rc;` |
|         - |  729 | `			}` |
|    502145 |  730 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    251075 |  731 | `		}else{` |
|         - |  732 | `			/* Elvis operator: (expr) ?: (else)` |
|         - |  733 | `			 * Duplicate condition so original value is the 'then' result.` |
|         - |  734 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      3973 |  735 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      3973 |  736 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - |  737 | `		}` |
|         - |  738 | `		/* Phase#4: Emit the unconditional jump */` |
|    506113 |  739 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|         - |  740 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    506113 |  741 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    506113 |  742 | `		if( pInstr ){` |
|    506113 |  743 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    253054 |  744 | `		}` |
|    506113 |  745 | `		if( !pNode->pLeft ){` |
|         - |  746 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      3973 |  747 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      1984 |  748 | `		}` |
|         - |  749 | `		/* Phase#6: Compile the 'else' expression */` |
|    506113 |  750 | `		if( pNode->pRight ){` |
|    506113 |  751 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    506113 |  752 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    506113 |  753 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  754 | `				return rc;` |
|         - |  755 | `			}` |
|    506113 |  756 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    253054 |  757 | `		}` |
|    506113 |  758 | `		if( nJmp > 0 ){` |
|         - |  759 | `			/* Phase#7: Fix the unconditional jump */` |
|    506113 |  760 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    506113 |  761 | `			if( pInstr ){` |
|    506113 |  762 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    253054 |  763 | `			}` |
|    253054 |  764 | `		}` |
|         - |  765 | `		/* All done */` |
|    506113 |  766 | `		return SXRET_OK;` |
|         - |  767 | `	}` |
|  29793039 |  768 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
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
|  29793013 |  801 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|         - |  802 | `	/* Generate code for the left tree */` |
|  29793013 |  803 | `	if( pNode->pLeft ){` |
|  29738629 |  804 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  29738629 |  805 | `		if( iVmOp == PH7_OP_CALL ){` |
|         - |  806 | `			ph7_expr_node **apNode;` |
|   7090841 |  807 | `			int hasSpread = 0;` |
|   7090841 |  808 | `			int hasNamed = 0;` |
|   7090841 |  809 | `			int bAnySpread = 0;` |
|   7090841 |  810 | `			sxu32 byRefMask = 0;` |
|         - |  811 | `			sxi32 nArgs;` |
|         - |  812 | `			sxi32 n;` |
|         - |  813 | `			/* Recurse and generate bytecodes for function arguments */` |
|   7090841 |  814 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   7090841 |  815 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|         - |  816 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|         - |  817 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|         - |  818 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|   7090841 |  819 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|        84 |  820 | `				bFcc = 1;` |
|        84 |  821 | `				nArgs = 0;` |
|        41 |  822 | `			}` |
|         - |  823 | `			/* Validate argument order like php: no positional argument after a` |
|         - |  824 | ``			 * named one OR after unpacking, and `name: ...$x` is a parse error. */`` |
|         - |  825 | `			{` |
|   7090841 |  826 | `				int seenNamed = 0;` |
|   7090841 |  827 | `				int seenSpread = 0;` |
|  15142925 |  828 | `				for( n = 0; n < nArgs; ++n ){` |
|   8052091 |  829 | `					if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|      4101 |  830 | `						bAnySpread = 1;` |
|      4101 |  831 | `						seenSpread = 1;` |
|      4101 |  832 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       ! 0 |  833 | `							rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - |  834 | `								"syntax error, unexpected token \"...\"");` |
|       ! 0 |  835 | `							return SXERR_SYNTAX;` |
|         5 |  836 | `						}` |
|   8050043 |  837 | `					}else if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       300 |  838 | `						seenNamed = 1;` |
|       300 |  839 | `						hasNamed = 1;` |
|   8047847 |  840 | `					}else if( seenNamed ){` |
|         3 |  841 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - |  842 | `							"Cannot use positional argument after named argument");` |
|         3 |  843 | `						return SXERR_SYNTAX;` |
|   8047697 |  844 | `					}else if( seenSpread ){` |
|       ! 0 |  845 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - |  846 | `							"Cannot use positional argument after argument unpacking");` |
|       ! 0 |  847 | `						return SXERR_SYNTAX;` |
|         - |  848 | `					}` |
|   4026047 |  849 | `				}` |
|         - |  850 | `			}` |
|         - |  851 | `			/* Read-only load */` |
|   7090839 |  852 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|         - |  853 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|         - |  854 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|         - |  855 | `			 * objects dispatch to the right method (offsetExists for both;` |
|         - |  856 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|   7090839 |  857 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|   7090839 |  858 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|  11031417 |  859 | `				int bIsset = pCallName->nByte == 5` |
|   7090834 |  860 | `					&& SyStrnicmp(pCallName->zString,"isset",5) == 0;` |
|  11031417 |  861 | `				int bEmpty = pCallName->nByte == 5` |
|   7090834 |  862 | `					&& SyStrnicmp(pCallName->zString,"empty",5) == 0;` |
|         - |  863 | `				/* isset()/empty() are language CONSTRUCTS, not functions: php parses` |
|         - |  864 | `				 * their argument list in the grammar and a missing operand is a parse` |
|         - |  865 | `				 * error on the ')'. They compile through this ordinary call loop, which` |
|         - |  866 | ``				 * never checked arity, so `empty()` quietly evaluated to true and`` |
|         - |  867 | ``				 * `isset()` to false. (empty() also takes exactly one operand in php,`` |
|         - |  868 | `				 * unlike isset(), which is variadic.) */` |
|   7090839 |  869 | `				if( (bIsset \|\| bEmpty) && nArgs < 1 ){` |
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
|   7090835 |  880 | `				if( bIsset ){` |
|    327857 |  881 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|   6926909 |  882 | `				}else if( bEmpty ){` |
|       129 |  883 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|        62 |  884 | `				}` |
|         - |  885 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|         - |  886 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|         - |  887 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|         - |  888 | `				 * write back through. Skipped when spread/named args are present:` |
|         - |  889 | `				 * the compile-time positional index no longer maps to the` |
|         - |  890 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|   7090835 |  891 | `				if( !bAnySpread && !hasNamed ){` |
|         - |  892 | `					SyString sBuiltin;` |
|   7086597 |  893 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|   7086597 |  894 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|   3543296 |  895 | `				}` |
|   3545415 |  896 | `			}` |
|  15142915 |  897 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|   8052087 |  898 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   8052087 |  899 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|         - |  900 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|         - |  901 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|         - |  902 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|         - |  903 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|         - |  904 | `				 * builtin to write back through. A plain $var target is unaffected` |
|         - |  905 | `				 * (iP1=0 either way). */` |
|   8052087 |  906 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|     39071 |  907 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|     39071 |  908 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|     19533 |  909 | `				}` |
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
|   8052082 |  920 | `				if( (iFlags & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_LOAD_IDX_UNSET)) == 0` |
|   7888081 |  921 | `				 && apNode[n]->pOp == 0 && apNode[n]->xCode == PH7_CompileVariable` |
|   4133940 |  922 | `				 && (apNode[n]->iFlags & (EXPR_NODE_NAMED_ARG\|EXPR_NODE_SPREAD)) == 0 ){` |
|   3378421 |  923 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|   1689208 |  924 | `				}` |
|   8052087 |  925 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|   8052087 |  926 | `				if( rc != SXRET_OK ){` |
|         3 |  927 | `					return rc;` |
|         - |  928 | `				}` |
|         - |  929 | `				/* Each argument is an independent nullsafe scope. */` |
|   8052085 |  930 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|   8052085 |  931 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|         - |  932 | `					/* Emit spread opcode to unpack this array argument */` |
|      4101 |  933 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      4101 |  934 | `					hasSpread = 1;` |
|      2048 |  935 | `				}` |
|   4026045 |  936 | `			}` |
|         - |  937 | `			/* Total number of given arguments */` |
|   7090833 |  938 | `			iP1 = nArgs;` |
|   7090833 |  939 | `			iP2 = hasSpread;` |
|         - |  940 | `			/* Build VmCallArgMap if named arguments are present.` |
|         - |  941 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|   7090833 |  942 | `			if( hasNamed ){` |
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
|   7090833 |  974 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|   3545414 |  975 | `		}` |
|         - |  976 | `		{` |
|         - |  977 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|         - |  978 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|         - |  979 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|         - |  980 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|         - |  981 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|         - |  982 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|         - |  983 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|         - |  984 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
|  29738621 |  985 | `			sxi32 iLeftFlags = iFlags;` |
|  29738621 |  986 | `			sxu32 nNullcLhsFirst = PH7_VmInstrLength(pGen->pVm);` |
|  29738621 |  987 | `			int bNullcLhs = 0;` |
|  29738616 |  988 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
|  24146280 |  989 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|   9276998 |  990 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|   7986195 |  991 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   2792721 |  992 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|   1396358 |  993 | `			}` |
|         - |  994 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|         - |  995 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|         - |  996 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|         - |  997 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|         - |  998 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|         - |  999 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|         - | 1000 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
|  29738616 | 1001 | `			if( pNode->pOp` |
|  41809435 | 1002 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
|  26940174 | 1003 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  24141680 | 1004 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|   6019051 | 1005 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|   3009523 | 1006 | `			}` |
|         - | 1007 | ``			/* `++`/`--` mutate their operand in place — the operand is a write`` |
|         - | 1008 | ``			 * lvalue exactly like a compound assign's (`$o->m[0]++` must tag the`` |
|         - | 1009 | ``			 * member base PH7_MEMBER_WRITE the way `$o->m[0] += 1` does: hooked`` |
|         - | 1010 | `			 * properties throw php's Indirect-modification Error, missing ones` |
|         - | 1011 | `			 * auto-vivify). The prec-18 site below handles the assign family;` |
|         - | 1012 | ``			 * `++`/`--` are unary, their operand is pLeft. */`` |
|  29738616 | 1013 | `			if( pNode->pOp` |
|  29738621 | 1014 | `				&& (pNode->pOp->iVmOp == PH7_OP_INCR \|\| pNode->pOp->iVmOp == PH7_OP_DECR) ){` |
|    218739 | 1015 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE` |
|         - | 1016 | ``					\| EXPR_FLAG_RMW_LOAD /* php warns before seeding `$undef++` */;`` |
|    109367 | 1017 | `			}` |
|         - | 1018 | ``			/* `??` reads its LEFT operand in isset-context: an undefined or`` |
|         - | 1019 | `			 * UNINITIALIZED typed PROPERTY must yield the default rather than a` |
|         - | 1020 | `			 * warning/Error (php). Tag a member-access LHS so its OP_MEMBER takes` |
|         - | 1021 | `			 * the silent-lookup path (iP2 = ISSET), which still loads a present` |
|         - | 1022 | `			 * value. A SUBSCRIPT LHS is left untagged: LOAD_IDX's ISSET mode means` |
|         - | 1023 | ``			 * offsetExists (a bool), but `$o[$k] ?? d` needs the offsetGet value —`` |
|         - | 1024 | `			 * that path is already handled correctly by OP_NULLC. */` |
|  29738621 | 1025 | `			if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC && pNode->pLeft ){` |
|         - | 1026 | ``				/* php reads the ENTIRE left operand of `??` in isset-context: no`` |
|         - | 1027 | ``				 * "Undefined variable" for `$x ?? d` OR for the base of`` |
|         - | 1028 | ``				 * `$x['k'] ?? d`. QUIET_VAR silences the variable read wherever it`` |
|         - | 1029 | `				 * sits in the chain. */` |
|     58661 | 1030 | `				iLeftFlags \|= EXPR_FLAG_QUIET_VAR;` |
|     58661 | 1031 | `				bNullcLhs = 1;` |
|     58656 | 1032 | `				if( pNode->pLeft->pOp` |
|     87920 | 1033 | `					&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|     58598 | 1034 | `						\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|     58581 | 1035 | `						\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|         - | 1036 | `					/* A member-access LHS additionally takes OP_MEMBER's silent` |
|         - | 1037 | `					 * lookup (iP2 = ISSET) so an uninitialized typed property` |
|         - | 1038 | `					 * yields the default instead of an Error. A SUBSCRIPT LHS must` |
|         - | 1039 | `					 * NOT: LOAD_IDX's ISSET mode means offsetExists (a bool), while` |
|         - | 1040 | ``					 * `$o[$k] ?? d` needs the offsetGet value — OP_NULLC already`` |
|         - | 1041 | `					 * handles that path. */` |
|        37 | 1042 | `					iLeftFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|        18 | 1043 | `				}` |
|     29328 | 1044 | `			}` |
|  29738621 | 1045 | `			if( iVmOp == PH7_OP_ERR_CTRL ){` |
|         - | 1046 | `				/* '@' must suppress the diagnostics raised WHILE its operand runs, so` |
|         - | 1047 | `				 * open the window here; the trailing emit below closes it (iP1 = 0). */` |
|     15849 | 1048 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_ERR_CTRL,1,0,0,0);` |
|      7922 | 1049 | `			}` |
|  29738621 | 1050 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags\|EXPR_FLAG_RDONLY_LOAD);` |
|  29738621 | 1051 | `			if( rc == SXRET_OK && bNullcLhs ){` |
|         - | 1052 | ``				/* Mark EVERY subscript read in the `??` left chain quiet (iP2=8).`` |
|         - | 1053 | `				 * Peeking at the next instruction only catches the OUTERMOST` |
|         - | 1054 | ``				 * access, so `$d['x']['y'] ?? $v` still warned for the inner one;`` |
|         - | 1055 | `				 * and a forward scan would be unsound, since an unrelated sibling` |
|         - | 1056 | ``				 * access can sit immediately before a coalesce (`f($a['k'],`` |
|         - | 1057 | ``				 * $b['j'] ?? 1)`). The compiler knows the real extent, so it marks`` |
|         - | 1058 | `				 * the range it just emitted. Write-context codes (1/3/5) belong to` |
|         - | 1059 | ``				 * `??=` and keep their meaning. */`` |
|     58661 | 1060 | `				sxu32 nEnd = PH7_VmInstrLength(pGen->pVm);` |
|         - | 1061 | `				sxu32 nAt;` |
|    328187 | 1062 | `				for( nAt = nNullcLhsFirst ; nAt < nEnd ; ++nAt ){` |
|    269531 | 1063 | `					VmInstr *pFix = PH7_VmGetInstr(pGen->pVm,nAt);` |
|    269531 | 1064 | `					if( pFix && pFix->iOp == PH7_OP_LOAD_IDX && pFix->iP2 == 0 ){` |
|     58563 | 1065 | `						pFix->iP2 = 8;` |
|     29279 | 1066 | `					}` |
|    134768 | 1067 | `				}` |
|     29328 | 1068 | `			}` |
|         - | 1069 | `		}` |
|  29738621 | 1070 | `		if( rc != SXRET_OK ){` |
|        36 | 1071 | `			return rc;` |
|         - | 1072 | `		}` |
|  29738589 | 1073 | `		if( !bIsChainOp ){` |
|         - | 1074 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|         - | 1075 | `			 * target the end of that LHS chain, which is right here. */` |
|  13588615 | 1076 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|   6794305 | 1077 | `		}` |
|  29738589 | 1078 | `		if( iVmOp == PH7_OP_CALL ){` |
|   7090833 | 1079 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   7090833 | 1080 | `			if( pInstr ){` |
|   7090833 | 1081 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|   5272703 | 1082 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|         - | 1083 | `					sxu32 nQual;` |
|   5272703 | 1084 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 1085 | `					/* Prevent constant expansion but preserve the absolute flag` |
|         - | 1086 | `					 * so the later NEW handler (if any) can see it. */` |
|   5272703 | 1087 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|         - | 1088 | `					/* Namespace-qualify the function name for CALL, unless the` |
|         - | 1089 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|         - | 1090 | `					 * imports — class imports must NOT affect function` |
|         - | 1091 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|         - | 1092 | `					 * before NEW; we store the original literal index in the` |
|         - | 1093 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|         - | 1094 | `					 * the unqualified name and re-qualify with class imports. */` |
|   5272703 | 1095 | `					if( bAbsolute ){` |
|      3941 | 1096 | `						pInstr->iP2 = (sxi32)nOrig;` |
|      1973 | 1097 | `					}else{` |
|   5268767 | 1098 | `						int fromImport = 0;` |
|   5268767 | 1099 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|   5268767 | 1100 | `						pInstr->iP2 = (sxi32)nQual;` |
|   5268767 | 1101 | `						if( nQual != nOrig ){` |
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
|   4454484 | 1124 | `				}else if( (pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */` |
|   1808028 | 1125 | `						&& !(pNode->pLeft && (pNode->pLeft->iFlags & EXPR_NODE_PARENS)))` |
|    919176 | 1126 | `					\|\| pInstr->iOp == PH7_OP_NEW ){` |
|         - | 1127 | `					/* Method call,flag that. But NOT when the callee was an explicitly` |
|         - | 1128 | ``					 * PARENTHESISED member access: `($o->p)(...)` / `($o::$p)(...)` invokes`` |
|         - | 1129 | `					 * the VALUE of the property (php's variable-invocation), so the OP_MEMBER` |
|         - | 1130 | `					 * must stay a plain property READ (leaving the callable on the stack for` |
|         - | 1131 | `					 * OP_CALL to invoke) rather than being rewritten into a method-name` |
|         - | 1132 | ``					 * resolution — the parens are exactly what distinguishes `($o->p)()` from`` |
|         - | 1133 | ``					 * the method call `$o->p()`. */`` |
|   1797929 | 1134 | `					pInstr->iP2 = 1;` |
|         - | 1135 | ``					/* A static call with a DYNAMIC method name (`C::$m(...)`): the`` |
|         - | 1136 | ``					 * static-`::` codegen folded the variable NAME into OP_MEMBER->p3`` |
|         - | 1137 | ``					 * as if it were a static-PROPERTY read (`C::$m`), but in a CALL the`` |
|         - | 1138 | `					 * method name is the variable's VALUE. Rebuild the sequence` |
|         - | 1139 | `					 * [class, OP_LOAD $m -> value, OP_MEMBER(static, method)] so the` |
|         - | 1140 | `					 * dynamic name is read off the stack, matching the instance` |
|         - | 1141 | ``					 * (`$o->$m()`) path. iP1==1 marks a static member. */`` |
|   1797929 | 1142 | `					if( pInstr->iOp == PH7_OP_MEMBER && pInstr->iP1 == 1 && pInstr->p3 ){` |
|        11 | 1143 | `						void *pDynName = pInstr->p3;` |
|        11 | 1144 | `						(void)PH7_VmPopInstr(pGen->pVm);` |
|        11 | 1145 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,pDynName,0);` |
|        11 | 1146 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_MEMBER,1,PH7_MEMBER_METHOD,0,0);` |
|         5 | 1147 | `					}` |
|    898962 | 1148 | `				}` |
|   3545419 | 1149 | `			}` |
|  26193175 | 1150 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|         - | 1151 | `			ph7_expr_node **apNode;` |
|         - | 1152 | `			sxi32 n;` |
|   3040105 | 1153 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|         - | 1154 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|         - | 1155 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE` |
|         - | 1156 | `				\|EXPR_FLAG_QUIET_VAR\|EXPR_FLAG_RMW_LOAD);` |
|         - | 1157 | `			/* Recurse and generate bytecodes for array index */` |
|   3040105 | 1158 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   5841981 | 1159 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   2801881 | 1160 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   2801881 | 1161 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   2801881 | 1162 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 1163 | `					return rc;` |
|         - | 1164 | `				}` |
|         - | 1165 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   2801881 | 1166 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   1400943 | 1167 | `			}` |
|   3040105 | 1168 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   2801881 | 1169 | `				iP1 = 1; /* Node have an index associated with it */` |
|   1400938 | 1170 | `			}` |
|   3040105 | 1171 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|         - | 1172 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|    374509 | 1173 | `				iP2 = 4;` |
|   2852853 | 1174 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|         - | 1175 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|         - | 1176 | `				 * so the trailing unset() builtin can drop the slot. */` |
|     23471 | 1177 | `				iP2 = 5;` |
|   2653868 | 1178 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|         - | 1179 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|         - | 1180 | `				 * short-circuit on missing keys without invoking offsetGet` |
|         - | 1181 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|        35 | 1182 | `				iP2 = 6;` |
|   2642120 | 1183 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|         - | 1184 | `				/* Create an empty entry when the desired index is not found */` |
|    558349 | 1185 | `				iP2 = 1;` |
|    279177 | 1186 | `			}` |
|  21127711 | 1187 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         - | 1188 | `			/* POP the left node */` |
|         5 | 1189 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         2 | 1190 | `		}` |
|  14869292 | 1191 | `	}` |
|  29792973 | 1192 | `	rc = SXRET_OK;` |
|  29792973 | 1193 | `	nJmpIdx = 0;` |
|         - | 1194 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|         - | 1195 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|         - | 1196 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  29792973 | 1197 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|    468847 | 1198 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    468847 | 1199 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    468847 | 1200 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    468847 | 1201 | `			int isSpecial = 0;` |
|    468847 | 1202 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|    375243 | 1203 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|    375243 | 1204 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|    375238 | 1205 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    349817 | 1206 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    189524 | 1207 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|    148325 | 1208 | `					isSpecial = 1;` |
|     74160 | 1209 | `				}` |
|    211020 | 1210 | `			}` |
|    515649 | 1211 | `			pInstr->iP1 = 0;` |
|         - | 1212 | ``			/* A leading `\` (NSSEP) makes the class name ABSOLUTE: it names the`` |
|         - | 1213 | `			 * global class, so it must NOT be re-qualified with the current namespace.` |
|         - | 1214 | ``			 * `\Closure::bind(...)` inside `namespace X` is Closure, not X\Closure.`` |
|         - | 1215 | ``			 * (Multi-component `\A\B::m` already resolves absolutely because its`` |
|         - | 1216 | ``			 * literal keeps a backslash; the single-component `\Closure` lost it.) */`` |
|         - | 1217 | `			{` |
|    726669 | 1218 | `				int bAbsolute = (pNode->pLeft && pNode->pLeft->pStart` |
|    633060 | 1219 | `					&& (pNode->pLeft->pStart->nType & PH7_TK_NSSEP));` |
|    422045 | 1220 | `				if( !isSpecial && !bAbsolute ){` |
|    273707 | 1221 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|    136851 | 1222 | `				}` |
|         - | 1223 | `			}` |
|         - | 1224 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|         - | 1225 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|    422045 | 1226 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|    273725 | 1227 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|    273725 | 1228 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|        78 | 1229 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|        78 | 1230 | `					return SXRET_OK;` |
|         - | 1231 | `				}` |
|    136823 | 1232 | `			}` |
|    210983 | 1233 | `		}` |
|    304560 | 1234 | `	}` |
|         - | 1235 | `	/* Generate code for the right tree */` |
|  29746115 | 1236 | `	if( pNode->pRight ){` |
|  17083889 | 1237 | `		if( iVmOp == PH7_OP_LAND ){` |
|         - | 1238 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    452673 | 1239 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  16857555 | 1240 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|         - | 1241 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    327573 | 1242 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  16467437 | 1243 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|         - | 1244 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     58661 | 1245 | `			iVmOp = 0; /* No binary operator to emit */` |
|     58661 | 1246 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  16274377 | 1247 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|         - | 1248 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|         - | 1249 | `			 * the entire containing postfix chain to null. The jump target is` |
|         - | 1250 | `			 * patched later by the innermost non-chain ancestor (or by` |
|         - | 1251 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|         - | 1252 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|         - | 1253 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|       107 | 1254 | `			sxu32 nNsJmp = 0;` |
|       107 | 1255 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|       107 | 1256 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  16244945 | 1257 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|         - | 1258 | `			/* The lvalue is the RIGHT operand (these ops are right-associative). Mark it a write` |
|         - | 1259 | ``			 * target so a missing member (the base of a subscript-write, or a bare `$o->p`) is`` |
|         - | 1260 | `			 * auto-created — PHP auto-vivifies on write. */` |
|   5239417 | 1261 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|   5239417 | 1262 | `			if( iVmOp != PH7_OP_STORE ){` |
|         - | 1263 | ``				/* COMPOUND assignment (`.=`, `+=`, ...) READS the target first, so`` |
|         - | 1264 | `` 				 * php warns when it is undefined and then seeds it; a plain `=` `` |
|         - | 1265 | `				 * writes without reading and stays silent. */` |
|    452535 | 1266 | `				iFlags \|= EXPR_FLAG_RMW_LOAD;` |
|    226265 | 1267 | `			}` |
|   2619706 | 1268 | `		}` |
|  17083889 | 1269 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  17083889 | 1270 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_RDONLY_LOAD);` |
|  17083889 | 1271 | `		if( !bIsChainOp ){` |
|         - | 1272 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|         - | 1273 | `			 * operator instruction is emitted. */` |
|  11064917 | 1274 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|   5532456 | 1275 | `		}` |
|  17083889 | 1276 | `		if( iVmOp == PH7_OP_STORE ){` |
|   4786887 | 1277 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|   4786850 | 1278 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|         - | 1279 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|         - | 1280 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|         - | 1281 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|         - | 1282 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|         - | 1283 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|         - | 1284 | `				 */` |
|        91 | 1285 | `				iVmOp = 0;` |
|   4786844 | 1286 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|   4786801 | 1287 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 1288 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|    924601 | 1289 | `					iP2 = 1;` |
|    462303 | 1290 | `				}else{` |
|   3862205 | 1291 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 1292 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|    538759 | 1293 | `						iVmOp = PH7_OP_STORE_IDX;` |
|    538759 | 1294 | `						iP1 = pInstr->iP1;` |
|    269382 | 1295 | `					}else{` |
|   3323451 | 1296 | `						p3 = pInstr->p3;` |
|         - | 1297 | `					}` |
|         - | 1298 | `					/* POP the last dynamic load instruction */` |
|   3862205 | 1299 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         - | 1300 | `				}` |
|   2393403 | 1301 | `			}` |
|  14690448 | 1302 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
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
|   8541942 | 1330 | `	}` |
|  29746110 | 1331 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|    434508 | 1332 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|         - | 1333 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|         - | 1334 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|        34 | 1335 | `		iVmOp = 0;` |
|        15 | 1336 | `	}` |
|  29746115 | 1337 | `	if( iVmOp > 0 ){` |
|  29687339 | 1338 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|    218739 | 1339 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|         - | 1340 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|     15625 | 1341 | `				iP1 = 1;` |
|      7815 | 1342 | `			}` |
|  29577972 | 1343 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|         - | 1344 | `			/* Namespace-qualify the class name for NEW */ {` |
|    872259 | 1345 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|    872259 | 1346 | `				VmInstr *pCallInstr = 0;` |
|    872259 | 1347 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|    860367 | 1348 | `					pCallInstr = pPeek;` |
|    860367 | 1349 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    430181 | 1350 | `				}` |
|    872259 | 1351 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|    856659 | 1352 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 1353 | `					sxu32 nLitForClass;` |
|    856659 | 1354 | `					VmCallArgMap *pCallNsMap = pCallInstr ? (VmCallArgMap *)pCallInstr->p3 : 0;` |
|         - | 1355 | `					/* If the CALL handler qualified the name with FUNCTION` |
|         - | 1356 | `					 * imports, recover the original literal (recorded in the` |
|         - | 1357 | `					 * arg map — OP_CALL's iP2 is the hasSpread flag, and` |
|         - | 1358 | `` 					 * misreading it as a literal index made `new C(...$args)` `` |
|         - | 1359 | `					 * fatal with "Class ' ' is not defined") and re-qualify` |
|         - | 1360 | `					 * with class imports. */` |
|    856659 | 1361 | `					if( pCallNsMap && pCallNsMap->nOrigNameLit > 0 ){` |
|        55 | 1362 | `						nLitForClass = pCallNsMap->nOrigNameLit - 1;` |
|        30 | 1363 | `					}else{` |
|    856609 | 1364 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|         - | 1365 | `					}` |
|    856659 | 1366 | `					pPeek->iP1 = 0;` |
|    856659 | 1367 | `					if( !bAbsolute ){` |
|         - | 1368 | `						/* self/static/parent are resolved at runtime against the` |
|         - | 1369 | `						 * current class — never namespace-qualify them (else` |
|         - | 1370 | ``						 * `new self` in namespace N becomes "N\self"). Mirrors the`` |
|         - | 1371 | `						 * instanceof (IS_A) guard below. */` |
|    852733 | 1372 | `						ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nLitForClass);` |
|    852733 | 1373 | `						int isSpecialNew = 0;` |
|    852733 | 1374 | `						if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|    837533 | 1375 | `							const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|    837533 | 1376 | `							sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|    837528 | 1377 | `							if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    841275 | 1378 | `								(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    418712 | 1379 | `								(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      7835 | 1380 | `								isSpecialNew = 1;` |
|      3915 | 1381 | `							}` |
|    422564 | 1382 | `						}` |
|    860333 | 1383 | `						if( isSpecialNew ){` |
|      7835 | 1384 | `							pPeek->iP2 = (sxi32)nLitForClass;` |
|      3920 | 1385 | `						}else{` |
|    837303 | 1386 | `							pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|         - | 1387 | `						}` |
|    422569 | 1388 | `					}else{` |
|      3931 | 1389 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|         - | 1390 | `					}` |
|    424527 | 1391 | `				}` |
|         - | 1392 | `			}` |
|    864659 | 1393 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    864659 | 1394 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|         - | 1395 | `				VmInstr *pPrev;` |
|    860367 | 1396 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|    860367 | 1397 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|         - | 1398 | `					/* Pop the call instruction, preserve named-arg map and` |
|         - | 1399 | `					 * the hasSpread flag (OP_NEW consumes the spread` |
|         - | 1400 | `					 * accumulator exactly like OP_CALL would have). */` |
|    860367 | 1401 | `					iP1 = pInstr->iP1;` |
|    860367 | 1402 | `					iP2 = pInstr->iP2;` |
|    860367 | 1403 | `					if( pInstr->p3 ){` |
|        65 | 1404 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|        30 | 1405 | `					}` |
|    860367 | 1406 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    430181 | 1407 | `				}` |
|    430186 | 1408 | `			}` |
|  29028678 | 1409 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|         - | 1410 | `			/* instanceof: right operand is a class name, not a constant.` |
|         - | 1411 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     82171 | 1412 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     82171 | 1413 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     82171 | 1414 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     82171 | 1415 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     82171 | 1416 | `				int isSpecialIs = 0;` |
|     82171 | 1417 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     82171 | 1418 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     82171 | 1419 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     82166 | 1420 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     82169 | 1421 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     41083 | 1422 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|        12 | 1423 | `						isSpecialIs = 1;` |
|         5 | 1424 | `					}` |
|     41083 | 1425 | `				}` |
|     82171 | 1426 | `				pInstr->iP1 = 0;` |
|     82171 | 1427 | `				if( !isSpecialIs && !bAbsolute ){` |
|     82151 | 1428 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     41073 | 1429 | `				}` |
|     41088 | 1430 | `			}` |
|  28555268 | 1431 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|         - | 1432 | `			/* Prevent constant expansion for member/property names.` |
|         - | 1433 | `			 * The right child (member name) was just compiled — its LOADC` |
|         - | 1434 | `			 * should not trigger constant lookup. */` |
|   6018977 | 1435 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   6018977 | 1436 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|   5777199 | 1437 | `				pInstr->iP1 = 0;` |
|   2888597 | 1438 | `			}` |
|   6018977 | 1439 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|         - | 1440 | `				/* Static member access,remember that */` |
|    421989 | 1441 | `				iP1 = 1;` |
|    421989 | 1442 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    421989 | 1443 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|    237871 | 1444 | `					p3 = pInstr->p3;` |
|    237871 | 1445 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    118933 | 1446 | `				}` |
|    210992 | 1447 | `			}` |
|         - | 1448 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|         - | 1449 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|         - | 1450 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|         - | 1451 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|   6018977 | 1452 | `			if( iP2 == PH7_MEMBER_READ ){` |
|   6018977 | 1453 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|        42 | 1454 | `					iP2 = PH7_MEMBER_UNSET;` |
|   6018957 | 1455 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|     70307 | 1456 | `					iP2 = PH7_MEMBER_ISSET;` |
|   5983786 | 1457 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|        17 | 1458 | `					iP2 = PH7_MEMBER_EMPTY;` |
|   5948627 | 1459 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|         - | 1460 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|   1119687 | 1461 | `					iP2 = PH7_MEMBER_WRITE;` |
|    559841 | 1462 | `				}` |
|   3009486 | 1463 | `			}` |
|   3009486 | 1464 | `		}` |
|         - | 1465 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|         - | 1466 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|         - | 1467 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|         - | 1468 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|         - | 1469 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
|  29679739 | 1470 | `		if( bFcc ){` |
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
|  29679739 | 1492 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|   7955405 | 1493 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|   3977700 | 1494 | `		}` |
|         - | 1495 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  29679739 | 1496 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  14839867 | 1497 | `	}` |
|  29738515 | 1498 | `	if( nJmpIdx > 0 ){` |
|         - | 1499 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|    838897 | 1500 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|    838897 | 1501 | `		if( pInstr ){` |
|    838897 | 1502 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    419446 | 1503 | `		}` |
|    419446 | 1504 | `	}` |
|  29738515 | 1505 | `	return rc;` |
|  38186567 | 1506 | `}` |
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
| 253526706 | 1529 | `static int GenStateTreeHasComma(ph7_expr_node *pNode)` |
|         5 | 1530 | `{` |
|         - | 1531 | `	ph7_expr_node **apArg;` |
|         - | 1532 | `	sxu32 n;` |
| 253526711 | 1533 | `	if( pNode == 0 ){` |
| 178238289 | 1534 | `		return 0;` |
|         - | 1535 | `	}` |
|  75288427 | 1536 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         6 | 1537 | `		return 1;` |
|         - | 1538 | `	}` |
|  75288418 | 1539 | `	if( GenStateTreeHasComma(pNode->pLeft) \|\| GenStateTreeHasComma(pNode->pRight)` |
|  75288419 | 1540 | `	 \|\| GenStateTreeHasComma(pNode->pCond) ){` |
|         6 | 1541 | `		return 1;` |
|         - | 1542 | `	}` |
|  75288419 | 1543 | `	apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  86119071 | 1544 | `	for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; n++ ){` |
|  10830657 | 1545 | `		if( GenStateTreeHasComma(apArg[n]) ){` |
|       ! 0 | 1546 | `			return 1;` |
|         - | 1547 | `		}` |
|   5415331 | 1548 | `	}` |
|  75288419 | 1549 | `	return 0;` |
| 126763358 | 1550 | `}` |
|  17205658 | 1551 | `PH7_PRIVATE sxi32 PH7_CompileExpr(` |
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
|  17205663 | 1565 | `	nExpr = 0;` |
|  17205663 | 1566 | `	pRoot = 0;` |
|         - | 1567 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|         - | 1568 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  17205663 | 1569 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  17205663 | 1570 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  17205663 | 1571 | `	SySetAlloc(&sExprNode,0x10);` |
|  17205663 | 1572 | `	rc = SXRET_OK;` |
|         - | 1573 | `	/* Delimit the expression */` |
|  17205663 | 1574 | `	pEnd = pGen->pIn;` |
|  17205663 | 1575 | `	iNest = 0;` |
| 134691595 | 1576 | `	while( pEnd < pGen->pEnd ){` |
| 128076189 | 1577 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 1578 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|      4777 | 1579 | `			iNest++;` |
| 128073803 | 1580 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|      4787 | 1581 | `			iNest--;` |
| 128069026 | 1582 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  10591185 | 1583 | `			if( iNest <= 0 ){` |
|  10590257 | 1584 | `				break;` |
|         - | 1585 | `			}` |
|       464 | 1586 | `		}` |
| 117485937 | 1587 | `		pEnd++;` |
|         5 | 1588 | `	}` |
|  17205663 | 1589 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|    741777 | 1590 | `		SyToken *pEnd2 = pGen->pIn;` |
|    741777 | 1591 | `		iNest = 0;` |
|         - | 1592 | `		/* Stop at the first comma */` |
|   1628905 | 1593 | `		while( pEnd2 < pEnd ){` |
|    887135 | 1594 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|     50805 | 1595 | `				iNest++;` |
|    861735 | 1596 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|     50805 | 1597 | `				iNest--;` |
|    810935 | 1598 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      6067 | 1599 | `				if( iNest <= 0 ){` |
|         3 | 1600 | `					break;` |
|         - | 1601 | `				}` |
|      3030 | 1602 | `			}` |
|    887133 | 1603 | `			pEnd2++;` |
|         5 | 1604 | `		}` |
|    741777 | 1605 | `		if( pEnd2 <pEnd ){` |
|         3 | 1606 | `			pEnd = pEnd2;` |
|         1 | 1607 | `		}` |
|    370886 | 1608 | `	}` |
|  17205663 | 1609 | `	if( pEnd > pGen->pIn ){` |
|  17182267 | 1610 | `		SyToken *pTmp = pGen->pEnd;` |
|         - | 1611 | `		/* Swap delimiter */` |
|  17182267 | 1612 | `		pGen->pEnd = pEnd;` |
|         - | 1613 | `		/* Try to get an expression tree */` |
|  17182267 | 1614 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  17182262 | 1615 | `		if( rc == SXRET_OK && pRoot && pGen->nCommaExprOk < 1` |
|  17006450 | 1616 | `		 && GenStateTreeHasComma(pRoot) ){` |
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
|  17182263 | 1630 | `		if( rc == SXRET_OK && pRoot ){` |
|  17182083 | 1631 | `			rc = SXRET_OK;` |
|  17182083 | 1632 | `			if( xTreeValidator ){` |
|         - | 1633 | `				/* Call the upper layer validator callback */` |
|   1038827 | 1634 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    519411 | 1635 | `			}` |
|  17182083 | 1636 | `			if( rc != SXERR_ABORT ){` |
|         - | 1637 | `				/* Generate code for the given tree */` |
|  17182083 | 1638 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|         - | 1639 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|         - | 1640 | `				 * expression so they short-circuit to its end. */` |
|  17182083 | 1641 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|   8591039 | 1642 | `			}` |
|  17182083 | 1643 | `			nExpr = 1;` |
|   8591039 | 1644 | `		}` |
|         - | 1645 | `		/* Release the whole tree */` |
|  17182263 | 1646 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|         - | 1647 | `		/* Synchronize token stream */` |
|  17182263 | 1648 | `		pGen->pEnd = pTmp;` |
|  17182263 | 1649 | `		pGen->pIn  = pEnd;` |
|  17182263 | 1650 | `		if( rc == SXERR_ABORT ){` |
|        24 | 1651 | `			SySetRelease(&sExprNode);` |
|        24 | 1652 | `			return SXERR_ABORT;` |
|         - | 1653 | `		}` |
|   8591119 | 1654 | `	}` |
|  17205639 | 1655 | `	SySetRelease(&sExprNode);` |
|  17205639 | 1656 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|   8602834 | 1657 | `}` |
|         - | 1658 | `/*` |
|         - | 1659 | ` * Return a pointer to the node construct handler associated` |
|         - | 1660 | ` * with a given node type [i.e: string,integer,float,...].` |
|         - | 1661 | ` */` |
|   9556346 | 1662 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|         5 | 1663 | `{` |
|   9556351 | 1664 | `	if( nNodeType & PH7_TK_NUM ){` |
|         - | 1665 | `		/* Numeric literal: Either real or integer */` |
|   3843215 | 1666 | `		return PH7_CompileNumLiteral;` |
|   5713141 | 1667 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|         - | 1668 | `		/* Double quoted string */` |
|    125621 | 1669 | `		return PH7_CompileString;` |
|   5587525 | 1670 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|         - | 1671 | `		/* Single quoted string */` |
|   5587401 | 1672 | `		return PH7_CompileSimpleString;` |
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
|   4778178 | 1684 | `}` |
|         - | 1685 | `/*` |
|         - | 1686 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|         - | 1687 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|         - | 1688 | ` * in write context" parse error.` |
|         - | 1689 | ` */` |
|     23508 | 1690 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|         5 | 1691 | `{` |
|         - | 1692 | `	sxi32 rc;` |
|     23513 | 1693 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|     23511 | 1694 | `		return SXRET_OK;` |
|         - | 1695 | `	}` |
|         5 | 1696 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|         2 | 1697 | `		pNode ? pNode->pStart->nLine : 1,` |
|         - | 1698 | `		"Can't use nullsafe operator in write context");` |
|         3 | 1699 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|     11759 | 1700 | `}` |
|         - | 1701 | `/*` |
|         - | 1702 | ` * Compile an unset() statement.` |
|         - | 1703 | ` * unset($var, $arr[$key], ...);` |
|         - | 1704 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|         - | 1705 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|         - | 1706 | ` * parent array before extracting the element to unset.` |
|         - | 1707 | ` */` |
|     26218 | 1708 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|         5 | 1709 | `{` |
|     26223 | 1710 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|     26223 | 1711 | `	sxu32 nIdx = 0;` |
|         - | 1712 | `	SyString sName;` |
|         - | 1713 | `	sxi32 rc;` |
|         - | 1714 | `	/* Jump the 'unset' keyword */` |
|     26223 | 1715 | `	pGen->pIn++;` |
|         - | 1716 | `	/* Save delimiter */` |
|     26223 | 1717 | `	pTmp = pGen->pEnd;` |
|         - | 1718 | `	/* Skip optional opening parenthesis and find the matching close */` |
|     26223 | 1719 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|     26223 | 1720 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 1721 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|         - | 1722 | `		SyToken *pClose;` |
|     26223 | 1723 | `		pGen->pIn++;   /* Skip '(' */` |
|     26223 | 1724 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|     26223 | 1725 | `		pEnd = pClose; /* Stop at ')' */` |
|     13109 | 1726 | `	}` |
|     26223 | 1727 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|         - | 1728 | `	/* Resolve the 'unset' builtin name once */` |
|     26223 | 1729 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|      3903 | 1730 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3903 | 1731 | `		if( pObj == 0 ){` |
|       ! 0 | 1732 | `			return SXERR_ABORT;` |
|         - | 1733 | `		}` |
|      3903 | 1734 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      3903 | 1735 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      1949 | 1736 | `	}` |
|         - | 1737 | `	/* Compile each comma-separated argument */` |
|     56295 | 1738 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|     30077 | 1739 | `		if( pGen->pIn < pNext ){` |
|         - | 1740 | `			/*` |
|         - | 1741 | ``			 * A PLAIN variable (`unset($x)`, exactly two tokens: '$' and the name) drops a`` |
|         - | 1742 | `			 * single NAME binding, which the unset() builtin cannot express — all it ever` |
|         - | 1743 | `			 * receives is the value's slot index, and unsetting the slot destroys whatever` |
|         - | 1744 | `			 * else aliases it. Emit OP_UNSET_VAR with the name instead. Subscripts and` |
|         - | 1745 | ``			 * properties (`unset($a[k])`, `unset($o->p)`) keep the existing path, which`` |
|         - | 1746 | `			 * already removes just the element/property.` |
|         - | 1747 | `			 */` |
|     30072 | 1748 | `			if( &pGen->pIn[2] == pNext` |
|     18318 | 1749 | `				&& (pGen->pIn[0].nType & PH7_TK_DOLLAR)` |
|      6569 | 1750 | `				&& (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         - | 1751 | `				SyString *pVarName;` |
|      9848 | 1752 | `				char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      6562 | 1753 | `					pGen->pIn[1].sData.zString,pGen->pIn[1].sData.nByte);` |
|      6567 | 1754 | `				pVarName = (SyString *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(SyString));` |
|      6567 | 1755 | `				if( zDup == 0 \|\| pVarName == 0 ){` |
|       ! 0 | 1756 | `					PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - | 1757 | `						"Fatal, PH7 is running out of memory");` |
|       ! 0 | 1758 | `					return SXERR_ABORT;` |
|         - | 1759 | `				}` |
|      6567 | 1760 | `				SyStringInitFromBuf(pVarName,zDup,pGen->pIn[1].sData.nByte);` |
|      6567 | 1761 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_UNSET_VAR,0,0,pVarName,0);` |
|      6567 | 1762 | `				pGen->pIn = pNext;` |
|      6567 | 1763 | `				if( pGen->pIn < pEnd ){` |
|      3855 | 1764 | `					pGen->pIn++; /* Jump the trailing comma */` |
|      1925 | 1765 | `				}` |
|      6567 | 1766 | `				continue;` |
|         - | 1767 | `			}` |
|     23515 | 1768 | `			pGen->pEnd = pNext;` |
|     23515 | 1769 | `			rc = PH7_CompileExpr(&(*pGen),` |
|         - | 1770 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|         - | 1771 | `				GenStateUnsetValidator);` |
|     23515 | 1772 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 1773 | `				return SXERR_ABORT;` |
|         - | 1774 | `			}` |
|     23515 | 1775 | `			if( rc != SXERR_EMPTY ){` |
|         - | 1776 | `				/* Emit call for this single argument */` |
|     23513 | 1777 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     23513 | 1778 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|     23513 | 1779 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     11754 | 1780 | `			}` |
|     11755 | 1781 | `		}` |
|         - | 1782 | `		/* Jump trailing commas */` |
|     23521 | 1783 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|         7 | 1784 | `			pNext++;` |
|         1 | 1785 | `		}` |
|     23515 | 1786 | `		pGen->pIn = pNext;` |
|         5 | 1787 | `	}` |
|         - | 1788 | `	/* Skip past the closing ')' if present */` |
|     26223 | 1789 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|     26223 | 1790 | `		pGen->pIn++;` |
|     13109 | 1791 | `	}` |
|         - | 1792 | `	/* Restore token stream */` |
|     26223 | 1793 | `	pGen->pEnd = pTmp;` |
|     26223 | 1794 | `	return SXRET_OK;` |
|     13114 | 1795 | `}` |
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
|   8499122 | 1829 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|         - | 1830 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|         - | 1831 | `	SyToken *pLookahed  /* Look-ahead token */` |
|         - | 1832 | `	)` |
|         5 | 1833 | `{` |
|   8499127 | 1834 | `	sxu32 n = 0;` |
|  34389996 | 1835 | `	for(;;){` |
|  68779997 | 1836 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|    544377 | 1837 | `			break;` |
|         - | 1838 | `		}` |
|  68235625 | 1839 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|   7954755 | 1840 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|       ! 0 | 1841 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|       ! 0 | 1842 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|         - | 1843 | `					/* 'static' (class context),return null */` |
|       ! 0 | 1844 | `					return 0;` |
|         - | 1845 | `				}` |
|       ! 0 | 1846 | `			}` |
|   7954750 | 1847 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|     11708 | 1848 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|      5861 | 1849 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|         - | 1850 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|         3 | 1851 | `				return 0;` |
|         - | 1852 | `			}` |
|         - | 1853 | `			/* Return a pointer to the handler.` |
|         - | 1854 | `			*/` |
|   7954753 | 1855 | `			return aLangConstruct[n].xConstruct;` |
|         - | 1856 | `		}` |
|  60280875 | 1857 | `		n++;` |
|         5 | 1858 | `	}` |
|    544377 | 1859 | `	if( pLookahed ){` |
|    544377 | 1860 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|     70293 | 1861 | `			return PH7_CompileClassInterface;` |
|    474089 | 1862 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|    407231 | 1863 | `			return PH7_CompileClass;` |
|     66863 | 1864 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      7901 | 1865 | `			return PH7_CompileTrait;` |
|         - | 1866 | `		}` |
|         - | 1867 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|         - | 1868 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|         - | 1869 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|         - | 1870 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     29481 | 1871 | `	}` |
|         - | 1872 | `	/* Not a language construct */` |
|     58967 | 1873 | `	return 0;` |
|   4249566 | 1874 | `}` |
|         - | 1875 | `/*` |
|         - | 1876 | ` * Check if the given keyword is in fact a PHP language construct.` |
|         - | 1877 | ` * Return TRUE on success. FALSE otheriwse.` |
|         - | 1878 | ` */` |
|     58964 | 1879 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|         5 | 1880 | `{` |
|         - | 1881 | `	int rc;` |
|     58969 | 1882 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     58969 | 1883 | `	if( rc == FALSE ){` |
|     58854 | 1884 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|     15962 | 1885 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|         - | 1886 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|         - | 1887 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|         - | 1888 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|         - | 1889 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|         - | 1890 | `			*/` |
|         - | 1891 | `			){` |
|     58851 | 1892 | `				rc = TRUE;` |
|     29423 | 1893 | `		}` |
|     29427 | 1894 | `	}` |
|     58969 | 1895 | `	return rc;` |
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
|  18231306 | 1910 | `PH7_PRIVATE void GenStateSetPendingDoc(ph7_gen_state *pGen)` |
|         5 | 1911 | `{` |
|  18231311 | 1912 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|  18231311 | 1913 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|  18231311 | 1914 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 1915 | `	sxu32 nIdx, n;` |
|  18231306 | 1916 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|   3444853 | 1917 | `	 \|\| pGen->pIn < pBase \|\| pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|         - | 1918 | `		/* Re-tokenized substream (string interpolation, synthesized code):` |
|         - | 1919 | `		 * indexes do not map to the sidecar */` |
|  14786465 | 1920 | `		return;` |
|         - | 1921 | `	}` |
|   3444851 | 1922 | `	nIdx = (sxu32)(pGen->pIn - pBase);` |
|         - | 1923 | `	/* Attributes must be adjacent to their declaration (unlike docblocks):` |
|         - | 1924 | `	 * reset at every boundary, then collect the groups keyed to this token. */` |
|   3444851 | 1925 | `	SySetReset(&pGen->aPendingAttrs);` |
|  10336303 | 1926 | `	for( n = 0 ; n < nT ; n++ ){` |
|   6891457 | 1927 | `		if( aT[n].nTokIdx != nIdx ){` |
|   6883489 | 1928 | `			continue;` |
|         - | 1929 | `		}` |
|      7973 | 1930 | `		if( aT[n].iKind == PH7_TRIVIA_DOC ){` |
|        29 | 1931 | `			pGen->sPendingDoc = aT[n].sText;` |
|      7961 | 1932 | `		}else if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|      7949 | 1933 | `			SySetPut(&pGen->aPendingAttrs,(const void *)&aT[n]);` |
|      3972 | 1934 | `		}` |
|      3989 | 1935 | `	}` |
|   9115658 | 1936 | `}` |
|         - | 1937 | `/*` |
|         - | 1938 | ` * Hand the pending docblock (if any) to a declaration: duplicate it into` |
|         - | 1939 | ` * the VM allocator (the raw script buffer dies after compilation) and` |
|         - | 1940 | ` * clear the pending slot so sibling declarations do not inherit it.` |
|         - | 1941 | ` */` |
|   4911170 | 1942 | `PH7_PRIVATE void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut)` |
|         5 | 1943 | `{` |
|         - | 1944 | `	char *zDup;` |
|   4911175 | 1945 | `	if( SyStringLength(&pGen->sPendingDoc) < 1 ){` |
|   4911155 | 1946 | `		return;` |
|         - | 1947 | `	}` |
|        35 | 1948 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        10 | 1949 | `		SyStringData(&pGen->sPendingDoc),SyStringLength(&pGen->sPendingDoc));` |
|        25 | 1950 | `	if( zDup ){` |
|        25 | 1951 | `		SyStringInitFromBuf(pOut,zDup,SyStringLength(&pGen->sPendingDoc));` |
|        10 | 1952 | `	}` |
|        25 | 1953 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|   2455590 | 1954 | `}` |
|         - | 1955 | `/*` |
|         - | 1956 | ` * Compile one recorded #[...] attribute group (the span between the group` |
|         - | 1957 | ` * delimiters) into ph7_attribute records appended to pOut. The span is` |
|         - | 1958 | ` * duplicated into the VM allocator FIRST (compiled bytecode and interned` |
|         - | 1959 | ` * names may point into the token text, which must outlive the raw script` |
|         - | 1960 | ` * buffer), then re-tokenized on its own. Each argument expression compiles` |
|         - | 1961 | ` * with the container-swap idiom into its own OP_DONE-terminated set,` |
|         - | 1962 | ` * evaluated lazily at ReflectionAttribute time (PHP semantics).` |
|         - | 1963 | ` */` |
|      7958 | 1964 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut)` |
|         5 | 1965 | `{` |
|         - | 1966 | `	SySet *pToken;` |
|         - | 1967 | `	SyToken *pIn, *pEnd, *pSavedIn, *pSavedEnd;` |
|         - | 1968 | `	char *zSpan;` |
|      7963 | 1969 | `	sxi32 rc = SXRET_OK;` |
|      7963 | 1970 | `	if( SyStringLength(&pTrivia->sText) < 1 ){` |
|       ! 0 | 1971 | `		return SXRET_OK;` |
|         - | 1972 | `	}` |
|     11942 | 1973 | `	zSpan = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      3979 | 1974 | `		SyStringData(&pTrivia->sText),SyStringLength(&pTrivia->sText));` |
|      7963 | 1975 | `	if( zSpan == 0 ){` |
|       ! 0 | 1976 | `		return SXRET_OK;` |
|         - | 1977 | `	}` |
|         - | 1978 | `	/* The token set must outlive compilation too: interned operands may` |
|         - | 1979 | `	 * reference token payloads. Pool-allocated, never released — bounded by` |
|         - | 1980 | `	 * the number of attribute declarations in the program. */` |
|      7963 | 1981 | `	pToken = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      7963 | 1982 | `	if( pToken == 0 ){` |
|       ! 0 | 1983 | `		return SXRET_OK;` |
|         - | 1984 | `	}` |
|      7963 | 1985 | `	SySetInit(pToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      7963 | 1986 | `	PH7_TokenizePHP(zSpan,SyStringLength(&pTrivia->sText),pTrivia->nLine,pToken,0);` |
|      7963 | 1987 | `	pIn = (SyToken *)SySetBasePtr(pToken);` |
|      7963 | 1988 | `	pEnd = &pIn[SySetUsed(pToken)];` |
|      7963 | 1989 | `	pSavedIn = pGen->pIn;` |
|      7963 | 1990 | `	pSavedEnd = pGen->pEnd;` |
|      7967 | 1991 | `	while( pIn < pEnd ){` |
|         - | 1992 | `		ph7_attribute sAttr;` |
|         - | 1993 | `		SyBlob sFQN;` |
|      7967 | 1994 | `		int bAbsolute = 0;` |
|      7967 | 1995 | `		SyZero(&sAttr,sizeof(sAttr));` |
|      7967 | 1996 | `		SySetInit(&sAttr.aArgs,&pGen->pVm->sAllocator,sizeof(ph7_attr_arg));` |
|      7967 | 1997 | `		sAttr.nLine = pIn->nLine;` |
|      7967 | 1998 | `		if( pIn->nType & PH7_TK_NSSEP ){` |
|        75 | 1999 | `			bAbsolute = 1;` |
|        75 | 2000 | `			pIn++;` |
|        35 | 2001 | `		}` |
|      7967 | 2002 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      7967 | 2003 | `		while( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      7967 | 2004 | `			SyBlobAppend(&sFQN,pIn->sData.zString,pIn->sData.nByte);` |
|      7967 | 2005 | `			pIn++;` |
|      7967 | 2006 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       ! 0 | 2007 | `				SyBlobAppend(&sFQN,"\\",1);` |
|       ! 0 | 2008 | `				pIn++;` |
|       ! 0 | 2009 | `				continue;` |
|         - | 2010 | `			}` |
|      7967 | 2011 | `			break;` |
|       ! 0 | 2012 | `		}` |
|      7967 | 2013 | `		if( SyBlobLength(&sFQN) < 1 ){` |
|         - | 2014 | `			/* Malformed group: stop quietly (the group was inert trivia before` |
|         - | 2015 | `			 * this feature; never turn it into a new fatal) */` |
|       ! 0 | 2016 | `			SyBlobRelease(&sFQN);` |
|       ! 0 | 2017 | `			break;` |
|         - | 2018 | `		}` |
|         - | 2019 | `		/* Resolve to an FQN: absolute names verbatim; else use-import alias,` |
|         - | 2020 | `		 * else current-namespace prefix (PHP attribute name resolution) */` |
|         - | 2021 | `		{` |
|      7967 | 2022 | `			const char *zName = (const char *)SyBlobData(&sFQN);` |
|      7967 | 2023 | `			sxu32 nName = SyBlobLength(&sFQN);` |
|      7967 | 2024 | `			char *zDup = 0;` |
|      7967 | 2025 | `			if( !bAbsolute ){` |
|      7897 | 2026 | `				SyHashEntry *pImp = SyHashGet(&pGen->hUseImports,(const void *)zName,nName);` |
|      7897 | 2027 | `				if( pImp ){` |
|       ! 0 | 2028 | `					const char *zFqn = (const char *)pImp->pUserData;` |
|       ! 0 | 2029 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFqn,SyStrlen(zFqn));` |
|       ! 0 | 2030 | `					if( zDup ){` |
|       ! 0 | 2031 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyStrlen(zDup));` |
|       ! 0 | 2032 | `					}` |
|      7897 | 2033 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
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
|      3946 | 2046 | `			}` |
|      7967 | 2047 | `			if( SyStringLength(&sAttr.sName) < 1 ){` |
|      7967 | 2048 | `				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|      7967 | 2049 | `				if( zDup ){` |
|      7967 | 2050 | `					SyStringInitFromBuf(&sAttr.sName,zDup,nName);` |
|      3981 | 2051 | `				}` |
|      3981 | 2052 | `			}` |
|         - | 2053 | `		}` |
|      7967 | 2054 | `		SyBlobRelease(&sFQN);` |
|      7967 | 2055 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 2056 | `			SyToken *pArgsEnd;` |
|      7865 | 2057 | `			pIn++;` |
|      7865 | 2058 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pArgsEnd);` |
|     15739 | 2059 | `			while( pIn < pArgsEnd ){` |
|      7879 | 2060 | `				SyToken *pArgStart = pIn, *pArgStop = pIn;` |
|      7879 | 2061 | `				sxi32 iDepth = 0;` |
|         - | 2062 | `				ph7_attr_arg sArgRec;` |
|     78237 | 2063 | `				while( pArgStop < pArgsEnd ){` |
|     70379 | 2064 | `					if( pArgStop->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        11 | 2065 | `						iDepth++;` |
|     70374 | 2066 | `					}else if( pArgStop->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        11 | 2067 | `						iDepth--;` |
|     70364 | 2068 | `					}else if( (pArgStop->nType & PH7_TK_COMMA) && iDepth == 0 ){` |
|        17 | 2069 | `						break;` |
|         - | 2070 | `					}` |
|     70363 | 2071 | `					pArgStop++;` |
|         5 | 2072 | `				}` |
|      7879 | 2073 | `				SyZero(&sArgRec,sizeof(sArgRec));` |
|      7879 | 2074 | `				SySetInit(&sArgRec.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      7874 | 2075 | `				if( pArgStart < pArgStop && (pArgStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|      7856 | 2076 | `				 && &pArgStart[1] < pArgStop && (pArgStart[1].nType & PH7_TK_COLON) ){` |
|        28 | 2077 | `					char *zN = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|         9 | 2078 | `						pArgStart->sData.zString,pArgStart->sData.nByte);` |
|        19 | 2079 | `					if( zN ){` |
|        19 | 2080 | `						SyStringInitFromBuf(&sArgRec.sName,zN,pArgStart->sData.nByte);` |
|         9 | 2081 | `					}` |
|        19 | 2082 | `					pArgStart += 2;` |
|         9 | 2083 | `				}` |
|      7879 | 2084 | `				if( pArgStart < pArgStop ){` |
|         - | 2085 | `					SySet *pInstrContainer;` |
|      7879 | 2086 | `					pGen->pIn = pArgStart;` |
|      7879 | 2087 | `					pGen->pEnd = pArgStop;` |
|      7879 | 2088 | `					pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      7879 | 2089 | `					PH7_VmSetByteCodeContainer(pGen->pVm,&sArgRec.aByteCode);` |
|      7879 | 2090 | `					rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      7879 | 2091 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      7879 | 2092 | `					PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      7879 | 2093 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 2094 | `						pGen->pIn = pSavedIn;` |
|       ! 0 | 2095 | `						pGen->pEnd = pSavedEnd;` |
|       ! 0 | 2096 | `						return SXERR_ABORT;` |
|         - | 2097 | `					}` |
|      7879 | 2098 | `					SySetPut(&sAttr.aArgs,(const void *)&sArgRec);` |
|      3937 | 2099 | `				}` |
|      7879 | 2100 | `				pIn = pArgStop;` |
|      7879 | 2101 | `				if( pIn < pArgsEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        17 | 2102 | `					pIn++;` |
|         8 | 2103 | `				}` |
|         5 | 2104 | `			}` |
|      7865 | 2105 | `			pIn = (pArgsEnd < pEnd) ? &pArgsEnd[1] : pEnd;` |
|      3930 | 2106 | `		}` |
|      7967 | 2107 | `		SySetPut(pOut,(const void *)&sAttr);` |
|      7967 | 2108 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|         5 | 2109 | `			pIn++;` |
|         5 | 2110 | `			continue;` |
|         - | 2111 | `		}` |
|      7963 | 2112 | `		break;` |
|       ! 0 | 2113 | `	}` |
|      7963 | 2114 | `	pGen->pIn = pSavedIn;` |
|      7963 | 2115 | `	pGen->pEnd = pSavedEnd;` |
|      7963 | 2116 | `	return SXRET_OK;` |
|      3984 | 2117 | `}` |
|         - | 2118 | `/*` |
|         - | 2119 | ` * Hand the pending attribute groups (if any) to a declaration: compile` |
|         - | 2120 | ` * every recorded group into pOut and clear the pending list.` |
|         - | 2121 | ` */` |
|   4911174 | 2122 | `PH7_PRIVATE sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut)` |
|         5 | 2123 | `{` |
|   4911179 | 2124 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|         - | 2125 | `	sxu32 n;` |
|         - | 2126 | `	sxi32 rc;` |
|   4919123 | 2127 | `	for( n = 0 ; n < SySetUsed(&pGen->aPendingAttrs) ; n++ ){` |
|      7949 | 2128 | `		rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|      7949 | 2129 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 2130 | `			return SXERR_ABORT;` |
|         - | 2131 | `		}` |
|      3977 | 2132 | `	}` |
|   4911179 | 2133 | `	SySetReset(&pGen->aPendingAttrs);` |
|   4911179 | 2134 | `	return SXRET_OK;` |
|   2455592 | 2135 | `}` |
|         - | 2136 | `/*` |
|         - | 2137 | ` * Compile the attribute groups keyed to the given token (a parameter's` |
|         - | 2138 | ` * first token inside a signature) into pOut. Parameters are parsed from` |
|         - | 2139 | ` * the main token stream, so the sidecar indexes map directly.` |
|         - | 2140 | ` */` |
|   2467396 | 2141 | `PH7_PRIVATE sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut)` |
|         5 | 2142 | `{` |
|   2467401 | 2143 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|   2467401 | 2144 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|   2467401 | 2145 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 2146 | `	sxu32 nIdx, n;` |
|         - | 2147 | `	sxi32 rc;` |
|   2467396 | 2148 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|    557723 | 2149 | `	 \|\| pTok < pBase \|\| pTok >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|   1909683 | 2150 | `		return SXRET_OK;` |
|         - | 2151 | `	}` |
|    557723 | 2152 | `	nIdx = (sxu32)(pTok - pBase);` |
|   1673203 | 2153 | `	for( n = 0 ; n < nT ; n++ ){` |
|   1115485 | 2154 | `		if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|        16 | 2155 | `			rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|        16 | 2156 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 2157 | `				return SXERR_ABORT;` |
|         - | 2158 | `			}` |
|         7 | 2159 | `		}` |
|    557745 | 2160 | `	}` |
|    557723 | 2161 | `	return SXRET_OK;` |
|   1233703 | 2162 | `}` |
|  13370578 | 2163 | `PH7_PRIVATE sxi32 GenStateCompileChunk(` |
|         - | 2164 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 2165 | `	sxi32 iFlags         /* Compile flags */` |
|         - | 2166 | `	)` |
|         5 | 2167 | `{` |
|         - | 2168 | `	ProcLangConstruct xCons;` |
|         - | 2169 | `	sxi32 rc;` |
|  13370583 | 2170 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|   7779295 | 2171 | `	for(;;){` |
|  14464589 | 2172 | `		int bStmtIsDeclare = 0;` |
|  14464589 | 2173 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 2174 | `			/* No more input to process */` |
|     91707 | 2175 | `			break;` |
|         - | 2176 | `		}` |
|         - | 2177 | `		/* Bind a directly-preceding docblock to this statement */` |
|  14372887 | 2178 | `		GenStateSetPendingDoc(&(*pGen));` |
|  14372887 | 2179 | `		if( SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|         - | 2180 | `			/* php: a statement-position attribute group must be followed by a` |
|         - | 2181 | ``			 * declaration (function/class-like/const) — `#[A] $x = 1;` is a`` |
|         - | 2182 | `` 			 * parse error, never a silent discard. `static`/`fn`/`function` `` |
|         - | 2183 | ``			 * cover bare closure-expression statements; `readonly`/`enum` are`` |
|         - | 2184 | `			 * context-sensitive IDs handled by the modified-class/enum scans. */` |
|      7861 | 2185 | `			int bAttrTarget = 0;` |
|      7856 | 2186 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd)` |
|      3963 | 2187 | `			 \|\| GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|      7801 | 2188 | `				bAttrTarget = 1;` |
|      3959 | 2189 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|        61 | 2190 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        60 | 2191 | `				if( nKw == PH7_TKWRD_FUNCTION \|\| nKw == PH7_TKWRD_CLASS` |
|        16 | 2192 | `				 \|\| nKw == PH7_TKWRD_INTERFACE \|\| nKw == PH7_TKWRD_TRAIT` |
|         4 | 2193 | `				 \|\| nKw == PH7_TKWRD_ABSTRACT \|\| nKw == PH7_TKWRD_FINAL` |
|         4 | 2194 | `				 \|\| nKw == PH7_TKWRD_CONST \|\| nKw == PH7_TKWRD_STATIC` |
|         1 | 2195 | `				 \|\| nKw == PH7_TKWRD_FN ){` |
|        61 | 2196 | `					bAttrTarget = 1;` |
|        30 | 2197 | `				}` |
|        30 | 2198 | `			}` |
|      7861 | 2199 | `			if( !bAttrTarget ){` |
|       ! 0 | 2200 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 2201 | `					"syntax error, unexpected token \"%z\" after attribute group; expecting a declaration",` |
|       ! 0 | 2202 | `					&pGen->pIn->sData);` |
|       ! 0 | 2203 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 2204 | `					break;` |
|         - | 2205 | `				}` |
|       ! 0 | 2206 | `				SySetReset(&pGen->aPendingAttrs);` |
|       ! 0 | 2207 | `			}` |
|      3928 | 2208 | `		}` |
|         - | 2209 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|         - | 2210 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  14372887 | 2211 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   8542075 | 2212 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   8542075 | 2213 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|        49 | 2214 | `				bStmtIsDeclare = 1;` |
|        22 | 2215 | `			}` |
|   4271035 | 2216 | `		}` |
|  14372887 | 2217 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|         - | 2218 | `			/* Any non-declare top-level statement locks the strict_types` |
|         - | 2219 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|   1094005 | 2220 | `			pGen->bStrictTypesLocked = 1;` |
|    547000 | 2221 | `		}` |
|  14372887 | 2222 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 2223 | `			/* Compile block */` |
|      3943 | 2224 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      3943 | 2225 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 2226 | `				break;` |
|         - | 2227 | `			}` |
|      1974 | 2228 | `		}else{` |
|  14368949 | 2229 | `			xCons = 0;` |
|  14368949 | 2230 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|         - | 2231 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|         - | 2232 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|         - | 2233 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|     42979 | 2234 | `				xCons = PH7_CompileClassModifiers;` |
|  14347462 | 2235 | `			}else if( GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|         - | 2236 | ``				/* `enum Name …` (PHP 8.1) — `enum` is a context-sensitive ID,`` |
|         - | 2237 | `				 * so it is detected here rather than the keyword dispatcher. */` |
|      3939 | 2238 | `				xCons = PH7_CompileEnum;` |
|  14324008 | 2239 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   8499127 | 2240 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         - | 2241 | `				/* Try to extract a language construct handler */` |
|   8499127 | 2242 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|   8499127 | 2243 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
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
|  10072480 | 2255 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|    430223 | 2256 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|         - | 2257 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|       117 | 2258 | `				xCons = PH7_CompileLabel;` |
|        56 | 2259 | `			}` |
|  14368949 | 2260 | `			if( xCons == 0 ){` |
|         - | 2261 | `				/* Assume an expression an try to compile it */` |
|   5881763 | 2262 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   5881763 | 2263 | `				if(  rc != SXERR_EMPTY ){` |
|         - | 2264 | `					/* Pop l-value */` |
|   5881613 | 2265 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|   2940804 | 2266 | `				}` |
|   2940884 | 2267 | `			}else{` |
|         - | 2268 | `				/* Go compile the sucker */` |
|   8487191 | 2269 | `				rc = xCons(&(*pGen));` |
|         - | 2270 | `			}` |
|  14368949 | 2271 | `			if( rc == SXERR_ABORT ){` |
|         - | 2272 | `				/* Request to abort compilation */` |
|        42 | 2273 | `				break;` |
|         - | 2274 | `			}` |
|         - | 2275 | `		}` |
|         - | 2276 | `		/* Ignore trailing semi-colons ';' */` |
|  24675549 | 2277 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  10302705 | 2278 | `			pGen->pIn++;` |
|         5 | 2279 | `		}` |
|  14372849 | 2280 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|         - | 2281 | `			/* Compile a single statement and return */` |
|  13278843 | 2282 | `			break;` |
|         - | 2283 | `		}` |
|         - | 2284 | `		/* LOOP ONE */` |
|         - | 2285 | `		/* LOOP TWO */` |
|         - | 2286 | `		/* LOOP THREE */` |
|         - | 2287 | `		/* LOOP FOUR */` |
|         5 | 2288 | `	}` |
|         - | 2289 | `	/* Return compilation status */` |
|  13370583 | 2290 | `	return rc;` |
|         5 | 2291 | `}` |
|         - | 2292 | `/*` |
|         - | 2293 | ` * Compile a Raw PHP chunk.` |
|         - | 2294 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 2295 | ` * takes care of generating the appropriate error message.` |
|         - | 2296 | ` */` |
|     91742 | 2297 | `static sxi32 PH7_CompilePHP(` |
|         - | 2298 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 2299 | `	SySet *pTokenSet,     /* Token set */` |
|         - | 2300 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|         - | 2301 | `	)` |
|         5 | 2302 | `{` |
|     91747 | 2303 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|         - | 2304 | `	sxi32 rc;` |
|         - | 2305 | `	/* Reset the token set (and its trivia sidecar) */` |
|     91747 | 2306 | `	SySetReset(&(*pTokenSet));` |
|     91747 | 2307 | `	SySetReset(&pGen->aTrivia);` |
|         - | 2308 | `	/* Mark as the default token set */` |
|     91747 | 2309 | `	pGen->pTokenSet = &(*pTokenSet);` |
|         - | 2310 | `	/* Advance the stream cursor */` |
|     91747 | 2311 | `	pGen->pRawIn++;` |
|         - | 2312 | `	/* Tokenize the PHP chunk first */` |
|     91747 | 2313 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet),&pGen->aTrivia);` |
|         - | 2314 | `	/* Point to the head and tail of the token stream. */` |
|     91747 | 2315 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     91747 | 2316 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|     91747 | 2317 | `	if( is_expr ){` |
|       ! 0 | 2318 | `		rc = SXERR_EMPTY;` |
|       ! 0 | 2319 | `		if( pGen->pIn < pGen->pEnd ){` |
|         - | 2320 | `			/* A simple expression,compile it */` |
|       ! 0 | 2321 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|       ! 0 | 2322 | `		}` |
|         - | 2323 | `		/* Emit the DONE instruction */` |
|       ! 0 | 2324 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       ! 0 | 2325 | `		return SXRET_OK;` |
|         - | 2326 | `	}` |
|     91747 | 2327 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
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
|     91745 | 2359 | `	rc = GenStateCompileChunk(pGen,0);` |
|         - | 2360 | `	/* Fix exceptions jumps */` |
|     91745 | 2361 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - | 2362 | `	/* Fix gotos now, the jump destination is resolved */` |
|     91745 | 2363 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|         3 | 2364 | `		rc = SXERR_ABORT;` |
|         1 | 2365 | `	}` |
|         - | 2366 | `	/* Reset container */` |
|     91745 | 2367 | `	SySetReset(&pGen->aGoto);` |
|     91745 | 2368 | `	SySetReset(&pGen->aLabel);` |
|     91745 | 2369 | `	SySetReset(&pGen->aNullsafeJmp);` |
|         - | 2370 | `	/* Compilation result */` |
|     91745 | 2371 | `	return rc;` |
|     45876 | 2372 | `}` |
|         - | 2373 | `/*` |
|         - | 2374 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|         - | 2375 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|         - | 2376 | ` * This is the only compile interface exported from this file.` |
|         - | 2377 | ` */` |
|     94906 | 2378 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
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
|     94911 | 2394 | `	sxu32 nBaseLine = 1;` |
|     94911 | 2395 | `	if( pScript->nByte < 1 ){` |
|         - | 2396 | `		/* Nothing to compile */` |
|       ! 0 | 2397 | `		return PH7_OK;` |
|         - | 2398 | `	}` |
|         - | 2399 | `	/* php skips a "#!" shebang on the first line of a CLI script: consume it` |
|         - | 2400 | `	 * (including its newline) so it is not echoed as inline text, and bump the` |
|         - | 2401 | `	 * base line to 2 so the code below still reports php-matching line numbers. */` |
|     94911 | 2402 | `	if( pScript->nByte >= 2 && pScript->zString[0]=='#' && pScript->zString[1]=='!' ){` |
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
|     94911 | 2416 | `	pCodeGen = &pVm->sCodeGen;` |
|         - | 2417 | ``	/* aPhpToken below is a LOCAL token set: once it is released at `cleanup`, any`` |
|         - | 2418 | `	 * pGen->pIn/pEnd still pointing into it DANGLE. PH7_VmEmitInstr reads pIn to stamp` |
|         - | 2419 | `	 * each instruction's source line, and instructions are still emitted after this` |
|         - | 2420 | `	 * function returns (VmEvalChunk's trailing OP_DONE) -- a use-after-free ASan caught` |
|         - | 2421 | `	 * immediately. Save the caller's cursor and restore it on the way out, so an` |
|         - | 2422 | `	 * enclosing compile keeps its (live) tokens and the top level goes back to NULL. */` |
|     94911 | 2423 | `	pSavedIn = pCodeGen->pIn;` |
|     94911 | 2424 | `	pSavedEnd = pCodeGen->pEnd;` |
|     94911 | 2425 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|     94911 | 2426 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|     94911 | 2427 | `	pCodeGen->bStrictTypes = 0;` |
|     94911 | 2428 | `	pCodeGen->bStrictTypesLocked = 0;` |
|         - | 2429 | `	/* Initialize the tokens containers */` |
|     94911 | 2430 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|     94911 | 2431 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|     94911 | 2432 | `	SySetAlloc(&aPhpToken,0xc0);` |
|     94911 | 2433 | `	is_expr = 0;` |
|     94911 | 2434 | `	if( iFlags & PH7_PHP_ONLY ){` |
|         - | 2435 | `		SyToken sTmp;` |
|         - | 2436 | `		/* PHP only: -*/` |
|     81989 | 2437 | `		sTmp.nLine = 1;` |
|     81989 | 2438 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|     81989 | 2439 | `		sTmp.pUserData = 0;` |
|     81989 | 2440 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|     81989 | 2441 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|     81989 | 2442 | `		if( iFlags & PH7_PHP_EXPR ){` |
|         - | 2443 | `			/* A simple PHP expression */` |
|       ! 0 | 2444 | `			is_expr = 1;` |
|       ! 0 | 2445 | `		}` |
|     40997 | 2446 | `	}else{` |
|         - | 2447 | `		/* Tokenize raw text */` |
|     12927 | 2448 | `		SySetAlloc(&aRawToken,32);` |
|     12927 | 2449 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken,nBaseLine);` |
|         - | 2450 | `	}` |
|         - | 2451 | `	/* Process high-level tokens */` |
|     94911 | 2452 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|     94911 | 2453 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|     94911 | 2454 | `	rc = PH7_OK;` |
|     94911 | 2455 | `	if( is_expr ){` |
|         - | 2456 | `		/* Compile the expression */` |
|       ! 0 | 2457 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|       ! 0 | 2458 | `		goto cleanup;` |
|         - | 2459 | `	}` |
|     94911 | 2460 | `	nObjIdx = 0;` |
|         - | 2461 | `	/* Each compilation unit starts in the global namespace.` |
|         - | 2462 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|         - | 2463 | `	 * preventing namespace bleeding across include()d files. */` |
|     94911 | 2464 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|         - | 2465 | `	/* Start the compilation process */` |
|     53918 | 2466 | `	for(;;){` |
|    199543 | 2467 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|     94871 | 2468 | `			break; /* No more tokens to process */` |
|         - | 2469 | `		}` |
|    104677 | 2470 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|         - | 2471 | `			/* Compile the PHP chunk */` |
|     91747 | 2472 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|     91747 | 2473 | `			if( rc == SXERR_ABORT ){` |
|        44 | 2474 | `				break;` |
|         - | 2475 | `			}` |
|     91707 | 2476 | `			continue;` |
|         - | 2477 | `		}` |
|         - | 2478 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|     12935 | 2479 | `		nRawObj = 0;` |
|     25865 | 2480 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|         - | 2481 | `			/* Consume the raw chunk without any processing */` |
|     12935 | 2482 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|     12935 | 2483 | `			if( pRawObj == 0 ){` |
|       ! 0 | 2484 | `				rc = SXERR_MEM;` |
|       ! 0 | 2485 | `				break;` |
|         - | 2486 | `			}` |
|         - | 2487 | `			/* Mark as constant and emit the load constant instruction */` |
|     12935 | 2488 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|     12935 | 2489 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     12935 | 2490 | `			++nRawObj;` |
|     12935 | 2491 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|         5 | 2492 | `		}` |
|     12935 | 2493 | `		if( nRawObj > 0 ){` |
|         - | 2494 | `			/* Emit the consume instruction */` |
|     12935 | 2495 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|      6465 | 2496 | `		}` |
|     47458 | 2497 | `	}` |
|     47453 | 2498 | `cleanup:` |
|         - | 2499 | `	/* Drop the cursor into the token set BEFORE the set is freed (see above). */` |
|     94911 | 2500 | `	pCodeGen->pIn = pSavedIn;` |
|     94911 | 2501 | `	pCodeGen->pEnd = pSavedEnd;` |
|     94911 | 2502 | `	SySetRelease(&aRawToken);` |
|     94911 | 2503 | `	SySetRelease(&aPhpToken);` |
|         - | 2504 | `	/* Restore outer file's strict_types scope */` |
|     94911 | 2505 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|     94911 | 2506 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|     94911 | 2507 | `	return rc;` |
|     47458 | 2508 | `}` |
|         - | 2509 | `/*` |
|         - | 2510 | ` * Utility routines.Initialize the code generator.` |
|         - | 2511 | ` */` |
|      3898 | 2512 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|         - | 2513 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 2514 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 2515 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 2516 | `	)` |
|         5 | 2517 | `{` |
|      3903 | 2518 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 2519 | `	/* Zero the structure */` |
|      3903 | 2520 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|         - | 2521 | `	/* Initial state */` |
|      3903 | 2522 | `	pGen->pVm  = &(*pVm);` |
|      3903 | 2523 | `	pGen->xErr = xErr;` |
|      3903 | 2524 | `	pGen->pErrData = pErrData;` |
|      3903 | 2525 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|      3903 | 2526 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|      3903 | 2527 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|      3903 | 2528 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|      3903 | 2529 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|      3903 | 2530 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3903 | 2531 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3903 | 2532 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|      3903 | 2533 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|         - | 2534 | `	/* Error log buffer */` |
|      3903 | 2535 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|         - | 2536 | `	/* General purpose working buffer */` |
|      3903 | 2537 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|         - | 2538 | `	/* Namespace state */` |
|      3903 | 2539 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|      3903 | 2540 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|      3903 | 2541 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|      3903 | 2542 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 2543 | `	/* Create the global scope */` |
|      3903 | 2544 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|         - | 2545 | `	/* Point to the global scope */` |
|      3903 | 2546 | `	pGen->pCurrent = &pGen->sGlobal;` |
|      3903 | 2547 | `	return SXRET_OK;` |
|         5 | 2548 | `}` |
|         - | 2549 | `/*` |
|         - | 2550 | ` * Utility routines. Reset the code generator to it's initial state.` |
|         - | 2551 | ` */` |
|     98316 | 2552 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|         - | 2553 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 2554 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 2555 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 2556 | `	)` |
|         5 | 2557 | `{` |
|     98321 | 2558 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 2559 | `	GenBlock *pBlock,*pParent;` |
|         - | 2560 | `	/* Reset state */` |
|     98321 | 2561 | `	SySetReset(&pGen->aLabel);` |
|     98321 | 2562 | `	SySetReset(&pGen->aGoto);` |
|     98321 | 2563 | `	SySetReset(&pGen->aNullsafeJmp);` |
|     98321 | 2564 | `	SySetReset(&pGen->aTrivia);` |
|     98321 | 2565 | `	SySetReset(&pGen->aPendingAttrs);` |
|     98321 | 2566 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|     98321 | 2567 | `	SyBlobRelease(&pGen->sErrBuf);` |
|     98321 | 2568 | `	SyBlobRelease(&pGen->sWorker);` |
|     98321 | 2569 | `	SyBlobRelease(&pGen->sNamespace);` |
|     98321 | 2570 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|     98321 | 2571 | `	SyHashRelease(&pGen->hUseImports);` |
|     98321 | 2572 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|     98321 | 2573 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     98321 | 2574 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|     98321 | 2575 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     98321 | 2576 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 2577 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|         - | 2578 | `	 * They intern variable names and literal strings that are referenced by` |
|         - | 2579 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|         - | 2580 | `	 * Releasing them would either leak the interned strings or require freeing` |
|         - | 2581 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|         - | 2582 | `	 * number of unique names, which is acceptable. */` |
|         - | 2583 | `	/* Point to the global scope */` |
|     98321 | 2584 | `	pBlock = pGen->pCurrent;` |
|     98321 | 2585 | `	while( pBlock->pParent != 0 ){` |
|       ! 0 | 2586 | `		pParent = pBlock->pParent;` |
|       ! 0 | 2587 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 2588 | `		pBlock = pParent;` |
|       ! 0 | 2589 | `	}` |
|     98321 | 2590 | `	pGen->xErr = xErr;` |
|     98321 | 2591 | `	pGen->pErrData = pErrData;` |
|     98321 | 2592 | `	pGen->pCurrent = &pGen->sGlobal;` |
|     98321 | 2593 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|     98321 | 2594 | `	pGen->pIn = pGen->pEnd = 0;` |
|     98321 | 2595 | `	pGen->nErr = 0;` |
|     98321 | 2596 | `	return SXRET_OK;` |
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
|         5 | 2648 | `	pGen->bInGenerator = 0;` |
|         5 | 2649 | `	pGen->bStrictTypes = 0;` |
|         5 | 2650 | `	pGen->bStrictTypesLocked = 0;` |
|         5 | 2651 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|         5 | 2652 | `	pGen->xErr = xErr;` |
|         5 | 2653 | `	pGen->pErrData = pErrData;` |
|         5 | 2654 | `}` |
|         - | 2655 | `/*` |
|         - | 2656 | ` * Restore the outer compile-position state saved by PH7_CompilerSaveState,` |
|         - | 2657 | ` * releasing the nested unit's position containers first. The shared` |
|         - | 2658 | ` * hLiteral/hNumLiteral/hVar tables (grown by the nested compile) are carried` |
|         - | 2659 | ` * forward, NOT rolled back to the snapshot's stale headers.` |
|         - | 2660 | ` */` |
|         4 | 2661 | `PH7_PRIVATE void PH7_CompilerRestoreState(ph7_vm *pVm,ph7_gen_state *pSaved)` |
|         1 | 2662 | `{` |
|         5 | 2663 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 2664 | `	GenBlock *pBlock,*pParent;` |
|         - | 2665 | `	SyHash hVar,hLiteral,hNumLiteral;` |
|         - | 2666 | `	/* Free any nested blocks left open (e.g. an aborted nested compile), then the` |
|         - | 2667 | `	 * nested global block's own fixup sets. */` |
|         5 | 2668 | `	pBlock = pGen->pCurrent;` |
|         5 | 2669 | `	while( pBlock && pBlock->pParent != 0 ){` |
|       ! 0 | 2670 | `		pParent = pBlock->pParent;` |
|       ! 0 | 2671 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 2672 | `		pBlock = pParent;` |
|       ! 0 | 2673 | `	}` |
|         5 | 2674 | `	GenStateReleaseBlock(&pGen->sGlobal);` |
|         - | 2675 | `	/* Release the nested unit's position containers. */` |
|         5 | 2676 | `	SySetRelease(&pGen->aLabel);` |
|         5 | 2677 | `	SySetRelease(&pGen->aGoto);` |
|         5 | 2678 | `	SySetRelease(&pGen->aNullsafeJmp);` |
|         5 | 2679 | `	SySetRelease(&pGen->aLoopParent);` |
|         5 | 2680 | `	SySetRelease(&pGen->aTrivia);` |
|         5 | 2681 | `	SySetRelease(&pGen->aPendingAttrs);` |
|         5 | 2682 | `	SyBlobRelease(&pGen->sWorker);` |
|         5 | 2683 | `	SyBlobRelease(&pGen->sErrBuf);` |
|         5 | 2684 | `	SyBlobRelease(&pGen->sNamespace);` |
|         5 | 2685 | `	SyHashRelease(&pGen->hUseImports);` |
|         5 | 2686 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|         5 | 2687 | `	SyHashRelease(&pGen->hUseConstImports);` |
|         - | 2688 | `	/* Preserve the (possibly grown) shared intern tables across the restore. */` |
|         5 | 2689 | `	hVar = pGen->hVar;` |
|         5 | 2690 | `	hLiteral = pGen->hLiteral;` |
|         5 | 2691 | `	hNumLiteral = pGen->hNumLiteral;` |
|         5 | 2692 | `	*pGen = *pSaved;` |
|         5 | 2693 | `	pGen->hVar = hVar;` |
|         5 | 2694 | `	pGen->hLiteral = hLiteral;` |
|         5 | 2695 | `	pGen->hNumLiteral = hNumLiteral;` |
|         5 | 2696 | `}` |
|         - | 2697 | `/*` |
|         - | 2698 | ` * Raise php's parse error for an unexpected token: E_PARSE with the exact text` |
|         - | 2699 | ` * php's parser prints, e.g.` |
|         - | 2700 | ` *` |
|         - | 2701 | ` *   syntax error, unexpected token ";", expecting "{"` |
|         - | 2702 | ` *   syntax error, unexpected identifier "invalid", expecting "("` |
|         - | 2703 | ` *   syntax error, unexpected end of file` |
|         - | 2704 | ` *` |
|         - | 2705 | ` * php names the token by CLASS, not just by text: an identifier, a variable and` |
|         - | 2706 | ` * a number each get their own noun, while everything else (keywords, operators,` |
|         - | 2707 | ` * punctuation) is a "token". zExpecting is the optional ", expecting ..." tail` |
|         - | 2708 | ` * — pass NULL when the site cannot say what it wanted (php often can't either).` |
|         - | 2709 | ` * pTok == NULL means the input ran out: "unexpected end of file".` |
|         - | 2710 | ` *` |
|         - | 2711 | ` * PHL's hand-written recursive-descent parser has no bison expectation sets, so` |
|         - | 2712 | ` * a site can only claim an "expecting" clause it genuinely knows; every clause` |
|         - | 2713 | ` * emitted here was verified against php 8.5.7 for the construct in question.` |
|         - | 2714 | ` */` |
|       194 | 2715 | `PH7_PRIVATE sxi32 PH7_GenSyntaxError(` |
|         - | 2716 | `	ph7_gen_state *pGen,   /* Code generator state */` |
|         - | 2717 | `	SyToken *pTok,         /* Offending token, or NULL for end of file */` |
|         - | 2718 | `	const char *zExpecting /* ", expecting <this>" tail, or NULL */` |
|         - | 2719 | `	)` |
|         5 | 2720 | `{` |
|       199 | 2721 | `	const char *zNoun = "token";` |
|         - | 2722 | `	sxu32 nLine;` |
|       199 | 2723 | `	if( pTok == 0 && pGen->pTokenSet ){` |
|         - | 2724 | `		/* The caller ran out of tokens inside its own slice — but a statement's slice stops` |
|         - | 2725 | `		 * BEFORE its terminator, so the token php actually names (typically the ';') is the` |
|         - | 2726 | `		 * one sitting just past the slice, still inside the chunk's token stream. Reach for` |
|         - | 2727 | `		 * it before concluding "end of file". */` |
|        96 | 2728 | `		SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        96 | 2729 | `		SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|        96 | 2730 | `		if( pGen->pEnd >= pBase && pGen->pEnd < pStreamEnd ){` |
|        94 | 2731 | `			pTok = pGen->pEnd;` |
|        45 | 2732 | `		}` |
|        46 | 2733 | `	}` |
|       199 | 2734 | `	nLine = pTok ? pTok->nLine : (pGen->pIn > (SyToken *)SySetBasePtr(pGen->pTokenSet) ? pGen->pIn[-1].nLine : 1);` |
|       199 | 2735 | `	if( pTok == 0 ){` |
|         4 | 2736 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|         1 | 2737 | `			zExpecting ? "syntax error, unexpected end of file, expecting %s"` |
|         - | 2738 | `			           : "syntax error, unexpected end of file",` |
|         1 | 2739 | `			zExpecting);` |
|         - | 2740 | `	}` |
|       197 | 2741 | `	if( pTok->nType & PH7_TK_ID ){` |
|        16 | 2742 | `		zNoun = "identifier";` |
|       191 | 2743 | `	}else if( pTok->nType & PH7_TK_DOLLAR ){` |
|         6 | 2744 | `		zNoun = "variable";` |
|         - | 2745 | `		/* The '$' is its own token and carries only "$" as text; the NAME is the` |
|         - | 2746 | ``		 * token after it. php names the whole variable, so `$x` was being reported`` |
|         - | 2747 | ``		 * as the nameless `variable "$"`. Stitch the two back together. */`` |
|         6 | 2748 | `		if( pGen->pTokenSet ){` |
|         6 | 2749 | `			SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|         6 | 2750 | `			SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|         6 | 2751 | `			SyToken *pName = &pTok[1];` |
|         4 | 2752 | `			if( pTok >= pBase && pName < pStreamEnd` |
|         4 | 2753 | `				&& (pName->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|         5 | 2754 | `				&& pName->sData.nByte > 0 ){` |
|         3 | 2755 | `				SyBlobReset(&pGen->sWorker);` |
|         3 | 2756 | `				SyBlobAppend(&pGen->sWorker,"$",sizeof(char));` |
|         3 | 2757 | `				SyBlobAppend(&pGen->sWorker,pName->sData.zString,pName->sData.nByte);` |
|         - | 2758 | `				{` |
|         - | 2759 | `					SyString sVar;` |
|         3 | 2760 | `					SyStringInitFromBuf(&sVar,SyBlobData(&pGen->sWorker),` |
|         - | 2761 | `						SyBlobLength(&pGen->sWorker));` |
|         3 | 2762 | `					if( zExpecting ){` |
|         4 | 2763 | `						return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|         - | 2764 | `							"syntax error, unexpected %s \"%z\", expecting %s",` |
|         1 | 2765 | `							zNoun,&sVar,zExpecting);` |
|         - | 2766 | `					}` |
|       ! 0 | 2767 | `					return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|       ! 0 | 2768 | `						"syntax error, unexpected %s \"%z\"",zNoun,&sVar);` |
|         - | 2769 | `				}` |
|         - | 2770 | `			}` |
|         2 | 2771 | `		}` |
|       182 | 2772 | `	}else if( pTok->nType & PH7_TK_INTEGER ){` |
|        23 | 2773 | `		zNoun = "integer";` |
|       171 | 2774 | `	}else if( pTok->nType & PH7_TK_REAL ){` |
|       ! 0 | 2775 | `		zNoun = "float";` |
|       ! 0 | 2776 | `	}` |
|       195 | 2777 | `	if( zExpecting ){` |
|       122 | 2778 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        39 | 2779 | `			"syntax error, unexpected %s \"%z\", expecting %s",zNoun,&pTok->sData,zExpecting);` |
|         - | 2780 | `	}` |
|       173 | 2781 | `	return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        56 | 2782 | `		"syntax error, unexpected %s \"%z\"",zNoun,&pTok->sData);` |
|       102 | 2783 | `}` |
|         - | 2784 | `/*` |
|         - | 2785 | ` * Generate a compile-time error message.` |
|         - | 2786 | ` * If the error count limit is reached (usually 15 error message)` |
|         - | 2787 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|         - | 2788 | ` * abort compilation immediately.` |
|         - | 2789 | ` */` |
|       690 | 2790 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|         5 | 2791 | `{` |
|       695 | 2792 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|       695 | 2793 | `	const char *zErr = "Error";` |
|         - | 2794 | `	SyString *pFile;` |
|         - | 2795 | `	va_list ap;` |
|         - | 2796 | `	sxi32 rc;` |
|         - | 2797 | `	/* Reset the working buffer */` |
|       695 | 2798 | `	SyBlobReset(pWorker);` |
|         - | 2799 | `	/* Peek the processed file path if available */` |
|       695 | 2800 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|       695 | 2801 | `	if( nErrType == E_ERROR \|\| nErrType == E_PARSE ){` |
|         - | 2802 | `		/* Increment the error counter. A PARSE error is every bit as fatal as an` |
|         - | 2803 | `		 * E_ERROR one: php compiles nothing, runs nothing and exits 255. Counting` |
|         - | 2804 | `		 * only E_ERROR let a parse error print its diagnostic and then fall through` |
|         - | 2805 | `		 * into execution with a 0 exit status. */` |
|       687 | 2806 | `		pGen->nErr++;` |
|       687 | 2807 | `		if( pGen->nErr > 15 ){` |
|         - | 2808 | `			/* Error count limit reached */` |
|         6 | 2809 | `			if( pGen->xErr ){` |
|         6 | 2810 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|         6 | 2811 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|         6 | 2812 | `				if( pFile ){` |
|         6 | 2813 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|         2 | 2814 | `				}` |
|         6 | 2815 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|         6 | 2816 | `				if( SyBlobLength(pWorker) > 0 ){` |
|         6 | 2817 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|         2 | 2818 | `				}` |
|         2 | 2819 | `			}` |
|         - | 2820 | `			/* Abort immediately */` |
|         6 | 2821 | `			return SXERR_ABORT;` |
|         - | 2822 | `		}` |
|       339 | 2823 | `	}` |
|       691 | 2824 | `	if( pGen->xErr == 0 ){` |
|         - | 2825 | `		/* No consumer — but keep the BARE message in the error buffer so a caller` |
|         - | 2826 | `		 * that needs the text can read it back. eval() compiles with logging off` |
|         - | 2827 | `		 * (a parse error there is php's catchable ParseError, not a printed` |
|         - | 2828 | `		 * diagnostic) and needs exactly this string for the exception message. */` |
|         5 | 2829 | `		va_start(ap,zFormat);` |
|         5 | 2830 | `		SyBlobFormatAp(pWorker,zFormat,ap);` |
|         5 | 2831 | `		va_end(ap);` |
|         5 | 2832 | `		return SXRET_OK;` |
|         - | 2833 | `	}` |
|       687 | 2834 | `	switch(nErrType){` |
|       324 | 2835 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|        11 | 2836 | `	case E_WARNING: zErr = "Warning";     break;` |
|       358 | 2837 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       ! 0 | 2838 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|       ! 0 | 2839 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|       ! 0 | 2840 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|       ! 0 | 2841 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|       ! 0 | 2842 | `	case 8192 /* E_DEPRECATED */: zErr = "Deprecated"; break;` |
|       ! 0 | 2843 | `	default:` |
|       ! 0 | 2844 | `		break;` |
|         - | 2845 | `	}` |
|       687 | 2846 | `	rc = SXRET_OK;` |
|         - | 2847 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|       687 | 2848 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|       687 | 2849 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|       687 | 2850 | `	va_start(ap,zFormat);` |
|       687 | 2851 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|       687 | 2852 | `	va_end(ap);` |
|       687 | 2853 | `	if( pFile ){` |
|       687 | 2854 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       341 | 2855 | `	}` |
|         - | 2856 | `	/* Append a new line */` |
|       687 | 2857 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       687 | 2858 | `	if( SyBlobLength(pWorker) > 0 ){` |
|         - | 2859 | `		/* Consume the generated error message */` |
|       687 | 2860 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       341 | 2861 | `	}` |
|       687 | 2862 | `	return rc;` |
|       350 | 2863 | `}` |
|         - | 2864 |  |
