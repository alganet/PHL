# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1675/1795 lines (93.31%)

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
|         - |   18 | ` * Check if the given name refer to a valid label declared in the given function` |
|         - |   19 | ` * (NULL = file scope).` |
|         - |   20 | ` * Return SXRET_OK and write a pointer to that label on success.` |
|         - |   21 | ` * Any other return value indicates no such label.` |
|         - |   22 | ` *` |
|         - |   23 | ` * Labels are scoped PER FUNCTION in php, so the owning function is part of the key:` |
|         - |   24 | ` * the same name may be declared in as many functions as one likes, and each goto sees` |
|         - |   25 | ` * only its own. Matching on the name alone made the first declaration win everywhere,` |
|         - |   26 | `` * which rejected `function a(){ done: } function b(){ goto done; done: }` — ordinary`` |
|         - |   27 | ` * php — as a jump to an undefined label.` |
|         - |   28 | ` *` |
|         - |   29 | ` * Also serves PH7_CompileLabel, which asks the same question at DECLARATION time to reject` |
|         - |   30 | ` * a name its function already declared.` |
|         - |   31 | ` */` |
|       432 |   32 | `PH7_PRIVATE sxi32 GenStateGetLabel(ph7_gen_state *pGen,SyString *pName,ph7_vm_func *pFunc,Label **ppOut)` |
|         5 |   33 | `{` |
|         - |   34 | `	Label *aLabel;` |
|         - |   35 | `	sxu32 n;` |
|         - |   36 | `	/* Perform a linear scan on the label table */` |
|       437 |   37 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|      1601 |   38 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|      1329 |   39 | `		if( aLabel[n].pFunc == pFunc && SyStringCmp(&aLabel[n].sName,pName,SyMemcmp) == 0 ){` |
|         - |   40 | `			/* Jump destination found */` |
|       165 |   41 | `			if( ppOut ){` |
|       161 |   42 | `				*ppOut = &aLabel[n];` |
|        78 |   43 | `			}` |
|       165 |   44 | `			return SXRET_OK;` |
|         - |   45 | `		}` |
|       587 |   46 | `	}` |
|         - |   47 | `	/* No such destination */` |
|       277 |   48 | `	return SXERR_NOTFOUND;` |
|       221 |   49 | `}` |
|         - |   50 | `/*` |
|         - |   51 | ` * Fetch a block that correspond to the given criteria from the stack of` |
|         - |   52 | ` * compiled blocks.` |
|         - |   53 | ` * Return a pointer to that block on success. NULL otherwise.` |
|         - |   54 | ` */` |
|    213110 |   55 | `PH7_PRIVATE GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|         5 |   56 | `{` |
|    213115 |   57 | `	GenBlock *pBlock = pCurrent;` |
|    430565 |   58 | `	for(;;){` |
|    861135 |   59 | `		if( pBlock->iFlags & iBlockType ){` |
|    213097 |   60 | `			iCount--; /* Decrement nesting level */` |
|    213097 |   61 | `			if( iCount < 1 ){` |
|         - |   62 | `				/* Block meet with the desired criteria */` |
|    213065 |   63 | `				return pBlock;` |
|         - |   64 | `			}` |
|        16 |   65 | `		}` |
|         - |   66 | `		/* Point to the upper block */` |
|    648075 |   67 | `		pBlock = pBlock->pParent;` |
|    648075 |   68 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|         - |   69 | `			/* Forbidden */` |
|        28 |   70 | `			break;` |
|         - |   71 | `		}` |
|         5 |   72 | `	}` |
|         - |   73 | `	/* No such block */` |
|        53 |   74 | `	return 0;` |
|    106560 |   75 | `}` |
|         - |   76 | `/*` |
|         - |   77 | ` * Initialize a freshly allocated block instance.` |
|         - |   78 | ` */` |
|  15096722 |   79 | `static void GenStateInitBlock(` |
|         - |   80 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |   81 | `	GenBlock *pBlock,    /* Target block */` |
|         - |   82 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |   83 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|         - |   84 | `	void *pUserData      /* Upper layer private data */` |
|         - |   85 | `	)` |
|         5 |   86 | `{` |
|         - |   87 | `	/* Initialize block fields */` |
|  15096727 |   88 | `	pBlock->nFirstInstr = nFirstInstr;` |
|  15096727 |   89 | `	pBlock->pUserData   = pUserData;` |
|  15096727 |   90 | `	pBlock->pGen        = pGen;` |
|  15096727 |   91 | `	pBlock->iFlags      = iType;` |
|  15096727 |   92 | `	pBlock->pParent     = 0;` |
|  15096727 |   93 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  15096727 |   94 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|  15096727 |   95 | `}` |
|         - |   96 | `/*` |
|         - |   97 | ` * Allocate a new block instance.` |
|         - |   98 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|         - |   99 | ` * on success.Otherwise generate a compile-time error and abort` |
|         - |  100 | ` * processing on failure.` |
|         - |  101 | ` */` |
|  15092190 |  102 | `PH7_PRIVATE sxi32 GenStateEnterBlock(` |
|         - |  103 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |  104 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |  105 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|         - |  106 | `	void *pUserData,      /* Upper layer private data */` |
|         - |  107 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|         - |  108 | `	)` |
|         5 |  109 | `{` |
|         - |  110 | `	GenBlock *pBlock;` |
|         - |  111 | `	/* Allocate a new block instance */` |
|  15092195 |  112 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|  15092195 |  113 | `	if( pBlock == 0 ){` |
|         - |  114 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |  115 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - |  116 | `		 */` |
|       ! 0 |  117 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|         - |  118 | `		/* Abort processing immediately */` |
|       ! 0 |  119 | `		return SXERR_ABORT;` |
|         - |  120 | `	}` |
|         - |  121 | `	/* Zero the structure */` |
|  15092195 |  122 | `	SyZero(pBlock,sizeof(GenBlock));` |
|  15092195 |  123 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|         - |  124 | `	/* Link to the parent block */` |
|  15092195 |  125 | `	pBlock->pParent = pGen->pCurrent;` |
|         - |  126 | `	/* A loop or switch gets an id, and remembers the loop it nests inside, so a goto's` |
|         - |  127 | `	 * and a label's positions can be compared after compilation (see aLoopParent). */` |
|  15092195 |  128 | `	if( iType & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    621919 |  129 | `		sxu32 nParent = pGen->nCurLoopId;` |
|    621919 |  130 | `		pGen->nLoopId++;` |
|    621919 |  131 | `		SySetPut(&pGen->aLoopParent,(const void *)&nParent);` |
|    621919 |  132 | `		pBlock->nLoopId = pGen->nLoopId;` |
|    621919 |  133 | `		pBlock->nOuterLoopId = nParent;` |
|    621919 |  134 | `		pGen->nCurLoopId = pGen->nLoopId;` |
|    310957 |  135 | `	}` |
|         - |  136 | `	/* A try/catch/finally block gets a scope id, and remembers the scope it nests inside,` |
|         - |  137 | `	 * so the chain between any two points can be walked after compilation (aScope). Every` |
|         - |  138 | `	 * other block simply inherits the scope in effect. */` |
|  15092195 |  139 | `	pBlock->nOuterScopeId = pGen->nCurScopeId;` |
|  15092195 |  140 | `	pBlock->nScopeId = pGen->nCurScopeId;` |
|  15092195 |  141 | `	if( iType & GEN_BLOCK_EXCEPTION ){` |
|         - |  142 | `		GenScope sScope;` |
|     60007 |  143 | `		sScope.nParent = pGen->nCurScopeId;` |
|     60007 |  144 | `		sScope.pUserData = pUserData;` |
|     60007 |  145 | `		if( iType & GEN_BLOCK_FINALLY ){` |
|       295 |  146 | `			sScope.iKind = GEN_SCOPE_FINALLY;` |
|     59862 |  147 | `		}else if( iType & GEN_BLOCK_DETACHED ){` |
|     29695 |  148 | `			sScope.iKind = GEN_SCOPE_DETACHED;` |
|     14850 |  149 | `		}else{` |
|         - |  150 | `			/* A try. pUserData is its ph7_exception, which every try-block site passes at` |
|         - |  151 | `			 * ENTRY precisely so this can classify it. */` |
|     30027 |  152 | `			sScope.iKind = GenStateInlineTryCatch(pGen) ? GEN_SCOPE_TRY_INLINE : GEN_SCOPE_TRY;` |
|         - |  153 | `		}` |
|     60007 |  154 | `		if( SySetPut(&pGen->aScope,(const void *)&sScope) == SXRET_OK ){` |
|     60007 |  155 | `			pBlock->nScopeId = SySetUsed(&pGen->aScope);` |
|     60007 |  156 | `			pGen->nCurScopeId = pBlock->nScopeId;` |
|     30001 |  157 | `		}` |
|     30001 |  158 | `	}` |
|         - |  159 | `	/* Mark as the current block */` |
|  15092195 |  160 | `	pGen->pCurrent = pBlock;` |
|  15092195 |  161 | `	if( ppBlock ){` |
|         - |  162 | `		/* Write a pointer to the new instance */` |
|   7252913 |  163 | `		*ppBlock = pBlock;` |
|   3626454 |  164 | `	}` |
|  15092195 |  165 | `	return SXRET_OK;` |
|   7546100 |  166 | `}` |
|         - |  167 | `/*` |
|         - |  168 | ` * Release block fields without freeing the whole instance.` |
|         - |  169 | ` */` |
|  15092184 |  170 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|         5 |  171 | `{` |
|  15092189 |  172 | `	SySetRelease(&pBlock->aPostContFix);` |
|  15092189 |  173 | `	SySetRelease(&pBlock->aJumpFix);` |
|  15092189 |  174 | `}` |
|         - |  175 | `/*` |
|         - |  176 | ` * Release a block.` |
|         - |  177 | ` */` |
|  15092180 |  178 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|         5 |  179 | `{` |
|  15092185 |  180 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|  15092185 |  181 | `	GenStateReleaseBlock(&(*pBlock));` |
|         - |  182 | `	/* Free the instance */` |
|  15092185 |  183 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|  15092185 |  184 | `}` |
|         - |  185 | `/*` |
|         - |  186 | ` * POP and release a block from the stack of compiled blocks.` |
|         - |  187 | ` */` |
|  15092180 |  188 | `PH7_PRIVATE sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|         5 |  189 | `{` |
|  15092185 |  190 | `	GenBlock *pBlock = pGen->pCurrent;` |
|  15092185 |  191 | `	if( pBlock == 0 ){` |
|         - |  192 | `		/* No more block to pop */` |
|       ! 0 |  193 | `		return SXERR_EMPTY;` |
|         - |  194 | `	}` |
|  15092185 |  195 | `	if( pBlock->iFlags & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    621911 |  196 | `		pGen->nCurLoopId = pBlock->nOuterLoopId;` |
|    310953 |  197 | `	}` |
|  15092185 |  198 | `	if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|     60007 |  199 | `		pGen->nCurScopeId = pBlock->nOuterScopeId;` |
|     30001 |  200 | `	}` |
|         - |  201 | `	/* Point to the upper block */` |
|  15092185 |  202 | `	pGen->pCurrent = pBlock->pParent;` |
|  15092185 |  203 | `	if( ppBlock ){` |
|         - |  204 | `		/* Write a pointer to the popped block */` |
|       ! 0 |  205 | `		*ppBlock = pBlock;` |
|       ! 0 |  206 | `	}else{` |
|         - |  207 | `		/* Safely release the block */` |
|  15092185 |  208 | `		GenStateFreeBlock(&(*pBlock));` |
|         - |  209 | `	}` |
|  15092185 |  210 | `	return SXRET_OK;` |
|   7546095 |  211 | `}` |
|         - |  212 | `/*` |
|         - |  213 | ` * PHP-parity redeclaration guard.` |
|         - |  214 | ` *` |
|         - |  215 | ` * PHP raises a fatal "Cannot redeclare ..." when a class/interface/trait/enum` |
|         - |  216 | ` * or a function is declared a second time. PHL hoists every declaration into` |
|         - |  217 | `` * the VM at compile time (so `if(false){class C{}}` already makes C exist), and`` |
|         - |  218 | ` * historically it silently *overwrote* duplicates. We reproduce PHP for the` |
|         - |  219 | ` * case that matters and that real code hits: a declaration that is` |
|         - |  220 | ` * UNCONDITIONAL and at file top level, whose name is already bound by another` |
|         - |  221 | ` * unconditional top-level declaration (or by a builtin). Conditional` |
|         - |  222 | ` * declarations (inside if/loops/switch/try or nested in a function) are left` |
|         - |  223 | `` * hoisting as before, so the `if(!class_exists('C')){class C{}}` and`` |
|         - |  224 | `` * `if(false){class C{}} class C{}` guard idioms keep working.`` |
|         - |  225 | ` *` |
|         - |  226 | ` * Included files compile at include time (i.e. at run time relative to the main` |
|         - |  227 | ` * script), so this compile-time check surfaces the fatal at the same moment PHP` |
|         - |  228 | ` * does for the cross-include case too.` |
|         - |  229 | ` */` |
|   1204912 |  230 | `PH7_PRIVATE int GenStateUnconditionalTopLevel(ph7_gen_state *pGen)` |
|         5 |  231 | `{` |
|   1204917 |  232 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   1209587 |  233 | `	while( pBlock ){` |
|   1209587 |  234 | `		if( pBlock->iFlags & (GEN_BLOCK_COND\|GEN_BLOCK_LOOP\|GEN_BLOCK_FUNC\|GEN_BLOCK_SWITCH\|GEN_BLOCK_EXCEPTION) ){` |
|        95 |  235 | `			return 0; /* conditional / nested */` |
|         - |  236 | `		}` |
|   1209497 |  237 | `		if( pBlock->iFlags & GEN_BLOCK_GLOBAL ){` |
|   1204827 |  238 | `			return 1; /* reached the global block with no conditional ancestor */` |
|         - |  239 | `		}` |
|      4675 |  240 | `		pBlock = pBlock->pParent;` |
|         5 |  241 | `	}` |
|       ! 0 |  242 | `	return 1;` |
|    602461 |  243 | `}` |
|         - |  244 | `/*` |
|         - |  245 | ` * Guard a top-level function about to be installed. Same contract as the class` |
|         - |  246 | ` * guard above.` |
|         - |  247 | ` */` |
|    595144 |  248 | `PH7_PRIVATE sxi32 GenStateGuardFuncRedeclaration(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|         5 |  249 | `{` |
|         - |  250 | `	SyHashEntry *pEntry;` |
|    595149 |  251 | `	if( !GenStateUnconditionalTopLevel(pGen) ){` |
|        56 |  252 | `		return SXRET_OK;` |
|         - |  253 | `	}` |
|    595097 |  254 | `	pFunc->iFlags \|= VM_FUNC_BOUND;` |
|    595097 |  255 | `	if( pGen->pVm->bCompilingBuiltin ){` |
|    593173 |  256 | `		return SXRET_OK;` |
|         - |  257 | `	}` |
|         - |  258 | ``	/* NOTE: a userland function shadowing a C builtin (e.g. `function strlen(){}`)`` |
|         - |  259 | `	 * is NOT caught here — the C builtins register in PH7_VmMakeReady, after user` |
|         - |  260 | `	 * code has compiled, so hHostFunction is still empty at this point. Prelude` |
|         - |  261 | `	 * functions (ini_get, ...) and every builtin CLASS compile earlier and ARE` |
|         - |  262 | `	 * guarded. Redeclaring a C builtin function stays a known divergence. */` |
|      1929 |  263 | `	pEntry = SyHashGet(&pGen->pVm->hFunction,(const void *)pFunc->sName.zString,pFunc->sName.nByte);` |
|      1929 |  264 | `	if( pEntry ){` |
|        12 |  265 | `		ph7_vm_func *pPrev = (ph7_vm_func *)pEntry->pUserData;` |
|        14 |  266 | `		while( pPrev ){` |
|        12 |  267 | `			if( pPrev->iFlags & VM_FUNC_BOUND ){` |
|         9 |  268 | `				if( pPrev->sFile.nByte > 0 ){` |
|         8 |  269 | `					PH7_GenCompileError(pGen,E_ERROR,pFunc->nLine,` |
|         - |  270 | `						"Cannot redeclare function %z() (previously declared in %.*s:%u)",` |
|         2 |  271 | `						&pFunc->sName,pPrev->sFile.nByte,pPrev->sFile.zString,pPrev->nLine);` |
|         4 |  272 | `				}else{` |
|         4 |  273 | `					PH7_GenCompileError(pGen,E_ERROR,pFunc->nLine,` |
|         1 |  274 | `						"Cannot redeclare function %z()",&pFunc->sName);` |
|         - |  275 | `				}` |
|         9 |  276 | `				return SXERR_ABORT;` |
|         - |  277 | `			}` |
|         3 |  278 | `			pPrev = pPrev->pNextName;` |
|         1 |  279 | `		}` |
|         1 |  280 | `	}` |
|      1923 |  281 | `	return SXRET_OK;` |
|    297577 |  282 | `}` |
|         - |  283 | `/*` |
|         - |  284 | ` * Emit a forward jump.` |
|         - |  285 | ` * Notes on forward jumps` |
|         - |  286 | ` *  Compilation of some PHP constructs such as if,for,while and the logical or` |
|         - |  287 | ` *  (\|\|) and logical and (&&) operators in expressions requires the` |
|         - |  288 | ` *  generation of forward jumps.` |
|         - |  289 | ` *  Since the destination PC target of these jumps isn't known when the jumps` |
|         - |  290 | ` *  are emitted, we record each forward jump in an instance of the following` |
|         - |  291 | ` *  structure. Those jumps are fixed later when the jump destination is resolved.` |
|         - |  292 | ` */` |
|   5546232 |  293 | `PH7_PRIVATE sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|         5 |  294 | `{` |
|         - |  295 | `	JumpFixup sJumpFix;` |
|         - |  296 | `	sxi32 rc;` |
|         - |  297 | `	/* Init the JumpFixup structure */` |
|   5546237 |  298 | `	sJumpFix.nJumpType = nJumpType;` |
|   5546237 |  299 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|         - |  300 | `	/* Remember which bytecode array the emitted instruction lives in: the block whose` |
|         - |  301 | `	 * table this lands in may be resolved after a container swap (see JumpFixup). */` |
|   5546237 |  302 | `	sJumpFix.pContainer = PH7_VmGetByteCodeContainer(pBlock->pGen->pVm);` |
|         - |  303 | `	/* Insert in the jump fixup table */` |
|   5546237 |  304 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|   5546237 |  305 | `	return rc;` |
|         5 |  306 | `}` |
|         - |  307 | `/*` |
|         - |  308 | ` * TRUE when the body being compiled has its try/catch/finally compiled INLINE` |
|         - |  309 | ` * (ROOT C, generator bodies) rather than into detached mini-programs.` |
|         - |  310 | ` */` |
|     30022 |  311 | `PH7_PRIVATE int GenStateInlineTryCatch(ph7_gen_state *pGen)` |
|         5 |  312 | `{` |
|     30027 |  313 | `	return pGen->bInGenerator && pGen->pVm->bInlineTryCatch;` |
|         5 |  314 | `}` |
|         - |  315 | `/*` |
|         - |  316 | ` * Walk the scope chain from nFrom (where a jump is) out to nTo (where it lands) and` |
|         - |  317 | `` * describe what it crosses. One walk serves `break`, `continue` and `goto` alike,`` |
|         - |  318 | ` * because they all ask the same question of the same chain — only the two endpoints` |
|         - |  319 | ` * differ, and for a goto they are not both known until compilation ends.` |
|         - |  320 | ` *` |
|         - |  321 | ` * Returns TRUE when nTo was actually reached, i.e. the target's scope ENCLOSES the` |
|         - |  322 | ` * jump. FALSE means the target sits inside a try/catch the jump is not in — jumping` |
|         - |  323 | ` * into one, which PHL cannot express (its handler is pushed by the try's` |
|         - |  324 | ` * OP_LOAD_EXCEPTION, and a catch body is a mini-program entered at instruction 0).` |
|         - |  325 | ` * Depth counting cannot answer this: two sibling trys have the same depth.` |
|         - |  326 | ` *` |
|         - |  327 | ` * What is counted, for the opcode the caller then picks:` |
|         - |  328 | ` *  nDet    — DETACHED catch/finally bodies left. Each is its own bytecode array, so a` |
|         - |  329 | ` *            jump out of one cannot be a plain OP_JMP: it parks and travels out through` |
|         - |  330 | ` *            one OP_POP_EXCEPTION landing pad per boundary (OP_CATCH_JMP);` |
|         - |  331 | ` *  nTry    — legacy trys left whose OP_POP_EXCEPTION the jump SKIPS, so nothing else` |
|         - |  332 | ` *            would run their finally. Trys BELOW the first boundary do not qualify: a` |
|         - |  333 | ` *            break/continue emits their OP_POP_EXCEPTION right here (bEmitPops), and a` |
|         - |  334 | ` *            goto drains them where it parks — hence the reset when one is reached;` |
|         - |  335 | ` *  nInline — ROOT C inline trys left. Their finallys are driven by VmFinallyAdvance,` |
|         - |  336 | ` *            not by the aException drain, so they are crossed with OP_SET_FINALLY_JMP;` |
|         - |  337 | `` *  nFinally — `finally` bodies left, which php forbids outright. When this is non-zero the`` |
|         - |  338 | ` *            three above are NOT computed: callers must test it first and reject.` |
|         - |  339 | ` */` |
|    213214 |  340 | `PH7_PRIVATE int GenStateJumpScope(ph7_gen_state *pGen,sxu32 nFrom,sxu32 nTo,int bEmitPops,` |
|         - |  341 | `	GenJumpScope *pScope)` |
|         5 |  342 | `{` |
|    213219 |  343 | `	GenScope *aScope = (GenScope *)SySetBasePtr(&pGen->aScope);` |
|    213219 |  344 | `	sxu32 nUsed = SySetUsed(&pGen->aScope);` |
|    213219 |  345 | `	sxu32 nCur = nFrom;` |
|    213219 |  346 | `	SyZero(pScope,sizeof(*pScope));` |
|    213333 |  347 | `	while( nCur != nTo ){` |
|         - |  348 | `		GenScope *pScopeEnt;` |
|       123 |  349 | `		if( nCur == 0 \|\| nCur > nUsed ){` |
|         6 |  350 | `			return FALSE; /* ran off the top without meeting nTo */` |
|         - |  351 | `		}` |
|       119 |  352 | `		pScopeEnt = &aScope[nCur - 1];` |
|       119 |  353 | `		if( pScopeEnt->iKind == GEN_SCOPE_FINALLY ){` |
|         - |  354 | ``			/* php: `jump out of a finally block is disallowed`. Counted rather than`` |
|         - |  355 | `			 * rejected here because the caller owns the diagnostic and its line — but` |
|         - |  356 | `			 * ONLY counted: the jump is illegal, so the other three fields are left as` |
|         - |  357 | `			 * they are rather than pretending to describe a crossing that will never be` |
|         - |  358 | `			 * emitted. (They could not be right anyway: this kind covers both the legacy` |
|         - |  359 | `			 * detached finally and the generator's INLINE one, which is not a separate` |
|         - |  360 | `			 * bytecode container.) Every caller tests nFinally first. A jump that stays` |
|         - |  361 | `			 * INSIDE the finally never reaches this scope, so it stays legal. */` |
|        14 |  362 | `			pScope->nFinally++;` |
|       114 |  363 | `		}else if( pScopeEnt->iKind == GEN_SCOPE_DETACHED ){` |
|        74 |  364 | `			if( pScope->nDet == 0 ){` |
|        70 |  365 | `				pScope->nTry = 0;    /* below the first boundary: not the landing pad's */` |
|        70 |  366 | `				pScope->nInline = 0;` |
|        33 |  367 | `			}` |
|        74 |  368 | `			pScope->nDet++;` |
|        74 |  369 | `		}else if( pScopeEnt->iKind == GEN_SCOPE_TRY_INLINE ){` |
|        11 |  370 | `			pScope->nInline++;` |
|        35 |  371 | `		}else if( pScope->nDet == 0 && bEmitPops ){` |
|         3 |  372 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pScopeEnt->pUserData,0);` |
|         2 |  373 | `		}else{` |
|        28 |  374 | `			pScope->nTry++;` |
|         - |  375 | `		}` |
|       119 |  376 | `		nCur = pScopeEnt->nParent;` |
|         5 |  377 | `	}` |
|    213215 |  378 | `	return TRUE;` |
|    106612 |  379 | `}` |
|         - |  380 | `/*` |
|         - |  381 | ` * Pick the jump opcode for a crossing described by GenStateJumpScope, and its iP1.` |
|         - |  382 | ` * Shared by break/continue (which know their target at emit time) and goto (which` |
|         - |  383 | ` * settles this in GenStateFixGoto, once the label fixes the counts).` |
|         - |  384 | ` */` |
|    213102 |  385 | `PH7_PRIVATE sxi32 GenStateScopeJumpOp(const GenJumpScope *pCross,sxi32 *piP1)` |
|         5 |  386 | `{` |
|    213107 |  387 | `	if( pCross->nDet > 0 \|\| pCross->nTry > 0 ){` |
|        84 |  388 | `		*piP1 = PH7_CATCH_JMP_P1(pCross->nDet,pCross->nTry);` |
|        84 |  389 | `		return PH7_OP_CATCH_JMP;` |
|         - |  390 | `	}` |
|    213027 |  391 | `	if( pCross->nInline > 0 ){` |
|        11 |  392 | `		*piP1 = (sxi32)pCross->nInline;` |
|        11 |  393 | `		return PH7_OP_SET_FINALLY_JMP;` |
|         - |  394 | `	}` |
|    213019 |  395 | `	*piP1 = 0;` |
|    213019 |  396 | `	return PH7_OP_JMP;` |
|    106556 |  397 | `}` |
|         - |  398 | `/*` |
|         - |  399 | ` * Resolve a recorded fixup to its VM instruction, in the container it was emitted` |
|         - |  400 | ` * into (see JumpFixup.pContainer) rather than whichever one is current now.` |
|         - |  401 | ` */` |
|   5578094 |  402 | `PH7_PRIVATE VmInstr * GenStateFixupInstr(const JumpFixup *pFix)` |
|         5 |  403 | `{` |
|   5578099 |  404 | `	return (VmInstr *)SySetAt(pFix->pContainer,pFix->nInstrIdx);` |
|         5 |  405 | `}` |
|         - |  406 | `/*` |
|         - |  407 | ` * Fix a forward jump now the jump destination is resolved.` |
|         - |  408 | ` * Return the total number of fixed jumps.` |
|         - |  409 | ` * Notes on forward jumps:` |
|         - |  410 | ` *  Compilation of some PHP constructs such as if,for,while and the logical or` |
|         - |  411 | ` *  (\|\|) and logical and (&&) operators in expressions requires the` |
|         - |  412 | ` *  generation of forward jumps.` |
|         - |  413 | ` *  Since the destination PC target of these jumps isn't known when the jumps` |
|         - |  414 | ` *  are emitted, we record each forward jump in an instance of the following` |
|         - |  415 | ` *  structure.Those jumps are fixed later when the jump destination is resolved.` |
|         - |  416 | ` */` |
|  10543570 |  417 | `PH7_PRIVATE sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|         5 |  418 | `{` |
|         - |  419 | `	JumpFixup *aFix;` |
|         - |  420 | `	VmInstr *pInstr;` |
|         - |  421 | `	sxu32 nFixed;` |
|         - |  422 | `	sxu32 n;` |
|         - |  423 | `	/* Point to the jump fixup table */` |
|  10543575 |  424 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|         - |  425 | `	/* Fix the desired jumps */` |
|  22196051 |  426 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|  11652481 |  427 | `		if( aFix[n].nJumpType < 0 ){` |
|         - |  428 | `			/* Already fixed */` |
|   4348483 |  429 | `			continue;` |
|         - |  430 | `		}` |
|   7304003 |  431 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|         - |  432 | `			/* Not of our interest */` |
|   1757773 |  433 | `			continue;` |
|         - |  434 | `		}` |
|         - |  435 | `		/* Point to the instruction to fix */` |
|   5546235 |  436 | `		pInstr = GenStateFixupInstr(&aFix[n]);` |
|   5546235 |  437 | `		if( pInstr ){` |
|   5546235 |  438 | `			pInstr->iP2 = nJumpDest;` |
|   5546235 |  439 | `			nFixed++;` |
|         - |  440 | `			/* Mark as fixed */` |
|   5546235 |  441 | `			aFix[n].nJumpType = -1;` |
|   2773115 |  442 | `		}` |
|   2773120 |  443 | `	}` |
|         - |  444 | `	/* Total number of fixed jumps */` |
|  10543575 |  445 | `	return nFixed;` |
|         5 |  446 | `}` |
|         - |  447 | `/*` |
|         - |  448 | ` * Fix a 'goto' now the jump destination is resolved.` |
|         - |  449 | ` * The goto statement can be used to jump to another section` |
|         - |  450 | ` * in the program.` |
|         - |  451 | ` * Refer to the routine responsible of compiling the goto` |
|         - |  452 | ` * statement for more information.` |
|         - |  453 | ` */` |
|   3820170 |  454 | `PH7_PRIVATE sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|         5 |  455 | `{` |
|         - |  456 | `	JumpFixup *pJump,*aJumps;` |
|         - |  457 | `	GenJumpScope sCross;` |
|         - |  458 | `	Label *pLabel;` |
|         - |  459 | `	VmInstr *pInstr;` |
|         - |  460 | `	sxi32 rc;` |
|         - |  461 | `	sxu32 n;` |
|         - |  462 | `	/* Point to the goto table */` |
|   3820175 |  463 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|         - |  464 | `	/* Fix */` |
|   3820393 |  465 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
|       225 |  466 | `		pJump = &aJumps[n];` |
|         - |  467 | `		/* Extract the target label */` |
|         - |  468 | `		/* A label declared in ANOTHER function is not a destination: the lookup is keyed` |
|         - |  469 | `		 * on the goto's own function, so a same-named label elsewhere simply does not` |
|         - |  470 | `		 * answer and this reports php's undefined-label fatal. */` |
|       225 |  471 | `		rc = GenStateGetLabel(&(*pGen),&pJump->sLabel,pJump->pFunc,&pLabel);` |
|       225 |  472 | `		if( rc != SXRET_OK ){` |
|         - |  473 | `			/* No such label */` |
|        68 |  474 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"'goto' to undefined label '%z'",&pJump->sLabel);` |
|        68 |  475 | `			if( rc == SXERR_ABORT ){` |
|         3 |  476 | `				return SXERR_ABORT;` |
|         - |  477 | `			}` |
|        66 |  478 | `			continue;` |
|         - |  479 | `		}` |
|         - |  480 | `		/* php's one goto restriction: you may not jump INTO a loop or a switch. The label` |
|         - |  481 | `		 * is inside one exactly when it carries a loop id; that is legal only if the same` |
|         - |  482 | `		 * loop also encloses the goto, i.e. the label's loop is the goto's loop or one of` |
|         - |  483 | `		 * its ancestors. Walk up from the goto's loop looking for the label's. */` |
|       161 |  484 | `		if( pLabel->nLoopId != 0 ){` |
|         5 |  485 | `			sxu32 *aParent = (sxu32 *)SySetBasePtr(&pGen->aLoopParent);` |
|         5 |  486 | `			sxu32 nCur = pJump->nLoopId;` |
|         5 |  487 | `			int bInside = 0;` |
|         5 |  488 | `			while( nCur != 0 ){` |
|         5 |  489 | `				if( nCur == pLabel->nLoopId ){` |
|         5 |  490 | `					bInside = 1;` |
|         5 |  491 | `					break;` |
|         - |  492 | `				}` |
|       ! 0 |  493 | `				nCur = (nCur <= SySetUsed(&pGen->aLoopParent)) ? aParent[nCur - 1] : 0;` |
|       ! 0 |  494 | `			}` |
|         5 |  495 | `			if( !bInside ){` |
|       ! 0 |  496 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,` |
|         - |  497 | `					"'goto' into loop or switch statement is disallowed");` |
|       ! 0 |  498 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  499 | `					return SXERR_ABORT;` |
|         - |  500 | `				}` |
|       ! 0 |  501 | `				continue;` |
|         - |  502 | `			}` |
|         2 |  503 | `		}` |
|         - |  504 | `		/* What the jump crosses, and whether it is legal at all: the label's scope must` |
|         - |  505 | `		 * ENCLOSE the goto. Jumping INTO a try/catch/finally is fine in php (its handlers` |
|         - |  506 | `		 * are instruction RANGES, so landing anywhere in the body is being in the try),` |
|         - |  507 | `		 * but PHL pushes a handler at the try's OP_LOAD_EXCEPTION and runs a catch body` |
|         - |  508 | `		 * as a mini-program entered at its first instruction — there is no way to arrive` |
|         - |  509 | `		 * mid-body with the handler live. Say so rather than jump nowhere in silence,` |
|         - |  510 | `		 * skip a finally, or land in a foreign array. */` |
|       161 |  511 | `		if( !GenStateJumpScope(&(*pGen),pJump->nScopeId,pLabel->nScopeId,FALSE,&sCross) ){` |
|         6 |  512 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,` |
|         - |  513 | `				"'goto' into a try, catch or finally block is disallowed");` |
|         6 |  514 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  515 | `				return SXERR_ABORT;` |
|         - |  516 | `			}` |
|         6 |  517 | `			continue;` |
|         - |  518 | `		}` |
|       157 |  519 | `		if( sCross.nFinally > 0 ){` |
|         - |  520 | `			/* php's other structural rule, shared with break/continue. Tested AFTER the` |
|         - |  521 | `			 * reach test above, so a goto that both leaves a finally and lands somewhere` |
|         - |  522 | `			 * that does not enclose it reports the into-a-try wording instead of this` |
|         - |  523 | `			 * one. Both are fatal on the same line, and the two cannot be told apart` |
|         - |  524 | `			 * without a second walk outward from the LABEL — php accepts one of them` |
|         - |  525 | ``			 * (`finally { goto L; try { L: … } }`), which is the §7.2 divergence, and`` |
|         - |  526 | `			 * rejects the other. Not worth a second walk for a message on input that is` |
|         - |  527 | `			 * rejected either way. */` |
|         3 |  528 | `			if( GenStateJumpOutOfFinally(&(*pGen),pJump->nLine) == SXERR_ABORT ){` |
|       ! 0 |  529 | `				return SXERR_ABORT;` |
|         - |  530 | `			}` |
|         3 |  531 | `			continue;` |
|         - |  532 | `		}` |
|         - |  533 | `		/* Fix the jump now the destination is resolved — in the container the goto was` |
|         - |  534 | `		 * emitted into, which for a goto inside a catch/finally body is not the one` |
|         - |  535 | `		 * current here (gotos resolve at end of compilation, after every swap back). */` |
|       155 |  536 | `		pInstr = GenStateFixupInstr(pJump);` |
|       155 |  537 | `		if( pInstr ){` |
|       155 |  538 | `			pInstr->iP2 = pLabel->nJumpDest;` |
|       155 |  539 | `			if( pInstr->iOp == PH7_OP_CATCH_JMP ){` |
|         - |  540 | `				/* Emitted as a structure-crossing jump because the goto sits inside a` |
|         - |  541 | `				 * try or a detached body. Now that the crossing is known it may well` |
|         - |  542 | `				 * turn out to leave nothing, and downgrade to a plain OP_JMP. */` |
|        48 |  543 | `				sxi32 iP1 = 0;` |
|        48 |  544 | `				pInstr->iOp = (sxu8)GenStateScopeJumpOp(&sCross,&iP1);` |
|        48 |  545 | `				pInstr->iP1 = iP1;` |
|        22 |  546 | `			}` |
|        75 |  547 | `		}` |
|        80 |  548 | `	}` |
|         - |  549 | `	/* php says nothing about a label nobody jumps to — the old "defined but not` |
|         - |  550 | `	 * referenced" warning was a PH7-ism with no counterpart in the oracle. */` |
|   3820173 |  551 | `	return SXRET_OK;` |
|   1910090 |  552 | `}` |
|         - |  553 | `/*` |
|         - |  554 | ` * Check if a given token value is installed in the literal table.` |
|         - |  555 | ` */` |
|  19837430 |  556 | `PH7_PRIVATE sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|         5 |  557 | `{` |
|         - |  558 | `	SyHashEntry *pEntry;` |
|  19837435 |  559 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|  19837435 |  560 | `	if( pEntry == 0 ){` |
|   5228343 |  561 | `		return SXERR_NOTFOUND;` |
|         - |  562 | `	}` |
|  14609097 |  563 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  14609097 |  564 | `	return SXRET_OK;` |
|   9918720 |  565 | `}` |
|         - |  566 | `/*` |
|         - |  567 | ` * Install a given constant index in the literal table.` |
|         - |  568 | ` * In order to be installed, the ph7_value must be of type string.` |
|         - |  569 | ` *` |
|         - |  570 | ` * NOTE: empty strings are deliberately omitted here.  The VM reserves a` |
|         - |  571 | ` * single shared constant for "" during initialization (pVm->nEmptyStringIdx)` |
|         - |  572 | ` * and the compiler emits a LOADC referencing that slot whenever an empty` |
|         - |  573 | ` * literal is encountered.  This keeps the literal hash from growing when` |
|         - |  574 | ` * many "" literals appear in user code.` |
|         - |  575 | ` */` |
|   5228338 |  576 | `PH7_PRIVATE sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|         5 |  577 | `{` |
|   5228343 |  578 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|   5228343 |  579 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|   2614169 |  580 | `	}` |
|   5228343 |  581 | `	return SXRET_OK;` |
|         5 |  582 | `}` |
|         - |  583 | `/*` |
|         - |  584 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|         - |  585 | ` * in the constant table.` |
|         - |  586 | ` */` |
|   4481346 |  587 | `PH7_PRIVATE ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|         5 |  588 | `{` |
|         - |  589 | `	ph7_value *pObj;` |
|   4481351 |  590 | `	sxu32 nIdx = 0; /* cc warning */` |
|         - |  591 | `	/* Reserve a new constant */` |
|   4481351 |  592 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|   4481351 |  593 | `	if( pObj == 0 ){` |
|       ! 0 |  594 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  595 | `		return 0;` |
|         - |  596 | `	}` |
|   4481351 |  597 | `	*pIdx = nIdx;` |
|         - |  598 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|         - |  599 | `	 * the constant string iterals table [optimization purposes].` |
|         - |  600 | `	 */` |
|   4481351 |  601 | `	return pObj;` |
|   2240678 |  602 | `}` |
|         - |  603 | `/*` |
|         - |  604 | ` * Implementation of the PHP language constructs.` |
|         - |  605 | ` */` |
|         - |  606 | `/*` |
|         - |  607 | ` * Ensure the about-to-be-emitted CALL/NEW opcode carries a VmCallArgMap` |
|         - |  608 | ` * that reflects the caller file's strict_types mode. Returns the (possibly` |
|         - |  609 | ` * newly allocated and zero-initialized) map pointer. In weak-mode files` |
|         - |  610 | ` * this is a no-op and the caller's p3 is returned unchanged.` |
|         - |  611 | ` *` |
|         - |  612 | ` * NOTE: on allocation failure the call reverts to weak semantics rather` |
|         - |  613 | ` * than aborting compilation — out-of-memory during a map allocation is` |
|         - |  614 | ` * vanishingly unlikely and silently dropping to weak mode matches the` |
|         - |  615 | ` * surrounding callsites' zero-check fallback pattern.` |
|         - |  616 | ` */` |
|   9311766 |  617 | `PH7_PRIVATE void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|         5 |  618 | `{` |
|         - |  619 | `	VmCallArgMap *pMap;` |
|   9311771 |  620 | `	if( !pGen->bStrictTypes ) return p3;` |
|        90 |  621 | `	if( p3 == 0 ){` |
|        82 |  622 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|        82 |  623 | `		if( pMap == 0 ) return 0;` |
|        82 |  624 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|        82 |  625 | `		p3 = (void *)pMap;` |
|        39 |  626 | `	}` |
|        90 |  627 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|        90 |  628 | `	return p3;` |
|   4655888 |  629 | `}` |
|         - |  630 | `/* Forward declaration */` |
|         - |  631 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut);` |
|         - |  632 | `/* Forward declarations */` |
|         - |  633 | `/*` |
|         - |  634 | ` * Recover from a compile-time error. In other words synchronize` |
|         - |  635 | ` * the token stream cursor with the first semi-colon seen.` |
|         - |  636 | ` */` |
|         8 |  637 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|         1 |  638 | `{` |
|         - |  639 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|        17 |  640 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|         9 |  641 | `		pGen->pIn++;` |
|         1 |  642 | `	}` |
|         9 |  643 | `	return SXRET_OK;` |
|         1 |  644 | `}` |
|         - |  645 | `/*` |
|         - |  646 | ` * Check if the given identifier name is reserved or not.` |
|         - |  647 | ` * Return TRUE if reserved.FALSE otherwise.` |
|         - |  648 | ` */` |
|       136 |  649 | `PH7_PRIVATE int GenStateIsReservedConstant(SyString *pName)` |
|         5 |  650 | `{` |
|       141 |  651 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|        14 |  652 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|         3 |  653 | `			return TRUE;` |
|        12 |  654 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|         6 |  655 | `			return TRUE;` |
|         2 |  656 | `		}` |
|       133 |  657 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|        13 |  658 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|         3 |  659 | `			return TRUE;` |
|         - |  660 | `		}` |
|         4 |  661 | `	}` |
|         - |  662 | `	/* Not a reserved constant */` |
|       133 |  663 | `	return FALSE;` |
|        73 |  664 | `}` |
|         - |  665 | `/*` |
|         - |  666 | ` * Chain operators participate in a postfix member-access chain.` |
|         - |  667 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|         - |  668 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|         - |  669 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|         - |  670 | ` */` |
|         - |  671 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|         - |  672 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|         - |  673 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|         - |  674 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|         - |  675 |  |
|         - |  676 | `/*` |
|         - |  677 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|         - |  678 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|         - |  679 | ` * patched entries from the pending set.` |
|         - |  680 | ` */` |
|  63660586 |  681 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|         5 |  682 | `{` |
|  63660591 |  683 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|         - |  684 | `	sxu32 nTarget;` |
|         - |  685 | `	sxu32 *aIdx;` |
|         - |  686 | `	sxu32 i;` |
|  63660591 |  687 | `	if( nCur <= nBaseline ){` |
|  63660489 |  688 | `		return;` |
|         - |  689 | `	}` |
|       105 |  690 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|       105 |  691 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|       215 |  692 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|       113 |  693 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|       113 |  694 | `		if( pInstr ){` |
|       113 |  695 | `			pInstr->iP2 = (sxi32)nTarget;` |
|        55 |  696 | `		}` |
|        58 |  697 | `	}` |
|       105 |  698 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
|  31830298 |  699 | `}` |
|         - |  700 |  |
|         - |  701 | `/*` |
|         - |  702 | ` * By-reference out-parameters of builtin functions.` |
|         - |  703 | ` *` |
|         - |  704 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|         - |  705 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|         - |  706 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|         - |  707 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|         - |  708 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|         - |  709 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|         - |  710 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|         - |  711 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|         - |  712 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|         - |  713 | ` * creates it" behaviour).` |
|         - |  714 | ` *` |
|         - |  715 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|         - |  716 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|         - |  717 | ` */` |
|   8260614 |  718 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|         5 |  719 | `{` |
|         - |  720 | `	static const struct {` |
|         - |  721 | `		const char *zName;` |
|         - |  722 | `		sxu32 nByte;` |
|         - |  723 | `		sxu32 mask;` |
|         - |  724 | `	} aByRef[] = {` |
|         - |  725 | `		{ "parse_str",              9, 1u<<1 },  /* &$result (apArg[1]) */` |
|         - |  726 | `		{ "settype",                7, 1u<<0 },  /* &$var    (apArg[0]) */` |
|         - |  727 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|         - |  728 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|         - |  729 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|         - |  730 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|         - |  731 | `		{ "flock",                  5, 1u<<2 },  /* &$would_block (apArg[2]) */` |
|         - |  732 | `		{ "getopt",                 6, 1u<<2 },  /* &$rest_index (apArg[2]) */` |
|         - |  733 | `		{ "is_callable",           11, 1u<<2 },  /* &$callable_name (apArg[2]) */` |
|         - |  734 | `		{ "similar_text",          12, 1u<<2 },  /* &$percent (apArg[2]) */` |
|         - |  735 | `		{ "str_replace",           11, 1u<<3 },  /* &$count  (apArg[3]) */` |
|         - |  736 | `		{ "str_ireplace",          12, 1u<<3 },  /* &$count  (apArg[3]) */` |
|         - |  737 | `		{ "fsockopen",              9, (1u<<2)\|(1u<<3) },  /* &$error_code, &$error_message */` |
|         - |  738 | `		{ "pfsockopen",            10, (1u<<2)\|(1u<<3) },  /* same */` |
|         - |  739 | `		{ "stream_socket_client",  20, (1u<<1)\|(1u<<2) },  /* &$error_code, &$error_message */` |
|         - |  740 | `		{ "proc_open",              9, 1u<<2 },  /* &$pipes (apArg[2]) */` |
|         - |  741 | `	};` |
|         - |  742 | `	sxu32 i;` |
|   8260619 |  743 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|   2112951 |  744 | `		return 0;` |
|         - |  745 | `	}` |
| 103575697 |  746 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
|  97501130 |  747 | `		if( pName->nByte == aByRef[i].nByte` |
|  51957671 |  748 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|     73111 |  749 | `			return aByRef[i].mask;` |
|         - |  750 | `		}` |
|  48714017 |  751 | `	}` |
|   6074567 |  752 | `	return 0;` |
|   4130312 |  753 | `}` |
|         - |  754 | `/*` |
|         - |  755 | ` * Recover the bare global-builtin name from a call's callee node.` |
|         - |  756 | ` *` |
|         - |  757 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|         - |  758 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|         - |  759 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|         - |  760 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|         - |  761 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|         - |  762 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|         - |  763 | ` */` |
|  14745756 |  764 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|         5 |  765 | `{` |
|         - |  766 | `	SyToken *p, *pEnd;` |
|  14745761 |  767 | `	pOut->zString = 0;` |
|  14745761 |  768 | `	pOut->nByte = 0;` |
|  14745761 |  769 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|       ! 0 |  770 | `		return;` |
|         - |  771 | `	}` |
|  14745761 |  772 | `	p = pLeft->pStart;` |
|  14745761 |  773 | `	pEnd = pLeft->pEnd;` |
|         - |  774 | `	/* Optional single leading namespace separator (absolute path). */` |
|  14745761 |  775 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|      9129 |  776 | `		p++;` |
|      4562 |  777 | `	}` |
|  14745761 |  778 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|   2631151 |  779 | `		return;` |
|         - |  780 | `	}` |
|         - |  781 | `	/* Must be a single component: nothing follows the name token. */` |
|  12114615 |  782 | `	if( p + 1 != pEnd ){` |
|       107 |  783 | `		return;` |
|         - |  784 | `	}` |
|  12114513 |  785 | `	*pOut = p->sData;` |
|   7372883 |  786 | `}` |
|         - |  787 | `/*` |
|         - |  788 | ` * Generate bytecode for a given expression tree.` |
|         - |  789 | ` * If something goes wrong while generating bytecode` |
|         - |  790 | ` * for the expression tree (A very unlikely scenario)` |
|         - |  791 | ` * this function takes care of generating the appropriate` |
|         - |  792 | ` * error message.` |
|         - |  793 | ` */` |
|  89537562 |  794 | `static sxi32 GenStateEmitExprCode(` |
|         - |  795 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |  796 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|         - |  797 | `	sxi32 iFlags /* Control flags */` |
|         - |  798 | `	)` |
|         5 |  799 | `{` |
|         - |  800 | `	VmInstr *pInstr;` |
|         - |  801 | `	sxu32 nJmpIdx;` |
|  89537567 |  802 | `	sxi32 iP1 = 0;` |
|  89537567 |  803 | `	sxu32 iP2 = 0;` |
|  89537567 |  804 | `	void *p3  = 0;` |
|         - |  805 | `	sxi32 iVmOp;` |
|         - |  806 | `	sxi32 rc;` |
|  89537567 |  807 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
|  89537567 |  808 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
|  89537567 |  809 | `	sxu32 nRhsNsBase = 0;` |
|  89537567 |  810 | `	if( pNode->xCode ){` |
|         - |  811 | `		SyToken *pTmpIn,*pTmpEnd;` |
|         - |  812 | `		/* Compile node */` |
|  53935233 |  813 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
|  53935233 |  814 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
|  53935233 |  815 | `		RE_SWAP_DELIMITER(pGen);` |
|  53935233 |  816 | `		return rc;` |
|         - |  817 | `	}` |
|  35602339 |  818 | `	if( pNode->pOp == 0 ){` |
|       ! 0 |  819 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - |  820 | `			"Invalid expression node,PH7 is aborting compilation");` |
|       ! 0 |  821 | `		return SXERR_ABORT;` |
|         - |  822 | `	}` |
|  35602339 |  823 | `	iVmOp = pNode->pOp->iVmOp;` |
|  35602339 |  824 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|         - |  825 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|         - |  826 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|         - |  827 | `		 * and later errors are still reported. */` |
|         3 |  828 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - |  829 | `			"The (unset) cast is no longer supported");` |
|         3 |  830 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 |  831 | `			return SXERR_ABORT;` |
|         - |  832 | `		}` |
|         1 |  833 | `	}` |
|  35602339 |  834 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|       149 |  835 | `		sxu32 nJmp = 0;` |
|         - |  836 | `		sxu32 nNcNsBase;` |
|         - |  837 | `		VmInstr *pInstrFix;` |
|         - |  838 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|         - |  839 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|         - |  840 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|         - |  841 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|         - |  842 | `		 * stack slot carries a writable nIdx. */` |
|       149 |  843 | `		if( pNode->pRight ){` |
|       149 |  844 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       149 |  845 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|       149 |  846 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  847 | `				return rc;` |
|         - |  848 | `			}` |
|       149 |  849 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|         - |  850 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|         - |  851 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|         - |  852 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|         - |  853 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|         - |  854 | `			 * the store, so the parent array does not need to be copied at` |
|         - |  855 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|         - |  856 | `			 * cascade for the actual write path stays correct. */` |
|       149 |  857 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|       149 |  858 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|        89 |  859 | `				pInstrFix->iP2 = 3;` |
|        43 |  860 | `			}` |
|        73 |  861 | `		}` |
|         - |  862 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|       149 |  863 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|         - |  864 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|       149 |  865 | `		if( pNode->pLeft ){` |
|       149 |  866 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       149 |  867 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|       149 |  868 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  869 | `				return rc;` |
|         - |  870 | `			}` |
|       149 |  871 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|        73 |  872 | `		}` |
|         - |  873 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|       149 |  874 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|         - |  875 | `		/* Patch the short-circuit jump to land after the store. */` |
|       149 |  876 | `		if( nJmp > 0 ){` |
|       149 |  877 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|       149 |  878 | `			if( pInstrFix ){` |
|       149 |  879 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|        73 |  880 | `			}` |
|        73 |  881 | `		}` |
|       149 |  882 | `		return SXRET_OK;` |
|         - |  883 | `	}` |
|  35602193 |  884 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|         - |  885 | `		sxu32 nJz,nJmp;` |
|         - |  886 | `		sxu32 nTernaryNsBase;` |
|         - |  887 | `		/* Ternary operator require special handling */` |
|         - |  888 | `		/* Phase#1: Compile the condition */` |
|    601081 |  889 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    601081 |  890 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|    601081 |  891 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  892 | `			return rc;` |
|         - |  893 | `		}` |
|         - |  894 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|         - |  895 | `		 * compiling the condition must short-circuit to the end of the` |
|         - |  896 | `		 * condition expression, not leak past the ternary. */` |
|    601081 |  897 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    601081 |  898 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|    601081 |  899 | `		if( pNode->pLeft ){` |
|         - |  900 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|         - |  901 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|    596481 |  902 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - |  903 | `			/* Phase#3: Compile the 'then' expression  */` |
|    596481 |  904 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    596481 |  905 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|    596481 |  906 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  907 | `				return rc;` |
|         - |  908 | `			}` |
|    596481 |  909 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    298243 |  910 | `		}else{` |
|         - |  911 | `			/* Elvis operator: (expr) ?: (else)` |
|         - |  912 | `			 * Duplicate condition so original value is the 'then' result.` |
|         - |  913 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|      4605 |  914 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|      4605 |  915 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - |  916 | `		}` |
|         - |  917 | `		/* Phase#4: Emit the unconditional jump */` |
|    601081 |  918 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|         - |  919 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|    601081 |  920 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|    601081 |  921 | `		if( pInstr ){` |
|    601081 |  922 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    300538 |  923 | `		}` |
|    601081 |  924 | `		if( !pNode->pLeft ){` |
|         - |  925 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|      4605 |  926 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|      2300 |  927 | `		}` |
|         - |  928 | `		/* Phase#6: Compile the 'else' expression */` |
|    601081 |  929 | `		if( pNode->pRight ){` |
|    601081 |  930 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    601081 |  931 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|    601081 |  932 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  933 | `				return rc;` |
|         - |  934 | `			}` |
|    601081 |  935 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|    300538 |  936 | `		}` |
|    601081 |  937 | `		if( nJmp > 0 ){` |
|         - |  938 | `			/* Phase#7: Fix the unconditional jump */` |
|    601081 |  939 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|    601081 |  940 | `			if( pInstr ){` |
|    601081 |  941 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    300538 |  942 | `			}` |
|    300538 |  943 | `		}` |
|         - |  944 | `		/* All done */` |
|    601081 |  945 | `		return SXRET_OK;` |
|         - |  946 | `	}` |
|  35001117 |  947 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
|         - |  948 | ``		/* PHP 8.5 pipe: `$lhs \|> $rhs` invokes the RHS callable with the LHS`` |
|         - |  949 | ``		 * value as its sole argument [i.e. `$rhs($lhs)`]. Evaluate the LHS (the`` |
|         - |  950 | `		 * argument) first, then the RHS callable, then emit a one-argument` |
|         - |  951 | `		 * OP_CALL — the same stack shape the function-call path builds (the` |
|         - |  952 | `		 * argument sits below the callee). The RHS is any callable expression:` |
|         - |  953 | ``		 * an FCC `f(...)` (an OP_LOAD_FCC Closure), a closure variable, an`` |
|         - |  954 | ``		 * `[obj,method]` pair, or a callable string. */`` |
|         - |  955 | `		sxu32 nPipeNsBase;` |
|        27 |  956 | `		sxi32 iOperandFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE\|EXPR_FLAG_RDONLY_LOAD);` |
|        27 |  957 | `		if( pNode->pLeft == 0 \|\| pNode->pRight == 0 ){` |
|       ! 0 |  958 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - |  959 | `				"'\|>': Missing operand");` |
|       ! 0 |  960 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - |  961 | `		}` |
|         - |  962 | `		/* Argument: the LHS value. */` |
|        27 |  963 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 |  964 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iOperandFlags);` |
|        27 |  965 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  966 | `			return rc;` |
|         - |  967 | `		}` |
|        27 |  968 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - |  969 | `		/* Callable: the RHS. */` |
|        27 |  970 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 |  971 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iOperandFlags);` |
|        27 |  972 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  973 | `			return rc;` |
|         - |  974 | `		}` |
|        27 |  975 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - |  976 | `		/* Invoke the callable with the single piped argument. */` |
|        27 |  977 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        27 |  978 | `		return SXRET_OK;` |
|         - |  979 | `	}` |
|  35001091 |  980 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|         - |  981 | `	/* Generate code for the left tree */` |
|  35001091 |  982 | `	if( pNode->pLeft ){` |
|  34938069 |  983 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  34938069 |  984 | `		if( iVmOp == PH7_OP_CALL ){` |
|         - |  985 | `			ph7_expr_node **apNode;` |
|   8265583 |  986 | `			int hasSpread = 0;` |
|   8265583 |  987 | `			int hasNamed = 0;` |
|   8265583 |  988 | `			int bAnySpread = 0;` |
|   8265583 |  989 | `			sxu32 byRefMask = 0;` |
|         - |  990 | `			sxi32 nArgs;` |
|         - |  991 | `			sxi32 n;` |
|         - |  992 | `			/* Recurse and generate bytecodes for function arguments */` |
|   8265583 |  993 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   8265583 |  994 | `			nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|         - |  995 | ``			/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|         - |  996 | `			 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|         - |  997 | `			 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|   8265583 |  998 | `			if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|       103 |  999 | `				bFcc = 1;` |
|       103 | 1000 | `				nArgs = 0;` |
|        50 | 1001 | `			}` |
|         - | 1002 | `			/* Validate argument order like php: no positional argument after a` |
|         - | 1003 | ``			 * named one OR after unpacking, and `name: ...$x` is a parse error. */`` |
|         - | 1004 | `			{` |
|   8265583 | 1005 | `				int seenNamed = 0;` |
|   8265583 | 1006 | `				int seenSpread = 0;` |
|  17641173 | 1007 | `				for( n = 0; n < nArgs; ++n ){` |
|   9375597 | 1008 | `					if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|      4733 | 1009 | `						bAnySpread = 1;` |
|      4733 | 1010 | `						seenSpread = 1;` |
|      4733 | 1011 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       ! 0 | 1012 | `							rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 1013 | `								"syntax error, unexpected token \"...\"");` |
|       ! 0 | 1014 | `							return SXERR_SYNTAX;` |
|         5 | 1015 | `						}` |
|   9373233 | 1016 | `					}else if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       421 | 1017 | `						seenNamed = 1;` |
|       421 | 1018 | `						hasNamed = 1;` |
|   9370661 | 1019 | `					}else if( seenNamed ){` |
|         3 | 1020 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 1021 | `							"Cannot use positional argument after named argument");` |
|         3 | 1022 | `						return SXERR_SYNTAX;` |
|   9370451 | 1023 | `					}else if( seenSpread ){` |
|       ! 0 | 1024 | `						rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 1025 | `							"Cannot use positional argument after argument unpacking");` |
|       ! 0 | 1026 | `						return SXERR_SYNTAX;` |
|         - | 1027 | `					}` |
|   4687800 | 1028 | `				}` |
|         - | 1029 | `			}` |
|         - | 1030 | `			/* Read-only load */` |
|   8265581 | 1031 | `			iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|         - | 1032 | `			/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|         - | 1033 | ``			 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|         - | 1034 | `			 * objects dispatch to the right method (offsetExists for both;` |
|         - | 1035 | `			 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|   8265581 | 1036 | `			if( pNode->pLeft && pNode->pLeft->pStart ){` |
|   8265581 | 1037 | `				SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|  12857450 | 1038 | `				int bIsset = pCallName->nByte == 5` |
|   8265576 | 1039 | `					&& SyStrnicmp(pCallName->zString,"isset",5) == 0;` |
|  12857450 | 1040 | `				int bEmpty = pCallName->nByte == 5` |
|   8265576 | 1041 | `					&& SyStrnicmp(pCallName->zString,"empty",5) == 0;` |
|         - | 1042 | `				/* isset()/empty() are language CONSTRUCTS, not functions: php parses` |
|         - | 1043 | `				 * their argument list in the grammar and a missing operand is a parse` |
|         - | 1044 | `				 * error on the ')'. They compile through this ordinary call loop, which` |
|         - | 1045 | ``				 * never checked arity, so `empty()` quietly evaluated to true and`` |
|         - | 1046 | ``				 * `isset()` to false. (empty() also takes exactly one operand in php,`` |
|         - | 1047 | `				 * unlike isset(), which is variadic.) */` |
|   8265581 | 1048 | `				if( (bIsset \|\| bEmpty) && nArgs < 1 ){` |
|         - | 1049 | `					/* php names the ')' itself as the unexpected token, so point at the` |
|         - | 1050 | `					 * node's last token rather than pGen->pIn (which has already moved` |
|         - | 1051 | `					 * past the call to the statement's ';'). */` |
|         5 | 1052 | `					SyToken *pTok = pNode->pEnd;` |
|         5 | 1053 | `					if( pTok && pTok > pNode->pStart && (pTok->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 | 1054 | `						pTok--;` |
|       ! 0 | 1055 | `					}` |
|         5 | 1056 | `					PH7_GenSyntaxError(&(*pGen),pTok,0);` |
|         5 | 1057 | `					return SXERR_ABORT;` |
|         - | 1058 | `				}` |
|   8265577 | 1059 | `				if( bIsset ){` |
|    380867 | 1060 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|   8075146 | 1061 | `				}else if( bEmpty ){` |
|       149 | 1062 | `					iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|        72 | 1063 | `				}` |
|         - | 1064 | `				/* Auto-vivify by-reference out-params of known builtins so an` |
|         - | 1065 | `				 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|         - | 1066 | `				 * $m never assigned) gets a real memobj slot for the builtin to` |
|         - | 1067 | `				 * write back through. Skipped when spread/named args are present:` |
|         - | 1068 | `				 * the compile-time positional index no longer maps to the` |
|         - | 1069 | `				 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|   8265577 | 1070 | `				if( !bAnySpread && !hasNamed ){` |
|         - | 1071 | `					SyString sBuiltin;` |
|   8260619 | 1072 | `					GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|   8260619 | 1073 | `					byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|   4130307 | 1074 | `				}` |
|   4132786 | 1075 | `			}` |
|  17641163 | 1076 | `			for( n = 0 ; n < nArgs ; ++n ){` |
|   9375593 | 1077 | `				sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   9375593 | 1078 | `				sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|         - | 1079 | `				/* For a by-ref argument position, drop the read-only flag so the` |
|         - | 1080 | `				 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|         - | 1081 | `				 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|         - | 1082 | `				 * auto-vivifies its element and exposes a writable memobj slot for the` |
|         - | 1083 | `				 * builtin to write back through. A plain $var target is unaffected` |
|         - | 1084 | `				 * (iP1=0 either way). */` |
|   9375593 | 1085 | `				if( n < 31 && (byRefMask & (1u<<n)) ){` |
|     50057 | 1086 | `					iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|     50057 | 1087 | `					iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|     25026 | 1088 | `				}` |
|         - | 1089 | ``				/* D1: a plain `$var` argument may bind to a by-ref parameter whose signature`` |
|         - | 1090 | `				 * is unknown at compile time (forward reference, dynamic call, or method` |
|         - | 1091 | ``				 * dispatch — e.g. PHPUnit's `willReturnReference($undef)`). We used to clear`` |
|         - | 1092 | `				 * the read-only flag here so an undefined variable vivified a real slot the` |
|         - | 1093 | `				 * by-ref write-back could reach — but that also invented the variable as NULL` |
|         - | 1094 | `				 * in the caller when the parameter turned out by-VALUE, and suppressed php's` |
|         - | 1095 | ``				 * `Undefined variable $x` warning. Instead mark it DEFERRED: OP_LOAD leaves an`` |
|         - | 1096 | `				 * undefined variable uncreated and carries a lazy-lvalue marker, and OP_CALL` |
|         - | 1097 | `				 * materializes it ONLY for a by-ref parameter once the callee is resolved` |
|         - | 1098 | `				 * (VmResolveDeferredArgs). Excludes isset()/empty()/unset(), which compile` |
|         - | 1099 | `				 * through this same call loop but must NEVER create their operand, and` |
|         - | 1100 | `				 * named/spread args (positional-index and by-ref semantics don't apply).` |
|         - | 1101 | `				 *` |
|         - | 1102 | `				 * D1 commit 2: the same reasoning extends to an array-element ($a["k"]) or` |
|         - | 1103 | `				 * property ($o->p) argument — a by-ref user-function parameter must vivify the` |
|         - | 1104 | `				 * element/property, a by-value one must warn and NOT vivify. Those nodes carry a` |
|         - | 1105 | `				 * subscript/arrow operator (pOp != 0). The DEFER flag rides down to the base LOAD` |
|         - | 1106 | `				 * (undefined base auto-defers via commit 1) and to the LOAD_IDX/MEMBER, which` |
|         - | 1107 | ``				 * record the lvalue path on a lookup miss. Static `::` and nullsafe `?->` stay`` |
|         - | 1108 | `				 * eager. */` |
|   9375588 | 1109 | `				if( (iFlags & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_LOAD_IDX_UNSET` |
|   4687794 | 1110 | `				               \|EXPR_FLAG_MEMBER_COALESCE)) == 0` |
|   9185072 | 1111 | `				 && (iArgFlags & EXPR_FLAG_RDONLY_LOAD) /* not a known builtin by-ref slot (kept eager above) */` |
|   8969530 | 1112 | `				 && (apNode[n]->iFlags & (EXPR_NODE_NAMED_ARG\|EXPR_NODE_SPREAD)) == 0` |
|  10392090 | 1113 | `				 && ( (apNode[n]->pOp == 0 && apNode[n]->xCode == PH7_CompileVariable)` |
|   7015189 | 1114 | `				   \|\| (apNode[n]->pOp != 0 && (apNode[n]->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|   3122301 | 1115 | `				                            \|\| apNode[n]->pOp->iOp == EXPR_OP_ARROW)) ) ){` |
|   5931865 | 1116 | `					iArgFlags \|= EXPR_FLAG_DEFER_ARG;` |
|   2965930 | 1117 | `				}` |
|   9375593 | 1118 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|   9375593 | 1119 | `				if( rc != SXRET_OK ){` |
|         3 | 1120 | `					return rc;` |
|         - | 1121 | `				}` |
|         - | 1122 | `				/* Each argument is an independent nullsafe scope. */` |
|   9375591 | 1123 | `				GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|   9375591 | 1124 | `				if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|         - | 1125 | `					/* Emit spread opcode to unpack this array argument */` |
|      4733 | 1126 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD, 0, 0, 0, 0);` |
|      4733 | 1127 | `					hasSpread = 1;` |
|      2364 | 1128 | `				}` |
|   4687798 | 1129 | `			}` |
|         - | 1130 | `			/* Total number of given arguments */` |
|   8265575 | 1131 | `			iP1 = nArgs;` |
|   8265575 | 1132 | `			iP2 = hasSpread;` |
|         - | 1133 | `			/* Build VmCallArgMap if named arguments are present.` |
|         - | 1134 | `			 * Deep-copy name strings so they survive token stream cleanup. */` |
|   8265575 | 1135 | `			if( hasNamed ){` |
|       279 | 1136 | `				sxu32 nStrBytes = 0;` |
|         - | 1137 | `				char *zBuf;` |
|       807 | 1138 | `				for( n = 0; n < nArgs; ++n ){` |
|       533 | 1139 | `					if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       419 | 1140 | `						nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|       207 | 1141 | `					}` |
|       269 | 1142 | `				}` |
|         - | 1143 | `				{` |
|       279 | 1144 | `				sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|       279 | 1145 | `				VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|       274 | 1146 | `					&pGen->pVm->sAllocator, mapSize);` |
|       279 | 1147 | `				if( pMap ){` |
|       279 | 1148 | `					SyZero(pMap, mapSize);` |
|       279 | 1149 | `					pMap->bHasNamed = 1;` |
|       279 | 1150 | `					pMap->nTotal = (sxu32)nArgs;` |
|       279 | 1151 | `					pMap->aNames = (SyString *)&pMap[1];` |
|       279 | 1152 | `					zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|       807 | 1153 | `					for( n = 0; n < nArgs; ++n ){` |
|       533 | 1154 | `						if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       419 | 1155 | `							sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|       419 | 1156 | `							SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|       419 | 1157 | `							SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|       419 | 1158 | `							zBuf += nb;` |
|       207 | 1159 | `						}` |
|         - | 1160 | `						/* else: aNames[n] remains {NULL, 0} for positional */` |
|       269 | 1161 | `					}` |
|       279 | 1162 | `					p3 = (void *)pMap;` |
|       137 | 1163 | `				}` |
|         - | 1164 | `				}` |
|       137 | 1165 | `			}` |
|         - | 1166 | `			/* assert(): php's compiler keeps a copy of the assertion's AST and a` |
|         - | 1167 | ``			 * failing assert reports its rendered SOURCE (`assert(1 == 2)`), not the`` |
|         - | 1168 | `			 * evaluated value. Render the first argument's token span` |
|         - | 1169 | `			 * into the call map so vm_builtin_assert can echo it. Only a DIRECT` |
|         - | 1170 | `			 * unqualified/absolute call qualifies — matching php, an indirect call` |
|         - | 1171 | `			 * (call_user_func, a callable variable) has no source text and its` |
|         - | 1172 | `			 * AssertionError carries an empty message. A spread first argument is` |
|         - | 1173 | `			 * skipped (its span is the unpacked array, not the assertion). */` |
|   8265570 | 1174 | `			if( nArgs >= 1 && !bFcc` |
|   6489835 | 1175 | `			 && (apNode[0]->iFlags & EXPR_NODE_SPREAD) == 0 ){` |
|         - | 1176 | `				SyString sCallee;` |
|   6485147 | 1177 | `				GenStateCallBuiltinName(pNode->pLeft,&sCallee);` |
|   6485142 | 1178 | `				if( sCallee.nByte == sizeof("assert")-1` |
|   3565447 | 1179 | `				 && SyStrnicmp(sCallee.zString,"assert",sizeof("assert")-1) == 0 ){` |
|         - | 1180 | `					/* An operator root's pStart/pEnd name only the operator token` |
|         - | 1181 | ``					 * (`1 == 2` roots at `==`); the subtree walk recovers the whole`` |
|         - | 1182 | `					 * raw extent, re-adding parens the grouping pass consumed. */` |
|        67 | 1183 | `					SyToken *pSpanIn = 0;` |
|        67 | 1184 | `					SyToken *pSpanEnd = 0;` |
|         - | 1185 | `					SyBlob sSrc;` |
|        67 | 1186 | `					PH7_ExprSubtreeSpan(apNode[0],&pSpanIn,&pSpanEnd);` |
|        67 | 1187 | `					SyBlobInit(&sSrc,&pGen->pVm->sAllocator);` |
|        67 | 1188 | `					if( pSpanIn && pSpanEnd && pSpanIn < pSpanEnd ){` |
|        67 | 1189 | `						if( apNode[0]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|         - | 1190 | ``							/* php renders the name too: `assert(assertion: 1 == 2)`. */`` |
|         3 | 1191 | `							SyBlobAppend(&sSrc,apNode[0]->sArgName.zString,apNode[0]->sArgName.nByte);` |
|         3 | 1192 | `							SyBlobAppend(&sSrc,": ",2);` |
|         1 | 1193 | `						}` |
|        67 | 1194 | `						PH7_GenRenderAssertSpan(pGen,pSpanIn,pSpanEnd,&sSrc);` |
|        31 | 1195 | `					}` |
|        67 | 1196 | `					if( SyBlobLength(&sSrc) > 0 ){` |
|        98 | 1197 | `						char *zDup = (char *)SyMemBackendDup(&pGen->pVm->sAllocator,` |
|        62 | 1198 | `							SyBlobData(&sSrc),SyBlobLength(&sSrc));` |
|        67 | 1199 | `						if( zDup ){` |
|        67 | 1200 | `							if( p3 == 0 ){` |
|        65 | 1201 | `								VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|        60 | 1202 | `									&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|        65 | 1203 | `								if( pMap ){` |
|        65 | 1204 | `									SyZero(pMap,sizeof(VmCallArgMap));` |
|        65 | 1205 | `									p3 = (void *)pMap;` |
|        30 | 1206 | `								}` |
|        30 | 1207 | `							}` |
|        67 | 1208 | `							if( p3 ){` |
|        67 | 1209 | `								SyStringInitFromBuf(&((VmCallArgMap *)p3)->sAssertSrc,` |
|         - | 1210 | `									zDup,SyBlobLength(&sSrc));` |
|        31 | 1211 | `							}` |
|        31 | 1212 | `						}` |
|        31 | 1213 | `					}` |
|        67 | 1214 | `					SyBlobRelease(&sSrc);` |
|        31 | 1215 | `				}` |
|   3242571 | 1216 | `			}` |
|         - | 1217 | `			/* Remove stale flags now */` |
|   8265575 | 1218 | `			iFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|   4132785 | 1219 | `		}` |
|         - | 1220 | `		{` |
|         - | 1221 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|         - | 1222 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|         - | 1223 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|         - | 1224 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|         - | 1225 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|         - | 1226 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|         - | 1227 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|         - | 1228 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
|  34938061 | 1229 | `			sxi32 iLeftFlags = iFlags;` |
|  34938061 | 1230 | `			sxu32 nNullcLhsFirst = PH7_VmInstrLength(pGen->pVm);` |
|  34938061 | 1231 | `			int bNullcLhs = 0;` |
|         - | 1232 | `			/* D1 commit 2: a deferred element/property call arg records its lvalue chain, but` |
|         - | 1233 | `			 * that chain must be CONTIGUOUS. Only propagate DEFER_ARG to the base when the base` |
|         - | 1234 | `			 * is itself a continuable lvalue — a plain variable (an undefined base auto-defers),` |
|         - | 1235 | ``			 * another subscript, or a `->` member. If the base is anything else (most importantly`` |
|         - | 1236 | ``			 * a method CALL, e.g. `$o->items()->prop` or `$r->attributes->item(0)->nodeName`),`` |
|         - | 1237 | `			 * strip DEFER so that intermediate read is a NORMAL read, not a record-mode carrier. */` |
|  34938061 | 1238 | `			if( iLeftFlags & EXPR_FLAG_DEFER_ARG ){` |
|   4447833 | 1239 | `				int bContinuable = pNode->pLeft` |
|   3406098 | 1240 | `					&& ( (pNode->pLeft->pOp == 0 && pNode->pLeft->xCode == PH7_CompileVariable)` |
|   1252410 | 1241 | `					  \|\| (pNode->pLeft->pOp != 0 && (pNode->pLeft->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|    126860 | 1242 | `					                              \|\| pNode->pLeft->pOp->iOp == EXPR_OP_ARROW)) );` |
|   2223919 | 1243 | `				if( !bContinuable ){` |
|        58 | 1244 | `					iLeftFlags &= ~EXPR_FLAG_DEFER_ARG;` |
|        27 | 1245 | `				}` |
|   1111957 | 1246 | `			}` |
|  34938056 | 1247 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
|  28473909 | 1248 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|  11004909 | 1249 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|   9503065 | 1250 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|   3248949 | 1251 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|   1624472 | 1252 | `			}` |
|         - | 1253 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|         - | 1254 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|         - | 1255 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|         - | 1256 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|         - | 1257 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|         - | 1258 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|         - | 1259 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
|  34938056 | 1260 | `			if( pNode->pOp` |
|  49147057 | 1261 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
|  31678079 | 1262 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|  28418047 | 1263 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|   7010615 | 1264 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|   3505305 | 1265 | `			}` |
|         - | 1266 | ``			/* `++`/`--` mutate their operand in place — the operand is a write`` |
|         - | 1267 | ``			 * lvalue exactly like a compound assign's (`$o->m[0]++` must tag the`` |
|         - | 1268 | ``			 * member base PH7_MEMBER_WRITE the way `$o->m[0] += 1` does: hooked`` |
|         - | 1269 | `			 * properties throw php's Indirect-modification Error, missing ones` |
|         - | 1270 | `			 * auto-vivify). The prec-18 site below handles the assign family;` |
|         - | 1271 | ``			 * `++`/`--` are unary, their operand is pLeft. */`` |
|  34938056 | 1272 | `			if( pNode->pOp` |
|  34938061 | 1273 | `				&& (pNode->pOp->iVmOp == PH7_OP_INCR \|\| pNode->pOp->iVmOp == PH7_OP_DECR) ){` |
|    249629 | 1274 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE` |
|         - | 1275 | ``					\| EXPR_FLAG_RMW_LOAD /* php warns before seeding `$undef++` */;`` |
|    124812 | 1276 | `			}` |
|         - | 1277 | ``			/* `??` reads its LEFT operand in isset-context: an undefined or`` |
|         - | 1278 | `			 * UNINITIALIZED typed PROPERTY must yield the default rather than a` |
|         - | 1279 | `			 * warning/Error (php). Tag a member-access LHS so its OP_MEMBER takes` |
|         - | 1280 | `			 * the silent-lookup path (iP2 = ISSET), which still loads a present` |
|         - | 1281 | `			 * value. A SUBSCRIPT LHS is left untagged: LOAD_IDX's ISSET mode means` |
|         - | 1282 | ``			 * offsetExists (a bool), but `$o[$k] ?? d` needs the offsetGet value —`` |
|         - | 1283 | `			 * that path is already handled correctly by OP_NULLC. */` |
|  34938061 | 1284 | `			if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC && pNode->pLeft ){` |
|         - | 1285 | ``				/* php reads the ENTIRE left operand of `??` in isset-context: no`` |
|         - | 1286 | ``				 * "Undefined variable" for `$x ?? d` OR for the base of`` |
|         - | 1287 | ``				 * `$x['k'] ?? d`. QUIET_VAR silences the variable read wherever it`` |
|         - | 1288 | `				 * sits in the chain. */` |
|     68197 | 1289 | `				iLeftFlags \|= EXPR_FLAG_QUIET_VAR;` |
|     68197 | 1290 | `				bNullcLhs = 1;` |
|     68192 | 1291 | `				if( pNode->pLeft->pOp` |
|    102201 | 1292 | `					&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|     68111 | 1293 | `						\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|     68087 | 1294 | `						\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|         - | 1295 | `					/* A member-access LHS additionally takes OP_MEMBER's SILENT` |
|         - | 1296 | `					 * lookup so an uninitialized typed property yields the default` |
|         - | 1297 | `					 * instead of an Error. It used to borrow isset()'s context for` |
|         - | 1298 | `					 * that, which is the same mistake the comment below records for` |
|         - | 1299 | `					 * subscripts: silence is shared, but isset() context makes every` |
|         - | 1300 | ``					 * ACCESSOR answer a truth, and `$o->p ?? d` needs the accessor's`` |
|         - | 1301 | ``					 * VALUE — so `??` has its own member context. A SUBSCRIPT LHS`` |
|         - | 1302 | `					 * still takes neither: LOAD_IDX's ISSET mode means offsetExists` |
|         - | 1303 | ``					 * (a bool), while `$o[$k] ?? d` needs the offsetGet value —`` |
|         - | 1304 | `					 * OP_NULLC already handles that path. */` |
|        52 | 1305 | `					iLeftFlags \|= EXPR_FLAG_MEMBER_COALESCE;` |
|        25 | 1306 | `				}` |
|     34096 | 1307 | `			}` |
|  34938061 | 1308 | `			if( iVmOp == PH7_OP_ERR_CTRL ){` |
|         - | 1309 | `				/* '@' must suppress the diagnostics raised WHILE its operand runs, so` |
|         - | 1310 | `				 * open the window here; the trailing emit below closes it (iP1 = 0). */` |
|     18407 | 1311 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_ERR_CTRL,1,0,0,0);` |
|      9201 | 1312 | `			}` |
|  34938061 | 1313 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags\|EXPR_FLAG_RDONLY_LOAD);` |
|  34938061 | 1314 | `			if( rc == SXRET_OK && bNullcLhs ){` |
|         - | 1315 | ``				/* Mark EVERY subscript read in the `??` left chain quiet (iP2=8).`` |
|         - | 1316 | `				 * Peeking at the next instruction only catches the OUTERMOST` |
|         - | 1317 | ``				 * access, so `$d['x']['y'] ?? $v` still warned for the inner one;`` |
|         - | 1318 | `				 * and a forward scan would be unsound, since an unrelated sibling` |
|         - | 1319 | ``				 * access can sit immediately before a coalesce (`f($a['k'],`` |
|         - | 1320 | ``				 * $b['j'] ?? 1)`). The compiler knows the real extent, so it marks`` |
|         - | 1321 | `				 * the range it just emitted. Write-context codes (1/3/5) belong to` |
|         - | 1322 | ``				 * `??=` and keep their meaning. */`` |
|     68197 | 1323 | `				sxu32 nEnd = PH7_VmInstrLength(pGen->pVm);` |
|         - | 1324 | `				sxu32 nAt;` |
|    381443 | 1325 | `				for( nAt = nNullcLhsFirst ; nAt < nEnd ; ++nAt ){` |
|    313251 | 1326 | `					VmInstr *pFix = PH7_VmGetInstr(pGen->pVm,nAt);` |
|    313251 | 1327 | `					if( pFix && pFix->iOp == PH7_OP_LOAD_IDX && pFix->iP2 == 0 ){` |
|     68069 | 1328 | `						pFix->iP2 = 8;` |
|     34032 | 1329 | `					}` |
|    156628 | 1330 | `				}` |
|     34096 | 1331 | `			}` |
|         - | 1332 | `		}` |
|  34938061 | 1333 | `		if( rc != SXRET_OK ){` |
|        67 | 1334 | `			return rc;` |
|         - | 1335 | `		}` |
|  34937999 | 1336 | `		if( !bIsChainOp ){` |
|         - | 1337 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|         - | 1338 | `			 * target the end of that LHS chain, which is right here. */` |
|  16134621 | 1339 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|   8067308 | 1340 | `		}` |
|  34937999 | 1341 | `		if( iVmOp == PH7_OP_CALL ){` |
|   8265575 | 1342 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   8265575 | 1343 | `			if( pInstr ){` |
|   8265575 | 1344 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|   6148089 | 1345 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|         - | 1346 | `					sxu32 nQual;` |
|   6148089 | 1347 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 1348 | `					/* Prevent constant expansion but preserve the absolute flag` |
|         - | 1349 | `					 * so the later NEW handler (if any) can see it. */` |
|   6148089 | 1350 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|         - | 1351 | `					/* Namespace-qualify the function name for CALL, unless the` |
|         - | 1352 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|         - | 1353 | `					 * imports — class imports must NOT affect function` |
|         - | 1354 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|         - | 1355 | `					 * before NEW; we store the original literal index in the` |
|         - | 1356 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|         - | 1357 | `					 * the unqualified name and re-qualify with class imports. */` |
|   6148089 | 1358 | `					if( bAbsolute ){` |
|      4593 | 1359 | `						pInstr->iP2 = (sxi32)nOrig;` |
|      2299 | 1360 | `					}else{` |
|   6143501 | 1361 | `						int fromImport = 0;` |
|   6143501 | 1362 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|   6143501 | 1363 | `						pInstr->iP2 = (sxi32)nQual;` |
|   6143501 | 1364 | `						if( nQual != nOrig ){` |
|         - | 1365 | `							/* Record the original literal index in the arg map` |
|         - | 1366 | `							 * (NOT in the CALL's iP2 — that is the hasSpread` |
|         - | 1367 | `							 * flag) so the NEW handler can recover the` |
|         - | 1368 | `							 * unqualified name and re-qualify with CLASS` |
|         - | 1369 | `							 * imports. */` |
|       153 | 1370 | `							if( p3 == 0 ){` |
|       153 | 1371 | `								VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|       148 | 1372 | `									&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|       153 | 1373 | `								if( pMap ){` |
|       153 | 1374 | `									SyZero(pMap, sizeof(VmCallArgMap));` |
|       153 | 1375 | `									p3 = (void *)pMap;` |
|        74 | 1376 | `								}` |
|        74 | 1377 | `							}` |
|       153 | 1378 | `							if( p3 ){` |
|       153 | 1379 | `								((VmCallArgMap *)p3)->nOrigNameLit = nOrig + 1;` |
|       153 | 1380 | `								if( !fromImport ){` |
|         - | 1381 | `									/* Mark as namespace-qualified */` |
|       129 | 1382 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|        62 | 1383 | `								}` |
|        74 | 1384 | `							}` |
|        74 | 1385 | `						}` |
|         - | 1386 | `					}` |
|   5191533 | 1387 | `				}else if( (pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */` |
|   2105458 | 1388 | `						&& !(pNode->pLeft && (pNode->pLeft->iFlags & EXPR_NODE_PARENS)))` |
|   1070783 | 1389 | `					\|\| pInstr->iOp == PH7_OP_NEW ){` |
|         - | 1390 | `					/* Method call,flag that. But NOT when the callee was an explicitly` |
|         - | 1391 | ``					 * PARENTHESISED member access: `($o->p)(...)` / `($o::$p)(...)` invokes`` |
|         - | 1392 | `					 * the VALUE of the property (php's variable-invocation), so the OP_MEMBER` |
|         - | 1393 | `					 * must stay a plain property READ (leaving the callable on the stack for` |
|         - | 1394 | `					 * OP_CALL to invoke) rather than being rewritten into a method-name` |
|         - | 1395 | ``					 * resolution — the parens are exactly what distinguishes `($o->p)()` from`` |
|         - | 1396 | ``					 * the method call `$o->p()`. */`` |
|   2093427 | 1397 | `					pInstr->iP2 = 1;` |
|         - | 1398 | ``					/* A static call with a DYNAMIC method name (`C::$m(...)`): the`` |
|         - | 1399 | ``					 * static-`::` codegen folded the variable NAME into OP_MEMBER->p3`` |
|         - | 1400 | ``					 * as if it were a static-PROPERTY read (`C::$m`), but in a CALL the`` |
|         - | 1401 | `					 * method name is the variable's VALUE. Rebuild the sequence` |
|         - | 1402 | `					 * [class, OP_LOAD $m -> value, OP_MEMBER(static, method)] so the` |
|         - | 1403 | `					 * dynamic name is read off the stack, matching the instance` |
|         - | 1404 | ``					 * (`$o->$m()`) path. iP1==1 marks a static member. */`` |
|   2093427 | 1405 | `					if( pInstr->iOp == PH7_OP_MEMBER && pInstr->iP1 == 1 && pInstr->p3 ){` |
|        11 | 1406 | `						void *pDynName = pInstr->p3;` |
|        11 | 1407 | `						(void)PH7_VmPopInstr(pGen->pVm);` |
|        11 | 1408 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,pDynName,0);` |
|        11 | 1409 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_MEMBER,1,PH7_MEMBER_METHOD,0,0);` |
|         5 | 1410 | `					}` |
|   1046711 | 1411 | `				}` |
|   4132790 | 1412 | `			}` |
|  30805214 | 1413 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|         - | 1414 | `			ph7_expr_node **apNode;` |
|         - | 1415 | `			sxi32 n;` |
|   3527203 | 1416 | `			sxi32 iChildMask = ~(EXPR_FLAG_LOAD_IDX_STORE` |
|         - | 1417 | `				\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET` |
|         - | 1418 | `				\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE` |
|         - | 1419 | `				\|EXPR_FLAG_MEMBER_COALESCE` |
|         - | 1420 | `				\|EXPR_FLAG_QUIET_VAR\|EXPR_FLAG_RMW_LOAD\|EXPR_FLAG_DEFER_ARG);` |
|         - | 1421 | `			/* Recurse and generate bytecodes for array index */` |
|   3527203 | 1422 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   6782211 | 1423 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|   3255013 | 1424 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   3255013 | 1425 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|   3255013 | 1426 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 1427 | `					return rc;` |
|         - | 1428 | `				}` |
|         - | 1429 | `				/* Each subscript index is an independent nullsafe scope. */` |
|   3255013 | 1430 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|   1627509 | 1431 | `			}` |
|   3527203 | 1432 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|   3255013 | 1433 | `				iP1 = 1; /* Node have an index associated with it */` |
|   1627504 | 1434 | `			}` |
|   3527203 | 1435 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|         - | 1436 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|    435031 | 1437 | `				iP2 = 4;` |
|   3309690 | 1438 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|         - | 1439 | `				/* offsetUnset for ArrayAccess; auto-vivify+load for arrays` |
|         - | 1440 | `				 * so the trailing unset() builtin can drop the slot. */` |
|     27315 | 1441 | `				iP2 = 5;` |
|   3078522 | 1442 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|         - | 1443 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|         - | 1444 | `				 * short-circuit on missing keys without invoking offsetGet` |
|         - | 1445 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|        43 | 1446 | `				iP2 = 6;` |
|   3064848 | 1447 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|         - | 1448 | `				/* Create an empty entry when the desired index is not found */` |
|    644283 | 1449 | `				iP2 = 1;` |
|   2742690 | 1450 | `			}else if( iFlags & EXPR_FLAG_DEFER_ARG ){` |
|         - | 1451 | `				/* D1 commit 2: deferred by-ref/by-value element arg. Behaves as a read but,` |
|         - | 1452 | `				 * on a lookup miss, records the lvalue path instead of warning; OP_CALL` |
|         - | 1453 | `				 * re-walks it in vivify (by-ref) or read+warn (by-value) mode. */` |
|    471183 | 1454 | `				iP2 = 9;` |
|    235594 | 1455 | `			}` |
|  24908830 | 1456 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         - | 1457 | `			/* POP the left node */` |
|         5 | 1458 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         2 | 1459 | `		}` |
|  17468997 | 1460 | `	}` |
|  35001021 | 1461 | `	rc = SXRET_OK;` |
|  35001021 | 1462 | `	nJmpIdx = 0;` |
|         - | 1463 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|         - | 1464 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|         - | 1465 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|  35001021 | 1466 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|    544889 | 1467 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    544889 | 1468 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|    544889 | 1469 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|    544889 | 1470 | `			int isSpecial = 0;` |
|    544889 | 1471 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|    436169 | 1472 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|    436169 | 1473 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|    436164 | 1474 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    406646 | 1475 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    220309 | 1476 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|    172289 | 1477 | `					isSpecial = 1;` |
|     86142 | 1478 | `				}` |
|    245262 | 1479 | `			}` |
|    599249 | 1480 | `			pInstr->iP1 = 0;` |
|         - | 1481 | ``			/* A leading `\` (NSSEP) makes the class name ABSOLUTE: it names the`` |
|         - | 1482 | `			 * global class, so it must NOT be re-qualified with the current namespace.` |
|         - | 1483 | ``			 * `\Closure::bind(...)` inside `namespace X` is Closure, not X\Closure.`` |
|         - | 1484 | ``			 * (Multi-component `\A\B::m` already resolves absolutely because its`` |
|         - | 1485 | ``			 * literal keeps a backslash; the single-component `\Closure` lost it.) */`` |
|         - | 1486 | `			{` |
|    844511 | 1487 | `				int bAbsolute = (pNode->pLeft && pNode->pLeft->pStart` |
|    735786 | 1488 | `					&& (pNode->pLeft->pStart->nType & PH7_TK_NSSEP));` |
|    490529 | 1489 | `				if( !isSpecial && !bAbsolute ){` |
|    318223 | 1490 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|    159109 | 1491 | `				}` |
|         - | 1492 | `			}` |
|         - | 1493 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|         - | 1494 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|    490529 | 1495 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|    318245 | 1496 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|    318245 | 1497 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|       176 | 1498 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|       103 | 1499 | `					return SXRET_OK;` |
|         - | 1500 | `				}` |
|    159071 | 1501 | `			}` |
|    245213 | 1502 | `		}` |
|    353900 | 1503 | `	}` |
|         - | 1504 | `	/* Generate code for the right tree */` |
|  34946585 | 1505 | `	if( pNode->pRight ){` |
|  20149207 | 1506 | `		if( iVmOp == PH7_OP_LAND ){` |
|         - | 1507 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    552937 | 1508 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  19872741 | 1509 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|         - | 1510 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|    475571 | 1511 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|  19358492 | 1512 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|         - | 1513 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|     68197 | 1514 | `			iVmOp = 0; /* No binary operator to emit */` |
|     68197 | 1515 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|  19086668 | 1516 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|         - | 1517 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|         - | 1518 | `			 * the entire containing postfix chain to null. The jump target is` |
|         - | 1519 | `			 * patched later by the innermost non-chain ancestor (or by` |
|         - | 1520 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|         - | 1521 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|         - | 1522 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|       113 | 1523 | `			sxu32 nNsJmp = 0;` |
|       113 | 1524 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|       113 | 1525 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|  19052460 | 1526 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */` |
|  15990700 | 1527 | ``			\|\| pNode->pOp->iOp == EXPR_OP_REF /* `=&` reference bind */ ){`` |
|         - | 1528 | `` 			/* The lvalue is the RIGHT operand (prec-18 ops are right-associative; `=&` `` |
|         - | 1529 | `			 * swaps its operands in parse.c so its target is pRight too). Mark it a write` |
|         - | 1530 | `			 * target so a missing base (the container of a subscript-write, or a bare` |
|         - | 1531 | `` 			 * `$o->p`) is auto-created — PHP auto-vivifies on a plain write AND on a `=&` `` |
|         - | 1532 | ``			 * bind (`$a[0] =& $x` creates $a as [0 => &$x], it does not warn). */`` |
|   6123527 | 1533 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|   6123527 | 1534 | `			if( iVmOp != PH7_OP_STORE && pNode->pOp->iOp != EXPR_OP_REF ){` |
|         - | 1535 | ``				/* COMPOUND assignment (`.=`, `+=`, ...) READS the target first, so`` |
|         - | 1536 | ``				 * php warns when it is undefined and then seeds it; a plain `=` and a`` |
|         - | 1537 | ``				 * `=&` rebind write without reading the target and stay silent. */`` |
|    525713 | 1538 | `				iFlags \|= EXPR_FLAG_RMW_LOAD;` |
|    262854 | 1539 | `			}` |
|   3061761 | 1540 | `		}` |
|  20149207 | 1541 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  20149207 | 1542 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_RDONLY_LOAD);` |
|  20149207 | 1543 | `		if( !bIsChainOp ){` |
|         - | 1544 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|         - | 1545 | `			 * operator instruction is emitted. */` |
|  13138695 | 1546 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|   6569345 | 1547 | `		}` |
|  20149207 | 1548 | `		if( iVmOp == PH7_OP_STORE ){` |
|   5597711 | 1549 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|   5597668 | 1550 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|         - | 1551 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|         - | 1552 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|         - | 1553 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|         - | 1554 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|         - | 1555 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|         - | 1556 | `				 */` |
|       149 | 1557 | `				iVmOp = 0;` |
|   5597639 | 1558 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|   5597567 | 1559 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 1560 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|   1074039 | 1561 | `					iP2 = 1;` |
|    537022 | 1562 | `				}else{` |
|   4523533 | 1563 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 1564 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|    621365 | 1565 | `						iVmOp = PH7_OP_STORE_IDX;` |
|    621365 | 1566 | `						iP1 = pInstr->iP1;` |
|    310685 | 1567 | `					}else{` |
|   3902173 | 1568 | `						p3 = pInstr->p3;` |
|         - | 1569 | `					}` |
|         - | 1570 | `					/* POP the last dynamic load instruction */` |
|   4523533 | 1571 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         - | 1572 | `				}` |
|   2798786 | 1573 | `			}` |
|  17350354 | 1574 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|         - | 1575 | `			/* Peek first: a member LHS ($o->p =& $x, C::$s =& $x) keeps its` |
|         - | 1576 | `			 * OP_MEMBER in place (it resolves + stashes the target slot at` |
|         - | 1577 | `			 * runtime), unlike the variable/array shapes which fold their load` |
|         - | 1578 | `			 * away. Mirrors the normal member-store fold above (iP2 kept-MEMBER). */` |
|       113 | 1579 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|       113 | 1580 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 1581 | `				/* Tag the member as a reference target and flag STORE_REF (iP2=1)` |
|         - | 1582 | `				 * to take the member-rebind path in the VM. */` |
|        19 | 1583 | `				pInstr->iP2 = PH7_MEMBER_REF_TARGET;` |
|        19 | 1584 | `				iP2 = 1;` |
|        11 | 1585 | `			}else{` |
|        97 | 1586 | `				pInstr = PH7_VmPopInstr(pGen->pVm);` |
|        97 | 1587 | `				if( pInstr ){` |
|        97 | 1588 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 1589 | `						/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|         - | 1590 | `						 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|         - | 1591 | `						 */` |
|        41 | 1592 | `						iVmOp = PH7_OP_STORE_IDX_REF;` |
|        41 | 1593 | `						iP1 = pInstr->iP1;` |
|        41 | 1594 | `						iP2 = pInstr->iP2;` |
|        41 | 1595 | `						p3  = pInstr->p3;` |
|        22 | 1596 | `					}else{` |
|        59 | 1597 | `						p3 = pInstr->p3;` |
|         - | 1598 | `					}` |
|        46 | 1599 | `				}` |
|         - | 1600 | `			}` |
|        54 | 1601 | `		}` |
|  10074601 | 1602 | `	}` |
|  34946580 | 1603 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|    512153 | 1604 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|         - | 1605 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|         - | 1606 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|        61 | 1607 | `		iVmOp = 0;` |
|        28 | 1608 | `	}` |
|  34946585 | 1609 | `	if( iVmOp > 0 ){` |
|  34878189 | 1610 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|    249629 | 1611 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|         - | 1612 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|     13637 | 1613 | `				iP1 = 1;` |
|      6821 | 1614 | `			}` |
|  34753377 | 1615 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|         - | 1616 | `			/* Namespace-qualify the class name for NEW */ {` |
|   1027483 | 1617 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|   1027483 | 1618 | `				VmInstr *pCallInstr = 0;` |
|   1027483 | 1619 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|   1013409 | 1620 | `					pCallInstr = pPeek;` |
|   1013409 | 1621 | `					pPeek = PH7_VmPeekNextInstr(pGen->pVm);` |
|    506702 | 1622 | `				}` |
|   1027483 | 1623 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|   1009343 | 1624 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 1625 | `					sxu32 nLitForClass;` |
|   1009343 | 1626 | `					VmCallArgMap *pCallNsMap = pCallInstr ? (VmCallArgMap *)pCallInstr->p3 : 0;` |
|         - | 1627 | `					/* If the CALL handler qualified the name with FUNCTION` |
|         - | 1628 | `					 * imports, recover the original literal (recorded in the` |
|         - | 1629 | `					 * arg map — OP_CALL's iP2 is the hasSpread flag, and` |
|         - | 1630 | `` 					 * misreading it as a literal index made `new C(...$args)` `` |
|         - | 1631 | `					 * fatal with "Class ' ' is not defined") and re-qualify` |
|         - | 1632 | `					 * with class imports. */` |
|   1009343 | 1633 | `					if( pCallNsMap && pCallNsMap->nOrigNameLit > 0 ){` |
|        65 | 1634 | `						nLitForClass = pCallNsMap->nOrigNameLit - 1;` |
|        35 | 1635 | `					}else{` |
|   1009283 | 1636 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|         - | 1637 | `					}` |
|   1009343 | 1638 | `					pPeek->iP1 = 0;` |
|   1009343 | 1639 | `					if( !bAbsolute ){` |
|         - | 1640 | `						/* self/static/parent are resolved at runtime against the` |
|         - | 1641 | `						 * current class — never namespace-qualify them (else` |
|         - | 1642 | ``						 * `new self` in namespace N becomes "N\self"). Mirrors the`` |
|         - | 1643 | `						 * instanceof (IS_A) guard below. */` |
|   1004769 | 1644 | `						ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nLitForClass);` |
|   1004769 | 1645 | `						int isSpecialNew = 0;` |
|   1004769 | 1646 | `						if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|    987401 | 1647 | `							const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|    987401 | 1648 | `							sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|    987396 | 1649 | `							if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    991638 | 1650 | `								(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|    493600 | 1651 | `								(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|      9099 | 1652 | `								isSpecialNew = 1;` |
|      4547 | 1653 | `							}` |
|    498040 | 1654 | `						}` |
|   1013453 | 1655 | `						if( isSpecialNew ){` |
|      9099 | 1656 | `							pPeek->iP2 = (sxi32)nLitForClass;` |
|      4552 | 1657 | `						}else{` |
|    986991 | 1658 | `							pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|         - | 1659 | `						}` |
|    498045 | 1660 | `					}else{` |
|      4579 | 1661 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|         - | 1662 | `					}` |
|    500327 | 1663 | `				}` |
|         - | 1664 | `			}` |
|   1018799 | 1665 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   1018799 | 1666 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|         - | 1667 | `				VmInstr *pPrev;` |
|   1013409 | 1668 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|   1013409 | 1669 | `				if( pPrev == 0 \|\| pPrev->iOp != PH7_OP_MEMBER ){` |
|         - | 1670 | `					/* Pop the call instruction, preserve named-arg map and` |
|         - | 1671 | `					 * the hasSpread flag (OP_NEW consumes the spread` |
|         - | 1672 | `					 * accumulator exactly like OP_CALL would have). */` |
|   1013409 | 1673 | `					iP1 = pInstr->iP1;` |
|   1013409 | 1674 | `					iP2 = pInstr->iP2;` |
|   1013409 | 1675 | `					if( pInstr->p3 ){` |
|        75 | 1676 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|        35 | 1677 | `					}` |
|   1013409 | 1678 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    506702 | 1679 | `				}` |
|    506707 | 1680 | `			}` |
|  34110484 | 1681 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|         - | 1682 | `			/* instanceof: right operand is a class name, not a constant.` |
|         - | 1683 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     95433 | 1684 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     95433 | 1685 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     95433 | 1686 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     95433 | 1687 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     95433 | 1688 | `				int isSpecialIs = 0;` |
|     95433 | 1689 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     95433 | 1690 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     95433 | 1691 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     95428 | 1692 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     95431 | 1693 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     47714 | 1694 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|        12 | 1695 | `						isSpecialIs = 1;` |
|         5 | 1696 | `					}` |
|     47714 | 1697 | `				}` |
|     95433 | 1698 | `				pInstr->iP1 = 0;` |
|     95433 | 1699 | `				if( !isSpecialIs && !bAbsolute ){` |
|     95397 | 1700 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|     47696 | 1701 | `				}` |
|     47719 | 1702 | `			}` |
|  33553373 | 1703 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|         - | 1704 | `			/* Prevent constant expansion for member/property names.` |
|         - | 1705 | `			 * The right child (member name) was just compiled — its LOADC` |
|         - | 1706 | `			 * should not trigger constant lookup. */` |
|   7010517 | 1707 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   7010517 | 1708 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|   6729573 | 1709 | `				pInstr->iP1 = 0;` |
|   3364784 | 1710 | `			}` |
|   7010517 | 1711 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|         - | 1712 | `				/* Static member access,remember that */` |
|    490453 | 1713 | `				iP1 = 1;` |
|    490453 | 1714 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    490453 | 1715 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|    276405 | 1716 | `					p3 = pInstr->p3;` |
|         - | 1717 | ``					/* A `$`-form (`C::$s`, `C::$$x`, `C::${$e}`) is a STATIC PROPERTY`` |
|         - | 1718 | `					 * access, never a constant. A LITERAL name folds into p3 (non-zero)` |
|         - | 1719 | ``					 * and the exec side reads it there; a DYNAMIC name (`$$x`/`${$e}`)`` |
|         - | 1720 | `					 * leaves p3==0 with the computed name on the stack — the SAME shape` |
|         - | 1721 | ``					 * as a bareword constant `C::C`. Mark iP1=2 so exec still routes it`` |
|         - | 1722 | `					 * to the property table (hAttr), not the constant table (hConst). */` |
|    276405 | 1723 | `					if( p3 == 0 ){` |
|         8 | 1724 | `						iP1 = 2;` |
|         3 | 1725 | `					}` |
|    276405 | 1726 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|    138200 | 1727 | `				}` |
|    245224 | 1728 | `			}` |
|         - | 1729 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|         - | 1730 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|         - | 1731 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|         - | 1732 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|   7010517 | 1733 | `			if( iP2 == PH7_MEMBER_READ ){` |
|   7010517 | 1734 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|        48 | 1735 | `					iP2 = PH7_MEMBER_UNSET;` |
|   7010495 | 1736 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|     81629 | 1737 | `					iP2 = PH7_MEMBER_ISSET;` |
|   6969661 | 1738 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|        27 | 1739 | `					iP2 = PH7_MEMBER_EMPTY;` |
|   6928837 | 1740 | `				}else if( iFlags & EXPR_FLAG_MEMBER_COALESCE ){` |
|        56 | 1741 | `					iP2 = PH7_MEMBER_COALESCE;` |
|   6928798 | 1742 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|         - | 1743 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|   1300645 | 1744 | `					iP2 = PH7_MEMBER_WRITE;` |
|   6278451 | 1745 | `				}else if( iFlags & EXPR_FLAG_DEFER_ARG ){` |
|         - | 1746 | `					/* D1 commit 2: deferred by-ref/by-value property arg ($o->p). */` |
|   1752741 | 1747 | `					iP2 = PH7_MEMBER_DEFPATH;` |
|    876368 | 1748 | `				}` |
|   3505256 | 1749 | `			}` |
|   3505256 | 1750 | `		}` |
|         - | 1751 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|         - | 1752 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|         - | 1753 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|         - | 1754 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|         - | 1755 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
|  34869505 | 1756 | `		if( bFcc ){` |
|       103 | 1757 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|       103 | 1758 | `			iP2 = 0;` |
|       103 | 1759 | `			p3 = 0;` |
|       103 | 1760 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|       103 | 1761 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 1762 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|         - | 1763 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|         - | 1764 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|         - | 1765 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|        44 | 1766 | `				void *pMemberName = pInstr->p3;` |
|        44 | 1767 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|        44 | 1768 | `				if( pMemberName ){` |
|       ! 0 | 1769 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|       ! 0 | 1770 | `				}` |
|        44 | 1771 | `				iP1 = 2;` |
|        23 | 1772 | `			}else{` |
|        61 | 1773 | `				iP1 = 1;` |
|         - | 1774 | `			}` |
|        50 | 1775 | `		}` |
|         - | 1776 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|         - | 1777 | `		 * This is the primary emit path for user-visible calls. */` |
|  34869505 | 1778 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|   9284269 | 1779 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|   4642132 | 1780 | `		}` |
|         - | 1781 | `		/* Finally,emit the VM instruction associated with this operator */` |
|  34869505 | 1782 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|  17434750 | 1783 | `	}` |
|  34937901 | 1784 | `	if( nJmpIdx > 0 ){` |
|         - | 1785 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|   1096695 | 1786 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|   1096695 | 1787 | `		if( pInstr ){` |
|   1096695 | 1788 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    548345 | 1789 | `		}` |
|    548345 | 1790 | `	}` |
|  34937901 | 1791 | `	return rc;` |
|  44737275 | 1792 | `}` |
|         - | 1793 | `/*` |
|         - | 1794 | ` * Compile a PHP expression.` |
|         - | 1795 | ` * According to the PHP language reference manual:` |
|         - | 1796 | ` *  Expressions are the most important building stones of PHP.` |
|         - | 1797 | ` *  In PHP, almost anything you write is an expression.` |
|         - | 1798 | ` *  The simplest yet most accurate way to define an expression` |
|         - | 1799 | ` *  is "anything that has a value".` |
|         - | 1800 | ` * If something goes wrong while compiling the expression,this` |
|         - | 1801 | ` * function takes care of generating the appropriate error` |
|         - | 1802 | ` * message.` |
|         - | 1803 | ` */` |
|         - | 1804 | `/*` |
|         - | 1805 | ` * Does this expression tree contain a comma OPERATOR node?` |
|         - | 1806 | ` *` |
|         - | 1807 | `` * PH7 shipped `,` as a lowest-precedence binary operator (IMP-0139-COMMA), so`` |
|         - | 1808 | `` * `(1, 2)` and `$x = (f(), $y)` compile and evaluate to the right operand.`` |
|         - | 1809 | ` * php 8 has no comma operator: its grammar only allows comma-separated` |
|         - | 1810 | ` * expression LISTS inside for(...) clauses (call arguments, array literals and` |
|         - | 1811 | ` * list() are split by the parser, never by this node). Accepting it changes the` |
|         - | 1812 | ` * meaning of source php rejects, which §10 classes as a bug — so every context` |
|         - | 1813 | ` * except for() now reports php's parse error.` |
|         - | 1814 | ` */` |
| 296918630 | 1815 | `static int GenStateTreeHasComma(ph7_expr_node *pNode)` |
|         5 | 1816 | `{` |
|         - | 1817 | `	ph7_expr_node **apArg;` |
|         - | 1818 | `	sxu32 n;` |
| 296918635 | 1819 | `	if( pNode == 0 ){` |
| 208667993 | 1820 | `		return 0;` |
|         - | 1821 | `	}` |
|  88250647 | 1822 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         6 | 1823 | `		return 1;` |
|         - | 1824 | `	}` |
|  88250638 | 1825 | `	if( GenStateTreeHasComma(pNode->pLeft) \|\| GenStateTreeHasComma(pNode->pRight)` |
|  88250639 | 1826 | `	 \|\| GenStateTreeHasComma(pNode->pCond) ){` |
|         6 | 1827 | `		return 1;` |
|         - | 1828 | `	}` |
|  88250639 | 1829 | `	apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
| 100854167 | 1830 | `	for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; n++ ){` |
|  12603533 | 1831 | `		if( GenStateTreeHasComma(apArg[n]) ){` |
|       ! 0 | 1832 | `			return 1;` |
|         - | 1833 | `		}` |
|   6301769 | 1834 | `	}` |
|  88250639 | 1835 | `	return 0;` |
| 148459320 | 1836 | `}` |
|  19985086 | 1837 | `PH7_PRIVATE sxi32 PH7_CompileExpr(` |
|         - | 1838 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 1839 | `	sxi32 iFlags,        /* Control flags */` |
|         - | 1840 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|         - | 1841 | `	)` |
|         5 | 1842 | `{` |
|         - | 1843 | `	ph7_expr_node *pRoot;` |
|         - | 1844 | `	SySet sExprNode;` |
|         - | 1845 | `	SyToken *pEnd;` |
|         - | 1846 | `	sxi32 nExpr;` |
|         - | 1847 | `	sxi32 iNest;` |
|         - | 1848 | `	sxi32 rc;` |
|         - | 1849 | `	sxu32 nNullsafeBase;` |
|         - | 1850 | `	/* Initialize worker variables */` |
|  19985091 | 1851 | `	nExpr = 0;` |
|  19985091 | 1852 | `	pRoot = 0;` |
|         - | 1853 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|         - | 1854 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|  19985091 | 1855 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|  19985091 | 1856 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|  19985091 | 1857 | `	SySetAlloc(&sExprNode,0x10);` |
|  19985091 | 1858 | `	rc = SXRET_OK;` |
|         - | 1859 | `	/* Delimit the expression */` |
|  19985091 | 1860 | `	pEnd = pGen->pIn;` |
|  19985091 | 1861 | `	iNest = 0;` |
| 157574691 | 1862 | `	while( pEnd < pGen->pEnd ){` |
| 149856763 | 1863 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 1864 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|      6603 | 1865 | `			iNest++;` |
| 149853464 | 1866 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|      6613 | 1867 | `			iNest--;` |
| 149846861 | 1868 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|  12270317 | 1869 | `			if( iNest <= 0 ){` |
|  12267163 | 1870 | `				break;` |
|         - | 1871 | `			}` |
|      1577 | 1872 | `		}` |
| 137589605 | 1873 | `		pEnd++;` |
|         5 | 1874 | `	}` |
|  19985091 | 1875 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|    862061 | 1876 | `		SyToken *pEnd2 = pGen->pIn;` |
|    862061 | 1877 | `		iNest = 0;` |
|         - | 1878 | `		/* Stop at the first comma */` |
|   1891181 | 1879 | `		while( pEnd2 < pEnd ){` |
|   1029143 | 1880 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|     59043 | 1881 | `				iNest++;` |
|    999624 | 1882 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|     59043 | 1883 | `				iNest--;` |
|    940586 | 1884 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      6097 | 1885 | `				if( iNest <= 0 ){` |
|        21 | 1886 | `					break;` |
|         - | 1887 | `				}` |
|      3037 | 1888 | `			}` |
|   1029125 | 1889 | `			pEnd2++;` |
|         5 | 1890 | `		}` |
|    862061 | 1891 | `		if( pEnd2 <pEnd ){` |
|        21 | 1892 | `			pEnd = pEnd2;` |
|         9 | 1893 | `		}` |
|    431028 | 1894 | `	}` |
|  19985091 | 1895 | `	if( pEnd > pGen->pIn ){` |
|  19957909 | 1896 | `		SyToken *pTmp = pGen->pEnd;` |
|         - | 1897 | `		/* Swap delimiter */` |
|  19957909 | 1898 | `		pGen->pEnd = pEnd;` |
|         - | 1899 | `		/* Try to get an expression tree */` |
|  19957909 | 1900 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|  19957904 | 1901 | `		if( rc == SXRET_OK && pRoot && pGen->nCommaExprOk < 1` |
|  19760462 | 1902 | `		 && GenStateTreeHasComma(pRoot) ){` |
|         - | 1903 | `			/* php has no comma operator outside a for() clause */` |
|         6 | 1904 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pRoot->pStart->nLine,` |
|         - | 1905 | `				"syntax error, unexpected token \",\"");` |
|         6 | 1906 | `			pGen->pEnd = pTmp;` |
|         6 | 1907 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 1908 | `				SySetRelease(&sExprNode);` |
|       ! 0 | 1909 | `				return SXERR_ABORT;` |
|         - | 1910 | `			}` |
|         6 | 1911 | `			pGen->pIn = pEnd;` |
|         6 | 1912 | `			SySetRelease(&sExprNode);` |
|         6 | 1913 | `			SySetTruncate(&pGen->aNullsafeJmp,nNullsafeBase);` |
|         6 | 1914 | `			return SXRET_OK;` |
|         - | 1915 | `		}` |
|  19957905 | 1916 | `		if( rc == SXRET_OK && pRoot ){` |
|  19957719 | 1917 | `			rc = SXRET_OK;` |
|  19957719 | 1918 | `			if( xTreeValidator ){` |
|         - | 1919 | `				/* Call the upper layer validator callback */` |
|   1216109 | 1920 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    608052 | 1921 | `			}` |
|  19957719 | 1922 | `			if( rc != SXERR_ABORT ){` |
|         - | 1923 | `				/* Generate code for the given tree */` |
|  19957719 | 1924 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|         - | 1925 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|         - | 1926 | `				 * expression so they short-circuit to its end. */` |
|  19957719 | 1927 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|   9978857 | 1928 | `			}` |
|  19957719 | 1929 | `			nExpr = 1;` |
|   9978857 | 1930 | `		}` |
|         - | 1931 | `		/* Release the whole tree */` |
|  19957905 | 1932 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|         - | 1933 | `		/* Synchronize token stream */` |
|  19957905 | 1934 | `		pGen->pEnd = pTmp;` |
|  19957905 | 1935 | `		pGen->pIn  = pEnd;` |
|  19957905 | 1936 | `		if( rc == SXERR_ABORT ){` |
|        59 | 1937 | `			SySetRelease(&sExprNode);` |
|        59 | 1938 | `			return SXERR_ABORT;` |
|         - | 1939 | `		}` |
|   9978923 | 1940 | `	}` |
|  19985033 | 1941 | `	SySetRelease(&sExprNode);` |
|  19985033 | 1942 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|   9992548 | 1943 | `}` |
|         - | 1944 | `/*` |
|         - | 1945 | ` * Return a pointer to the node construct handler associated` |
|         - | 1946 | ` * with a given node type [i.e: string,integer,float,...].` |
|         - | 1947 | ` */` |
|  11198012 | 1948 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|         5 | 1949 | `{` |
|  11198017 | 1950 | `	if( nNodeType & PH7_TK_NUM ){` |
|         - | 1951 | `		/* Numeric literal: Either real or integer */` |
|   4491789 | 1952 | `		return PH7_CompileNumLiteral;` |
|   6706233 | 1953 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|         - | 1954 | `		/* Double quoted string */` |
|    148731 | 1955 | `		return PH7_CompileString;` |
|   6557507 | 1956 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|         - | 1957 | `		/* Single quoted string */` |
|   6557375 | 1958 | `		return PH7_CompileSimpleString;` |
|       137 | 1959 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|         - | 1960 | `		/* Heredoc */` |
|        81 | 1961 | `		return PH7_CompileHereDoc;` |
|        61 | 1962 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|         - | 1963 | `		/* Nowdoc */` |
|        57 | 1964 | `		return PH7_CompileNowDoc;` |
|         6 | 1965 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|         - | 1966 | `		/* Backtick quoted string */` |
|         3 | 1967 | `		return PH7_CompileBacktic;` |
|         - | 1968 | `	}` |
|         3 | 1969 | `	return 0;` |
|   5599011 | 1970 | `}` |
|         - | 1971 | `/*` |
|         - | 1972 | `` * Tree validator for unset() arguments — rejects any `?->` node in`` |
|         - | 1973 | ` * the argument expression with PHP's "Can't use nullsafe operator` |
|         - | 1974 | ` * in write context" parse error.` |
|         - | 1975 | ` */` |
|     27344 | 1976 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|         5 | 1977 | `{` |
|         - | 1978 | `	sxi32 rc;` |
|     27349 | 1979 | `	if( !PH7_ExprContainsNullsafe(pNode) ){` |
|     27347 | 1980 | `		return SXRET_OK;` |
|         - | 1981 | `	}` |
|         5 | 1982 | `	rc = PH7_GenCompileError(pGen,E_PARSE,` |
|         2 | 1983 | `		pNode ? pNode->pStart->nLine : 1,` |
|         - | 1984 | `		"Can't use nullsafe operator in write context");` |
|         3 | 1985 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|     13677 | 1986 | `}` |
|         - | 1987 | `/*` |
|         - | 1988 | ` * Compile an unset() statement.` |
|         - | 1989 | ` * unset($var, $arr[$key], ...);` |
|         - | 1990 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|         - | 1991 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|         - | 1992 | ` * parent array before extracting the element to unset.` |
|         - | 1993 | ` */` |
|     30108 | 1994 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|         5 | 1995 | `{` |
|     30113 | 1996 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|     30113 | 1997 | `	sxu32 nIdx = 0;` |
|         - | 1998 | `	SyString sName;` |
|         - | 1999 | `	sxi32 rc;` |
|         - | 2000 | `	/* Jump the 'unset' keyword */` |
|     30113 | 2001 | `	pGen->pIn++;` |
|         - | 2002 | `	/* Save delimiter */` |
|     30113 | 2003 | `	pTmp = pGen->pEnd;` |
|         - | 2004 | `	/* Skip optional opening parenthesis and find the matching close */` |
|     30113 | 2005 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|     30113 | 2006 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 2007 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|         - | 2008 | `		SyToken *pClose;` |
|     30113 | 2009 | `		pGen->pIn++;   /* Skip '(' */` |
|     30113 | 2010 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|     30113 | 2011 | `		pEnd = pClose; /* Stop at ')' */` |
|     15054 | 2012 | `	}` |
|     30113 | 2013 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|         - | 2014 | `	/* Resolve the 'unset' builtin name once */` |
|     30113 | 2015 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|      4533 | 2016 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|      4533 | 2017 | `		if( pObj == 0 ){` |
|       ! 0 | 2018 | `			return SXERR_ABORT;` |
|         - | 2019 | `		}` |
|      4533 | 2020 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|      4533 | 2021 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|      2264 | 2022 | `	}` |
|         - | 2023 | `	/* Compile each comma-separated argument */` |
|     64261 | 2024 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|     34153 | 2025 | `		if( pGen->pIn < pNext ){` |
|         - | 2026 | `			/*` |
|         - | 2027 | ``			 * A PLAIN variable (`unset($x)`, exactly two tokens: '$' and the name) drops a`` |
|         - | 2028 | `			 * single NAME binding, which the unset() builtin cannot express — all it ever` |
|         - | 2029 | `			 * receives is the value's slot index, and unsetting the slot destroys whatever` |
|         - | 2030 | `			 * else aliases it. Emit OP_UNSET_VAR with the name instead. Subscripts and` |
|         - | 2031 | ``			 * properties (`unset($a[k])`, `unset($o->p)`) keep the existing path, which`` |
|         - | 2032 | `			 * already removes just the element/property.` |
|         - | 2033 | `			 */` |
|     34148 | 2034 | `			if( &pGen->pIn[2] == pNext` |
|     20476 | 2035 | `				&& (pGen->pIn[0].nType & PH7_TK_DOLLAR)` |
|      6809 | 2036 | `				&& (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         - | 2037 | `				SyString *pVarName;` |
|     10208 | 2038 | `				char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      6802 | 2039 | `					pGen->pIn[1].sData.zString,pGen->pIn[1].sData.nByte);` |
|      6807 | 2040 | `				pVarName = (SyString *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(SyString));` |
|      6807 | 2041 | `				if( zDup == 0 \|\| pVarName == 0 ){` |
|       ! 0 | 2042 | `					PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - | 2043 | `						"Fatal, PH7 is running out of memory");` |
|       ! 0 | 2044 | `					return SXERR_ABORT;` |
|         - | 2045 | `				}` |
|      6807 | 2046 | `				SyStringInitFromBuf(pVarName,zDup,pGen->pIn[1].sData.nByte);` |
|      6807 | 2047 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_UNSET_VAR,0,0,pVarName,0);` |
|      6807 | 2048 | `				pGen->pIn = pNext;` |
|      6807 | 2049 | `				if( pGen->pIn < pEnd ){` |
|      4031 | 2050 | `					pGen->pIn++; /* Jump the trailing comma */` |
|      2013 | 2051 | `				}` |
|      6807 | 2052 | `				continue;` |
|         - | 2053 | `			}` |
|     27351 | 2054 | `			pGen->pEnd = pNext;` |
|     27351 | 2055 | `			rc = PH7_CompileExpr(&(*pGen),` |
|         - | 2056 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|         - | 2057 | `				GenStateUnsetValidator);` |
|     27351 | 2058 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 2059 | `				return SXERR_ABORT;` |
|         - | 2060 | `			}` |
|     27351 | 2061 | `			if( rc != SXERR_EMPTY ){` |
|         - | 2062 | `				/* Emit call for this single argument */` |
|     27349 | 2063 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|     27349 | 2064 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|     27349 | 2065 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|     13672 | 2066 | `			}` |
|     13673 | 2067 | `		}` |
|         - | 2068 | `		/* Jump trailing commas */` |
|     27367 | 2069 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|        18 | 2070 | `			pNext++;` |
|         2 | 2071 | `		}` |
|     27351 | 2072 | `		pGen->pIn = pNext;` |
|         5 | 2073 | `	}` |
|         - | 2074 | `	/* Skip past the closing ')' if present */` |
|     30113 | 2075 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|     30113 | 2076 | `		pGen->pIn++;` |
|     15054 | 2077 | `	}` |
|         - | 2078 | `	/* Restore token stream */` |
|     30113 | 2079 | `	pGen->pEnd = pTmp;` |
|     30113 | 2080 | `	return SXRET_OK;` |
|     15059 | 2081 | `}` |
|         - | 2082 | `/*` |
|         - | 2083 | ` * PHP Language construct table.` |
|         - | 2084 | ` */` |
|         - | 2085 | `static const LangConstruct aLangConstruct[] = {` |
|         - | 2086 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|         - | 2087 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|         - | 2088 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|         - | 2089 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|         - | 2090 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|         - | 2091 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|         - | 2092 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|         - | 2093 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|         - | 2094 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|         - | 2095 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|         - | 2096 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|         - | 2097 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|         - | 2098 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|         - | 2099 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|         - | 2100 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|         - | 2101 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|         - | 2102 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|         - | 2103 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|         - | 2104 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|         - | 2105 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|         - | 2106 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|         - | 2107 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|         - | 2108 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|         - | 2109 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|         - | 2110 | `};` |
|         - | 2111 | `/*` |
|         - | 2112 | ` * Return a pointer to the statement handler routine associated` |
|         - | 2113 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|         - | 2114 | ` */` |
|   9794166 | 2115 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|         - | 2116 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|         - | 2117 | `	SyToken *pLookahed  /* Look-ahead token */` |
|         - | 2118 | `	)` |
|         5 | 2119 | `{` |
|   9794171 | 2120 | `	sxu32 n = 0;` |
|  39558163 | 2121 | `	for(;;){` |
|  79116331 | 2122 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|    623807 | 2123 | `			break;` |
|         - | 2124 | `		}` |
|  78492529 | 2125 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|   9170369 | 2126 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|       ! 0 | 2127 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|       ! 0 | 2128 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|         - | 2129 | `					/* 'static' (class context),return null */` |
|       ! 0 | 2130 | `					return 0;` |
|         - | 2131 | `				}` |
|       ! 0 | 2132 | `			}` |
|   9170364 | 2133 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|     13600 | 2134 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|      6807 | 2135 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|         - | 2136 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|         3 | 2137 | `				return 0;` |
|         - | 2138 | `			}` |
|         - | 2139 | `			/* Return a pointer to the handler.` |
|         - | 2140 | `			*/` |
|   9170367 | 2141 | `			return aLangConstruct[n].xConstruct;` |
|         - | 2142 | `		}` |
|  69322165 | 2143 | `		n++;` |
|         5 | 2144 | `	}` |
|    623807 | 2145 | `	if( pLookahed ){` |
|    623807 | 2146 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && (pLookahed->nType & PH7_TK_ID) ){` |
|     81679 | 2147 | `			return PH7_CompileClassInterface;` |
|    542133 | 2148 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && (pLookahed->nType & PH7_TK_ID) ){` |
|    464395 | 2149 | `			return PH7_CompileClass;` |
|     77743 | 2150 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && (pLookahed->nType & PH7_TK_ID) ){` |
|      9219 | 2151 | `			return PH7_CompileTrait;` |
|         - | 2152 | `		}` |
|         - | 2153 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|         - | 2154 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|         - | 2155 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|         - | 2156 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|     34262 | 2157 | `	}` |
|         - | 2158 | `	/* Not a language construct */` |
|     68529 | 2159 | `	return 0;` |
|   4897088 | 2160 | `}` |
|         - | 2161 | `/*` |
|         - | 2162 | ` * Check if the given keyword is in fact a PHP language construct.` |
|         - | 2163 | ` * Return TRUE on success. FALSE otheriwse.` |
|         - | 2164 | ` */` |
|     68526 | 2165 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|         5 | 2166 | `{` |
|         - | 2167 | `	int rc;` |
|     68531 | 2168 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|     68531 | 2169 | `	if( rc == FALSE ){` |
|     68366 | 2170 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|     18544 | 2171 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|         - | 2172 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|         - | 2173 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|         - | 2174 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|         - | 2175 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|         - | 2176 | `			*/` |
|         - | 2177 | `			){` |
|     68363 | 2178 | `				rc = TRUE;` |
|     34179 | 2179 | `		}` |
|     34183 | 2180 | `	}` |
|     68531 | 2181 | `	return rc;` |
|         5 | 2182 | `}` |
|         - | 2183 | `/*` |
|         - | 2184 | ` * Compile a PHP chunk.` |
|         - | 2185 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 2186 | ` * takes care of generating the appropriate error message.` |
|         - | 2187 | ` */` |
|         - | 2188 | `/*` |
|         - | 2189 | ` * Update pGen->sPendingDoc for the statement whose first token is` |
|         - | 2190 | ` * pGen->pIn: when a docblock trivia is keyed to that token's index in` |
|         - | 2191 | ` * the chunk token set it becomes the pending docblock. An existing` |
|         - | 2192 | ` * pending docblock is LEFT in place otherwise: Zend keeps the last-seen` |
|         - | 2193 | ` * doc comment until a declaration consumes it, so a docblock survives` |
|         - | 2194 | ` * intervening non-declaration statements.` |
|         - | 2195 | ` */` |
|  21041126 | 2196 | `PH7_PRIVATE void GenStateSetPendingDoc(ph7_gen_state *pGen)` |
|         5 | 2197 | `{` |
|  21041131 | 2198 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|  21041131 | 2199 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|  21041131 | 2200 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 2201 | `	sxu32 nIdx, n;` |
|  21041126 | 2202 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|   3824717 | 2203 | `	 \|\| pGen->pIn < pBase \|\| pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|         - | 2204 | `		/* Re-tokenized substream (string interpolation, synthesized code):` |
|         - | 2205 | `		 * indexes do not map to the sidecar */` |
|  17216421 | 2206 | `		return;` |
|         - | 2207 | `	}` |
|   3824715 | 2208 | `	nIdx = (sxu32)(pGen->pIn - pBase);` |
|         - | 2209 | `	/* Attributes must be adjacent to their declaration (unlike docblocks):` |
|         - | 2210 | `	 * reset at every boundary, then collect the groups keyed to this token. */` |
|   3824715 | 2211 | `	SySetReset(&pGen->aPendingAttrs);` |
|  11475895 | 2212 | `	for( n = 0 ; n < nT ; n++ ){` |
|   7651185 | 2213 | `		if( aT[n].nTokIdx != nIdx ){` |
|   7641949 | 2214 | `			continue;` |
|         - | 2215 | `		}` |
|      9241 | 2216 | `		if( aT[n].iKind == PH7_TRIVIA_DOC ){` |
|        29 | 2217 | `			pGen->sPendingDoc = aT[n].sText;` |
|      9229 | 2218 | `		}else if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|      9217 | 2219 | `			SySetPut(&pGen->aPendingAttrs,(const void *)&aT[n]);` |
|      4606 | 2220 | `		}` |
|      4623 | 2221 | `	}` |
|  10520568 | 2222 | `}` |
|         - | 2223 | `/*` |
|         - | 2224 | ` * Hand the pending docblock (if any) to a declaration: duplicate it into` |
|         - | 2225 | ` * the VM allocator (the raw script buffer dies after compilation) and` |
|         - | 2226 | ` * clear the pending slot so sibling declarations do not inherit it.` |
|         - | 2227 | ` */` |
|   5571436 | 2228 | `PH7_PRIVATE void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut)` |
|         5 | 2229 | `{` |
|         - | 2230 | `	char *zDup;` |
|   5571441 | 2231 | `	if( SyStringLength(&pGen->sPendingDoc) < 1 ){` |
|   5571421 | 2232 | `		return;` |
|         - | 2233 | `	}` |
|        35 | 2234 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        10 | 2235 | `		SyStringData(&pGen->sPendingDoc),SyStringLength(&pGen->sPendingDoc));` |
|        25 | 2236 | `	if( zDup ){` |
|        25 | 2237 | `		SyStringInitFromBuf(pOut,zDup,SyStringLength(&pGen->sPendingDoc));` |
|        10 | 2238 | `	}` |
|        25 | 2239 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|   2785723 | 2240 | `}` |
|         - | 2241 | `/*` |
|         - | 2242 | ` * Compile one recorded #[...] attribute group (the span between the group` |
|         - | 2243 | ` * delimiters) into ph7_attribute records appended to pOut. The span is` |
|         - | 2244 | ` * duplicated into the VM allocator FIRST (compiled bytecode and interned` |
|         - | 2245 | ` * names may point into the token text, which must outlive the raw script` |
|         - | 2246 | ` * buffer), then re-tokenized on its own. Each argument expression compiles` |
|         - | 2247 | ` * with the container-swap idiom into its own OP_DONE-terminated set,` |
|         - | 2248 | ` * evaluated lazily at ReflectionAttribute time (PHP semantics).` |
|         - | 2249 | ` */` |
|      9226 | 2250 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut)` |
|         5 | 2251 | `{` |
|         - | 2252 | `	SySet *pToken;` |
|         - | 2253 | `	SyToken *pIn, *pEnd, *pSavedIn, *pSavedEnd;` |
|         - | 2254 | `	char *zSpan;` |
|      9231 | 2255 | `	sxi32 rc = SXRET_OK;` |
|      9231 | 2256 | `	if( SyStringLength(&pTrivia->sText) < 1 ){` |
|       ! 0 | 2257 | `		return SXRET_OK;` |
|         - | 2258 | `	}` |
|     13844 | 2259 | `	zSpan = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      4613 | 2260 | `		SyStringData(&pTrivia->sText),SyStringLength(&pTrivia->sText));` |
|      9231 | 2261 | `	if( zSpan == 0 ){` |
|       ! 0 | 2262 | `		return SXRET_OK;` |
|         - | 2263 | `	}` |
|         - | 2264 | `	/* The token set must outlive compilation too: interned operands may` |
|         - | 2265 | `	 * reference token payloads. Pool-allocated, never released — bounded by` |
|         - | 2266 | `	 * the number of attribute declarations in the program. */` |
|      9231 | 2267 | `	pToken = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|      9231 | 2268 | `	if( pToken == 0 ){` |
|       ! 0 | 2269 | `		return SXRET_OK;` |
|         - | 2270 | `	}` |
|      9231 | 2271 | `	SySetInit(pToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|      9231 | 2272 | `	PH7_TokenizePHP(zSpan,SyStringLength(&pTrivia->sText),pTrivia->nLine,pToken,0);` |
|      9231 | 2273 | `	pIn = (SyToken *)SySetBasePtr(pToken);` |
|      9231 | 2274 | `	pEnd = &pIn[SySetUsed(pToken)];` |
|      9231 | 2275 | `	pSavedIn = pGen->pIn;` |
|      9231 | 2276 | `	pSavedEnd = pGen->pEnd;` |
|      9235 | 2277 | `	while( pIn < pEnd ){` |
|         - | 2278 | `		ph7_attribute sAttr;` |
|         - | 2279 | `		SyBlob sFQN;` |
|      9235 | 2280 | `		int bAbsolute = 0;` |
|      9235 | 2281 | `		SyZero(&sAttr,sizeof(sAttr));` |
|      9235 | 2282 | `		SySetInit(&sAttr.aArgs,&pGen->pVm->sAllocator,sizeof(ph7_attr_arg));` |
|      9235 | 2283 | `		sAttr.nLine = pIn->nLine;` |
|      9235 | 2284 | `		if( pIn->nType & PH7_TK_NSSEP ){` |
|        77 | 2285 | `			bAbsolute = 1;` |
|        77 | 2286 | `			pIn++;` |
|        36 | 2287 | `		}` |
|      9235 | 2288 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|         - | 2289 | ``		/* `#[namespace\Attr]` — the current namespace, absolute from there. */`` |
|      9235 | 2290 | `		if( !bAbsolute && GenStateNsRelPrefix(pGen,&pIn,pEnd,&sFQN) ){` |
|       ! 0 | 2291 | `			bAbsolute = 1;` |
|       ! 0 | 2292 | `		}` |
|      9235 | 2293 | `		while( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|      9235 | 2294 | `			SyBlobAppend(&sFQN,pIn->sData.zString,pIn->sData.nByte);` |
|      9235 | 2295 | `			pIn++;` |
|      9235 | 2296 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|       ! 0 | 2297 | `				SyBlobAppend(&sFQN,"\\",1);` |
|       ! 0 | 2298 | `				pIn++;` |
|       ! 0 | 2299 | `				continue;` |
|         - | 2300 | `			}` |
|      9235 | 2301 | `			break;` |
|       ! 0 | 2302 | `		}` |
|      9235 | 2303 | `		if( SyBlobLength(&sFQN) < 1 ){` |
|         - | 2304 | `			/* Malformed group: stop quietly (the group was inert trivia before` |
|         - | 2305 | `			 * this feature; never turn it into a new fatal) */` |
|       ! 0 | 2306 | `			SyBlobRelease(&sFQN);` |
|       ! 0 | 2307 | `			break;` |
|         - | 2308 | `		}` |
|         - | 2309 | `		/* Resolve to an FQN: absolute names verbatim; else use-import alias,` |
|         - | 2310 | `		 * else current-namespace prefix (PHP attribute name resolution) */` |
|         - | 2311 | `		{` |
|      9235 | 2312 | `			const char *zName = (const char *)SyBlobData(&sFQN);` |
|      9235 | 2313 | `			sxu32 nName = SyBlobLength(&sFQN);` |
|      9235 | 2314 | `			char *zDup = 0;` |
|      9235 | 2315 | `			if( !bAbsolute ){` |
|      9163 | 2316 | `				SyHashEntry *pImp = SyHashGet(&pGen->hUseImports,(const void *)zName,nName);` |
|      9163 | 2317 | `				if( pImp ){` |
|         3 | 2318 | `					const char *zFqn = (const char *)pImp->pUserData;` |
|         3 | 2319 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zFqn,SyStrlen(zFqn));` |
|         3 | 2320 | `					if( zDup ){` |
|         3 | 2321 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyStrlen(zDup));` |
|         2 | 2322 | `					}` |
|      9162 | 2323 | `				}else if( SyBlobLength(&pGen->sNamespace) > 0 ){` |
|         - | 2324 | `					SyBlob sTmp;` |
|       ! 0 | 2325 | `					SyBlobInit(&sTmp,&pGen->pVm->sAllocator);` |
|       ! 0 | 2326 | `					SyBlobAppend(&sTmp,SyBlobData(&pGen->sNamespace),SyBlobLength(&pGen->sNamespace));` |
|       ! 0 | 2327 | `					SyBlobAppend(&sTmp,"\\",1);` |
|       ! 0 | 2328 | `					SyBlobAppend(&sTmp,zName,nName);` |
|       ! 0 | 2329 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       ! 0 | 2330 | `						(const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|       ! 0 | 2331 | `					if( zDup ){` |
|       ! 0 | 2332 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyBlobLength(&sTmp));` |
|       ! 0 | 2333 | `					}` |
|       ! 0 | 2334 | `					SyBlobRelease(&sTmp);` |
|       ! 0 | 2335 | `				}` |
|      4579 | 2336 | `			}` |
|      9235 | 2337 | `			if( SyStringLength(&sAttr.sName) < 1 ){` |
|      9233 | 2338 | `				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|      9233 | 2339 | `				if( zDup ){` |
|      9233 | 2340 | `					SyStringInitFromBuf(&sAttr.sName,zDup,nName);` |
|      4614 | 2341 | `				}` |
|      4614 | 2342 | `			}` |
|         - | 2343 | `		}` |
|      9235 | 2344 | `		SyBlobRelease(&sFQN);` |
|      9235 | 2345 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 2346 | `			SyToken *pArgsEnd;` |
|      9127 | 2347 | `			pIn++;` |
|      9127 | 2348 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pArgsEnd);` |
|     18263 | 2349 | `			while( pIn < pArgsEnd ){` |
|      9141 | 2350 | `				SyToken *pArgStart = pIn, *pArgStop = pIn;` |
|      9141 | 2351 | `				sxi32 iDepth = 0;` |
|         - | 2352 | `				ph7_attr_arg sArgRec;` |
|     90841 | 2353 | `				while( pArgStop < pArgsEnd ){` |
|     81721 | 2354 | `					if( pArgStop->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        11 | 2355 | `						iDepth++;` |
|     81716 | 2356 | `					}else if( pArgStop->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        11 | 2357 | `						iDepth--;` |
|     81706 | 2358 | `					}else if( (pArgStop->nType & PH7_TK_COMMA) && iDepth == 0 ){` |
|        17 | 2359 | `						break;` |
|         - | 2360 | `					}` |
|     81705 | 2361 | `					pArgStop++;` |
|         5 | 2362 | `				}` |
|      9141 | 2363 | `				SyZero(&sArgRec,sizeof(sArgRec));` |
|      9141 | 2364 | `				SySetInit(&sArgRec.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|      9136 | 2365 | `				if( pArgStart < pArgStop && (pArgStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|      9117 | 2366 | `				 && &pArgStart[1] < pArgStop && (pArgStart[1].nType & PH7_TK_COLON) ){` |
|        28 | 2367 | `					char *zN = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|         9 | 2368 | `						pArgStart->sData.zString,pArgStart->sData.nByte);` |
|        19 | 2369 | `					if( zN ){` |
|        19 | 2370 | `						SyStringInitFromBuf(&sArgRec.sName,zN,pArgStart->sData.nByte);` |
|         9 | 2371 | `					}` |
|        19 | 2372 | `					pArgStart += 2;` |
|         9 | 2373 | `				}` |
|      9141 | 2374 | `				if( pArgStart < pArgStop ){` |
|         - | 2375 | `					SySet *pInstrContainer;` |
|      9141 | 2376 | `					pGen->pIn = pArgStart;` |
|      9141 | 2377 | `					pGen->pEnd = pArgStop;` |
|      9141 | 2378 | `					pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|      9141 | 2379 | `					PH7_VmSetByteCodeContainer(pGen->pVm,&sArgRec.aByteCode);` |
|      9141 | 2380 | `					rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|      9141 | 2381 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|      9141 | 2382 | `					PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|      9141 | 2383 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 2384 | `						pGen->pIn = pSavedIn;` |
|       ! 0 | 2385 | `						pGen->pEnd = pSavedEnd;` |
|       ! 0 | 2386 | `						return SXERR_ABORT;` |
|         - | 2387 | `					}` |
|      9141 | 2388 | `					SySetPut(&sAttr.aArgs,(const void *)&sArgRec);` |
|      4568 | 2389 | `				}` |
|      9141 | 2390 | `				pIn = pArgStop;` |
|      9141 | 2391 | `				if( pIn < pArgsEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        17 | 2392 | `					pIn++;` |
|         8 | 2393 | `				}` |
|         5 | 2394 | `			}` |
|      9127 | 2395 | `			pIn = (pArgsEnd < pEnd) ? &pArgsEnd[1] : pEnd;` |
|      4561 | 2396 | `		}` |
|      9235 | 2397 | `		SySetPut(pOut,(const void *)&sAttr);` |
|      9235 | 2398 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|         5 | 2399 | `			pIn++;` |
|         5 | 2400 | `			continue;` |
|         - | 2401 | `		}` |
|      9231 | 2402 | `		break;` |
|       ! 0 | 2403 | `	}` |
|      9231 | 2404 | `	pGen->pIn = pSavedIn;` |
|      9231 | 2405 | `	pGen->pEnd = pSavedEnd;` |
|      9231 | 2406 | `	return SXRET_OK;` |
|      4618 | 2407 | `}` |
|         - | 2408 | `/*` |
|         - | 2409 | ` * Hand the pending attribute groups (if any) to a declaration: compile` |
|         - | 2410 | ` * every recorded group into pOut and clear the pending list.` |
|         - | 2411 | ` */` |
|   5571442 | 2412 | `PH7_PRIVATE sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut)` |
|         5 | 2413 | `{` |
|   5571447 | 2414 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|         - | 2415 | `	sxu32 n;` |
|         - | 2416 | `	sxi32 rc;` |
|   5580659 | 2417 | `	for( n = 0 ; n < SySetUsed(&pGen->aPendingAttrs) ; n++ ){` |
|      9217 | 2418 | `		rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|      9217 | 2419 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 2420 | `			return SXERR_ABORT;` |
|         - | 2421 | `		}` |
|      4611 | 2422 | `	}` |
|   5571447 | 2423 | `	SySetReset(&pGen->aPendingAttrs);` |
|   5571447 | 2424 | `	return SXRET_OK;` |
|   2785726 | 2425 | `}` |
|         - | 2426 | `/*` |
|         - | 2427 | ` * Compile the attribute groups keyed to the given token (a parameter's` |
|         - | 2428 | ` * first token inside a signature) into pOut. Parameters are parsed from` |
|         - | 2429 | ` * the main token stream, so the sidecar indexes map directly.` |
|         - | 2430 | ` */` |
|   2850546 | 2431 | `PH7_PRIVATE sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut)` |
|         5 | 2432 | `{` |
|   2850551 | 2433 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|   2850551 | 2434 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|   2850551 | 2435 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 2436 | `	sxu32 nIdx, n;` |
|         - | 2437 | `	sxi32 rc;` |
|   2850546 | 2438 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|    634243 | 2439 | `	 \|\| pTok < pBase \|\| pTok >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|   2216313 | 2440 | `		return SXRET_OK;` |
|         - | 2441 | `	}` |
|    634243 | 2442 | `	nIdx = (sxu32)(pTok - pBase);` |
|   1902763 | 2443 | `	for( n = 0 ; n < nT ; n++ ){` |
|   1268525 | 2444 | `		if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|        16 | 2445 | `			rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|        16 | 2446 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 2447 | `				return SXERR_ABORT;` |
|         - | 2448 | `			}` |
|         7 | 2449 | `		}` |
|    634265 | 2450 | `	}` |
|    634243 | 2451 | `	return SXRET_OK;` |
|   1425278 | 2452 | `}` |
|  15526000 | 2453 | `PH7_PRIVATE sxi32 GenStateCompileChunk(` |
|         - | 2454 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 2455 | `	sxi32 iFlags         /* Compile flags */` |
|         - | 2456 | `	)` |
|         5 | 2457 | `{` |
|         - | 2458 | `	ProcLangConstruct xCons;` |
|         - | 2459 | `	sxi32 rc;` |
|  15526005 | 2460 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|   9014226 | 2461 | `	for(;;){` |
|  16777231 | 2462 | `		int bStmtIsDeclare = 0;` |
|  16777231 | 2463 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 2464 | `			/* No more input to process */` |
|    105613 | 2465 | `			break;` |
|         - | 2466 | `		}` |
|         - | 2467 | `		/* Bind a directly-preceding docblock to this statement */` |
|  16671623 | 2468 | `		GenStateSetPendingDoc(&(*pGen));` |
|  16671623 | 2469 | `		if( SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|         - | 2470 | `			/* php: a statement-position attribute group must be followed by a` |
|         - | 2471 | ``			 * declaration (function/class-like/const) — `#[A] $x = 1;` is a`` |
|         - | 2472 | `` 			 * parse error, never a silent discard. `static`/`fn`/`function` `` |
|         - | 2473 | ``			 * cover bare closure-expression statements; `readonly`/`enum` are`` |
|         - | 2474 | `			 * context-sensitive IDs handled by the modified-class/enum scans. */` |
|      9129 | 2475 | `			int bAttrTarget = 0;` |
|      9124 | 2476 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd)` |
|      4601 | 2477 | `			 \|\| GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|      9061 | 2478 | `				bAttrTarget = 1;` |
|      4599 | 2479 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|        71 | 2480 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|        68 | 2481 | `				if( nKw == PH7_TKWRD_FUNCTION \|\| nKw == PH7_TKWRD_CLASS` |
|        21 | 2482 | `				 \|\| nKw == PH7_TKWRD_INTERFACE \|\| nKw == PH7_TKWRD_TRAIT` |
|         6 | 2483 | `				 \|\| nKw == PH7_TKWRD_ABSTRACT \|\| nKw == PH7_TKWRD_FINAL` |
|         6 | 2484 | `				 \|\| nKw == PH7_TKWRD_CONST \|\| nKw == PH7_TKWRD_STATIC` |
|         3 | 2485 | `				 \|\| nKw == PH7_TKWRD_FN ){` |
|        71 | 2486 | `					bAttrTarget = 1;` |
|        34 | 2487 | `				}` |
|        34 | 2488 | `			}` |
|      9129 | 2489 | `			if( !bAttrTarget ){` |
|       ! 0 | 2490 | `				rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 2491 | `					"syntax error, unexpected token \"%z\" after attribute group; expecting a declaration",` |
|       ! 0 | 2492 | `					&pGen->pIn->sData);` |
|       ! 0 | 2493 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 2494 | `					break;` |
|         - | 2495 | `				}` |
|       ! 0 | 2496 | `				SySetReset(&pGen->aPendingAttrs);` |
|       ! 0 | 2497 | `			}` |
|      4562 | 2498 | `		}` |
|         - | 2499 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|         - | 2500 | `		 * below doesn't fire before the directive has a chance to run. */` |
|  16671623 | 2501 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   9844079 | 2502 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   9844079 | 2503 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|        55 | 2504 | `				bStmtIsDeclare = 1;` |
|        25 | 2505 | `			}` |
|   4922037 | 2506 | `		}` |
|  16671623 | 2507 | `		if( !bStmtIsDeclare && pGen->pCurrent == &pGen->sGlobal ){` |
|         - | 2508 | `			/* Any non-declare top-level statement locks the strict_types` |
|         - | 2509 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|   1251255 | 2510 | `			pGen->bStrictTypesLocked = 1;` |
|    625625 | 2511 | `		}` |
|  16671623 | 2512 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 2513 | `			/* Compile block */` |
|      4583 | 2514 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|      4583 | 2515 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 2516 | `				break;` |
|         - | 2517 | `			}` |
|      2294 | 2518 | `		}else{` |
|  16667045 | 2519 | `			xCons = 0;` |
|  16667045 | 2520 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|         - | 2521 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|         - | 2522 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|         - | 2523 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|     49933 | 2524 | `				xCons = PH7_CompileClassModifiers;` |
|  16642081 | 2525 | `			}else if( GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|         - | 2526 | ``				/* `enum Name …` (PHP 8.1) — `enum` is a context-sensitive ID,`` |
|         - | 2527 | `				 * so it is detected here rather than the keyword dispatcher. */` |
|      4595 | 2528 | `				xCons = PH7_CompileEnum;` |
|  16614822 | 2529 | `			}else if( GenStateIsNsRelName(pGen->pIn,pGen->pEnd) ){` |
|         - | 2530 | ``				/* A statement that STARTS with php's `namespace\X` name operator`` |
|         - | 2531 | ``				 * (`namespace\Cee::m();`) is an expression, not a namespace`` |
|         - | 2532 | ``				 * DECLARATION — the glued `\` is what tells the two apart. */`` |
|         7 | 2533 | `				xCons = 0;` |
|  16612524 | 2534 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   9794171 | 2535 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         - | 2536 | `				/* Try to extract a language construct handler */` |
|   9794171 | 2537 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|   9794171 | 2538 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|        13 | 2539 | `					rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 2540 | `						"Syntax error: Unexpected keyword '%z'",` |
|         8 | 2541 | `						&pGen->pIn->sData);` |
|         9 | 2542 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 2543 | `						break;` |
|         - | 2544 | `					}` |
|         - | 2545 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|         - | 2546 | `					 * this erroneous statement.` |
|         - | 2547 | `					 */` |
|         9 | 2548 | `					xCons = PH7_ErrorRecover;` |
|         4 | 2549 | `				}` |
|  11715438 | 2550 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|    512051 | 2551 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|         - | 2552 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|       217 | 2553 | `				xCons = PH7_CompileLabel;` |
|       106 | 2554 | `			}` |
|  16667045 | 2555 | `			if( xCons == 0 ){` |
|         - | 2556 | `				/* Assume an expression an try to compile it */` |
|   6886667 | 2557 | `				rc = PH7_CompileExpr(&(*pGen),0,0);` |
|   6886667 | 2558 | `				if(  rc != SXERR_EMPTY ){` |
|         - | 2559 | `					/* Pop l-value */` |
|   6886507 | 2560 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|   3443251 | 2561 | `				}` |
|   3443336 | 2562 | `			}else{` |
|         - | 2563 | `				/* Go compile the sucker */` |
|   9780383 | 2564 | `				rc = xCons(&(*pGen));` |
|         - | 2565 | `			}` |
|  16667045 | 2566 | `			if( rc == SXERR_ABORT ){` |
|         - | 2567 | `				/* Request to abort compilation */` |
|        79 | 2568 | `				break;` |
|         - | 2569 | `			}` |
|         - | 2570 | `		}` |
|         - | 2571 | `		/* Ignore trailing semi-colons ';' */` |
|  28622639 | 2572 | `		while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|  11951095 | 2573 | `			pGen->pIn++;` |
|         5 | 2574 | `		}` |
|  16671549 | 2575 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|         - | 2576 | `			/* Compile a single statement and return */` |
|  15420323 | 2577 | `			break;` |
|         - | 2578 | `		}` |
|         - | 2579 | `		/* LOOP ONE */` |
|         - | 2580 | `		/* LOOP TWO */` |
|         - | 2581 | `		/* LOOP THREE */` |
|         - | 2582 | `		/* LOOP FOUR */` |
|         5 | 2583 | `	}` |
|         - | 2584 | `	/* Return compilation status */` |
|  15526005 | 2585 | `	return rc;` |
|         5 | 2586 | `}` |
|         - | 2587 | `/*` |
|         - | 2588 | ` * Compile a Raw PHP chunk.` |
|         - | 2589 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 2590 | ` * takes care of generating the appropriate error message.` |
|         - | 2591 | ` */` |
|    105684 | 2592 | `static sxi32 PH7_CompilePHP(` |
|         - | 2593 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 2594 | `	SySet *pTokenSet,     /* Token set */` |
|         - | 2595 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|         - | 2596 | `	)` |
|         5 | 2597 | `{` |
|    105689 | 2598 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|         - | 2599 | `	sxi32 rc;` |
|         - | 2600 | `	/* Reset the token set (and its trivia sidecar) */` |
|    105689 | 2601 | `	SySetReset(&(*pTokenSet));` |
|    105689 | 2602 | `	SySetReset(&pGen->aTrivia);` |
|         - | 2603 | `	/* Mark as the default token set */` |
|    105689 | 2604 | `	pGen->pTokenSet = &(*pTokenSet);` |
|         - | 2605 | `	/* Advance the stream cursor */` |
|    105689 | 2606 | `	pGen->pRawIn++;` |
|         - | 2607 | `	/* Tokenize the PHP chunk first */` |
|    105689 | 2608 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet),&pGen->aTrivia);` |
|         - | 2609 | `	/* Point to the head and tail of the token stream. */` |
|    105689 | 2610 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|    105689 | 2611 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|    105689 | 2612 | `	if( is_expr ){` |
|       ! 0 | 2613 | `		rc = SXERR_EMPTY;` |
|       ! 0 | 2614 | `		if( pGen->pIn < pGen->pEnd ){` |
|         - | 2615 | `			/* A simple expression,compile it */` |
|       ! 0 | 2616 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|       ! 0 | 2617 | `		}` |
|         - | 2618 | `		/* Emit the DONE instruction */` |
|       ! 0 | 2619 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       ! 0 | 2620 | `		return SXRET_OK;` |
|         - | 2621 | `	}` |
|    105689 | 2622 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|         - | 2623 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|         - | 2624 | `		/*` |
|         - | 2625 | `		 * Shortcut syntax for the 'echo' language construct.` |
|         - | 2626 | `		 * According to the PHP reference manual:` |
|         - | 2627 | `		 *  echo() also has a shortcut syntax, where you can` |
|         - | 2628 | `		 *  immediately follow` |
|         - | 2629 | `		 *  the opening tag with an equals sign as follows:` |
|         - | 2630 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|         - | 2631 | `		 * Symisc extension:` |
|         - | 2632 | `		 *   This short syntax works with all PHP opening` |
|         - | 2633 | `		 *   tags unlike the default PHP engine that handle` |
|         - | 2634 | `		 *   only short tag.` |
|         - | 2635 | `		 */` |
|         - | 2636 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|         3 | 2637 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|         3 | 2638 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|         3 | 2639 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|         - | 2640 | `		/* This synthesized echo is compiled as an EXPRESSION, which is otherwise a` |
|         - | 2641 | `		 * parse error; allow it for the duration of this one compile. */` |
|         3 | 2642 | `		pGen->nExprEchoOk++;` |
|         3 | 2643 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|         3 | 2644 | `		pGen->nExprEchoOk--;` |
|         3 | 2645 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 2646 | `			return SXERR_ABORT;` |
|         - | 2647 | `		}` |
|         3 | 2648 | `		if( rc != SXERR_EMPTY ){` |
|         3 | 2649 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         1 | 2650 | `		}` |
|         3 | 2651 | `		return SXRET_OK;` |
|         - | 2652 | `	}` |
|         - | 2653 | `	/* Compile the PHP chunk */` |
|    105687 | 2654 | `	rc = GenStateCompileChunk(pGen,0);` |
|         - | 2655 | `	/* Fix exceptions jumps */` |
|    105687 | 2656 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - | 2657 | `	/* Fix gotos now, the jump destination is resolved */` |
|    105687 | 2658 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|         3 | 2659 | `		rc = SXERR_ABORT;` |
|         1 | 2660 | `	}` |
|         - | 2661 | `	/* Reset container */` |
|    105687 | 2662 | `	SySetReset(&pGen->aGoto);` |
|    105687 | 2663 | `	SySetReset(&pGen->aLabel);` |
|    105687 | 2664 | `	SySetReset(&pGen->aNullsafeJmp);` |
|         - | 2665 | `	/* Compilation result */` |
|    105687 | 2666 | `	return rc;` |
|     52847 | 2667 | `}` |
|         - | 2668 | `/*` |
|         - | 2669 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|         - | 2670 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|         - | 2671 | ` * This is the only compile interface exported from this file.` |
|         - | 2672 | ` */` |
|    109214 | 2673 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|         - | 2674 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|         - | 2675 | `	SyString *pScript,  /* Script to compile */` |
|         - | 2676 | `	sxi32 iFlags        /* Compile flags */` |
|         - | 2677 | `	)` |
|         5 | 2678 | `{` |
|         - | 2679 | `	SySet aPhpToken,aRawToken;` |
|         - | 2680 | `	ph7_gen_state *pCodeGen;` |
|         - | 2681 | `	ph7_value *pRawObj;` |
|         - | 2682 | `	sxu32 nObjIdx;` |
|         - | 2683 | `	sxi32 nRawObj;` |
|         - | 2684 | `	int is_expr;` |
|         - | 2685 | `	sxi8 bSavedStrict;` |
|         - | 2686 | `	sxi8 bSavedStrictLocked;` |
|         - | 2687 | `	SyToken *pSavedIn,*pSavedEnd;` |
|         - | 2688 | `	sxi32 rc;` |
|    109219 | 2689 | `	sxu32 nBaseLine = 1;` |
|    109219 | 2690 | `	if( pScript->nByte < 1 ){` |
|         - | 2691 | `		/* Nothing to compile */` |
|       ! 0 | 2692 | `		return PH7_OK;` |
|         - | 2693 | `	}` |
|         - | 2694 | `	/* php skips a "#!" shebang on the first line of a CLI script: consume it` |
|         - | 2695 | `	 * (including its newline) so it is not echoed as inline text, and bump the` |
|         - | 2696 | `	 * base line to 2 so the code below still reports php-matching line numbers. */` |
|    109219 | 2697 | `	if( pScript->nByte >= 2 && pScript->zString[0]=='#' && pScript->zString[1]=='!' ){` |
|         3 | 2698 | `		const char *z = pScript->zString;` |
|         3 | 2699 | `		const char *zEnd = &z[pScript->nByte];` |
|        39 | 2700 | `		while( z < zEnd && z[0] != '\n' ){ z++; }` |
|         3 | 2701 | `		if( z < zEnd ){ z++; } /* consume the newline too */` |
|         3 | 2702 | `		pScript->nByte -= (sxu32)(z - pScript->zString);` |
|         3 | 2703 | `		pScript->zString = z;` |
|         3 | 2704 | `		nBaseLine = 2;` |
|         3 | 2705 | `		if( pScript->nByte < 1 ){` |
|       ! 0 | 2706 | `			return PH7_OK;` |
|         - | 2707 | `		}` |
|         1 | 2708 | `	}` |
|         - | 2709 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|         - | 2710 | `	 * file's flags so include/require restore them on return. */` |
|    109219 | 2711 | `	pCodeGen = &pVm->sCodeGen;` |
|         - | 2712 | ``	/* aPhpToken below is a LOCAL token set: once it is released at `cleanup`, any`` |
|         - | 2713 | `	 * pGen->pIn/pEnd still pointing into it DANGLE. PH7_VmEmitInstr reads pIn to stamp` |
|         - | 2714 | `	 * each instruction's source line, and instructions are still emitted after this` |
|         - | 2715 | `	 * function returns (VmEvalChunk's trailing OP_DONE) -- a use-after-free ASan caught` |
|         - | 2716 | `	 * immediately. Save the caller's cursor and restore it on the way out, so an` |
|         - | 2717 | `	 * enclosing compile keeps its (live) tokens and the top level goes back to NULL. */` |
|    109219 | 2718 | `	pSavedIn = pCodeGen->pIn;` |
|    109219 | 2719 | `	pSavedEnd = pCodeGen->pEnd;` |
|    109219 | 2720 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|    109219 | 2721 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|    109219 | 2722 | `	pCodeGen->bStrictTypes = 0;` |
|    109219 | 2723 | `	pCodeGen->bStrictTypesLocked = 0;` |
|         - | 2724 | `	/* Initialize the tokens containers */` |
|    109219 | 2725 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|    109219 | 2726 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|    109219 | 2727 | `	SySetAlloc(&aPhpToken,0xc0);` |
|    109219 | 2728 | `	is_expr = 0;` |
|    109219 | 2729 | `	if( iFlags & PH7_PHP_ONLY ){` |
|         - | 2730 | `		SyToken sTmp;` |
|         - | 2731 | `		/* PHP only: -*/` |
|     95349 | 2732 | `		sTmp.nLine = 1;` |
|     95349 | 2733 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|     95349 | 2734 | `		sTmp.pUserData = 0;` |
|     95349 | 2735 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|     95349 | 2736 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|     95349 | 2737 | `		if( iFlags & PH7_PHP_EXPR ){` |
|         - | 2738 | `			/* A simple PHP expression */` |
|       ! 0 | 2739 | `			is_expr = 1;` |
|       ! 0 | 2740 | `		}` |
|     47677 | 2741 | `	}else{` |
|         - | 2742 | `		/* Tokenize raw text */` |
|     13875 | 2743 | `		SySetAlloc(&aRawToken,32);` |
|     13875 | 2744 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken,nBaseLine);` |
|         - | 2745 | `	}` |
|         - | 2746 | `	/* Process high-level tokens */` |
|    109219 | 2747 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|    109219 | 2748 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|    109219 | 2749 | `	rc = PH7_OK;` |
|    109219 | 2750 | `	if( is_expr ){` |
|         - | 2751 | `		/* Compile the expression */` |
|       ! 0 | 2752 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|       ! 0 | 2753 | `		goto cleanup;` |
|         - | 2754 | `	}` |
|    109219 | 2755 | `	nObjIdx = 0;` |
|         - | 2756 | `	/* Start the compilation process */` |
|     61546 | 2757 | `	for(;;){` |
|    228705 | 2758 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|    109143 | 2759 | `			break; /* No more tokens to process */` |
|         - | 2760 | `		}` |
|    119567 | 2761 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|         - | 2762 | `			/* Compile the PHP chunk */` |
|    105689 | 2763 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|    105689 | 2764 | `			if( rc == SXERR_ABORT ){` |
|        81 | 2765 | `				break;` |
|         - | 2766 | `			}` |
|    105613 | 2767 | `			continue;` |
|         - | 2768 | `		}` |
|         - | 2769 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|     13883 | 2770 | `		nRawObj = 0;` |
|     27761 | 2771 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|         - | 2772 | `			/* Consume the raw chunk without any processing */` |
|     13883 | 2773 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|     13883 | 2774 | `			if( pRawObj == 0 ){` |
|       ! 0 | 2775 | `				rc = SXERR_MEM;` |
|       ! 0 | 2776 | `				break;` |
|         - | 2777 | `			}` |
|         - | 2778 | `			/* Mark as constant and emit the load constant instruction */` |
|     13883 | 2779 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|     13883 | 2780 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     13883 | 2781 | `			++nRawObj;` |
|     13883 | 2782 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|         5 | 2783 | `		}` |
|     13883 | 2784 | `		if( nRawObj > 0 ){` |
|         - | 2785 | `			/* Emit the consume instruction */` |
|     13883 | 2786 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|      6939 | 2787 | `		}` |
|     54612 | 2788 | `	}` |
|     54607 | 2789 | `cleanup:` |
|         - | 2790 | `	/* Drop the cursor into the token set BEFORE the set is freed (see above). */` |
|    109219 | 2791 | `	pCodeGen->pIn = pSavedIn;` |
|    109219 | 2792 | `	pCodeGen->pEnd = pSavedEnd;` |
|    109219 | 2793 | `	SySetRelease(&aRawToken);` |
|    109219 | 2794 | `	SySetRelease(&aPhpToken);` |
|         - | 2795 | `	/* Restore outer file's strict_types scope */` |
|    109219 | 2796 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|    109219 | 2797 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|    109219 | 2798 | `	return rc;` |
|     54612 | 2799 | `}` |
|         - | 2800 | `/*` |
|         - | 2801 | ` * Utility routines.Initialize the code generator.` |
|         - | 2802 | ` */` |
|      4528 | 2803 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|         - | 2804 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 2805 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 2806 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 2807 | `	)` |
|         5 | 2808 | `{` |
|      4533 | 2809 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 2810 | `	/* Zero the structure */` |
|      4533 | 2811 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|         - | 2812 | `	/* Initial state */` |
|      4533 | 2813 | `	pGen->pVm  = &(*pVm);` |
|      4533 | 2814 | `	pGen->xErr = xErr;` |
|      4533 | 2815 | `	pGen->pErrData = pErrData;` |
|      4533 | 2816 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|      4533 | 2817 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|      4533 | 2818 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|      4533 | 2819 | `	SySetInit(&pGen->aScope,&pVm->sAllocator,sizeof(GenScope));` |
|      4533 | 2820 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|      4533 | 2821 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|      4533 | 2822 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      4533 | 2823 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      4533 | 2824 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|      4533 | 2825 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|         - | 2826 | `	/* Error log buffer */` |
|      4533 | 2827 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|         - | 2828 | `	/* General purpose working buffer */` |
|      4533 | 2829 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|         - | 2830 | `	/* Namespace state */` |
|      4533 | 2831 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|      4533 | 2832 | `	GenStateInitUseImports(&(*pGen),&(*pVm));` |
|      4533 | 2833 | `	GenStateInitSeenSymbols(&(*pGen),&(*pVm));` |
|         - | 2834 | `	/* Create the global scope */` |
|      4533 | 2835 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|         - | 2836 | `	/* Point to the global scope */` |
|      4533 | 2837 | `	pGen->pCurrent = &pGen->sGlobal;` |
|      4533 | 2838 | `	return SXRET_OK;` |
|         5 | 2839 | `}` |
|         - | 2840 | `/*` |
|         - | 2841 | ` * Utility routines. Reset the code generator to it's initial state.` |
|         - | 2842 | ` */` |
|    113166 | 2843 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|         - | 2844 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 2845 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 2846 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 2847 | `	)` |
|         5 | 2848 | `{` |
|    113171 | 2849 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 2850 | `	GenBlock *pBlock,*pParent;` |
|         - | 2851 | `	/* Reset state */` |
|    113171 | 2852 | `	SySetReset(&pGen->aLabel);` |
|    113171 | 2853 | `	SySetReset(&pGen->aGoto);` |
|    113171 | 2854 | `	SySetReset(&pGen->aNullsafeJmp);` |
|    113171 | 2855 | `	SySetReset(&pGen->aTrivia);` |
|    113171 | 2856 | `	SySetReset(&pGen->aPendingAttrs);` |
|    113171 | 2857 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|    113171 | 2858 | `	SyBlobRelease(&pGen->sErrBuf);` |
|    113171 | 2859 | `	SyBlobRelease(&pGen->sWorker);` |
|    113171 | 2860 | `	SyBlobRelease(&pGen->sNamespace);` |
|    113171 | 2861 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|    113171 | 2862 | `	GenStateResetUseImports(&(*pGen),&(*pVm));` |
|         - | 2863 | `	/* A fresh compile unit has declared nothing yet. */` |
|    113171 | 2864 | `	GenStateResetSeenSymbols(&(*pGen),&(*pVm));` |
|         - | 2865 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|         - | 2866 | `	 * They intern variable names and literal strings that are referenced by` |
|         - | 2867 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|         - | 2868 | `	 * Releasing them would either leak the interned strings or require freeing` |
|         - | 2869 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|         - | 2870 | `	 * number of unique names, which is acceptable. */` |
|         - | 2871 | `	/* Point to the global scope */` |
|    113171 | 2872 | `	pBlock = pGen->pCurrent;` |
|    113171 | 2873 | `	while( pBlock->pParent != 0 ){` |
|       ! 0 | 2874 | `		pParent = pBlock->pParent;` |
|       ! 0 | 2875 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 2876 | `		pBlock = pParent;` |
|       ! 0 | 2877 | `	}` |
|    113171 | 2878 | `	pGen->xErr = xErr;` |
|    113171 | 2879 | `	pGen->pErrData = pErrData;` |
|    113171 | 2880 | `	pGen->pCurrent = &pGen->sGlobal;` |
|    113171 | 2881 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|    113171 | 2882 | `	pGen->pIn = pGen->pEnd = 0;` |
|    113171 | 2883 | `	pGen->nErr = 0;` |
|         - | 2884 | `	/* Clear the class-body context (a prior compile aborted mid-class-body would` |
|         - | 2885 | `	 * otherwise leave these live for the next eval/include on this VM). */` |
|    113171 | 2886 | `	pGen->pCurClass = 0;` |
|    113171 | 2887 | `	pGen->iInMemberDefault = 0;` |
|    113171 | 2888 | `	return SXRET_OK;` |
|         5 | 2889 | `}` |
|         - | 2890 | `/*` |
|         - | 2891 | ` * Save the code generator's compile-position state and hand the live generator a` |
|         - | 2892 | ` * fresh, empty one for a NESTED compilation unit.` |
|         - | 2893 | ` *` |
|         - | 2894 | ` * A require/include normally runs at execution time, when no compile is in flight,` |
|         - | 2895 | ` * so VmEvalChunk can call PH7_ResetCodeGenerator to wipe the generator. But an` |
|         - | 2896 | `` * autoload can fire in the MIDDLE of compiling a class: resolving `class Child`` |
|         - | 2897 | `` * extends Base` calls PH7_VmExtractClass, which triggers the autoloader, whose`` |
|         - | 2898 | `` * body `require`s Base's file — a nested compile while the outer one is mid-token.`` |
|         - | 2899 | ` * PH7_ResetCodeGenerator would then blow away the outer compile's cursor` |
|         - | 2900 | ` * (pIn/pEnd), current block, use-imports, labels, etc.; the outer parse resumes` |
|         - | 2901 | ` * with pIn == NULL and reports a bogus "Expected '{' after class 'Child'". (Common` |
|         - | 2902 | ` * in every real framework: Composer autoloads parent classes on demand.)` |
|         - | 2903 | ` *` |
|         - | 2904 | ` * This snapshots the position/scope fields into *pSaved (a caller-stack` |
|         - | 2905 | ` * ph7_gen_state used purely as storage) and re-initializes the live generator's` |
|         - | 2906 | ` * position containers to FRESH, empty ones WITHOUT releasing the outer's (the` |
|         - | 2907 | ` * snapshot now owns those). hLiteral/hNumLiteral/hVar are intentionally left LIVE` |
|         - | 2908 | ` * and shared — compiled bytecode interns names/literals into them and they grow` |
|         - | 2909 | ` * monotonically (exactly why PH7_ResetCodeGenerator keeps them); restoring an old` |
|         - | 2910 | ` * copy of those headers after a nested grow would use-after-free the bucket array.` |
|         - | 2911 | ` */` |
|         4 | 2912 | `PH7_PRIVATE void PH7_CompilerSaveState(ph7_vm *pVm,ph7_gen_state *pSaved,ProcConsumer xErr,void *pErrData)` |
|         1 | 2913 | `{` |
|         5 | 2914 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 2915 | `	/* Shallow-copy every field; the position containers below are then replaced` |
|         - | 2916 | `	 * with fresh ones on the live struct, so *pSaved keeps the outer's memory. */` |
|         5 | 2917 | `	*pSaved = *pGen;` |
|         5 | 2918 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|         5 | 2919 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|         5 | 2920 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|         5 | 2921 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|         5 | 2922 | `	SySetInit(&pGen->aScope,&pVm->sAllocator,sizeof(GenScope));` |
|         5 | 2923 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|         5 | 2924 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|         5 | 2925 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|         5 | 2926 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|         5 | 2927 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|         5 | 2928 | `	GenStateInitUseImports(&(*pGen),&(*pVm));` |
|         5 | 2929 | `	GenStateInitSeenSymbols(&(*pGen),&(*pVm));` |
|         - | 2930 | `	/* Fresh global scope for the nested unit (address of the embedded sGlobal is` |
|         - | 2931 | `	 * stable, so any outer block still parented to it stays valid across restore). */` |
|         5 | 2932 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(pVm),0);` |
|         5 | 2933 | `	pGen->pCurrent = &pGen->sGlobal;` |
|         5 | 2934 | `	pGen->pIn = pGen->pEnd = 0;` |
|         5 | 2935 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|         5 | 2936 | `	pGen->pTokenSet = 0;` |
|         5 | 2937 | `	pGen->nErr = 0;` |
|         5 | 2938 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|         5 | 2939 | `	pGen->nCommaExprOk = 0;` |
|         5 | 2940 | `	pGen->zClauseCloser = 0;` |
|         5 | 2941 | `	pGen->bInGenerator = 0;` |
|         5 | 2942 | `	pGen->bStrictTypes = 0;` |
|         5 | 2943 | `	pGen->bStrictTypesLocked = 0;` |
|         - | 2944 | `	/* The nested unit is a fresh top-level compile: it is not lexically inside the` |
|         - | 2945 | `	 * outer's class body nor its member default, so a __TRAIT__ in the nested file` |
|         - | 2946 | `	 * must not inherit the outer's trait. (Restore below carries the outer's values` |
|         - | 2947 | `	 * back, so only the nested unit sees these zeros.) */` |
|         5 | 2948 | `	pGen->pCurClass = 0;` |
|         5 | 2949 | `	pGen->iInMemberDefault = 0;` |
|         5 | 2950 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|         5 | 2951 | `	pGen->xErr = xErr;` |
|         5 | 2952 | `	pGen->pErrData = pErrData;` |
|         5 | 2953 | `}` |
|         - | 2954 | `/*` |
|         - | 2955 | ` * Restore the outer compile-position state saved by PH7_CompilerSaveState,` |
|         - | 2956 | ` * releasing the nested unit's position containers first. The shared` |
|         - | 2957 | ` * hLiteral/hNumLiteral/hVar tables (grown by the nested compile) are carried` |
|         - | 2958 | ` * forward, NOT rolled back to the snapshot's stale headers.` |
|         - | 2959 | ` */` |
|         4 | 2960 | `PH7_PRIVATE void PH7_CompilerRestoreState(ph7_vm *pVm,ph7_gen_state *pSaved)` |
|         1 | 2961 | `{` |
|         5 | 2962 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 2963 | `	GenBlock *pBlock,*pParent;` |
|         - | 2964 | `	SyHash hVar,hLiteral,hNumLiteral;` |
|         - | 2965 | `	/* Free any nested blocks left open (e.g. an aborted nested compile), then the` |
|         - | 2966 | `	 * nested global block's own fixup sets. */` |
|         5 | 2967 | `	pBlock = pGen->pCurrent;` |
|         5 | 2968 | `	while( pBlock && pBlock->pParent != 0 ){` |
|       ! 0 | 2969 | `		pParent = pBlock->pParent;` |
|       ! 0 | 2970 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 2971 | `		pBlock = pParent;` |
|       ! 0 | 2972 | `	}` |
|         5 | 2973 | `	GenStateReleaseBlock(&pGen->sGlobal);` |
|         - | 2974 | `	/* Release the nested unit's position containers. */` |
|         5 | 2975 | `	SySetRelease(&pGen->aLabel);` |
|         5 | 2976 | `	SySetRelease(&pGen->aGoto);` |
|         5 | 2977 | `	SySetRelease(&pGen->aNullsafeJmp);` |
|         5 | 2978 | `	SySetRelease(&pGen->aLoopParent);` |
|         5 | 2979 | `	SySetRelease(&pGen->aScope);` |
|         5 | 2980 | `	SySetRelease(&pGen->aTrivia);` |
|         5 | 2981 | `	SySetRelease(&pGen->aPendingAttrs);` |
|         5 | 2982 | `	SyBlobRelease(&pGen->sWorker);` |
|         5 | 2983 | `	SyBlobRelease(&pGen->sErrBuf);` |
|         5 | 2984 | `	SyBlobRelease(&pGen->sNamespace);` |
|         5 | 2985 | `	SyHashRelease(&pGen->hUseImports);` |
|         5 | 2986 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|         5 | 2987 | `	SyHashRelease(&pGen->hUseConstImports);` |
|         5 | 2988 | `	GenStateReleaseSeenSymbols(&(*pGen));` |
|         - | 2989 | `	/* Preserve the (possibly grown) shared intern tables across the restore. */` |
|         5 | 2990 | `	hVar = pGen->hVar;` |
|         5 | 2991 | `	hLiteral = pGen->hLiteral;` |
|         5 | 2992 | `	hNumLiteral = pGen->hNumLiteral;` |
|         5 | 2993 | `	*pGen = *pSaved;` |
|         5 | 2994 | `	pGen->hVar = hVar;` |
|         5 | 2995 | `	pGen->hLiteral = hLiteral;` |
|         5 | 2996 | `	pGen->hNumLiteral = hNumLiteral;` |
|         5 | 2997 | `}` |
|         - | 2998 | `/*` |
|         - | 2999 | ` * Raise php's parse error for an unexpected token: E_PARSE with the exact text` |
|         - | 3000 | ` * php's parser prints, e.g.` |
|         - | 3001 | ` *` |
|         - | 3002 | ` *   syntax error, unexpected token ";", expecting "{"` |
|         - | 3003 | ` *   syntax error, unexpected identifier "invalid", expecting "("` |
|         - | 3004 | ` *   syntax error, unexpected end of file` |
|         - | 3005 | ` *` |
|         - | 3006 | ` * php names the token by CLASS, not just by text: an identifier, a variable and` |
|         - | 3007 | ` * a number each get their own noun, while everything else (keywords, operators,` |
|         - | 3008 | ` * punctuation) is a "token". zExpecting is the optional ", expecting ..." tail` |
|         - | 3009 | ` * — pass NULL when the site cannot say what it wanted (php often can't either).` |
|         - | 3010 | ` * pTok == NULL means the input ran out: "unexpected end of file".` |
|         - | 3011 | ` *` |
|         - | 3012 | ` * PHL's hand-written recursive-descent parser has no bison expectation sets, so` |
|         - | 3013 | ` * a site can only claim an "expecting" clause it genuinely knows; every clause` |
|         - | 3014 | ` * emitted here was verified against php 8.5.7 for the construct in question.` |
|         - | 3015 | ` */` |
|       208 | 3016 | `PH7_PRIVATE sxi32 PH7_GenSyntaxError(` |
|         - | 3017 | `	ph7_gen_state *pGen,   /* Code generator state */` |
|         - | 3018 | `	SyToken *pTok,         /* Offending token, or NULL for end of file */` |
|         - | 3019 | `	const char *zExpecting /* ", expecting <this>" tail, or NULL */` |
|         - | 3020 | `	)` |
|         5 | 3021 | `{` |
|       213 | 3022 | `	const char *zNoun = "token";` |
|         - | 3023 | `	sxu32 nLine;` |
|       213 | 3024 | `	if( pTok == 0 && pGen->pTokenSet ){` |
|         - | 3025 | `		/* The caller ran out of tokens inside its own slice — but a statement's slice stops` |
|         - | 3026 | `		 * BEFORE its terminator, so the token php actually names (typically the ';') is the` |
|         - | 3027 | `		 * one sitting just past the slice, still inside the chunk's token stream. Reach for` |
|         - | 3028 | `		 * it before concluding "end of file". */` |
|        96 | 3029 | `		SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        96 | 3030 | `		SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|        96 | 3031 | `		if( pGen->pEnd >= pBase && pGen->pEnd < pStreamEnd ){` |
|        92 | 3032 | `			pTok = pGen->pEnd;` |
|        44 | 3033 | `		}` |
|        46 | 3034 | `	}` |
|       213 | 3035 | `	nLine = pTok ? pTok->nLine : (pGen->pIn > (SyToken *)SySetBasePtr(pGen->pTokenSet) ? pGen->pIn[-1].nLine : 1);` |
|       213 | 3036 | `	if( pTok == 0 ){` |
|         8 | 3037 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|         2 | 3038 | `			zExpecting ? "syntax error, unexpected end of file, expecting %s"` |
|         - | 3039 | `			           : "syntax error, unexpected end of file",` |
|         2 | 3040 | `			zExpecting);` |
|         - | 3041 | `	}` |
|       209 | 3042 | `	if( pTok->nType & PH7_TK_ID ){` |
|        21 | 3043 | `		zNoun = "identifier";` |
|       200 | 3044 | `	}else if( pTok->nType & PH7_TK_DOLLAR ){` |
|         9 | 3045 | `		zNoun = "variable";` |
|         - | 3046 | `		/* The '$' is its own token and carries only "$" as text; the NAME is the` |
|         - | 3047 | ``		 * token after it. php names the whole variable, so `$x` was being reported`` |
|         - | 3048 | ``		 * as the nameless `variable "$"`. Stitch the two back together. */`` |
|         9 | 3049 | `		if( pGen->pTokenSet ){` |
|         9 | 3050 | `			SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|         9 | 3051 | `			SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|         9 | 3052 | `			SyToken *pName = &pTok[1];` |
|         6 | 3053 | `			if( pTok >= pBase && pName < pStreamEnd` |
|         6 | 3054 | `				&& (pName->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|         9 | 3055 | `				&& pName->sData.nByte > 0 ){` |
|         9 | 3056 | `				SyBlobReset(&pGen->sWorker);` |
|         9 | 3057 | `				SyBlobAppend(&pGen->sWorker,"$",sizeof(char));` |
|         9 | 3058 | `				SyBlobAppend(&pGen->sWorker,pName->sData.zString,pName->sData.nByte);` |
|         - | 3059 | `				{` |
|         - | 3060 | `					SyString sVar;` |
|         9 | 3061 | `					SyStringInitFromBuf(&sVar,SyBlobData(&pGen->sWorker),` |
|         - | 3062 | `						SyBlobLength(&pGen->sWorker));` |
|         9 | 3063 | `					if( zExpecting ){` |
|        12 | 3064 | `						return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|         - | 3065 | `							"syntax error, unexpected %s \"%z\", expecting %s",` |
|         3 | 3066 | `							zNoun,&sVar,zExpecting);` |
|         - | 3067 | `					}` |
|       ! 0 | 3068 | `					return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|       ! 0 | 3069 | `						"syntax error, unexpected %s \"%z\"",zNoun,&sVar);` |
|         - | 3070 | `				}` |
|         - | 3071 | `			}` |
|       ! 0 | 3072 | `		}` |
|       185 | 3073 | `	}else if( pTok->nType & PH7_TK_INTEGER ){` |
|        28 | 3074 | `		zNoun = "integer";` |
|       173 | 3075 | `	}else if( pTok->nType & PH7_TK_REAL ){` |
|       ! 0 | 3076 | `		zNoun = "float";` |
|       ! 0 | 3077 | `	}` |
|       203 | 3078 | `	if( zExpecting ){` |
|       143 | 3079 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        46 | 3080 | `			"syntax error, unexpected %s \"%z\", expecting %s",zNoun,&pTok->sData,zExpecting);` |
|         - | 3081 | `	}` |
|       164 | 3082 | `	return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        53 | 3083 | `		"syntax error, unexpected %s \"%z\"",zNoun,&pTok->sData);` |
|       109 | 3084 | `}` |
|         - | 3085 | `/*` |
|         - | 3086 | ` * Generate a compile-time error message.` |
|         - | 3087 | ` * If the error count limit is reached (usually 15 error message)` |
|         - | 3088 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|         - | 3089 | ` * abort compilation immediately.` |
|         - | 3090 | ` */` |
|       846 | 3091 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|         5 | 3092 | `{` |
|       851 | 3093 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|       851 | 3094 | `	const char *zErr = "Error";` |
|         - | 3095 | `	SyString *pFile;` |
|         - | 3096 | `	va_list ap;` |
|         - | 3097 | `	sxi32 rc;` |
|         - | 3098 | `	/* Reset the working buffer */` |
|       851 | 3099 | `	SyBlobReset(pWorker);` |
|         - | 3100 | `	/* Peek the processed file path if available */` |
|       851 | 3101 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|       851 | 3102 | `	if( nErrType == E_ERROR \|\| nErrType == E_PARSE ){` |
|         - | 3103 | `		/* Increment the error counter. A PARSE error is every bit as fatal as an` |
|         - | 3104 | `		 * E_ERROR one: php compiles nothing, runs nothing and exits 255. Counting` |
|         - | 3105 | `		 * only E_ERROR let a parse error print its diagnostic and then fall through` |
|         - | 3106 | `		 * into execution with a 0 exit status. */` |
|       811 | 3107 | `		pGen->nErr++;` |
|       811 | 3108 | `		if( pGen->nErr > 15 ){` |
|         - | 3109 | `			/* Error count limit reached */` |
|         6 | 3110 | `			if( pGen->xErr ){` |
|         6 | 3111 | `				SyBlobAppend(pWorker,"PHP ",4);` |
|         6 | 3112 | `				SyBlobFormat(pWorker,"Fatal error:  Error count limit reached,PH7 is aborting compilation");` |
|         6 | 3113 | `				if( pFile ){` |
|         6 | 3114 | `					SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|         2 | 3115 | `				}` |
|         6 | 3116 | `				SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|         6 | 3117 | `				if( SyBlobLength(pWorker) > 0 ){` |
|         6 | 3118 | `					pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|         2 | 3119 | `				}` |
|         2 | 3120 | `			}` |
|         - | 3121 | `			/* Abort immediately */` |
|         6 | 3122 | `			return SXERR_ABORT;` |
|         - | 3123 | `		}` |
|       401 | 3124 | `	}` |
|       847 | 3125 | `	if( pGen->xErr == 0 ){` |
|         - | 3126 | `		/* No consumer — but keep the BARE message in the error buffer so a caller` |
|         - | 3127 | `		 * that needs the text can read it back. eval() compiles with logging off` |
|         - | 3128 | `		 * (a parse error there is php's catchable ParseError, not a printed` |
|         - | 3129 | `		 * diagnostic) and needs exactly this string for the exception message. */` |
|        39 | 3130 | `		va_start(ap,zFormat);` |
|        39 | 3131 | `		SyBlobFormatAp(pWorker,zFormat,ap);` |
|        39 | 3132 | `		va_end(ap);` |
|        39 | 3133 | `		return SXRET_OK;` |
|         - | 3134 | `	}` |
|       809 | 3135 | `	switch(nErrType){` |
|       404 | 3136 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|        44 | 3137 | `	case E_WARNING: zErr = "Warning";     break;` |
|       368 | 3138 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       ! 0 | 3139 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|       ! 0 | 3140 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|       ! 0 | 3141 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|       ! 0 | 3142 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|       ! 0 | 3143 | `	case 8192 /* E_DEPRECATED */: zErr = "Deprecated"; break;` |
|       ! 0 | 3144 | `	default:` |
|       ! 0 | 3145 | `		break;` |
|         - | 3146 | `	}` |
|       809 | 3147 | `	rc = SXRET_OK;` |
|         - | 3148 | `	/* Format: PHP <severity>:  <message> in <file> on line <line> */` |
|       809 | 3149 | `	SyBlobAppend(pWorker,"PHP ",4);` |
|       809 | 3150 | `	SyBlobFormat(pWorker,"%s:  ",zErr);` |
|       809 | 3151 | `	va_start(ap,zFormat);` |
|       809 | 3152 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|       809 | 3153 | `	va_end(ap);` |
|       809 | 3154 | `	if( pFile ){` |
|       809 | 3155 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       402 | 3156 | `	}` |
|         - | 3157 | `	/* Append a new line */` |
|       809 | 3158 | `	SyBlobAppend(pWorker,(const void *)"\n",sizeof(char));` |
|       809 | 3159 | `	if( SyBlobLength(pWorker) > 0 ){` |
|         - | 3160 | `		/* Consume the generated error message */` |
|       809 | 3161 | `		pGen->xErr(SyBlobData(pWorker),SyBlobLength(pWorker),pGen->pErrData);` |
|       402 | 3162 | `	}` |
|       809 | 3163 | `	return rc;` |
|       428 | 3164 | `}` |
|         - | 3165 |  |
