# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1481/1609 lines (92.04%)

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
|    179124 |   46 | `PH7_PRIVATE GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|         5 |   47 | `{` |
|    179129 |   48 | `	GenBlock *pBlock = pCurrent;` |
|    361997 |   49 | `	for(;;){` |
|    723999 |   50 | `		if( pBlock->iFlags & iBlockType ){` |
|    179129 |   51 | `			iCount--; /* Decrement nesting level */` |
|    179129 |   52 | `			if( iCount < 1 ){` |
|         - |   53 | `				/* Block meet with the desired criteria */` |
|    179103 |   54 | `				return pBlock;` |
|         - |   55 | `			}` |
|        13 |   56 | `		}` |
|         - |   57 | `		/* Point to the upper block */` |
|    544901 |   58 | `		pBlock = pBlock->pParent;` |
|    544901 |   59 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|         - |   60 | `			/* Forbidden */` |
|        17 |   61 | `			break;` |
|         - |   62 | `		}` |
|         5 |   63 | `	}` |
|         - |   64 | `	/* No such block */` |
|        30 |   65 | `	return 0;` |
|     89567 |   66 | `}` |
|         - |   67 | `/*` |
|         - |   68 | ` * Initialize a freshly allocated block instance.` |
|         - |   69 | ` */` |
|  13130706 |   70 | `static void GenStateInitBlock(` |
|         - |   71 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |   72 | `	GenBlock *pBlock,    /* Target block */` |
|         - |   73 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   74 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|         - |   75 | `	void *pUserData      /* Upper layer private data */` |
|         - |   76 | `	)` |
|         5 |   77 | `{` |
|         - |   78 | `	/* Initialize block fields */` |
|  13130711 |   79 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  13130711 |   80 | `	pBlock->pUserData   = pUserData;` |
|  13130711 |   81 | `	pBlock->pGen        = pGen;` |
|  13130711 |   82 | `	pBlock->iFlags      = iType;` |
|  13130711 |   83 | `	pBlock->pParent     = 0;` |
|  13130711 |   84 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  13130711 |   85 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  13130711 |   86 | `}` |
|         - |   87 | `/*` |
|         - |   88 | ` * Allocate a new block instance.` |
|         - |   89 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|         - |   90 | ` * on success.Otherwise generate a compile-time error and abort` |
|         - |   91 | ` * processing on failure.` |
|         - |   92 | ` */` |
|  13126812 |   93 | `PH7_PRIVATE sxi32 GenStateEnterBlock(` |
|         - |   94 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |   95 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   96 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|         - |   97 | `	void *pUserData,      /* Upper layer private data */` |
|         - |   98 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|         - |   99 | `	)` |
|         5 |  100 | `{` |
|         - |  101 | `	GenBlock *pBlock;` |
|         - |  102 | `	/* Allocate a new block instance */` |
|  13126817 |  103 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  13126817 |  104 | `	if( pBlock == 0 ){` |
|         - |  105 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |  106 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - |  107 | `		 */` |
|       ! 0 |  108 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|         - |  109 | `		/* Abort processing immediately */` |
|       ! 0 |  110 | `		return SXERR_ABORT;` |
|         - |  111 | `	}` |
|         - |  112 | `	/* Zero the structure */` |
|  13126817 |  113 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  13126817 |  114 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|         - |  115 | `	/* Link to the parent block */` |
|  13126817 |  116 | `	pBlock->pParent = pGen->pCurrent;` |
|         - |  117 | `	/* A loop or switch gets an id, and remembers the loop it nests inside, so a goto's` |
|         - |  118 | `	 * and a label's positions can be compared after compilation (see aLoopParent). */` |
|  13126817 |  119 | `	if( iType & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    541865 |  120 | `		sxu32 nParent = pGen->nCurLoopId;` |
|    541865 |  121 | `		pGen->nLoopId++;` |
|    541865 |  122 | `		SySetPut(&pGen->aLoopParent,(const void *)&nParent);` |
|    541865 |  123 | `		pBlock->nLoopId = pGen->nLoopId;` |
|    541865 |  124 | `		pBlock->nOuterLoopId = nParent;` |
|    541865 |  125 | `		pGen->nCurLoopId = pGen->nLoopId;` |
|    270930 |  126 | `	}` |
|         - |  127 | `	/* Mark as the current block */` |
|  13126817 |  128 | `	pGen->pCurrent = pBlock;` |
|  13126817 |  129 | `	if( ppBlock ){` |
|         - |  130 | `		/* Write a pointer to the new instance */` |
|   6313093 |  131 | `		*ppBlock = pBlock;` |
|   3156544 |  132 | `	}` |
|  13126817 |  133 | `	return SXRET_OK;` |
|   6563411 |  134 | `}` |
|         - |  135 | `/*` |
|         - |  136 | ` * Release block fields without freeing the whole instance.` |
|         - |  137 | ` */` |
|  13126804 |  138 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|         5 |  139 | `{` |
|  13126809 |  140 | `	SySetRelease(&pBlock->aPostContFix);` |
|  13126809 |  141 | `	SySetRelease(&pBlock->aJumpFix);` |
|  13126809 |  142 | `}` |
|         - |  143 | `/*` |
|         - |  144 | ` * Release a block.` |
|         - |  145 | ` */` |
|  13126800 |  146 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|         5 |  147 | `{` |
|  13126805 |  148 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  13126805 |  149 | `	GenStateReleaseBlock(&(*pBlock));` |
|         - |  150 | `	/* Free the instance */` |
|  13126805 |  151 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  13126805 |  152 | `}` |
|         - |  153 | `/*` |
|         - |  154 | ` * POP and release a block from the stack of compiled blocks.` |
|         - |  155 | ` */` |
|  13126800 |  156 | `PH7_PRIVATE sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|         5 |  157 | `{` |
|  13126805 |  158 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  13126805 |  159 | `	if( pBlock == 0 ){` |
|         - |  160 | `		/* No more block to pop */` |
|       ! 0 |  161 | `		return SXERR_EMPTY;` |
|         - |  162 | `	}` |
|  13126805 |  163 | `	if( pBlock->iFlags & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    541857 |  164 | `		pGen->nCurLoopId = pBlock->nOuterLoopId;` |
|    270926 |  165 | `	}` |
|         - |  166 | `	/* Point to the upper block */` |
|  13126805 |  167 | `	pGen->pCurrent = pBlock->pParent;` |
|  13126805 |  168 | `	if( ppBlock ){` |
|         - |  169 | `		/* Write a pointer to the popped block */` |
|       ! 0 |  170 | `		*ppBlock = pBlock;` |
|       ! 0 |  171 | `	}else{` |
|         - |  172 | `		/* Safely release the block */` |
|  13126805 |  173 | `		GenStateFreeBlock(&(*pBlock));` |
|         - |  174 | `	}` |
|  13126805 |  175 | `	return SXRET_OK;` |
|   6563405 |  176 | `}` |
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
|   1046078 |  195 | `PH7_PRIVATE int GenStateUnconditionalTopLevel(ph7_gen_state *pGen)` |
|         5 |  196 | `{` |
|   1046083 |  197 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   1050077 |  198 | `	while( pBlock ){` |
|   1050077 |  199 | `		if( pBlock->iFlags & (GEN_BLOCK_COND\|GEN_BLOCK_LOOP\|GEN_BLOCK_FUNC\|GEN_BLOCK_SWITCH\|GEN_BLOCK_EXCEPTION) ){` |
|        77 |  200 | `			return 0; /* conditional / nested */` |
|         - |  201 | `		}` |
|   1050005 |  202 | `		if( pBlock->iFlags & GEN_BLOCK_GLOBAL ){` |
|   1046011 |  203 | `			return 1; /* reached the global block with no conditional ancestor */` |
|         - |  204 | `		}` |
|      3999 |  205 | `		pBlock = pBlock->pParent;` |
|         5 |  206 | `	}` |
|       ! 0 |  207 | `	return 1;` |
|    523044 |  208 | `}` |
|         - |  209 | `/*` |
|         - |  210 | ` * Guard a top-level function about to be installed. Same contract as the class` |
|         - |  211 | ` * guard above.` |
|         - |  212 | ` */` |
|    522666 |  213 | `PH7_PRIVATE sxi32 GenStateGuardFuncRedeclaration(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|         5 |  214 | `{` |
|         - |  215 | `	SyHashEntry *pEntry;` |
|    522671 |  216 | `	if( !GenStateUnconditionalTopLevel(pGen) ){` |
|        52 |  217 | `		return SXRET_OK;` |
|         - |  218 | `	}` |
|    522621 |  219 | `	pFunc->iFlags \|= VM_FUNC_BOUND;` |
|    522621 |  220 | `	if( pGen->pVm->bCompilingBuiltin ){` |
|    521265 |  221 | `		return SXRET_OK;` |
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
|    261338 |  247 | `}` |
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
|   4740730 |  258 | `PH7_PRIVATE sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|         5 |  259 | `{` |
|         - |  260 | `	JumpFixup sJumpFix;` |
|         - |  261 | `	sxi32 rc;` |
|         - |  262 | `	/* Init the JumpFixup structure */` |
|   4740735 |  263 | `	sJumpFix.nJumpType = nJumpType;` |
|   4740735 |  264 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|         - |  265 | `	/* Insert in the jump fixup table */` |
|   4740735 |  266 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|   4740735 |  267 | `	return rc;` |
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
|   9125760 |  280 | `PH7_PRIVATE sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|         5 |  281 | `{` |
|         - |  282 | `	JumpFixup *aFix;` |
|         - |  283 | `	VmInstr *pInstr;` |
|         - |  284 | `	sxu32 nFixed;` |
|         - |  285 | `	sxu32 n;` |
|         - |  286 | `	/* Point to the jump fixup table */` |
|   9125765 |  287 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|         - |  288 | `	/* Fix the desired jumps */` |
|  19101677 |  289 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|   9975917 |  290 | `		if( aFix[n].nJumpType < 0 ){` |
|         - |  291 | `			/* Already fixed */` |
|   3725043 |  292 | `			continue;` |
|         - |  293 | `		}` |
|   6250879 |  294 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|         - |  295 | `			/* Not of our interest */` |
|   1510151 |  296 | `			continue;` |
|         - |  297 | `		}` |
|         - |  298 | `		/* Point to the instruction to fix */` |
|   4740733 |  299 | `		pInstr = PH7_VmGetInstr(pBlock->pGen->pVm,aFix[n].nInstrIdx);` |
|   4740733 |  300 | `		if( pInstr ){` |
|   4740733 |  301 | `			pInstr->iP2 = nJumpDest;` |
|   4740733 |  302 | `			nFixed++;` |
|         - |  303 | `			/* Mark as fixed */` |
|   4740733 |  304 | `			aFix[n].nJumpType = -1;` |
|   2370364 |  305 | `		}` |
|   2370369 |  306 | `	}` |
|         - |  307 | `	/* Total number of fixed jumps */` |
|   9125765 |  308 | `	return nFixed;` |
|         5 |  309 | `}` |
|         - |  310 | `/*` |
|         - |  311 | ` * Fix a 'goto' now the jump destination is resolved.` |
|         - |  312 | ` * The goto statement can be used to jump to another section` |
|         - |  313 | ` * in the program.` |
|         - |  314 | ` * Refer to the routine responsible of compiling the goto` |
|         - |  315 | ` * statement for more information.` |
|         - |  316 | ` */` |
|   3378660 |  317 | `PH7_PRIVATE sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|         5 |  318 | `{` |
|         - |  319 | `	JumpFixup *pJump,*aJumps;` |
|         - |  320 | `	Label *pLabel;` |
|         - |  321 | `	VmInstr *pInstr;` |
|         - |  322 | `	sxi32 rc;` |
|         - |  323 | `	sxu32 n;` |
|         - |  324 | `	/* Point to the goto table */` |
|   3378665 |  325 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|         - |  326 | `	/* Fix */` |
|   3378811 |  327 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
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
|   3378663 |  378 | `	return SXRET_OK;` |
|   1689335 |  379 | `}` |
|         - |  380 | `/*` |
|         - |  381 | ` * Check if a given token value is installed in the literal table.` |
|         - |  382 | ` */` |
|  16924374 |  383 | `PH7_PRIVATE sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|         5 |  384 | `{` |
|         - |  385 | `	SyHashEntry *pEntry;` |
|  16924379 |  386 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  16924379 |  387 | `	if( pEntry == 0 ){` |
|   4511949 |  388 | `		return SXERR_NOTFOUND;` |
|         - |  389 | `	}` |
|  12412435 |  390 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  12412435 |  391 | `	return SXRET_OK;` |
|   8462192 |  392 | `}` |
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
|   4511944 |  403 | `PH7_PRIVATE sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|         5 |  404 | `{` |
|   4511949 |  405 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|   4511949 |  406 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   2255972 |  407 | `	}` |
|   4511949 |  408 | `	return SXRET_OK;` |
|         5 |  409 | `}` |
|         - |  410 | `/*` |
|         - |  411 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|         - |  412 | ` * in the constant table.` |
|         - |  413 | ` */` |
|   3826250 |  414 | `PH7_PRIVATE ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|         5 |  415 | `{` |
|         - |  416 | `	ph7_value *pObj;` |
|   3826255 |  417 | `	sxu32 nIdx = 0; /* cc warning */` |
|         - |  418 | `	/* Reserve a new constant */` |
|   3826255 |  419 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   3826255 |  420 | `	if( pObj == 0 ){` |
|       ! 0 |  421 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  422 | `		return 0;` |
|         - |  423 | `	}` |
|   3826255 |  424 | `	*pIdx = nIdx;` |
|         - |  425 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|         - |  426 | `	 * the constant string iterals table [optimization purposes].` |
|         - |  427 | `	 */` |
|   3826255 |  428 | `	return pObj;` |
|   1913130 |  429 | `}` |
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
|   7962172 |  444 | `PH7_PRIVATE void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|         5 |  445 | `{` |
|         - |  446 | `	VmCallArgMap *pMap;` |
|   7962177 |  447 | `	if( !pGen->bStrictTypes ) return p3;` |
|        58 |  448 | `	if( p3 == 0 ){` |
|        54 |  449 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|        54 |  450 | `		if( pMap == 0 ) return 0;` |
|        54 |  451 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|        54 |  452 | `		p3 = (void *)pMap;` |
|        25 |  453 | `	}` |
|        58 |  454 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|        58 |  455 | `	return p3;` |
|   3981091 |  456 | `}` |
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
|    342560 |  476 | `PH7_PRIVATE int GenStateIsReservedConstant(SyString *pName)` |
|         5 |  477 | `{` |
|    342565 |  478 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|      3939 |  479 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|         3 |  480 | `			return TRUE;` |
|      3937 |  481 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|         6 |  482 | `			return TRUE;` |
|         5 |  483 | `		}` |
|    340595 |  484 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|      7805 |  485 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|         3 |  486 | `			return TRUE;` |
|         - |  487 | `		}` |
|      3899 |  488 | `	}` |
|         - |  489 | `	/* Not a reserved constant */` |
|    342557 |  490 | `	return FALSE;` |
|    171285 |  491 | `}` |
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
|  54091242 |  508 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|         5 |  509 | `{` |
|  54091247 |  510 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|         - |  511 | `	sxu32 nTarget;` |
|         - |  512 | `	sxu32 *aIdx;` |
|         - |  513 | `	sxu32 i;` |
|  54091247 |  514 | `	if( nCur <= nBaseline ){` |
|  54091151 |  515 | `		return;` |
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
|  27045626 |  526 | `}` |
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
|   7071584 |  545 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
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
|   7071589 |  564 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|   1810433 |  565 | `		return 0;` |
|         - |  566 | `	}` |
|  57389149 |  567 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
|  52186504 |  568 | `		if( pName->nByte == aByRef[i].nByte` |
|  27612047 |  569 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|     58521 |  570 | `			return aByRef[i].mask;` |
|         - |  571 | `		}` |
|  26063999 |  572 | `	}` |
|   5202645 |  573 | `	return 0;` |
|   3535797 |  574 | `}` |
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
|   7071584 |  585 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|         5 |  586 | `{` |
|         - |  587 | `	SyToken *p, *pEnd;` |
|   7071589 |  588 | `	pOut->zString = 0;` |
|   7071589 |  589 | `	pOut->nByte = 0;` |
|   7071589 |  590 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|       ! 0 |  591 | `		return;` |
|         - |  592 | `	}` |
|   7071589 |  593 | `	p = pLeft->pStart;` |
|   7071589 |  594 | `	pEnd = pLeft->pEnd;` |
|         - |  595 | `	/* Optional single leading namespace separator (absolute path). */` |
|   7071589 |  596 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|      3933 |  597 | `		p++;` |
|      1964 |  598 | `	}` |
|   7071589 |  599 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|   1810387 |  600 | `		return;` |
|         - |  601 | `	}` |
|         - |  602 | `	/* Must be a single component: nothing follows the name token. */` |
|   5261207 |  603 | `	if( p + 1 != pEnd ){` |
|        51 |  604 | `		return;` |
|         - |  605 | `	}` |
|   5261161 |  606 | `	*pOut = p->sData;` |
|   3535797 |  607 | `}` |
|         - |  608 | `/*` |
|         - |  609 | ` * Generate bytecode for a given expression tree.` |
|         - |  610 | ` * If something goes wrong while generating bytecode` |
|         - |  611 | ` * for the expression tree (A very unlikely scenario)` |
|         - |  612 | ` * this function takes care of generating the appropriate` |
|         - |  613 | ` * error message.` |
|         - |  614 | ` */` |
|  76268274 |  615 | `static sxi32 GenStateEmitExprCode(` |
|         - |  616 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |  617 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|         - |  618 | `	sxi32 iFlags /* Control flags */` |
|         - |  619 | `	)` |
|         5 |  620 | `{` |
|         - |  621 | `	VmInstr *pInstr;` |
|         - |  622 | `	sxu32 nJmpIdx;` |
|  76268279 |  623 | `	sxi32 iP1 = 0;` |
|  76268279 |  624 | `	sxu32 iP2 = 0;` |
|  76268279 |  625 | `	void *p3  = 0;` |
|         - |  626 | `	sxi32 iVmOp;` |
|         - |  627 | `	sxi32 rc;` |
|  76268279 |  628 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
|  76268279 |  629 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
|  76268279 |  630 | `	sxu32 nRhsNsBase = 0;` |
|  76268279 |  631 | `	if( pNode->xCode ){` |
|         - |  632 | `		SyToken *pTmpIn,*pTmpEnd;` |
|         - |  633 | `		/* Compile node */` |
|  46031919 |  634 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
|  46031919 |  635 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
|  46031919 |  636 | `		RE_SWAP_DELIMITER(pGen);` |
|  46031919 |  637 | `		return rc;` |
|         - |  638 | `	}` |
|  30236365 |  639 | `	if( pNode->pOp == 0 ){` |
|       ! 0 |  640 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - |  641 | `			"Invalid expression node,PH7 is aborting compilation");` |
|       ! 0 |  642 | `		return SXERR_ABORT;` |
|         - |  643 | `	}` |
|  30236365 |  644 | `	iVmOp = pNode->pOp->iVmOp;` |
|  30236365 |  645 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|         - |  646 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|         - |  647 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|         - |  648 | `		 * and later errors are still reported. */` |
|         3 |  649 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - |  650 | `			"The (unset) cast is no longer supported");` |
|         3 |  651 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  652 | `			return SXERR_ABORT;` |
|         - |  653 | `		}` |
|         1 |  654 | `	}` |
|  30236365 |  655 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
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
|  30236275 |  705 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|         - |  706 | `		sxu32 nJz,nJmp;` |
|         - |  707 | `		sxu32 nTernaryNsBase;` |
|         - |  708 | `		/* Ternary operator require special handling */` |
|         - |  709 | `		/* Phase#1: Compile the condition */` |
|    505115 |  710 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    505115 |  711 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    505115 |  712 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  713 | `			return rc;` |
|         - |  714 | `		}` |
|         - |  715 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|         - |  716 | `		 * compiling the condition must short-circuit to the end of the` |
|         - |  717 | `		 * condition expression, not leak past the ternary. */` |
|    505115 |  718 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    505115 |  719 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    505115 |  720 | `		if( pNode->pLeft ){` |
|         - |  721 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|         - |  722 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    501155 |  723 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - |  724 | `			/* Phase#3: Compile the 'then' expression  */` |
|    501155 |  725 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    501155 |  726 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    501155 |  727 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  728 | `				return rc;` |
|         - |  729 | `			}` |
|    501155 |  730 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    250580 |  731 | `		}else{` |
|         - |  732 | `			/* Elvis operator: (expr) ?: (else)` |
|         - |  733 | `			 * Duplicate condition so original value is the 'then' result.` |
|         - |  734 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      3965 |  735 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      3965 |  736 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - |  737 | `		}` |
|         - |  738 | `		/* Phase#4: Emit the unconditional jump */` |
|    505115 |  739 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|         - |  740 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    505115 |  741 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    505115 |  742 | `		if( pInstr ){` |
|    505115 |  743 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    252555 |  744 | `		}` |
|    505115 |  745 | `		if( !pNode->pLeft ){` |
|         - |  746 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      3965 |  747 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      1980 |  748 | `		}` |
|         - |  749 | `		/* Phase#6: Compile the 'else' expression */` |
|    505115 |  750 | `		if( pNode->pRight ){` |
|    505115 |  751 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    505115 |  752 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    505115 |  753 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  754 | `				return rc;` |
|         - |  755 | `			}` |
|    505115 |  756 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    252555 |  757 | `		}` |
|    505115 |  758 | `		if( nJmp > 0 ){` |
|         - |  759 | `			/* Phase#7: Fix the unconditional jump */` |
|    505115 |  760 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    505115 |  761 | `			if( pInstr ){` |
|    505115 |  762 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    252555 |  763 | `			}` |
|    252555 |  764 | `		}` |
|         - |  765 | `		/* All done */` |
|    505115 |  766 | `		return SXRET_OK;` |
|         - |  767 | `	}` |
|  29731165 |  768 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
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
|  29731139 |  801 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|         - |  802 | `	/* Generate code for the left tree */` |
|  29731139 |  803 | `	if( pNode->pLeft ){` |
|  29676867 |  804 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  29676867 |  805 | `		if( iVmOp == PH7_OP_CALL ){` |
|         - |  806 | `			ph7_expr_node **apNode;` |
|   7075819 |  807 | `			int hasSpread = 0;` |
|   7075819 |  808 | `			int hasNamed = 0;` |
|   7075819 |  809 | `			int bAnySpread = 0;` |
|   7075819 |  810 | `			sxu32 byRefMask = 0;` |
|         - |  811 | `			sxi32 nArgs;` |
|         - |  812 | `			sxi32 n;` |
|         - |  813 | `			/* Recurse and generate bytecodes for function arguments */` |
|   7075819 |  814 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   7075819 |  815 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|         - |  816 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|         - |  817 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|         - |  818 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|   7075819 |  819 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|        81 |  820 | `				bFcc = 1;` |
|        81 |  821 | `				nArgs = 0;` |
|        40 |  822 | `			}` |
|         - |  823 | `			/* Validate argument order like php: no positional argument after a` |
|         - |  824 | ``			 * named one OR after unpacking, and `name: ...$x` is a parse error. */`` |
|         - |  825 | `			{` |
|   7075819 |  826 | `				int seenNamed = 0;` |
|   7075819 |  827 | `				int seenSpread = 0;` |
|  15110745 |  828 | `				for( n = 0; n < nArgs; ++n ){` |
|   8034933 |  829 | `					if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|      4091 |  830 | `						bAnySpread = 1;` |
|      4091 |  831 | `						seenSpread = 1;` |
|      4091 |  832 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       ! 0 |  833 | `							rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - |  834 | `								"syntax error, unexpected token \"...\"");` |
|       ! 0 |  835 | `							return SXERR_SYNTAX;` |
|         5 |  836 | `						}` |
|   8032890 |  837 | `					}else if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       300 |  838 | `						seenNamed = 1;` |
|       300 |  839 | `						hasNamed = 1;` |
|   8030699 |  840 | `					}else if( seenNamed ){` |
|         3 |  841 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - |  842 | `							"Cannot use positional argument after named argument");` |
|         3 |  843 | `						return SXERR_SYNTAX;` |
|   8030549 |  844 | `					}else if( seenSpread ){` |
|       ! 0 |  845 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - |  846 | `							"Cannot use positional argument after argument unpacking");` |
|       ! 0 |  847 | `						return SXERR_SYNTAX;` |
|         - |  848 | `					}` |
|   4017468 |  849 | `				}` |
|         - |  850 | `			}` |
|         - |  851 | `			/* Read-only load */` |
|   7075817 |  852 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|         - |  853 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|         - |  854 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|         - |  855 | `			 * objects dispatch to the right method (offsetExists for both;` |
|         - |  856 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|   7075817 |  857 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|   7075817 |  858 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|   7075812 |  859 | `				if( pCallName->nByte == 5` |
|   3932244 |  860 | `				 && SyStrnicmp(pCallName->zString,"isset",5) == 0 ){` |
|    327165 |  861 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|   6912237 |  862 | `				}else if( pCallName->nByte == 5` |
|   3605084 |  863 | `				 && SyStrnicmp(pCallName->zString,"empty",5) == 0 ){` |
|       117 |  864 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|        56 |  865 | `				}` |
|         - |  866 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|         - |  867 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|         - |  868 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|         - |  869 | `				 * write back through. Skipped when spread/named args are present:` |
|         - |  870 | `				 * the compile-time positional index no longer maps to the` |
|         - |  871 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|   7075817 |  872 | `				if( !bAnySpread && !hasNamed ){` |
|         - |  873 | `					SyString sBuiltin;` |
|   7071589 |  874 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|   7071589 |  875 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|   3535792 |  876 | `				}` |
|   3537906 |  877 | `			}` |
|  15110741 |  878 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|   8034929 |  879 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   8034929 |  880 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|         - |  881 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|         - |  882 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|         - |  883 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|         - |  884 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|         - |  885 | `				 * builtin to write back through. A plain $var target is unaffected` |
|         - |  886 | `				 * (iP1=0 either way). */` |
|   8034929 |  887 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|     38991 |  888 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|     38991 |  889 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|     19493 |  890 | `				}` |
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
|   8034924 |  901 | `				if( (iFlags & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_LOAD_IDX_UNSET)) == 0` |
|   7871276 |  902 | `				 && apNode[n]->pOp == 0 && apNode[n]->xCode == PH7_CompileVariable` |
|   4125212 |  903 | `				 && (apNode[n]->iFlags & (EXPR_NODE_NAMED_ARG\|EXPR_NODE_SPREAD)) == 0 ){` |
|   3371347 |  904 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|   1685671 |  905 | `				}` |
|   8034929 |  906 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|   8034929 |  907 | `				if( rc != SXRET_OK ){` |
|       ! 0 |  908 | `					return rc;` |
|         - |  909 | `				}` |
|         - |  910 | `				/* Each argument is an independent nullsafe scope. */` |
|   8034929 |  911 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|   8034929 |  912 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|         - |  913 | `					/* Emit spread opcode to unpack this array argument */` |
|      4091 |  914 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      4091 |  915 | `					hasSpread = 1;` |
|      2043 |  916 | `				}` |
|   4017467 |  917 | `			}` |
|         - |  918 | `			/* Total number of given arguments */` |
|   7075817 |  919 | `			iP1 = nArgs;` |
|   7075817 |  920 | `			iP2 = hasSpread;` |
|         - |  921 | `			/* Build VmCallArgMap if named arguments are present.` |
|         - |  922 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|   7075817 |  923 | `			if( hasNamed ){` |
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
|   7075817 |  955 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|   3537906 |  956 | `		}` |
|         - |  957 | `		{` |
|         - |  958 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|         - |  959 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|         - |  960 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|         - |  961 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|         - |  962 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|         - |  963 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|         - |  964 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|         - |  965 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
|  29676865 |  966 | `			sxi32 iLeftFlags = iFlags;` |
|  29676860 |  967 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
|  24096324 |  968 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|   9257920 |  969 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|   7969787 |  970 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   2786937 |  971 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|   1393466 |  972 | `			}` |
|         - |  973 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|         - |  974 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|         - |  975 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|         - |  976 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|         - |  977 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|         - |  978 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|         - |  979 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
|  29676860 |  980 | `			if( pNode->pOp` |
|  41722565 |  981 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
|  26884182 |  982 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  24091452 |  983 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|   6006637 |  984 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|   3003316 |  985 | `			}` |
|         - |  986 | ``			/* `++`/`--` mutate their operand in place — the operand is a write`` |
|         - |  987 | ``			 * lvalue exactly like a compound assign's (`$o->m[0]++` must tag the`` |
|         - |  988 | ``			 * member base PH7_MEMBER_WRITE the way `$o->m[0] += 1` does: hooked`` |
|         - |  989 | `			 * properties throw php's Indirect-modification Error, missing ones` |
|         - |  990 | `			 * auto-vivify). The prec-18 site below handles the assign family;` |
|         - |  991 | ``			 * `++`/`--` are unary, their operand is pLeft. */`` |
|  29676860 |  992 | `			if( pNode->pOp` |
|  29676865 |  993 | `				&& (pNode->pOp->iVmOp == PH7_OP_INCR \|\| pNode->pOp->iVmOp == PH7_OP_DECR) ){` |
|    218285 |  994 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE` |
|         - |  995 | ``					\| EXPR_FLAG_RMW_LOAD /* php warns before seeding `$undef++` */;`` |
|    109140 |  996 | `			}` |
|         - |  997 | ``			/* `??` reads its LEFT operand in isset-context: an undefined or`` |
|         - |  998 | `			 * UNINITIALIZED typed PROPERTY must yield the default rather than a` |
|         - |  999 | `			 * warning/Error (php). Tag a member-access LHS so its OP_MEMBER takes` |
|         - | 1000 | `			 * the silent-lookup path (iP2 = ISSET), which still loads a present` |
|         - | 1001 | `			 * value. A SUBSCRIPT LHS is left untagged: LOAD_IDX's ISSET mode means` |
|         - | 1002 | ``			 * offsetExists (a bool), but `$o[$k] ?? d` needs the offsetGet value —`` |
|         - | 1003 | `			 * that path is already handled correctly by OP_NULLC. */` |
|  29676865 | 1004 | `			if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC && pNode->pLeft ){` |
|         - | 1005 | ``				/* php reads the ENTIRE left operand of `??` in isset-context: no`` |
|         - | 1006 | ``				 * "Undefined variable" for `$x ?? d` OR for the base of`` |
|         - | 1007 | ``				 * `$x['k'] ?? d`. QUIET_VAR silences the variable read wherever it`` |
|         - | 1008 | `				 * sits in the chain. */` |
|     58521 | 1009 | `				iLeftFlags \|= EXPR_FLAG_QUIET_VAR;` |
|     58516 | 1010 | `				if( pNode->pLeft->pOp` |
|     87710 | 1011 | `					&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|     58458 | 1012 | `						\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|     58441 | 1013 | `						\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|         - | 1014 | `					/* A member-access LHS additionally takes OP_MEMBER's silent` |
|         - | 1015 | `					 * lookup (iP2 = ISSET) so an uninitialized typed property` |
|         - | 1016 | `					 * yields the default instead of an Error. A SUBSCRIPT LHS must` |
|         - | 1017 | `					 * NOT: LOAD_IDX's ISSET mode means offsetExists (a bool), while` |
|         - | 1018 | ``					 * `$o[$k] ?? d` needs the offsetGet value — OP_NULLC already`` |
|         - | 1019 | `					 * handles that path. */` |
|        37 | 1020 | `					iLeftFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|        18 | 1021 | `				}` |
|     29258 | 1022 | `			}` |
|  29676865 | 1023 | `			if( iVmOp == PH7_OP_ERR_CTRL ){` |
|         - | 1024 | `				/* '@' must suppress the diagnostics raised WHILE its operand runs, so` |
|         - | 1025 | `				 * open the window here; the trailing emit below closes it (iP1 = 0). */` |
|     15801 | 1026 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_ERR_CTRL,1,0,0,0);` |
|      7898 | 1027 | `			}` |
|  29676865 | 1028 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags\|EXPR_FLAG_RDONLY_LOAD);` |
|         - | 1029 | `		}` |
|  29676865 | 1030 | `		if( rc != SXRET_OK ){` |
|        34 | 1031 | `			return rc;` |
|         - | 1032 | `		}` |
|  29676835 | 1033 | `		if( !bIsChainOp ){` |
|         - | 1034 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|         - | 1035 | `			 * target the end of that LHS chain, which is right here. */` |
|  13560663 | 1036 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|   6780329 | 1037 | `		}` |
|  29676835 | 1038 | `		if( iVmOp == PH7_OP_CALL ){` |
|   7075817 | 1039 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   7075817 | 1040 | `			if( pInstr ){` |
|   7075817 | 1041 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|   5261469 | 1042 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|         - | 1043 | `					sxu32 nQual;` |
|   5261469 | 1044 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 1045 | `					/* Prevent constant expansion but preserve the absolute flag` |
|         - | 1046 | `					 * so the later NEW handler (if any) can see it. */` |
|   5261469 | 1047 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|         - | 1048 | `					/* Namespace-qualify the function name for CALL, unless the` |
|         - | 1049 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|         - | 1050 | `					 * imports — class imports must NOT affect function` |
|         - | 1051 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|         - | 1052 | `					 * before NEW; we store the original literal index in the` |
|         - | 1053 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|         - | 1054 | `					 * the unqualified name and re-qualify with class imports. */` |
|   5261469 | 1055 | `					if( bAbsolute ){` |
|      3933 | 1056 | `						pInstr->iP2 = (sxi32)nOrig;` |
|      1969 | 1057 | `					}else{` |
|   5257541 | 1058 | `						int fromImport = 0;` |
|   5257541 | 1059 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|   5257541 | 1060 | `						pInstr->iP2 = (sxi32)nQual;` |
|   5257541 | 1061 | `						if( nQual != nOrig ){` |
|         - | 1062 | `							/* Record the original literal index in the arg map` |
|         - | 1063 | `							 * (NOT in the CALL's iP2 — that is the hasSpread` |
|         - | 1064 | `							 * flag) so the NEW handler can recover the` |
|         - | 1065 | `							 * unqualified name and re-qualify with CLASS` |
|         - | 1066 | `							 * imports. */` |
|       103 | 1067 | `							if( p3 == 0 ){` |
|       103 | 1068 | `								VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|        98 | 1069 | `									&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|       103 | 1070 | `								if( pMap ){` |
|       103 | 1071 | `									SyZero(pMap, sizeof(VmCallArgMap));` |
|       103 | 1072 | `									p3 = (void *)pMap;` |
|        49 | 1073 | `								}` |
|        49 | 1074 | `							}` |
|       103 | 1075 | `							if( p3 ){` |
|       103 | 1076 | `								((VmCallArgMap *)p3)->nOrigNameLit = nOrig + 1;` |
|       103 | 1077 | `								if( !fromImport ){` |
|         - | 1078 | `									/* Mark as namespace-qualified */` |
|        93 | 1079 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|        44 | 1080 | `								}` |
|        49 | 1081 | `							}` |
|        49 | 1082 | `						}` |
|         - | 1083 | `					}` |
|   4445085 | 1084 | `				}else if( (pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */` |
|   1804269 | 1085 | `						&& !(pNode->pLeft && (pNode->pLeft->iFlags & EXPR_NODE_PARENS)))` |
|    917260 | 1086 | `					\|\| pInstr->iOp == PH7_OP_NEW ){` |
|         - | 1087 | `					/* Method call,flag that. But NOT when the callee was an explicitly` |
|         - | 1088 | ``					 * PARENTHESISED member access: `($o->p)(...)` / `($o::$p)(...)` invokes`` |
|         - | 1089 | `					 * the VALUE of the property (php's variable-invocation), so the OP_MEMBER` |
|         - | 1090 | `					 * must stay a plain property READ (leaving the callable on the stack for` |
|         - | 1091 | `					 * OP_CALL to invoke) rather than being rewritten into a method-name` |
|         - | 1092 | ``					 * resolution — the parens are exactly what distinguishes `($o->p)()` from`` |
|         - | 1093 | ``					 * the method call `$o->p()`. */`` |
|   1794197 | 1094 | `					pInstr->iP2 = 1;` |
|         - | 1095 | ``					/* A static call with a DYNAMIC method name (`C::$m(...)`): the`` |
|         - | 1096 | ``					 * static-`::` codegen folded the variable NAME into OP_MEMBER->p3`` |
|         - | 1097 | ``					 * as if it were a static-PROPERTY read (`C::$m`), but in a CALL the`` |
|         - | 1098 | `					 * method name is the variable's VALUE. Rebuild the sequence` |
|         - | 1099 | `					 * [class, OP_LOAD $m -> value, OP_MEMBER(static, method)] so the` |
|         - | 1100 | `					 * dynamic name is read off the stack, matching the instance` |
|         - | 1101 | ``					 * (`$o->$m()`) path. iP1==1 marks a static member. */`` |
|   1794197 | 1102 | `					if( pInstr->iOp == PH7_OP_MEMBER && pInstr->iP1 == 1 && pInstr->p3 ){` |
|        11 | 1103 | `						void *pDynName = pInstr->p3;` |
|        11 | 1104 | `						(void)PH7_VmPopInstr(pGen->pVm);` |
|        11 | 1105 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,pDynName,0);` |
|        11 | 1106 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_MEMBER,1,PH7_MEMBER_METHOD,0,0);` |
|         5 | 1107 | `					}` |
|    897096 | 1108 | `				}` |
|   3537911 | 1109 | `			}` |
|  26138929 | 1110 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|         - | 1111 | `			ph7_expr_node **apNode;` |
|         - | 1112 | `			sxi32 n;` |
|   3033733 | 1113 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|         - | 1114 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|         - | 1115 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE` |
|         - | 1116 | `				\|EXPR_FLAG_QUIET_VAR\|EXPR_FLAG_RMW_LOAD);` |
|         - | 1117 | `			/* Recurse and generate bytecodes for array index */` |
|   3033733 | 1118 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   5829735 | 1119 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   2796007 | 1120 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   2796007 | 1121 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   2796007 | 1122 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 1123 | `					return rc;` |
|         - | 1124 | `				}` |
|         - | 1125 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   2796007 | 1126 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   1398006 | 1127 | `			}` |
|   3033733 | 1128 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   2796007 | 1129 | `				iP1 = 1; /* Node have an index associated with it */` |
|   1398001 | 1130 | `			}` |
|   3033733 | 1131 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|         - | 1132 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|    373721 | 1133 | `				iP2 = 4;` |
|   2846875 | 1134 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|         - | 1135 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|         - | 1136 | `				 * so the trailing unset() builtin can drop the slot. */` |
|     23421 | 1137 | `				iP2 = 5;` |
|   2648309 | 1138 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|         - | 1139 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|         - | 1140 | `				 * short-circuit on missing keys without invoking offsetGet` |
|         - | 1141 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|        29 | 1142 | `				iP2 = 6;` |
|   2636589 | 1143 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|         - | 1144 | `				/* Create an empty entry when the desired index is not found */` |
|    557187 | 1145 | `				iP2 = 1;` |
|    278596 | 1146 | `			}` |
|  21084159 | 1147 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         - | 1148 | `			/* POP the left node */` |
|         5 | 1149 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         2 | 1150 | `		}` |
|  14838415 | 1151 | `	}` |
|  29731107 | 1152 | `	rc = SXRET_OK;` |
|  29731107 | 1153 | `	nJmpIdx = 0;` |
|         - | 1154 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|         - | 1155 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|         - | 1156 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  29731107 | 1157 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|    467865 | 1158 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    467865 | 1159 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    467865 | 1160 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    467865 | 1161 | `			int isSpecial = 0;` |
|    467865 | 1162 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|    374453 | 1163 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|    374453 | 1164 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|    374448 | 1165 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    349081 | 1166 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    189126 | 1167 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|    148019 | 1168 | `					isSpecial = 1;` |
|     74007 | 1169 | `				}` |
|    210577 | 1170 | `			}` |
|    514571 | 1171 | `			pInstr->iP1 = 0;` |
|         - | 1172 | ``			/* A leading `\` (NSSEP) makes the class name ABSOLUTE: it names the`` |
|         - | 1173 | `			 * global class, so it must NOT be re-qualified with the current namespace.` |
|         - | 1174 | ``			 * `\Closure::bind(...)` inside `namespace X` is Closure, not X\Closure.`` |
|         - | 1175 | ``			 * (Multi-component `\A\B::m` already resolves absolutely because its`` |
|         - | 1176 | ``			 * literal keeps a backslash; the single-component `\Closure` lost it.) */`` |
|         - | 1177 | `			{` |
|    725148 | 1178 | `				int bAbsolute = (pNode->pLeft && pNode->pLeft->pStart` |
|    631731 | 1179 | `					&& (pNode->pLeft->pStart->nType & PH7_TK_NSSEP));` |
|    421159 | 1180 | `				if( !isSpecial && !bAbsolute ){` |
|    273127 | 1181 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|    136561 | 1182 | `				}` |
|         - | 1183 | `			}` |
|         - | 1184 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|         - | 1185 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|    421159 | 1186 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|    273145 | 1187 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|    273145 | 1188 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|        78 | 1189 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|        78 | 1190 | `					return SXRET_OK;` |
|         - | 1191 | `				}` |
|    136533 | 1192 | `			}` |
|    210540 | 1193 | `		}` |
|    303925 | 1194 | `	}` |
|         - | 1195 | `	/* Generate code for the right tree */` |
|  29684345 | 1196 | `	if( pNode->pRight ){` |
|  17048705 | 1197 | `		if( iVmOp == PH7_OP_LAND ){` |
|         - | 1198 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    451747 | 1199 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  16822834 | 1200 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|         - | 1201 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    326923 | 1202 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  16433504 | 1203 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|         - | 1204 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     58521 | 1205 | `			iVmOp = 0; /* No binary operator to emit */` |
|     58521 | 1206 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  16240839 | 1207 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|         - | 1208 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|         - | 1209 | `			 * the entire containing postfix chain to null. The jump target is` |
|         - | 1210 | `			 * patched later by the innermost non-chain ancestor (or by` |
|         - | 1211 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|         - | 1212 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|         - | 1213 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|       107 | 1214 | `			sxu32 nNsJmp = 0;` |
|       107 | 1215 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|       107 | 1216 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  16211477 | 1217 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */ ){` |
|         - | 1218 | `			/* The lvalue is the RIGHT operand (these ops are right-associative). Mark it a write` |
|         - | 1219 | ``			 * target so a missing member (the base of a subscript-write, or a bare `$o->p`) is`` |
|         - | 1220 | `			 * auto-created — PHP auto-vivifies on write. */` |
|   5228601 | 1221 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|   5228601 | 1222 | `			if( iVmOp != PH7_OP_STORE ){` |
|         - | 1223 | ``				/* COMPOUND assignment (`.=`, `+=`, ...) READS the target first, so`` |
|         - | 1224 | `` 				 * php warns when it is undefined and then seeds it; a plain `=` `` |
|         - | 1225 | `				 * writes without reading and stays silent. */` |
|    451595 | 1226 | `				iFlags \|= EXPR_FLAG_RMW_LOAD;` |
|    225795 | 1227 | `			}` |
|   2614298 | 1228 | `		}` |
|  17048705 | 1229 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  17048705 | 1230 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_RDONLY_LOAD);` |
|  17048705 | 1231 | `		if( !bIsChainOp ){` |
|         - | 1232 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|         - | 1233 | `			 * operator instruction is emitted. */` |
|  11042147 | 1234 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|   5521071 | 1235 | `		}` |
|  17048705 | 1236 | `		if( iVmOp == PH7_OP_STORE ){` |
|   4777011 | 1237 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|   4776974 | 1238 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|         - | 1239 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|         - | 1240 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|         - | 1241 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|         - | 1242 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|         - | 1243 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|         - | 1244 | `				 */` |
|        91 | 1245 | `				iVmOp = 0;` |
|   4776968 | 1246 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|   4776925 | 1247 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 1248 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|    922705 | 1249 | `					iP2 = 1;` |
|    461355 | 1250 | `				}else{` |
|   3854225 | 1251 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 1252 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|    537637 | 1253 | `						iVmOp = PH7_OP_STORE_IDX;` |
|    537637 | 1254 | `						iP1 = pInstr->iP1;` |
|    268821 | 1255 | `					}else{` |
|   3316593 | 1256 | `						p3 = pInstr->p3;` |
|         - | 1257 | `					}` |
|         - | 1258 | `					/* POP the last dynamic load instruction */` |
|   3854225 | 1259 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         - | 1260 | `				}` |
|   2388465 | 1261 | `			}` |
|  14660202 | 1262 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|         - | 1263 | `			/* Peek first: a member LHS ($o->p =& $x, C::$s =& $x) keeps its` |
|         - | 1264 | `			 * OP_MEMBER in place (it resolves + stashes the target slot at` |
|         - | 1265 | `			 * runtime), unlike the variable/array shapes which fold their load` |
|         - | 1266 | `			 * away. Mirrors the normal member-store fold above (iP2 kept-MEMBER). */` |
|        73 | 1267 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|        73 | 1268 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 1269 | `				/* Tag the member as a reference target and flag STORE_REF (iP2=1)` |
|         - | 1270 | `				 * to take the member-rebind path in the VM. */` |
|        11 | 1271 | `				pInstr->iP2 = PH7_MEMBER_REF_TARGET;` |
|        11 | 1272 | `				iP2 = 1;` |
|         6 | 1273 | `			}else{` |
|        63 | 1274 | `				pInstr = PH7_VmPopInstr(pGen->pVm);` |
|        63 | 1275 | `				if( pInstr ){` |
|        63 | 1276 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 1277 | `						/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|         - | 1278 | `						 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|         - | 1279 | `						 */` |
|        19 | 1280 | `						iVmOp = PH7_OP_STORE_IDX_REF;` |
|        19 | 1281 | `						iP1 = pInstr->iP1;` |
|        19 | 1282 | `						iP2 = pInstr->iP2;` |
|        19 | 1283 | `						p3  = pInstr->p3;` |
|        10 | 1284 | `					}else{` |
|        45 | 1285 | `						p3 = pInstr->p3;` |
|         - | 1286 | `					}` |
|        30 | 1287 | `				}` |
|         - | 1288 | `			}` |
|        35 | 1289 | `		}` |
|   8524350 | 1290 | `	}` |
|  29684340 | 1291 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|    433612 | 1292 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|         - | 1293 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|         - | 1294 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|        34 | 1295 | `		iVmOp = 0;` |
|        15 | 1296 | `	}` |
|  29684345 | 1297 | `	if( iVmOp > 0 ){` |
|  29625709 | 1298 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|    218285 | 1299 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|         - | 1300 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|     15593 | 1301 | `				iP1 = 1;` |
|      7799 | 1302 | `			}` |
|  29516569 | 1303 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|         - | 1304 | `			/* Namespace-qualify the class name for NEW */ {` |
|    870457 | 1305 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|    870457 | 1306 | `				VmInstr *pCallInstr = 0;` |
|    870457 | 1307 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|    858587 | 1308 | `					pCallInstr = pPeek;` |
|    858587 | 1309 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    429291 | 1310 | `				}` |
|    870457 | 1311 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|    854889 | 1312 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 1313 | `					sxu32 nLitForClass;` |
|    854889 | 1314 | `					VmCallArgMap *pCallNsMap = pCallInstr ? (VmCallArgMap *)pCallInstr->p3 : 0;` |
|         - | 1315 | `					/* If the CALL handler qualified the name with FUNCTION` |
|         - | 1316 | `					 * imports, recover the original literal (recorded in the` |
|         - | 1317 | `					 * arg map — OP_CALL's iP2 is the hasSpread flag, and` |
|         - | 1318 | `` 					 * misreading it as a literal index made `new C(...$args)` `` |
|         - | 1319 | `					 * fatal with "Class ' ' is not defined") and re-qualify` |
|         - | 1320 | `					 * with class imports. */` |
|    854889 | 1321 | `					if( pCallNsMap && pCallNsMap->nOrigNameLit > 0 ){` |
|        55 | 1322 | `						nLitForClass = pCallNsMap->nOrigNameLit - 1;` |
|        30 | 1323 | `					}else{` |
|    854839 | 1324 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|         - | 1325 | `					}` |
|    854889 | 1326 | `					pPeek->iP1 = 0;` |
|    854889 | 1327 | `					if( !bAbsolute ){` |
|         - | 1328 | `						/* self/static/parent are resolved at runtime against the` |
|         - | 1329 | `						 * current class — never namespace-qualify them (else` |
|         - | 1330 | ``						 * `new self` in namespace N becomes "N\self"). Mirrors the`` |
|         - | 1331 | `						 * instanceof (IS_A) guard below. */` |
|    850971 | 1332 | `						ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nLitForClass);` |
|    850971 | 1333 | `						int isSpecialNew = 0;` |
|    850971 | 1334 | `						if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|    835803 | 1335 | `							const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|    835803 | 1336 | `							sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|    835798 | 1337 | `							if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    839537 | 1338 | `								(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    417847 | 1339 | `								(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      7819 | 1340 | `								isSpecialNew = 1;` |
|      3907 | 1341 | `							}` |
|    421691 | 1342 | `						}` |
|    858555 | 1343 | `						if( isSpecialNew ){` |
|      7819 | 1344 | `							pPeek->iP2 = (sxi32)nLitForClass;` |
|      3912 | 1345 | `						}else{` |
|    835573 | 1346 | `							pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|         - | 1347 | `						}` |
|    421696 | 1348 | `					}else{` |
|      3923 | 1349 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|         - | 1350 | `					}` |
|    423650 | 1351 | `				}` |
|         - | 1352 | `			}` |
|    862873 | 1353 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    862873 | 1354 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|         - | 1355 | `				VmInstr *pPrev;` |
|    858587 | 1356 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|    858587 | 1357 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|         - | 1358 | `					/* Pop the call instruction, preserve named-arg map and` |
|         - | 1359 | `					 * the hasSpread flag (OP_NEW consumes the spread` |
|         - | 1360 | `					 * accumulator exactly like OP_CALL would have). */` |
|    858587 | 1361 | `					iP1 = pInstr->iP1;` |
|    858587 | 1362 | `					iP2 = pInstr->iP2;` |
|    858587 | 1363 | `					if( pInstr->p3 ){` |
|        65 | 1364 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|        30 | 1365 | `					}` |
|    858587 | 1366 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    429291 | 1367 | `				}` |
|    429296 | 1368 | `			}` |
|  28968411 | 1369 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|         - | 1370 | `			/* instanceof: right operand is a class name, not a constant.` |
|         - | 1371 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     82003 | 1372 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     82003 | 1373 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     82003 | 1374 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     82003 | 1375 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     82003 | 1376 | `				int isSpecialIs = 0;` |
|     82003 | 1377 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     82003 | 1378 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     82003 | 1379 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     81998 | 1380 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     82001 | 1381 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     40999 | 1382 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|        12 | 1383 | `						isSpecialIs = 1;` |
|         5 | 1384 | `					}` |
|     40999 | 1385 | `				}` |
|     82003 | 1386 | `				pInstr->iP1 = 0;` |
|     82003 | 1387 | `				if( !isSpecialIs && !bAbsolute ){` |
|     81983 | 1388 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     40989 | 1389 | `				}` |
|     41004 | 1390 | `			}` |
|  28495978 | 1391 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|         - | 1392 | `			/* Prevent constant expansion for member/property names.` |
|         - | 1393 | `			 * The right child (member name) was just compiled — its LOADC` |
|         - | 1394 | `			 * should not trigger constant lookup. */` |
|   6006563 | 1395 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   6006563 | 1396 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|   5765281 | 1397 | `				pInstr->iP1 = 0;` |
|   2882638 | 1398 | `			}` |
|   6006563 | 1399 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|         - | 1400 | `				/* Static member access,remember that */` |
|    421103 | 1401 | `				iP1 = 1;` |
|    421103 | 1402 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    421103 | 1403 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|    237383 | 1404 | `					p3 = pInstr->p3;` |
|    237383 | 1405 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    118689 | 1406 | `				}` |
|    210549 | 1407 | `			}` |
|         - | 1408 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|         - | 1409 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|         - | 1410 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|         - | 1411 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|   6006563 | 1412 | `			if( iP2 == PH7_MEMBER_READ ){` |
|   6006563 | 1413 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|        42 | 1414 | `					iP2 = PH7_MEMBER_UNSET;` |
|   6006543 | 1415 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|     70163 | 1416 | `					iP2 = PH7_MEMBER_ISSET;` |
|   5971444 | 1417 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|        17 | 1418 | `					iP2 = PH7_MEMBER_EMPTY;` |
|   5936357 | 1419 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|         - | 1420 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|   1117391 | 1421 | `					iP2 = PH7_MEMBER_WRITE;` |
|    558693 | 1422 | `				}` |
|   3003279 | 1423 | `			}` |
|   3003279 | 1424 | `		}` |
|         - | 1425 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|         - | 1426 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|         - | 1427 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|         - | 1428 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|         - | 1429 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
|  29618125 | 1430 | `		if( bFcc ){` |
|        81 | 1431 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|        81 | 1432 | `			iP2 = 0;` |
|        81 | 1433 | `			p3 = 0;` |
|        81 | 1434 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|        81 | 1435 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 1436 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|         - | 1437 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|         - | 1438 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|         - | 1439 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|        37 | 1440 | `				void *pMemberName = pInstr->p3;` |
|        37 | 1441 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|        37 | 1442 | `				if( pMemberName ){` |
|       ! 0 | 1443 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|       ! 0 | 1444 | `				}` |
|        37 | 1445 | `				iP1 = 2;` |
|        19 | 1446 | `			}else{` |
|        45 | 1447 | `				iP1 = 1;` |
|         - | 1448 | `			}` |
|        40 | 1449 | `		}` |
|         - | 1450 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|         - | 1451 | `		 * This is the primary emit path for user-visible calls. */` |
|  29618125 | 1452 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|   7938605 | 1453 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|   3969300 | 1454 | `		}` |
|         - | 1455 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  29618125 | 1456 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  14809060 | 1457 | `	}` |
|  29676761 | 1458 | `	if( nJmpIdx > 0 ){` |
|         - | 1459 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|    837181 | 1460 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|    837181 | 1461 | `		if( pInstr ){` |
|    837181 | 1462 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    418588 | 1463 | `		}` |
|    418588 | 1464 | `	}` |
|  29676761 | 1465 | `	return rc;` |
|  38107006 | 1466 | `}` |
|         - | 1467 | `/*` |
|         - | 1468 | ` * Compile a PHP expression.` |
|         - | 1469 | ` * According to the PHP language reference manual:` |
|         - | 1470 | ` *  Expressions are the most important building stones of PHP.` |
|         - | 1471 | ` *  In PHP, almost anything you write is an expression.` |
|         - | 1472 | ` *  The simplest yet most accurate way to define an expression` |
|         - | 1473 | ` *  is "anything that has a value".` |
|         - | 1474 | ` * If something goes wrong while compiling the expression,this` |
|         - | 1475 | ` * function takes care of generating the appropriate error` |
|         - | 1476 | ` * message.` |
|         - | 1477 | ` */` |
|         - | 1478 | `/*` |
|         - | 1479 | ` * Does this expression tree contain a comma OPERATOR node?` |
|         - | 1480 | ` *` |
|         - | 1481 | `` * PH7 shipped `,` as a lowest-precedence binary operator (IMP-0139-COMMA), so`` |
|         - | 1482 | `` * `(1, 2)` and `$x = (f(), $y)` compile and evaluate to the right operand.`` |
|         - | 1483 | ` * php 8 has no comma operator: its grammar only allows comma-separated` |
|         - | 1484 | ` * expression LISTS inside for(...) clauses (call arguments, array literals and` |
|         - | 1485 | ` * list() are split by the parser, never by this node). Accepting it changes the` |
|         - | 1486 | ` * meaning of source php rejects, which §10 classes as a bug — so every context` |
|         - | 1487 | ` * except for() now reports php's parse error.` |
|         - | 1488 | ` */` |
| 252997606 | 1489 | `static int GenStateTreeHasComma(ph7_expr_node *pNode)` |
|         5 | 1490 | `{` |
|         - | 1491 | `	ph7_expr_node **apArg;` |
|         - | 1492 | `	sxu32 n;` |
| 252997611 | 1493 | `	if( pNode == 0 ){` |
| 177866081 | 1494 | `		return 0;` |
|         - | 1495 | `	}` |
|  75131535 | 1496 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         6 | 1497 | `		return 1;` |
|         - | 1498 | `	}` |
|  75131526 | 1499 | `	if( GenStateTreeHasComma(pNode->pLeft) \|\| GenStateTreeHasComma(pNode->pRight)` |
|  75131527 | 1500 | `	 \|\| GenStateTreeHasComma(pNode->pCond) ){` |
|         6 | 1501 | `		return 1;` |
|         - | 1502 | `	}` |
|  75131527 | 1503 | `	apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  85939193 | 1504 | `	for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; n++ ){` |
|  10807671 | 1505 | `		if( GenStateTreeHasComma(apArg[n]) ){` |
|       ! 0 | 1506 | `			return 1;` |
|         - | 1507 | `		}` |
|   5403838 | 1508 | `	}` |
|  75131527 | 1509 | `	return 0;` |
| 126498808 | 1510 | `}` |
|  17169446 | 1511 | `PH7_PRIVATE sxi32 PH7_CompileExpr(` |
|         - | 1512 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 1513 | `	sxi32 iFlags,        /* Control flags */` |
|         - | 1514 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|         - | 1515 | `	)` |
|         5 | 1516 | `{` |
|         - | 1517 | `	ph7_expr_node *pRoot;` |
|         - | 1518 | `	SySet sExprNode;` |
|         - | 1519 | `	SyToken *pEnd;` |
|         - | 1520 | `	sxi32 nExpr;` |
|         - | 1521 | `	sxi32 iNest;` |
|         - | 1522 | `	sxi32 rc;` |
|         - | 1523 | `	sxu32 nNullsafeBase;` |
|         - | 1524 | `	/* Initialize worker variables */` |
|  17169451 | 1525 | `	nExpr = 0;` |
|  17169451 | 1526 | `	pRoot = 0;` |
|         - | 1527 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|         - | 1528 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  17169451 | 1529 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  17169451 | 1530 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  17169451 | 1531 | `	SySetAlloc(&sExprNode,0x10);` |
|  17169451 | 1532 | `	rc = SXRET_OK;` |
|         - | 1533 | `	/* Delimit the expression */` |
|  17169451 | 1534 | `	pEnd = pGen->pIn;` |
|  17169451 | 1535 | `	iNest = 0;` |
| 134409039 | 1536 | `	while( pEnd < pGen->pEnd ){` |
| 127807727 | 1537 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 1538 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|      4735 | 1539 | `			iNest++;` |
| 127805362 | 1540 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|      4745 | 1541 | `			iNest--;` |
| 127800627 | 1542 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  10569013 | 1543 | `			if( iNest <= 0 ){` |
|  10568139 | 1544 | `				break;` |
|         - | 1545 | `			}` |
|       437 | 1546 | `		}` |
| 117239593 | 1547 | `		pEnd++;` |
|         5 | 1548 | `	}` |
|  17169451 | 1549 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|    740229 | 1550 | `		SyToken *pEnd2 = pGen->pIn;` |
|    740229 | 1551 | `		iNest = 0;` |
|         - | 1552 | `		/* Stop at the first comma */` |
|   1625469 | 1553 | `		while( pEnd2 < pEnd ){` |
|    885247 | 1554 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|     50687 | 1555 | `				iNest++;` |
|    859906 | 1556 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|     50687 | 1557 | `				iNest--;` |
|    809224 | 1558 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      6061 | 1559 | `				if( iNest <= 0 ){` |
|         3 | 1560 | `					break;` |
|         - | 1561 | `				}` |
|      3027 | 1562 | `			}` |
|    885245 | 1563 | `			pEnd2++;` |
|         5 | 1564 | `		}` |
|    740229 | 1565 | `		if( pEnd2 <pEnd ){` |
|         3 | 1566 | `			pEnd = pEnd2;` |
|         1 | 1567 | `		}` |
|    370112 | 1568 | `	}` |
|  17169451 | 1569 | `	if( pEnd > pGen->pIn ){` |
|  17146103 | 1570 | `		SyToken *pTmp = pGen->pEnd;` |
|         - | 1571 | `		/* Swap delimiter */` |
|  17146103 | 1572 | `		pGen->pEnd = pEnd;` |
|         - | 1573 | `		/* Try to get an expression tree */` |
|  17146103 | 1574 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  17146098 | 1575 | `		if( rc == SXRET_OK && pRoot && pGen->nCommaExprOk < 1` |
|  16970649 | 1576 | `		 && GenStateTreeHasComma(pRoot) ){` |
|         - | 1577 | `			/* php has no comma operator outside a for() clause */` |
|         6 | 1578 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pRoot->pStart->nLine,` |
|         - | 1579 | `				"syntax error, unexpected token \",\"");` |
|         6 | 1580 | `			pGen->pEnd = pTmp;` |
|         6 | 1581 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 1582 | `				SySetRelease(&sExprNode);` |
|       ! 0 | 1583 | `				return SXERR_ABORT;` |
|         - | 1584 | `			}` |
|         6 | 1585 | `			pGen->pIn = pEnd;` |
|         6 | 1586 | `			SySetRelease(&sExprNode);` |
|         6 | 1587 | `			SySetTruncate(&pGen->aNullsafeJmp,nNullsafeBase);` |
|         6 | 1588 | `			return SXRET_OK;` |
|         - | 1589 | `		}` |
|  17146099 | 1590 | `		if( rc == SXRET_OK && pRoot ){` |
|  17145919 | 1591 | `			rc = SXRET_OK;` |
|  17145919 | 1592 | `			if( xTreeValidator ){` |
|         - | 1593 | `				/* Call the upper layer validator callback */` |
|   1036669 | 1594 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    518332 | 1595 | `			}` |
|  17145919 | 1596 | `			if( rc != SXERR_ABORT ){` |
|         - | 1597 | `				/* Generate code for the given tree */` |
|  17145919 | 1598 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|         - | 1599 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|         - | 1600 | `				 * expression so they short-circuit to its end. */` |
|  17145919 | 1601 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|   8572957 | 1602 | `			}` |
|  17145919 | 1603 | `			nExpr = 1;` |
|   8572957 | 1604 | `		}` |
|         - | 1605 | `		/* Release the whole tree */` |
|  17146099 | 1606 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|         - | 1607 | `		/* Synchronize token stream */` |
|  17146099 | 1608 | `		pGen->pEnd = pTmp;` |
|  17146099 | 1609 | `		pGen->pIn  = pEnd;` |
|  17146099 | 1610 | `		if( rc == SXERR_ABORT ){` |
|        18 | 1611 | `			SySetRelease(&sExprNode);` |
|        18 | 1612 | `			return SXERR_ABORT;` |
|         - | 1613 | `		}` |
|   8573040 | 1614 | `	}` |
|  17169433 | 1615 | `	SySetRelease(&sExprNode);` |
|  17169433 | 1616 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|   8584728 | 1617 | `}` |
|         - | 1618 | `/*` |
|         - | 1619 | ` * Return a pointer to the node construct handler associated` |
|         - | 1620 | ` * with a given node type [i.e: string,integer,float,...].` |
|         - | 1621 | ` */` |
|   9536204 | 1622 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|         5 | 1623 | `{` |
|   9536209 | 1624 | `	if( nNodeType & PH7_TK_NUM ){` |
|         - | 1625 | `		/* Numeric literal: Either real or integer */` |
|   3835129 | 1626 | `		return PH7_CompileNumLiteral;` |
|   5701085 | 1627 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|         - | 1628 | `		/* Double quoted string */` |
|    125215 | 1629 | `		return PH7_CompileString;` |
|   5575875 | 1630 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|         - | 1631 | `		/* Single quoted string */` |
|   5575751 | 1632 | `		return PH7_CompileSimpleString;` |
|       129 | 1633 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|         - | 1634 | `		/* Heredoc */` |
|        73 | 1635 | `		return PH7_CompileHereDoc;` |
|        60 | 1636 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|         - | 1637 | `		/* Nowdoc */` |
|        55 | 1638 | `		return PH7_CompileNowDoc;` |
|         6 | 1639 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|         - | 1640 | `		/* Backtick quoted string */` |
|         3 | 1641 | `		return PH7_CompileBacktic;` |
|         - | 1642 | `	}` |
|         3 | 1643 | `	return 0;` |
|   4768107 | 1644 | `}` |
|         - | 1645 | `/*` |
|         - | 1646 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|         - | 1647 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|         - | 1648 | ` * in write context" parse error.` |
|         - | 1649 | ` */` |
|     23458 | 1650 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|         5 | 1651 | `{` |
|         - | 1652 | `	sxi32 rc;` |
|     23463 | 1653 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|     23461 | 1654 | `		return SXRET_OK;` |
|         - | 1655 | `	}` |
|         5 | 1656 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|         2 | 1657 | `		pNode ? pNode->pStart->nLine : 1,` |
|         - | 1658 | `		"Can't use nullsafe operator in write context");` |
|         3 | 1659 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|     11734 | 1660 | `}` |
|         - | 1661 | `/*` |
|         - | 1662 | ` * Compile an unset() statement.` |
|         - | 1663 | ` * unset($var, $arr[$key], ...);` |
|         - | 1664 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|         - | 1665 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|         - | 1666 | ` * parent array before extracting the element to unset.` |
|         - | 1667 | ` */` |
|     26170 | 1668 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|         5 | 1669 | `{` |
|     26175 | 1670 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|     26175 | 1671 | `	sxu32 nIdx = 0;` |
|         - | 1672 | `	SyString sName;` |
|         - | 1673 | `	sxi32 rc;` |
|         - | 1674 | `	/* Jump the 'unset' keyword */` |
|     26175 | 1675 | `	pGen->pIn++;` |
|         - | 1676 | `	/* Save delimiter */` |
|     26175 | 1677 | `	pTmp = pGen->pEnd;` |
|         - | 1678 | `	/* Skip optional opening parenthesis and find the matching close */` |
|     26175 | 1679 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|     26175 | 1680 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 1681 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|         - | 1682 | `		SyToken *pClose;` |
|     26175 | 1683 | `		pGen->pIn++;   /* Skip '(' */` |
|     26175 | 1684 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|     26175 | 1685 | `		pEnd = pClose; /* Stop at ')' */` |
|     13085 | 1686 | `	}` |
|     26175 | 1687 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|         - | 1688 | `	/* Resolve the 'unset' builtin name once */` |
|     26175 | 1689 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|      3895 | 1690 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      3895 | 1691 | `		if( pObj == 0 ){` |
|       ! 0 | 1692 | `			return SXERR_ABORT;` |
|         - | 1693 | `		}` |
|      3895 | 1694 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      3895 | 1695 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      1945 | 1696 | `	}` |
|         - | 1697 | `	/* Compile each comma-separated argument */` |
|     56225 | 1698 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|     30055 | 1699 | `		if( pGen->pIn < pNext ){` |
|         - | 1700 | `			/*` |
|         - | 1701 | ``			 * A PLAIN variable (`unset($x)`, exactly two tokens: '$' and the name) drops a`` |
|         - | 1702 | `			 * single NAME binding, which the unset() builtin cannot express — all it ever` |
|         - | 1703 | `			 * receives is the value's slot index, and unsetting the slot destroys whatever` |
|         - | 1704 | `			 * else aliases it. Emit OP_UNSET_VAR with the name instead. Subscripts and` |
|         - | 1705 | ``			 * properties (`unset($a[k])`, `unset($o->p)`) keep the existing path, which`` |
|         - | 1706 | `			 * already removes just the element/property.` |
|         - | 1707 | `			 */` |
|     30050 | 1708 | `			if( &pGen->pIn[2] == pNext` |
|     18321 | 1709 | `				&& (pGen->pIn[0].nType & PH7_TK_DOLLAR)` |
|      6597 | 1710 | `				&& (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         - | 1711 | `				SyString *pVarName;` |
|      9890 | 1712 | `				char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      6590 | 1713 | `					pGen->pIn[1].sData.zString,pGen->pIn[1].sData.nByte);` |
|      6595 | 1714 | `				pVarName = (SyString *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(SyString));` |
|      6595 | 1715 | `				if( zDup == 0 \|\| pVarName == 0 ){` |
|       ! 0 | 1716 | `					PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - | 1717 | `						"Fatal, PH7 is running out of memory");` |
|       ! 0 | 1718 | `					return SXERR_ABORT;` |
|         - | 1719 | `				}` |
|      6595 | 1720 | `				SyStringInitFromBuf(pVarName,zDup,pGen->pIn[1].sData.nByte);` |
|      6595 | 1721 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_UNSET_VAR,0,0,pVarName,0);` |
|      6595 | 1722 | `				pGen->pIn = pNext;` |
|      6595 | 1723 | `				if( pGen->pIn < pEnd ){` |
|      3881 | 1724 | `					pGen->pIn++; /* Jump the trailing comma */` |
|      1938 | 1725 | `				}` |
|      6595 | 1726 | `				continue;` |
|         - | 1727 | `			}` |
|     23465 | 1728 | `			pGen->pEnd = pNext;` |
|     23465 | 1729 | `			rc = PH7_CompileExpr(&(*pGen),` |
|         - | 1730 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|         - | 1731 | `				GenStateUnsetValidator);` |
|     23465 | 1732 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 1733 | `				return SXERR_ABORT;` |
|         - | 1734 | `			}` |
|     23465 | 1735 | `			if( rc != SXERR_EMPTY ){` |
|         - | 1736 | `				/* Emit call for this single argument */` |
|     23463 | 1737 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     23463 | 1738 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|     23463 | 1739 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     11729 | 1740 | `			}` |
|     11730 | 1741 | `		}` |
|         - | 1742 | `		/* Jump trailing commas */` |
|     23471 | 1743 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|         7 | 1744 | `			pNext++;` |
|         1 | 1745 | `		}` |
|     23465 | 1746 | `		pGen->pIn = pNext;` |
|         5 | 1747 | `	}` |
|         - | 1748 | `	/* Skip past the closing ')' if present */` |
|     26175 | 1749 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|     26175 | 1750 | `		pGen->pIn++;` |
|     13085 | 1751 | `	}` |
|         - | 1752 | `	/* Restore token stream */` |
|     26175 | 1753 | `	pGen->pEnd = pTmp;` |
|     26175 | 1754 | `	return SXRET_OK;` |
|     13090 | 1755 | `}` |
|         - | 1756 | `/*` |
|         - | 1757 | ` * PHP Language construct table.` |
|         - | 1758 | ` */` |
|         - | 1759 | `static const LangConstruct aLangConstruct[] = {` |
|         - | 1760 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|         - | 1761 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|         - | 1762 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|         - | 1763 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|         - | 1764 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|         - | 1765 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|         - | 1766 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|         - | 1767 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|         - | 1768 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|         - | 1769 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|         - | 1770 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|         - | 1771 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|         - | 1772 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|         - | 1773 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|         - | 1774 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|         - | 1775 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|         - | 1776 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|         - | 1777 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|         - | 1778 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|         - | 1779 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|         - | 1780 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|         - | 1781 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|         - | 1782 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|         - | 1783 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|         - | 1784 | `};` |
|         - | 1785 | `/*` |
|         - | 1786 | ` * Return a pointer to the statement handler routine associated` |
|         - | 1787 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|         - | 1788 | ` */` |
|   8473972 | 1789 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|         - | 1790 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|         - | 1791 | `	SyToken *pLookahed  /* Look-ahead token */` |
|         - | 1792 | `	)` |
|         5 | 1793 | `{` |
|   8473977 | 1794 | `	sxu32 n = 0;` |
|  34221535 | 1795 | `	for(;;){` |
|  68443075 | 1796 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|    535457 | 1797 | `			break;` |
|         - | 1798 | `		}` |
|  67907623 | 1799 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|   7938525 | 1800 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|       ! 0 | 1801 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|       ! 0 | 1802 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|         - | 1803 | `					/* 'static' (class context),return null */` |
|       ! 0 | 1804 | `					return 0;` |
|         - | 1805 | `				}` |
|       ! 0 | 1806 | `			}` |
|   7938520 | 1807 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|     11684 | 1808 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|      5849 | 1809 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|         - | 1810 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|         3 | 1811 | `				return 0;` |
|         - | 1812 | `			}` |
|         - | 1813 | `			/* Return a pointer to the handler.` |
|         - | 1814 | `			*/` |
|   7938523 | 1815 | `			return aLangConstruct[n].xConstruct;` |
|         - | 1816 | `		}` |
|  59969103 | 1817 | `		n++;` |
|         5 | 1818 | `	}` |
|    535457 | 1819 | `	if( pLookahed ){` |
|    535457 | 1820 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|     70149 | 1821 | `			return PH7_CompileClassInterface;` |
|    465313 | 1822 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|    398599 | 1823 | `			return PH7_CompileClass;` |
|     66719 | 1824 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      7885 | 1825 | `			return PH7_CompileTrait;` |
|         - | 1826 | `		}` |
|         - | 1827 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|         - | 1828 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|         - | 1829 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|         - | 1830 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     29417 | 1831 | `	}` |
|         - | 1832 | `	/* Not a language construct */` |
|     58839 | 1833 | `	return 0;` |
|   4236991 | 1834 | `}` |
|         - | 1835 | `/*` |
|         - | 1836 | ` * Check if the given keyword is in fact a PHP language construct.` |
|         - | 1837 | ` * Return TRUE on success. FALSE otheriwse.` |
|         - | 1838 | ` */` |
|     58836 | 1839 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|         5 | 1840 | `{` |
|         - | 1841 | `	int rc;` |
|     58841 | 1842 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     58841 | 1843 | `	if( rc == FALSE ){` |
|     58734 | 1844 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|     15930 | 1845 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|         - | 1846 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|         - | 1847 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|         - | 1848 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|         - | 1849 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|         - | 1850 | `			*/` |
|         - | 1851 | `			){` |
|     58731 | 1852 | `				rc = TRUE;` |
|     29363 | 1853 | `		}` |
|     29367 | 1854 | `	}` |
|     58841 | 1855 | `	return rc;` |
|         5 | 1856 | `}` |
|         - | 1857 | `/*` |
|         - | 1858 | ` * Compile a PHP chunk.` |
|         - | 1859 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 1860 | ` * takes care of generating the appropriate error message.` |
|         - | 1861 | ` */` |
|         - | 1862 | `/*` |
|         - | 1863 | ` * Update pGen->sPendingDoc for the statement whose first token is` |
|         - | 1864 | ` * pGen->pIn: when a docblock trivia is keyed to that token's index in` |
|         - | 1865 | ` * the chunk token set it becomes the pending docblock. An existing` |
|         - | 1866 | ` * pending docblock is LEFT in place otherwise: Zend keeps the last-seen` |
|         - | 1867 | ` * doc comment until a declaration consumes it, so a docblock survives` |
|         - | 1868 | ` * intervening non-declaration statements.` |
|         - | 1869 | ` */` |
|  18185822 | 1870 | `PH7_PRIVATE void GenStateSetPendingDoc(ph7_gen_state *pGen)` |
|         5 | 1871 | `{` |
|  18185827 | 1872 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|  18185827 | 1873 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|  18185827 | 1874 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 1875 | `	sxu32 nIdx, n;` |
|  18185822 | 1876 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|   3429901 | 1877 | `	 \|\| pGen->pIn < pBase \|\| pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|         - | 1878 | `		/* Re-tokenized substream (string interpolation, synthesized code):` |
|         - | 1879 | `		 * indexes do not map to the sidecar */` |
|  14755933 | 1880 | `		return;` |
|         - | 1881 | `	}` |
|   3429899 | 1882 | `	nIdx = (sxu32)(pGen->pIn - pBase);` |
|         - | 1883 | `	/* Attributes must be adjacent to their declaration (unlike docblocks):` |
|         - | 1884 | `	 * reset at every boundary, then collect the groups keyed to this token. */` |
|   3429899 | 1885 | `	SySetReset(&pGen->aPendingAttrs);` |
|  10291447 | 1886 | `	for( n = 0 ; n < nT ; n++ ){` |
|   6861553 | 1887 | `		if( aT[n].nTokIdx != nIdx ){` |
|   6853601 | 1888 | `			continue;` |
|         - | 1889 | `		}` |
|      7957 | 1890 | `		if( aT[n].iKind == PH7_TRIVIA_DOC ){` |
|        29 | 1891 | `			pGen->sPendingDoc = aT[n].sText;` |
|      7945 | 1892 | `		}else if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|      7933 | 1893 | `			SySetPut(&pGen->aPendingAttrs,(const void *)&aT[n]);` |
|      3964 | 1894 | `		}` |
|      3981 | 1895 | `	}` |
|   9092916 | 1896 | `}` |
|         - | 1897 | `/*` |
|         - | 1898 | ` * Hand the pending docblock (if any) to a declaration: duplicate it into` |
|         - | 1899 | ` * the VM allocator (the raw script buffer dies after compilation) and` |
|         - | 1900 | ` * clear the pending slot so sibling declarations do not inherit it.` |
|         - | 1901 | ` */` |
|   4893222 | 1902 | `PH7_PRIVATE void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut)` |
|         5 | 1903 | `{` |
|         - | 1904 | `	char *zDup;` |
|   4893227 | 1905 | `	if( SyStringLength(&pGen->sPendingDoc) < 1 ){` |
|   4893207 | 1906 | `		return;` |
|         - | 1907 | `	}` |
|        35 | 1908 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        10 | 1909 | `		SyStringData(&pGen->sPendingDoc),SyStringLength(&pGen->sPendingDoc));` |
|        25 | 1910 | `	if( zDup ){` |
|        25 | 1911 | `		SyStringInitFromBuf(pOut,zDup,SyStringLength(&pGen->sPendingDoc));` |
|        10 | 1912 | `	}` |
|        25 | 1913 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|   2446616 | 1914 | `}` |
|         - | 1915 | `/*` |
|         - | 1916 | ` * Compile one recorded #[...] attribute group (the span between the group` |
|         - | 1917 | ` * delimiters) into ph7_attribute records appended to pOut. The span is` |
|         - | 1918 | ` * duplicated into the VM allocator FIRST (compiled bytecode and interned` |
|         - | 1919 | ` * names may point into the token text, which must outlive the raw script` |
|         - | 1920 | ` * buffer), then re-tokenized on its own. Each argument expression compiles` |
|         - | 1921 | ` * with the container-swap idiom into its own OP_DONE-terminated set,` |
|         - | 1922 | ` * evaluated lazily at ReflectionAttribute time (PHP semantics).` |
|         - | 1923 | ` */` |
|      7942 | 1924 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut)` |
|         5 | 1925 | `{` |
|         - | 1926 | `	SySet *pToken;` |
|         - | 1927 | `	SyToken *pIn, *pEnd, *pSavedIn, *pSavedEnd;` |
|         - | 1928 | `	char *zSpan;` |
|      7947 | 1929 | `	sxi32 rc = SXRET_OK;` |
|      7947 | 1930 | `	if( SyStringLength(&pTrivia->sText) < 1 ){` |
|       ! 0 | 1931 | `		return SXRET_OK;` |
|         - | 1932 | `	}` |
|     11918 | 1933 | `	zSpan = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      3971 | 1934 | `		SyStringData(&pTrivia->sText),SyStringLength(&pTrivia->sText));` |
|      7947 | 1935 | `	if( zSpan == 0 ){` |
|       ! 0 | 1936 | `		return SXRET_OK;` |
|         - | 1937 | `	}` |
|         - | 1938 | `	/* The token set must outlive compilation too: interned operands may` |
|         - | 1939 | `	 * reference token payloads. Pool-allocated, never released — bounded by` |
|         - | 1940 | `	 * the number of attribute declarations in the program. */` |
|      7947 | 1941 | `	pToken = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      7947 | 1942 | `	if( pToken == 0 ){` |
|       ! 0 | 1943 | `		return SXRET_OK;` |
|         - | 1944 | `	}` |
|      7947 | 1945 | `	SySetInit(pToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      7947 | 1946 | `	PH7_TokenizePHP(zSpan,SyStringLength(&pTrivia->sText),pTrivia->nLine,pToken,0);` |
|      7947 | 1947 | `	pIn = (SyToken *)SySetBasePtr(pToken);` |
|      7947 | 1948 | `	pEnd = &pIn[SySetUsed(pToken)];` |
|      7947 | 1949 | `	pSavedIn = pGen->pIn;` |
|      7947 | 1950 | `	pSavedEnd = pGen->pEnd;` |
|      7951 | 1951 | `	while( pIn < pEnd ){` |
|         - | 1952 | `		ph7_attribute sAttr;` |
|         - | 1953 | `		SyBlob sFQN;` |
|      7951 | 1954 | `		int bAbsolute = 0;` |
|      7951 | 1955 | `		SyZero(&sAttr,sizeof(sAttr));` |
|      7951 | 1956 | `		SySetInit(&sAttr.aArgs,&pGen->pVm->sAllocator,sizeof(ph7_attr_arg));` |
|      7951 | 1957 | `		sAttr.nLine = pIn->nLine;` |
|      7951 | 1958 | `		if( pIn->nType & PH7_TK_NSSEP ){` |
|        75 | 1959 | `			bAbsolute = 1;` |
|        75 | 1960 | `			pIn++;` |
|        35 | 1961 | `		}` |
|      7951 | 1962 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|      7951 | 1963 | `		while( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      7951 | 1964 | `			SyBlobAppend(&sFQN,pIn->sData.zString,pIn->sData.nByte);` |
|      7951 | 1965 | `			pIn++;` |
|      7951 | 1966 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       ! 0 | 1967 | `				SyBlobAppend(&sFQN,"\\",1);` |
|       ! 0 | 1968 | `				pIn++;` |
|       ! 0 | 1969 | `				continue;` |
|         - | 1970 | `			}` |
|      7951 | 1971 | `			break;` |
|       ! 0 | 1972 | `		}` |
|      7951 | 1973 | `		if( SyBlobLength(&sFQN) < 1 ){` |
|         - | 1974 | `			/* Malformed group: stop quietly (the group was inert trivia before` |
|         - | 1975 | `			 * this feature; never turn it into a new fatal) */` |
|       ! 0 | 1976 | `			SyBlobRelease(&sFQN);` |
|       ! 0 | 1977 | `			break;` |
|         - | 1978 | `		}` |
|         - | 1979 | `		/* Resolve to an FQN: absolute names verbatim; else use-import alias,` |
|         - | 1980 | `		 * else current-namespace prefix (PHP attribute name resolution) */` |
|         - | 1981 | `		{` |
|      7951 | 1982 | `			const char *zName = (const char *)SyBlobData(&sFQN);` |
|      7951 | 1983 | `			sxu32 nName = SyBlobLength(&sFQN);` |
|      7951 | 1984 | `			char *zDup = 0;` |
|      7951 | 1985 | `			if( !bAbsolute ){` |
|      7881 | 1986 | `				SyHashEntry *pImp = SyHashGet(&pGen->hUseImports,(const void *)zName,nName);` |
|      7881 | 1987 | `				if( pImp ){` |
|       ! 0 | 1988 | `					const char *zFqn = (const char *)pImp->pUserData;` |
|       ! 0 | 1989 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFqn,SyStrlen(zFqn));` |
|       ! 0 | 1990 | `					if( zDup ){` |
|       ! 0 | 1991 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyStrlen(zDup));` |
|       ! 0 | 1992 | `					}` |
|      7881 | 1993 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         - | 1994 | `					SyBlob sTmp;` |
|       ! 0 | 1995 | `					SyBlobInit(&sTmp,&pGen->pVm->sAllocator);` |
|       ! 0 | 1996 | `					SyBlobAppend(&sTmp,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       ! 0 | 1997 | `					SyBlobAppend(&sTmp,"\\",1);` |
|       ! 0 | 1998 | `					SyBlobAppend(&sTmp,zName,nName);` |
|       ! 0 | 1999 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       ! 0 | 2000 | `						(const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|       ! 0 | 2001 | `					if( zDup ){` |
|       ! 0 | 2002 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyBlobLength(&sTmp));` |
|       ! 0 | 2003 | `					}` |
|       ! 0 | 2004 | `					SyBlobRelease(&sTmp);` |
|       ! 0 | 2005 | `				}` |
|      3938 | 2006 | `			}` |
|      7951 | 2007 | `			if( SyStringLength(&sAttr.sName) < 1 ){` |
|      7951 | 2008 | `				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|      7951 | 2009 | `				if( zDup ){` |
|      7951 | 2010 | `					SyStringInitFromBuf(&sAttr.sName,zDup,nName);` |
|      3973 | 2011 | `				}` |
|      3973 | 2012 | `			}` |
|         - | 2013 | `		}` |
|      7951 | 2014 | `		SyBlobRelease(&sFQN);` |
|      7951 | 2015 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 2016 | `			SyToken *pArgsEnd;` |
|      7849 | 2017 | `			pIn++;` |
|      7849 | 2018 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pArgsEnd);` |
|     15707 | 2019 | `			while( pIn < pArgsEnd ){` |
|      7863 | 2020 | `				SyToken *pArgStart = pIn, *pArgStop = pIn;` |
|      7863 | 2021 | `				sxi32 iDepth = 0;` |
|         - | 2022 | `				ph7_attr_arg sArgRec;` |
|     78077 | 2023 | `				while( pArgStop < pArgsEnd ){` |
|     70235 | 2024 | `					if( pArgStop->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        11 | 2025 | `						iDepth++;` |
|     70230 | 2026 | `					}else if( pArgStop->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        11 | 2027 | `						iDepth--;` |
|     70220 | 2028 | `					}else if( (pArgStop->nType & PH7_TK_COMMA) && iDepth == 0 ){` |
|        17 | 2029 | `						break;` |
|         - | 2030 | `					}` |
|     70219 | 2031 | `					pArgStop++;` |
|         5 | 2032 | `				}` |
|      7863 | 2033 | `				SyZero(&sArgRec,sizeof(sArgRec));` |
|      7863 | 2034 | `				SySetInit(&sArgRec.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      7858 | 2035 | `				if( pArgStart < pArgStop && (pArgStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|      7840 | 2036 | `				 && &pArgStart[1] < pArgStop && (pArgStart[1].nType & PH7_TK_COLON) ){` |
|        28 | 2037 | `					char *zN = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|         9 | 2038 | `						pArgStart->sData.zString,pArgStart->sData.nByte);` |
|        19 | 2039 | `					if( zN ){` |
|        19 | 2040 | `						SyStringInitFromBuf(&sArgRec.sName,zN,pArgStart->sData.nByte);` |
|         9 | 2041 | `					}` |
|        19 | 2042 | `					pArgStart += 2;` |
|         9 | 2043 | `				}` |
|      7863 | 2044 | `				if( pArgStart < pArgStop ){` |
|         - | 2045 | `					SySet *pInstrContainer;` |
|      7863 | 2046 | `					pGen->pIn = pArgStart;` |
|      7863 | 2047 | `					pGen->pEnd = pArgStop;` |
|      7863 | 2048 | `					pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      7863 | 2049 | `					PH7_VmSetByteCodeContainer(pGen->pVm,&sArgRec.aByteCode);` |
|      7863 | 2050 | `					rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      7863 | 2051 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      7863 | 2052 | `					PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      7863 | 2053 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 2054 | `						pGen->pIn = pSavedIn;` |
|       ! 0 | 2055 | `						pGen->pEnd = pSavedEnd;` |
|       ! 0 | 2056 | `						return SXERR_ABORT;` |
|         - | 2057 | `					}` |
|      7863 | 2058 | `					SySetPut(&sAttr.aArgs,(const void *)&sArgRec);` |
|      3929 | 2059 | `				}` |
|      7863 | 2060 | `				pIn = pArgStop;` |
|      7863 | 2061 | `				if( pIn < pArgsEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        17 | 2062 | `					pIn++;` |
|         8 | 2063 | `				}` |
|         5 | 2064 | `			}` |
|      7849 | 2065 | `			pIn = (pArgsEnd < pEnd) ? &pArgsEnd[1] : pEnd;` |
|      3922 | 2066 | `		}` |
|      7951 | 2067 | `		SySetPut(pOut,(const void *)&sAttr);` |
|      7951 | 2068 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|         5 | 2069 | `			pIn++;` |
|         5 | 2070 | `			continue;` |
|         - | 2071 | `		}` |
|      7947 | 2072 | `		break;` |
|       ! 0 | 2073 | `	}` |
|      7947 | 2074 | `	pGen->pIn = pSavedIn;` |
|      7947 | 2075 | `	pGen->pEnd = pSavedEnd;` |
|      7947 | 2076 | `	return SXRET_OK;` |
|      3976 | 2077 | `}` |
|         - | 2078 | `/*` |
|         - | 2079 | ` * Hand the pending attribute groups (if any) to a declaration: compile` |
|         - | 2080 | ` * every recorded group into pOut and clear the pending list.` |
|         - | 2081 | ` */` |
|   4893226 | 2082 | `PH7_PRIVATE sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut)` |
|         5 | 2083 | `{` |
|   4893231 | 2084 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|         - | 2085 | `	sxu32 n;` |
|         - | 2086 | `	sxi32 rc;` |
|   4901159 | 2087 | `	for( n = 0 ; n < SySetUsed(&pGen->aPendingAttrs) ; n++ ){` |
|      7933 | 2088 | `		rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|      7933 | 2089 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 2090 | `			return SXERR_ABORT;` |
|         - | 2091 | `		}` |
|      3969 | 2092 | `	}` |
|   4893231 | 2093 | `	SySetReset(&pGen->aPendingAttrs);` |
|   4893231 | 2094 | `	return SXRET_OK;` |
|   2446618 | 2095 | `}` |
|         - | 2096 | `/*` |
|         - | 2097 | ` * Compile the attribute groups keyed to the given token (a parameter's` |
|         - | 2098 | ` * first token inside a signature) into pOut. Parameters are parsed from` |
|         - | 2099 | ` * the main token stream, so the sidecar indexes map directly.` |
|         - | 2100 | ` */` |
|   2462258 | 2101 | `PH7_PRIVATE sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut)` |
|         5 | 2102 | `{` |
|   2462263 | 2103 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|   2462263 | 2104 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|   2462263 | 2105 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 2106 | `	sxu32 nIdx, n;` |
|         - | 2107 | `	sxi32 rc;` |
|   2462258 | 2108 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|    556571 | 2109 | `	 \|\| pTok < pBase \|\| pTok >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|   1905697 | 2110 | `		return SXRET_OK;` |
|         - | 2111 | `	}` |
|    556571 | 2112 | `	nIdx = (sxu32)(pTok - pBase);` |
|   1669747 | 2113 | `	for( n = 0 ; n < nT ; n++ ){` |
|   1113181 | 2114 | `		if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|        16 | 2115 | `			rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|        16 | 2116 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 2117 | `				return SXERR_ABORT;` |
|         - | 2118 | `			}` |
|         7 | 2119 | `		}` |
|    556593 | 2120 | `	}` |
|    556571 | 2121 | `	return SXRET_OK;` |
|   1231134 | 2122 | `}` |
|  13343124 | 2123 | `PH7_PRIVATE sxi32 GenStateCompileChunk(` |
|         - | 2124 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 2125 | `	sxi32 iFlags         /* Compile flags */` |
|         - | 2126 | `	)` |
|         5 | 2127 | `{` |
|         - | 2128 | `	ProcLangConstruct xCons;` |
|         - | 2129 | `	sxi32 rc;` |
|  13343129 | 2130 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|   7755428 | 2131 | `	for(;;){` |
|  14426995 | 2132 | `		int bStmtIsDeclare = 0;` |
|  14426995 | 2133 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 2134 | `			/* No more input to process */` |
|     91647 | 2135 | `			break;` |
|         - | 2136 | `		}` |
|         - | 2137 | `		/* Bind a directly-preceding docblock to this statement */` |
|  14335353 | 2138 | `		GenStateSetPendingDoc(&(*pGen));` |
|  14335353 | 2139 | `		if( SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|         - | 2140 | `			/* php: a statement-position attribute group must be followed by a` |
|         - | 2141 | ``			 * declaration (function/class-like/const) — `#[A] $x = 1;` is a`` |
|         - | 2142 | `` 			 * parse error, never a silent discard. `static`/`fn`/`function` `` |
|         - | 2143 | ``			 * cover bare closure-expression statements; `readonly`/`enum` are`` |
|         - | 2144 | `			 * context-sensitive IDs handled by the modified-class/enum scans. */` |
|      7845 | 2145 | `			int bAttrTarget = 0;` |
|      7840 | 2146 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd)` |
|      3955 | 2147 | `			 \|\| GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|      7785 | 2148 | `				bAttrTarget = 1;` |
|      3951 | 2149 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|        61 | 2150 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        60 | 2151 | `				if( nKw == PH7_TKWRD_FUNCTION \|\| nKw == PH7_TKWRD_CLASS` |
|        16 | 2152 | `				 \|\| nKw == PH7_TKWRD_INTERFACE \|\| nKw == PH7_TKWRD_TRAIT` |
|         4 | 2153 | `				 \|\| nKw == PH7_TKWRD_ABSTRACT \|\| nKw == PH7_TKWRD_FINAL` |
|         4 | 2154 | `				 \|\| nKw == PH7_TKWRD_CONST \|\| nKw == PH7_TKWRD_STATIC` |
|         1 | 2155 | `				 \|\| nKw == PH7_TKWRD_FN ){` |
|        61 | 2156 | `					bAttrTarget = 1;` |
|        30 | 2157 | `				}` |
|        30 | 2158 | `			}` |
|      7845 | 2159 | `			if( !bAttrTarget ){` |
|       ! 0 | 2160 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 2161 | `					"syntax error, unexpected token \"%z\" after attribute group; expecting a declaration",` |
|       ! 0 | 2162 | `					&pGen->pIn->sData);` |
|       ! 0 | 2163 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 2164 | `					break;` |
|         - | 2165 | `				}` |
|       ! 0 | 2166 | `				SySetReset(&pGen->aPendingAttrs);` |
|       ! 0 | 2167 | `			}` |
|      3920 | 2168 | `		}` |
|         - | 2169 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|         - | 2170 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  14335353 | 2171 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   8516837 | 2172 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   8516837 | 2173 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|        49 | 2174 | `				bStmtIsDeclare = 1;` |
|        22 | 2175 | `			}` |
|   4258416 | 2176 | `		}` |
|  14335353 | 2177 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|         - | 2178 | `			/* Any non-declare top-level statement locks the strict_types` |
|         - | 2179 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|   1083857 | 2180 | `			pGen->bStrictTypesLocked = 1;` |
|    541926 | 2181 | `		}` |
|  14335353 | 2182 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 2183 | `			/* Compile block */` |
|      3935 | 2184 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      3935 | 2185 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 2186 | `				break;` |
|         - | 2187 | `			}` |
|      1970 | 2188 | `		}else{` |
|  14331423 | 2189 | `			xCons = 0;` |
|  14331423 | 2190 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|         - | 2191 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|         - | 2192 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|         - | 2193 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|     42891 | 2194 | `				xCons = PH7_CompileClassModifiers;` |
|  14309980 | 2195 | `			}else if( GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|         - | 2196 | ``				/* `enum Name …` (PHP 8.1) — `enum` is a context-sensitive ID,`` |
|         - | 2197 | `				 * so it is detected here rather than the keyword dispatcher. */` |
|      3931 | 2198 | `				xCons = PH7_CompileEnum;` |
|  14286574 | 2199 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   8473977 | 2200 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         - | 2201 | `				/* Try to extract a language construct handler */` |
|   8473977 | 2202 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|   8473977 | 2203 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|        13 | 2204 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 2205 | `						"Syntax error: Unexpected keyword '%z'",` |
|         8 | 2206 | `						&pGen->pIn->sData);` |
|         9 | 2207 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 2208 | `						break;` |
|         - | 2209 | `					}` |
|         - | 2210 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|         - | 2211 | `					 * this erroneous statement.` |
|         - | 2212 | `					 */` |
|         9 | 2213 | `					xCons = PH7_ErrorRecover;` |
|         4 | 2214 | `				}` |
|  10047625 | 2215 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|    429083 | 2216 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|         - | 2217 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|       117 | 2218 | `				xCons = PH7_CompileLabel;` |
|        56 | 2219 | `			}` |
|  14331423 | 2220 | `			if( xCons == 0 ){` |
|         - | 2221 | `				/* Assume an expression an try to compile it */` |
|   5869355 | 2222 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   5869355 | 2223 | `				if(  rc != SXERR_EMPTY ){` |
|         - | 2224 | `					/* Pop l-value */` |
|   5869205 | 2225 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|   2934600 | 2226 | `				}` |
|   2934680 | 2227 | `			}else{` |
|         - | 2228 | `				/* Go compile the sucker */` |
|   8462073 | 2229 | `				rc = xCons(&(*pGen));` |
|         - | 2230 | `			}` |
|  14331423 | 2231 | `			if( rc == SXERR_ABORT ){` |
|         - | 2232 | `				/* Request to abort compilation */` |
|        34 | 2233 | `				break;` |
|         - | 2234 | `			}` |
|         - | 2235 | `		}` |
|         - | 2236 | `		/* Ignore trailing semi-colons ';' */` |
|  24616587 | 2237 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  10281269 | 2238 | `			pGen->pIn++;` |
|         5 | 2239 | `		}` |
|  14335323 | 2240 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|         - | 2241 | `			/* Compile a single statement and return */` |
|  13251457 | 2242 | `			break;` |
|         - | 2243 | `		}` |
|         - | 2244 | `		/* LOOP ONE */` |
|         - | 2245 | `		/* LOOP TWO */` |
|         - | 2246 | `		/* LOOP THREE */` |
|         - | 2247 | `		/* LOOP FOUR */` |
|         5 | 2248 | `	}` |
|         - | 2249 | `	/* Return compilation status */` |
|  13343129 | 2250 | `	return rc;` |
|         5 | 2251 | `}` |
|         - | 2252 | `/*` |
|         - | 2253 | ` * Compile a Raw PHP chunk.` |
|         - | 2254 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 2255 | ` * takes care of generating the appropriate error message.` |
|         - | 2256 | ` */` |
|     91674 | 2257 | `static sxi32 PH7_CompilePHP(` |
|         - | 2258 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 2259 | `	SySet *pTokenSet,     /* Token set */` |
|         - | 2260 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|         - | 2261 | `	)` |
|         5 | 2262 | `{` |
|     91679 | 2263 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|         - | 2264 | `	sxi32 rc;` |
|         - | 2265 | `	/* Reset the token set (and its trivia sidecar) */` |
|     91679 | 2266 | `	SySetReset(&(*pTokenSet));` |
|     91679 | 2267 | `	SySetReset(&pGen->aTrivia);` |
|         - | 2268 | `	/* Mark as the default token set */` |
|     91679 | 2269 | `	pGen->pTokenSet = &(*pTokenSet);` |
|         - | 2270 | `	/* Advance the stream cursor */` |
|     91679 | 2271 | `	pGen->pRawIn++;` |
|         - | 2272 | `	/* Tokenize the PHP chunk first */` |
|     91679 | 2273 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet),&pGen->aTrivia);` |
|         - | 2274 | `	/* Point to the head and tail of the token stream. */` |
|     91679 | 2275 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     91679 | 2276 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|     91679 | 2277 | `	if( is_expr ){` |
|       ! 0 | 2278 | `		rc = SXERR_EMPTY;` |
|       ! 0 | 2279 | `		if( pGen->pIn < pGen->pEnd ){` |
|         - | 2280 | `			/* A simple expression,compile it */` |
|       ! 0 | 2281 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|       ! 0 | 2282 | `		}` |
|         - | 2283 | `		/* Emit the DONE instruction */` |
|       ! 0 | 2284 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       ! 0 | 2285 | `		return SXRET_OK;` |
|         - | 2286 | `	}` |
|     91679 | 2287 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|         - | 2288 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|         - | 2289 | `		/*` |
|         - | 2290 | `		 * Shortcut syntax for the 'echo' language construct.` |
|         - | 2291 | `		 * According to the PHP reference manual:` |
|         - | 2292 | `		 *  echo() also has a shortcut syntax, where you can` |
|         - | 2293 | `		 *  immediately follow` |
|         - | 2294 | `		 *  the opening tag with an equals sign as follows:` |
|         - | 2295 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|         - | 2296 | `		 * Symisc extension:` |
|         - | 2297 | `		 *   This short syntax works with all PHP opening` |
|         - | 2298 | `		 *   tags unlike the default PHP engine that handle` |
|         - | 2299 | `		 *   only short tag.` |
|         - | 2300 | `		 */` |
|         - | 2301 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|         3 | 2302 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|         3 | 2303 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|         3 | 2304 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|         3 | 2305 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|         3 | 2306 | `		if( rc != SXERR_EMPTY ){` |
|         3 | 2307 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         1 | 2308 | `		}` |
|         3 | 2309 | `		return SXRET_OK;` |
|         - | 2310 | `	}` |
|         - | 2311 | `	/* Compile the PHP chunk */` |
|     91677 | 2312 | `	rc = GenStateCompileChunk(pGen,0);` |
|         - | 2313 | `	/* Fix exceptions jumps */` |
|     91677 | 2314 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - | 2315 | `	/* Fix gotos now, the jump destination is resolved */` |
|     91677 | 2316 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|         3 | 2317 | `		rc = SXERR_ABORT;` |
|         1 | 2318 | `	}` |
|         - | 2319 | `	/* Reset container */` |
|     91677 | 2320 | `	SySetReset(&pGen->aGoto);` |
|     91677 | 2321 | `	SySetReset(&pGen->aLabel);` |
|     91677 | 2322 | `	SySetReset(&pGen->aNullsafeJmp);` |
|         - | 2323 | `	/* Compilation result */` |
|     91677 | 2324 | `	return rc;` |
|     45842 | 2325 | `}` |
|         - | 2326 | `/*` |
|         - | 2327 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|         - | 2328 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|         - | 2329 | ` * This is the only compile interface exported from this file.` |
|         - | 2330 | ` */` |
|     94824 | 2331 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|         - | 2332 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|         - | 2333 | `	SyString *pScript,  /* Script to compile */` |
|         - | 2334 | `	sxi32 iFlags        /* Compile flags */` |
|         - | 2335 | `	)` |
|         5 | 2336 | `{` |
|         - | 2337 | `	SySet aPhpToken,aRawToken;` |
|         - | 2338 | `	ph7_gen_state *pCodeGen;` |
|         - | 2339 | `	ph7_value *pRawObj;` |
|         - | 2340 | `	sxu32 nObjIdx;` |
|         - | 2341 | `	sxi32 nRawObj;` |
|         - | 2342 | `	int is_expr;` |
|         - | 2343 | `	sxi8 bSavedStrict;` |
|         - | 2344 | `	sxi8 bSavedStrictLocked;` |
|         - | 2345 | `	SyToken *pSavedIn,*pSavedEnd;` |
|         - | 2346 | `	sxi32 rc;` |
|     94829 | 2347 | `	sxu32 nBaseLine = 1;` |
|     94829 | 2348 | `	if( pScript->nByte < 1 ){` |
|         - | 2349 | `		/* Nothing to compile */` |
|       ! 0 | 2350 | `		return PH7_OK;` |
|         - | 2351 | `	}` |
|         - | 2352 | `	/* php skips a "#!" shebang on the first line of a CLI script: consume it` |
|         - | 2353 | `	 * (including its newline) so it is not echoed as inline text, and bump the` |
|         - | 2354 | `	 * base line to 2 so the code below still reports php-matching line numbers. */` |
|     94829 | 2355 | `	if( pScript->nByte >= 2 && pScript->zString[0]=='#' && pScript->zString[1]=='!' ){` |
|         3 | 2356 | `		const char *z = pScript->zString;` |
|         3 | 2357 | `		const char *zEnd = &z[pScript->nByte];` |
|        39 | 2358 | `		while( z < zEnd && z[0] != '\n' ){ z++; }` |
|         3 | 2359 | `		if( z < zEnd ){ z++; } /* consume the newline too */` |
|         3 | 2360 | `		pScript->nByte -= (sxu32)(z - pScript->zString);` |
|         3 | 2361 | `		pScript->zString = z;` |
|         3 | 2362 | `		nBaseLine = 2;` |
|         3 | 2363 | `		if( pScript->nByte < 1 ){` |
|       ! 0 | 2364 | `			return PH7_OK;` |
|         - | 2365 | `		}` |
|         1 | 2366 | `	}` |
|         - | 2367 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|         - | 2368 | `	 * file's flags so include/require restore them on return. */` |
|     94829 | 2369 | `	pCodeGen = &pVm->sCodeGen;` |
|         - | 2370 | ``	/* aPhpToken below is a LOCAL token set: once it is released at `cleanup`, any`` |
|         - | 2371 | `	 * pGen->pIn/pEnd still pointing into it DANGLE. PH7_VmEmitInstr reads pIn to stamp` |
|         - | 2372 | `	 * each instruction's source line, and instructions are still emitted after this` |
|         - | 2373 | `	 * function returns (VmEvalChunk's trailing OP_DONE) -- a use-after-free ASan caught` |
|         - | 2374 | `	 * immediately. Save the caller's cursor and restore it on the way out, so an` |
|         - | 2375 | `	 * enclosing compile keeps its (live) tokens and the top level goes back to NULL. */` |
|     94829 | 2376 | `	pSavedIn = pCodeGen->pIn;` |
|     94829 | 2377 | `	pSavedEnd = pCodeGen->pEnd;` |
|     94829 | 2378 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|     94829 | 2379 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|     94829 | 2380 | `	pCodeGen->bStrictTypes = 0;` |
|     94829 | 2381 | `	pCodeGen->bStrictTypesLocked = 0;` |
|         - | 2382 | `	/* Initialize the tokens containers */` |
|     94829 | 2383 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|     94829 | 2384 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|     94829 | 2385 | `	SySetAlloc(&aPhpToken,0xc0);` |
|     94829 | 2386 | `	is_expr = 0;` |
|     94829 | 2387 | `	if( iFlags & PH7_PHP_ONLY ){` |
|         - | 2388 | `		SyToken sTmp;` |
|         - | 2389 | `		/* PHP only: -*/` |
|     81815 | 2390 | `		sTmp.nLine = 1;` |
|     81815 | 2391 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|     81815 | 2392 | `		sTmp.pUserData = 0;` |
|     81815 | 2393 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|     81815 | 2394 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|     81815 | 2395 | `		if( iFlags & PH7_PHP_EXPR ){` |
|         - | 2396 | `			/* A simple PHP expression */` |
|       ! 0 | 2397 | `			is_expr = 1;` |
|       ! 0 | 2398 | `		}` |
|     40910 | 2399 | `	}else{` |
|         - | 2400 | `		/* Tokenize raw text */` |
|     13019 | 2401 | `		SySetAlloc(&aRawToken,32);` |
|     13019 | 2402 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken,nBaseLine);` |
|         - | 2403 | `	}` |
|         - | 2404 | `	/* Process high-level tokens */` |
|     94829 | 2405 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|     94829 | 2406 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|     94829 | 2407 | `	rc = PH7_OK;` |
|     94829 | 2408 | `	if( is_expr ){` |
|         - | 2409 | `		/* Compile the expression */` |
|       ! 0 | 2410 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|       ! 0 | 2411 | `		goto cleanup;` |
|         - | 2412 | `	}` |
|     94829 | 2413 | `	nObjIdx = 0;` |
|         - | 2414 | `	/* Each compilation unit starts in the global namespace.` |
|         - | 2415 | `	 * Emit NSSWITCH(NULL) so the VM resets namespace state at runtime,` |
|         - | 2416 | `	 * preventing namespace bleeding across include()d files. */` |
|     94829 | 2417 | `	PH7_VmEmitInstr(pVm,PH7_OP_NSSWITCH,0,0,0,0);` |
|         - | 2418 | `	/* Start the compilation process */` |
|     53924 | 2419 | `	for(;;){` |
|    199495 | 2420 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|     94797 | 2421 | `			break; /* No more tokens to process */` |
|         - | 2422 | `		}` |
|    104703 | 2423 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|         - | 2424 | `			/* Compile the PHP chunk */` |
|     91679 | 2425 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|     91679 | 2426 | `			if( rc == SXERR_ABORT ){` |
|        36 | 2427 | `				break;` |
|         - | 2428 | `			}` |
|     91647 | 2429 | `			continue;` |
|         - | 2430 | `		}` |
|         - | 2431 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|     13029 | 2432 | `		nRawObj = 0;` |
|     26053 | 2433 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|         - | 2434 | `			/* Consume the raw chunk without any processing */` |
|     13029 | 2435 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|     13029 | 2436 | `			if( pRawObj == 0 ){` |
|       ! 0 | 2437 | `				rc = SXERR_MEM;` |
|       ! 0 | 2438 | `				break;` |
|         - | 2439 | `			}` |
|         - | 2440 | `			/* Mark as constant and emit the load constant instruction */` |
|     13029 | 2441 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|     13029 | 2442 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     13029 | 2443 | `			++nRawObj;` |
|     13029 | 2444 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|         5 | 2445 | `		}` |
|     13029 | 2446 | `		if( nRawObj > 0 ){` |
|         - | 2447 | `			/* Emit the consume instruction */` |
|     13029 | 2448 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|      6512 | 2449 | `		}` |
|     47417 | 2450 | `	}` |
|     47412 | 2451 | `cleanup:` |
|         - | 2452 | `	/* Drop the cursor into the token set BEFORE the set is freed (see above). */` |
|     94829 | 2453 | `	pCodeGen->pIn = pSavedIn;` |
|     94829 | 2454 | `	pCodeGen->pEnd = pSavedEnd;` |
|     94829 | 2455 | `	SySetRelease(&aRawToken);` |
|     94829 | 2456 | `	SySetRelease(&aPhpToken);` |
|         - | 2457 | `	/* Restore outer file's strict_types scope */` |
|     94829 | 2458 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|     94829 | 2459 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|     94829 | 2460 | `	return rc;` |
|     47417 | 2461 | `}` |
|         - | 2462 | `/*` |
|         - | 2463 | ` * Utility routines.Initialize the code generator.` |
|         - | 2464 | ` */` |
|      3890 | 2465 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|         - | 2466 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 2467 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 2468 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 2469 | `	)` |
|         5 | 2470 | `{` |
|      3895 | 2471 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 2472 | `	/* Zero the structure */` |
|      3895 | 2473 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|         - | 2474 | `	/* Initial state */` |
|      3895 | 2475 | `	pGen->pVm  = &(*pVm);` |
|      3895 | 2476 | `	pGen->xErr = xErr;` |
|      3895 | 2477 | `	pGen->pErrData = pErrData;` |
|      3895 | 2478 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|      3895 | 2479 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|      3895 | 2480 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|      3895 | 2481 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|      3895 | 2482 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|      3895 | 2483 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3895 | 2484 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      3895 | 2485 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|      3895 | 2486 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|         - | 2487 | `	/* Error log buffer */` |
|      3895 | 2488 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|         - | 2489 | `	/* General purpose working buffer */` |
|      3895 | 2490 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|         - | 2491 | `	/* Namespace state */` |
|      3895 | 2492 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|      3895 | 2493 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|      3895 | 2494 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|      3895 | 2495 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 2496 | `	/* Create the global scope */` |
|      3895 | 2497 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|         - | 2498 | `	/* Point to the global scope */` |
|      3895 | 2499 | `	pGen->pCurrent = &pGen->sGlobal;` |
|      3895 | 2500 | `	return SXRET_OK;` |
|         5 | 2501 | `}` |
|         - | 2502 | `/*` |
|         - | 2503 | ` * Utility routines. Reset the code generator to it's initial state.` |
|         - | 2504 | ` */` |
|     98242 | 2505 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|         - | 2506 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 2507 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 2508 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 2509 | `	)` |
|         5 | 2510 | `{` |
|     98247 | 2511 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 2512 | `	GenBlock *pBlock,*pParent;` |
|         - | 2513 | `	/* Reset state */` |
|     98247 | 2514 | `	SySetReset(&pGen->aLabel);` |
|     98247 | 2515 | `	SySetReset(&pGen->aGoto);` |
|     98247 | 2516 | `	SySetReset(&pGen->aNullsafeJmp);` |
|     98247 | 2517 | `	SySetReset(&pGen->aTrivia);` |
|     98247 | 2518 | `	SySetReset(&pGen->aPendingAttrs);` |
|     98247 | 2519 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|     98247 | 2520 | `	SyBlobRelease(&pGen->sErrBuf);` |
|     98247 | 2521 | `	SyBlobRelease(&pGen->sWorker);` |
|     98247 | 2522 | `	SyBlobRelease(&pGen->sNamespace);` |
|     98247 | 2523 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|     98247 | 2524 | `	SyHashRelease(&pGen->hUseImports);` |
|     98247 | 2525 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|     98247 | 2526 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|     98247 | 2527 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|     98247 | 2528 | `	SyHashRelease(&pGen->hUseConstImports);` |
|     98247 | 2529 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 2530 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|         - | 2531 | `	 * They intern variable names and literal strings that are referenced by` |
|         - | 2532 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|         - | 2533 | `	 * Releasing them would either leak the interned strings or require freeing` |
|         - | 2534 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|         - | 2535 | `	 * number of unique names, which is acceptable. */` |
|         - | 2536 | `	/* Point to the global scope */` |
|     98247 | 2537 | `	pBlock = pGen->pCurrent;` |
|     98247 | 2538 | `	while( pBlock->pParent != 0 ){` |
|       ! 0 | 2539 | `		pParent = pBlock->pParent;` |
|       ! 0 | 2540 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 2541 | `		pBlock = pParent;` |
|       ! 0 | 2542 | `	}` |
|     98247 | 2543 | `	pGen->xErr = xErr;` |
|     98247 | 2544 | `	pGen->pErrData = pErrData;` |
|     98247 | 2545 | `	pGen->pCurrent = &pGen->sGlobal;` |
|     98247 | 2546 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|     98247 | 2547 | `	pGen->pIn = pGen->pEnd = 0;` |
|     98247 | 2548 | `	pGen->nErr = 0;` |
|     98247 | 2549 | `	return SXRET_OK;` |
|         5 | 2550 | `}` |
|         - | 2551 | `/*` |
|         - | 2552 | ` * Save the code generator's compile-position state and hand the live generator a` |
|         - | 2553 | ` * fresh, empty one for a NESTED compilation unit.` |
|         - | 2554 | ` *` |
|         - | 2555 | ` * A require/include normally runs at execution time, when no compile is in flight,` |
|         - | 2556 | ` * so VmEvalChunk can call PH7_ResetCodeGenerator to wipe the generator. But an` |
|         - | 2557 | `` * autoload can fire in the MIDDLE of compiling a class: resolving `class Child`` |
|         - | 2558 | `` * extends Base` calls PH7_VmExtractClass, which triggers the autoloader, whose`` |
|         - | 2559 | `` * body `require`s Base's file — a nested compile while the outer one is mid-token.`` |
|         - | 2560 | ` * PH7_ResetCodeGenerator would then blow away the outer compile's cursor` |
|         - | 2561 | ` * (pIn/pEnd), current block, use-imports, labels, etc.; the outer parse resumes` |
|         - | 2562 | ` * with pIn == NULL and reports a bogus "Expected '{' after class 'Child'". (Common` |
|         - | 2563 | ` * in every real framework: Composer autoloads parent classes on demand.)` |
|         - | 2564 | ` *` |
|         - | 2565 | ` * This snapshots the position/scope fields into *pSaved (a caller-stack` |
|         - | 2566 | ` * ph7_gen_state used purely as storage) and re-initializes the live generator's` |
|         - | 2567 | ` * position containers to FRESH, empty ones WITHOUT releasing the outer's (the` |
|         - | 2568 | ` * snapshot now owns those). hLiteral/hNumLiteral/hVar are intentionally left LIVE` |
|         - | 2569 | ` * and shared — compiled bytecode interns names/literals into them and they grow` |
|         - | 2570 | ` * monotonically (exactly why PH7_ResetCodeGenerator keeps them); restoring an old` |
|         - | 2571 | ` * copy of those headers after a nested grow would use-after-free the bucket array.` |
|         - | 2572 | ` */` |
|         4 | 2573 | `PH7_PRIVATE void PH7_CompilerSaveState(ph7_vm *pVm,ph7_gen_state *pSaved,ProcConsumer xErr,void *pErrData)` |
|         1 | 2574 | `{` |
|         5 | 2575 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 2576 | `	/* Shallow-copy every field; the position containers below are then replaced` |
|         - | 2577 | `	 * with fresh ones on the live struct, so *pSaved keeps the outer's memory. */` |
|         5 | 2578 | `	*pSaved = *pGen;` |
|         5 | 2579 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|         5 | 2580 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|         5 | 2581 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|         5 | 2582 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|         5 | 2583 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|         5 | 2584 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|         5 | 2585 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|         5 | 2586 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|         5 | 2587 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|         5 | 2588 | `	SyHashInit(&pGen->hUseImports,&pVm->sAllocator,0,0);` |
|         5 | 2589 | `	SyHashInit(&pGen->hUseFuncImports,&pVm->sAllocator,0,0);` |
|         5 | 2590 | `	SyHashInit(&pGen->hUseConstImports,&pVm->sAllocator,0,0);` |
|         - | 2591 | `	/* Fresh global scope for the nested unit (address of the embedded sGlobal is` |
|         - | 2592 | `	 * stable, so any outer block still parented to it stays valid across restore). */` |
|         5 | 2593 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(pVm),0);` |
|         5 | 2594 | `	pGen->pCurrent = &pGen->sGlobal;` |
|         5 | 2595 | `	pGen->pIn = pGen->pEnd = 0;` |
|         5 | 2596 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|         5 | 2597 | `	pGen->pTokenSet = 0;` |
|         5 | 2598 | `	pGen->nErr = 0;` |
|         5 | 2599 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|         5 | 2600 | `	pGen->nCommaExprOk = 0;` |
|         5 | 2601 | `	pGen->bInGenerator = 0;` |
|         5 | 2602 | `	pGen->bStrictTypes = 0;` |
|         5 | 2603 | `	pGen->bStrictTypesLocked = 0;` |
|         5 | 2604 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|         5 | 2605 | `	pGen->xErr = xErr;` |
|         5 | 2606 | `	pGen->pErrData = pErrData;` |
|         5 | 2607 | `}` |
|         - | 2608 | `/*` |
|         - | 2609 | ` * Restore the outer compile-position state saved by PH7_CompilerSaveState,` |
|         - | 2610 | ` * releasing the nested unit's position containers first. The shared` |
|         - | 2611 | ` * hLiteral/hNumLiteral/hVar tables (grown by the nested compile) are carried` |
|         - | 2612 | ` * forward, NOT rolled back to the snapshot's stale headers.` |
|         - | 2613 | ` */` |
|         4 | 2614 | `PH7_PRIVATE void PH7_CompilerRestoreState(ph7_vm *pVm,ph7_gen_state *pSaved)` |
|         1 | 2615 | `{` |
|         5 | 2616 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 2617 | `	GenBlock *pBlock,*pParent;` |
|         - | 2618 | `	SyHash hVar,hLiteral,hNumLiteral;` |
|         - | 2619 | `	/* Free any nested blocks left open (e.g. an aborted nested compile), then the` |
|         - | 2620 | `	 * nested global block's own fixup sets. */` |
|         5 | 2621 | `	pBlock = pGen->pCurrent;` |
|         5 | 2622 | `	while( pBlock && pBlock->pParent != 0 ){` |
|       ! 0 | 2623 | `		pParent = pBlock->pParent;` |
|       ! 0 | 2624 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 2625 | `		pBlock = pParent;` |
|       ! 0 | 2626 | `	}` |
|         5 | 2627 | `	GenStateReleaseBlock(&pGen->sGlobal);` |
|         - | 2628 | `	/* Release the nested unit's position containers. */` |
|         5 | 2629 | `	SySetRelease(&pGen->aLabel);` |
|         5 | 2630 | `	SySetRelease(&pGen->aGoto);` |
|         5 | 2631 | `	SySetRelease(&pGen->aNullsafeJmp);` |
|         5 | 2632 | `	SySetRelease(&pGen->aLoopParent);` |
|         5 | 2633 | `	SySetRelease(&pGen->aTrivia);` |
|         5 | 2634 | `	SySetRelease(&pGen->aPendingAttrs);` |
|         5 | 2635 | `	SyBlobRelease(&pGen->sWorker);` |
|         5 | 2636 | `	SyBlobRelease(&pGen->sErrBuf);` |
|         5 | 2637 | `	SyBlobRelease(&pGen->sNamespace);` |
|         5 | 2638 | `	SyHashRelease(&pGen->hUseImports);` |
|         5 | 2639 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|         5 | 2640 | `	SyHashRelease(&pGen->hUseConstImports);` |
|         - | 2641 | `	/* Preserve the (possibly grown) shared intern tables across the restore. */` |
|         5 | 2642 | `	hVar = pGen->hVar;` |
|         5 | 2643 | `	hLiteral = pGen->hLiteral;` |
|         5 | 2644 | `	hNumLiteral = pGen->hNumLiteral;` |
|         5 | 2645 | `	*pGen = *pSaved;` |
|         5 | 2646 | `	pGen->hVar = hVar;` |
|         5 | 2647 | `	pGen->hLiteral = hLiteral;` |
|         5 | 2648 | `	pGen->hNumLiteral = hNumLiteral;` |
|         5 | 2649 | `}` |
|         - | 2650 | `/*` |
|         - | 2651 | ` * Raise php's parse error for an unexpected token: E_PARSE with the exact text` |
|         - | 2652 | ` * php's parser prints, e.g.` |
|         - | 2653 | ` *` |
|         - | 2654 | ` *   syntax error, unexpected token ";", expecting "{"` |
|         - | 2655 | ` *   syntax error, unexpected identifier "invalid", expecting "("` |
|         - | 2656 | ` *   syntax error, unexpected end of file` |
|         - | 2657 | ` *` |
|         - | 2658 | ` * php names the token by CLASS, not just by text: an identifier, a variable and` |
|         - | 2659 | ` * a number each get their own noun, while everything else (keywords, operators,` |
|         - | 2660 | ` * punctuation) is a "token". zExpecting is the optional ", expecting ..." tail` |
|         - | 2661 | ` * — pass NULL when the site cannot say what it wanted (php often can't either).` |
|         - | 2662 | ` * pTok == NULL means the input ran out: "unexpected end of file".` |
|         - | 2663 | ` *` |
|         - | 2664 | ` * PHL's hand-written recursive-descent parser has no bison expectation sets, so` |
|         - | 2665 | ` * a site can only claim an "expecting" clause it genuinely knows; every clause` |
|         - | 2666 | ` * emitted here was verified against php 8.5.7 for the construct in question.` |
|         - | 2667 | ` */` |
|       178 | 2668 | `PH7_PRIVATE sxi32 PH7_GenSyntaxError(` |
|         - | 2669 | `	ph7_gen_state *pGen,   /* Code generator state */` |
|         - | 2670 | `	SyToken *pTok,         /* Offending token, or NULL for end of file */` |
|         - | 2671 | `	const char *zExpecting /* ", expecting <this>" tail, or NULL */` |
|         - | 2672 | `	)` |
|         5 | 2673 | `{` |
|       183 | 2674 | `	const char *zNoun = "token";` |
|         - | 2675 | `	sxu32 nLine;` |
|       183 | 2676 | `	if( pTok == 0 && pGen->pTokenSet ){` |
|         - | 2677 | `		/* The caller ran out of tokens inside its own slice — but a statement's slice stops` |
|         - | 2678 | `		 * BEFORE its terminator, so the token php actually names (typically the ';') is the` |
|         - | 2679 | `		 * one sitting just past the slice, still inside the chunk's token stream. Reach for` |
|         - | 2680 | `		 * it before concluding "end of file". */` |
|        90 | 2681 | `		SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        90 | 2682 | `		SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|        90 | 2683 | `		if( pGen->pEnd >= pBase && pGen->pEnd < pStreamEnd ){` |
|        90 | 2684 | `			pTok = pGen->pEnd;` |
|        43 | 2685 | `		}` |
|        43 | 2686 | `	}` |
|       183 | 2687 | `	nLine = pTok ? pTok->nLine : (pGen->pIn > (SyToken *)SySetBasePtr(pGen->pTokenSet) ? pGen->pIn[-1].nLine : 1);` |
|       183 | 2688 | `	if( pTok == 0 ){` |
|       ! 0 | 2689 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|       ! 0 | 2690 | `			zExpecting ? "syntax error, unexpected end of file, expecting %s"` |
|         - | 2691 | `			           : "syntax error, unexpected end of file",` |
|       ! 0 | 2692 | `			zExpecting);` |
|         - | 2693 | `	}` |
|       183 | 2694 | `	if( pTok->nType & PH7_TK_ID ){` |
|        17 | 2695 | `		zNoun = "identifier";` |
|       176 | 2696 | `	}else if( pTok->nType & PH7_TK_DOLLAR ){` |
|         6 | 2697 | `		zNoun = "variable";` |
|       168 | 2698 | `	}else if( pTok->nType & PH7_TK_INTEGER ){` |
|        24 | 2699 | `		zNoun = "integer";` |
|       156 | 2700 | `	}else if( pTok->nType & PH7_TK_REAL ){` |
|       ! 0 | 2701 | `		zNoun = "float";` |
|       ! 0 | 2702 | `	}` |
|       183 | 2703 | `	if( zExpecting ){` |
|       118 | 2704 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        38 | 2705 | `			"syntax error, unexpected %s \"%z\", expecting %s",zNoun,&pTok->sData,zExpecting);` |
|         - | 2706 | `	}` |
|       158 | 2707 | `	return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        51 | 2708 | `		"syntax error, unexpected %s \"%z\"",zNoun,&pTok->sData);` |
|        94 | 2709 | `}` |
|         - | 2710 | `/*` |
|         - | 2711 | ` * Generate a compile-time error message.` |
|         - | 2712 | ` * If the error count limit is reached (usually 15 error message)` |
|         - | 2713 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|         - | 2714 | ` * abort compilation immediately.` |
|         - | 2715 | ` */` |
|       674 | 2716 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|         5 | 2717 | `{` |
|       679 | 2718 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|       679 | 2719 | `	const char *zErr = "Error";` |
|         - | 2720 | `	SyString *pFile;` |
|         - | 2721 | `	va_list ap;` |
|         - | 2722 | `	sxi32 rc;` |
|         - | 2723 | `	/* Reset the working buffer */` |
|       679 | 2724 | `	SyBlobReset(pWorker);` |
|         - | 2725 | `	/* Peek the processed file path if available */` |
|       679 | 2726 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|       679 | 2727 | `	if( nErrType == E_ERROR \|\| nErrType == E_PARSE ){` |
|         - | 2728 | `		/* Increment the error counter. A PARSE error is every bit as fatal as an` |
|         - | 2729 | `		 * E_ERROR one: php compiles nothing, runs nothing and exits 255. Counting` |
|         - | 2730 | `		 * only E_ERROR let a parse error print its diagnostic and then fall through` |
|         - | 2731 | `		 * into execution with a 0 exit status. */` |
|       669 | 2732 | `		pGen->nErr++;` |
|       669 | 2733 | `		if( pGen->nErr > 15 ){` |
|         - | 2734 | `			/* Error count limit reached */` |
|         6 | 2735 | `			if( pGen->xErr ){` |
|         6 | 2736 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|         6 | 2737 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|         6 | 2738 | `				if( pFile ){` |
|         6 | 2739 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|         2 | 2740 | `				}` |
|         6 | 2741 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|         6 | 2742 | `				if( SyBlobLength(pWorker) > 0 ){` |
|         6 | 2743 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|         2 | 2744 | `				}` |
|         2 | 2745 | `			}` |
|         - | 2746 | `			/* Abort immediately */` |
|         6 | 2747 | `			return SXERR_ABORT;` |
|         - | 2748 | `		}` |
|       330 | 2749 | `	}` |
|       675 | 2750 | `	if( pGen->xErr == 0 ){` |
|         - | 2751 | `		/* No available error consumer,return immediately */` |
|         3 | 2752 | `		return SXRET_OK;` |
|         - | 2753 | `	}` |
|       673 | 2754 | `	switch(nErrType){` |
|       322 | 2755 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|         8 | 2756 | `	case E_WARNING: zErr = "Warning";     break;` |
|       344 | 2757 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|         6 | 2758 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|       ! 0 | 2759 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|       ! 0 | 2760 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|       ! 0 | 2761 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|       ! 0 | 2762 | `	case 8192 /* E_DEPRECATED */: zErr = "Deprecated"; break;` |
|       ! 0 | 2763 | `	default:` |
|       ! 0 | 2764 | `		break;` |
|         - | 2765 | `	}` |
|       673 | 2766 | `	rc = SXRET_OK;` |
|         - | 2767 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|       673 | 2768 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|       673 | 2769 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|       673 | 2770 | `	va_start(ap,zFormat);` |
|       673 | 2771 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|       673 | 2772 | `	va_end(ap);` |
|       673 | 2773 | `	if( pFile ){` |
|       673 | 2774 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       334 | 2775 | `	}` |
|         - | 2776 | `	/* Append a new line */` |
|       673 | 2777 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       673 | 2778 | `	if( SyBlobLength(pWorker) > 0 ){` |
|         - | 2779 | `		/* Consume the generated error message */` |
|       673 | 2780 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       334 | 2781 | `	}` |
|       673 | 2782 | `	return rc;` |
|       342 | 2783 | `}` |
|         - | 2784 |  |
