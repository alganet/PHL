# src/ph7/compile.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2803/2975 lines (94.22%)

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
|       446 |   32 | `PH7_PRIVATE sxi32 GenStateGetLabel(ph7_gen_state *pGen,SyString *pName,ph7_vm_func *pFunc,Label **ppOut)` |
|         5 |   33 | `{` |
|         - |   34 | `	Label *aLabel;` |
|         - |   35 | `	sxu32 n;` |
|         - |   36 | `	/* Perform a linear scan on the label table */` |
|       451 |   37 | `	aLabel = (Label *)SySetBasePtr(&pGen->aLabel);` |
|      1615 |   38 | `	for( n = 0 ; n < SySetUsed(&pGen->aLabel) ; ++n ){` |
|      1333 |   39 | `		if( aLabel[n].pFunc == pFunc && SyStringCmp(&aLabel[n].sName,pName,SyMemcmp) == 0 ){` |
|         - |   40 | `			/* Jump destination found */` |
|       169 |   41 | `			if( ppOut ){` |
|       164 |   42 | `				*ppOut = &aLabel[n];` |
|        80 |   43 | `			}` |
|       169 |   44 | `			return SXRET_OK;` |
|         - |   45 | `		}` |
|       587 |   46 | `	}` |
|         - |   47 | `	/* No such destination */` |
|       287 |   48 | `	return SXERR_NOTFOUND;` |
|       228 |   49 | `}` |
|         - |   50 | `/*` |
|         - |   51 | ` * Fetch a block that correspond to the given criteria from the stack of` |
|         - |   52 | ` * compiled blocks.` |
|         - |   53 | ` * Return a pointer to that block on success. NULL otherwise.` |
|         - |   54 | ` */` |
|         - |   55 | `/*` |
|         - |   56 | ` * Is the declaration the generator is standing on CONDITIONAL -- php's "not early` |
|         - |   57 | `` * bound"? A `function` or `class` written at the top level of a unit is bound when`` |
|         - |   58 | `` * the unit compiles, and one written anywhere else -- inside an `if`, a loop, a`` |
|         - |   59 | `` * `try`, another function's body -- is bound when execution REACHES it, and not`` |
|         - |   60 | ` * before.` |
|         - |   61 | ` *` |
|         - |   62 | `` * The difference is not academic: `if (!function_exists('mb_convert_encoding')) {`` |
|         - |   63 | `` * function mb_convert_encoding(...) {...} }` is how every symfony/polyfill-* package`` |
|         - |   64 | ` * is written, and binding that body unconditionally REPLACED the engine's own` |
|         - |   65 | ` * builtin with the polyfill -- in a tree that has one, which is nearly every real` |
|         - |   66 | `` * project. `if (false) { function f(){} }` declared `f` too.`` |
|         - |   67 | ` */` |
|    184872 |   68 | `PH7_PRIVATE int GenStateDeclIsConditional(ph7_gen_state *pGen)` |
|         5 |   69 | `{` |
|    184877 |   70 | `	GenBlock *pBlock = pGen->pCurrent;` |
|    184877 |   71 | `	return pBlock != 0 && (pBlock->iFlags & GEN_BLOCK_GLOBAL) == 0;` |
|         5 |   72 | `}` |
|         - |   73 |  |
|     56315 |   74 | `PH7_PRIVATE GenBlock * GenStateFetchBlock(GenBlock *pCurrent,sxi32 iBlockType,sxi32 iCount)` |
|         5 |   75 | `{` |
|     56320 |   76 | `	GenBlock *pBlock = pCurrent;` |
|    128061 |   77 | `	for(;;){` |
|    256479 |   78 | `		if( pBlock->iFlags & iBlockType ){` |
|     56322 |   79 | `			iCount--; /* Decrement nesting level */` |
|     56322 |   80 | `			if( iCount < 1 ){` |
|         - |   81 | `				/* Block meet with the desired criteria */` |
|     56268 |   82 | `				return pBlock;` |
|         - |   83 | `			}` |
|        27 |   84 | `		}` |
|         - |   85 | `		/* Point to the upper block */` |
|    200216 |   86 | `		pBlock = pBlock->pParent;` |
|    200216 |   87 | `		if( pBlock == 0 \|\| (pBlock->iFlags & (GEN_BLOCK_PROTECTED\|GEN_BLOCK_FUNC)) ){` |
|         - |   88 | `			/* Forbidden */` |
|        29 |   89 | `			break;` |
|         - |   90 | `		}` |
|         5 |   91 | `	}` |
|         - |   92 | `	/* No such block */` |
|        55 |   93 | `	return 0;` |
|     28124 |   94 | `}` |
|         - |   95 | `/*` |
|         - |   96 | ` * Initialize a freshly allocated block instance.` |
|         - |   97 | ` */` |
|   1936896 |   98 | `static void GenStateInitBlock(` |
|         - |   99 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - |  100 | `	GenBlock *pBlock,    /* Target block */` |
|         - |  101 | `	sxi32 iType,         /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |  102 | `	sxu32 nFirstInstr,   /* First instruction to compile */` |
|         - |  103 | `	void *pUserData      /* Upper layer private data */` |
|         - |  104 | `	)` |
|         5 |  105 | `{` |
|         - |  106 | `	/* Initialize block fields */` |
|   1936901 |  107 | `	pBlock->nFirstInstr = nFirstInstr;` |
|   1936901 |  108 | `	pBlock->pUserData   = pUserData;` |
|   1936901 |  109 | `	pBlock->pGen        = pGen;` |
|   1936901 |  110 | `	pBlock->iFlags      = iType;` |
|   1936901 |  111 | `	pBlock->pParent     = 0;` |
|   1936901 |  112 | `	SySetInit(&pBlock->aJumpFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|   1936901 |  113 | `	SySetInit(&pBlock->aPostContFix,&pGen->pVm->sAllocator,sizeof(JumpFixup));` |
|   1936901 |  114 | `}` |
|         - |  115 | `/*` |
|         - |  116 | ` * Allocate a new block instance.` |
|         - |  117 | ` * Return SXRET_OK and write a pointer to the new instantiated block` |
|         - |  118 | ` * on success.Otherwise generate a compile-time error and abort` |
|         - |  119 | ` * processing on failure.` |
|         - |  120 | ` */` |
|   1928961 |  121 | `PH7_PRIVATE sxi32 GenStateEnterBlock(` |
|         - |  122 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - |  123 | `	sxi32 iType,          /* Block type [i.e: loop, conditional, function body, etc.]*/` |
|         - |  124 | `	sxu32 nFirstInstr,    /* First instruction to compile */` |
|         - |  125 | `	void *pUserData,      /* Upper layer private data */` |
|         - |  126 | `	GenBlock **ppBlock    /* OUT: instantiated block */` |
|         - |  127 | `	)` |
|         5 |  128 | `{` |
|         - |  129 | `	GenBlock *pBlock;` |
|         - |  130 | `	/* Allocate a new block instance */` |
|   1928966 |  131 | `	pBlock = (GenBlock *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(GenBlock));` |
|   1928966 |  132 | `	if( pBlock == 0 ){` |
|         - |  133 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |  134 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - |  135 | `		 */` |
|       ! 0 |  136 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"Fatal, PH7 engine is running out-of-memory");` |
|         - |  137 | `		/* Abort processing immediately */` |
|       ! 0 |  138 | `		return SXERR_ABORT;` |
|         - |  139 | `	}` |
|         - |  140 | `	/* Zero the structure */` |
|   1928966 |  141 | `	SyZero(pBlock,sizeof(GenBlock));` |
|   1928966 |  142 | `	GenStateInitBlock(&(*pGen),pBlock,iType,nFirstInstr,pUserData);` |
|         - |  143 | `	/* Link to the parent block */` |
|   1928966 |  144 | `	pBlock->pParent = pGen->pCurrent;` |
|         - |  145 | `	/* A loop or switch gets an id, and remembers the loop it nests inside, so a goto's` |
|         - |  146 | `	 * and a label's positions can be compared after compilation (see aLoopParent). */` |
|   1928966 |  147 | `	if( iType & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    156576 |  148 | `		sxu32 nParent = pGen->nCurLoopId;` |
|    156576 |  149 | `		pGen->nLoopId++;` |
|    156576 |  150 | `		SySetPut(&pGen->aLoopParent,(const void *)&nParent);` |
|    156576 |  151 | `		pBlock->nLoopId = pGen->nLoopId;` |
|    156576 |  152 | `		pBlock->nOuterLoopId = nParent;` |
|    156576 |  153 | `		pGen->nCurLoopId = pGen->nLoopId;` |
|     78168 |  154 | `	}` |
|         - |  155 | `	/* A try/catch/finally block gets a scope id, and remembers the scope it nests inside,` |
|         - |  156 | `	 * so the chain between any two points can be walked after compilation (aScope). Every` |
|         - |  157 | `	 * other block simply inherits the scope in effect. */` |
|   1928966 |  158 | `	pBlock->nOuterScopeId = pGen->nCurScopeId;` |
|   1928966 |  159 | `	pBlock->nScopeId = pGen->nCurScopeId;` |
|   1928966 |  160 | `	if( iType & GEN_BLOCK_EXCEPTION ){` |
|         - |  161 | `		GenScope sScope;` |
|     10537 |  162 | `		sScope.nParent = pGen->nCurScopeId;` |
|     10537 |  163 | `		sScope.pUserData = pUserData;` |
|     10537 |  164 | `		if( iType & GEN_BLOCK_FINALLY ){` |
|       329 |  165 | `			sScope.iKind = GEN_SCOPE_FINALLY;` |
|     10375 |  166 | `		}else if( iType & GEN_BLOCK_DETACHED ){` |
|      4921 |  167 | `			sScope.iKind = GEN_SCOPE_DETACHED;` |
|      2459 |  168 | `		}else{` |
|         - |  169 | `			/* A try. pUserData is its ph7_exception, which every try-block site passes at` |
|         - |  170 | `			 * ENTRY precisely so this can classify it. */` |
|      5297 |  171 | `			sScope.iKind = GenStateInlineTryCatch(pGen) ? GEN_SCOPE_TRY_INLINE : GEN_SCOPE_TRY;` |
|         - |  172 | `		}` |
|     10537 |  173 | `		if( SySetPut(&pGen->aScope,(const void *)&sScope) == SXRET_OK ){` |
|     10537 |  174 | `			pBlock->nScopeId = SySetUsed(&pGen->aScope);` |
|     10537 |  175 | `			pGen->nCurScopeId = pBlock->nScopeId;` |
|      5258 |  176 | `		}` |
|      5258 |  177 | `	}` |
|         - |  178 | `	/* Mark as the current block */` |
|   1928966 |  179 | `	pGen->pCurrent = pBlock;` |
|   1928966 |  180 | `	if( ppBlock ){` |
|         - |  181 | `		/* Write a pointer to the new instance */` |
|    909780 |  182 | `		*ppBlock = pBlock;` |
|    454143 |  183 | `	}` |
|   1928966 |  184 | `	return SXRET_OK;` |
|    963013 |  185 | `}` |
|         - |  186 | `/*` |
|         - |  187 | ` * Release block fields without freeing the whole instance.` |
|         - |  188 | ` */` |
|   1928953 |  189 | `static void GenStateReleaseBlock(GenBlock *pBlock)` |
|         5 |  190 | `{` |
|   1928958 |  191 | `	SySetRelease(&pBlock->aPostContFix);` |
|   1928958 |  192 | `	SySetRelease(&pBlock->aJumpFix);` |
|   1928958 |  193 | `}` |
|         - |  194 | `/*` |
|         - |  195 | ` * Release a block.` |
|         - |  196 | ` */` |
|   1928943 |  197 | `static void GenStateFreeBlock(GenBlock *pBlock)` |
|         5 |  198 | `{` |
|   1928948 |  199 | `	ph7_gen_state *pGen = pBlock->pGen;` |
|   1928948 |  200 | `	GenStateReleaseBlock(&(*pBlock));` |
|         - |  201 | `	/* Free the instance */` |
|   1928948 |  202 | `	SyMemBackendPoolFree(&pGen->pVm->sAllocator,pBlock);` |
|   1928948 |  203 | `}` |
|         - |  204 | `/*` |
|         - |  205 | ` * POP and release a block from the stack of compiled blocks.` |
|         - |  206 | ` */` |
|   1928943 |  207 | `PH7_PRIVATE sxi32 GenStateLeaveBlock(ph7_gen_state *pGen,GenBlock **ppBlock)` |
|         5 |  208 | `{` |
|   1928948 |  209 | `	GenBlock *pBlock = pGen->pCurrent;` |
|   1928948 |  210 | `	if( pBlock == 0 ){` |
|         - |  211 | `		/* No more block to pop */` |
|       ! 0 |  212 | `		return SXERR_EMPTY;` |
|         - |  213 | `	}` |
|   1928948 |  214 | `	if( pBlock->iFlags & (GEN_BLOCK_LOOP\|GEN_BLOCK_SWITCH) ){` |
|    156566 |  215 | `		pGen->nCurLoopId = pBlock->nOuterLoopId;` |
|     78163 |  216 | `	}` |
|   1928948 |  217 | `	if( pBlock->iFlags & GEN_BLOCK_EXCEPTION ){` |
|     10537 |  218 | `		pGen->nCurScopeId = pBlock->nOuterScopeId;` |
|      5258 |  219 | `	}` |
|         - |  220 | `	/* Point to the upper block */` |
|   1928948 |  221 | `	pGen->pCurrent = pBlock->pParent;` |
|   1928948 |  222 | `	if( ppBlock ){` |
|         - |  223 | `		/* Write a pointer to the popped block */` |
|       ! 0 |  224 | `		*ppBlock = pBlock;` |
|       ! 0 |  225 | `	}else{` |
|         - |  226 | `		/* Safely release the block */` |
|   1928948 |  227 | `		GenStateFreeBlock(&(*pBlock));` |
|         - |  228 | `	}` |
|   1928948 |  229 | `	return SXRET_OK;` |
|    963004 |  230 | `}` |
|         - |  231 | `/*` |
|         - |  232 | ` * PHP-parity redeclaration guard.` |
|         - |  233 | ` *` |
|         - |  234 | ` * PHP raises a fatal "Cannot redeclare ..." when a class/interface/trait/enum` |
|         - |  235 | ` * or a function is declared a second time. PHL hoists every declaration into` |
|         - |  236 | `` * the VM at compile time (so `if(false){class C{}}` already makes C exist), and`` |
|         - |  237 | ` * historically it silently *overwrote* duplicates. We reproduce PHP for the` |
|         - |  238 | ` * case that matters and that real code hits: a declaration that is` |
|         - |  239 | ` * UNCONDITIONAL and at file top level, whose name is already bound by another` |
|         - |  240 | ` * unconditional top-level declaration (or by a builtin). Conditional` |
|         - |  241 | ` * declarations (inside if/loops/switch/try or nested in a function) are left` |
|         - |  242 | `` * hoisting as before, so the `if(!class_exists('C')){class C{}}` and`` |
|         - |  243 | `` * `if(false){class C{}} class C{}` guard idioms keep working.`` |
|         - |  244 | ` *` |
|         - |  245 | ` * Included files compile at include time (i.e. at run time relative to the main` |
|         - |  246 | ` * script), so this compile-time check surfaces the fatal at the same moment PHP` |
|         - |  247 | ` * does for the cross-include case too.` |
|         - |  248 | ` */` |
|    184584 |  249 | `PH7_PRIVATE int GenStateUnconditionalTopLevel(ph7_gen_state *pGen)` |
|         5 |  250 | `{` |
|    184589 |  251 | `	GenBlock *pBlock = pGen->pCurrent;` |
|    184655 |  252 | `	while( pBlock ){` |
|    184655 |  253 | `		if( pBlock->iFlags & (GEN_BLOCK_COND\|GEN_BLOCK_LOOP\|GEN_BLOCK_FUNC\|GEN_BLOCK_SWITCH\|GEN_BLOCK_EXCEPTION) ){` |
|       111 |  254 | `			return 0; /* conditional / nested */` |
|         - |  255 | `		}` |
|    184549 |  256 | `		if( pBlock->iFlags & GEN_BLOCK_GLOBAL ){` |
|    184483 |  257 | `			return 1; /* reached the global block with no conditional ancestor */` |
|         - |  258 | `		}` |
|        71 |  259 | `		pBlock = pBlock->pParent;` |
|         5 |  260 | `	}` |
|       ! 0 |  261 | `	return 1;` |
|     92170 |  262 | `}` |
|         - |  263 | `/*` |
|         - |  264 | ` * Guard a top-level function about to be installed. Same contract as the class` |
|         - |  265 | ` * guard above.` |
|         - |  266 | ` */` |
|    178790 |  267 | `PH7_PRIVATE sxi32 GenStateGuardFuncRedeclaration(ph7_gen_state *pGen,ph7_vm_func *pFunc)` |
|         5 |  268 | `{` |
|         - |  269 | `	SyHashEntry *pEntry;` |
|    178795 |  270 | `	if( !GenStateUnconditionalTopLevel(pGen) ){` |
|       ! 0 |  271 | `		return SXRET_OK;` |
|         - |  272 | `	}` |
|    178795 |  273 | `	pFunc->iFlags \|= VM_FUNC_BOUND;` |
|    178795 |  274 | `	if( pGen->pVm->bCompilingBuiltin ){` |
|    174355 |  275 | `		return SXRET_OK;` |
|         - |  276 | `	}` |
|         - |  277 | `	/* An INTERNAL function of this name makes the declaration php's fatal, and every` |
|         - |  278 | `	 * one of them can be seen from here: PH7_VmInit registers the whole host table` |
|         - |  279 | `	 * before a program compiles, the way php has its own before it compiles. php` |
|         - |  280 | `	 * names no previous declaration for this arm -- an internal function has no file` |
|         - |  281 | `	 * and no line to name -- so the sentence is the short one.` |
|         - |  282 | `	 *` |
|         - |  283 | `	 * The bCompilingBuiltin early-return above keeps the prelude itself exempt: ~22` |
|         - |  284 | `	 * builtins ARE embedded PHP, and each of them declares its own name. */` |
|      4445 |  285 | `	if( PH7_VmNameIsInternalFunc(pGen->pVm,pFunc->sName.zString,pFunc->sName.nByte) ){` |
|        11 |  286 | `		PH7_GenCompileError(pGen,E_ERROR,pFunc->nLine,` |
|         3 |  287 | `			"Cannot redeclare function %z()",&pFunc->sName);` |
|         8 |  288 | `		return SXERR_ABORT;` |
|         - |  289 | `	}` |
|      4439 |  290 | `	pEntry = SyHashGet(&pGen->pVm->hFunction,(const void *)pFunc->sName.zString,pFunc->sName.nByte);` |
|      4439 |  291 | `	if( pEntry ){` |
|         8 |  292 | `		ph7_vm_func *pPrev = (ph7_vm_func *)pEntry->pUserData;` |
|         8 |  293 | `		while( pPrev ){` |
|         8 |  294 | `			if( pPrev->iFlags & VM_FUNC_BOUND ){` |
|         8 |  295 | `				if( pPrev->sFile.nByte > 0 ){` |
|        11 |  296 | `					PH7_GenCompileError(pGen,E_ERROR,pFunc->nLine,` |
|         - |  297 | `						"Cannot redeclare function %z() (previously declared in %.*s:%u)",` |
|         3 |  298 | `						&pFunc->sName,pPrev->sFile.nByte,pPrev->sFile.zString,pPrev->nLine);` |
|         5 |  299 | `				}else{` |
|       ! 0 |  300 | `					PH7_GenCompileError(pGen,E_ERROR,pFunc->nLine,` |
|       ! 0 |  301 | `						"Cannot redeclare function %z()",&pFunc->sName);` |
|         - |  302 | `				}` |
|         8 |  303 | `				return SXERR_ABORT;` |
|         - |  304 | `			}` |
|       ! 0 |  305 | `			pPrev = pPrev->pNextName;` |
|       ! 0 |  306 | `		}` |
|       ! 0 |  307 | `	}` |
|      4433 |  308 | `	return SXRET_OK;` |
|     89273 |  309 | `}` |
|         - |  310 | `/*` |
|         - |  311 | ` * Emit a forward jump.` |
|         - |  312 | ` * Notes on forward jumps` |
|         - |  313 | ` *  Compilation of some PHP constructs such as if,for,while and the logical or` |
|         - |  314 | ` *  (\|\|) and logical and (&&) operators in expressions requires the` |
|         - |  315 | ` *  generation of forward jumps.` |
|         - |  316 | ` *  Since the destination PC target of these jumps isn't known when the jumps` |
|         - |  317 | ` *  are emitted, we record each forward jump in an instance of the following` |
|         - |  318 | ` *  structure. Those jumps are fixed later when the jump destination is resolved.` |
|         - |  319 | ` */` |
|   1100420 |  320 | `PH7_PRIVATE sxi32 GenStateNewJumpFixup(GenBlock *pBlock,sxi32 nJumpType,sxu32 nInstrIdx)` |
|         5 |  321 | `{` |
|         - |  322 | `	JumpFixup sJumpFix;` |
|         - |  323 | `	sxi32 rc;` |
|         - |  324 | `	/* Init the JumpFixup structure */` |
|   1100425 |  325 | `	sJumpFix.nJumpType = nJumpType;` |
|   1100425 |  326 | `	sJumpFix.nInstrIdx = nInstrIdx;` |
|         - |  327 | `	/* Remember which bytecode array the emitted instruction lives in: the block whose` |
|         - |  328 | `	 * table this lands in may be resolved after a container swap (see JumpFixup). */` |
|   1100425 |  329 | `	sJumpFix.pContainer = PH7_VmGetByteCodeContainer(pBlock->pGen->pVm);` |
|         - |  330 | `	/* Insert in the jump fixup table */` |
|   1100425 |  331 | `	rc = SySetPut(&pBlock->aJumpFix,(const void *)&sJumpFix);` |
|   1100425 |  332 | `	return rc;` |
|         5 |  333 | `}` |
|         - |  334 | `/*` |
|         - |  335 | ` * TRUE when the body being compiled has its try/catch/finally compiled INLINE` |
|         - |  336 | ` * (ROOT C, generator bodies) rather than into detached mini-programs.` |
|         - |  337 | ` */` |
|      5292 |  338 | `PH7_PRIVATE int GenStateInlineTryCatch(ph7_gen_state *pGen)` |
|         5 |  339 | `{` |
|      5297 |  340 | `	return pGen->bInGenerator && pGen->pVm->bInlineTryCatch;` |
|         5 |  341 | `}` |
|         - |  342 | `/*` |
|         - |  343 | ` * Walk the scope chain from nFrom (where a jump is) out to nTo (where it lands) and` |
|         - |  344 | `` * describe what it crosses. One walk serves `break`, `continue` and `goto` alike,`` |
|         - |  345 | ` * because they all ask the same question of the same chain — only the two endpoints` |
|         - |  346 | ` * differ, and for a goto they are not both known until compilation ends.` |
|         - |  347 | ` *` |
|         - |  348 | ` * Returns TRUE when nTo was actually reached, i.e. the target's scope ENCLOSES the` |
|         - |  349 | ` * jump. FALSE means the target sits inside a try/catch the jump is not in — jumping` |
|         - |  350 | ` * into one, which PHL cannot express (its handler is pushed by the try's` |
|         - |  351 | ` * OP_LOAD_EXCEPTION, and a catch body is a mini-program entered at instruction 0).` |
|         - |  352 | ` * Depth counting cannot answer this: two sibling trys have the same depth.` |
|         - |  353 | ` *` |
|         - |  354 | ` * What is counted, for the opcode the caller then picks:` |
|         - |  355 | ` *  nDet    — DETACHED catch/finally bodies left. Each is its own bytecode array, so a` |
|         - |  356 | ` *            jump out of one cannot be a plain OP_JMP: it parks and travels out through` |
|         - |  357 | ` *            one OP_POP_EXCEPTION landing pad per boundary (OP_CATCH_JMP);` |
|         - |  358 | ` *  nTry    — legacy trys left whose OP_POP_EXCEPTION the jump SKIPS, so nothing else` |
|         - |  359 | ` *            would run their finally. Trys BELOW the first boundary do not qualify: a` |
|         - |  360 | ` *            break/continue emits their OP_POP_EXCEPTION right here (bEmitPops), and a` |
|         - |  361 | ` *            goto drains them where it parks — hence the reset when one is reached;` |
|         - |  362 | ` *  nInline — ROOT C inline trys left. Their finallys are driven by VmFinallyAdvance,` |
|         - |  363 | ` *            not by the aException drain, so they are crossed with OP_SET_FINALLY_JMP;` |
|         - |  364 | `` *  nFinally — `finally` bodies left, which php forbids outright. When this is non-zero the`` |
|         - |  365 | ` *            three above are NOT computed: callers must test it first and reject.` |
|         - |  366 | ` */` |
|     56419 |  367 | `PH7_PRIVATE int GenStateJumpScope(ph7_gen_state *pGen,sxu32 nFrom,sxu32 nTo,int bEmitPops,` |
|         - |  368 | `	GenJumpScope *pScope)` |
|         5 |  369 | `{` |
|     56424 |  370 | `	GenScope *aScope = (GenScope *)SySetBasePtr(&pGen->aScope);` |
|     56424 |  371 | `	sxu32 nUsed = SySetUsed(&pGen->aScope);` |
|     56424 |  372 | `	sxu32 nCur = nFrom;` |
|     56424 |  373 | `	SyZero(pScope,sizeof(*pScope));` |
|     56538 |  374 | `	while( nCur != nTo ){` |
|         - |  375 | `		GenScope *pScopeEnt;` |
|       123 |  376 | `		if( nCur == 0 \|\| nCur > nUsed ){` |
|         6 |  377 | `			return FALSE; /* ran off the top without meeting nTo */` |
|         - |  378 | `		}` |
|       119 |  379 | `		pScopeEnt = &aScope[nCur - 1];` |
|       119 |  380 | `		if( pScopeEnt->iKind == GEN_SCOPE_FINALLY ){` |
|         - |  381 | ``			/* php: `jump out of a finally block is disallowed`. Counted rather than`` |
|         - |  382 | `			 * rejected here because the caller owns the diagnostic and its line — but` |
|         - |  383 | `			 * ONLY counted: the jump is illegal, so the other three fields are left as` |
|         - |  384 | `			 * they are rather than pretending to describe a crossing that will never be` |
|         - |  385 | `			 * emitted. (They could not be right anyway: this kind covers both the legacy` |
|         - |  386 | `			 * detached finally and the generator's INLINE one, which is not a separate` |
|         - |  387 | `			 * bytecode container.) Every caller tests nFinally first. A jump that stays` |
|         - |  388 | `			 * INSIDE the finally never reaches this scope, so it stays legal. */` |
|        14 |  389 | `			pScope->nFinally++;` |
|       114 |  390 | `		}else if( pScopeEnt->iKind == GEN_SCOPE_DETACHED ){` |
|        73 |  391 | `			if( pScope->nDet == 0 ){` |
|        69 |  392 | `				pScope->nTry = 0;    /* below the first boundary: not the landing pad's */` |
|        69 |  393 | `				pScope->nInline = 0;` |
|        33 |  394 | `			}` |
|        73 |  395 | `			pScope->nDet++;` |
|        74 |  396 | `		}else if( pScopeEnt->iKind == GEN_SCOPE_TRY_INLINE ){` |
|        11 |  397 | `			pScope->nInline++;` |
|        35 |  398 | `		}else if( pScope->nDet == 0 && bEmitPops ){` |
|         3 |  399 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP_EXCEPTION,0,0,pScopeEnt->pUserData,0);` |
|         2 |  400 | `		}else{` |
|        28 |  401 | `			pScope->nTry++;` |
|         - |  402 | `		}` |
|       119 |  403 | `		nCur = pScopeEnt->nParent;` |
|         5 |  404 | `	}` |
|     56420 |  405 | `	return TRUE;` |
|     28176 |  406 | `}` |
|         - |  407 | `/*` |
|         - |  408 | ` * Pick the jump opcode for a crossing described by GenStateJumpScope, and its iP1.` |
|         - |  409 | ` * Shared by break/continue (which know their target at emit time) and goto (which` |
|         - |  410 | ` * settles this in GenStateFixGoto, once the label fixes the counts).` |
|         - |  411 | ` */` |
|     56303 |  412 | `PH7_PRIVATE sxi32 GenStateScopeJumpOp(const GenJumpScope *pCross,sxi32 *piP1)` |
|         5 |  413 | `{` |
|     56308 |  414 | `	if( pCross->nDet > 0 \|\| pCross->nTry > 0 ){` |
|        84 |  415 | `		*piP1 = PH7_CATCH_JMP_P1(pCross->nDet,pCross->nTry);` |
|        84 |  416 | `		return PH7_OP_CATCH_JMP;` |
|         - |  417 | `	}` |
|     56228 |  418 | `	if( pCross->nInline > 0 ){` |
|        11 |  419 | `		*piP1 = (sxi32)pCross->nInline;` |
|        11 |  420 | `		return PH7_OP_SET_FINALLY_JMP;` |
|         - |  421 | `	}` |
|     56220 |  422 | `	*piP1 = 0;` |
|     56220 |  423 | `	return PH7_OP_JMP;` |
|     28118 |  424 | `}` |
|         - |  425 | `/*` |
|         - |  426 | ` * Resolve a recorded fixup to its VM instruction, in the container it was emitted` |
|         - |  427 | ` * into (see JumpFixup.pContainer) rather than whichever one is current now.` |
|         - |  428 | ` */` |
|   1116514 |  429 | `PH7_PRIVATE VmInstr * GenStateFixupInstr(const JumpFixup *pFix)` |
|         5 |  430 | `{` |
|   1116519 |  431 | `	return (VmInstr *)SySetAt(pFix->pContainer,pFix->nInstrIdx);` |
|         5 |  432 | `}` |
|         - |  433 | `/*` |
|         - |  434 | ` * Fix a forward jump now the jump destination is resolved.` |
|         - |  435 | ` * Return the total number of fixed jumps.` |
|         - |  436 | ` * Notes on forward jumps:` |
|         - |  437 | ` *  Compilation of some PHP constructs such as if,for,while and the logical or` |
|         - |  438 | ` *  (\|\|) and logical and (&&) operators in expressions requires the` |
|         - |  439 | ` *  generation of forward jumps.` |
|         - |  440 | ` *  Since the destination PC target of these jumps isn't known when the jumps` |
|         - |  441 | ` *  are emitted, we record each forward jump in an instance of the following` |
|         - |  442 | ` *  structure.Those jumps are fixed later when the jump destination is resolved.` |
|         - |  443 | ` */` |
|   1550132 |  444 | `PH7_PRIVATE sxu32 GenStateFixJumps(GenBlock *pBlock,sxi32 nJumpType,sxu32 nJumpDest)` |
|         5 |  445 | `{` |
|         - |  446 | `	JumpFixup *aFix;` |
|         - |  447 | `	VmInstr *pInstr;` |
|         - |  448 | `	sxu32 nFixed;` |
|         - |  449 | `	sxu32 n;` |
|         - |  450 | `	/* Point to the jump fixup table */` |
|   1550137 |  451 | `	aFix = (JumpFixup *)SySetBasePtr(&pBlock->aJumpFix);` |
|         - |  452 | `	/* Fix the desired jumps */` |
|   3551357 |  453 | `	for( nFixed = n = 0 ; n < SySetUsed(&pBlock->aJumpFix) ; ++n ){` |
|   2001225 |  454 | `		if( aFix[n].nJumpType < 0 ){` |
|         - |  455 | `			/* Already fixed */` |
|    701364 |  456 | `			continue;` |
|         - |  457 | `		}` |
|   1299866 |  458 | `		if( nJumpType > 0 && aFix[n].nJumpType != nJumpType ){` |
|         - |  459 | `			/* Not of our interest */` |
|    199450 |  460 | `			continue;` |
|         - |  461 | `		}` |
|         - |  462 | `		/* Point to the instruction to fix */` |
|   1100421 |  463 | `		pInstr = GenStateFixupInstr(&aFix[n]);` |
|   1100421 |  464 | `		if( pInstr ){` |
|   1100421 |  465 | `			pInstr->iP2 = nJumpDest;` |
|   1100421 |  466 | `			nFixed++;` |
|         - |  467 | `			/* Mark as fixed */` |
|   1100421 |  468 | `			aFix[n].nJumpType = -1;` |
|    549428 |  469 | `		}` |
|    549433 |  470 | `	}` |
|         - |  471 | `	/* Total number of fixed jumps */` |
|   1550137 |  472 | `	return nFixed;` |
|         5 |  473 | `}` |
|         - |  474 | `/*` |
|         - |  475 | ` * Fix a 'goto' now the jump destination is resolved.` |
|         - |  476 | ` * The goto statement can be used to jump to another section` |
|         - |  477 | ` * in the program.` |
|         - |  478 | ` * Refer to the routine responsible of compiling the goto` |
|         - |  479 | ` * statement for more information.` |
|         - |  480 | ` */` |
|    223529 |  481 | `PH7_PRIVATE sxi32 GenStateFixGoto(ph7_gen_state *pGen,sxu32 nOfft)` |
|         5 |  482 | `{` |
|         - |  483 | `	JumpFixup *pJump,*aJumps;` |
|         - |  484 | `	GenJumpScope sCross;` |
|         - |  485 | `	Label *pLabel;` |
|         - |  486 | `	VmInstr *pInstr;` |
|         - |  487 | `	sxi32 rc;` |
|         - |  488 | `	sxu32 n;` |
|         - |  489 | `	/* Point to the goto table */` |
|    223534 |  490 | `	aJumps = (JumpFixup *)SySetBasePtr(&pGen->aGoto);` |
|         - |  491 | `	/* Fix */` |
|    223758 |  492 | `	for( n = nOfft ; n < SySetUsed(&pGen->aGoto) ; ++n ){` |
|       231 |  493 | `		pJump = &aJumps[n];` |
|         - |  494 | `		/* Extract the target label */` |
|         - |  495 | `		/* A label declared in ANOTHER function is not a destination: the lookup is keyed` |
|         - |  496 | `		 * on the goto's own function, so a same-named label elsewhere simply does not` |
|         - |  497 | `		 * answer and this reports php's undefined-label fatal. */` |
|       231 |  498 | `		rc = GenStateGetLabel(&(*pGen),&pJump->sLabel,pJump->pFunc,&pLabel);` |
|       231 |  499 | `		if( rc != SXRET_OK ){` |
|         - |  500 | `			/* No such label */` |
|        70 |  501 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,"'goto' to undefined label '%z'",&pJump->sLabel);` |
|        70 |  502 | `			if( rc == SXERR_ABORT ){` |
|         3 |  503 | `				return SXERR_ABORT;` |
|         - |  504 | `			}` |
|        68 |  505 | `			continue;` |
|         - |  506 | `		}` |
|         - |  507 | `		/* php's one goto restriction: you may not jump INTO a loop or a switch. The label` |
|         - |  508 | `		 * is inside one exactly when it carries a loop id; that is legal only if the same` |
|         - |  509 | `		 * loop also encloses the goto, i.e. the label's loop is the goto's loop or one of` |
|         - |  510 | `		 * its ancestors. Walk up from the goto's loop looking for the label's. */` |
|       164 |  511 | `		if( pLabel->nLoopId != 0 ){` |
|         5 |  512 | `			sxu32 *aParent = (sxu32 *)SySetBasePtr(&pGen->aLoopParent);` |
|         5 |  513 | `			sxu32 nCur = pJump->nLoopId;` |
|         5 |  514 | `			int bInside = 0;` |
|         5 |  515 | `			while( nCur != 0 ){` |
|         5 |  516 | `				if( nCur == pLabel->nLoopId ){` |
|         5 |  517 | `					bInside = 1;` |
|         5 |  518 | `					break;` |
|         - |  519 | `				}` |
|       ! 0 |  520 | `				nCur = (nCur <= SySetUsed(&pGen->aLoopParent)) ? aParent[nCur - 1] : 0;` |
|       ! 0 |  521 | `			}` |
|         5 |  522 | `			if( !bInside ){` |
|       ! 0 |  523 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,` |
|         - |  524 | `					"'goto' into loop or switch statement is disallowed");` |
|       ! 0 |  525 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 |  526 | `					return SXERR_ABORT;` |
|         - |  527 | `				}` |
|       ! 0 |  528 | `				continue;` |
|         - |  529 | `			}` |
|         2 |  530 | `		}` |
|         - |  531 | `		/* What the jump crosses, and whether it is legal at all: the label's scope must` |
|         - |  532 | `		 * ENCLOSE the goto. Jumping INTO a try/catch/finally is fine in php (its handlers` |
|         - |  533 | `		 * are instruction RANGES, so landing anywhere in the body is being in the try),` |
|         - |  534 | `		 * but PHL pushes a handler at the try's OP_LOAD_EXCEPTION and runs a catch body` |
|         - |  535 | `		 * as a mini-program entered at its first instruction — there is no way to arrive` |
|         - |  536 | `		 * mid-body with the handler live. Say so rather than jump nowhere in silence,` |
|         - |  537 | `		 * skip a finally, or land in a foreign array. */` |
|       164 |  538 | `		if( !GenStateJumpScope(&(*pGen),pJump->nScopeId,pLabel->nScopeId,FALSE,&sCross) ){` |
|         6 |  539 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pJump->nLine,` |
|         - |  540 | `				"'goto' into a try, catch or finally block is disallowed");` |
|         6 |  541 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 |  542 | `				return SXERR_ABORT;` |
|         - |  543 | `			}` |
|         6 |  544 | `			continue;` |
|         - |  545 | `		}` |
|       160 |  546 | `		if( sCross.nFinally > 0 ){` |
|         - |  547 | `			/* php's other structural rule, shared with break/continue. Tested AFTER the` |
|         - |  548 | `			 * reach test above, so a goto that both leaves a finally and lands somewhere` |
|         - |  549 | `			 * that does not enclose it reports the into-a-try wording instead of this` |
|         - |  550 | `			 * one. Both are fatal on the same line, and the two cannot be told apart` |
|         - |  551 | `			 * without a second walk outward from the LABEL — php accepts one of them` |
|         - |  552 | ``			 * (`finally { goto L; try { L: … } }`), which is the recorded divergence, and`` |
|         - |  553 | `			 * rejects the other. Not worth a second walk for a message on input that is` |
|         - |  554 | `			 * rejected either way. */` |
|         3 |  555 | `			if( GenStateJumpOutOfFinally(&(*pGen),pJump->nLine) == SXERR_ABORT ){` |
|       ! 0 |  556 | `				return SXERR_ABORT;` |
|         - |  557 | `			}` |
|         3 |  558 | `			continue;` |
|         - |  559 | `		}` |
|         - |  560 | `		/* Fix the jump now the destination is resolved — in the container the goto was` |
|         - |  561 | `		 * emitted into, which for a goto inside a catch/finally body is not the one` |
|         - |  562 | `		 * current here (gotos resolve at end of compilation, after every swap back). */` |
|       158 |  563 | `		pInstr = GenStateFixupInstr(pJump);` |
|       158 |  564 | `		if( pInstr ){` |
|       158 |  565 | `			pInstr->iP2 = pLabel->nJumpDest;` |
|       158 |  566 | `			if( pInstr->iOp == PH7_OP_CATCH_JMP ){` |
|         - |  567 | `				/* Emitted as a structure-crossing jump because the goto sits inside a` |
|         - |  568 | `				 * try or a detached body. Now that the crossing is known it may well` |
|         - |  569 | `				 * turn out to leave nothing, and downgrade to a plain OP_JMP. */` |
|        47 |  570 | `				sxi32 iP1 = 0;` |
|        47 |  571 | `				pInstr->iOp = (sxu8)GenStateScopeJumpOp(&sCross,&iP1);` |
|        47 |  572 | `				pInstr->iP1 = iP1;` |
|        22 |  573 | `			}` |
|        77 |  574 | `		}` |
|        81 |  575 | `	}` |
|         - |  576 | `	/* php says nothing about a label nobody jumps to — the old "defined but not` |
|         - |  577 | `	 * referenced" warning was a PH7-ism with no counterpart in the oracle. */` |
|    223532 |  578 | `	return SXRET_OK;` |
|    111615 |  579 | `}` |
|         - |  580 | `/*` |
|         - |  581 | ` * Check if a given token value is installed in the literal table.` |
|         - |  582 | ` */` |
|   2089031 |  583 | `PH7_PRIVATE sxi32 GenStateFindLiteral(ph7_gen_state *pGen,const SyString *pValue,sxu32 *pIdx)` |
|         5 |  584 | `{` |
|         - |  585 | `	SyHashEntry *pEntry;` |
|   2089036 |  586 | `	pEntry = SyHashGet(&pGen->hLiteral,(const void *)pValue->zString,pValue->nByte);` |
|   2089036 |  587 | `	if( pEntry == 0 ){` |
|    872328 |  588 | `		return SXERR_NOTFOUND;` |
|         - |  589 | `	}` |
|   1216713 |  590 | `	*pIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|   1216713 |  591 | `	return SXRET_OK;` |
|   1042347 |  592 | `}` |
|         - |  593 | `/*` |
|         - |  594 | ` * Install a given constant index in the literal table.` |
|         - |  595 | ` * In order to be installed, the ph7_value must be of type string.` |
|         - |  596 | ` *` |
|         - |  597 | ` * NOTE: empty strings are deliberately omitted here.  The VM reserves a` |
|         - |  598 | ` * single shared constant for "" during initialization (pVm->nEmptyStringIdx)` |
|         - |  599 | ` * and the compiler emits a LOADC referencing that slot whenever an empty` |
|         - |  600 | ` * literal is encountered.  This keeps the literal hash from growing when` |
|         - |  601 | ` * many "" literals appear in user code.` |
|         - |  602 | ` */` |
|    872323 |  603 | `PH7_PRIVATE sxi32 GenStateInstallLiteral(ph7_gen_state *pGen,ph7_value *pObj,sxu32 nIdx)` |
|         5 |  604 | `{` |
|    872328 |  605 | `	if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|    872328 |  606 | `		SyHashInsert(&pGen->hLiteral,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),SX_INT_TO_PTR(nIdx));` |
|    435223 |  607 | `	}` |
|    872328 |  608 | `	return SXRET_OK;` |
|         5 |  609 | `}` |
|         - |  610 | `/*` |
|         - |  611 | ` * Reserve a room for a numeric constant [i.e: 64-bit integer or real number]` |
|         - |  612 | ` * in the constant table.` |
|         - |  613 | ` */` |
|    924558 |  614 | `PH7_PRIVATE ph7_value * GenStateInstallNumLiteral(ph7_gen_state *pGen,sxu32 *pIdx)` |
|         5 |  615 | `{` |
|         - |  616 | `	ph7_value *pObj;` |
|    924563 |  617 | `	sxu32 nIdx = 0; /* cc warning */` |
|         - |  618 | `	/* Reserve a new constant */` |
|    924563 |  619 | `	pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|    924563 |  620 | `	if( pObj == 0 ){` |
|       ! 0 |  621 | `		PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 |  622 | `		return 0;` |
|         - |  623 | `	}` |
|    924563 |  624 | `	*pIdx = nIdx;` |
|         - |  625 | `	/* TODO(chems): Create a numeric table (64bit int keys) same as` |
|         - |  626 | `	 * the constant string iterals table [optimization purposes].` |
|         - |  627 | `	 */` |
|    924563 |  628 | `	return pObj;` |
|    461612 |  629 | `}` |
|         - |  630 | `/*` |
|         - |  631 | ` * Implementation of the PHP language constructs.` |
|         - |  632 | ` */` |
|         - |  633 | `/*` |
|         - |  634 | ` * Ensure the about-to-be-emitted CALL/NEW opcode carries a VmCallArgMap` |
|         - |  635 | ` * that reflects the caller file's strict_types mode. Returns the (possibly` |
|         - |  636 | ` * newly allocated and zero-initialized) map pointer. In weak-mode files` |
|         - |  637 | ` * this is a no-op and the caller's p3 is returned unchanged.` |
|         - |  638 | ` *` |
|         - |  639 | ` * NOTE: on allocation failure the call reverts to weak semantics rather` |
|         - |  640 | ` * than aborting compilation — out-of-memory during a map allocation is` |
|         - |  641 | ` * vanishingly unlikely and silently dropping to weak mode matches the` |
|         - |  642 | ` * surrounding callsites' zero-check fallback pattern.` |
|         - |  643 | ` */` |
|   1238211 |  644 | `PH7_PRIVATE void *GenStateAttachStrictFlag(ph7_gen_state *pGen, void *p3)` |
|         5 |  645 | `{` |
|         - |  646 | `	VmCallArgMap *pMap;` |
|   1238216 |  647 | `	if( !pGen->bStrictTypes ) return p3;` |
|       521 |  648 | `	if( p3 == 0 ){` |
|        54 |  649 | `		pMap = (VmCallArgMap *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|        54 |  650 | `		if( pMap == 0 ) return 0;` |
|        54 |  651 | `		SyZero(pMap,sizeof(VmCallArgMap));` |
|        54 |  652 | `		p3 = (void *)pMap;` |
|        25 |  653 | `	}` |
|       521 |  654 | `	((VmCallArgMap *)p3)->bStrict = 1;` |
|       521 |  655 | `	return p3;` |
|    617867 |  656 | `}` |
|         - |  657 | `/* Forward declaration */` |
|         - |  658 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut);` |
|         - |  659 | `/* Forward declarations */` |
|         - |  660 | `/*` |
|         - |  661 | ` * Recover from a compile-time error. In other words synchronize` |
|         - |  662 | ` * the token stream cursor with the first semi-colon seen.` |
|         - |  663 | ` */` |
|         8 |  664 | `static sxi32 PH7_ErrorRecover(ph7_gen_state *pGen)` |
|         1 |  665 | `{` |
|         - |  666 | `	/* Synchronize with the next-semi-colon and avoid compiling this erroneous statement */` |
|        17 |  667 | `	while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI /*';'*/) == 0){` |
|         9 |  668 | `		pGen->pIn++;` |
|         1 |  669 | `	}` |
|         9 |  670 | `	return SXRET_OK;` |
|         1 |  671 | `}` |
|         - |  672 | `/*` |
|         - |  673 | ` * Check if the given identifier name is reserved or not.` |
|         - |  674 | ` * Return TRUE if reserved.FALSE otherwise.` |
|         - |  675 | ` */` |
|       202 |  676 | `PH7_PRIVATE int GenStateIsReservedConstant(SyString *pName)` |
|         5 |  677 | `{` |
|       207 |  678 | `	if( pName->nByte == sizeof("null") - 1 ){` |
|        22 |  679 | `		if( SyStrnicmp(pName->zString,"null",sizeof("null")-1) == 0 ){` |
|         3 |  680 | `			return TRUE;` |
|        20 |  681 | `		}else if( SyStrnicmp(pName->zString,"true",sizeof("true")-1) == 0 ){` |
|         5 |  682 | `			return TRUE;` |
|         4 |  683 | `		}` |
|       195 |  684 | `	}else if( pName->nByte == sizeof("false") - 1 ){` |
|        18 |  685 | `		if( SyStrnicmp(pName->zString,"false",sizeof("false")-1) == 0 ){` |
|         3 |  686 | `			return TRUE;` |
|         - |  687 | `		}` |
|         6 |  688 | `	}` |
|         - |  689 | `	/* Not a reserved constant */` |
|       199 |  690 | `	return FALSE;` |
|       106 |  691 | `}` |
|         - |  692 | `/*` |
|         - |  693 | ` * Chain operators participate in a postfix member-access chain.` |
|         - |  694 | `` * A `?->` emitted inside such a chain must short-circuit to the end of`` |
|         - |  695 | ` * the chain, not just past its own member access. Any non-chain ancestor` |
|         - |  696 | ` * terminates the chain and is where pending NULLSAFE_JMP targets are patched.` |
|         - |  697 | ` */` |
|         - |  698 | `#define GEN_IS_CHAIN_OP(iOp) \` |
|         - |  699 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|         - |  700 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT     \|\| \` |
|         - |  701 | `   (iOp) == EXPR_OP_FUNC_CALL)` |
|         - |  702 | `/*` |
|         - |  703 | ` * The chain operators that ACCESS a container -- the same four minus the call,` |
|         - |  704 | ` * whose result is an ordinary value however the chain around it is read. Used` |
|         - |  705 | ` * to spot an INTERMEDIATE link of an isset()/empty() chain, which php reads for` |
|         - |  706 | ` * its value rather than for a truth.` |
|         - |  707 | ` */` |
|         - |  708 | `#define GEN_IS_ACCESS_OP(iOp) \` |
|         - |  709 | `  ((iOp) == EXPR_OP_ARROW \|\| (iOp) == EXPR_OP_NULLSAFE_ARROW \|\| \` |
|         - |  710 | `   (iOp) == EXPR_OP_DC    \|\| (iOp) == EXPR_OP_SUBSCRIPT)` |
|         - |  711 |  |
|         - |  712 | `/*` |
|         - |  713 | ` * Patch every pending NULLSAFE_JMP recorded after the given baseline so` |
|         - |  714 | ` * that it jumps to the current end-of-emission instruction. Then drop the` |
|         - |  715 | ` * patched entries from the pending set.` |
|         - |  716 | ` */` |
|  10016215 |  717 | `static void GenStatePatchNullsafeJumps(ph7_gen_state *pGen, sxu32 nBaseline)` |
|         5 |  718 | `{` |
|  10016220 |  719 | `	sxu32 nCur = SySetUsed(&pGen->aNullsafeJmp);` |
|         - |  720 | `	sxu32 nTarget;` |
|         - |  721 | `	sxu32 *aIdx;` |
|         - |  722 | `	sxu32 i;` |
|  10016220 |  723 | `	if( nCur <= nBaseline ){` |
|  10016056 |  724 | `		return;` |
|         - |  725 | `	}` |
|       169 |  726 | `	aIdx = (sxu32 *)SySetBasePtr(&pGen->aNullsafeJmp);` |
|       169 |  727 | `	nTarget = PH7_VmInstrLength(pGen->pVm);` |
|       341 |  728 | `	for( i = nBaseline ; i < nCur ; ++i ){` |
|       177 |  729 | `		VmInstr *pInstr = PH7_VmGetInstr(pGen->pVm, aIdx[i]);` |
|       177 |  730 | `		if( pInstr ){` |
|       177 |  731 | `			pInstr->iP2 = (sxi32)nTarget;` |
|        86 |  732 | `		}` |
|        91 |  733 | `	}` |
|       169 |  734 | `	SySetTruncate(&pGen->aNullsafeJmp, nBaseline);` |
|   4999993 |  735 | `}` |
|         - |  736 |  |
|         - |  737 | `/*` |
|         - |  738 | `` * Does this call-argument node reach its target THROUGH a property? `$o->p`,`` |
|         - |  739 | ``  * `$this->m['k']`, `$o->a->b` all do; `$a['k']`, `$a[$i][$j]` and a plain `$var` `` |
|         - |  740 | ` * do not.` |
|         - |  741 | ` *` |
|         - |  742 | ` * Only the SUBSCRIPT spine is walked, because that is the only operator whose` |
|         - |  743 | `` * base is still part of the same lvalue: everything else (a call, a cast, `::`,`` |
|         - |  744 | `` * `?->`) either ends the path or is not writable through at all.`` |
|         - |  745 | ` */` |
|      9821 |  746 | `static int GenStateArgHasPropertyStep(ph7_expr_node *pNode)` |
|         5 |  747 | `{` |
|      9868 |  748 | `	while( pNode && pNode->pOp ){` |
|       130 |  749 | `		if( pNode->pOp->iOp == EXPR_OP_ARROW ){` |
|        80 |  750 | `			return 1;` |
|         - |  751 | `		}` |
|        51 |  752 | `		if( pNode->pOp->iOp != EXPR_OP_SUBSCRIPT ){` |
|         8 |  753 | `			return 0;` |
|         - |  754 | `		}` |
|        44 |  755 | `		pNode = pNode->pLeft;` |
|         2 |  756 | `	}` |
|      9742 |  757 | `	return 0;` |
|      4903 |  758 | `}` |
|         - |  759 | `/*` |
|         - |  760 | ` * By-reference out-parameters of builtin functions.` |
|         - |  761 | ` *` |
|         - |  762 | ` * PH7 foreign/builtin functions carry no parameter signature, so the call` |
|         - |  763 | ` * compiler cannot otherwise know that e.g. preg_match()'s 3rd argument` |
|         - |  764 | ` * ($matches) is passed by reference. Without that knowledge an *undefined*` |
|         - |  765 | ` * variable argument is compiled as a read-only load (EXPR_FLAG_RDONLY_LOAD)` |
|         - |  766 | ` * and reaches the builtin tagged nIdx == SXU32_HIGH, so the builtin's write-` |
|         - |  767 | ` * back is a silent no-op — the caller's variable stays null unless it was` |
|         - |  768 | ` * pre-initialised. This table maps a builtin name to a bitmask of the argument` |
|         - |  769 | ` * positions it writes back through, letting the caller auto-vivify just those` |
|         - |  770 | ` * argument variables (PHP's exact "passing an undefined var by reference` |
|         - |  771 | ` * creates it" behaviour).` |
|         - |  772 | ` *` |
|         - |  773 | ` * Bit N (1u<<N) set => the argument at position N is by reference. Out-params` |
|         - |  774 | ` * live at low indices, so a 32-bit mask is sufficient.` |
|         - |  775 | ` */` |
|   1121184 |  776 | `static sxu32 GenStateByRefBuiltinMask(SyString *pName)` |
|         5 |  777 | `{` |
|         - |  778 | `	static const struct {` |
|         - |  779 | `		const char *zName;` |
|         - |  780 | `		sxu32 nByte;` |
|         - |  781 | `		sxu32 mask;` |
|         - |  782 | `	} aByRef[] = {` |
|         - |  783 | `		{ "parse_str",              9, 1u<<1 },  /* &$result (apArg[1]) */` |
|         - |  784 | `		{ "settype",                7, 1u<<0 },  /* &$var    (apArg[0]) */` |
|         - |  785 | `		{ "preg_match",            10, 1u<<2 },  /* $matches (apArg[2]) */` |
|         - |  786 | `		{ "preg_match_all",        14, 1u<<2 },  /* $matches (apArg[2]) */` |
|         - |  787 | `		{ "preg_replace",          12, 1u<<4 },  /* &$count  (apArg[4]) */` |
|         - |  788 | `		{ "preg_replace_callback", 21, 1u<<4 },  /* &$count  (apArg[4]) */` |
|         - |  789 | `		{ "flock",                  5, 1u<<2 },  /* &$would_block (apArg[2]) */` |
|         - |  790 | `		{ "getopt",                 6, 1u<<2 },  /* &$rest_index (apArg[2]) */` |
|         - |  791 | `		{ "is_callable",           11, 1u<<2 },  /* &$callable_name (apArg[2]) */` |
|         - |  792 | `		{ "similar_text",          12, 1u<<2 },  /* &$percent (apArg[2]) */` |
|         - |  793 | `		{ "headers_sent",         12, (1u<<0)\|(1u<<1) },  /* &$filename, &$line */` |
|         - |  794 | `		{ "str_replace",           11, 1u<<3 },  /* &$count  (apArg[3]) */` |
|         - |  795 | `		{ "str_ireplace",          12, 1u<<3 },  /* &$count  (apArg[3]) */` |
|         - |  796 | `		{ "fsockopen",              9, (1u<<2)\|(1u<<3) },  /* &$error_code, &$error_message */` |
|         - |  797 | `		{ "pfsockopen",            10, (1u<<2)\|(1u<<3) },  /* same */` |
|         - |  798 | `		{ "stream_socket_client",  20, (1u<<1)\|(1u<<2) },  /* &$error_code, &$error_message */` |
|         - |  799 | `		{ "stream_socket_server",  20, (1u<<1)\|(1u<<2) },  /* same pair */` |
|         - |  800 | `		{ "stream_socket_accept",  20, 1u<<2 },            /* &$peer_name (apArg[2]) */` |
|         - |  801 | `		{ "stream_select",         13, (1u<<0)\|(1u<<1)\|(1u<<2) }, /* &$read, &$write, &$except */` |
|         - |  802 | `		{ "stream_socket_recvfrom",22, 1u<<3 },            /* &$address (apArg[3]) */` |
|         - |  803 | `		{ "proc_open",              9, 1u<<2 },  /* &$pipes (apArg[2]) */` |
|         - |  804 | `		{ "exec",                   4, (1u<<1)\|(1u<<2) },  /* &$output, &$result_code */` |
|         - |  805 | `		{ "system",                 6, 1u<<1 },  /* &$result_code (apArg[1]) */` |
|         - |  806 | `		{ "passthru",               8, 1u<<1 },  /* &$result_code (apArg[1]) */` |
|         - |  807 | `		/* A by-ref VARIADIC tail: every actual from the third on is one of` |
|         - |  808 | ``		 * sscanf()'s `&...$vars`, so each is created rather than read. */`` |
|         - |  809 | ``		/* ext/openssl's out-params. Each is the argument php declares `&$x`;`` |
|         - |  810 | `		 * the certificate half adds its own rows beside these. */` |
|         - |  811 | `		{ "openssl_encrypt",             15, 1u<<5 },  /* &$tag (apArg[5]) */` |
|         - |  812 | `		{ "openssl_random_pseudo_bytes", 27, 1u<<1 },  /* &$strong_result */` |
|         - |  813 | `		{ "openssl_pkey_export",         19, 1u<<1 },  /* &$output */` |
|         - |  814 | `		{ "openssl_sign",                12, 1u<<1 },  /* &$signature */` |
|         - |  815 | `		{ "openssl_private_encrypt",     23, 1u<<1 },  /* &$encrypted_data */` |
|         - |  816 | `		{ "openssl_private_decrypt",     23, 1u<<1 },  /* &$decrypted_data */` |
|         - |  817 | `		{ "openssl_public_encrypt",      22, 1u<<1 },  /* &$encrypted_data */` |
|         - |  818 | `		{ "openssl_public_decrypt",      22, 1u<<1 },  /* &$decrypted_data */` |
|         - |  819 | `		{ "openssl_seal",                12, (1u<<1)\|(1u<<2)\|(1u<<5) },` |
|         - |  820 | `		{ "openssl_open",                12, 1u<<1 },  /* &$output */` |
|         - |  821 | `		{ "openssl_x509_export",         19, 1u<<1 },  /* &$output */` |
|         - |  822 | `		{ "openssl_csr_export",          18, 1u<<1 },  /* &$output */` |
|         - |  823 | `		{ "openssl_csr_new",             15, 1u<<1 },  /* &$private_key */` |
|         - |  824 | `		{ "openssl_pkcs12_export",       21, 1u<<1 },  /* &$output */` |
|         - |  825 | `		{ "openssl_pkcs12_read",         19, 1u<<1 },  /* &$certificates */` |
|         - |  826 | `		{ "openssl_pkcs7_read",          18, 1u<<1 },  /* &$certificates */` |
|         - |  827 | `		{ "openssl_cms_read",            16, 1u<<1 },  /* &$certificates */` |
|         - |  828 | ``		/* ext/pcntl's out-params. `pcntl_signal_dispatch` has none; every`` |
|         - |  829 | `		 * other by-reference argument in the extension is one of these. */` |
|         - |  830 | `		{ "pcntl_waitpid",         13, (1u<<1)\|(1u<<3) }, /* &$status, &$resource_usage */` |
|         - |  831 | `		{ "pcntl_wait",            10, (1u<<0)\|(1u<<2) }, /* &$status, &$resource_usage */` |
|         - |  832 | `		{ "pcntl_waitid",          12, (1u<<2)\|(1u<<4) }, /* &$info,   &$resource_usage */` |
|         - |  833 | `		{ "pcntl_sigprocmask",     17, 1u<<2 },           /* &$old_signals */` |
|         - |  834 | `		{ "pcntl_sigwaitinfo",     17, 1u<<1 },           /* &$info */` |
|         - |  835 | `		{ "pcntl_sigtimedwait",    18, 1u<<1 },           /* &$info */` |
|         - |  836 | `		{ "sscanf",                 6, ~((1u<<2) - 1u) },` |
|         - |  837 | `		{ "fscanf",                 6, ~((1u<<2) - 1u) },` |
|         - |  838 | `	};` |
|         - |  839 | `	sxu32 i;` |
|   1121189 |  840 | `	if( pName == 0 \|\| pName->zString == 0 \|\| pName->nByte == 0 ){` |
|     29303 |  841 | `		return 0;` |
|         - |  842 | `	}` |
|  54106750 |  843 | `	for( i = 0 ; i < SX_ARRAYSIZE(aByRef) ; ++i ){` |
|  53025892 |  844 | `		if( pName->nByte == aByRef[i].nByte` |
|  27795306 |  845 | `		 && SyStrnicmp(pName->zString, aByRef[i].zName, pName->nByte) == 0 ){` |
|     11038 |  846 | `			return aByRef[i].mask;` |
|         - |  847 | `		}` |
|  26452263 |  848 | `	}` |
|   1080858 |  849 | `	return 0;` |
|    559435 |  850 | `}` |
|         - |  851 | `/*` |
|         - |  852 | ` * What may be passed by REFERENCE is decided from the argument's SHAPE, at compile` |
|         - |  853 | ` * time, exactly as php decides it (zend_compile_args -> zend_is_variable).` |
|         - |  854 | ` *` |
|         - |  855 | ` * php sorts every actual argument into three buckets:` |
|         - |  856 | ` *` |
|         - |  857 | ` *   GEN_ARG_LVALUE   a variable, an element, a property, a static property. It has a` |
|         - |  858 | ` *                    slot, so a by-ref parameter aliases it.` |
|         - |  859 | `` *   GEN_ARG_TEMPCALL the result of a call or of `new`. It has no slot, but php cannot`` |
|         - |  860 | ` *                    know at compile time whether the callee returns a reference, so it` |
|         - |  861 | ` *                    defers: E_NOTICE "Only variables should be passed by reference",` |
|         - |  862 | ` *                    then it operates on the temporary.` |
|         - |  863 | ` *   GEN_ARG_NONE     everything else — a literal, an operator/cast result, a class` |
|         - |  864 | `` *                    constant, `@$x`, `$o?->p`, an assignment. Binding one to a by-ref`` |
|         - |  865 | ` *                    parameter is a catchable Error at the CALL.` |
|         - |  866 | ` *` |
|         - |  867 | ` * Deciding it from the argument's runtime memobj instead does not work and was silently` |
|         - |  868 | ` * wrong in both directions: an arithmetic or concatenation result keeps its LEFT operand's` |
|         - |  869 | `` * slot index, so `f($i + 1)` with `function f(&$x)` aliased and overwrote `$i`; and a`` |
|         - |  870 | ` * builtin's by-ref row saw only "no slot", which a call result has too.` |
|         - |  871 | ` */` |
|         - |  872 | `#define GEN_ARG_LVALUE   0` |
|         - |  873 | `#define GEN_ARG_TEMPCALL 1` |
|         - |  874 | `#define GEN_ARG_NONE     2` |
|   1591913 |  875 | `static int GenStateArgShape(ph7_expr_node *pNode)` |
|         5 |  876 | `{` |
|   1591918 |  877 | `	if( pNode == 0 ){` |
|       ! 0 |  878 | `		return GEN_ARG_NONE;` |
|         - |  879 | `	}` |
|   1591918 |  880 | `	if( pNode->pOp == 0 ){` |
|         - |  881 | ``		/* A leaf: only the `$…` family is a variable. Everything else the parser`` |
|         - |  882 | ``		 * files here — a literal, an array/list constructor, a closure, `match`,`` |
|         - |  883 | ``		 * `clone` — is a temporary. */`` |
|   1292443 |  884 | `		return pNode->xCode == PH7_CompileVariable ? GEN_ARG_LVALUE : GEN_ARG_NONE;` |
|         - |  885 | `	}` |
|    299480 |  886 | `	switch( pNode->pOp->iOp ){` |
|     22715 |  887 | `	case EXPR_OP_SUBSCRIPT: /* $a[k], and any base: php accepts g()[0] and C::m()[0] */` |
|         - |  888 | `	case EXPR_OP_ARROW:     /* $o->p */` |
|     45376 |  889 | `		return GEN_ARG_LVALUE;` |
|       545 |  890 | `	case EXPR_OP_DC:` |
|         - |  891 | ``		/* `C::$s` is a static property (an lvalue); `C::K` is a class constant and`` |
|         - |  892 | ``		 * `C::CASE` an enum case, neither of which php will bind. The right operand`` |
|         - |  893 | `		 * tells them apart. */` |
|      1640 |  894 | `		return ( pNode->pRight && pNode->pRight->pOp == 0` |
|      1090 |  895 | `		      && pNode->pRight->xCode == PH7_CompileVariable )` |
|      1090 |  896 | `			? GEN_ARG_LVALUE : GEN_ARG_NONE;` |
|     43399 |  897 | `	case EXPR_OP_FUNC_CALL:` |
|         - |  898 | `	case EXPR_OP_NEW:` |
|     86591 |  899 | `		return GEN_ARG_TEMPCALL;` |
|         2 |  900 | `	case EXPR_OP_REF:` |
|         - |  901 | ``		/* `take($q = &$p)`: a reference ASSIGNMENT hands back the reference it`` |
|         - |  902 | `		 * made, so php passes it on to a by-ref parameter and all three names end` |
|         - |  903 | ``		 * up aliasing one slot. A plain `$q = $p` does not -- php's ASSIGN yields`` |
|         - |  904 | `		 * a temporary where ASSIGN_REF yields the VAR -- which is why only this` |
|         - |  905 | `		 * one arm moves. */` |
|         5 |  906 | `		return GEN_ARG_LVALUE;` |
|     83340 |  907 | `	default:` |
|         - |  908 | ``		/* Includes `?->` (php: "Cannot use nullsafe operator in write context"),`` |
|         - |  909 | ``		 * `@$x`, `$q = …`, `clone $o` and every arithmetic/logical operator. */`` |
|    166429 |  910 | `		return GEN_ARG_NONE;` |
|         - |  911 | `	}` |
|    794242 |  912 | `}` |
|         - |  913 | `/*` |
|         - |  914 | ` * Can evaluating this argument expression RUN anything?` |
|         - |  915 | ` *` |
|         - |  916 | ` * php materializes every by-value argument where it is written, so an argument already` |
|         - |  917 | ` * pushed cannot see what a later one does. PHL pushes an aliasing view of the source's` |
|         - |  918 | ` * bytes instead, which is only equivalent while nothing between the two pushes can write.` |
|         - |  919 | ` * This is the question that decides it, asked of every argument that FOLLOWS the one in` |
|         - |  920 | `` * hand: a plain `$var` read and a scalar literal execute nothing, so the alias is safe`` |
|         - |  921 | `` * beside them; every other shape -- an assignment, a call, `new`, `++`, a property or`` |
|         - |  922 | ` * element fetch (which may reach __get / offsetGet), an interpolated string, an array` |
|         - |  923 | `` * constructor, `match`, a closure with a by-reference `use` -- either writes or hands`` |
|         - |  924 | ` * control to code that can, so the earlier arguments are copied first (PH7_OP_SNAPSHOT).` |
|         - |  925 | ` *` |
|         - |  926 | ` * Deliberately answered from the SHAPE and not from what the shape is likely to do: the` |
|         - |  927 | ` * cost of a false yes is one copy of an argument the callee was about to copy anyway,` |
|         - |  928 | ` * and the cost of a false no is a silent wrong value.` |
|         - |  929 | ` */` |
|   2853066 |  930 | `static int GenStateArgRunsCode(ph7_expr_node *pNode)` |
|         5 |  931 | `{` |
|   2853071 |  932 | `	if( pNode == 0 ){` |
|       ! 0 |  933 | `		return 0;` |
|         - |  934 | `	}` |
|   2853071 |  935 | `	if( pNode->pOp == 0 ){` |
|         - |  936 | ``		/* A leaf. Only these read without running: `$x` (and `$$x`, whose name`` |
|         - |  937 | `		 * comes off the stack and still runs nothing), a number, and a string with no` |
|         - |  938 | `		 * interpolation in it. A bare identifier is a constant lookup -- a table read` |
|         - |  939 | `		 * with no user code behind it in php either. */` |
|   3890091 |  940 | `		return !( pNode->xCode == PH7_CompileVariable` |
|   1944331 |  941 | `		       \|\| pNode->xCode == PH7_CompileLiteral` |
|   1307475 |  942 | `		       \|\| pNode->xCode == PH7_CompileNumLiteral` |
|    841900 |  943 | `		       \|\| pNode->xCode == PH7_CompileSimpleString` |
|    294826 |  944 | `		       \|\| pNode->xCode == PH7_CompileNowDoc );` |
|         - |  945 | `	}` |
|         - |  946 | ``	/* An operator node, and none of them is whitelisted: `.` and the arithmetic`` |
|         - |  947 | `	 * operators reach __toString, the fetches reach __get and offsetGet, and the rest` |
|         - |  948 | `	 * write outright. Answering yes for the whole family also spares this a subtree` |
|         - |  949 | `	 * walk -- a nested call is an operator node at its own root. */` |
|    441635 |  950 | `	return 1;` |
|   1424081 |  951 | `}` |
|         - |  952 | `/*` |
|         - |  953 | `` * Is this node the plain variable `$name`?`` |
|         - |  954 | ` *` |
|         - |  955 | `` * `$$name`, `${expr}`, `$a[0]`, `$o->p` and `C::$s` are all excluded: php materializes`` |
|         - |  956 | ` * every one of those where it is written and re-reads only a compiled variable, so the` |
|         - |  957 | ` * two halves of the operand rule below turn on exactly this question.` |
|         - |  958 | ` */` |
|    172222 |  959 | `static int GenStateNodeIsSimpleVar(ph7_expr_node *pNode)` |
|         5 |  960 | `{` |
|         - |  961 | `	SyToken *pTok;` |
|    172227 |  962 | `	if( pNode == 0 \|\| pNode->pOp != 0 \|\| pNode->xCode != PH7_CompileVariable ){` |
|    115490 |  963 | `		return 0;` |
|         - |  964 | `	}` |
|     56742 |  965 | `	pTok = pNode->pStart;` |
|     56742 |  966 | `	if( pTok == 0 \|\| pNode->pEnd != &pTok[2] ){` |
|         3 |  967 | `		return 0;` |
|         - |  968 | `	}` |
|     85065 |  969 | `	return (pTok[0].nType & PH7_TK_DOLLAR) != 0` |
|     56735 |  970 | `	    && (pTok[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0;` |
|     85989 |  971 | `}` |
|         - |  972 | `/*` |
|         - |  973 | ` * Does this operator take the VALUE of both operands, with nothing between them?` |
|         - |  974 | ` *` |
|         - |  975 | `` * The concatenation, arithmetic, shift, bitwise, comparison and `xor` operators, which`` |
|         - |  976 | ``  * evaluate their left operand, then their right, and then read both. `&&`, `\|\|` and `??` `` |
|         - |  977 | ` * are excluded because they may not evaluate the right operand at all -- and when they do,` |
|         - |  978 | ` * the left one has already been consumed by the short-circuit test. Everything else here` |
|         - |  979 | ` * is either unary, an assignment (whose operands parse.c has swapped), or an access.` |
|         - |  980 | ` */` |
|   4459459 |  981 | `static int GenStateBinOpReadsBothOperands(sxi32 iVmOp)` |
|         5 |  982 | `{` |
|   4459464 |  983 | `	switch( iVmOp ){` |
|    692929 |  984 | `	case PH7_OP_CAT:` |
|         - |  985 | `	case PH7_OP_ADD: case PH7_OP_SUB: case PH7_OP_MUL:` |
|         - |  986 | `	case PH7_OP_DIV: case PH7_OP_MOD: case PH7_OP_POW:` |
|         - |  987 | `	case PH7_OP_SHL: case PH7_OP_SHR:` |
|         - |  988 | `	case PH7_OP_BAND: case PH7_OP_BOR: case PH7_OP_BXOR: case PH7_OP_LXOR:` |
|         - |  989 | `	case PH7_OP_LT: case PH7_OP_LE: case PH7_OP_GT: case PH7_OP_GE:` |
|         - |  990 | `	case PH7_OP_EQ: case PH7_OP_NEQ: case PH7_OP_TEQ: case PH7_OP_TNE:` |
|         - |  991 | `	case PH7_OP_SPACESHIP:` |
|   1383876 |  992 | `		return 1;` |
|   1540356 |  993 | `	default:` |
|   3075593 |  994 | `		return 0;` |
|         - |  995 | `	}` |
|   2226179 |  996 | `}` |
|         - |  997 | `/*` |
|         - |  998 | ` * Can the value this expression leaves on the stack be a VIEW of storage user code can` |
|         - |  999 | ` * still write to?` |
|         - | 1000 | ` *` |
|         - | 1001 | `` * A variable read of any spelling -- `$x`, `$$x`, `$a[0]`, `$o->p`, `C::$s` -- pushes the`` |
|         - | 1002 | ` * source's own bytes (PH7_MemObjLoad sets SXBLOB_RDONLY and points at them), and so does` |
|         - | 1003 | `` * anything that merely SELECTS one of those: a ternary, `??`, `@`, an assignment (which`` |
|         - | 1004 | `` * hands back what it stored), a short-circuit `&&`/`\|\|` (whose jump keeps the operand`` |
|         - | 1005 | ` * itself), and a CAST, which for a string already a string is a no-op that keeps the view.` |
|         - | 1006 | `` * An operator's own result, a call's or `new`'s return, a literal, an array constructor and`` |
|         - | 1007 | ` * a closure are values the expression owns and nobody can reach.` |
|         - | 1008 | ` *` |
|         - | 1009 | ` * Answered from the SHAPE, and the unknown shape answers YES: a needless copy costs one` |
|         - | 1010 | ` * instruction, a missed one is a silently wrong value.` |
|         - | 1011 | ` */` |
|    115487 | 1012 | `static int GenStateNodeMayAliasStorage(ph7_expr_node *pNode)` |
|         5 | 1013 | `{` |
|    115492 | 1014 | `	if( pNode == 0 ){` |
|       ! 0 | 1015 | `		return 0;` |
|         - | 1016 | `	}` |
|    115492 | 1017 | `	if( pNode->pOp == 0 ){` |
|     28742 | 1018 | `		return pNode->xCode == PH7_CompileVariable;` |
|         - | 1019 | `	}` |
|     86755 | 1020 | `	if( GenStateBinOpReadsBothOperands(pNode->pOp->iVmOp) ){` |
|     59836 | 1021 | `		return 0;` |
|         - | 1022 | `	}` |
|     26924 | 1023 | `	switch( pNode->pOp->iVmOp ){` |
|      5031 | 1024 | `	case PH7_OP_CALL: case PH7_OP_NEW: case PH7_OP_CLONE: case PH7_OP_IS_A:` |
|         - | 1025 | `	case PH7_OP_UMINUS: case PH7_OP_UPLUS: case PH7_OP_BITNOT: case PH7_OP_LNOT:` |
|     10043 | 1026 | `		return 0;` |
|      8453 | 1027 | `	default:` |
|     16886 | 1028 | `		return 1;` |
|         - | 1029 | `	}` |
|     57664 | 1030 | `}` |
|         - | 1031 | `/*` |
|         - | 1032 | ` * Re-emit an instruction that was popped, exactly as it stood.` |
|         - | 1033 | ` *` |
|         - | 1034 | ` * PH7_VmEmitInstr stamps the CURRENT token's line and strict_types mode, which for a` |
|         - | 1035 | ` * moved instruction is the wrong position -- the codegen cursor has walked past the` |
|         - | 1036 | ` * operand it belongs to. Overwriting the fresh entry with the saved one keeps the source` |
|         - | 1037 | ` * line a diagnostic will name.` |
|         - | 1038 | ` */` |
|     56735 | 1039 | `static void GenStateReEmitInstr(ph7_gen_state *pGen,const VmInstr *pSaved)` |
|         5 | 1040 | `{` |
|         - | 1041 | `	VmInstr *pNew;` |
|     56740 | 1042 | `	PH7_VmEmitInstr(pGen->pVm,pSaved->iOp,pSaved->iP1,pSaved->iP2,pSaved->p3,0);` |
|     56740 | 1043 | `	pNew = PH7_VmPeekInstr(pGen->pVm);` |
|     56740 | 1044 | `	if( pNew ){` |
|     56740 | 1045 | `		*pNew = *pSaved;` |
|     28325 | 1046 | `	}` |
|     56740 | 1047 | `}` |
|         - | 1048 | `/*` |
|         - | 1049 | ` * Recover the bare global-builtin name from a call's callee node.` |
|         - | 1050 | ` *` |
|         - | 1051 | `` * Handles the unqualified form `preg_match(...)` (a single PH7_TK_ID token) and`` |
|         - | 1052 | `` * the absolute single-component form `\preg_match(...)` (a leading PH7_TK_NSSEP`` |
|         - | 1053 | ` * then one identifier) — both resolve to the global builtin. A deeper-qualified` |
|         - | 1054 | `` * name (`Foo\preg_match`, `\Foo\bar`) is a *different* function, so no name is`` |
|         - | 1055 | ` * returned for it. pEnd is exclusive (one past the last name token). Returns` |
|         - | 1056 | ` * {NULL,0} in *pOut when the callee is not a plain global function name.` |
|         - | 1057 | ` */` |
|   2201693 | 1058 | `static void GenStateCallBuiltinName(ph7_expr_node *pLeft, SyString *pOut)` |
|         5 | 1059 | `{` |
|         - | 1060 | `	SyToken *p, *pEnd;` |
|   2201698 | 1061 | `	pOut->zString = 0;` |
|   2201698 | 1062 | `	pOut->nByte = 0;` |
|   2201698 | 1063 | `	if( pLeft == 0 \|\| pLeft->pStart == 0 \|\| pLeft->pEnd == 0 ){` |
|       ! 0 | 1064 | `		return;` |
|         - | 1065 | `	}` |
|   2201698 | 1066 | `	p = pLeft->pStart;` |
|   2201698 | 1067 | `	pEnd = pLeft->pEnd;` |
|         - | 1068 | `	/* Optional single leading namespace separator (absolute path). */` |
|   2201698 | 1069 | `	if( p < pEnd && (p->nType & PH7_TK_NSSEP) ){` |
|       267 | 1070 | `		p++;` |
|       131 | 1071 | `	}` |
|   2201698 | 1072 | `	if( p >= pEnd \|\| (p->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) == 0 ){` |
|     42302 | 1073 | `		return;` |
|         - | 1074 | `	}` |
|         - | 1075 | `	/* Must be a single component: nothing follows the name token. */` |
|   2159401 | 1076 | `	if( p + 1 != pEnd ){` |
|       562 | 1077 | `		return;` |
|         - | 1078 | `	}` |
|   2158844 | 1079 | `	*pOut = p->sData;` |
|   1098606 | 1080 | `}` |
|         - | 1081 | `/*` |
|         - | 1082 | `` * Is this expression node the bare variable `$this`?`` |
|         - | 1083 | ` */` |
|   1055052 | 1084 | `PH7_PRIVATE int PH7_ExprNodeIsThis(ph7_expr_node *pNode)` |
|         5 | 1085 | `{` |
|         - | 1086 | `	SyToken *pTok;` |
|   1055057 | 1087 | `	if( pNode == 0 \|\| pNode->pOp != 0 \|\| pNode->xCode != PH7_CompileVariable ){` |
|    197329 | 1088 | `		return 0;` |
|         - | 1089 | `	}` |
|    857733 | 1090 | `	pTok = pNode->pStart;` |
|    857733 | 1091 | `	if( pTok == 0 \|\| pNode->pEnd == 0 \|\| pNode->pEnd < &pTok[2] ){` |
|       ! 0 | 1092 | `		return 0;` |
|         - | 1093 | `	}` |
|   1285968 | 1094 | `	return (pTok[0].nType & PH7_TK_DOLLAR) != 0` |
|    857728 | 1095 | `		&& (pTok[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) != 0` |
|    857716 | 1096 | `		&& pTok[1].sData.nByte == sizeof("this")-1` |
|   1287221 | 1097 | `		&& SyMemcmp((const void *)pTok[1].sData.zString,(const void *)"this",sizeof("this")-1) == 0;` |
|    526765 | 1098 | `}` |
|         - | 1099 | `/*` |
|         - | 1100 | ` * TRUE when codegen is inside a real FUNCTION body — php's` |
|         - | 1101 | `` * `CG(active_op_array)->function_name`. A synthetic block (a match() arm's`` |
|         - | 1102 | ` * throw-fixup) carries no ph7_vm_func and is not a scope.` |
|         - | 1103 | ` */` |
|         2 | 1104 | `static int GenStateInFunction(ph7_gen_state *pGen)` |
|         1 | 1105 | `{` |
|         3 | 1106 | `	GenBlock *pBlock = pGen->pCurrent;` |
|         9 | 1107 | `	while( pBlock ){` |
|         7 | 1108 | `		if( (pBlock->iFlags & GEN_BLOCK_FUNC) && pBlock->pUserData ){` |
|       ! 0 | 1109 | `			return 1;` |
|         - | 1110 | `		}` |
|         7 | 1111 | `		pBlock = pBlock->pParent;` |
|         1 | 1112 | `	}` |
|         3 | 1113 | `	return 0;` |
|         2 | 1114 | `}` |
|         - | 1115 | `/*` |
|         - | 1116 | `` * php's SPECIALIZED builtins — the list behind `Cannot use result of built-in`` |
|         - | 1117 | `` * function in write context`.`` |
|         - | 1118 | ` *` |
|         - | 1119 | ` * The wording says "built-in function" but the rule is not about builtins: php` |
|         - | 1120 | ` * refuses the write when the call was compiled to an OPCODE of its own rather` |
|         - | 1121 | ` * than a real call, because a specialized opcode leaves a TMP where a call` |
|         - | 1122 | `` * leaves a VAR (`zend_separate_if_call_and_write`). So `strlen("x")[0] = 1` and`` |
|         - | 1123 | `` * `count([1])[0] = 1` are compile fatals while `array_values([1])[0] = 2`,`` |
|         - | 1124 | `` * `str_split("ab")[0] = "z"` and `get_object_vars($o)["k"] = 2` all RUN — the`` |
|         - | 1125 | `` * difference being php's `zend_try_compile_special_func_ex` table, reproduced`` |
|         - | 1126 | ` * here name for name with the ARITY each entry demands.` |
|         - | 1127 | ` *` |
|         - | 1128 | ` * Six of php's names are deliberately absent, and only the last pair is a` |
|         - | 1129 | ` * simplification — the other four are not refusals of php's at all:` |
|         - | 1130 | `` *   `chr`/`ord` gate on BP_VAR_R, so they are never special in a WRITE context;`` |
|         - | 1131 | `` *   `call_user_func`/`call_user_func_array` emit a REAL call, so their result is`` |
|         - | 1132 | ` *     a VAR and php does not refuse a write through it either;` |
|         - | 1133 | `` *   `in_array` and `array_slice` gate on the CONTENTS of a literal array`` |
|         - | 1134 | `` *     argument and on a `func_get_args()`-shaped first argument — value-dependent`` |
|         - | 1135 | ` *     shapes no program writes through, left out under the scope policy. Leaving them out` |
|         - | 1136 | ` *     ACCEPTS where php refuses, which is the direction that keeps running a` |
|         - | 1137 | ` *     program php runs.` |
|         - | 1138 | ` * Verified by sweeping every internal function of both engines at arities 0-3:` |
|         - | 1139 | ` * the two specialized sets are identical, 27 names at the same arities.` |
|         - | 1140 | ` */` |
|         - | 1141 | `#define SPECFN_LITERAL_ARG0 0x01 /* php gives up unless argument #1 is a literal */` |
|         - | 1142 | ``#define SPECFN_ANY_ARGS     0x02 /* …and `assert` is decided BEFORE php's unpack/named`` |
|         - | 1143 | ``                                  * bail, so it stays special even for `assert(...$a)` */`` |
|         - | 1144 | `#define SPECFN_IN_FUNC      0x04 /* php's gate reads CG(active_op_array)->function_name:` |
|         - | 1145 | `                                  * at GLOBAL scope it emits a real call, whose runtime` |
|         - | 1146 | `                                  * Error ("cannot be called from the global scope") is` |
|         - | 1147 | `                                  * what the program actually gets */` |
|         - | 1148 | ``#define SPECFN_FORMAT_ARG0  0x08 /* …and `sprintf` also needs php's format arithmetic`` |
|         - | 1149 | `                                  * (implies SPECFN_LITERAL_ARG0) */` |
|         - | 1150 | `static const struct {` |
|         - | 1151 | `	const char *zName;` |
|         - | 1152 | `	int nMinArg;   /* inclusive */` |
|         - | 1153 | `	int nMaxArg;   /* inclusive; -1 = variadic */` |
|         - | 1154 | `	int iFlags;` |
|         - | 1155 | `} aSpecialFunc[] = {` |
|         - | 1156 | `	{ "strlen",           1,  1, 0 },` |
|         - | 1157 | `	{ "is_null",          1,  1, 0 },  { "is_bool",          1,  1, 0 },` |
|         - | 1158 | `	{ "is_long",          1,  1, 0 },  { "is_int",           1,  1, 0 },` |
|         - | 1159 | `	{ "is_integer",       1,  1, 0 },  { "is_float",         1,  1, 0 },` |
|         - | 1160 | `	{ "is_double",        1,  1, 0 },  { "is_string",        1,  1, 0 },` |
|         - | 1161 | `	{ "is_array",         1,  1, 0 },  { "is_object",        1,  1, 0 },` |
|         - | 1162 | `	{ "is_resource",      1,  1, 0 },  { "is_scalar",        1,  1, 0 },` |
|         - | 1163 | `	{ "boolval",          1,  1, 0 },  { "intval",           1,  1, 0 },` |
|         - | 1164 | `	{ "floatval",         1,  1, 0 },  { "doubleval",        1,  1, 0 },` |
|         - | 1165 | `	{ "strval",           1,  1, 0 },` |
|         - | 1166 | `	{ "count",            1,  1, 0 },  { "sizeof",           1,  1, 0 },` |
|         - | 1167 | `	{ "get_class",        0,  1, 0 },  { "get_called_class", 0,  0, 0 },` |
|         - | 1168 | `	{ "gettype",          1,  1, 0 },` |
|         - | 1169 | `	{ "func_num_args",    0,  0, SPECFN_IN_FUNC },` |
|         - | 1170 | `	{ "func_get_args",    0,  0, SPECFN_IN_FUNC },` |
|         - | 1171 | `	{ "array_key_exists", 2,  2, 0 },` |
|         - | 1172 | `	{ "defined",          1,  1, SPECFN_LITERAL_ARG0 },` |
|         - | 1173 | `	{ "sprintf",          1, -1, SPECFN_LITERAL_ARG0\|SPECFN_FORMAT_ARG0 },` |
|         - | 1174 | `	/* php compiles assert() to its own opcode pair "independently of compiler` |
|         - | 1175 | `	 * flags", in zend_compile_call BEFORE the special-func table is consulted —` |
|         - | 1176 | `	 * so every arity counts and an unpacked argument does not exempt it. */` |
|         - | 1177 | `	{ "assert",           0, -1, SPECFN_ANY_ARGS },` |
|         - | 1178 | `};` |
|         - | 1179 | `/*` |
|         - | 1180 | ` * TRUE when this call node is one php compiles to an opcode of its own, so a` |
|         - | 1181 | ` * write THROUGH its result is php's built-in-function refusal. pName is the` |
|         - | 1182 | ` * callee's bare global name, already resolved by GenStateCallBuiltinName.` |
|         - | 1183 | ` */` |
|       160 | 1184 | `static int GenStateCallIsSpecialized(ph7_gen_state *pGen,ph7_expr_node *pCall,SyString *pName)` |
|         5 | 1185 | `{` |
|         - | 1186 | `	ph7_expr_node **apArg;` |
|         - | 1187 | `	sxu32 nArg, n;` |
|         - | 1188 | `	sxu32 i;` |
|       165 | 1189 | `	if( pName->nByte < 1 ){` |
|        61 | 1190 | `		return 0;` |
|         - | 1191 | `	}` |
|       106 | 1192 | `	apArg = (ph7_expr_node **)SySetBasePtr(&pCall->aNodeArgs);` |
|       106 | 1193 | `	nArg = SySetUsed(&pCall->aNodeArgs);` |
|      2818 | 1194 | `	for( i = 0 ; i < SX_ARRAYSIZE(aSpecialFunc) ; ++i ){` |
|         - | 1195 | `		SyString sEntry;` |
|      2728 | 1196 | `		SyStringInitFromBuf(&sEntry,aSpecialFunc[i].zName,SyStrlen(aSpecialFunc[i].zName));` |
|      2724 | 1197 | `		if( sEntry.nByte != pName->nByte` |
|      1564 | 1198 | `		 \|\| SyStrnicmp(sEntry.zString,pName->zString,pName->nByte) != 0 ){` |
|      2714 | 1199 | `			continue;` |
|         - | 1200 | `		}` |
|        12 | 1201 | `		if( (int)nArg < aSpecialFunc[i].nMinArg` |
|        16 | 1202 | `		 \|\| (aSpecialFunc[i].nMaxArg >= 0 && (int)nArg > aSpecialFunc[i].nMaxArg) ){` |
|        10 | 1203 | `			return 0;` |
|         - | 1204 | `		}` |
|         - | 1205 | `		/* php bails out of the whole table when any argument unpacks or is named` |
|         - | 1206 | ``		 * (`zend_args_contain_unpack_or_named`), so `strlen(...$a)[0] = 1` RUNS. */`` |
|        11 | 1207 | `		if( (aSpecialFunc[i].iFlags & SPECFN_ANY_ARGS) == 0 ){` |
|        17 | 1208 | `			for( n = 0 ; n < nArg ; ++n ){` |
|        11 | 1209 | `				if( apArg[n] && (apArg[n]->iFlags & (EXPR_NODE_SPREAD\|EXPR_NODE_NAMED_ARG)) ){` |
|         3 | 1210 | `					return 0;` |
|         - | 1211 | `				}` |
|         5 | 1212 | `			}` |
|         3 | 1213 | `		}` |
|         9 | 1214 | `		if( (aSpecialFunc[i].iFlags & SPECFN_IN_FUNC) && !GenStateInFunction(pGen) ){` |
|         3 | 1215 | `			return 0;` |
|         - | 1216 | `		}` |
|         - | 1217 | ``		/* `defined` and `sprintf` specialize only over a LITERAL first argument;`` |
|         - | 1218 | `		 * php gives up on a computed one and emits an ordinary call. */` |
|         6 | 1219 | `		if( aSpecialFunc[i].iFlags & SPECFN_LITERAL_ARG0 ){` |
|         2 | 1220 | `			if( nArg < 1 \|\| apArg[0] == 0 \|\| apArg[0]->pOp != 0` |
|         2 | 1221 | `			 \|\| apArg[0]->pStart == 0` |
|         3 | 1222 | `			 \|\| (apArg[0]->pStart->nType & (PH7_TK_SSTR\|PH7_TK_DSTR)) == 0 ){` |
|       ! 0 | 1223 | `				return 0;` |
|         - | 1224 | `			}` |
|         1 | 1225 | `		}` |
|         6 | 1226 | `		if( (aSpecialFunc[i].iFlags & SPECFN_FORMAT_ARG0) && nArg >= 1 && apArg[0] ){` |
|         - | 1227 | `			/* php's own sprintf gate, and it is arithmetic: a format under 256` |
|         - | 1228 | ``			 * bytes carrying nothing but `%s`, `%d` and `%%`, with exactly one`` |
|         - | 1229 | ``			 * VALUE per placeholder. `sprintf("a","b")` fails it (no placeholder,`` |
|         - | 1230 | `			 * one value) and compiles to an ordinary call, which is why the write` |
|         - | 1231 | `			 * through it RUNS. */` |
|         3 | 1232 | `			const SyString *pFmt = &apArg[0]->pStart->sData;` |
|         3 | 1233 | `			sxu32 nPlace = 0, k;` |
|         3 | 1234 | `			if( pFmt->nByte >= 256 ){` |
|       ! 0 | 1235 | `				return 0;` |
|         - | 1236 | `			}` |
|         - | 1237 | `			/* An escape or an interpolation makes php's argument something other` |
|         - | 1238 | `			 * than a plain literal; leave those to the ordinary call. */` |
|         7 | 1239 | `			for( k = 0 ; k < pFmt->nByte ; ++k ){` |
|         4 | 1240 | `				if( pFmt->zString[k] == '\\'` |
|         7 | 1241 | `				 \|\| ((apArg[0]->pStart->nType & PH7_TK_DSTR)` |
|         4 | 1242 | `				  && (pFmt->zString[k] == '$' \|\| pFmt->zString[k] == '{')) ){` |
|       ! 0 | 1243 | `					return 0;` |
|         - | 1244 | `				}` |
|         3 | 1245 | `			}` |
|         5 | 1246 | `			for( k = 0 ; k < pFmt->nByte ; ++k ){` |
|         3 | 1247 | `				if( pFmt->zString[k] != '%' ){` |
|       ! 0 | 1248 | `					continue;` |
|         - | 1249 | `				}` |
|         3 | 1250 | `				if( k + 1 >= pFmt->nByte ){` |
|       ! 0 | 1251 | `					return 0; /* a trailing '%' */` |
|         - | 1252 | `				}` |
|         3 | 1253 | `				k++;` |
|         3 | 1254 | `				if( pFmt->zString[k] == 's' \|\| pFmt->zString[k] == 'd' ){` |
|         3 | 1255 | `					nPlace++;` |
|         1 | 1256 | `				}else if( pFmt->zString[k] != '%' ){` |
|       ! 0 | 1257 | `					return 0; /* any other conversion */` |
|         - | 1258 | `				}` |
|         2 | 1259 | `			}` |
|         3 | 1260 | `			if( nPlace != nArg - 1 ){` |
|       ! 0 | 1261 | `				return 0;` |
|         - | 1262 | `			}` |
|         1 | 1263 | `		}` |
|         6 | 1264 | `		return 1;` |
|       ! 0 | 1265 | `	}` |
|        91 | 1266 | `	return 0;` |
|        85 | 1267 | `}` |
|         - | 1268 | `/*` |
|         - | 1269 | ` * The two write-target rules php decides at COMPILE time, in one place because` |
|         - | 1270 | ` * every write site has to make both of them.` |
|         - | 1271 | ` *` |
|         - | 1272 | `` * **`$this`** is not a variable a program may re-point: php refuses the`` |
|         - | 1273 | `` * assignment, the reference bind, a foreach/list target and `unset()` where they`` |
|         - | 1274 | `` * are WRITTEN. PHL performed all of them, so `$this = 5;` inside a method`` |
|         - | 1275 | ` * replaced the receiver with an int for the rest of the call and every later` |
|         - | 1276 | `` * `$this->x` failed somewhere else entirely.`` |
|         - | 1277 | ` *` |
|         - | 1278 | ``  * **A temporary** cannot be written THROUGH: `(new A)->p = 1` and `"s"->p = 1` `` |
|         - | 1279 | ` * modify an object/value that no longer exists after the statement, so php` |
|         - | 1280 | `` * refuses the whole chain — every write kind, including `+=`, `++`, `=&` and`` |
|         - | 1281 | `` * `unset()`. The base of the access chain decides: a variable and a userland`` |
|         - | 1282 | `` * CALL are writable (`f()->p = 1` is php-legal), a `new`, a literal and any`` |
|         - | 1283 | ` * other computed value are not, and an internal function's result gets php's own` |
|         - | 1284 | `` * separate wording — which is what `(clone $o)->p = 1` is, `clone` being a`` |
|         - | 1285 | ` * function in php 8.5.` |
|         - | 1286 | ` *` |
|         - | 1287 | ` * **A call is writable THROUGH but not writable INTO.** The distinction is` |
|         - | 1288 | `` * php's, and it is made in two different places: `zend_compile_var_inner` lets`` |
|         - | 1289 | `` * a call be the base of a chain, while `zend_ensure_writable_variable` refuses`` |
|         - | 1290 | ` * the call when it is the target ITSELF, with a wording that says which kind of` |
|         - | 1291 | `` * call it was. So `f()[0] = 5` compiles and `f() = 5` does not. The one write`` |
|         - | 1292 | `` * site that does not ask the second question is the SOURCE of `=&`, which is`` |
|         - | 1293 | `` * why `$r =& f()` is a runtime notice rather than a compile error.`` |
|         - | 1294 | ` */` |
|   1054476 | 1295 | `PH7_PRIVATE sxi32 GenStateWriteTargetCheck(ph7_gen_state *pGen,ph7_expr_node *pTarget,int iCtx)` |
|         5 | 1296 | `{` |
|   1054481 | 1297 | `	ph7_expr_node *pBase = pTarget;` |
|   1054481 | 1298 | `	const char *zMsg = 0;` |
|         - | 1299 | `	sxi32 rc;` |
|   1054481 | 1300 | `	if( pTarget == 0 ){` |
|       ! 0 | 1301 | `		return SXRET_OK;` |
|         - | 1302 | `	}` |
|   1054481 | 1303 | `	if( PH7_ExprNodeIsThis(pTarget) && (iCtx & (PH7_WTC_REFSRC\|PH7_WTC_RMW\|PH7_WTC_THISSRC)) == 0 ){` |
|         - | 1304 | ``		/* Only as the TARGET. php refuses `$this = …`, `$this =& …`, a`` |
|         - | 1305 | ``		 * foreach/list target and `unset($this)` -- but the SOURCE of a `=&` is`` |
|         - | 1306 | `		 * compiled in write context WITHOUT zend_ensure_writable_variable, and` |
|         - | 1307 | ``		 * that is the function that holds the $this rule. So `$t =& $this` binds`` |
|         - | 1308 | ``		 * the receiver, and so do `$a[] =& $this`, `$this->p =& $this` and`` |
|         - | 1309 | ``		 * `self::$s =& $this`; a $this that has no object behind it is the`` |
|         - | 1310 | `		 * ordinary RUNTIME "Using $this when not in object context". Refusing the` |
|         - | 1311 | ``		 * source here cost react/promise's `$target =& $this` -- Composer's whole`` |
|         - | 1312 | `		 * async download layer.` |
|         - | 1313 | `		 *` |
|         - | 1314 | `		 * And only for an ASSIGNMENT. php makes this rule in the assignment` |
|         - | 1315 | ``		 * compiler, so a READ-MODIFY-WRITE (`$this += 1`, `$this .= "x"`,`` |
|         - | 1316 | ``		 * `$this++`) compiles and raises the ordinary operand error at run time`` |
|         - | 1317 | ``		 * (`Unsupported operand types: C + int`, `Cannot increment C`) -- which`` |
|         - | 1318 | `		 * this engine already words for any other object. */` |
|        16 | 1319 | `		zMsg = (iCtx & PH7_WTC_UNSET) ? "Cannot unset $this" : "Cannot re-assign $this";` |
|   1054474 | 1320 | `	}else if( pTarget->pOp && pTarget->pOp->iOp == EXPR_OP_FUNC_CALL` |
|     98241 | 1321 | `	       && (iCtx & PH7_WTC_REFSRC) == 0 ){` |
|         - | 1322 | ``		/* The target is the call itself (`f() = 5`, `f()++`, `unset(f())`,`` |
|         - | 1323 | ``		 * `foreach (… as f())`). php names the kind of call: a METHOD callee —`` |
|         - | 1324 | ``		 * `$o->m()`, `C::m()` — reports "method", everything else "function".`` |
|         - | 1325 | `		 * A PARENTHESISED member callee is php's variable-invocation` |
|         - | 1326 | ``		 * (`($o->p)()` calls the property's VALUE), which is an ordinary`` |
|         - | 1327 | `		 * function call, exactly the distinction the OP_CALL codegen makes. */` |
|        27 | 1328 | `		int bMethod = pTarget->pLeft` |
|        12 | 1329 | `			&& pTarget->pLeft->pOp` |
|         9 | 1330 | `			&& (pTarget->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|         5 | 1331 | `			 \|\| pTarget->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|         3 | 1332 | `			 \|\| pTarget->pLeft->pOp->iOp == EXPR_OP_DC)` |
|        18 | 1333 | `			&& (pTarget->pLeft->iFlags & EXPR_NODE_PARENS) == 0;` |
|        15 | 1334 | `		zMsg = bMethod` |
|         - | 1335 | `			? "Can't use method return value in write context"` |
|         6 | 1336 | `			: "Can't use function return value in write context";` |
|   1054463 | 1337 | `	}else if( PH7_ExprContainsNullsafe(pTarget) ){` |
|         - | 1338 | ``		/* php asks this AFTER the call question (`$o?->m()++` is a method return`` |
|         - | 1339 | `` 		 * value, not a nullsafe chain) and BEFORE the base one (`(new A)?->p = 1` `` |
|         - | 1340 | `		 * is the nullsafe refusal, not the temporary). A reference SOURCE has its` |
|         - | 1341 | ``		 * own sentence for it. The `=`/`+=`/`unset()`/foreach paths screened this`` |
|         - | 1342 | ``		 * themselves; `++`/`--`, `??=` and `array(&…)` did not, so `$o?->p++` ran. */`` |
|         9 | 1343 | `		zMsg = (iCtx & PH7_WTC_REFSRC)` |
|         - | 1344 | `			? "Cannot take reference of a nullsafe chain"` |
|         4 | 1345 | `			: "Can't use nullsafe operator in write context";` |
|         5 | 1346 | `	}else{` |
|         - | 1347 | `		/* Walk to the base of the access chain; the links themselves are writable. */` |
|   1251405 | 1348 | `		while( pBase && pBase->pOp ){` |
|    197481 | 1349 | `			if( pBase->pOp->iOp == EXPR_OP_DC && !PH7_ExprNodeIsClassConst(pBase) ){` |
|         - | 1350 | `` 				/* A `::` left operand is a CLASS reference, not a value — `C::$s = 1` `` |
|         - | 1351 | ``				 * and even `(new C)::$s = 1` write class-level storage that outlives`` |
|         - | 1352 | `` 				 * any temporary, so the chain stops being about a base here. A `::` `` |
|         - | 1353 | ``				 * naming a CONSTANT is not storage, though: `A::K[0] = 5` subscripts`` |
|         - | 1354 | `				 * a COPY, so it falls through to the computed-base verdict below —` |
|         - | 1355 | `				 * php's "Cannot use temporary expression in write context", where` |
|         - | 1356 | `				 * PHL wrote into the copy and answered nothing. */` |
|       297 | 1357 | `				return SXRET_OK;` |
|         - | 1358 | `			}` |
|    197184 | 1359 | `			if( pBase->pOp->iOp != EXPR_OP_ARROW && pBase->pOp->iOp != EXPR_OP_NULLSAFE_ARROW` |
|    194261 | 1360 | `			 && pBase->pOp->iOp != EXPR_OP_SUBSCRIPT ){` |
|       233 | 1361 | `				break;` |
|         - | 1362 | `			}` |
|    196961 | 1363 | `			pBase = pBase->pLeft;` |
|         5 | 1364 | `		}` |
|   1054157 | 1365 | `		if( pBase == 0 \|\| pBase == pTarget ){` |
|         - | 1366 | `			/* No chain: a non-variable target of its own is the caller's business` |
|         - | 1367 | `			 * (php reports its parse error / "Assignments can only happen to` |
|         - | 1368 | `			 * writable values" there, and so does PHL). */` |
|    857837 | 1369 | `			return SXRET_OK;` |
|         - | 1370 | `		}` |
|    196325 | 1371 | `		if( pBase->pOp == 0 ){` |
|    196155 | 1372 | `			if( pBase->xCode != PH7_CompileVariable ){` |
|       ! 0 | 1373 | `				zMsg = "Cannot use temporary expression in write context";` |
|         5 | 1374 | `			}` |
|     98113 | 1375 | `		}else if( pBase->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|         - | 1376 | `			/* php refuses a write through the result of a call it SPECIALIZED into` |
|         - | 1377 | `			 * an opcode — see aSpecialFunc above. The old test asked whether the` |
|         - | 1378 | `			 * name was a host function AT ALL, which would have refused every` |
|         - | 1379 | `			 * builtin (php specializes 28 of them), and asked it of the CALL node` |
|         - | 1380 | `			 * where GenStateCallBuiltinName wants the CALLEE node — so it never` |
|         - | 1381 | ``			 * matched anything and `clone` below was the only arm that ever fired.`` |
|         - | 1382 | `			 * The name table IS the resolution here: php looks the callee up in a` |
|         - | 1383 | `			 * function table that is fully populated at compile time, and PHL's is` |
|         - | 1384 | `			 * not — the ~650 core builtins register in PH7_VmMakeReady, which runs` |
|         - | 1385 | `			 * AFTER compilation (see the redeclaration guard near the top of this` |
|         - | 1386 | ``			 * file), so hHostFunction has no `strlen` to find. */`` |
|         - | 1387 | `			SyString sName;` |
|       165 | 1388 | `			GenStateCallBuiltinName(pBase->pLeft,&sName);` |
|       165 | 1389 | `			if( GenStateCallIsSpecialized(&(*pGen),pBase,&sName) ){` |
|         6 | 1390 | `				zMsg = "Cannot use result of built-in function in write context";` |
|         7 | 1391 | `			}` |
|        92 | 1392 | `		}else if( pBase->pOp->iOp == EXPR_OP_CLONE ){` |
|         - | 1393 | ``			/* php 8.5 implements `clone` AS a function, so a write through its result`` |
|         - | 1394 | `			 * takes the internal-function wording rather than the temporary one. */` |
|         3 | 1395 | `			zMsg = "Cannot use result of built-in function in write context";` |
|         2 | 1396 | `		}else{` |
|         - | 1397 | ``			/* `new`, and every other computed base. */`` |
|        10 | 1398 | `			zMsg = "Cannot use temporary expression in write context";` |
|         - | 1399 | `		}` |
|         - | 1400 | `	}` |
|    196357 | 1401 | `	if( zMsg == 0 ){` |
|    196311 | 1402 | `		return SXRET_OK;` |
|         - | 1403 | `	}` |
|        50 | 1404 | `	rc = PH7_GenCompileError(&(*pGen),E_ERROR,` |
|        46 | 1405 | `		pTarget->pStart ? pTarget->pStart->nLine : 0,"%s",zMsg);` |
|        50 | 1406 | `	return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_INVALID;` |
|    526477 | 1407 | `}` |
|         - | 1408 | `/*` |
|         - | 1409 | ` * What emitting a call's ARGUMENT LIST decided, handed back to the CALL codegen.` |
|         - | 1410 | ` * The arguments are emitted from their own routine because php evaluates them` |
|         - | 1411 | ` * AFTER the callee has been resolved, so this runs between the callee's emission` |
|         - | 1412 | ` * and the OP_CALL — see GenStateEmitCallArgs.` |
|         - | 1413 | ` */` |
|         - | 1414 | `typedef struct GenCallArgs GenCallArgs;` |
|         - | 1415 | `struct GenCallArgs {` |
|         - | 1416 | `	sxi32 iP1;      /* OP_CALL.iP1: the compile-time argument count */` |
|         - | 1417 | ``	sxu32 iP2;      /* OP_CALL.iP2: 1 if any argument unpacks (`...$a`) */`` |
|         - | 1418 | `	void *p3;       /* OP_CALL.p3: the VmCallArgMap, which may ALREADY carry the callee's` |
|         - | 1419 | `	                 * namespace qualification — the callee is emitted first now */` |
|         - | 1420 | ``	int bFcc;       /* First-class callable `f(...)`: no arguments, OP_LOAD_FCC follows */`` |
|         - | 1421 | ``	int bAnySpread; /* Any `...` argument (iP2 says the same; kept for the shape masks) */`` |
|         - | 1422 | `};` |
|         - | 1423 | `static sxi32 GenStateEmitCallArgs(ph7_gen_state *pGen,ph7_expr_node *pNode,sxi32 iFlags,` |
|         - | 1424 | `	GenCallArgs *pArgs);` |
|         - | 1425 | `/*` |
|         - | 1426 | `` * TRUE when the `instanceof` SUBJECT that just compiled into the instruction`` |
|         - | 1427 | ` * stream starting at nFirst is what zend calls IS_CONST -- the shape whose` |
|         - | 1428 | ` * whole expression its compiler folds to FALSE, without ever compiling the` |
|         - | 1429 | ` * class operand.` |
|         - | 1430 | ` *` |
|         - | 1431 | `` * php decides this from its own constant FOLDER: `zend_compile_expr` on the`` |
|         - | 1432 | `` * subject comes back IS_CONST for a literal, for `null`/`true`/`false`, for an`` |
|         - | 1433 | ` * engine constant, for an array literal, and for arithmetic or concatenation` |
|         - | 1434 | `` * over any of those -- and `5 instanceof $x` is then false whatever $x holds,`` |
|         - | 1435 | `` * while `$v instanceof $x` with the same 5 in $v reaches the runtime opcode and`` |
|         - | 1436 | ` * is refused when $x is neither an object nor a string. The two spellings really` |
|         - | 1437 | ` * do answer differently, so OP_IS_A's screen has to be told which one it is.` |
|         - | 1438 | ` *` |
|         - | 1439 | ` * PHL has no constant folder, so the question is asked of the INSTRUCTIONS the` |
|         - | 1440 | ` * subject compiled to: a run built only from LITERAL loads and pure value` |
|         - | 1441 | ` * operators is a constant expression, and anything that reads a variable, names` |
|         - | 1442 | ` * a constant, calls something or touches an object is not. php's own folder` |
|         - | 1443 | `` * reaches two shapes further -- an ENGINE constant (`PHP_EOL`) and a builtin`` |
|         - | 1444 | `` * call it ct-evaluates (`strlen("a")`) -- where php answers false and this`` |
|         - | 1445 | ` * refuses; the pair is recorded under the constant-folding family.` |
|         - | 1446 | ` */` |
|     16494 | 1447 | `static int GenStateInstanceofFoldsLhs(ph7_gen_state *pGen,sxu32 nFirst)` |
|         5 | 1448 | `{` |
|         - | 1449 | `	sxu32 n;` |
|     16499 | 1450 | `	sxu32 nLen = PH7_VmInstrLength(pGen->pVm);` |
|     16791 | 1451 | `	for( n = nFirst ; n < nLen ; ++n ){` |
|     16775 | 1452 | `		VmInstr *pIn = PH7_VmGetInstr(pGen->pVm,n);` |
|     16775 | 1453 | `		if( pIn == 0 ){` |
|       ! 0 | 1454 | `			return 0;` |
|         - | 1455 | `		}` |
|     16775 | 1456 | `		switch( pIn->iOp ){` |
|       145 | 1457 | `		case PH7_OP_LOADC:` |
|         - | 1458 | `			/* A LOADC that still carries EXPAND is a NAME the runtime resolves --` |
|         - | 1459 | `			 * a constant -- and that is exactly what php does NOT fold: its` |
|         - | 1460 | `			 * compiler substitutes only the engine's own persistent constants, so` |
|         - | 1461 | ``			 * a userland `const OBJ = new C();` reaches the runtime opcode and`` |
|         - | 1462 | ``			 * `OBJ instanceof C` is a real question. Folding it answered FALSE for`` |
|         - | 1463 | `			 * every constant that holds an object. */` |
|       295 | 1464 | `			if( pIn->iP1 & PH7_LOADC_EXPAND ){` |
|         5 | 1465 | `				return 0;` |
|         - | 1466 | `			}` |
|       291 | 1467 | `			break;` |
|         3 | 1468 | `		case PH7_OP_LOAD_MAP:` |
|         - | 1469 | `		case PH7_OP_CAT:` |
|         - | 1470 | `		case PH7_OP_CVT_INT: case PH7_OP_CVT_STR: case PH7_OP_CVT_REAL:` |
|         - | 1471 | `		case PH7_OP_CVT_BOOL: case PH7_OP_CVT_NUMC: case PH7_OP_CVT_NULL:` |
|         - | 1472 | `		case PH7_OP_UMINUS: case PH7_OP_UPLUS: case PH7_OP_BITNOT: case PH7_OP_LNOT:` |
|         - | 1473 | `		case PH7_OP_MUL: case PH7_OP_DIV: case PH7_OP_MOD: case PH7_OP_POW:` |
|         - | 1474 | `		case PH7_OP_ADD: case PH7_OP_SUB: case PH7_OP_SHL: case PH7_OP_SHR:` |
|         - | 1475 | `		case PH7_OP_LT: case PH7_OP_LE: case PH7_OP_GT: case PH7_OP_GE:` |
|         - | 1476 | `		case PH7_OP_SPACESHIP: case PH7_OP_EQ: case PH7_OP_NEQ:` |
|         - | 1477 | `		case PH7_OP_TEQ: case PH7_OP_TNE:` |
|         - | 1478 | `		case PH7_OP_BAND: case PH7_OP_BXOR: case PH7_OP_BOR:` |
|         7 | 1479 | `			break;` |
|      8248 | 1480 | `		default:` |
|     16479 | 1481 | `			return 0;` |
|         - | 1482 | `		}` |
|       151 | 1483 | `	}` |
|        17 | 1484 | `	return nLen > nFirst;` |
|      8241 | 1485 | `}` |
|         - | 1486 | `/*` |
|         - | 1487 | ` * The child flags a subscript/property NAME is compiled under: whatever the access itself` |
|         - | 1488 | ` * was given, minus every context that belongs to the ACCESS rather than to the expression` |
|         - | 1489 | ` * that names it. Kept beside the LOAD_IDX arm that uses the same mask, since the two have` |
|         - | 1490 | ` * to agree -- a name parked ahead of the assigned value is compiled here and read back` |
|         - | 1491 | ` * there, and a difference between the two would compile one expression two ways.` |
|         - | 1492 | ` */` |
|         - | 1493 | `#define GEN_ACCESS_NAME_MASK  (~(EXPR_FLAG_LOAD_IDX_STORE \` |
|         - | 1494 | `	\|EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_UNSET \` |
|         - | 1495 | `	\|EXPR_FLAG_LOAD_IDX_UNSET_BASE \` |
|         - | 1496 | `	\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_MEMBER_WRITE \` |
|         - | 1497 | `	\|EXPR_FLAG_MEMBER_COALESCE \` |
|         - | 1498 | `	\|EXPR_FLAG_QUIET_VAR\|EXPR_FLAG_RMW_LOAD\|EXPR_FLAG_DEFER_ARG))` |
|         - | 1499 | `/*` |
|         - | 1500 | ` * The NAME expression of one access, or 0 when the access has none to run.` |
|         - | 1501 | ` *` |
|         - | 1502 | `` * A subscript names its element with its single index node; `->` and `?->` name their`` |
|         - | 1503 | `` * property with the right operand. `::` is left out on purpose: a static property's name`` |
|         - | 1504 | `` * is folded into the OP_MEMBER itself rather than pushed, and the append form `[]` names`` |
|         - | 1505 | ` * nothing.` |
|         - | 1506 | ` */` |
|    195612 | 1507 | `static ph7_expr_node * GenStateAccessNameNode(ph7_expr_node *pAccess)` |
|         5 | 1508 | `{` |
|    195617 | 1509 | `	if( pAccess == 0 \|\| pAccess->pOp == 0 ){` |
|       ! 0 | 1510 | `		return 0;` |
|         - | 1511 | `	}` |
|    195617 | 1512 | `	if( pAccess->pOp->iOp == EXPR_OP_SUBSCRIPT ){` |
|    193351 | 1513 | `		ph7_expr_node **apArg = (ph7_expr_node **)SySetBasePtr(&pAccess->aNodeArgs);` |
|    193351 | 1514 | `		return SySetUsed(&pAccess->aNodeArgs) == 1 ? apArg[0] : 0;` |
|         - | 1515 | `	}` |
|      2271 | 1516 | `	if( pAccess->pOp->iOp == EXPR_OP_ARROW \|\| pAccess->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|      2271 | 1517 | `		return pAccess->pRight;` |
|         - | 1518 | `	}` |
|       ! 0 | 1519 | `	return 0;` |
|     97674 | 1520 | `}` |
|         - | 1521 | `/*` |
|         - | 1522 | ` * Collect an assignment TARGET's dynamic names, in the order php evaluates them.` |
|         - | 1523 | ` *` |
|         - | 1524 | `` * php compiles `$a[k()][j()] = v()` as: the two subscript expressions, then the assigned`` |
|         - | 1525 | ``  * value, and only then the fetches that use them -- so `k()` and `j()` run before `v()` `` |
|         - | 1526 | ` * does, while the array is still untouched by the write. PHL emits the value first and the` |
|         - | 1527 | ` * whole target after it, which reverses every one of those side effects.` |
|         - | 1528 | ` *` |
|         - | 1529 | `` * Only a name that can RUN is collected. php reads a plain `$var` or a literal name off the`` |
|         - | 1530 | `` * fetch opline's own operand, at the fetch, which is AFTER the value: `$k = 'A'; $a[$k] =`` |
|         - | 1531 | `` * f();` with an `f()` that assigns `'B'` to `$k` stores under `B` there, and under `B` here`` |
|         - | 1532 | `` * for the same reason. Parking one of those would answer `A`, so the same shape test the`` |
|         - | 1533 | ` * operand-snapshot rule turns on decides this too.` |
|         - | 1534 | ` *` |
|         - | 1535 | ` * Returns the count (0 = nothing to park, compile as before), or -1 when more than` |
|         - | 1536 | ` * PH7_STORE_KEY_MAX names in one target can run -- which compiles as before, in the old` |
|         - | 1537 | ` * order. The ceiling is a real one and it is set well past anything written: it takes NINE` |
|         - | 1538 | ` * running subscripts on a single assignment target to reach it.` |
|         - | 1539 | ` */` |
|    870383 | 1540 | `static int GenStateCollectStoreNames(ph7_expr_node *pTarget,ph7_expr_node **apOut)` |
|         5 | 1541 | `{` |
|         - | 1542 | `	ph7_expr_node *apChain[32];` |
|    870388 | 1543 | `	ph7_expr_node *p = pTarget;` |
|    870388 | 1544 | `	int nChain = 0;` |
|    870388 | 1545 | `	int nOut = 0;` |
|         - | 1546 | `	int n;` |
|   1163842 | 1547 | `	while( p && p->pOp && (p->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|     99148 | 1548 | `	    \|\| p->pOp->iOp == EXPR_OP_ARROW \|\| p->pOp->iOp == EXPR_OP_NULLSAFE_ARROW) ){` |
|    195617 | 1549 | `		if( nChain >= (int)SX_ARRAYSIZE(apChain) ){` |
|       ! 0 | 1550 | `			return -1;` |
|         - | 1551 | `		}` |
|    195617 | 1552 | `		apChain[nChain++] = p;` |
|    195617 | 1553 | `		p = p->pLeft;` |
|         5 | 1554 | `	}` |
|         - | 1555 | `	/* The chain was walked outermost-first; php names them base-first. */` |
|   1066000 | 1556 | `	for( n = nChain - 1 ; n >= 0 ; --n ){` |
|    195617 | 1557 | `		ph7_expr_node *pName = GenStateAccessNameNode(apChain[n]);` |
|    195617 | 1558 | `		if( pName == 0 \|\| !GenStateArgRunsCode(pName) ){` |
|    195177 | 1559 | `			continue;` |
|         - | 1560 | `		}` |
|       445 | 1561 | `		if( nOut >= PH7_STORE_KEY_MAX ){` |
|       ! 0 | 1562 | `			return -1;` |
|         - | 1563 | `		}` |
|       445 | 1564 | `		apOut[nOut++] = pName;` |
|       225 | 1565 | `	}` |
|    870388 | 1566 | `	return nOut;` |
|    434569 | 1567 | `}` |
|         - | 1568 | `/*` |
|         - | 1569 | ` * Where the parked name at index iSlot sits, counted down from the top of the stack, at the` |
|         - | 1570 | ` * moment the access that owns it is emitted.` |
|         - | 1571 | ` *` |
|         - | 1572 | ` * Above the parked run the stack holds exactly two things by then: the assigned value, and` |
|         - | 1573 | ` * the container this access is about to read -- every level of the chain consumes a` |
|         - | 1574 | ` * container and a name and leaves one element in their place, so the shape is the same at` |
|         - | 1575 | ` * every level. The parked names sit under that in push order, so the FIRST one parked is` |
|         - | 1576 | ` * the deepest.` |
|         - | 1577 | ` */` |
|         - | 1578 | `#define GEN_STORE_KEY_DEPTH(nParked,iSlot)  ((sxi32)((nParked) + 1 - (iSlot)))` |
|         - | 1579 | `/*` |
|         - | 1580 | ` * Generate bytecode for a given expression tree.` |
|         - | 1581 | ` * If something goes wrong while generating bytecode` |
|         - | 1582 | ` * for the expression tree (A very unlikely scenario)` |
|         - | 1583 | ` * this function takes care of generating the appropriate` |
|         - | 1584 | ` * error message.` |
|         - | 1585 | ` */` |
|         - | 1586 | `/*` |
|         - | 1587 | `` * php's `zend_is_variable_or_call`: what may sit on the right of a destructuring`` |
|         - | 1588 | ` * assignment whose target list binds BY REFERENCE. A variable, a property, a` |
|         - | 1589 | ` * static property, a subscript and a CALL can each hand a slot over; an array` |
|         - | 1590 | `` * literal, a string, `new`, and any computed value cannot, and php refuses those`` |
|         - | 1591 | ` * at compile time rather than binding to a temporary.` |
|         - | 1592 | ` */` |
|       348 | 1593 | `static int GenStateNodeIsRefSource(ph7_expr_node *pNode)` |
|         5 | 1594 | `{` |
|       353 | 1595 | `	if( pNode == 0 ){` |
|       ! 0 | 1596 | `		return 0;` |
|         - | 1597 | `	}` |
|       353 | 1598 | `	if( pNode->pOp ){` |
|       202 | 1599 | `		return pNode->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|       131 | 1600 | `		    \|\| pNode->pOp->iOp == EXPR_OP_ARROW` |
|       125 | 1601 | `		    \|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|       120 | 1602 | `		    \|\| pNode->pOp->iOp == EXPR_OP_DC` |
|       197 | 1603 | `		    \|\| pNode->pOp->iOp == EXPR_OP_FUNC_CALL;` |
|         - | 1604 | `	}` |
|       219 | 1605 | `	return pNode->xCode == PH7_CompileVariable;` |
|       179 | 1606 | `}` |
|         - | 1607 | `/*` |
|         - | 1608 | `` * Is this call node's callee the `isset` KEYWORD itself?`` |
|         - | 1609 | ` *` |
|         - | 1610 | ` * isset() is a language construct, not a name: it reaches the call path as a` |
|         - | 1611 | ` * keyword token whose literal the compiler canonicalizes to "isset" (see` |
|         - | 1612 | ` * compile_node.c). A keyword used as a MEMBER name is excluded here for the same` |
|         - | 1613 | `` * reason it is excluded there — `$o->isset(...)` names a method — and so is any`` |
|         - | 1614 | ` * callee that is an operator node rather than a bare literal.` |
|         - | 1615 | ` */` |
|    429333 | 1616 | `static int GenStateCalleeIsIsset(ph7_expr_node *pCallee)` |
|         5 | 1617 | `{` |
|         - | 1618 | `	SyString *pName;` |
|    429338 | 1619 | `	if( pCallee == 0 \|\| pCallee->pOp != 0 \|\| pCallee->pStart == 0 ){` |
|      1989 | 1620 | `		return 0;` |
|         - | 1621 | `	}` |
|    427349 | 1622 | `	if( (pCallee->pStart->nType & PH7_TK_KEYWORD) == 0` |
|    213188 | 1623 | `	 \|\| (pCallee->pStart->nType & PH7_TK_MEMBER_NAME) ){` |
|    427266 | 1624 | `		return 0;` |
|         - | 1625 | `	}` |
|        91 | 1626 | `	pName = &pCallee->pStart->sData;` |
|       132 | 1627 | `	return pName->nByte == sizeof("isset")-1` |
|        88 | 1628 | `		&& SyStrnicmp(pName->zString,"isset",sizeof("isset")-1) == 0;` |
|    214133 | 1629 | `}` |
|         - | 1630 | `/*` |
|         - | 1631 | ` * Is this callee the keyword of a language CONSTRUCT that compiles to a call --` |
|         - | 1632 | `` * `isset`, `empty` or `eval`? (`unset`, `print` and the four file-inclusion words have`` |
|         - | 1633 | ` * their own codegen and mark their OP_CALL directly.)` |
|         - | 1634 | ` *` |
|         - | 1635 | ` * The answer rides the emitted call as PH7_CALL_CONSTRUCT, which is what lets the` |
|         - | 1636 | ` * construct's hidden host function answer it and nothing else: see` |
|         - | 1637 | ` * PH7_VmGetHostFunction. Read off the token's KEYWORD ID, never its text -- only a` |
|         - | 1638 | ` * keyword token can produce one of these calls, and that is what makes the mark` |
|         - | 1639 | ` * unforgeable by a program.` |
|         - | 1640 | ` */` |
|   2083811 | 1641 | `static int GenStateCalleeIsConstruct(ph7_expr_node *pCallee)` |
|         5 | 1642 | `{` |
|         - | 1643 | `	sxu32 nKw;` |
|   2083816 | 1644 | `	if( pCallee == 0 \|\| pCallee->pOp != 0 \|\| pCallee->pStart == 0 ){` |
|     23223 | 1645 | `		return 0;` |
|         - | 1646 | `	}` |
|   2060593 | 1647 | `	if( (pCallee->pStart->nType & PH7_TK_KEYWORD) == 0` |
|   1045798 | 1648 | `	 \|\| (pCallee->pStart->nType & PH7_TK_MEMBER_NAME) ){` |
|   2025365 | 1649 | `		return 0;` |
|         - | 1650 | `	}` |
|     35238 | 1651 | `	nKw = (sxu32)SX_PTR_TO_INT(pCallee->pStart->pUserData);` |
|     35238 | 1652 | `	return nKw == PH7_TKWRD_ISSET \|\| nKw == PH7_TKWRD_EMPTY \|\| nKw == PH7_TKWRD_EVAL;` |
|   1039749 | 1653 | `}` |
|  11571417 | 1654 | `static sxi32 GenStateEmitExprCode(` |
|         - | 1655 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 1656 | `	ph7_expr_node *pNode, /* Root of the expression tree */` |
|         - | 1657 | `	sxi32 iFlags /* Control flags */` |
|         - | 1658 | `	)` |
|         5 | 1659 | `{` |
|         - | 1660 | `	VmInstr *pInstr;` |
|         - | 1661 | `	sxu32 nJmpIdx;` |
|  11571422 | 1662 | `	sxi32 iP1 = 0;` |
|  11571422 | 1663 | `	sxu32 iP2 = 0;` |
|  11571422 | 1664 | `	void *p3  = 0;` |
|         - | 1665 | `	sxi32 iVmOp;` |
|         - | 1666 | `	sxi32 rc;` |
|  11571422 | 1667 | `	int bIsChainOp = 0; /* Set below once we know pNode->pOp */` |
|  11571422 | 1668 | ``	int bFcc = 0;       /* First-class callable `f(...)`: emit OP_LOAD_FCC, not OP_CALL */`` |
|  11571422 | 1669 | `	sxu32 nRhsNsBase = 0;` |
|  11571422 | 1670 | `	sxi32 iRhsFlags = 0; /* control flags the RIGHT operand is compiled under */` |
|  11571422 | 1671 | `	sxu32 nLhsFirst = 0; /* instruction index the LEFT operand starts at */` |
|  11571422 | 1672 | `	int bMoveLhs = 0;    /* the LEFT operand's load was lifted past the RIGHT one */` |
|         - | 1673 | `	VmInstr sMovedLhs;   /* ...and this is it, verbatim */` |
|         - | 1674 | ``	/* Consumed here so it describes THIS node only — the direct operand of a `new` —`` |
|         - | 1675 | `	 * and never travels down into the operand's own sub-expressions. */` |
|  11571422 | 1676 | `	int bNewCallee = (iFlags & EXPR_FLAG_NEW_CALLEE) != 0;` |
|  11571422 | 1677 | `	int nParked = 0;                          /* assignment target names parked on the stack */` |
|  11571422 | 1678 | `	int nOuterParked = 0;                     /* ...and what an enclosing emission had parked */` |
|  11571422 | 1679 | `	ph7_expr_node **apOuterParked = 0;` |
|         - | 1680 | `	ph7_expr_node *apParked[PH7_STORE_KEY_MAX];` |
|  11571422 | 1681 | `	iFlags &= ~EXPR_FLAG_NEW_CALLEE;` |
|  11571422 | 1682 | `	if( pGen->nStoreKey > 0 ){` |
|         - | 1683 | `		/* This node is a target name that was already compiled and parked, ahead of the` |
|         - | 1684 | `		 * assigned value; its access is being emitted now, so read it back rather than` |
|         - | 1685 | `		 * running it a second time. */` |
|         - | 1686 | `		int iSlot;` |
|      2335 | 1687 | `		for( iSlot = 0 ; iSlot < pGen->nStoreKey ; ++iSlot ){` |
|      1409 | 1688 | `			if( pGen->apStoreKey[iSlot] == pNode ){` |
|       665 | 1689 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_PICK,` |
|       440 | 1690 | `					GEN_STORE_KEY_DEPTH(pGen->nStoreKey,iSlot),0,0,0);` |
|       445 | 1691 | `				return SXRET_OK;` |
|         - | 1692 | `			}` |
|       487 | 1693 | `		}` |
|       463 | 1694 | `	}` |
|  11570982 | 1695 | `	if( pNode->xCode ){` |
|         - | 1696 | `		SyToken *pTmpIn,*pTmpEnd;` |
|         - | 1697 | `		/* Compile node */` |
|   7153499 | 1698 | `		SWAP_DELIMITER(pGen,pNode->pStart,pNode->pEnd);` |
|   7153499 | 1699 | `		rc = pNode->xCode(&(*pGen),iFlags);` |
|   7153499 | 1700 | `		RE_SWAP_DELIMITER(pGen);` |
|   7153499 | 1701 | `		return rc;` |
|         - | 1702 | `	}` |
|   4417488 | 1703 | `	if( pNode->pOp == 0 ){` |
|       ! 0 | 1704 | `		PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 1705 | `			"Invalid expression node,PH7 is aborting compilation");` |
|       ! 0 | 1706 | `		return SXERR_ABORT;` |
|         - | 1707 | `	}` |
|   4417488 | 1708 | `	iVmOp = pNode->pOp->iVmOp;` |
|   4417488 | 1709 | `	if( iVmOp == PH7_OP_STORE_REF && pNode->pLeft && PH7_ExprNodeIsThis(pNode->pLeft) ){` |
|         - | 1710 | ``		/* `$t =& $this` is a VALUE assignment. php's `$this` is not a slot a`` |
|         - | 1711 | `		 * reference can name -- the receiver lives in the frame's own field, not` |
|         - | 1712 | `		 * in a variable -- so the bind quietly degrades to a copy of the object` |
|         - | 1713 | ``		 * HANDLE: `$t` gets a slot of its own, `$t->v = 9` still reaches the same`` |
|         - | 1714 | `` 		 * object (that is identity, not reference), and `$t = 5` leaves `$this` `` |
|         - | 1715 | `		 * an object. Binding the slot instead let a write through the alias` |
|         - | 1716 | `		 * REPLACE the receiver for the rest of the call. The operands were` |
|         - | 1717 | `		 * swapped in parse.c, so the source is pLeft. */` |
|        17 | 1718 | `		iVmOp = PH7_OP_STORE;` |
|         8 | 1719 | `	}` |
|   4417488 | 1720 | `	if( iVmOp == PH7_OP_CVT_NULL ){` |
|         - | 1721 | `		/* php 8 removed the (unset) cast. Error recorded (nErr>0 fails the` |
|         - | 1722 | `		 * whole compile); keep emitting so expression codegen stays aligned` |
|         - | 1723 | `		 * and later errors are still reported. */` |
|         3 | 1724 | `		rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 1725 | `			"The (unset) cast is no longer supported");` |
|         3 | 1726 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 1727 | `			return SXERR_ABORT;` |
|         - | 1728 | `		}` |
|         1 | 1729 | `	}` |
|   4417488 | 1730 | `	if( pNode->pOp->iOp == EXPR_OP_NULLC_ASSIGN ){` |
|       190 | 1731 | `		sxu32 nJmp = 0;` |
|         - | 1732 | `		sxu32 nNcNsBase;` |
|         - | 1733 | `		VmInstr *pInstrFix;` |
|         - | 1734 | `		/* Null coalescing assignment requires a custom compile order: the LHS` |
|         - | 1735 | `		 * target (pRight for prec-18 right-assoc ops) must be evaluated first` |
|         - | 1736 | `		 * so we can short-circuit the RHS when LHS is non-null. Pass` |
|         - | 1737 | `		 * EXPR_FLAG_LOAD_IDX_STORE so subscript LHS auto-vivifies and the` |
|         - | 1738 | `		 * stack slot carries a writable nIdx. */` |
|       190 | 1739 | `		if( pNode->pRight ){` |
|         - | 1740 | ``			/* `$a[] ??= v` READS its target before deciding to write, so php refuses the`` |
|         - | 1741 | ``			 * append form anywhere in that target's chain (`$a[][0] ??= v` too), even`` |
|         - | 1742 | ``			 * though the same tag makes every other `[]` on this path a legal write`` |
|         - | 1743 | ``			 * target. Only the container chain is walked — a `[]` inside an INDEX`` |
|         - | 1744 | `			 * expression is an ordinary read and the subscript codegen refuses it. */` |
|       190 | 1745 | `			ph7_expr_node *pTgt = pNode->pRight;` |
|       437 | 1746 | `			while( pTgt && pTgt->pOp && (pTgt->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|       112 | 1747 | `			      \|\| pTgt->pOp->iOp == EXPR_OP_ARROW \|\| pTgt->pOp->iOp == EXPR_OP_DC) ){` |
|       168 | 1748 | `				if( pTgt->pOp->iOp == EXPR_OP_SUBSCRIPT && SySetUsed(&pTgt->aNodeArgs) < 1 ){` |
|       ! 0 | 1749 | `					break;` |
|         - | 1750 | `				}` |
|       168 | 1751 | `				pTgt = pTgt->pLeft;` |
|         4 | 1752 | `			}` |
|       186 | 1753 | `			if( pTgt && pTgt->pOp && pTgt->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|         5 | 1754 | `			 && SySetUsed(&pTgt->aNodeArgs) < 1 ){` |
|       ! 0 | 1755 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,` |
|       ! 0 | 1756 | `					pNode->pRight->pStart ? pNode->pRight->pStart->nLine : 0,` |
|         - | 1757 | `					"Cannot use [] for reading");` |
|       ! 0 | 1758 | `				return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 1759 | `			}` |
|         - | 1760 | `` 			/* …and only THEN the write-target rules, php's order: `strval(1)[] ??= 3` `` |
|         - | 1761 | ``			 * is the append refusal, not the specialized-builtin one. `??=` compiles`` |
|         - | 1762 | `			 * its own way and so never reached this check at all, which is why` |
|         - | 1763 | ``			 * `(new A)->p ??= 3` and `"lit"->p->q ??= 3` used to run. */`` |
|       190 | 1764 | `			rc = GenStateWriteTargetCheck(&(*pGen),pNode->pRight,0);` |
|       190 | 1765 | `			if( rc != SXRET_OK ){` |
|         3 | 1766 | `				return rc;` |
|         - | 1767 | `			}` |
|       187 | 1768 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       187 | 1769 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags\|EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE);` |
|       187 | 1770 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1771 | `				return rc;` |
|         - | 1772 | `			}` |
|       187 | 1773 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|         - | 1774 | `			/* Optimisation: if the outermost LHS access is a subscript, demote` |
|         - | 1775 | `			 * its LOAD_IDX from write-context (iP2=1, eager COW separation +` |
|         - | 1776 | `			 * insert) to peek-mode (iP2=3, separate-only-on-null/missing). On` |
|         - | 1777 | `			 * the common "already set" path the upcoming NULLC_JMP will skip` |
|         - | 1778 | `			 * the store, so the parent array does not need to be copied at` |
|         - | 1779 | `			 * all. Inner levels of a nested LHS keep iP2=1 so the separation` |
|         - | 1780 | `			 * cascade for the actual write path stays correct. */` |
|       187 | 1781 | `			pInstrFix = PH7_VmPeekInstr(pGen->pVm);` |
|       187 | 1782 | `			if( pInstrFix && pInstrFix->iOp == PH7_OP_LOAD_IDX && pInstrFix->iP2 == 1 ){` |
|       101 | 1783 | `				pInstrFix->iP2 = 3;` |
|        49 | 1784 | `			}` |
|        92 | 1785 | `		}` |
|         - | 1786 | `		/* Short-circuit: if LHS is non-null, jump past the RHS + store. */` |
|       187 | 1787 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_JMP,0,0,0,&nJmp);` |
|         - | 1788 | `		/* Compile the RHS value (pLeft for prec-18 right-assoc). */` |
|       187 | 1789 | `		if( pNode->pLeft ){` |
|       187 | 1790 | `			nNcNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|       187 | 1791 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|       187 | 1792 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1793 | `				return rc;` |
|         - | 1794 | `			}` |
|       187 | 1795 | `			GenStatePatchNullsafeJumps(pGen, nNcNsBase);` |
|        92 | 1796 | `		}` |
|         - | 1797 | `		/* Store RHS into LHS's memobj slot; leave RHS as the result on stack. */` |
|       187 | 1798 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC_STORE,0,0,0,0);` |
|         - | 1799 | `		/* Patch the short-circuit jump to land after the store. */` |
|       187 | 1800 | `		if( nJmp > 0 ){` |
|       187 | 1801 | `			pInstrFix = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|       187 | 1802 | `			if( pInstrFix ){` |
|       187 | 1803 | `				pInstrFix->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|        92 | 1804 | `			}` |
|        92 | 1805 | `		}` |
|       187 | 1806 | `		return SXRET_OK;` |
|         - | 1807 | `	}` |
|   4417302 | 1808 | `	if( pNode->pOp->iOp == EXPR_OP_QUESTY ){` |
|         - | 1809 | `		sxu32 nJz,nJmp;` |
|         - | 1810 | `		sxu32 nTernaryNsBase;` |
|         - | 1811 | `		/* Ternary operator require special handling */` |
|         - | 1812 | `		/* Phase#1: Compile the condition */` |
|     44217 | 1813 | `		nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|     44217 | 1814 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pCond,iFlags);` |
|     44217 | 1815 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1816 | `			return rc;` |
|         - | 1817 | `		}` |
|         - | 1818 | `		/* Ternary is not a chain operator: any nullsafe jumps emitted while` |
|         - | 1819 | `		 * compiling the condition must short-circuit to the end of the` |
|         - | 1820 | `		 * condition expression, not leak past the ternary. */` |
|     44217 | 1821 | `		GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|     44217 | 1822 | `		nJz = nJmp = 0; /* cc -O6 warning */` |
|     44217 | 1823 | `		if( pNode->pLeft ){` |
|         - | 1824 | `			/* Standard ternary: (expr) ? (then) : (else) */` |
|         - | 1825 | `			/* Phase#2: Emit the false jump (pops condition) */` |
|     44063 | 1826 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - | 1827 | `			/* Phase#3: Compile the 'then' expression  */` |
|     44063 | 1828 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|     44063 | 1829 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iFlags);` |
|     44063 | 1830 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1831 | `				return rc;` |
|         - | 1832 | `			}` |
|     44063 | 1833 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|     22004 | 1834 | `		}else{` |
|         - | 1835 | `			/* Elvis operator: (expr) ?: (else)` |
|         - | 1836 | `			 * Duplicate condition so original value is the 'then' result.` |
|         - | 1837 | `			 * JZ consumes the copy; original stays on stack if truthy. */` |
|       159 | 1838 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_DUP,0,0,0,0);` |
|       159 | 1839 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,0,0,0,&nJz);` |
|         - | 1840 | `		}` |
|         - | 1841 | `		/* Phase#4: Emit the unconditional jump */` |
|     44217 | 1842 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_JMP,0,0,0,&nJmp);` |
|         - | 1843 | `		/* Phase#5: Fix the false jump now the jump destination is resolved. */` |
|     44217 | 1844 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJz);` |
|     44217 | 1845 | `		if( pInstr ){` |
|     44217 | 1846 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     22076 | 1847 | `		}` |
|     44217 | 1848 | `		if( !pNode->pLeft ){` |
|         - | 1849 | `			/* Elvis operator: discard the falsy condition value before evaluating 'else' */` |
|       159 | 1850 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        77 | 1851 | `		}` |
|         - | 1852 | `		/* Phase#6: Compile the 'else' expression */` |
|     44217 | 1853 | `		if( pNode->pRight ){` |
|     44217 | 1854 | `			nTernaryNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|     44217 | 1855 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iFlags);` |
|     44217 | 1856 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1857 | `				return rc;` |
|         - | 1858 | `			}` |
|     44217 | 1859 | `			GenStatePatchNullsafeJumps(pGen, nTernaryNsBase);` |
|     22076 | 1860 | `		}` |
|     44217 | 1861 | `		if( nJmp > 0 ){` |
|         - | 1862 | `			/* Phase#7: Fix the unconditional jump */` |
|     44217 | 1863 | `			pInstr = PH7_VmGetInstr(pGen->pVm,nJmp);` |
|     44217 | 1864 | `			if( pInstr ){` |
|     44217 | 1865 | `				pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|     22076 | 1866 | `			}` |
|     22076 | 1867 | `		}` |
|         - | 1868 | `		/* All done */` |
|     44217 | 1869 | `		return SXRET_OK;` |
|         - | 1870 | `	}` |
|   4373090 | 1871 | `	if( pNode->pOp->iOp == EXPR_OP_PIPE ){` |
|         - | 1872 | ``		/* PHP 8.5 pipe: `$lhs \|> $rhs` invokes the RHS callable with the LHS`` |
|         - | 1873 | ``		 * value as its sole argument [i.e. `$rhs($lhs)`]. Evaluate the LHS (the`` |
|         - | 1874 | `		 * argument) first, then the RHS callable, then emit a one-argument` |
|         - | 1875 | `		 * OP_CALL — the same stack shape the function-call path builds (the` |
|         - | 1876 | `		 * argument sits below the callee). The RHS is any callable expression:` |
|         - | 1877 | ``		 * an FCC `f(...)` (an OP_LOAD_FCC Closure), a closure variable, an`` |
|         - | 1878 | ``		 * `[obj,method]` pair, or a callable string. */`` |
|         - | 1879 | `		sxu32 nPipeNsBase;` |
|        27 | 1880 | `		sxi32 iOperandFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE` |
|         - | 1881 | `			\|EXPR_FLAG_MEMBER_REFSRC\|EXPR_FLAG_RDONLY_LOAD);` |
|        27 | 1882 | `		if( pNode->pLeft == 0 \|\| pNode->pRight == 0 ){` |
|       ! 0 | 1883 | `			rc = PH7_GenCompileError(&(*pGen),E_ERROR,pNode->pStart->nLine,` |
|         - | 1884 | `				"'\|>': Missing operand");` |
|       ! 0 | 1885 | `			return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 1886 | `		}` |
|         - | 1887 | `		/* Argument: the LHS value. */` |
|        27 | 1888 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 | 1889 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iOperandFlags);` |
|        27 | 1890 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1891 | `			return rc;` |
|         - | 1892 | `		}` |
|        27 | 1893 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - | 1894 | `		/* Callable: the RHS. */` |
|        27 | 1895 | `		nPipeNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|        27 | 1896 | `		rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iOperandFlags);` |
|        27 | 1897 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1898 | `			return rc;` |
|         - | 1899 | `		}` |
|        27 | 1900 | `		GenStatePatchNullsafeJumps(pGen, nPipeNsBase);` |
|         - | 1901 | `		/* Invoke the callable with the single piped argument. */` |
|        27 | 1902 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,0,GenStateAttachStrictFlag(pGen,0),0);` |
|        27 | 1903 | `		return SXRET_OK;` |
|         - | 1904 | `	}` |
|         - | 1905 | `	/*` |
|         - | 1906 | ``	 * php compiles the multi-operand `isset($a, $b, ...)` as a short-circuit CHAIN --`` |
|         - | 1907 | ``	 * `isset($a) && isset($b) && ...` -- so nothing after the first operand that is not`` |
|         - | 1908 | `	 * set is ever evaluated. isset() is a host function here, and a call evaluates every` |
|         - | 1909 | `	 * argument before dispatching, so the later operands ran for real: the ordinary` |
|         - | 1910 | ``	 * `isset($info['k'], $data[$info['k']])` answered false through an `Undefined array`` |
|         - | 1911 | ``	 * key` warning and a null-offset deprecation php never raises, and`` |
|         - | 1912 | ``	 * `isset($a['no'], $b[side()])` CALLED side(). Doctrine's hydrator guards its`` |
|         - | 1913 | `	 * discriminator lookup in exactly that shape, so every hydrated row of every query` |
|         - | 1914 | `	 * carried two diagnostics php does not.` |
|         - | 1915 | `	 *` |
|         - | 1916 | `	 * Emit the chain the compiler owes: one SINGLE-operand isset() per argument, joined` |
|         - | 1917 | `	 * by a keep-the-value JZ to the end (the false it left IS the answer) and a POP on` |
|         - | 1918 | `	 * the fall-through. Each link is the ordinary call path below, re-entered with the` |
|         - | 1919 | `	 * argument set narrowed to one node, so every operand keeps the exact isset context` |
|         - | 1920 | `	 * it already had -- LOAD_IDX iP2=4, the quiet intermediates of an access chain,` |
|         - | 1921 | `	 * ArrayAccess::offsetExists -- and only the ORDER changes. A spread or named operand` |
|         - | 1922 | `	 * opts out: php refuses both in this position, and neither maps to one link.` |
|         - | 1923 | `	 */` |
|   4373059 | 1924 | `	if( iVmOp == PH7_OP_CALL && SySetUsed(&pNode->aNodeArgs) > 1` |
|    775092 | 1925 | `	 && GenStateCalleeIsIsset(pNode->pLeft) ){` |
|        84 | 1926 | `		ph7_expr_node **apIsset = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|        84 | 1927 | `		sxu32 nIsset = SySetUsed(&pNode->aNodeArgs);` |
|        84 | 1928 | `		SySet sSaved = pNode->aNodeArgs;` |
|         - | 1929 | `		SySet sJz;` |
|         - | 1930 | `		sxu32 n;` |
|        84 | 1931 | `		int bPlain = 1;` |
|       314 | 1932 | `		for( n = 0 ; n < nIsset ; ++n ){` |
|       230 | 1933 | `			if( apIsset[n] == 0` |
|       232 | 1934 | `			 \|\| (apIsset[n]->iFlags & (EXPR_NODE_SPREAD\|EXPR_NODE_NAMED_ARG)) ){` |
|       ! 0 | 1935 | `				bPlain = 0;` |
|       ! 0 | 1936 | `				break;` |
|         - | 1937 | `			}` |
|       117 | 1938 | `		}` |
|        84 | 1939 | `		if( bPlain ){` |
|        84 | 1940 | `			SySetInit(&sJz,&pGen->pVm->sAllocator,sizeof(sxu32));` |
|        84 | 1941 | `			rc = SXRET_OK;` |
|       314 | 1942 | `			for( n = 0 ; n < nIsset ; ++n ){` |
|       232 | 1943 | `				pNode->aNodeArgs.pBase = (void *)&apIsset[n];` |
|       232 | 1944 | `				pNode->aNodeArgs.nUsed = 1;` |
|       232 | 1945 | `				pNode->aNodeArgs.nSize = 1;` |
|       232 | 1946 | `				pNode->aNodeArgs.nCursor = 0;` |
|       232 | 1947 | `				rc = GenStateEmitExprCode(&(*pGen),pNode,iFlags);` |
|       232 | 1948 | `				pNode->aNodeArgs = sSaved;` |
|       232 | 1949 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 1950 | `					break;` |
|         - | 1951 | `				}` |
|       232 | 1952 | `				if( n + 1 < nIsset ){` |
|       150 | 1953 | `					sxu32 nJz = 0;` |
|       150 | 1954 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,` |
|         - | 1955 | `						1 /* keep the false on the stack: it is the answer */,0,0,&nJz);` |
|       150 | 1956 | `					SySetPut(&sJz,(const void *)&nJz);` |
|         - | 1957 | `					/* Truthy link: drop it and ask the next operand. */` |
|       150 | 1958 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|        74 | 1959 | `				}` |
|       117 | 1960 | `			}` |
|        84 | 1961 | `			if( rc == SXRET_OK ){` |
|        84 | 1962 | `				sxu32 *aJz = (sxu32 *)SySetBasePtr(&sJz);` |
|        84 | 1963 | `				sxu32 nEnd = PH7_VmInstrLength(pGen->pVm);` |
|       232 | 1964 | `				for( n = 0 ; n < SySetUsed(&sJz) ; ++n ){` |
|       150 | 1965 | `					VmInstr *pFix = PH7_VmGetInstr(pGen->pVm,aJz[n]);` |
|       150 | 1966 | `					if( pFix ){` |
|       150 | 1967 | `						pFix->iP2 = nEnd;` |
|        74 | 1968 | `					}` |
|        76 | 1969 | `				}` |
|        41 | 1970 | `			}` |
|        84 | 1971 | `			SySetRelease(&sJz);` |
|        84 | 1972 | `			return rc;` |
|         - | 1973 | `		}` |
|       ! 0 | 1974 | `	}` |
|         - | 1975 | `	/* php evaluates an assignment TARGET's dynamic subscript and property names before the` |
|         - | 1976 | `` 	 * assigned value, and performs the fetches they belong to after it -- `$a[k()] = v()` `` |
|         - | 1977 | `	 * runs k() first, and the array it is about to write is untouched while v() runs, so a` |
|         - | 1978 | ``	 * recursive memoization (`$cache[$k] = compute()` whose compute() asks whether $k is`` |
|         - | 1979 | `	 * already there) sees the truth. PHL emitted the value first and the whole target after` |
|         - | 1980 | ``	 * it, which reversed every side effect in the target and answered `v k`.`` |
|         - | 1981 | `	 *` |
|         - | 1982 | `	 * Both halves are wanted, and in a stack machine they need three pieces: the names are` |
|         - | 1983 | `	 * compiled HERE, ahead of the value; the value lands on top of them; and the access` |
|         - | 1984 | `	 * chain is emitted last, reading each name back from where it was parked (OP_PICK) so` |
|         - | 1985 | `	 * nothing it creates is visible to the value. The names are snapshotted for the same` |
|         - | 1986 | `	 * reason a call's earlier arguments are -- the value runs between the push and the` |
|         - | 1987 | `	 * consumer, and a pushed value only borrows the bytes it was loaded from. */` |
|   4372977 | 1988 | `	if( pNode->pOp->iPrec == 18 && pNode->pOp->iOp != EXPR_OP_REF` |
|    870731 | 1989 | `	 && pNode->pLeft && pNode->pRight` |
|    870731 | 1990 | `	 && pNode->pRight->xCode != PH7_CompileList` |
|    870707 | 1991 | `	 && pNode->pRight->xCode != PH7_CompileShortList ){` |
|    870388 | 1992 | `		nParked = GenStateCollectStoreNames(pNode->pRight,apParked);` |
|    870388 | 1993 | `		if( nParked > 0 ){` |
|         - | 1994 | `			int nAt;` |
|       877 | 1995 | `			for( nAt = 0 ; nAt < nParked ; ++nAt ){` |
|       445 | 1996 | `				sxu32 nParkNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|         - | 1997 | `				/* The same flags the access arm would have compiled this name under:` |
|         - | 1998 | `				 * the access's own contexts masked off, and the reference-source` |
|         - | 1999 | `				 * marker with them -- the target emission strips that before it` |
|         - | 2000 | `				 * reaches a name too, since a name is never the source of a bind. */` |
|       665 | 2001 | `				rc = GenStateEmitExprCode(&(*pGen),apParked[nAt],` |
|       220 | 2002 | `					(iFlags & GEN_ACCESS_NAME_MASK & ~EXPR_FLAG_MEMBER_REFSRC)` |
|       440 | 2003 | `						\|EXPR_FLAG_RDONLY_LOAD);` |
|       445 | 2004 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 2005 | `					return rc;` |
|         - | 2006 | `				}` |
|         - | 2007 | `				/* Each name is its own nullsafe scope, exactly as it is where the` |
|         - | 2008 | `				 * subscript arm compiles it. */` |
|       445 | 2009 | `				GenStatePatchNullsafeJumps(pGen, nParkNsBase);` |
|       225 | 2010 | `			}` |
|       437 | 2011 | `			if( GenStateArgRunsCode(pNode->pLeft) ){` |
|       219 | 2012 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_SNAPSHOT,nParked,0,0,0);` |
|       107 | 2013 | `			}` |
|       221 | 2014 | `		}else{` |
|    869956 | 2015 | `			nParked = 0; /* nothing to park, or a chain longer than the parking area */` |
|         - | 2016 | `		}` |
|    434564 | 2017 | `	}` |
|   4372982 | 2018 | `	bIsChainOp = GEN_IS_CHAIN_OP(pNode->pOp->iOp);` |
|   4372982 | 2019 | `	nLhsFirst = PH7_VmInstrLength(pGen->pVm);` |
|         - | 2020 | `	/* Generate code for the left tree */` |
|   4372982 | 2021 | `	if( pNode->pLeft ){` |
|   4372970 | 2022 | `		sxu32 nLhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|         - | 2023 | `		GenCallArgs sArgs;` |
|   4372970 | 2024 | `		int bArgsEmitted = 0;` |
|   4372970 | 2025 | ``		sxu32 nNewClassInstr = 0; /* index+1 of a `new` operand's class-name push */`` |
|   4372970 | 2026 | `		SyZero(&sArgs,sizeof(sArgs));` |
|         - | 2027 | `		{` |
|         - | 2028 | `			/* The unset() target is the OUTERMOST access. When the intermediate container — the left` |
|         - | 2029 | ``			 * operand of `->`/`::`/`[]` — is itself a MEMBER access (`unset($o->a->b)` /`` |
|         - | 2030 | ``			 * `unset($o->arr[$k])`), strip the UNSET context from it: OP_MEMBER's iP2=2 unset mode is`` |
|         - | 2031 | `			 * DESTRUCTIVE (it removes the property), but the inner $o->a / $o->arr is only a read.` |
|         - | 2032 | `			 * A SUBSCRIPT intermediate is left alone — its LOAD_IDX iP2=5 must keep firing to` |
|         - | 2033 | ``			 * COW-separate the parent array (e.g. `$c['k'][1]` on a copy must not mutate the`` |
|         - | 2034 | `			 * original). isset/empty are never stripped: PHP stays silent on a missing intermediate` |
|         - | 2035 | ``			 * in `isset($o->a->b)`, which the suppression modes mirror. */`` |
|   4372970 | 2036 | `			sxi32 iLeftFlags = iFlags;` |
|         - | 2037 | `			/* The LHS chain whose subscript reads must be QUIET (LOAD_IDX iP2=8):` |
|         - | 2038 | ``			 * `??`'s left operand, and an isset()/empty() chain's intermediate`` |
|         - | 2039 | `			 * links -- php reads both silently and for the value. */` |
|   4372970 | 2040 | `			sxu32 nQuietLhsFirst = PH7_VmInstrLength(pGen->pVm);` |
|   4372970 | 2041 | `			int bQuietLhs = 0;` |
|         - | 2042 | `			/* D1 commit 2: a deferred element/property call arg records its lvalue chain, but` |
|         - | 2043 | `			 * that chain must be CONTIGUOUS. Only propagate DEFER_ARG to the base when the base` |
|         - | 2044 | `			 * is itself a continuable lvalue — a plain variable (an undefined base auto-defers),` |
|         - | 2045 | ``			 * another subscript, or a `->` member. If the base is anything else (most importantly`` |
|         - | 2046 | ``			 * a method CALL, e.g. `$o->items()->prop` or `$r->attributes->item(0)->nodeName`),`` |
|         - | 2047 | `			 * strip DEFER so that intermediate read is a NORMAL read, not a record-mode carrier. */` |
|   4372970 | 2048 | `			if( iLeftFlags & EXPR_FLAG_DEFER_ARG ){` |
|     57589 | 2049 | `				int bContinuable = pNode->pLeft` |
|     43617 | 2050 | `					&& ( (pNode->pLeft->pOp == 0 && pNode->pLeft->xCode == PH7_CompileVariable)` |
|     15254 | 2051 | `					  \|\| (pNode->pLeft->pOp != 0 && (pNode->pLeft->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|       789 | 2052 | `					                              \|\| pNode->pLeft->pOp->iOp == EXPR_OP_ARROW)) );` |
|     28797 | 2053 | `				if( !bContinuable ){` |
|       439 | 2054 | `					iLeftFlags &= ~EXPR_FLAG_DEFER_ARG;` |
|       216 | 2055 | `				}` |
|     14377 | 2056 | `			}` |
|         - | 2057 | `			/*` |
|         - | 2058 | `			 * An isset()/empty() CHAIN reads its intermediate links for their` |
|         - | 2059 | ``			 * VALUE, not for a truth. php walks `isset($o->a->b)` by fetching`` |
|         - | 2060 | ``			 * `$o->a` in BP_VAR_IS mode -- silent, but a real read that runs`` |
|         - | 2061 | `			 * __isset AND THEN __get (or offsetExists and then offsetGet) --` |
|         - | 2062 | `			 * and only the LAST link answers the isset question. PHL gave every` |
|         - | 2063 | `			 * link the terminal context, so the intermediate pushed a bool and` |
|         - | 2064 | `` 			 * the final `->b` was a property of `true`: `isset($model->rel->id)` `` |
|         - | 2065 | `			 * was FALSE for every class with accessors, and so was` |
|         - | 2066 | ``			 * `isset($container['k']['j'])` over ArrayAccess -- a silently wrong`` |
|         - | 2067 | `			 * guard, not a diagnostic.` |
|         - | 2068 | `			 *` |
|         - | 2069 | ``			 * The intermediate context is `??`'s (PH7_MEMBER_COALESCE for a`` |
|         - | 2070 | `			 * member, LOAD_IDX iP2=8 for a subscript, patched over the emitted` |
|         - | 2071 | `			 * range below), which is exactly "silent, and the value": EMPTY's` |
|         - | 2072 | `			 * would read a shade differently, since a class declaring __get with` |
|         - | 2073 | `			 * no __isset is read through __get for an intermediate link and is` |
|         - | 2074 | `			 * NOT for a terminal isset()/empty().` |
|         - | 2075 | `			 */` |
|   4372965 | 2076 | `			if( (iLeftFlags & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY))` |
|   2191564 | 2077 | `				&& pNode->pOp && pNode->pLeft && pNode->pLeft->pOp` |
|      8634 | 2078 | `				&& GEN_IS_ACCESS_OP(pNode->pOp->iOp)` |
|       182 | 2079 | `				&& GEN_IS_ACCESS_OP(pNode->pLeft->pOp->iOp) ){` |
|       127 | 2080 | `				iLeftFlags &= ~(EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY);` |
|       127 | 2081 | `				iLeftFlags \|= EXPR_FLAG_MEMBER_COALESCE\|EXPR_FLAG_QUIET_VAR;` |
|       127 | 2082 | `				bQuietLhs = 1;` |
|        61 | 2083 | `			}` |
|   4372965 | 2084 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
|   3560491 | 2085 | `				&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|   1377517 | 2086 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|   1365287 | 2087 | `					\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|     26509 | 2088 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|   4359699 | 2089 | `			}else if( iLeftFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|         - | 2090 | ``				/* A SUBSCRIPT intermediate of an unset chain (`unset($a['k']['n'])`) keeps`` |
|         - | 2091 | `				 * the unset context — it must COW-separate the parent and must NOT vivify a` |
|         - | 2092 | `				 * missing key — but it is a READ of the container, not an unset of it. The` |
|         - | 2093 | ``				 * UNSET_BASE context says exactly that: `$a['k']` is loaded, where the`` |
|         - | 2094 | `				 * plain unset context would have removed the ELEMENT (and, for an` |
|         - | 2095 | `				 * ArrayAccess base, called offsetUnset() on the intermediate key). */` |
|       425 | 2096 | `				iLeftFlags &= ~EXPR_FLAG_LOAD_IDX_UNSET;` |
|       425 | 2097 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_UNSET_BASE;` |
|       210 | 2098 | `			}` |
|         - | 2099 | `			/* Only the OUTERMOST access of a reference SOURCE is the reference fetch;` |
|         - | 2100 | `` 			 * every container under it is php's ordinary write base (`$r =& $o->arr['k']` `` |
|         - | 2101 | ``			 * creates `arr` the way `$o->arr['k'] = v` does). So the flag never travels`` |
|         - | 2102 | `			 * down as itself -- it decays to the write-lvalue flag, which the strip just` |
|         - | 2103 | ``			 * below then applies its own `->`-intermediate rule to. */`` |
|   4372970 | 2104 | `			if( iLeftFlags & EXPR_FLAG_MEMBER_REFSRC ){` |
|       303 | 2105 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_REFSRC;` |
|       303 | 2106 | `				iLeftFlags \|= EXPR_FLAG_MEMBER_WRITE;` |
|       149 | 2107 | `			}` |
|         - | 2108 | `			/* Write-lvalue propagation (mirrors the UNSET strip): EXPR_FLAG_MEMBER_WRITE marks the` |
|         - | 2109 | `			 * write target of an assignment and flows through a SUBSCRIPT to its base member` |
|         - | 2110 | ``			 * ($o->arr[$k]=v → create arr). But when THIS node is itself a `->`/`::` member access, its`` |
|         - | 2111 | `			 * left operand is an intermediate container that is only READ ($o->a->b=v must not create` |
|         - | 2112 | `			 * a; $o->arr[]=v reads $o), so strip MEMBER_WRITE there — PHP auto-vivifies arrays, never` |
|         - | 2113 | `` 			 * objects. (The flag is ADDED to the lvalue at the precedence-18 site below / the `??=` `` |
|         - | 2114 | ``			 * site, since `=` is right-associative and its lvalue is pNode->pRight.) */`` |
|   4372965 | 2115 | `			if( pNode->pOp` |
|   6540001 | 2116 | `				&& (pNode->pOp->iOp == EXPR_OP_ARROW` |
|   4357052 | 2117 | `					\|\| pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|   4341096 | 2118 | `					\|\| pNode->pOp->iOp == EXPR_OP_DC) ){` |
|     36535 | 2119 | `				iLeftFlags &= ~EXPR_FLAG_MEMBER_WRITE;` |
|     18246 | 2120 | `			}` |
|         - | 2121 | ``			/* `++`/`--` mutate their operand in place — the operand is a write`` |
|         - | 2122 | ``			 * lvalue exactly like a compound assign's (`$o->m[0]++` must tag the`` |
|         - | 2123 | ``			 * member base PH7_MEMBER_WRITE the way `$o->m[0] += 1` does: hooked`` |
|         - | 2124 | `			 * properties throw php's Indirect-modification Error, missing ones` |
|         - | 2125 | `			 * auto-vivify). The prec-18 site below handles the assign family;` |
|         - | 2126 | ``			 * `++`/`--` are unary, their operand is pLeft. */`` |
|   4372965 | 2127 | `			if( pNode->pOp` |
|   4372970 | 2128 | `				&& (pNode->pOp->iVmOp == PH7_OP_INCR \|\| pNode->pOp->iVmOp == PH7_OP_DECR) ){` |
|         - | 2129 | ``				/* `(new A)->p++` writes through a temporary exactly as `= 1` does --`` |
|         - | 2130 | ``				 * but a `$this++` is a read-modify-write php leaves to run time. */`` |
|     72549 | 2131 | `				rc = GenStateWriteTargetCheck(&(*pGen),pNode->pLeft,PH7_WTC_RMW);` |
|     72549 | 2132 | `				if( rc != SXRET_OK ){` |
|        45 | 2133 | `					return rc;` |
|         - | 2134 | `				}` |
|     72541 | 2135 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE` |
|         - | 2136 | ``					\| EXPR_FLAG_RMW_LOAD /* php warns before seeding `$undef++` */;`` |
|     36216 | 2137 | `			}` |
|         - | 2138 | ``			/* The SOURCE of a `=&` (pLeft, the operands having been swapped in`` |
|         - | 2139 | `			 * parse.c) is compiled in WRITE context by php too —` |
|         - | 2140 | ``			 * `zend_compile_var(source, BP_VAR_W, 1)` — which is what makes`` |
|         - | 2141 | ``			 * `$r =& $undef` and `$r =& $a[5]` CREATE the thing they bind to,`` |
|         - | 2142 | `			 * silently. PHL READ it, so both warned about what was missing and then` |
|         - | 2143 | `			 * refused the bind outright, leaving $r undefined as well. No` |
|         - | 2144 | `			 * RMW_LOAD: a bind does not read the source's value, and no` |
|         - | 2145 | `			 * MEMBER_WRITE: a handler-backed native property has no pointer for` |
|         - | 2146 | ``			 * php to hand out either, so `$r =& $iv->s` must keep taking the`` |
|         - | 2147 | `			 * read COPY it takes in php. */` |
|   4372962 | 2148 | `			if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_REF ){` |
|       457 | 2149 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_REFSRC;` |
|       226 | 2150 | `			}` |
|         - | 2151 | `			/* A destructuring target list that binds BY REFERENCE reads its SOURCE` |
|         - | 2152 | `			 * in write context for the same reason: the bind must reach the thing` |
|         - | 2153 | ``			 * the source NAMES. That is what keeps `[&$t] = $undef;` from warning`` |
|         - | 2154 | `			 * about what it is on the point of creating. */` |
|   4372957 | 2155 | `			if( iVmOp == PH7_OP_STORE && pNode->pRight && pNode->pRight->pStart` |
|    841144 | 2156 | `			 && (pNode->pRight->xCode == PH7_CompileList` |
|    841115 | 2157 | `			  \|\| pNode->pRight->xCode == PH7_CompileShortList)` |
|    420143 | 2158 | `			 && PH7_GenStateListSpanHasRef(pNode->pRight->pStart,pNode->pRight->pEnd) ){` |
|        45 | 2159 | `				iLeftFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_REFSRC;` |
|        22 | 2160 | `			}` |
|         - | 2161 | ``			/* `??` reads its LEFT operand in isset-context: an undefined or`` |
|         - | 2162 | `			 * UNINITIALIZED typed PROPERTY must yield the default rather than a` |
|         - | 2163 | `			 * warning/Error (php). Tag a member-access LHS so its OP_MEMBER takes` |
|         - | 2164 | `			 * the silent-lookup path (iP2 = ISSET), which still loads a present` |
|         - | 2165 | `			 * value. A SUBSCRIPT LHS is left untagged: LOAD_IDX's ISSET mode means` |
|         - | 2166 | ``			 * offsetExists (a bool), but `$o[$k] ?? d` needs the offsetGet value —`` |
|         - | 2167 | `			 * that path is already handled correctly by OP_NULLC. */` |
|   4372948 | 2168 | `			if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC && pNode->pLeft ){` |
|         - | 2169 | ``				/* php reads the ENTIRE left operand of `??` in isset-context: no`` |
|         - | 2170 | ``				 * "Undefined variable" for `$x ?? d` OR for the base of`` |
|         - | 2171 | ``				 * `$x['k'] ?? d`. QUIET_VAR silences the variable read wherever it`` |
|         - | 2172 | `				 * sits in the chain. */` |
|       561 | 2173 | `				iLeftFlags \|= EXPR_FLAG_QUIET_VAR;` |
|       561 | 2174 | `				bQuietLhs = 1;` |
|       556 | 2175 | `				if( pNode->pLeft->pOp` |
|       670 | 2176 | `					&& (pNode->pLeft->pOp->iOp == EXPR_OP_ARROW` |
|       399 | 2177 | `						\|\| pNode->pLeft->pOp->iOp == EXPR_OP_NULLSAFE_ARROW` |
|       324 | 2178 | `						\|\| pNode->pLeft->pOp->iOp == EXPR_OP_DC) ){` |
|         - | 2179 | `					/* A member-access LHS additionally takes OP_MEMBER's SILENT` |
|         - | 2180 | `					 * lookup so an uninitialized typed property yields the default` |
|         - | 2181 | `					 * instead of an Error. It used to borrow isset()'s context for` |
|         - | 2182 | `					 * that, which is the same mistake the comment below records for` |
|         - | 2183 | `					 * subscripts: silence is shared, but isset() context makes every` |
|         - | 2184 | ``					 * ACCESSOR answer a truth, and `$o->p ?? d` needs the accessor's`` |
|         - | 2185 | ``					 * VALUE — so `??` has its own member context. A SUBSCRIPT LHS`` |
|         - | 2186 | `					 * still takes neither: LOAD_IDX's ISSET mode means offsetExists` |
|         - | 2187 | ``					 * (a bool), while `$o[$k] ?? d` needs the offsetGet value —`` |
|         - | 2188 | `					 * OP_NULLC already handles that path. */` |
|       164 | 2189 | `					iLeftFlags \|= EXPR_FLAG_MEMBER_COALESCE;` |
|        80 | 2190 | `				}` |
|       278 | 2191 | `			}` |
|   4372948 | 2192 | `			if( iVmOp == PH7_OP_ERR_CTRL ){` |
|         - | 2193 | `				/* '@' must suppress the diagnostics raised WHILE its operand runs, so` |
|         - | 2194 | `				 * open the window here; the trailing emit below closes it (iP1 = 0). */` |
|     25795 | 2195 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_ERR_CTRL,1,0,0,0);` |
|     12871 | 2196 | `			}` |
|   4372948 | 2197 | `			if( iVmOp == PH7_OP_NEW ){` |
|         - | 2198 | ``				/* Mark the direct operand so a call node under it (`new C($a)`) keeps the`` |
|         - | 2199 | `				 * emission order OP_NEW is assembled from — see the bNewCallee branch above. */` |
|    115639 | 2200 | `				iLeftFlags \|= EXPR_FLAG_NEW_CALLEE;` |
|     57736 | 2201 | `			}` |
|   4372948 | 2202 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pLeft,iLeftFlags\|EXPR_FLAG_RDONLY_LOAD);` |
|   4372948 | 2203 | `			if( rc == SXRET_OK && bQuietLhs ){` |
|         - | 2204 | `				/* Mark EVERY subscript read in the quiet left chain (iP2=8).` |
|         - | 2205 | `				 * Peeking at the next instruction only catches the OUTERMOST` |
|         - | 2206 | ``				 * access, so `$d['x']['y'] ?? $v` still warned for the inner one;`` |
|         - | 2207 | `				 * and a forward scan would be unsound, since an unrelated sibling` |
|         - | 2208 | ``				 * access can sit immediately before a coalesce (`f($a['k'],`` |
|         - | 2209 | ``				 * $b['j'] ?? 1)`). The compiler knows the real extent, so it marks`` |
|         - | 2210 | `				 * the range it just emitted. Write-context codes (1/3/5) belong to` |
|         - | 2211 | ``				 * `??=` and keep their meaning. */`` |
|       683 | 2212 | `				sxu32 nEnd = PH7_VmInstrLength(pGen->pVm);` |
|         - | 2213 | `				sxu32 nAt;` |
|      2881 | 2214 | `				for( nAt = nQuietLhsFirst ; nAt < nEnd ; ++nAt ){` |
|      2203 | 2215 | `					VmInstr *pFix = PH7_VmGetInstr(pGen->pVm,nAt);` |
|      2203 | 2216 | `					if( pFix && pFix->iOp == PH7_OP_LOAD_IDX && pFix->iP2 == 0 ){` |
|       315 | 2217 | `						pFix->iP2 = 8;` |
|       155 | 2218 | `					}` |
|      1104 | 2219 | `				}` |
|       339 | 2220 | `			}` |
|         - | 2221 | `		}` |
|   4372948 | 2222 | `		if( rc != SXRET_OK ){` |
|        65 | 2223 | `			return rc;` |
|         - | 2224 | `		}` |
|   4372888 | 2225 | `		if( !bIsChainOp ){` |
|         - | 2226 | `			/* Non-chain parent: any nullsafe jumps produced by the LHS sub-tree` |
|         - | 2227 | `			 * target the end of that LHS chain, which is right here. */` |
|   2854386 | 2228 | `			GenStatePatchNullsafeJumps(pGen, nLhsNsBase);` |
|   1425138 | 2229 | `		}` |
|   4372888 | 2230 | `		if( iVmOp == PH7_OP_CALL ){` |
|   1122012 | 2231 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   1122012 | 2232 | `			if( pInstr ){` |
|   1122012 | 2233 | `				if ( pInstr->iOp == PH7_OP_LOADC ){` |
|   1092776 | 2234 | `					sxu32 nOrig = (sxu32)pInstr->iP2;` |
|         - | 2235 | `					sxu32 nQual;` |
|   1092776 | 2236 | `					int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 2237 | `					/* Prevent constant expansion but preserve the absolute flag` |
|         - | 2238 | `					 * so the later NEW handler (if any) can see it. */` |
|   1092776 | 2239 | `					pInstr->iP1 &= ~PH7_LOADC_EXPAND;` |
|         - | 2240 | `					/* Namespace-qualify the function name for CALL, unless the` |
|         - | 2241 | ``					 * literal is absolute (`\Foo(...)`). Only check function`` |
|         - | 2242 | `					 * imports — class imports must NOT affect function` |
|         - | 2243 | ``					 * resolution. For `new Foo()`, the CALL handler fires`` |
|         - | 2244 | `					 * before NEW; we store the original literal index in the` |
|         - | 2245 | `					 * CALL instruction's iP2 so the NEW handler can recover` |
|         - | 2246 | `					 * the unqualified name and re-qualify with class imports. */` |
|   1092776 | 2247 | `					if( bAbsolute ){` |
|       173 | 2248 | `						pInstr->iP2 = (sxi32)nOrig;` |
|        89 | 2249 | `					}else{` |
|   1092608 | 2250 | `						int fromImport = 0;` |
|   1092608 | 2251 | `						nQual = GenStateNsQualifyName(pGen,nOrig,&pGen->hUseFuncImports,&fromImport);` |
|   1092608 | 2252 | `						pInstr->iP2 = (sxi32)nQual;` |
|   1092608 | 2253 | `						if( nQual != nOrig ){` |
|         - | 2254 | `							/* Record the original literal index in the arg map` |
|         - | 2255 | `							 * (NOT in the CALL's iP2 — that is the hasSpread` |
|         - | 2256 | `							 * flag) so the NEW handler can recover the` |
|         - | 2257 | `							 * unqualified name and re-qualify with CLASS` |
|         - | 2258 | `							 * imports. */` |
|       301 | 2259 | `							if( p3 == 0 ){` |
|       301 | 2260 | `								VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|       296 | 2261 | `									&pGen->pVm->sAllocator, sizeof(VmCallArgMap));` |
|       301 | 2262 | `								if( pMap ){` |
|       301 | 2263 | `									SyZero(pMap, sizeof(VmCallArgMap));` |
|       301 | 2264 | `									p3 = (void *)pMap;` |
|       148 | 2265 | `								}` |
|       148 | 2266 | `							}` |
|       301 | 2267 | `							if( p3 ){` |
|       301 | 2268 | `								((VmCallArgMap *)p3)->nOrigNameLit = nOrig + 1;` |
|       301 | 2269 | `								if( !fromImport ){` |
|         - | 2270 | `									/* Mark as namespace-qualified */` |
|       271 | 2271 | `									((VmCallArgMap *)p3)->bIsNamespaced = 1;` |
|       133 | 2272 | `								}` |
|       148 | 2273 | `							}` |
|       148 | 2274 | `						}` |
|         - | 2275 | `					}` |
|    574491 | 2276 | `				}else if( (pInstr->iOp == PH7_OP_MEMBER /* $a->b(1,2,3) */` |
|     26031 | 2277 | `						&& !bNewCallee` |
|     22835 | 2278 | `						&& !(pNode->pLeft && (pNode->pLeft->iFlags & EXPR_NODE_PARENS)))` |
|     17828 | 2279 | `					\|\| pInstr->iOp == PH7_OP_NEW ){` |
|         - | 2280 | `					/* Method call,flag that. But NOT when the callee was an explicitly` |
|         - | 2281 | ``					 * PARENTHESISED member access: `($o->p)(...)` / `($o::$p)(...)` invokes`` |
|         - | 2282 | `					 * the VALUE of the property (php's variable-invocation), so the OP_MEMBER` |
|         - | 2283 | `					 * must stay a plain property READ (leaving the callable on the stack for` |
|         - | 2284 | `					 * OP_CALL to invoke) rather than being rewritten into a method-name` |
|         - | 2285 | ``					 * resolution — the parens are exactly what distinguishes `($o->p)()` from`` |
|         - | 2286 | ``					 * the method call `$o->p()`. */`` |
|     22801 | 2287 | `					pInstr->iP2 = 1;` |
|         - | 2288 | ``					/* …and a `new`'s operand is the third shape that is NOT a method`` |
|         - | 2289 | ``					 * call: after `new`, php's class expression is a `new_variable`,`` |
|         - | 2290 | `					 * which has no call in it, so the parentheses that follow are` |
|         - | 2291 | ``					 * always the CONSTRUCTOR's. `new $config->defType()` -- how`` |
|         - | 2292 | `					 * nette/di spells every service it builds -- read the property as` |
|         - | 2293 | ``					 * a METHOD and died on `Call to undefined method`` |
|         - | 2294 | ``					 * stdClass::defType()`. Same exception, same reason, as the`` |
|         - | 2295 | `					 * parenthesised member above: the OP_MEMBER stays a property READ` |
|         - | 2296 | `					 * and leaves the class NAME for OP_NEW. */` |
|         - | 2297 | `					/* This OP_MEMBER is where php SCREENS a method call -- an undefined` |
|         - | 2298 | `					 * or inaccessible method, a class that is not there -- and php` |
|         - | 2299 | `					 * reports that refusal at the line the CALL BEGINS on. The emitter` |
|         - | 2300 | `					 * stamped it with the token the generator was standing on, which` |
|         - | 2301 | `					 * for a call spanning several lines is its closing ')'. Same rule,` |
|         - | 2302 | `					 * same source, as the OP_CALL/OP_NEW stamp further down. */` |
|     22801 | 2303 | `					if( pInstr->iOp == PH7_OP_MEMBER && pNode->pStart ){` |
|     22793 | 2304 | `						pInstr->nLine = pNode->pStart->nLine;` |
|     11375 | 2305 | `					}` |
|         - | 2306 | ``					/* A static call with a DYNAMIC method name (`C::$m(...)`): the`` |
|         - | 2307 | ``					 * static-`::` codegen folded the variable NAME into OP_MEMBER->p3`` |
|         - | 2308 | ``					 * as if it were a static-PROPERTY read (`C::$m`), but in a CALL the`` |
|         - | 2309 | `					 * method name is the variable's VALUE. Rebuild the sequence` |
|         - | 2310 | `					 * [class, OP_LOAD $m -> value, OP_MEMBER(static, method)] so the` |
|         - | 2311 | `					 * dynamic name is read off the stack, matching the instance` |
|         - | 2312 | ``					 * (`$o->$m()`) path. iP1==1 marks a static member. */`` |
|     22801 | 2313 | `					if( pInstr->iOp == PH7_OP_MEMBER && pInstr->iP1 == 1 && pInstr->p3 ){` |
|        21 | 2314 | `						void *pDynName = pInstr->p3;` |
|        21 | 2315 | `						(void)PH7_VmPopInstr(pGen->pVm);` |
|        21 | 2316 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOAD,0,0,pDynName,0);` |
|        21 | 2317 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_MEMBER,1,PH7_MEMBER_METHOD,0,0);` |
|        10 | 2318 | `					}` |
|     11379 | 2319 | `				}` |
|    559841 | 2320 | `			}` |
|         - | 2321 | `			/* The callee is resolved; NOW emit the arguments. php's order — the callee` |
|         - | 2322 | `			 * first, at its INIT_FCALL / INIT_METHOD_CALL, and the arguments only after` |
|         - | 2323 | ``			 * it — is what makes `(new C)->priv(boom())` report php's `Call to private`` |
|         - | 2324 | `` 			 * method` instead of whatever the argument threw, and what keeps `$o?->m(f())` `` |
|         - | 2325 | ``			 * from running `f()` on a null receiver. It also puts the callee in reach of`` |
|         - | 2326 | `			 * the argument ops one opcode EARLIER than OP_CALL.` |
|         - | 2327 | `			 *` |
|         - | 2328 | `			 * The stack that leaves here is therefore [callee][args…] — the mirror of the` |
|         - | 2329 | `			 * layout OP_CALL's whole dispatch is written against (the method-name pair` |
|         - | 2330 | `			 * below the arguments, the spread runs counted down from the top, the` |
|         - | 2331 | `			 * deferred-argument re-walk). OP_ROT_CALLEE turns the region back over just` |
|         - | 2332 | `			 * before the call, so nothing downstream of it changes. */` |
|   1122012 | 2333 | `			if( !bArgsEmitted ){` |
|         - | 2334 | `				int bTwoSlot;` |
|   1122012 | 2335 | `				sArgs.p3 = p3; /* the namespace map built just above, if any */` |
|         - | 2336 | `				/* A METHOD callee leaves TWO slots — [receiver][method name] — which` |
|         - | 2337 | `				 * OP_CALL reads as one callee (the receiver answers $this and the` |
|         - | 2338 | `				 * late-static-binding class); anything else leaves one. The instruction` |
|         - | 2339 | `				 * just emitted is what decides it. */` |
|   1122012 | 2340 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   1133414 | 2341 | `				bTwoSlot = pInstr && pInstr->iOp == PH7_OP_MEMBER` |
|    571281 | 2342 | `					&& pInstr->iP2 == PH7_MEMBER_METHOD` |
|         - | 2343 | `					/* …unless the member NAME was folded into p3 rather than pushed:` |
|         - | 2344 | `					 * that shape pushes the target alone, so the op leaves one slot,` |
|         - | 2345 | `					 * which is the same distinction vm_ops_oo.c makes before popping. */` |
|   1684173 | 2346 | `					&& pInstr->p3 == 0;` |
|         - | 2347 | `				/* Screen the callee HERE, where php screens it: an undefined function, a` |
|         - | 2348 | `				 * callable string/array naming nothing, a value that is not callable at` |
|         - | 2349 | `				 * all. A METHOD callee needs none — its OP_MEMBER just did it, against` |
|         - | 2350 | `				 * the entry it chose. An FCC needs none either: OP_LOAD_FCC runs the same` |
|         - | 2351 | `				 * screen, and nothing runs before it. And with NO arguments the call` |
|         - | 2352 | `				 * itself is already the first thing to happen, so there is nothing to` |
|         - | 2353 | `				 * order and no reason to pay for a second resolution. */` |
|         - | 2354 | `				{` |
|   1122012 | 2355 | `					ph7_expr_node **apCallArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   1122012 | 2356 | `					sxi32 nCallArg = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|   1211522 | 2357 | `					int bNodeFcc = nCallArg == 1 && apCallArg[0]` |
|   1448390 | 2358 | `						&& (apCallArg[0]->iFlags & EXPR_NODE_FCC);` |
|   1122012 | 2359 | `					if( bNewCallee ){` |
|         - | 2360 | ``						/* A `new`'s operand: the screen is OP_NEW itself, run with no`` |
|         - | 2361 | `						 * arguments on the stack (iP1 = -1). It asks every refusal the` |
|         - | 2362 | `						 * real pass asks and leaves the class name standing, so the two` |
|         - | 2363 | `						 * cannot disagree. Record where that push is — the NEW codegen` |
|         - | 2364 | `						 * used to find it one instruction behind the trailing OP_CALL,` |
|         - | 2365 | `						 * and the argument list now sits in between. */` |
|    112521 | 2366 | `						nNewClassInstr = PH7_VmInstrLength(pGen->pVm);` |
|    112521 | 2367 | `						if( nCallArg > 0 ){` |
|    109683 | 2368 | `							PH7_VmEmitInstr(pGen->pVm,PH7_OP_NEW,-1,0,0,0);` |
|     54765 | 2369 | `						}` |
|   1065675 | 2370 | `					}else if( nCallArg > 0 && !bTwoSlot && !bNodeFcc ){` |
|         - | 2371 | ``						/* This screen is where a `Call to undefined function` is`` |
|         - | 2372 | `						 * raised for a call that HAS arguments, and php reports such a` |
|         - | 2373 | `						 * call at the line the CALLEE is written on -- not at the` |
|         - | 2374 | `						 * closing parenthesis, which is where the emitter's token` |
|         - | 2375 | `						 * cursor has reached by now. The callee's own load is the` |
|         - | 2376 | ``						 * instruction immediately behind (`pInstr`, peeked just above`` |
|         - | 2377 | `						 * for bTwoSlot), so its line is the one to carry. A call with` |
|         - | 2378 | `						 * no arguments needs nothing: OP_CALL itself is then the first` |
|         - | 2379 | `						 * thing to run and already reports the callee's line. */` |
|    961813 | 2380 | `						sxu32 nInitIdx = PH7_VmInstrLength(pGen->pVm);` |
|   1443716 | 2381 | `						sxu32 nCalleeLine = pNode->pStart` |
|    961808 | 2382 | `							? pNode->pStart->nLine : (pInstr ? pInstr->nLine : 0);` |
|    961882 | 2383 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL_INIT,0,` |
|    479974 | 2384 | `							((p3 && ((VmCallArgMap *)p3)->bIsNamespaced)` |
|    481903 | 2385 | `								? PH7_CALLINIT_NAMESPACED : 0)` |
|   1443711 | 2386 | `							\| (GenStateCalleeIsConstruct(pNode->pLeft)` |
|    481903 | 2387 | `								? PH7_CALLINIT_CONSTRUCT : 0),0,0);` |
|    961813 | 2388 | `						if( nCalleeLine ){` |
|    961813 | 2389 | `							VmInstr *pInitInstr = PH7_VmGetInstr(pGen->pVm,nInitIdx);` |
|    961813 | 2390 | `							if( pInitInstr ){` |
|    961813 | 2391 | `								pInitInstr->nLine = nCalleeLine;` |
|    479905 | 2392 | `							}` |
|    479905 | 2393 | `						}` |
|    479905 | 2394 | `					}` |
|         - | 2395 | `				}` |
|   1122012 | 2396 | `				rc = GenStateEmitCallArgs(&(*pGen),pNode,iFlags,&sArgs);` |
|   1122012 | 2397 | `				if( rc != SXRET_OK ){` |
|        13 | 2398 | `					return rc;` |
|         - | 2399 | `				}` |
|   1122002 | 2400 | `				iP1 = sArgs.iP1;` |
|   1122002 | 2401 | `				iP2 = sArgs.iP2;` |
|   1122002 | 2402 | `				p3  = sArgs.p3;` |
|   1122002 | 2403 | `				bFcc = sArgs.bFcc;` |
|   1122002 | 2404 | `				if( iP1 > 0 \|\| (iP2 & PH7_CALL_SPREAD) ){` |
|   1619892 | 2405 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_ROT_CALLEE,iP1,` |
|   1080647 | 2406 | `						((iP2 & PH7_CALL_SPREAD) ? PH7_ROT_SPREAD : 0)` |
|   1080647 | 2407 | `						\| (bTwoSlot ? PH7_ROT_TWOSLOT : 0),0,0);` |
|    539240 | 2408 | `				}` |
|   1122002 | 2409 | `				if( bNewCallee && nNewClassInstr > 0 ){` |
|    112521 | 2410 | `					if( p3 == 0 ){` |
|      2799 | 2411 | `						VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|      2794 | 2412 | `							&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|      2799 | 2413 | `						if( pMap ){` |
|      2799 | 2414 | `							SyZero(pMap,sizeof(VmCallArgMap));` |
|      2799 | 2415 | `							p3 = (void *)pMap;` |
|      1397 | 2416 | `						}` |
|      1397 | 2417 | `					}` |
|    112521 | 2418 | `					if( p3 ){` |
|    112521 | 2419 | `						((VmCallArgMap *)p3)->nNewClassInstr = nNewClassInstr;` |
|     56179 | 2420 | `					}` |
|     56179 | 2421 | `				}` |
|    559841 | 2422 | `			}` |
|   3810717 | 2423 | `		}else if( iVmOp == PH7_OP_LOAD_IDX ){` |
|         - | 2424 | `			ph7_expr_node **apNode;` |
|         - | 2425 | `			sxi32 n;` |
|    359970 | 2426 | `			sxi32 iChildMask = GEN_ACCESS_NAME_MASK;` |
|         - | 2427 | `			/* Recurse and generate bytecodes for array index */` |
|    359970 | 2428 | `			apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|    631125 | 2429 | `			for( n = 0 ; n < (sxi32)SySetUsed(&pNode->aNodeArgs) ; ++n ){` |
|    271160 | 2430 | `				sxu32 nIdxNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|    271160 | 2431 | `				rc = GenStateEmitExprCode(&(*pGen),apNode[n],iFlags&iChildMask);` |
|    271160 | 2432 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 2433 | `					return rc;` |
|         - | 2434 | `				}` |
|         - | 2435 | `				/* Each subscript index is an independent nullsafe scope. */` |
|    271160 | 2436 | `				GenStatePatchNullsafeJumps(pGen, nIdxNsBase);` |
|    135392 | 2437 | `			}` |
|    359970 | 2438 | `			if( SySetUsed(&pNode->aNodeArgs) > 0 ){` |
|    271160 | 2439 | `				iP1 = 1; /* Node have an index associated with it */` |
|    135392 | 2440 | `			}else{` |
|         - | 2441 | ``				/* `[]` names the element a WRITE is about to create, so php allows it`` |
|         - | 2442 | `				 * only where a write lands: an assignment target (plain, compound,` |
|         - | 2443 | ``				 * `=&`, a list()/foreach target) and a by-reference argument. Every`` |
|         - | 2444 | `				 * other placement is a COMPILE error there — PHL accepted them all and` |
|         - | 2445 | ``				 * answered NULL after a PH7-worded notice, so `$x = $a[];`,`` |
|         - | 2446 | ``				 * `isset($a[])` and `unset($a[][0])` were silent no-ops on source php`` |
|         - | 2447 | `				 * refuses to run. A call ARGUMENT is the one shape php also leaves to` |
|         - | 2448 | `				 * runtime (it cannot know the parameter's by-ref-ness at compile time),` |
|         - | 2449 | `				 * which is what DEFER_ARG marks. */` |
|     88815 | 2450 | `				if( iFlags & (EXPR_FLAG_LOAD_IDX_UNSET\|EXPR_FLAG_LOAD_IDX_UNSET_BASE) ){` |
|       ! 0 | 2451 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,` |
|       ! 0 | 2452 | `						pNode->pStart ? pNode->pStart->nLine : 0,` |
|         - | 2453 | `						"Cannot use [] for unsetting");` |
|       ! 0 | 2454 | `					return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 2455 | `				}` |
|     88815 | 2456 | `				if( (iFlags & (EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_DEFER_ARG)) == 0 ){` |
|       ! 0 | 2457 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,` |
|       ! 0 | 2458 | `						pNode->pStart ? pNode->pStart->nLine : 0,` |
|         - | 2459 | `						"Cannot use [] for reading");` |
|       ! 0 | 2460 | `					return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 2461 | `				}` |
|         - | 2462 | `			}` |
|    359970 | 2463 | `			if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|         - | 2464 | `				/* offsetExists for ArrayAccess; peek-only for arrays */` |
|     16615 | 2465 | `				iP2 = 4;` |
|    351654 | 2466 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|         - | 2467 | `				/* offsetUnset for ArrayAccess; for an array, remove the ELEMENT. */` |
|       261 | 2468 | `				iP2 = 5;` |
|    343232 | 2469 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET_BASE ){` |
|         - | 2470 | `				/* An unset chain's intermediate container: read it, but with the` |
|         - | 2471 | `				 * unset context's COW-separate and no-vivify rules. */` |
|        26 | 2472 | `				iP2 = 10;` |
|    343092 | 2473 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|         - | 2474 | `				/* offsetExists+offsetGet for ArrayAccess so empty() can` |
|         - | 2475 | `				 * short-circuit on missing keys without invoking offsetGet` |
|         - | 2476 | `				 * unnecessarily; peek-only for arrays (same as iP2=0). */` |
|        61 | 2477 | `				iP2 = 6;` |
|    343052 | 2478 | `			}else if( iFlags & EXPR_FLAG_LOAD_IDX_STORE ){` |
|         - | 2479 | `				/* Create an empty entry when the desired index is not found.` |
|         - | 2480 | ``				 * A read-modify-write target (`$a[k] += v`, `$a[k]++`) creates it`` |
|         - | 2481 | `				 * the same way but READS it first, so php warns about the missing` |
|         - | 2482 | `				 * key before seeding it — the RMW context says which of the two` |
|         - | 2483 | `				 * this is (VM_IDX_CTX_RMW). The flag rides the whole LHS chain, so` |
|         - | 2484 | `				 * an intermediate level gets it too, as php's BP_VAR_RW fetch does. */` |
|    193787 | 2485 | `				iP2 = (iFlags & EXPR_FLAG_RMW_LOAD) ? VM_IDX_CTX_RMW : 1;` |
|    245996 | 2486 | `			}else if( iFlags & EXPR_FLAG_DEFER_ARG ){` |
|         - | 2487 | `				/* D1 commit 2: deferred by-ref/by-value element arg. Behaves as a read but,` |
|         - | 2488 | `				 * on a lookup miss, records the lvalue path instead of warning; OP_CALL` |
|         - | 2489 | `				 * re-walks it in vivify (by-ref) or read+warn (by-value) mode. */` |
|     26101 | 2490 | `				iP2 = 9;` |
|     13034 | 2491 | `			}` |
|   3070644 | 2492 | `		}else if( pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         - | 2493 | `			/* POP the left node — its answer is dropped exactly as a statement's is,` |
|         - | 2494 | `			 * so a #[\NoDiscard] callee warns for it too (php warns for every` |
|         - | 2495 | ``			 * element of a `for` clause list, not just the last). */`` |
|        14 | 2496 | `			GenStateMarkDiscardedCall(&(*pGen));` |
|        14 | 2497 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         6 | 2498 | `		}` |
|   2182948 | 2499 | `	}` |
|   4372890 | 2500 | `	rc = SXRET_OK;` |
|   4372890 | 2501 | `	nJmpIdx = 0;` |
|         - | 2502 | `	/* For :: (static member access), namespace-qualify the class name (left operand).` |
|         - | 2503 | `	 * The left child was just compiled; its LOADC is the last instruction.` |
|         - | 2504 | `	 * Skip self/static/parent — these are keywords, not class names. */` |
|   4372890 | 2505 | `	if( iVmOp == PH7_OP_MEMBER && pNode->pOp->iOp == EXPR_OP_DC ){` |
|      4606 | 2506 | `		pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      4606 | 2507 | `		if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|      4542 | 2508 | `			ph7_value *pLitCheck = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|      4542 | 2509 | `			int isSpecial = 0;` |
|      4542 | 2510 | `			if( pLitCheck && (pLitCheck->iFlags & MEMOBJ_STRING) ){` |
|      4518 | 2511 | `				const char *z = (const char *)SyBlobData(&pLitCheck->sBlob);` |
|      4518 | 2512 | `				sxu32 n = (sxu32)SyBlobLength(&pLitCheck->sBlob);` |
|      4513 | 2513 | `				if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|      4146 | 2514 | `					(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      2079 | 2515 | `					(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|       645 | 2516 | `					isSpecial = 1;` |
|       320 | 2517 | `				}` |
|      2258 | 2518 | `			}` |
|      4554 | 2519 | `			pInstr->iP1 = 0;` |
|         - | 2520 | ``			/* A leading `\` (NSSEP) makes the class name ABSOLUTE: it names the`` |
|         - | 2521 | `			 * global class, so it must NOT be re-qualified with the current namespace.` |
|         - | 2522 | ``			 * `\Closure::bind(...)` inside `namespace X` is Closure, not X\Closure.`` |
|         - | 2523 | ``			 * (Multi-component `\A\B::m` already resolves absolutely because its`` |
|         - | 2524 | ``			 * literal keeps a backslash; the single-component `\Closure` lost it.) */`` |
|         - | 2525 | `			{` |
|      6812 | 2526 | `				int bAbsolute = (pNode->pLeft && pNode->pLeft->pStart` |
|      6792 | 2527 | `					&& (pNode->pLeft->pStart->nType & PH7_TK_NSSEP));` |
|      4530 | 2528 | `				if( !isSpecial && !bAbsolute ){` |
|      3840 | 2529 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      1913 | 2530 | `				}` |
|         - | 2531 | `			}` |
|         - | 2532 | `			/* Foo::class — resolve at compile time. The LOADC already holds the` |
|         - | 2533 | `			 * namespace-qualified name. self/static/parent need runtime resolution. */` |
|      4530 | 2534 | `			if( !isSpecial && pNode->pRight && pNode->pRight->pStart ){` |
|      3890 | 2535 | `				SyToken *pRightTok = pNode->pRight->pStart;` |
|      3890 | 2536 | `				if( (pRightTok->nType & PH7_TK_KEYWORD) &&` |
|       244 | 2537 | `				    SX_PTR_TO_INT(pRightTok->pUserData) == PH7_TKWRD_CLASS ){` |
|       153 | 2538 | `					return SXRET_OK;` |
|         - | 2539 | `				}` |
|      1864 | 2540 | `			}` |
|      2184 | 2541 | `		}` |
|      2216 | 2542 | `	}` |
|   4372725 | 2543 | `	if( iVmOp == PH7_OP_IS_A && nLhsFirst < PH7_VmInstrLength(pGen->pVm)` |
|     16499 | 2544 | `	 && GenStateInstanceofFoldsLhs(&(*pGen),nLhsFirst) ){` |
|         - | 2545 | `` 		/* php never even compiles the class operand when the SUBJECT of `instanceof` `` |
|         - | 2546 | `		 * is a compile-time constant: zend_compile_instanceof folds the whole` |
|         - | 2547 | `		 * expression to FALSE the moment its left operand comes back IS_CONST, so` |
|         - | 2548 | `` 		 * `5 instanceof $x` is false whatever $x holds -- while `$v instanceof $x` `` |
|         - | 2549 | `		 * with the same 5 in $v reaches the runtime opcode and is refused when $x is` |
|         - | 2550 | `		 * neither an object nor a string. The two spellings really do answer` |
|         - | 2551 | `		 * differently, so the screen added to OP_IS_A has to be told which one this` |
|         - | 2552 | `		 * is, and GenStateInstanceofFoldsLhs reads it off the subject's own` |
|         - | 2553 | `		 * instructions. */` |
|         - | 2554 | `		ph7_value *pFalse;` |
|         - | 2555 | `		sxu32 nFalseIdx;` |
|         - | 2556 | `		/* The subject's own instructions go with it: php frees the folded operand` |
|         - | 2557 | `		 * (zend_do_free) rather than leaving it to be computed and dropped. */` |
|        45 | 2558 | `		while( PH7_VmInstrLength(pGen->pVm) > nLhsFirst ){` |
|        29 | 2559 | `			(void)PH7_VmPopInstr(pGen->pVm);` |
|         1 | 2560 | `		}` |
|        17 | 2561 | `		pFalse = PH7_ReserveConstObj(pGen->pVm,&nFalseIdx);` |
|        17 | 2562 | `		if( pFalse == 0 ){` |
|       ! 0 | 2563 | `			PH7_GenCompileError(&(*pGen),E_ERROR,1,"PH7 engine is running out of memory");` |
|       ! 0 | 2564 | `			return SXERR_ABORT;` |
|         - | 2565 | `		}` |
|        17 | 2566 | `		PH7_MemObjInitFromBool(pGen->pVm,pFalse,0);` |
|        17 | 2567 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,(sxi32)nFalseIdx,0,0);` |
|        17 | 2568 | `		return SXRET_OK;` |
|         - | 2569 | `	}` |
|         - | 2570 | `	/* An operand waiting on the stack BORROWS its source's string bytes: PH7_MemObjLoad` |
|         - | 2571 | `	 * hands back a read-only view -- a pointer plus the length the source had at the push` |
|         - | 2572 | ``	 * -- so a right operand that can RUN (an assignment, a call, `++`, a fetch that may`` |
|         - | 2573 | ``	 * reach __get) writes through a left operand that is already there. `$a[0] . ($a[0] =`` |
|         - | 2574 | ``	 * 'new')` answered `newnew` for php's `oldnew`, and when the write REALLOCATES the`` |
|         - | 2575 | `	 * buffer instead of overwriting it the view is a use-after-free.` |
|         - | 2576 | `	 *` |
|         - | 2577 | `	 * php has no such window, and its rule has two halves that point opposite ways: it` |
|         - | 2578 | ``	 * materializes every operand where it is written EXCEPT a plain `$var`, which it never`` |
|         - | 2579 | ``	 * pushes at all -- the operator reads the compiled variable itself, so `$x . ($x =`` |
|         - | 2580 | ``	 * 'new')` is `newnew` there and `$n = 1; $n - ($n = 5)` is 0. A copy is the answer for`` |
|         - | 2581 | `	 * one half and the wrong answer for the other.` |
|         - | 2582 | `	 *` |
|         - | 2583 | `	 * So: copy the shapes php copies, and for the plain variable MOVE its load past the` |
|         - | 2584 | `	 * right operand (OP_SWAP puts the two back in the operator's order), which is what` |
|         - | 2585 | `	 * "read it at the operator" means in a stack machine. Both arms are gated on the right` |
|         - | 2586 | ``	 * operand being able to run something, so an ordinary `$a . $b` emits neither. */`` |
|   4372709 | 2587 | `	if( GenStateBinOpReadsBothOperands(iVmOp) && pNode->pLeft && pNode->pRight` |
|   1324045 | 2588 | `	 && GenStateArgRunsCode(pNode->pRight) ){` |
|    172227 | 2589 | `		if( GenStateNodeIsSimpleVar(pNode->pLeft) ){` |
|     56740 | 2590 | `			VmInstr *pLhsLoad = PH7_VmPeekInstr(pGen->pVm);` |
|     56735 | 2591 | `			if( pLhsLoad && pLhsLoad->iOp == PH7_OP_LOAD` |
|     56740 | 2592 | `			 && PH7_VmInstrLength(pGen->pVm) == nLhsFirst + 1 ){` |
|     56740 | 2593 | `				sMovedLhs = *pLhsLoad;` |
|     56740 | 2594 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|     56740 | 2595 | `				bMoveLhs = 1;` |
|     28330 | 2596 | `			}` |
|    143817 | 2597 | `		}else if( GenStateNodeMayAliasStorage(pNode->pLeft) ){` |
|     16888 | 2598 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_SNAPSHOT,0,0,0,0);` |
|      8429 | 2599 | `		}` |
|     85984 | 2600 | `	}` |
|         - | 2601 | `	/* Generate code for the right tree */` |
|   4372714 | 2602 | `	if( pNode->pRight ){` |
|   2479792 | 2603 | `		if( iVmOp == PH7_OP_LAND ){` |
|         - | 2604 | `			/* Emit the false jump so we can short-circuit the logical and */` |
|    135570 | 2605 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|   2411913 | 2606 | `		}else if (iVmOp == PH7_OP_LOR ){` |
|         - | 2607 | `			/* Emit the true jump so we can short-circuit the logical or*/` |
|     95630 | 2608 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_JNZ,1/* Keep the value on the stack */,0,0,&nJmpIdx);` |
|   2296348 | 2609 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLC ){` |
|         - | 2610 | `			/* Null coalescing: if LHS is not null, jump past RHS */` |
|       561 | 2611 | `			iVmOp = 0; /* No binary operator to emit */` |
|       561 | 2612 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLC,0,0,0,&nJmpIdx);` |
|   2248410 | 2613 | `		}else if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_NULLSAFE_ARROW ){` |
|         - | 2614 | ``			/* Nullsafe operator `?->` (PHP 8.0): if LHS is null, short-circuit`` |
|         - | 2615 | `			 * the entire containing postfix chain to null. The jump target is` |
|         - | 2616 | `			 * patched later by the innermost non-chain ancestor (or by` |
|         - | 2617 | `			 * PH7_CompileExpr at the outer boundary). Leaves NULL on the stack` |
|         - | 2618 | `			 * when taken; otherwise falls through, leaving the object on stack` |
|         - | 2619 | `			 * so the PH7_OP_MEMBER that follows can consume it. */` |
|       177 | 2620 | `			sxu32 nNsJmp = 0;` |
|       177 | 2621 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_NULLSAFE_JMP,0,0,0,&nNsJmp);` |
|       177 | 2622 | `			SySetPut(&pGen->aNullsafeJmp,(const void *)&nNsJmp);` |
|   2247960 | 2623 | `		}else if( pNode->pOp->iPrec == 18 /* Combined binary operators [i.e: =,'.=','+=',*=' ...] precedence */` |
|   1811908 | 2624 | ``			\|\| pNode->pOp->iOp == EXPR_OP_REF /* `=&` reference bind */ ){`` |
|         - | 2625 | `` 			/* The lvalue is the RIGHT operand (prec-18 ops are right-associative; `=&` `` |
|         - | 2626 | `			 * swaps its operands in parse.c so its target is pRight too). Mark it a write` |
|         - | 2627 | `			 * target so a missing base (the container of a subscript-write, or a bare` |
|         - | 2628 | `` 			 * `$o->p`) is auto-created — PHP auto-vivifies on a plain write AND on a `=&` `` |
|         - | 2629 | ``			 * bind (`$a[0] =& $x` creates $a as [0 => &$x], it does not warn). */`` |
|         - | 2630 | `			/* php's compile-time write-target rules first ($this, a temporary base,` |
|         - | 2631 | `			 * the call that is the target itself). A COMPOUND assignment is a` |
|         - | 2632 | `			 * read-modify-write: php's $this rule does not reach it. */` |
|    901173 | 2633 | `			rc = GenStateWriteTargetCheck(&(*pGen),pNode->pRight,` |
|    449976 | 2634 | `				(iVmOp == PH7_OP_STORE \|\| pNode->pOp->iOp == EXPR_OP_REF)` |
|         - | 2635 | `					? 0 : PH7_WTC_RMW);` |
|    871134 | 2636 | `			if( rc != SXRET_OK ){` |
|        26 | 2637 | `				return rc;` |
|         - | 2638 | `			}` |
|    871112 | 2639 | `			if( pNode->pOp->iOp == EXPR_OP_REF ){` |
|         - | 2640 | `				/* php compiles a reference SOURCE in write context too` |
|         - | 2641 | `` 				 * (`zend_compile_var(source, BP_VAR_W, 1)`), so `$r =& (new A)->p` `` |
|         - | 2642 | ``				 * and `$r =& (clone $o)->p` are the same two compile refusals a`` |
|         - | 2643 | `				 * write to them would be. Only the call-as-target question is not` |
|         - | 2644 | ``				 * asked here: `$r =& f()` is legal. The operands were swapped in`` |
|         - | 2645 | `				 * parse.c, so the source is pLeft. */` |
|       453 | 2646 | `				rc = GenStateWriteTargetCheck(&(*pGen),pNode->pLeft,PH7_WTC_REFSRC);` |
|       453 | 2647 | `				if( rc != SXRET_OK ){` |
|       ! 0 | 2648 | `					return rc;` |
|         - | 2649 | `				}` |
|       224 | 2650 | `			}` |
|    871112 | 2651 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_STORE \| EXPR_FLAG_MEMBER_WRITE;` |
|    871112 | 2652 | `			if( iVmOp != PH7_OP_STORE && pNode->pOp->iOp != EXPR_OP_REF ){` |
|         - | 2653 | ``				/* COMPOUND assignment (`.=`, `+=`, ...) READS the target first, so`` |
|         - | 2654 | ``				 * php warns when it is undefined and then seeds it; a plain `=` and a`` |
|         - | 2655 | ``				 * `=&` rebind write without reading the target and stay silent. */`` |
|     29608 | 2656 | `				iFlags \|= EXPR_FLAG_RMW_LOAD;` |
|     14782 | 2657 | `			}` |
|    434926 | 2658 | `		}` |
|   2479770 | 2659 | `		nRhsNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|         - | 2660 | `		/* The RIGHT operand is never the reference source: for an assignment it is the` |
|         - | 2661 | `		 * TARGET (the operands were swapped), and for a member access it is the property` |
|         - | 2662 | ``		 * NAME -- and a name written as an expression (`$r =& $o->{$a->b}`) would`` |
|         - | 2663 | `		 * otherwise be compiled as a write-context fetch of its own. */` |
|   2479770 | 2664 | `		iRhsFlags = iFlags & ~EXPR_FLAG_MEMBER_REFSRC;` |
|   2479770 | 2665 | `		if( nParked > 0 ){` |
|         - | 2666 | `			/* The target's own emission is the only place a PICK may stand in for a` |
|         - | 2667 | `			 * name; a nested assignment inside the value has already been compiled` |
|         - | 2668 | `			 * with nothing parked, and one inside the target's container answers for` |
|         - | 2669 | `			 * itself through this save and the restore below. */` |
|       437 | 2670 | `			apOuterParked = pGen->apStoreKey;` |
|       437 | 2671 | `			nOuterParked = pGen->nStoreKey;` |
|       437 | 2672 | `			pGen->apStoreKey = apParked;` |
|       437 | 2673 | `			pGen->nStoreKey = nParked;` |
|       216 | 2674 | `		}` |
|   2479765 | 2675 | `		if( iVmOp == PH7_OP_STORE && pNode->pRight` |
|    841077 | 2676 | `		 && (pNode->pRight->xCode == PH7_CompileList` |
|    841043 | 2677 | `		  \|\| pNode->pRight->xCode == PH7_CompileShortList) ){` |
|         - | 2678 | ``			/* A destructuring target list may bind BY REFERENCE (`[&$t] = $src`),`` |
|         - | 2679 | `			 * and php asks at COMPILE time whether the source can hold one --` |
|         - | 2680 | ``			 * `zend_is_variable_or_call`, which takes a variable, a property, a`` |
|         - | 2681 | `			 * static property, a subscript and a CALL, and refuses everything else` |
|         - | 2682 | ``			 * with `Cannot assign reference to non referenceable value`. The list`` |
|         - | 2683 | `			 * body cannot ask: by the time it emits a bind the source is an` |
|         - | 2684 | `			 * anonymous value on the stack. Carry the answer to it. */` |
|       353 | 2685 | `			sxi8 bSavedSrcRef = pGen->bListSrcNotRef;` |
|       353 | 2686 | `			pGen->bListSrcNotRef = (sxi8)!GenStateNodeIsRefSource(pNode->pLeft);` |
|       353 | 2687 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iRhsFlags\|EXPR_FLAG_RDONLY_LOAD);` |
|       353 | 2688 | `			pGen->bListSrcNotRef = bSavedSrcRef;` |
|       179 | 2689 | `		}else{` |
|   2479422 | 2690 | `			rc = GenStateEmitExprCode(&(*pGen),pNode->pRight,iRhsFlags\|EXPR_FLAG_RDONLY_LOAD);` |
|         - | 2691 | `		}` |
|   2479770 | 2692 | `		if( nParked > 0 ){` |
|       437 | 2693 | `			pGen->apStoreKey = apOuterParked;` |
|       437 | 2694 | `			pGen->nStoreKey = nOuterParked;` |
|       216 | 2695 | `		}` |
|   2479770 | 2696 | `		if( !bIsChainOp ){` |
|         - | 2697 | `			/* Non-chain parent: RHS nullsafe chain ends here, before the` |
|         - | 2698 | `			 * operator instruction is emitted. */` |
|   2443388 | 2699 | `			GenStatePatchNullsafeJumps(pGen, nRhsNsBase);` |
|   1219939 | 2700 | `		}` |
|   2479770 | 2701 | `		if( iVmOp == PH7_OP_STORE ){` |
|    841077 | 2702 | `			if( pNode->pRight && (pNode->pRight->xCode == PH7_CompileList \|\|` |
|    841014 | 2703 | `				pNode->pRight->xCode == PH7_CompileShortList) ){` |
|         - | 2704 | `				/* list()/[] destructuring handles assignment internally via LOAD_LIST;` |
|         - | 2705 | `				 * suppress the STORE instruction entirely.  This check uses the node's` |
|         - | 2706 | `				 * compile handler rather than peeking at the last opcode, because nested` |
|         - | 2707 | `				 * list entries emit extra instructions (DUP, LOAD_IDX, POP) after the` |
|         - | 2708 | `				 * outer LOAD_LIST, which would fool an opcode-based check.` |
|         - | 2709 | `				 */` |
|       353 | 2710 | `				iVmOp = 0;` |
|    840903 | 2711 | `			}else if( (pInstr = PH7_VmPeekInstr(pGen->pVm)) != 0 ){` |
|    840729 | 2712 | `				if(pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 2713 | `					/* Perform a member store operation [i.e: $this->x = 50] */` |
|      1989 | 2714 | `					iP2 = 1;` |
|       997 | 2715 | `				}else{` |
|    838745 | 2716 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 2717 | `						/* Transform the STORE instruction to STORE_IDX instruction */` |
|    193029 | 2718 | `						iVmOp = PH7_OP_STORE_IDX;` |
|    193029 | 2719 | `						iP1 = pInstr->iP1;` |
|     96380 | 2720 | `					}else{` |
|    645721 | 2721 | `						p3 = pInstr->p3;` |
|         - | 2722 | `					}` |
|         - | 2723 | `					/* POP the last dynamic load instruction */` |
|    838745 | 2724 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|         - | 2725 | `				}` |
|    419759 | 2726 | `			}` |
|   2058626 | 2727 | `		}else if( iVmOp == PH7_OP_STORE_REF ){` |
|         - | 2728 | `			/* php records at COMPILE time whether the reference SOURCE was written` |
|         - | 2729 | ``			 * as a CALL (ZEND_RETURNS_FUNCTION), so the bind can raise `Only`` |
|         - | 2730 | ``			 * variables should be assigned by reference` when the callee turns out`` |
|         - | 2731 | `` 			 * not to return by reference. It is the direct call only: `$r =& f()` `` |
|         - | 2732 | ``			 * warns where `$r =& f()[0]` and `$r =& f()->p` are silent. The operands`` |
|         - | 2733 | `			 * were swapped in parse.c, so the source is pLeft. */` |
|       432 | 2734 | `			if( pNode->pLeft && pNode->pLeft->pOp` |
|       349 | 2735 | `			 && pNode->pLeft->pOp->iOp == EXPR_OP_FUNC_CALL ){` |
|        48 | 2736 | `				iP1 \|= PH7_STOREREF_CALLSRC;` |
|        22 | 2737 | `			}` |
|         - | 2738 | `			/* Peek first: a member LHS ($o->p =& $x, C::$s =& $x) keeps its` |
|         - | 2739 | `			 * OP_MEMBER in place (it resolves + stashes the target slot at` |
|         - | 2740 | `			 * runtime), unlike the variable/array shapes which fold their load` |
|         - | 2741 | `			 * away. Mirrors the normal member-store fold above (iP2 kept-MEMBER). */` |
|       437 | 2742 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|       437 | 2743 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER ){` |
|         - | 2744 | `				/* Tag the member as a reference target and flag STORE_REF (iP2=1)` |
|         - | 2745 | `				 * to take the member-rebind path in the VM. */` |
|        72 | 2746 | `				pInstr->iP2 = PH7_MEMBER_REF_TARGET;` |
|        72 | 2747 | `				iP2 = 1;` |
|        37 | 2748 | `			}else{` |
|       367 | 2749 | `				pInstr = PH7_VmPopInstr(pGen->pVm);` |
|       367 | 2750 | `				if( pInstr ){` |
|       367 | 2751 | `					if( pInstr->iOp == PH7_OP_LOAD_IDX ){` |
|         - | 2752 | `						/* Array insertion by reference [i.e: $pArray[] =& $some_var; ]` |
|         - | 2753 | `						 * We have to convert the STORE_REF instruction into STORE_IDX_REF` |
|         - | 2754 | `						 */` |
|        97 | 2755 | `						iVmOp = PH7_OP_STORE_IDX_REF;` |
|        97 | 2756 | `						iP1 = pInstr->iP1 \| (iP1 & PH7_STOREREF_CALLSRC);` |
|        97 | 2757 | `						iP2 = pInstr->iP2;` |
|        97 | 2758 | `						p3  = pInstr->p3;` |
|        51 | 2759 | `					}else{` |
|       275 | 2760 | `						p3 = pInstr->p3;` |
|         - | 2761 | `					}` |
|       181 | 2762 | `				}` |
|         - | 2763 | `			}` |
|       216 | 2764 | `		}` |
|   1238111 | 2765 | `	}` |
|   4372687 | 2766 | `	if( iVmOp == PH7_OP_NEW && pNode->pLeft && pNode->pLeft->pOp == 0` |
|     59296 | 2767 | `		&& pNode->pLeft->xCode == PH7_CompileAnnonClass ){` |
|         - | 2768 | ``		/* `new class {…}`: PH7_CompileAnnonClass already emitted the args, the`` |
|         - | 2769 | `		 * class-name constant, and OP_NEW. Suppress this redundant OP_NEW. */` |
|       187 | 2770 | `		iVmOp = 0;` |
|        91 | 2771 | `	}` |
|   4372692 | 2772 | `	if( bMoveLhs ){` |
|         - | 2773 | `		/* The left operand's load, lifted to here so it reads the variable AFTER the` |
|         - | 2774 | `		 * right operand ran -- php's compiled-variable read. Unconditional, and ahead` |
|         - | 2775 | `		 * of every branch below: the instruction was taken OUT of the stream, so there` |
|         - | 2776 | `		 * is no path this may be skipped on. */` |
|     56740 | 2777 | `		GenStateReEmitInstr(&(*pGen),&sMovedLhs);` |
|     56740 | 2778 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWAP,0,0,0,0);` |
|     28325 | 2779 | `	}` |
|   4372692 | 2780 | `	if( iVmOp > 0 ){` |
|   4371594 | 2781 | `		if( iVmOp == PH7_OP_INCR \|\| iVmOp == PH7_OP_DECR ){` |
|     72541 | 2782 | `			if( pNode->iFlags & EXPR_NODE_PRE_INCR ){` |
|         - | 2783 | `				/* Pre-increment/decrement operator [i.e: ++$i,--$j ] */` |
|      8016 | 2784 | `				iP1 = 1;` |
|      4005 | 2785 | `			}` |
|   4335274 | 2786 | `		}else if( iVmOp == PH7_OP_NEW ){` |
|         - | 2787 | `			/* Namespace-qualify the class name for NEW */ {` |
|    115453 | 2788 | `				VmInstr *pPeek = PH7_VmPeekInstr(pGen->pVm);` |
|    115453 | 2789 | `				VmInstr *pCallInstr = 0;` |
|    115453 | 2790 | `				if( pPeek && pPeek->iOp == PH7_OP_CALL ){` |
|    112521 | 2791 | `					VmCallArgMap *pNewMap = (VmCallArgMap *)pPeek->p3;` |
|    112521 | 2792 | `					pCallInstr = pPeek;` |
|         - | 2793 | `` 					/* The class-name push sits one instruction back only when this `new` `` |
|         - | 2794 | `					 * takes no arguments; with an argument list the reorder puts the whole` |
|         - | 2795 | `					 * list (and its screen and rotation) in between, so the call node` |
|         - | 2796 | `					 * recorded where the push is. */` |
|    168858 | 2797 | `					pPeek = (pNewMap && pNewMap->nNewClassInstr > 0)` |
|    112516 | 2798 | `						? PH7_VmGetInstr(pGen->pVm,pNewMap->nNewClassInstr - 1)` |
|     56337 | 2799 | `						: PH7_VmPeekNextInstr(pGen->pVm);` |
|     56179 | 2800 | `				}` |
|    115453 | 2801 | `				if( pPeek && pPeek->iOp == PH7_OP_LOADC ){` |
|    115329 | 2802 | `					int bAbsolute = (pPeek->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|         - | 2803 | `					sxu32 nLitForClass;` |
|    115329 | 2804 | `					VmCallArgMap *pCallNsMap = pCallInstr ? (VmCallArgMap *)pCallInstr->p3 : 0;` |
|         - | 2805 | `					/* If the CALL handler qualified the name with FUNCTION` |
|         - | 2806 | `					 * imports, recover the original literal (recorded in the` |
|         - | 2807 | `					 * arg map — OP_CALL's iP2 is the hasSpread flag, and` |
|         - | 2808 | `` 					 * misreading it as a literal index made `new C(...$args)` `` |
|         - | 2809 | `					 * fatal with "Class ' ' is not defined") and re-qualify` |
|         - | 2810 | `					 * with class imports. */` |
|    115329 | 2811 | `					if( pCallNsMap && pCallNsMap->nOrigNameLit > 0 ){` |
|       105 | 2812 | `						nLitForClass = pCallNsMap->nOrigNameLit - 1;` |
|        55 | 2813 | `					}else{` |
|    115229 | 2814 | `						nLitForClass = (sxu32)pPeek->iP2;` |
|         - | 2815 | `					}` |
|    115329 | 2816 | `					pPeek->iP1 = 0;` |
|    115329 | 2817 | `					if( !bAbsolute ){` |
|         - | 2818 | `						/* self/static/parent are resolved at runtime against the` |
|         - | 2819 | `						 * current class — never namespace-qualify them (else` |
|         - | 2820 | ``						 * `new self` in namespace N becomes "N\self"). Mirrors the`` |
|         - | 2821 | `						 * instanceof (IS_A) guard below. */` |
|    115243 | 2822 | `						ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,nLitForClass);` |
|    115243 | 2823 | `						int isSpecialNew = 0;` |
|    115243 | 2824 | `						if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|    115243 | 2825 | `							const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|    115243 | 2826 | `							sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|    115238 | 2827 | `							if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|    115473 | 2828 | `								(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|     57782 | 2829 | `								(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|        69 | 2830 | `								isSpecialNew = 1;` |
|        33 | 2831 | `							}` |
|     57538 | 2832 | `						}` |
|    115243 | 2833 | `						if( isSpecialNew ){` |
|        69 | 2834 | `							pPeek->iP2 = (sxi32)nLitForClass;` |
|        36 | 2835 | `						}else{` |
|    115177 | 2836 | `							pPeek->iP2 = (sxi32)GenStateNsQualifyName(pGen,nLitForClass,&pGen->hUseImports,0);` |
|         - | 2837 | `						}` |
|     57543 | 2838 | `					}else{` |
|        91 | 2839 | `						pPeek->iP2 = (sxi32)nLitForClass;` |
|         - | 2840 | `					}` |
|     57581 | 2841 | `				}` |
|         - | 2842 | `			}` |
|    115453 | 2843 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|    115453 | 2844 | `			if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|         - | 2845 | `				VmInstr *pPrev;` |
|         - | 2846 | `				int bPrevMember;` |
|    112521 | 2847 | `				pPrev = PH7_VmPeekNextInstr(pGen->pVm);` |
|         - | 2848 | `				/* "Was the callee a MEMBER access?" — which, once the reorder puts a` |
|         - | 2849 | `				 * rotation between the callee and its call, is the question the rotation` |
|         - | 2850 | `				 * already answers (a method callee is the two-slot one). */` |
|    170277 | 2851 | `				bPrevMember = pPrev && (pPrev->iOp == PH7_OP_ROT_CALLEE` |
|    109678 | 2852 | `					? (pPrev->iP2 & PH7_ROT_TWOSLOT) != 0` |
|         - | 2853 | ``					/* …and only a METHOD member is one. A `new`'s class expression may`` |
|         - | 2854 | ``					 * BE a property read (`new $config->defType()`), which leaves an`` |
|         - | 2855 | `					 * OP_MEMBER in PH7_MEMBER_READ mode with the class NAME on the` |
|         - | 2856 | `					 * stack -- the trailing OP_CALL is the constructor's and must be` |
|         - | 2857 | `					 * folded away like any other. Reading the opcode alone kept it, so` |
|         - | 2858 | `					 * the property's VALUE was then called as a function. */` |
|      2838 | 2859 | `					: (pPrev->iOp == PH7_OP_MEMBER && pPrev->iP2 == PH7_MEMBER_METHOD));` |
|    112521 | 2860 | `				if( !bPrevMember ){` |
|         - | 2861 | `					/* Pop the call instruction, preserve named-arg map and` |
|         - | 2862 | `					 * the hasSpread flag (OP_NEW consumes the spread` |
|         - | 2863 | `					 * accumulator exactly like OP_CALL would have). */` |
|    112521 | 2864 | `					iP1 = pInstr->iP1;` |
|    112521 | 2865 | `					iP2 = pInstr->iP2 & PH7_CALL_SPREAD;` |
|    112521 | 2866 | `					if( pInstr->p3 ){` |
|    112521 | 2867 | `						p3 = pInstr->p3; /* Transfer VmCallArgMap to NEW */` |
|     56179 | 2868 | `					}` |
|    112521 | 2869 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|     56179 | 2870 | `				}` |
|     56184 | 2871 | `			}` |
|   4241253 | 2872 | `		}else if( iVmOp == PH7_OP_IS_A ){` |
|         - | 2873 | `			/* instanceof: right operand is a class name, not a constant.` |
|         - | 2874 | `			 * Namespace-qualify it, but skip self/static/parent and absolute refs. */` |
|     16483 | 2875 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     16483 | 2876 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     16475 | 2877 | `				ph7_value *pLitChk = (ph7_value *)SySetAt(&pGen->pVm->aLitObj,(sxu32)pInstr->iP2);` |
|     16475 | 2878 | `				int bAbsolute = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|     16475 | 2879 | `				int isSpecialIs = 0;` |
|     16475 | 2880 | `				if( pLitChk && (pLitChk->iFlags & MEMOBJ_STRING) ){` |
|     16475 | 2881 | `					const char *z = (const char *)SyBlobData(&pLitChk->sBlob);` |
|     16475 | 2882 | `					sxu32 n = (sxu32)SyBlobLength(&pLitChk->sBlob);` |
|     16470 | 2883 | `					if( (n == 4 && SyMemcmp(z,"self",4) == 0) \|\|` |
|     16465 | 2884 | `						(n == 6 && SyMemcmp(z,"static",6) == 0) \|\|` |
|      8220 | 2885 | `						(n == 6 && SyMemcmp(z,"parent",6) == 0) ){` |
|        27 | 2886 | `						isSpecialIs = 1;` |
|        12 | 2887 | `					}` |
|      8224 | 2888 | `				}` |
|     16475 | 2889 | `				pInstr->iP1 = 0;` |
|     16475 | 2890 | `				if( !isSpecialIs && !bAbsolute ){` |
|     16421 | 2891 | `					pInstr->iP2 = (sxi32)GenStateNsQualifyName(pGen,(sxu32)pInstr->iP2,&pGen->hUseImports,0);` |
|      8197 | 2892 | `				}` |
|      8229 | 2893 | `			}` |
|   4175360 | 2894 | `		}else if( iVmOp == PH7_OP_MEMBER){` |
|         - | 2895 | `			/* Prevent constant expansion for member/property names.` |
|         - | 2896 | `			 * The right child (member name) was just compiled — its LOADC` |
|         - | 2897 | `			 * should not trigger constant lookup. */` |
|     36387 | 2898 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|     36387 | 2899 | `			if( pInstr && pInstr->iOp == PH7_OP_LOADC ){` |
|     35483 | 2900 | `				pInstr->iP1 = 0;` |
|     17720 | 2901 | `			}` |
|     36387 | 2902 | `			if( pNode->pOp->iOp == EXPR_OP_DC /* '::' */){` |
|         - | 2903 | `				/* Static member access,remember that */` |
|      4446 | 2904 | `				iP1 = 1;` |
|      4446 | 2905 | `				pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|      4446 | 2906 | `				if( pInstr && pInstr->iOp == PH7_OP_LOAD ){` |
|       687 | 2907 | `					p3 = pInstr->p3;` |
|         - | 2908 | ``					/* A `$`-form (`C::$s`, `C::$$x`, `C::${$e}`) is a STATIC PROPERTY`` |
|         - | 2909 | `					 * access, never a constant. A LITERAL name folds into p3 (non-zero)` |
|         - | 2910 | ``					 * and the exec side reads it there; a DYNAMIC name (`$$x`/`${$e}`)`` |
|         - | 2911 | `					 * leaves p3==0 with the computed name on the stack — the SAME shape` |
|         - | 2912 | ``					 * as a bareword constant `C::C`. Mark iP1=2 so exec still routes it`` |
|         - | 2913 | `					 * to the property table (hAttr), not the constant table (hConst). */` |
|       687 | 2914 | `					if( p3 == 0 ){` |
|        21 | 2915 | `						iP1 = 2;` |
|         9 | 2916 | `					}` |
|       687 | 2917 | `					(void)PH7_VmPopInstr(pGen->pVm);` |
|       341 | 2918 | `				}` |
|      2216 | 2919 | `			}` |
|         - | 2920 | `			/* Attribute access (iP2==0, not a method call which is iP2==1) in unset()/isset()/empty()` |
|         - | 2921 | `			 * context: tag the OP_MEMBER so the VM removes the property (unset) or suppresses the` |
|         - | 2922 | `			 * read-miss "Undefined class attribute" warning (isset/empty) — mirrors the same` |
|         - | 2923 | `			 * EXPR_FLAG_LOAD_IDX_* → LOAD_IDX iP2=5/4/6 mapping used for array subscripts above. */` |
|     36387 | 2924 | `			if( iP2 == PH7_MEMBER_READ ){` |
|     36387 | 2925 | `				if( iFlags & EXPR_FLAG_LOAD_IDX_UNSET ){` |
|       211 | 2926 | `					iP2 = PH7_MEMBER_UNSET;` |
|     36284 | 2927 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_ISSET ){` |
|       315 | 2928 | `					iP2 = PH7_MEMBER_ISSET;` |
|     36025 | 2929 | `				}else if( iFlags & EXPR_FLAG_LOAD_IDX_EMPTY ){` |
|        61 | 2930 | `					iP2 = PH7_MEMBER_EMPTY;` |
|     35840 | 2931 | `				}else if( iFlags & EXPR_FLAG_MEMBER_COALESCE ){` |
|       267 | 2932 | `					iP2 = PH7_MEMBER_COALESCE;` |
|     35680 | 2933 | `				}else if( iFlags & EXPR_FLAG_MEMBER_WRITE ){` |
|         - | 2934 | `					/* Write-lvalue base ($o->arr[$k]=v, $o->p ??= v): auto-create a missing prop. */` |
|      2707 | 2935 | `					iP2 = PH7_MEMBER_WRITE;` |
|     34198 | 2936 | `				}else if( iFlags & EXPR_FLAG_DEFER_ARG ){` |
|         - | 2937 | `					/* D1 commit 2: deferred by-ref/by-value property arg ($o->p). */` |
|      2701 | 2938 | `					iP2 = PH7_MEMBER_DEFPATH;` |
|      1348 | 2939 | `				}` |
|     18172 | 2940 | `			}` |
|     18172 | 2941 | `		}` |
|         - | 2942 | `		/* First-class callable: emit OP_LOAD_FCC to wrap the callee in a Closure instead of` |
|         - | 2943 | `		 * calling it. For a plain function the callee's OP_LOADC left its name on the stack` |
|         - | 2944 | `		 * (iP1=1). For a method/static callee the callee compiled to ... OP_MEMBER, which we` |
|         - | 2945 | `		 * DROP — the OP_MEMBER would dispatch and mangle the method name; popping it leaves` |
|         - | 2946 | `		 * [target, real-method-name] on the stack for OP_LOAD_FCC to bind (iP1=2). */` |
|   4371594 | 2947 | `		if( bFcc ){` |
|       279 | 2948 | `			iVmOp = PH7_OP_LOAD_FCC;` |
|         - | 2949 | `			/* php's global fallback applies to a first-class callable exactly as it does` |
|         - | 2950 | ``			 * to the call it stands for: inside a namespace, `strlen(...)` is the global`` |
|         - | 2951 | `			 * function when the current namespace has none. The callee's literal was` |
|         - | 2952 | `			 * namespace-qualified above and the arg map that records it is dropped here` |
|         - | 2953 | `			 * (an FCC has no arguments), so carry the one bit the resolution needs in the` |
|         - | 2954 | ``			 * instruction itself — without it `strlen(...)` in a namespaced file was`` |
|         - | 2955 | ``			 * `Call to undefined function A\B\strlen()`, a fatal on ordinary php. */`` |
|       279 | 2956 | `			iP2 = (p3 && ((VmCallArgMap *)p3)->bIsNamespaced) ? 1 : 0;` |
|       279 | 2957 | `			p3 = 0;` |
|       279 | 2958 | `			pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|       279 | 2959 | `			if( pInstr && pInstr->iOp == PH7_OP_MEMBER && pInstr->iP2 == PH7_MEMBER_METHOD ){` |
|         - | 2960 | ``				/* A static call with a DYNAMIC method name (`C::$m(...)`) folded that name`` |
|         - | 2961 | `				 * into OP_MEMBER->p3 and left only [class] on the stack (the name's OP_LOAD` |
|         - | 2962 | ``				 * was popped at the static-`::` codegen above). Re-load it so OP_LOAD_FCC`` |
|         - | 2963 | `				 * sees the [target, method-name] pair the iP1=2 handler expects. */` |
|       151 | 2964 | `				void *pMemberName = pInstr->p3;` |
|       151 | 2965 | `				(void)PH7_VmPopInstr(pGen->pVm);` |
|       151 | 2966 | `				if( pMemberName ){` |
|       ! 0 | 2967 | `					PH7_VmEmitInstr(pGen->pVm, PH7_OP_LOAD, 0, 0, pMemberName, 0);` |
|       ! 0 | 2968 | `				}` |
|       151 | 2969 | `				iP1 = 2;` |
|        78 | 2970 | `			}else{` |
|         - | 2971 | `				/* Only a METHOD member is the callee's NAME. A parenthesised PROPERTY read` |
|         - | 2972 | ``				 * (`($o->cb)(...)`, `(C::$cb)(...)`) is php's variable-invocation: the member`` |
|         - | 2973 | `				 * op stays, its VALUE is the callable, and this is the iP1=1 wrap. Dropping it` |
|         - | 2974 | `				 * here read the property NAME as a method name and answered` |
|         - | 2975 | ``				 * `Call to undefined method H::cb()` for a closure the object was holding —`` |
|         - | 2976 | `				 * the CALL codegen above already made the distinction (it leaves the member a` |
|         - | 2977 | `				 * plain read for a parenthesised callee) and this branch undid it. */` |
|       133 | 2978 | `				iP1 = 1;` |
|         - | 2979 | `			}` |
|       137 | 2980 | `		}` |
|         - | 2981 | `		/* Tag CALL/NEW sites with the caller file's strict_types flag.` |
|         - | 2982 | `		 * This is the primary emit path for user-visible calls. */` |
|   4371594 | 2983 | `		if( iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW ){` |
|   1237176 | 2984 | `			p3 = GenStateAttachStrictFlag(pGen,p3);` |
|    617342 | 2985 | `		}` |
|         - | 2986 | `		/* Finally,emit the VM instruction associated with this operator */` |
|   4371594 | 2987 | `		PH7_VmEmitInstr(pGen->pVm,iVmOp,iP1,iP2,p3,0);` |
|   4371589 | 2988 | `		if( iVmOp == PH7_OP_MEMBER && iP2 == PH7_MEMBER_READ` |
|     33269 | 2989 | `		 && (iFlags & EXPR_FLAG_MEMBER_REFSRC) ){` |
|         - | 2990 | `			/* The reference SOURCE keeps its READ mode -- php hands back a copy for a` |
|         - | 2991 | `			 * handler-backed property, and dispatches __get for an overloaded one -- and` |
|         - | 2992 | `			 * carries the write-context marker beside it (see VmInstr::bRefSrc). */` |
|       142 | 2993 | `			VmInstr *pRefSrc = PH7_VmPeekInstr(pGen->pVm);` |
|       142 | 2994 | `			if( pRefSrc ){` |
|       142 | 2995 | `				pRefSrc->bRefSrc = 1;` |
|        70 | 2996 | `			}` |
|        70 | 2997 | `		}` |
|   4371594 | 2998 | `		if( (iVmOp == PH7_OP_CALL \|\| iVmOp == PH7_OP_NEW) && pNode->pStart ){` |
|         - | 2999 | `			/* A call's own line is where it BEGINS, not where its argument list` |
|         - | 3000 | `			 * closes. The emitter stamps every instruction with the token the` |
|         - | 3001 | `			 * generator is standing on, which for a call is the ')' -- so a call` |
|         - | 3002 | `			 * written across several lines went into the backtrace at its LAST one` |
|         - | 3003 | `			 * and php records its first. */` |
|   1237176 | 3004 | `			VmInstr *pCallInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   1237176 | 3005 | `			if( pCallInstr ){` |
|   1237176 | 3006 | `				pCallInstr->nLine = pNode->pStart->nLine;` |
|    617342 | 3007 | `			}` |
|    617342 | 3008 | `		}` |
|   2182306 | 3009 | `	}` |
|   4372692 | 3010 | `	if( nParked > 0 ){` |
|         - | 3011 | `		/* The store consumed the value and the target and left the assignment's own` |
|         - | 3012 | `		 * result on top; the parked names are still under it. Lift the result over each` |
|         - | 3013 | `		 * one and drop it -- php's temporaries are freed at the same point. */` |
|         - | 3014 | `		int nAt;` |
|       877 | 3015 | `		for( nAt = 0 ; nAt < nParked ; ++nAt ){` |
|       445 | 3016 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_SWAP,0,0,0,0);` |
|       445 | 3017 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       225 | 3018 | `		}` |
|       216 | 3019 | `	}` |
|   4372692 | 3020 | `	if( nJmpIdx > 0 ){` |
|         - | 3021 | `		/* Fix short-circuited jumps now the destination is resolved */` |
|    231751 | 3022 | `		pInstr = PH7_VmGetInstr(pGen->pVm,nJmpIdx);` |
|    231751 | 3023 | `		if( pInstr ){` |
|    231751 | 3024 | `			pInstr->iP2 = PH7_VmInstrLength(pGen->pVm);` |
|    115710 | 3025 | `		}` |
|    115710 | 3026 | `	}` |
|   4372692 | 3027 | `	return rc;` |
|   5776126 | 3028 | `}` |
|         - | 3029 | `/*` |
|         - | 3030 | ` * Emit a call's ARGUMENT LIST, and decide everything about it the OP_CALL then carries:` |
|         - | 3031 | ` * the count, the unpack flag, the named-argument / assert-source / argument-shape map.` |
|         - | 3032 | ` *` |
|         - | 3033 | ` * Split out of GenStateEmitExprCode because php resolves a callee BEFORE it evaluates` |
|         - | 3034 | ` * the arguments, so this now runs AFTER the callee sub-tree has been emitted (the one` |
|         - | 3035 | `` * exception is a `new`'s constructor list — see EXPR_FLAG_NEW_CALLEE). It is otherwise`` |
|         - | 3036 | ` * the same code, and reads only the node: nothing here inspects the instructions the` |
|         - | 3037 | ` * callee left behind.` |
|         - | 3038 | ` *` |
|         - | 3039 | ` * pArgs->p3 may arrive non-NULL — the callee's own namespace qualification builds the` |
|         - | 3040 | ` * VmCallArgMap first now — and every allocation site below reuses it.` |
|         - | 3041 | ` */` |
|   1122007 | 3042 | `static sxi32 GenStateEmitCallArgs(` |
|         - | 3043 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 3044 | `	ph7_expr_node *pNode, /* The call node */` |
|         - | 3045 | `	sxi32 iFlags,         /* Control flags of the call site */` |
|         - | 3046 | `	GenCallArgs *pArgs    /* OUT: what the OP_CALL needs */` |
|         - | 3047 | `	)` |
|         5 | 3048 | `{` |
|   1122012 | 3049 | `	void *p3 = pArgs->p3;` |
|   1122012 | 3050 | `	sxi32 iP1 = 0;` |
|   1122012 | 3051 | `	sxu32 iP2 = 0;` |
|   1122012 | 3052 | `	int bFcc = 0;` |
|         - | 3053 | `	sxi32 rc;` |
|         - | 3054 | `	ph7_expr_node **apNode;` |
|   1122012 | 3055 | `	int hasSpread = 0;` |
|   1122012 | 3056 | `	int hasNamed = 0;` |
|   1122012 | 3057 | `	sxu32 byRefMask = 0;` |
|         - | 3058 | `	sxi32 nArgs;` |
|         - | 3059 | `	sxi32 n;` |
|   1122012 | 3060 | `	int bAnySpread = 0;` |
|   1122012 | 3061 | `	sxi32 nLastRunner = 0;` |
|   1122012 | 3062 | `	int bConstruct = 0; /* the callee is a language construct's keyword -- PH7_CALL_CONSTRUCT */` |
|         - | 3063 | `	/* Recurse and generate bytecodes for function arguments */` |
|   1122012 | 3064 | `	apNode = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|   1122012 | 3065 | `	nArgs = (sxi32)SySetUsed(&pNode->aNodeArgs);` |
|         - | 3066 | ``	/* First-class callable `f(...)`: the sole argument is the lone-ellipsis marker.`` |
|         - | 3067 | `	 * Emit no arguments; the callee (pNode->pLeft) is still compiled below, then we` |
|         - | 3068 | `	 * emit OP_LOAD_FCC instead of OP_CALL to wrap it in a Closure. */` |
|   1122012 | 3069 | `	if( nArgs == 1 && apNode[0] && (apNode[0]->iFlags & EXPR_NODE_FCC) ){` |
|       279 | 3070 | `		bFcc = 1;` |
|       279 | 3071 | `		nArgs = 0;` |
|       137 | 3072 | `	}` |
|         - | 3073 | `	/* Validate argument order like php: no positional argument after a` |
|         - | 3074 | ``	 * named one OR after unpacking, and `name: ...$x` is a parse error. */`` |
|         - | 3075 | `	{` |
|   1122012 | 3076 | `		int seenNamed = 0;` |
|   1122012 | 3077 | `		int seenSpread = 0;` |
|   2714409 | 3078 | `		for( n = 0; n < nArgs; ++n ){` |
|   1592406 | 3079 | `			if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|       378 | 3080 | `				bAnySpread = 1;` |
|       378 | 3081 | `				seenSpread = 1;` |
|       378 | 3082 | `				if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       ! 0 | 3083 | `					rc = PH7_GenCompileError(&(*pGen),E_PARSE,apNode[n]->pStart->nLine,` |
|         - | 3084 | `						"syntax error, unexpected token \"...\"");` |
|       ! 0 | 3085 | `					return SXERR_SYNTAX;` |
|         - | 3086 | `				}` |
|       378 | 3087 | `				if( seenNamed ){` |
|         - | 3088 | `					/* The mirror of the positional-after-named rule: php refuses the` |
|         - | 3089 | ``					 * UNPACK too, and at compile time. Without it `f(x: 1, ...$a)` ran`` |
|         - | 3090 | `					 * and reported whatever the runtime binder made of the flattened` |
|         - | 3091 | `					 * list -- a different sentence, raised too late, on a program php` |
|         - | 3092 | `					 * never starts. */` |
|         3 | 3093 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 3094 | `						"Cannot use argument unpacking after named arguments");` |
|         3 | 3095 | `					return SXERR_SYNTAX;` |
|         5 | 3096 | `				}` |
|   1592218 | 3097 | `			}else if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       763 | 3098 | `				seenNamed = 1;` |
|       763 | 3099 | `				hasNamed = 1;` |
|   1591654 | 3100 | `			}else if( seenNamed ){` |
|         3 | 3101 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 3102 | `					"Cannot use positional argument after named argument");` |
|         3 | 3103 | `				return SXERR_SYNTAX;` |
|   1591273 | 3104 | `			}else if( seenSpread ){` |
|       ! 0 | 3105 | `				rc = PH7_GenCompileError(&(*pGen),E_ERROR,apNode[n]->pStart->nLine,` |
|         - | 3106 | `					"Cannot use positional argument after argument unpacking");` |
|       ! 0 | 3107 | `				return SXERR_SYNTAX;` |
|         - | 3108 | `			}` |
|    794483 | 3109 | `		}` |
|         - | 3110 | `	}` |
|         - | 3111 | `	/* Read-only load */` |
|   1122008 | 3112 | `	iFlags \|= EXPR_FLAG_RDONLY_LOAD;` |
|         - | 3113 | `	/* Route subscript-argument LOAD_IDX through a special iP2 code` |
|         - | 3114 | ``	 * for the language constructs `isset` and `empty` so ArrayAccess`` |
|         - | 3115 | `	 * objects dispatch to the right method (offsetExists for both;` |
|         - | 3116 | `	 * empty also needs offsetGet to evaluate emptiness on hits). */` |
|   1122008 | 3117 | `	if( pNode->pLeft && pNode->pLeft->pStart ){` |
|   1122008 | 3118 | `		SyString *pCallName = &pNode->pLeft->pStart->sData;` |
|   1727853 | 3119 | `		int bIsset = pCallName->nByte == 5` |
|   1122003 | 3120 | `			&& SyStrnicmp(pCallName->zString,"isset",5) == 0;` |
|   1727853 | 3121 | `		int bEmpty = pCallName->nByte == 5` |
|   1122003 | 3122 | `			&& SyStrnicmp(pCallName->zString,"empty",5) == 0;` |
|         - | 3123 | ``		/* `isset`, `empty` and `eval` reach the VM as a call to a host function of`` |
|         - | 3124 | `		 * their own name, and php has no such function -- so the SITE has to say that` |
|         - | 3125 | `		 * the engine, not the program, spelled it (PH7_CALL_CONSTRUCT). */` |
|   1122008 | 3126 | `		bConstruct = GenStateCalleeIsConstruct(pNode->pLeft);` |
|         - | 3127 | `		/* isset()/empty() are language CONSTRUCTS, not functions: php parses` |
|         - | 3128 | `		 * their argument list in the grammar and a missing operand is a parse` |
|         - | 3129 | `		 * error on the ')'. They compile through this ordinary call loop, which` |
|         - | 3130 | ``		 * never checked arity, so `empty()` quietly evaluated to true and`` |
|         - | 3131 | ``		 * `isset()` to false. (empty() also takes exactly one operand in php,`` |
|         - | 3132 | `		 * unlike isset(), which is variadic.) */` |
|   1122008 | 3133 | `		if( (bIsset \|\| bEmpty) && nArgs < 1 ){` |
|         - | 3134 | `			/* php names the ')' itself as the unexpected token, so point at the` |
|         - | 3135 | `			 * node's last token rather than pGen->pIn (which has already moved` |
|         - | 3136 | `			 * past the call to the statement's ';'). */` |
|         6 | 3137 | `			SyToken *pTok = pNode->pEnd;` |
|         6 | 3138 | `			if( pTok && pTok > pNode->pStart && (pTok->nType & PH7_TK_RPAREN) == 0 ){` |
|       ! 0 | 3139 | `				pTok--;` |
|       ! 0 | 3140 | `			}` |
|         6 | 3141 | `			PH7_GenSyntaxError(&(*pGen),pTok,0);` |
|         6 | 3142 | `			return SXERR_ABORT;` |
|         - | 3143 | `		}` |
|   1122004 | 3144 | `		if( bIsset ){` |
|     17065 | 3145 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_ISSET;` |
|   1113463 | 3146 | `		}else if( bEmpty ){` |
|       215 | 3147 | `			iFlags \|= EXPR_FLAG_LOAD_IDX_EMPTY;` |
|       105 | 3148 | `		}` |
|         - | 3149 | `		/* Auto-vivify by-reference out-params of known builtins so an` |
|         - | 3150 | `		 * undefined variable argument (e.g. preg_match($p,$s,$m) with` |
|         - | 3151 | `		 * $m never assigned) gets a real memobj slot for the builtin to` |
|         - | 3152 | `		 * write back through. Skipped when spread/named args are present:` |
|         - | 3153 | `		 * the compile-time positional index no longer maps to the` |
|         - | 3154 | `		 * runtime apArg[] slot (and spread elements can't be by-ref). */` |
|   1122004 | 3155 | `		if( !bAnySpread && !hasNamed ){` |
|         - | 3156 | `			SyString sBuiltin;` |
|   1121189 | 3157 | `			GenStateCallBuiltinName(pNode->pLeft, &sBuiltin);` |
|   1121189 | 3158 | `			byRefMask = GenStateByRefBuiltinMask(&sBuiltin);` |
|    559430 | 3159 | `		}` |
|    559837 | 3160 | `	}` |
|         - | 3161 | `	/* The last argument position AFTER which nothing in this list can run code.` |
|         - | 3162 | `	 * Every by-value argument before it is pushed as a view of somebody's bytes and` |
|         - | 3163 | `	 * has to be given its own copy (PH7_OP_SNAPSHOT); at or after it, nothing between` |
|         - | 3164 | `	 * the push and the call can write, so the view is exactly php's answer and costs` |
|         - | 3165 | `	 * nothing. A list whose arguments are all plain variables and literals -- which is` |
|         - | 3166 | `	 * most of them -- leaves this at 0 and emits nothing. */` |
|   1122004 | 3167 | `	nLastRunner = 0;` |
|   2232202 | 3168 | `	for( n = nArgs - 1 ; n >= 0 ; --n ){` |
|   1421735 | 3169 | `		if( GenStateArgRunsCode(apNode[n]) ){` |
|    311537 | 3170 | `			nLastRunner = n;` |
|    311537 | 3171 | `			break;` |
|         - | 3172 | `		}` |
|    554082 | 3173 | `	}` |
|   2714395 | 3174 | `	for( n = 0 ; n < nArgs ; ++n ){` |
|   1592398 | 3175 | `		sxu32 nArgNsBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   1592398 | 3176 | `		sxi32 iArgFlags = iFlags & ~(EXPR_FLAG_LOAD_IDX_STORE\|EXPR_FLAG_MEMBER_WRITE` |
|         - | 3177 | `			\|EXPR_FLAG_MEMBER_REFSRC);` |
|         - | 3178 | `		/* For a by-ref argument position, drop the read-only flag so the` |
|         - | 3179 | `		 * variable is created if absent (PH7_OP_LOAD iP1=0 => bCreate), and` |
|         - | 3180 | `		 * set write-context so a subscript target (preg_match($p,$s,$a['k']))` |
|         - | 3181 | `		 * auto-vivifies its element and exposes a writable memobj slot for the` |
|         - | 3182 | `		 * builtin to write back through. A plain $var target is unaffected` |
|         - | 3183 | `		 * (iP1=0 either way).` |
|         - | 3184 | `		 *` |
|         - | 3185 | `		 * A PROPERTY target is the one shape this eager path cannot express, so it` |
|         - | 3186 | ``		 * is left to the deferred one below: what php's `FETCH_OBJ_W` does to`` |
|         - | 3187 | ``		 * `$o->p` is not what an ASSIGNMENT does to it — a missing property is`` |
|         - | 3188 | ``		 * CREATED, an overloaded one takes `Indirect modification of overloaded`` |
|         - | 3189 | ``		 * property` and is passed by VALUE (rather than reaching `__set`), and a`` |
|         - | 3190 | ``		 * non-object base is the catchable `Attempt to modify property`. The`` |
|         - | 3191 | `		 * deferred resolver already encodes all of that (VmBindPropByRef) and` |
|         - | 3192 | `		 * already reads a host function's by-ref mask, so routing the property` |
|         - | 3193 | ``		 * shapes through it is what makes `preg_match($p, $s, $this->matches)` —`` |
|         - | 3194 | `		 * the ordinary spelling — write anything at all. */` |
|   1592393 | 3195 | `		if( n < 31 && (byRefMask & (1u<<n))` |
|    799404 | 3196 | `		 && !GenStateArgHasPropertyStep(apNode[n]) ){` |
|      9748 | 3197 | `			iArgFlags &= ~EXPR_FLAG_RDONLY_LOAD;` |
|      9748 | 3198 | `			iArgFlags \|= EXPR_FLAG_LOAD_IDX_STORE;` |
|      4859 | 3199 | `		}` |
|         - | 3200 | ``		/* D1: a plain `$var` argument may bind to a by-ref parameter whose signature`` |
|         - | 3201 | `		 * is unknown at compile time (forward reference, dynamic call, or method` |
|         - | 3202 | ``		 * dispatch — e.g. PHPUnit's `willReturnReference($undef)`). We used to clear`` |
|         - | 3203 | `		 * the read-only flag here so an undefined variable vivified a real slot the` |
|         - | 3204 | `		 * by-ref write-back could reach — but that also invented the variable as NULL` |
|         - | 3205 | `		 * in the caller when the parameter turned out by-VALUE, and suppressed php's` |
|         - | 3206 | ``		 * `Undefined variable $x` warning. Instead mark it DEFERRED: OP_LOAD leaves an`` |
|         - | 3207 | `		 * undefined variable uncreated and carries a lazy-lvalue marker, and OP_CALL` |
|         - | 3208 | `		 * materializes it ONLY for a by-ref parameter once the callee is resolved` |
|         - | 3209 | `		 * (VmResolveDeferredArgs). Excludes isset()/empty()/unset(), which compile` |
|         - | 3210 | `		 * through this same call loop but must NEVER create their operand, and` |
|         - | 3211 | `		 * spread args (whose elements have no positional index of their own). A NAMED` |
|         - | 3212 | `		 * arg defers too: it binds to the formal its NAME picks, which the resolver` |
|         - | 3213 | `		 * looks up through the call's own argument map — excluding it left` |
|         - | 3214 | ``		 * `r(x: $a['new'])` warning `Undefined array key` and passing NULL where php`` |
|         - | 3215 | ``		 * creates the element for the by-ref parameter `$x`.`` |
|         - | 3216 | `		 *` |
|         - | 3217 | `		 * D1 commit 2: the same reasoning extends to an array-element ($a["k"]) or` |
|         - | 3218 | `		 * property ($o->p) argument — a by-ref user-function parameter must vivify the` |
|         - | 3219 | `		 * element/property, a by-value one must warn and NOT vivify. Those nodes carry a` |
|         - | 3220 | `		 * subscript/arrow operator (pOp != 0). The DEFER flag rides down to the base LOAD` |
|         - | 3221 | `		 * (undefined base auto-defers via commit 1) and to the LOAD_IDX/MEMBER, which` |
|         - | 3222 | ``		 * record the lvalue path on a lookup miss. Static `::` and nullsafe `?->` stay`` |
|         - | 3223 | `		 * eager. */` |
|   1592393 | 3224 | `		if( (iFlags & (EXPR_FLAG_LOAD_IDX_ISSET\|EXPR_FLAG_LOAD_IDX_EMPTY\|EXPR_FLAG_LOAD_IDX_UNSET` |
|    794476 | 3225 | `		               \|EXPR_FLAG_MEMBER_COALESCE)) == 0` |
|   1583737 | 3226 | `		 && (iArgFlags & EXPR_FLAG_RDONLY_LOAD) /* not a known builtin by-ref slot (kept eager above) */` |
|   1570219 | 3227 | `		 && (apNode[n]->iFlags & EXPR_NODE_SPREAD) == 0` |
|   1693184 | 3228 | `		 && ( (apNode[n]->pOp == 0 && apNode[n]->xCode == PH7_CompileVariable)` |
|   1173767 | 3229 | `		   \|\| (apNode[n]->pOp != 0 && (apNode[n]->pOp->iOp == EXPR_OP_SUBSCRIPT` |
|    269437 | 3230 | `		                            \|\| apNode[n]->pOp->iOp == EXPR_OP_ARROW)) ) ){` |
|    809573 | 3231 | `			iArgFlags \|= EXPR_FLAG_DEFER_ARG;` |
|    404161 | 3232 | `		}` |
|   1592398 | 3233 | `		rc = GenStateEmitExprCode(&(*pGen),apNode[n],iArgFlags);` |
|   1592398 | 3234 | `		if( rc != SXRET_OK ){` |
|         3 | 3235 | `			return rc;` |
|         - | 3236 | `		}` |
|         - | 3237 | `		/* Each argument is an independent nullsafe scope. */` |
|   1592396 | 3238 | `		GenStatePatchNullsafeJumps(pGen, nArgNsBase);` |
|   1592396 | 3239 | `		if( n < nLastRunner && (apNode[n]->iFlags & EXPR_NODE_SPREAD) == 0 ){` |
|         - | 3240 | `			/* Something later in this list can write to whatever this argument was` |
|         - | 3241 | `			 * loaded from, so take the bytes now. A spread argument is an array,` |
|         - | 3242 | `			 * which is reference-counted rather than aliased, and is skipped. */` |
|    170630 | 3243 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_SNAPSHOT,0,0,0,0);` |
|     85025 | 3244 | `		}` |
|   1592396 | 3245 | `		if( apNode[n]->iFlags & EXPR_NODE_SPREAD ){` |
|         - | 3246 | `			/* Emit spread opcode to unpack this array argument. iP1 marks a` |
|         - | 3247 | ``			 * source php will unpack BY REFERENCE: only a plain `$var` (php`` |
|         - | 3248 | ``			 * fetches every other shape — `$a[0]`, `$o->p`, `C::$s`, a cast, a`` |
|         - | 3249 | `			 * call — as an R-value, so a by-ref parameter binds its elements in` |
|         - | 3250 | `			 * a temporary and the write-back is invisible). The expander needs` |
|         - | 3251 | `			 * the distinction because it carries each element's slot for the` |
|         - | 3252 | ``			 * by-ref binder; without it `r(...$a[0])` wrote through to the real`` |
|         - | 3253 | `			 * element, which php leaves alone. */` |
|       513 | 3254 | `			PH7_VmEmitInstr(pGen->pVm, PH7_OP_SPREAD,` |
|       371 | 3255 | `				(apNode[n]->pOp == 0 && apNode[n]->xCode == PH7_CompileVariable) ? 1 : 0,` |
|         - | 3256 | `				0, 0, 0);` |
|       376 | 3257 | `			hasSpread = 1;` |
|       185 | 3258 | `		}` |
|    794480 | 3259 | `	}` |
|         - | 3260 | `	/* Total number of given arguments */` |
|   1122002 | 3261 | `	iP1 = nArgs;` |
|   1122002 | 3262 | `	iP2 = (hasSpread ? PH7_CALL_SPREAD : 0) \| (bConstruct ? PH7_CALL_CONSTRUCT : 0);` |
|         - | 3263 | `	/* Build VmCallArgMap if named arguments are present.` |
|         - | 3264 | `	 * Deep-copy name strings so they survive token stream cleanup. */` |
|   1122002 | 3265 | `	if( hasNamed ){` |
|       499 | 3266 | `		sxu32 nStrBytes = 0;` |
|         - | 3267 | `		char *zBuf;` |
|      1435 | 3268 | `		for( n = 0; n < nArgs; ++n ){` |
|       941 | 3269 | `			if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       759 | 3270 | `				nStrBytes += (sxu32)apNode[n]->sArgName.nByte;` |
|       377 | 3271 | `			}` |
|       473 | 3272 | `		}` |
|         - | 3273 | `		{` |
|       499 | 3274 | `		sxu32 mapSize = sizeof(VmCallArgMap) + nArgs * sizeof(SyString) + nStrBytes;` |
|       499 | 3275 | `		VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|       494 | 3276 | `			&pGen->pVm->sAllocator, mapSize);` |
|       499 | 3277 | `		if( pMap ){` |
|       499 | 3278 | `			SyZero(pMap, mapSize);` |
|         - | 3279 | `			/* The names need their own contiguous allocation, so this map REPLACES` |
|         - | 3280 | `			 * whatever the callee's namespace qualification built -- and it has to` |
|         - | 3281 | `			 * carry that map's findings across. Dropping them lost the ORIGINAL` |
|         - | 3282 | ``			 * name literal, which is the only thing the `new` codegen can`` |
|         - | 3283 | ``			 * re-qualify with CLASS imports: `new Imported(x: 1)` then resolved`` |
|         - | 3284 | `			 * against the current namespace and the class was not found, while` |
|         - | 3285 | ``			 * the same `new` with positional arguments worked. */`` |
|       499 | 3286 | `			if( p3 ){` |
|        25 | 3287 | `				VmCallArgMap *pPrior = (VmCallArgMap *)p3;` |
|        25 | 3288 | `				pMap->nOrigNameLit = pPrior->nOrigNameLit;` |
|        25 | 3289 | `				pMap->bIsNamespaced = pPrior->bIsNamespaced;` |
|        25 | 3290 | `				pMap->nNewClassInstr = pPrior->nNewClassInstr;` |
|        25 | 3291 | `				pMap->bStrict = pPrior->bStrict;` |
|         - | 3292 | `				/* Nothing else holds it: it is attached to no instruction yet. */` |
|        25 | 3293 | `				SyMemBackendFree(&pGen->pVm->sAllocator,pPrior);` |
|        12 | 3294 | `			}` |
|       499 | 3295 | `			pMap->bHasNamed = 1;` |
|       499 | 3296 | `			pMap->nTotal = (sxu32)nArgs;` |
|       499 | 3297 | `			pMap->aNames = (SyString *)&pMap[1];` |
|       499 | 3298 | `			zBuf = (char *)&pMap->aNames[nArgs]; /* string storage after SyString array */` |
|      1435 | 3299 | `			for( n = 0; n < nArgs; ++n ){` |
|       941 | 3300 | `				if( apNode[n]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|       759 | 3301 | `					sxu32 nb = (sxu32)apNode[n]->sArgName.nByte;` |
|       759 | 3302 | `					SyMemcpy(apNode[n]->sArgName.zString, zBuf, nb);` |
|       759 | 3303 | `					SyStringInitFromBuf(&pMap->aNames[n], zBuf, nb);` |
|       759 | 3304 | `					zBuf += nb;` |
|       377 | 3305 | `				}` |
|         - | 3306 | `				/* else: aNames[n] remains {NULL, 0} for positional */` |
|       473 | 3307 | `			}` |
|       499 | 3308 | `			p3 = (void *)pMap;` |
|       247 | 3309 | `		}` |
|         - | 3310 | `		}` |
|       247 | 3311 | `	}` |
|         - | 3312 | `	/* assert(): php's compiler keeps a copy of the assertion's AST and a` |
|         - | 3313 | ``	 * failing assert reports its rendered SOURCE (`assert(1 == 2)`), not the`` |
|         - | 3314 | `	 * evaluated value. Render the first argument's token span` |
|         - | 3315 | `	 * into the call map so vm_builtin_assert can echo it. Only a DIRECT` |
|         - | 3316 | `	 * unqualified/absolute call qualifies — matching php, an indirect call` |
|         - | 3317 | `	 * (call_user_func, a callable variable) has no source text and its` |
|         - | 3318 | `	 * AssertionError carries an empty message. A spread first argument is` |
|         - | 3319 | `	 * skipped (its span is the unpacked array, not the assertion). */` |
|   1121997 | 3320 | `	if( nArgs >= 1 && !bFcc` |
|   1080652 | 3321 | `	 && (apNode[0]->iFlags & EXPR_NODE_SPREAD) == 0 ){` |
|         - | 3322 | `		SyString sCallee;` |
|   1080354 | 3323 | `		GenStateCallBuiltinName(pNode->pLeft,&sCallee);` |
|   1080349 | 3324 | `		if( sCallee.nByte == sizeof("assert")-1` |
|    711216 | 3325 | `		 && SyStrnicmp(sCallee.zString,"assert",sizeof("assert")-1) == 0 ){` |
|         - | 3326 | `			/* An operator root's pStart/pEnd name only the operator token` |
|         - | 3327 | ``			 * (`1 == 2` roots at `==`); the subtree walk recovers the whole`` |
|         - | 3328 | `			 * raw extent, re-adding parens the grouping pass consumed. */` |
|        67 | 3329 | `			SyToken *pSpanIn = 0;` |
|        67 | 3330 | `			SyToken *pSpanEnd = 0;` |
|         - | 3331 | `			SyBlob sSrc;` |
|        67 | 3332 | `			PH7_ExprSubtreeSpan(apNode[0],&pSpanIn,&pSpanEnd);` |
|        67 | 3333 | `			SyBlobInit(&sSrc,&pGen->pVm->sAllocator);` |
|        67 | 3334 | `			if( pSpanIn && pSpanEnd && pSpanIn < pSpanEnd ){` |
|        67 | 3335 | `				if( apNode[0]->iFlags & EXPR_NODE_NAMED_ARG ){` |
|         - | 3336 | ``					/* php renders the name too: `assert(assertion: 1 == 2)`. */`` |
|         3 | 3337 | `					SyBlobAppend(&sSrc,apNode[0]->sArgName.zString,apNode[0]->sArgName.nByte);` |
|         3 | 3338 | `					SyBlobAppend(&sSrc,": ",2);` |
|         1 | 3339 | `				}` |
|        67 | 3340 | `				PH7_GenRenderAssertSpan(pGen,pSpanIn,pSpanEnd,&sSrc);` |
|        31 | 3341 | `			}` |
|        67 | 3342 | `			if( SyBlobLength(&sSrc) > 0 ){` |
|        98 | 3343 | `				char *zDup = (char *)SyMemBackendDup(&pGen->pVm->sAllocator,` |
|        62 | 3344 | `					SyBlobData(&sSrc),SyBlobLength(&sSrc));` |
|        67 | 3345 | `				if( zDup ){` |
|        67 | 3346 | `					if( p3 == 0 ){` |
|        65 | 3347 | `						VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|        60 | 3348 | `							&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|        65 | 3349 | `						if( pMap ){` |
|        65 | 3350 | `							SyZero(pMap,sizeof(VmCallArgMap));` |
|        65 | 3351 | `							p3 = (void *)pMap;` |
|        30 | 3352 | `						}` |
|        30 | 3353 | `					}` |
|        67 | 3354 | `					if( p3 ){` |
|        67 | 3355 | `						SyStringInitFromBuf(&((VmCallArgMap *)p3)->sAssertSrc,` |
|         - | 3356 | `							zDup,SyBlobLength(&sSrc));` |
|        31 | 3357 | `					}` |
|        31 | 3358 | `				}` |
|        31 | 3359 | `			}` |
|        67 | 3360 | `			SyBlobRelease(&sSrc);` |
|        31 | 3361 | `		}` |
|    539091 | 3362 | `	}` |
|         - | 3363 | `	/* Record each argument's compile-time SHAPE so the by-ref binders can` |
|         - | 3364 | `	 * refuse a non-variable where php refuses it — at the CALL, before the` |
|         - | 3365 | `	 * callee's ZPP runs. Skipped when the call SPREADS (one compile-time` |
|         - | 3366 | `	 * argument becomes N runtime slots, so the positions no longer line up)` |
|         - | 3367 | `	 * or when it carries more arguments than the masks can hold; a call` |
|         - | 3368 | `	 * without the flag keeps the old runtime nIdx test. Named arguments are` |
|         - | 3369 | `	 * fine: they change which FORMAL a slot binds to, not the slot's index. */` |
|   1122002 | 3370 | `	if( !bAnySpread && nArgs > 0 && nArgs <= 31 && !bFcc ){` |
|   1080297 | 3371 | `		sxu32 nNonLval = 0;` |
|   1080297 | 3372 | `		sxu32 nTempCall = 0;` |
|   2672210 | 3373 | `		for( n = 0 ; n < nArgs ; ++n ){` |
|   1591918 | 3374 | `			int iShape = GenStateArgShape(apNode[n]);` |
|   1591918 | 3375 | `			if( iShape == GEN_ARG_NONE ){` |
|    668673 | 3376 | `				nNonLval \|= (1u << n);` |
|   1256617 | 3377 | `			}else if( iShape == GEN_ARG_TEMPCALL ){` |
|     86591 | 3378 | `				nTempCall \|= (1u << n);` |
|     43187 | 3379 | `			}` |
|    794242 | 3380 | `		}` |
|   1080297 | 3381 | `		if( p3 == 0 ){` |
|   1079629 | 3382 | `			VmCallArgMap *pMap = (VmCallArgMap *)SyMemBackendAlloc(` |
|   1079624 | 3383 | `				&pGen->pVm->sAllocator,sizeof(VmCallArgMap));` |
|   1079629 | 3384 | `			if( pMap ){` |
|   1079629 | 3385 | `				SyZero(pMap,sizeof(VmCallArgMap));` |
|   1079629 | 3386 | `				p3 = (void *)pMap;` |
|    538729 | 3387 | `			}` |
|    538729 | 3388 | `		}` |
|   1080297 | 3389 | `		if( p3 ){` |
|   1080297 | 3390 | `			((VmCallArgMap *)p3)->bArgShapes = 1;` |
|   1080297 | 3391 | `			((VmCallArgMap *)p3)->nNonLvalMask = nNonLval;` |
|   1080297 | 3392 | `			((VmCallArgMap *)p3)->nTempCallMask = nTempCall;` |
|    539063 | 3393 | `		}` |
|    539063 | 3394 | `	}` |
|   1122002 | 3395 | `	pArgs->iP1 = iP1;` |
|   1122002 | 3396 | `	pArgs->iP2 = iP2;` |
|   1122002 | 3397 | `	pArgs->p3  = p3;` |
|   1122002 | 3398 | `	pArgs->bFcc = bFcc;` |
|   1122002 | 3399 | `	pArgs->bAnySpread = bAnySpread;` |
|   1122002 | 3400 | `	return SXRET_OK;` |
|    559846 | 3401 | `}` |
|         - | 3402 | `/*` |
|         - | 3403 | ` * Compile a PHP expression.` |
|         - | 3404 | ` * According to the PHP language reference manual:` |
|         - | 3405 | ` *  Expressions are the most important building stones of PHP.` |
|         - | 3406 | ` *  In PHP, almost anything you write is an expression.` |
|         - | 3407 | ` *  The simplest yet most accurate way to define an expression` |
|         - | 3408 | ` *  is "anything that has a value".` |
|         - | 3409 | ` * If something goes wrong while compiling the expression,this` |
|         - | 3410 | ` * function takes care of generating the appropriate error` |
|         - | 3411 | ` * message.` |
|         - | 3412 | ` */` |
|         - | 3413 | `/*` |
|         - | 3414 | ` * Does this expression tree contain a comma OPERATOR node?` |
|         - | 3415 | ` *` |
|         - | 3416 | `` * PH7 shipped `,` as a lowest-precedence binary operator (IMP-0139-COMMA), so`` |
|         - | 3417 | `` * `(1, 2)` and `$x = (f(), $y)` compile and evaluate to the right operand.`` |
|         - | 3418 | ` * php 8 has no comma operator: its grammar only allows comma-separated` |
|         - | 3419 | ` * expression LISTS inside for(...) clauses (call arguments, array literals and` |
|         - | 3420 | ` * list() are split by the parser, never by this node). Accepting it changes the` |
|         - | 3421 | ` * meaning of source php rejects, which the scope policy classes as a bug — so every context` |
|         - | 3422 | ` * except for() now reports php's parse error.` |
|         - | 3423 | ` */` |
|  37713319 | 3424 | `static int GenStateTreeHasComma(ph7_expr_node *pNode)` |
|         5 | 3425 | `{` |
|         - | 3426 | `	ph7_expr_node **apArg;` |
|         - | 3427 | `	sxu32 n;` |
|  37713324 | 3428 | `	if( pNode == 0 ){` |
|  26614614 | 3429 | `		return 0;` |
|         - | 3430 | `	}` |
|  11098715 | 3431 | `	if( pNode->pOp && pNode->pOp->iOp == EXPR_OP_COMMA ){` |
|         6 | 3432 | `		return 1;` |
|         - | 3433 | `	}` |
|  11098706 | 3434 | `	if( GenStateTreeHasComma(pNode->pLeft) \|\| GenStateTreeHasComma(pNode->pRight)` |
|  11098707 | 3435 | `	 \|\| GenStateTreeHasComma(pNode->pCond) ){` |
|         6 | 3436 | `		return 1;` |
|         - | 3437 | `	}` |
|  11098707 | 3438 | `	apArg = (ph7_expr_node **)SySetBasePtr(&pNode->aNodeArgs);` |
|  12938758 | 3439 | `	for( n = 0 ; n < SySetUsed(&pNode->aNodeArgs) ; n++ ){` |
|   1840056 | 3440 | `		if( GenStateTreeHasComma(apArg[n]) ){` |
|       ! 0 | 3441 | `			return 1;` |
|         - | 3442 | `		}` |
|    918136 | 3443 | `	}` |
|  11098707 | 3444 | `	return 0;` |
|  18824846 | 3445 | `}` |
|   2721991 | 3446 | `PH7_PRIVATE sxi32 PH7_CompileExpr(` |
|         - | 3447 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 3448 | `	sxi32 iFlags,        /* Control flags */` |
|         - | 3449 | `	sxi32 (*xTreeValidator)(ph7_gen_state *,ph7_expr_node *) /* Node validator callback.NULL otherwise */` |
|         - | 3450 | `	)` |
|         5 | 3451 | `{` |
|         - | 3452 | `	ph7_expr_node *pRoot;` |
|         - | 3453 | `	SySet sExprNode;` |
|         - | 3454 | `	SyToken *pEnd;` |
|         - | 3455 | `	sxi32 nExpr;` |
|         - | 3456 | `	sxi32 iNest;` |
|         - | 3457 | `	sxi32 rc;` |
|         - | 3458 | `	sxu32 nNullsafeBase;` |
|         - | 3459 | `	/* Initialize worker variables */` |
|   2721996 | 3460 | `	nExpr = 0;` |
|   2721996 | 3461 | `	pRoot = 0;` |
|         - | 3462 | `	/* Any nullsafe jumps still pending belong to an outer scope; isolate` |
|         - | 3463 | ``	 * this expression so its `?->` short-circuits don't leak out. */`` |
|   2721996 | 3464 | `	nNullsafeBase = SySetUsed(&pGen->aNullsafeJmp);` |
|   2721996 | 3465 | `	SySetInit(&sExprNode,&pGen->pVm->sAllocator,sizeof(ph7_expr_node *));` |
|   2721996 | 3466 | `	SySetAlloc(&sExprNode,0x10);` |
|   2721996 | 3467 | `	rc = SXRET_OK;` |
|         - | 3468 | `	/* Delimit the expression */` |
|   2721996 | 3469 | `	pEnd = pGen->pIn;` |
|   2721996 | 3470 | `	iNest = 0;` |
|  21228297 | 3471 | `	while( pEnd < pGen->pEnd ){` |
|  20050615 | 3472 | `		if( pEnd->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 3473 | `			/* Ticket 1433-30: Annonymous/Closure functions body */` |
|      9214 | 3474 | `			iNest++;` |
|  20045992 | 3475 | `		}else if(pEnd->nType & PH7_TK_CCB /* '}' */ ){` |
|      9226 | 3476 | `			iNest--;` |
|  20036777 | 3477 | `		}else if( pEnd->nType & PH7_TK_SEMI /* ';' */ ){` |
|   1560670 | 3478 | `			if( iNest <= 0 ){` |
|   1544314 | 3479 | `				break;` |
|         - | 3480 | `			}` |
|      8137 | 3481 | `		}` |
|  18506306 | 3482 | `		pEnd++;` |
|         5 | 3483 | `	}` |
|   2721996 | 3484 | `	if( iFlags & EXPR_FLAG_COMMA_STATEMENT ){` |
|      3231 | 3485 | `		SyToken *pEnd2 = pGen->pIn;` |
|      3231 | 3486 | `		iNest = 0;` |
|         - | 3487 | `		/* Stop at the first comma */` |
|     21423 | 3488 | `		while( pEnd2 < pEnd ){` |
|     18219 | 3489 | `			if( pEnd2->nType & (PH7_TK_OCB/*'{'*/\|PH7_TK_OSB/*'['*/\|PH7_TK_LPAREN/*'('*/) ){` |
|       511 | 3490 | `				iNest++;` |
|     17966 | 3491 | `			}else if(pEnd2->nType & (PH7_TK_CCB/*'}'*/\|PH7_TK_CSB/*']'*/\|PH7_TK_RPAREN/*')'*/)){` |
|       511 | 3492 | `				iNest--;` |
|     17460 | 3493 | `			}else if( pEnd2->nType & PH7_TK_COMMA /*','*/ ){` |
|      6269 | 3494 | `				if( iNest <= 0 ){` |
|        25 | 3495 | `					break;` |
|         - | 3496 | `				}` |
|      3121 | 3497 | `			}` |
|     18197 | 3498 | `			pEnd2++;` |
|         5 | 3499 | `		}` |
|      3231 | 3500 | `		if( pEnd2 <pEnd ){` |
|        25 | 3501 | `			pEnd = pEnd2;` |
|        11 | 3502 | `		}` |
|      1613 | 3503 | `	}` |
|   2721996 | 3504 | `	if( pEnd > pGen->pIn ){` |
|   2721956 | 3505 | `		SyToken *pTmp = pGen->pEnd;` |
|         - | 3506 | `		/* Swap delimiter */` |
|   2721956 | 3507 | `		pGen->pEnd = pEnd;` |
|         - | 3508 | `		/* Try to get an expression tree */` |
|   2721956 | 3509 | `		rc = PH7_ExprMakeTree(&(*pGen),&sExprNode,&pRoot);` |
|   2721951 | 3510 | `		if( rc == SXRET_OK && pRoot && pGen->nCommaExprOk < 1` |
|   2649264 | 3511 | `		 && GenStateTreeHasComma(pRoot) ){` |
|         - | 3512 | `			/* php has no comma operator outside a for() clause */` |
|         6 | 3513 | `			rc = PH7_GenCompileError(&(*pGen),E_PARSE,pRoot->pStart->nLine,` |
|         - | 3514 | `				"syntax error, unexpected token \",\"");` |
|         6 | 3515 | `			pGen->pEnd = pTmp;` |
|         - | 3516 | `			/* This refusal leaves by its own door, so it owes the release the` |
|         - | 3517 | `			 * ordinary path makes below -- the set owns every node the` |
|         - | 3518 | `			 * expression produced, and a bare SySetRelease drops the pointers` |
|         - | 3519 | `			 * without freeing what they point at. */` |
|         6 | 3520 | `			PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|         6 | 3521 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 3522 | `				SySetRelease(&sExprNode);` |
|       ! 0 | 3523 | `				return SXERR_ABORT;` |
|         - | 3524 | `			}` |
|         6 | 3525 | `			pGen->pIn = pEnd;` |
|         6 | 3526 | `			SySetRelease(&sExprNode);` |
|         6 | 3527 | `			SySetTruncate(&pGen->aNullsafeJmp,nNullsafeBase);` |
|         6 | 3528 | `			return SXRET_OK;` |
|         - | 3529 | `		}` |
|   2721952 | 3530 | `		if( rc == SXRET_OK && pRoot ){` |
|   2721568 | 3531 | `			rc = SXRET_OK;` |
|   2721568 | 3532 | `			if( xTreeValidator ){` |
|         - | 3533 | `				/* Call the upper layer validator callback */` |
|    214001 | 3534 | `				rc = xTreeValidator(&(*pGen),pRoot);` |
|    106839 | 3535 | `			}` |
|   2721568 | 3536 | `			if( rc != SXERR_ABORT ){` |
|         - | 3537 | `				/* Generate code for the given tree */` |
|   2721568 | 3538 | `				rc = GenStateEmitExprCode(&(*pGen),pRoot,iFlags);` |
|         - | 3539 | `				/* Patch any unresolved nullsafe jumps emitted by this` |
|         - | 3540 | `				 * expression so they short-circuit to its end. */` |
|   2721568 | 3541 | `				GenStatePatchNullsafeJumps(pGen, nNullsafeBase);` |
|   1358468 | 3542 | `			}` |
|   2721568 | 3543 | `			nExpr = 1;` |
|   1358468 | 3544 | `		}` |
|         - | 3545 | `		/* Release the whole tree */` |
|   2721952 | 3546 | `		PH7_ExprFreeTree(&(*pGen),&sExprNode);` |
|         - | 3547 | `		/* Synchronize token stream */` |
|   2721952 | 3548 | `		pGen->pEnd = pTmp;` |
|   2721952 | 3549 | `		pGen->pIn  = pEnd;` |
|   2721952 | 3550 | `		if( rc == SXERR_ABORT ){` |
|        59 | 3551 | `			SySetRelease(&sExprNode);` |
|        59 | 3552 | `			return SXERR_ABORT;` |
|         - | 3553 | `		}` |
|   1358633 | 3554 | `	}` |
|   2721938 | 3555 | `	SySetRelease(&sExprNode);` |
|   2721938 | 3556 | `	return nExpr > 0 ? SXRET_OK : SXERR_EMPTY;` |
|   1358687 | 3557 | `}` |
|         - | 3558 | `/*` |
|         - | 3559 | ` * Return a pointer to the node construct handler associated` |
|         - | 3560 | ` * with a given node type [i.e: string,integer,float,...].` |
|         - | 3561 | ` */` |
|   1754184 | 3562 | `PH7_PRIVATE ProcNodeConstruct PH7_GetNodeHandler(sxu32 nNodeType)` |
|         5 | 3563 | `{` |
|   1754189 | 3564 | `	if( nNodeType & PH7_TK_NUM ){` |
|         - | 3565 | `		/* Numeric literal: Either real or integer */` |
|    943070 | 3566 | `		return PH7_CompileNumLiteral;` |
|    811124 | 3567 | `	}else if( nNodeType & PH7_TK_DSTR ){` |
|         - | 3568 | `		/* Double quoted string */` |
|     82367 | 3569 | `		return PH7_CompileString;` |
|    728762 | 3570 | `	}else if( nNodeType & PH7_TK_SSTR ){` |
|         - | 3571 | `		/* Single quoted string */` |
|    728580 | 3572 | `		return PH7_CompileSimpleString;` |
|       187 | 3573 | `	}else if( nNodeType & PH7_TK_HEREDOC ){` |
|         - | 3574 | `		/* Heredoc */` |
|        90 | 3575 | `		return PH7_CompileHereDoc;` |
|       101 | 3576 | `	}else if( nNodeType & PH7_TK_NOWDOC ){` |
|         - | 3577 | `		/* Nowdoc */` |
|        66 | 3578 | `		return PH7_CompileNowDoc;` |
|        38 | 3579 | `	}else if( nNodeType & PH7_TK_BSTR ){` |
|         - | 3580 | `		/* Backtick quoted string */` |
|         3 | 3581 | `		return PH7_CompileBacktic;` |
|         - | 3582 | `	}` |
|        35 | 3583 | `	return 0;` |
|    875520 | 3584 | `}` |
|         - | 3585 | `/*` |
|         - | 3586 | ` * Tree validator for unset() arguments — php's write-target rules, then its` |
|         - | 3587 | ``  * "Can't use nullsafe operator in write context", then the grammar: `unset()` `` |
|         - | 3588 | `` * takes a `variable`, so `unset(GK)`, `unset("s")` and `unset(A::K)` are php`` |
|         - | 3589 | ` * PARSE errors where PHL let them reach the VM and answer with a PH7-ism.` |
|         - | 3590 | ` */` |
|       466 | 3591 | `static sxi32 GenStateUnsetValidator(ph7_gen_state *pGen, ph7_expr_node *pNode)` |
|         5 | 3592 | `{` |
|         - | 3593 | `	sxi32 rc;` |
|       471 | 3594 | `	rc = GenStateWriteTargetCheck(&(*pGen),pNode,PH7_WTC_UNSET);` |
|       471 | 3595 | `	if( rc != SXRET_OK ){` |
|         5 | 3596 | `		return rc;` |
|         - | 3597 | `	}` |
|       467 | 3598 | `	if( PH7_ExprContainsNullsafe(pNode) ){` |
|       ! 0 | 3599 | `		rc = PH7_GenCompileError(pGen,E_ERROR,` |
|       ! 0 | 3600 | `			pNode ? pNode->pStart->nLine : 1,` |
|         - | 3601 | `			"Can't use nullsafe operator in write context");` |
|       ! 0 | 3602 | `		return rc == SXERR_ABORT ? SXERR_ABORT : SXERR_SYNTAX;` |
|         - | 3603 | `	}` |
|       467 | 3604 | `	if( pNode && PH7_ExprIsModifiableValue(pNode) == FALSE ){` |
|         6 | 3605 | `		return PH7_ExprOperandNotAVariable(pGen,pNode);` |
|         - | 3606 | `	}` |
|       463 | 3607 | `	return SXRET_OK;` |
|       238 | 3608 | `}` |
|         - | 3609 | `/*` |
|         - | 3610 | ` * Compile an unset() statement.` |
|         - | 3611 | ` * unset($var, $arr[$key], ...);` |
|         - | 3612 | ` * Each argument is compiled with EXPR_FLAG_LOAD_IDX_STORE so that` |
|         - | 3613 | ` * PH7_OP_LOAD_IDX emits iP2=1, triggering COW separation on the` |
|         - | 3614 | ` * parent array before extracting the element to unset.` |
|         - | 3615 | ` */` |
|      3692 | 3616 | `static sxi32 PH7_CompileUnset(ph7_gen_state *pGen)` |
|         5 | 3617 | `{` |
|      3697 | 3618 | `	SyToken *pTmp,*pEnd,*pNext = 0;` |
|      3697 | 3619 | `	sxu32 nIdx = 0;` |
|         - | 3620 | `	SyString sName;` |
|         - | 3621 | `	sxi32 rc;` |
|         - | 3622 | `	/* Jump the 'unset' keyword */` |
|      3697 | 3623 | `	pGen->pIn++;` |
|         - | 3624 | `	/* Save delimiter */` |
|      3697 | 3625 | `	pTmp = pGen->pEnd;` |
|         - | 3626 | `	/* Skip optional opening parenthesis and find the matching close */` |
|      3697 | 3627 | `	pEnd = pTmp; /* Default: scan to statement end */` |
|      3697 | 3628 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 3629 | `		/* Find matching ')' — start scanning AFTER the '(' */` |
|         - | 3630 | `		SyToken *pClose;` |
|      3697 | 3631 | `		pGen->pIn++;   /* Skip '(' */` |
|      3697 | 3632 | `		PH7_DelimitNestedTokens(pGen->pIn,pTmp,PH7_TK_LPAREN,PH7_TK_RPAREN,&pClose);` |
|      3697 | 3633 | `		pEnd = pClose; /* Stop at ')' */` |
|      1844 | 3634 | `	}` |
|      3697 | 3635 | `	SyStringInitFromBuf(&sName,"unset",sizeof("unset")-1);` |
|         - | 3636 | `	/* Resolve the 'unset' builtin name once */` |
|      3697 | 3637 | `	if( SXRET_OK != GenStateFindLiteral(&(*pGen),&sName,&nIdx) ){` |
|       622 | 3638 | `		ph7_value *pObj = PH7_ReserveConstObj(pGen->pVm,&nIdx);` |
|       622 | 3639 | `		if( pObj == 0 ){` |
|       ! 0 | 3640 | `			return SXERR_ABORT;` |
|         - | 3641 | `		}` |
|       622 | 3642 | `		PH7_MemObjInitFromString(pGen->pVm,pObj,&sName);` |
|       622 | 3643 | `		GenStateInstallLiteral(&(*pGen),pObj,nIdx);` |
|       307 | 3644 | `	}` |
|         - | 3645 | `	/* Compile each comma-separated argument */` |
|     12794 | 3646 | `	while( SXRET_OK == PH7_GetNextExpr(pGen->pIn,pEnd,&pNext) ){` |
|      9106 | 3647 | `		if( pGen->pIn < pNext ){` |
|         - | 3648 | `			/*` |
|         - | 3649 | ``			 * A PLAIN variable (`unset($x)`, exactly two tokens: '$' and the name) drops a`` |
|         - | 3650 | `			 * single NAME binding, which the unset() builtin cannot express — all it ever` |
|         - | 3651 | `			 * receives is the value's slot index, and unsetting the slot destroys whatever` |
|         - | 3652 | `			 * else aliases it. Emit OP_UNSET_VAR with the name instead. Subscripts and` |
|         - | 3653 | ``			 * properties (`unset($a[k])`, `unset($o->p)`) keep the existing path, which`` |
|         - | 3654 | `			 * already removes just the element/property.` |
|         - | 3655 | `			 */` |
|      9101 | 3656 | `			if( &pGen->pIn[2] == pNext` |
|      8868 | 3657 | `				&& (pGen->pIn[0].nType & PH7_TK_DOLLAR)` |
|      8640 | 3658 | `				&& (pGen->pIn[1].nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|         - | 3659 | `				SyString *pVarName;` |
|         - | 3660 | ``				/* php refuses `unset($this)` where it is written. The tree validator`` |
|         - | 3661 | `				 * cannot see it — this fast path never builds a tree. */` |
|      8633 | 3662 | `				if( pGen->pIn[1].sData.nByte == sizeof("this")-1` |
|      4635 | 3663 | `				 && SyMemcmp((const void *)pGen->pIn[1].sData.zString,` |
|       320 | 3664 | `				             (const void *)"this",sizeof("this")-1) == 0 ){` |
|         5 | 3665 | `					rc = PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - | 3666 | `						"Cannot unset $this");` |
|         5 | 3667 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 3668 | `						return SXERR_ABORT;` |
|         - | 3669 | `					}` |
|         - | 3670 | `					/* php stops compiling at its own fatal; this generator carries` |
|         - | 3671 | `					 * on to a budget of fifteen, so leave the cursor PAST the whole` |
|         - | 3672 | ``					 * `unset(...)` rather than on the operand it refused. Resuming`` |
|         - | 3673 | `					 * there re-read the closing ')' as a statement of its own and` |
|         - | 3674 | `					 * printed an "Unmatched ')'" under the fatal that php never` |
|         - | 3675 | `					 * reaches. */` |
|         5 | 3676 | `					pGen->pIn = pEnd;` |
|         5 | 3677 | `					if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|         5 | 3678 | `						pGen->pIn++;` |
|         2 | 3679 | `					}` |
|         5 | 3680 | `					pGen->pEnd = pTmp;` |
|         5 | 3681 | `					return SXERR_SYNTAX;` |
|         - | 3682 | `				}` |
|     12942 | 3683 | `				char *zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|      8629 | 3684 | `					pGen->pIn[1].sData.zString,pGen->pIn[1].sData.nByte);` |
|      8634 | 3685 | `				pVarName = (SyString *)SyMemBackendAlloc(&pGen->pVm->sAllocator,sizeof(SyString));` |
|      8634 | 3686 | `				if( zDup == 0 \|\| pVarName == 0 ){` |
|       ! 0 | 3687 | `					PH7_GenCompileError(&(*pGen),E_ERROR,pGen->pIn->nLine,` |
|         - | 3688 | `						"Fatal, PH7 is running out of memory");` |
|       ! 0 | 3689 | `					return SXERR_ABORT;` |
|         - | 3690 | `				}` |
|      8634 | 3691 | `				SyStringInitFromBuf(pVarName,zDup,pGen->pIn[1].sData.nByte);` |
|      8634 | 3692 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_UNSET_VAR,0,0,pVarName,0);` |
|      8634 | 3693 | `				pGen->pIn = pNext;` |
|      8634 | 3694 | `				if( pGen->pIn < pEnd ){` |
|      5370 | 3695 | `					pGen->pIn++; /* Jump the trailing comma */` |
|      2678 | 3696 | `				}` |
|      8634 | 3697 | `				continue;` |
|         - | 3698 | `			}` |
|       473 | 3699 | `			pGen->pEnd = pNext;` |
|       473 | 3700 | `			rc = PH7_CompileExpr(&(*pGen),` |
|         - | 3701 | `				EXPR_FLAG_RDONLY_LOAD\|EXPR_FLAG_LOAD_IDX_UNSET,` |
|         - | 3702 | `				GenStateUnsetValidator);` |
|       473 | 3703 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 3704 | `				return SXERR_ABORT;` |
|         - | 3705 | `			}` |
|       473 | 3706 | `			if( rc != SXERR_EMPTY ){` |
|         - | 3707 | ``				/* Emit call for this single argument. PH7_CALL_CONSTRUCT: `unset` is a`` |
|         - | 3708 | `				 * language construct, so the host function this dispatches to is hidden` |
|         - | 3709 | `				 * from every name a script can spell (PH7_VmGetHostFunction). */` |
|       471 | 3710 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_LOADC,0,nIdx,0,0);` |
|       704 | 3711 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_CALL,1,PH7_CALL_CONSTRUCT,` |
|       233 | 3712 | `					GenStateAttachStrictFlag(pGen,0),0);` |
|       471 | 3713 | `				PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|       233 | 3714 | `			}` |
|       234 | 3715 | `		}` |
|         - | 3716 | `		/* Jump trailing commas */` |
|       519 | 3717 | `		while( pNext < pEnd && (pNext->nType & PH7_TK_COMMA) ){` |
|        50 | 3718 | `			pNext++;` |
|         4 | 3719 | `		}` |
|       473 | 3720 | `		pGen->pIn = pNext;` |
|         5 | 3721 | `	}` |
|         - | 3722 | `	/* Skip past the closing ')' if present */` |
|      3693 | 3723 | `	if( pGen->pIn < pTmp && (pGen->pIn->nType & PH7_TK_RPAREN) ){` |
|      3693 | 3724 | `		pGen->pIn++;` |
|      1842 | 3725 | `	}` |
|         - | 3726 | `	/* Restore token stream */` |
|      3693 | 3727 | `	pGen->pEnd = pTmp;` |
|      3693 | 3728 | `	return SXRET_OK;` |
|      1849 | 3729 | `}` |
|         - | 3730 | `/*` |
|         - | 3731 | ` * PHP Language construct table.` |
|         - | 3732 | ` */` |
|         - | 3733 | `static const LangConstruct aLangConstruct[] = {` |
|         - | 3734 | `	{ PH7_TKWRD_ECHO,     PH7_CompileEcho     }, /* echo language construct */` |
|         - | 3735 | `	{ PH7_TKWRD_IF,       PH7_CompileIf       }, /* if statement */` |
|         - | 3736 | `	{ PH7_TKWRD_FOR,      PH7_CompileFor      }, /* for statement */` |
|         - | 3737 | `	{ PH7_TKWRD_WHILE,    PH7_CompileWhile    }, /* while statement */` |
|         - | 3738 | `	{ PH7_TKWRD_FOREACH,  PH7_CompileForeach  }, /* foreach statement */` |
|         - | 3739 | `	{ PH7_TKWRD_FUNCTION, PH7_CompileFunction }, /* function statement */` |
|         - | 3740 | `	{ PH7_TKWRD_CONTINUE, PH7_CompileContinue }, /* continue statement */` |
|         - | 3741 | `	{ PH7_TKWRD_BREAK,    PH7_CompileBreak    }, /* break statement */` |
|         - | 3742 | `	{ PH7_TKWRD_RETURN,   PH7_CompileReturn   }, /* return statement */` |
|         - | 3743 | `	{ PH7_TKWRD_SWITCH,   PH7_CompileSwitch   }, /* Switch statement */` |
|         - | 3744 | `	{ PH7_TKWRD_DO,       PH7_CompileDoWhile  }, /* do{ }while(); statement */` |
|         - | 3745 | `	{ PH7_TKWRD_GLOBAL,   PH7_CompileGlobal   }, /* global statement */` |
|         - | 3746 | `	{ PH7_TKWRD_STATIC,   PH7_CompileStatic   }, /* static statement */` |
|         - | 3747 | `	{ PH7_TKWRD_DIE,      PH7_CompileHalt     }, /* die language construct */` |
|         - | 3748 | `	{ PH7_TKWRD_EXIT,     PH7_CompileHalt     }, /* exit language construct */` |
|         - | 3749 | `	{ PH7_TKWRD_TRY,      PH7_CompileTry      }, /* try statement */` |
|         - | 3750 | `	{ PH7_TKWRD_THROW,    PH7_CompileThrow    }, /* throw statement */` |
|         - | 3751 | `	{ PH7_TKWRD_GOTO,     PH7_CompileGoto     }, /* goto statement */` |
|         - | 3752 | `	{ PH7_TKWRD_CONST,    PH7_CompileConstant }, /* const statement */` |
|         - | 3753 | `	{ PH7_TKWRD_VAR,      PH7_CompileVar      }, /* var statement */` |
|         - | 3754 | `	{ PH7_TKWRD_NAMESPACE, PH7_CompileNamespace }, /* namespace statement */` |
|         - | 3755 | `	{ PH7_TKWRD_USE,      PH7_CompileUse      },  /* use statement */` |
|         - | 3756 | `	{ PH7_TKWRD_DECLARE,  PH7_CompileDeclare  },  /* declare statement */` |
|         - | 3757 | `	{ PH7_TKWRD_UNSET,    PH7_CompileUnset   }   /* unset statement */` |
|         - | 3758 | `};` |
|         - | 3759 | `/*` |
|         - | 3760 | ` * Return a pointer to the statement handler routine associated` |
|         - | 3761 | ` * with a given PHP keyword [i.e: if,for,while,...].` |
|         - | 3762 | ` */` |
|   1434104 | 3763 | `static ProcLangConstruct GenStateGetStatementHandler(` |
|         - | 3764 | `	sxu32 nKeywordID,   /* Keyword  ID*/` |
|         - | 3765 | `	SyToken *pLookahed  /* Look-ahead token */` |
|         - | 3766 | `	)` |
|         5 | 3767 | `{` |
|   1434109 | 3768 | `	sxu32 n = 0;` |
|   4254362 | 3769 | `	for(;;){` |
|   8520451 | 3770 | `		if( n >= SX_ARRAYSIZE(aLangConstruct) ){` |
|      6675 | 3771 | `			break;` |
|         - | 3772 | `		}` |
|   8513781 | 3773 | `		if( aLangConstruct[n].nID == nKeywordID ){` |
|   1427439 | 3774 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed && (pLookahed->nType & PH7_TK_OP)){` |
|       ! 0 | 3775 | `				const ph7_expr_op *pOp = (const ph7_expr_op *)pLookahed->pUserData;` |
|       ! 0 | 3776 | `				if( pOp && pOp->iOp == EXPR_OP_DC /*::*/){` |
|         - | 3777 | `					/* 'static' (class context),return null */` |
|       ! 0 | 3778 | `					return 0;` |
|         - | 3779 | `				}` |
|       ! 0 | 3780 | `			}` |
|   1427434 | 3781 | `			if( nKeywordID == PH7_TKWRD_STATIC && pLookahed` |
|        86 | 3782 | `				&& (pLookahed->nType & PH7_TK_KEYWORD)` |
|        52 | 3783 | `				&& SX_PTR_TO_INT(pLookahed->pUserData) == PH7_TKWRD_FN ){` |
|         - | 3784 | `				/* 'static fn(...)' arrow function — compile as expression */` |
|         5 | 3785 | `				return 0;` |
|         - | 3786 | `			}` |
|         - | 3787 | `			/* Return a pointer to the handler.` |
|         - | 3788 | `			*/` |
|   1427435 | 3789 | `			return aLangConstruct[n].xConstruct;` |
|         - | 3790 | `		}` |
|   7086347 | 3791 | `		n++;` |
|         5 | 3792 | `	}` |
|      6675 | 3793 | `	if( pLookahed ){` |
|      6675 | 3794 | `		if(nKeywordID == PH7_TKWRD_INTERFACE && PH7_IsClassNameToken(pLookahed) ){` |
|       371 | 3795 | `			return PH7_CompileClassInterface;` |
|      6309 | 3796 | `		}else if(nKeywordID == PH7_TKWRD_CLASS && PH7_IsClassNameToken(pLookahed) ){` |
|      4813 | 3797 | `			return PH7_CompileClass;` |
|      1501 | 3798 | `		}else if(nKeywordID == PH7_TKWRD_TRAIT && PH7_IsClassNameToken(pLookahed) ){` |
|       361 | 3799 | `			return PH7_CompileTrait;` |
|         - | 3800 | `		}` |
|         - | 3801 | ``		/* `final`/`abstract` (and `readonly`, an ID) class modifiers — possibly`` |
|         - | 3802 | `		 * combined — are routed via GenStateStartsModifiedClass in the chunk` |
|         - | 3803 | `		 * compiler, which can scan the whole modifier run (the lookahead here is` |
|         - | 3804 | ``		 * a single token and cannot see past `final readonly …`). */`` |
|       570 | 3805 | `	}` |
|         - | 3806 | `	/* Not a language construct */` |
|      1145 | 3807 | `	return 0;` |
|    716039 | 3808 | `}` |
|         - | 3809 | `/*` |
|         - | 3810 | ` * Which words may NAME a class, an interface, a trait or an enum, and which may` |
|         - | 3811 | ` * not — php has two separate answers and this file used to have one.` |
|         - | 3812 | ` *` |
|         - | 3813 | ` * php's SCANNER decides the first: a reserved keyword is not an identifier, so` |
|         - | 3814 | `` * `class list {}` and `class callable {}` are parse errors. Its COMPILER decides`` |
|         - | 3815 | ` * the second, for a handful of words the scanner does hand over as identifiers:` |
|         - | 3816 | ``  * `zend_is_reserved_class_name` refuses `int`, `bool`, `void`, `null`, `self` `` |
|         - | 3817 | `` * and their neighbours with `Cannot use "X" as a class name as it is reserved`.`` |
|         - | 3818 | ` *` |
|         - | 3819 | ` * PHL's keyword set is not php's, which is where the divergence came from in both` |
|         - | 3820 | `` * directions. `integer` and `boolean` are CAST words here and identifiers in php,`` |
|         - | 3821 | `` * so `class Integer extends Base {}` — phpseclib writes exactly that, three times`` |
|         - | 3822 | ``  * — did not compile at all. And `void`, `never`, `null`, `false`, `true`, `mixed` `` |
|         - | 3823 | `` * and `iterable` arrive as plain identifiers here, so declaring a class with one`` |
|         - | 3824 | ` * of those names SUCCEEDED where php refuses.` |
|         - | 3825 | ` */` |
|    108436 | 3826 | `static int GenStateNameIs(const SyString *pName,const char *zWord)` |
|         5 | 3827 | `{` |
|    108441 | 3828 | `	sxu32 n = (sxu32)SyStrlen(zWord);` |
|         - | 3829 | `	/* Length FIRST: the token's bytes point into the source and are not` |
|         - | 3830 | `	 * NUL-terminated, so a shorter name must never be compared over its end. */` |
|    108441 | 3831 | `	return pName->nByte == n && SyStrnicmp(pName->zString,zWord,n) == 0;` |
|         5 | 3832 | `}` |
|        24 | 3833 | `static int GenStateClassNameKeywordOk(const SyString *pName)` |
|         2 | 3834 | `{` |
|         - | 3835 | `	/* The only two words PHL lexes as keywords that php lets name a class. */` |
|        26 | 3836 | `	return GenStateNameIs(pName,"integer") \|\| GenStateNameIs(pName,"boolean");` |
|         2 | 3837 | `}` |
|     16388 | 3838 | `PH7_PRIVATE int PH7_IsClassNameToken(const SyToken *pTok)` |
|         5 | 3839 | `{` |
|     16393 | 3840 | `	if( pTok == 0 ){` |
|       ! 0 | 3841 | `		return 0;` |
|         - | 3842 | `	}` |
|     16393 | 3843 | `	if( pTok->nType & PH7_TK_KEYWORD ){` |
|        26 | 3844 | `		return GenStateClassNameKeywordOk(&pTok->sData);` |
|         - | 3845 | `	}` |
|     16369 | 3846 | `	if( (pTok->nType & PH7_TK_ID) == 0 ){` |
|         5 | 3847 | `		return 0;` |
|         - | 3848 | `	}` |
|     16365 | 3849 | `	if( pTok->nType & PH7_TK_OP ){` |
|         - | 3850 | ``		/* php's alpha-stream operators — `and`, `or`, `xor`, `new`, `clone`,`` |
|         - | 3851 | ``		 * `instanceof` — are keywords in its scanner and cannot be identifiers.`` |
|         - | 3852 | `		 * They reach here carrying both flags, and were accepted as names. */` |
|       ! 0 | 3853 | `		return 0;` |
|         - | 3854 | `	}` |
|         - | 3855 | `	{` |
|         - | 3856 | `		/* Two more php keywords that PHL treats as context-sensitive identifiers. */` |
|         - | 3857 | `		static const char *const azNo[] = { "callable", "readonly" };` |
|         - | 3858 | `		sxu32 i;` |
|     49085 | 3859 | `		for( i = 0 ; i < SX_ARRAYSIZE(azNo) ; ++i ){` |
|     32725 | 3860 | `			if( GenStateNameIs(&pTok->sData,azNo[i]) ){` |
|       ! 0 | 3861 | `				return 0;` |
|         - | 3862 | `			}` |
|     16365 | 3863 | `		}` |
|         - | 3864 | `	}` |
|     16365 | 3865 | `	return 1;` |
|      8199 | 3866 | `}` |
|         - | 3867 | `/*` |
|         - | 3868 | ` * php's zend_is_reserved_class_name: a word its scanner DOES hand over as an` |
|         - | 3869 | ` * identifier but its compiler refuses to name a class with. The check is` |
|         - | 3870 | ` * case-insensitive and the refusal quotes the name as WRITTEN.` |
|         - | 3871 | ` */` |
|      5046 | 3872 | `PH7_PRIVATE int PH7_IsReservedClassName(const SyString *pName)` |
|         5 | 3873 | `{` |
|         - | 3874 | `	static const char *const azReserved[] = {` |
|         - | 3875 | `		"bool", "int", "float", "string", "null", "false", "true", "void",` |
|         - | 3876 | `		"never", "iterable", "object", "mixed", "self", "parent", "static"` |
|         - | 3877 | `	};` |
|         - | 3878 | `	sxu32 i;` |
|     80729 | 3879 | `	for( i = 0 ; i < SX_ARRAYSIZE(azReserved) ; ++i ){` |
|     75685 | 3880 | `		if( GenStateNameIs(pName,azReserved[i]) ){` |
|         3 | 3881 | `			return 1;` |
|         - | 3882 | `		}` |
|     37844 | 3883 | `	}` |
|      5049 | 3884 | `	return 0;` |
|      2528 | 3885 | `}` |
|         - | 3886 | `/*` |
|         - | 3887 | ` * Check if the given keyword is in fact a PHP language construct.` |
|         - | 3888 | ` * Return TRUE on success. FALSE otheriwse.` |
|         - | 3889 | ` */` |
|      1144 | 3890 | `static int GenStateisLangConstruct(sxu32 nKeyword)` |
|         5 | 3891 | `{` |
|         - | 3892 | `	int rc;` |
|      1149 | 3893 | `	rc = PH7_IsLangConstruct(nKeyword,TRUE);` |
|      1149 | 3894 | `	if( rc == FALSE ){` |
|       726 | 3895 | `		if( nKeyword == PH7_TKWRD_SELF \|\| nKeyword == PH7_TKWRD_PARENT \|\| nKeyword == PH7_TKWRD_STATIC` |
|       584 | 3896 | `			\|\| nKeyword == PH7_TKWRD_YIELD` |
|         - | 3897 | ``			/* `match` is an EXPRESSION, and php takes an expression statement made of`` |
|         - | 3898 | ``			 * one: `match (true) { ... };` is how a dispatch table is written when the`` |
|         - | 3899 | `			 * answer is not wanted. Without this the statement dispatcher refused the` |
|         - | 3900 | `			 * keyword outright, and Doctrine's DQL parser -- which dispatches its tree` |
|         - | 3901 | `			 * walkers exactly that way -- did not compile. */` |
|       303 | 3902 | `			\|\| nKeyword == PH7_TKWRD_MATCH` |
|         - | 3903 | ``			/* php reserves NONE of `int`/`integer`/`bool`/`boolean`/`float`/`` |
|         - | 3904 | ``			 * `string`/`object`: its scanner hands every one of them back as a`` |
|         - | 3905 | `			 * plain T_STRING, and only a TYPE position gives them a meaning. A` |
|         - | 3906 | `			 * statement that begins with one is therefore an ordinary expression` |
|         - | 3907 | ``			 * -- `Integer::setModulo($id, $m);`, which is how phpseclib's`` |
|         - | 3908 | `			 * BinaryField spells the class it imported under that name, and which` |
|         - | 3909 | ``			 * this dispatcher answered `Unexpected keyword 'Integer'` for. The`` |
|         - | 3910 | ``			 * same word after `$x = ` already compiled, so only the STATEMENT head`` |
|         - | 3911 | `			 * was refusing it. */` |
|        19 | 3912 | `			\|\| nKeyword == PH7_TKWRD_INT \|\| nKeyword == PH7_TKWRD_BOOL` |
|         9 | 3913 | `			\|\| nKeyword == PH7_TKWRD_FLOAT \|\| nKeyword == PH7_TKWRD_STRING` |
|        13 | 3914 | `			\|\| nKeyword == PH7_TKWRD_OBJECT` |
|         - | 3915 | `			/*\|\| nKeyword == PH7_TKWRD_CLASS \|\| nKeyword == PH7_TKWRD_FINAL \|\| nKeyword == PH7_TKWRD_EXTENDS` |
|         - | 3916 | `			  \|\| nKeyword == PH7_TKWRD_ABSTRACT \|\| nKeyword == PH7_TKWRD_INTERFACE` |
|         - | 3917 | `			  \|\| nKeyword == PH7_TKWRD_PUBLIC \|\| nKeyword == PH7_TKWRD_PROTECTED` |
|         - | 3918 | `			  \|\| nKeyword == PH7_TKWRD_PRIVATE \|\| nKeyword == PH7_TKWRD_IMPLEMENTS` |
|         - | 3919 | `			*/` |
|         - | 3920 | `			){` |
|       727 | 3921 | `				rc = TRUE;` |
|       361 | 3922 | `		}` |
|       361 | 3923 | `	}` |
|      1149 | 3924 | `	return rc;` |
|         5 | 3925 | `}` |
|         - | 3926 | `/*` |
|         - | 3927 | ` * Compile a PHP chunk.` |
|         - | 3928 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 3929 | ` * takes care of generating the appropriate error message.` |
|         - | 3930 | ` */` |
|         - | 3931 | `/*` |
|         - | 3932 | ` * Update pGen->sPendingDoc for the statement whose first token is` |
|         - | 3933 | ` * pGen->pIn: when a docblock trivia is keyed to that token's index in` |
|         - | 3934 | ` * the chunk token set it becomes the pending docblock. An existing` |
|         - | 3935 | ` * pending docblock is LEFT in place otherwise: Zend keeps the last-seen` |
|         - | 3936 | ` * doc comment until a declaration consumes it, so a docblock survives` |
|         - | 3937 | ` * intervening non-declaration statements.` |
|         - | 3938 | ` */` |
|   2439824 | 3939 | `PH7_PRIVATE void GenStateSetPendingDoc(ph7_gen_state *pGen)` |
|         5 | 3940 | `{` |
|   2439829 | 3941 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|   2439829 | 3942 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|   2439829 | 3943 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 3944 | `	sxu32 nIdx, n;` |
|   2439824 | 3945 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|     14655 | 3946 | `	 \|\| pGen->pIn < pBase \|\| pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|         - | 3947 | `		/* Re-tokenized substream (string interpolation, synthesized code):` |
|         - | 3948 | `		 * indexes do not map to the sidecar */` |
|   2425179 | 3949 | `		return;` |
|         - | 3950 | `	}` |
|     14655 | 3951 | `	nIdx = (sxu32)(pGen->pIn - pBase);` |
|         - | 3952 | `	/* Attributes must be adjacent to their declaration (unlike docblocks):` |
|         - | 3953 | `	 * reset at every boundary, then collect the groups keyed to this token. */` |
|     14655 | 3954 | `	SySetReset(&pGen->aPendingAttrs);` |
|    108683 | 3955 | `	for( n = 0 ; n < nT ; n++ ){` |
|     94033 | 3956 | `		if( aT[n].nTokIdx != nIdx ){` |
|     92919 | 3957 | `			continue;` |
|         - | 3958 | `		}` |
|      1119 | 3959 | `		if( aT[n].iKind == PH7_TRIVIA_DOC ){` |
|       483 | 3960 | `			pGen->sPendingDoc = aT[n].sText;` |
|       880 | 3961 | `		}else if( aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|       641 | 3962 | `			SySetPut(&pGen->aPendingAttrs,(const void *)&aT[n]);` |
|       318 | 3963 | `		}` |
|       562 | 3964 | `	}` |
|   1217990 | 3965 | `}` |
|         - | 3966 | `/*` |
|         - | 3967 | ` * Hand the pending docblock (if any) to a declaration: duplicate it into` |
|         - | 3968 | ` * the VM allocator (the raw script buffer dies after compilation) and` |
|         - | 3969 | ` * clear the pending slot so sibling declarations do not inherit it.` |
|         - | 3970 | ` */` |
|    199755 | 3971 | `PH7_PRIVATE void GenStateConsumeDoc(ph7_gen_state *pGen,SyString *pOut)` |
|         5 | 3972 | `{` |
|         - | 3973 | `	char *zDup;` |
|    199760 | 3974 | `	if( SyStringLength(&pGen->sPendingDoc) < 1 ){` |
|    199328 | 3975 | `		return;` |
|         - | 3976 | `	}` |
|       653 | 3977 | `	zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       216 | 3978 | `		SyStringData(&pGen->sPendingDoc),SyStringLength(&pGen->sPendingDoc));` |
|       437 | 3979 | `	if( zDup ){` |
|       437 | 3980 | `		SyStringInitFromBuf(pOut,zDup,SyStringLength(&pGen->sPendingDoc));` |
|       216 | 3981 | `	}` |
|       437 | 3982 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|     99739 | 3983 | `}` |
|         - | 3984 | `/*` |
|         - | 3985 | ` * Compile one recorded #[...] attribute group (the span between the group` |
|         - | 3986 | ` * delimiters) into ph7_attribute records appended to pOut. The span is` |
|         - | 3987 | ` * duplicated into the VM allocator FIRST (compiled bytecode and interned` |
|         - | 3988 | ` * names may point into the token text, which must outlive the raw script` |
|         - | 3989 | ` * buffer), then re-tokenized on its own. Each argument expression compiles` |
|         - | 3990 | ` * with the container-swap idiom into its own OP_DONE-terminated set,` |
|         - | 3991 | ` * evaluated lazily at ReflectionAttribute time (PHP semantics).` |
|         - | 3992 | ` */` |
|       662 | 3993 | `static sxi32 GenStateCompileAttrSpan(ph7_gen_state *pGen,ph7_trivia *pTrivia,SySet *pOut)` |
|         5 | 3994 | `{` |
|         - | 3995 | `	SySet *pToken;` |
|         - | 3996 | `	SyToken *pIn, *pEnd, *pSavedIn, *pSavedEnd;` |
|         - | 3997 | `	char *zSpan;` |
|       667 | 3998 | `	sxi32 rc = SXRET_OK;` |
|       667 | 3999 | `	if( SyStringLength(&pTrivia->sText) < 1 ){` |
|       ! 0 | 4000 | `		return SXRET_OK;` |
|         - | 4001 | `	}` |
|       998 | 4002 | `	zSpan = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       331 | 4003 | `		SyStringData(&pTrivia->sText),SyStringLength(&pTrivia->sText));` |
|       667 | 4004 | `	if( zSpan == 0 ){` |
|       ! 0 | 4005 | `		return SXRET_OK;` |
|         - | 4006 | `	}` |
|         - | 4007 | `	/* The token set must outlive compilation too: interned operands may` |
|         - | 4008 | `	 * reference token payloads. Pool-allocated, never released — bounded by` |
|         - | 4009 | `	 * the number of attribute declarations in the program. */` |
|       667 | 4010 | `	pToken = (SySet *)SyMemBackendPoolAlloc(&pGen->pVm->sAllocator,sizeof(SySet));` |
|       667 | 4011 | `	if( pToken == 0 ){` |
|       ! 0 | 4012 | `		return SXRET_OK;` |
|         - | 4013 | `	}` |
|       667 | 4014 | `	SySetInit(pToken,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       667 | 4015 | `	PH7_TokenizePHP(zSpan,SyStringLength(&pTrivia->sText),pTrivia->nLine,pToken,0);` |
|       667 | 4016 | `	pIn = (SyToken *)SySetBasePtr(pToken);` |
|       667 | 4017 | `	pEnd = &pIn[SySetUsed(pToken)];` |
|       667 | 4018 | `	pSavedIn = pGen->pIn;` |
|       667 | 4019 | `	pSavedEnd = pGen->pEnd;` |
|       671 | 4020 | `	while( pIn < pEnd ){` |
|         - | 4021 | `		ph7_attribute sAttr;` |
|         - | 4022 | `		SyBlob sFQN;` |
|       671 | 4023 | `		int bAbsolute = 0;` |
|       671 | 4024 | `		SyZero(&sAttr,sizeof(sAttr));` |
|       671 | 4025 | `		SySetInit(&sAttr.aArgs,&pGen->pVm->sAllocator,sizeof(ph7_attr_arg));` |
|       671 | 4026 | `		sAttr.nLine = pIn->nLine;` |
|       671 | 4027 | `		if( pIn->nType & PH7_TK_NSSEP ){` |
|       479 | 4028 | `			bAbsolute = 1;` |
|       479 | 4029 | `			pIn++;` |
|       237 | 4030 | `		}` |
|       671 | 4031 | `		SyBlobInit(&sFQN,&pGen->pVm->sAllocator);` |
|         - | 4032 | ``		/* `#[namespace\Attr]` — the current namespace, absolute from there. */`` |
|       671 | 4033 | `		if( !bAbsolute && GenStateNsRelPrefix(pGen,&pIn,pEnd,&sFQN) ){` |
|       ! 0 | 4034 | `			bAbsolute = 1;` |
|       ! 0 | 4035 | `		}` |
|       681 | 4036 | `		while( pIn < pEnd && (pIn->nType & (PH7_TK_ID\|PH7_TK_KEYWORD)) ){` |
|       681 | 4037 | `			SyBlobAppend(&sFQN,pIn->sData.zString,pIn->sData.nByte);` |
|       681 | 4038 | `			pIn++;` |
|       681 | 4039 | `			if( pIn < pEnd && (pIn->nType & PH7_TK_NSSEP) ){` |
|        11 | 4040 | `				SyBlobAppend(&sFQN,"\\",1);` |
|        11 | 4041 | `				pIn++;` |
|        11 | 4042 | `				continue;` |
|         - | 4043 | `			}` |
|       671 | 4044 | `			break;` |
|       ! 0 | 4045 | `		}` |
|       671 | 4046 | `		if( SyBlobLength(&sFQN) < 1 ){` |
|         - | 4047 | `			/* Malformed group: stop quietly (the group was inert trivia before` |
|         - | 4048 | `			 * this feature; never turn it into a new fatal) */` |
|       ! 0 | 4049 | `			SyBlobRelease(&sFQN);` |
|       ! 0 | 4050 | `			break;` |
|         - | 4051 | `		}` |
|         - | 4052 | `		/* Resolve to an FQN: absolute names verbatim; else use-import alias,` |
|         - | 4053 | `		 * else current-namespace prefix (PHP attribute name resolution) */` |
|         - | 4054 | `		{` |
|       671 | 4055 | `			const char *zName = (const char *)SyBlobData(&sFQN);` |
|       671 | 4056 | `			sxu32 nName = SyBlobLength(&sFQN);` |
|       671 | 4057 | `			char *zDup = 0;` |
|       671 | 4058 | `			if( !bAbsolute ){` |
|         - | 4059 | `				/* An attribute name resolves exactly the way every other class name` |
|         - | 4060 | `				 * does -- the LEADING segment through the use imports, else the` |
|         - | 4061 | `				 * current-namespace prefix. This looked the WHOLE qualified string up` |
|         - | 4062 | `				 * in the import table, which can never match a single-segment alias,` |
|         - | 4063 | ``				 * and then prefixed the namespace anyway: `use Vv as Rule;` with`` |
|         - | 4064 | ``				 * `#[Rule\\A]` asked for `App\\Rule\\A` and got`` |
|         - | 4065 | ``				 * `Attribute class ... not found`. It is the same mistake`` |
|         - | 4066 | `				 * GenStateResolveName was written to fix for the other name positions,` |
|         - | 4067 | `				 * so it is that function's job here too. */` |
|         - | 4068 | `				SyBlob sTmp;` |
|         - | 4069 | `				SyString sRaw;` |
|       197 | 4070 | `				SyStringInitFromBuf(&sRaw,zName,nName);` |
|       197 | 4071 | `				SyBlobInit(&sTmp,&pGen->pVm->sAllocator);` |
|       197 | 4072 | `				GenStateResolveName(&(*pGen),&sRaw,&sTmp);` |
|       197 | 4073 | `				if( SyBlobLength(&sTmp) > 0 ){` |
|       293 | 4074 | `					zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|       192 | 4075 | `						(const char *)SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|       197 | 4076 | `					if( zDup ){` |
|       197 | 4077 | `						SyStringInitFromBuf(&sAttr.sName,zDup,SyBlobLength(&sTmp));` |
|        96 | 4078 | `					}` |
|        96 | 4079 | `				}` |
|       197 | 4080 | `				SyBlobRelease(&sTmp);` |
|        96 | 4081 | `			}` |
|       671 | 4082 | `			if( SyStringLength(&sAttr.sName) < 1 ){` |
|       479 | 4083 | `				zDup = SyMemBackendStrDup(&pGen->pVm->sAllocator,zName,nName);` |
|       479 | 4084 | `				if( zDup ){` |
|       479 | 4085 | `					SyStringInitFromBuf(&sAttr.sName,zDup,nName);` |
|       237 | 4086 | `				}` |
|       237 | 4087 | `			}` |
|         - | 4088 | `		}` |
|       671 | 4089 | `		SyBlobRelease(&sFQN);` |
|       671 | 4090 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_LPAREN) ){` |
|         - | 4091 | `			SyToken *pArgsEnd;` |
|       138 | 4092 | `			pIn++;` |
|       138 | 4093 | `			PH7_DelimitNestedTokens(pIn,pEnd,PH7_TK_LPAREN,PH7_TK_RPAREN,&pArgsEnd);` |
|       350 | 4094 | `			while( pIn < pArgsEnd ){` |
|       218 | 4095 | `				SyToken *pArgStart = pIn, *pArgStop = pIn;` |
|       218 | 4096 | `				sxi32 iDepth = 0;` |
|         - | 4097 | `				ph7_attr_arg sArgRec;` |
|       836 | 4098 | `				while( pArgStop < pArgsEnd ){` |
|       704 | 4099 | `					if( pArgStop->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|        44 | 4100 | `						iDepth++;` |
|       683 | 4101 | `					}else if( pArgStop->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|        44 | 4102 | `						iDepth--;` |
|       641 | 4103 | `					}else if( (pArgStop->nType & PH7_TK_COMMA) && iDepth == 0 ){` |
|        84 | 4104 | `						break;` |
|         - | 4105 | `					}` |
|       622 | 4106 | `					pArgStop++;` |
|         4 | 4107 | `				}` |
|       218 | 4108 | `				SyZero(&sArgRec,sizeof(sArgRec));` |
|       218 | 4109 | `				SySetInit(&sArgRec.aByteCode,&pGen->pVm->sAllocator,sizeof(VmInstr));` |
|       214 | 4110 | `				if( pArgStart < pArgStop && (pArgStart->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|       148 | 4111 | `				 && &pArgStart[1] < pArgStop && (pArgStart[1].nType & PH7_TK_COLON) ){` |
|        41 | 4112 | `					char *zN = SyMemBackendStrDup(&pGen->pVm->sAllocator,` |
|        13 | 4113 | `						pArgStart->sData.zString,pArgStart->sData.nByte);` |
|        28 | 4114 | `					if( zN ){` |
|        28 | 4115 | `						SyStringInitFromBuf(&sArgRec.sName,zN,pArgStart->sData.nByte);` |
|        13 | 4116 | `					}` |
|        28 | 4117 | `					pArgStart += 2;` |
|        13 | 4118 | `				}` |
|       218 | 4119 | `				if( pArgStart < pArgStop ){` |
|         - | 4120 | `					SySet *pInstrContainer;` |
|         - | 4121 | `					const char *zCErr;` |
|       218 | 4122 | `					pGen->pIn = pArgStart;` |
|       218 | 4123 | `					pGen->pEnd = pArgStop;` |
|         - | 4124 | `					/* An attribute argument is a constant expression -- php applies the` |
|         - | 4125 | ``					 * same rules it applies to a class constant, `new` excepted (an`` |
|         - | 4126 | `					 * attribute argument takes one). This is the argument's own rule,` |
|         - | 4127 | `					 * not the malformed-group case a few lines up, so it IS a fatal. */` |
|       218 | 4128 | `					zCErr = PH7_GenStateConstExprError(pGen,1);` |
|       218 | 4129 | `					if( zCErr ){` |
|         3 | 4130 | `						rc = PH7_GenCompileError(pGen,E_ERROR,pArgStart->nLine,"%s",zCErr);` |
|         3 | 4131 | `						pGen->pIn = pSavedIn;` |
|         3 | 4132 | `						pGen->pEnd = pSavedEnd;` |
|         3 | 4133 | `						return rc == SXERR_ABORT ? SXERR_ABORT : SXRET_OK;` |
|         - | 4134 | `					}` |
|       216 | 4135 | `					pInstrContainer = PH7_VmGetByteCodeContainer(pGen->pVm);` |
|       216 | 4136 | `					PH7_VmSetByteCodeContainer(pGen->pVm,&sArgRec.aByteCode);` |
|       216 | 4137 | `					rc = PH7_CompileExpr(&(*pGen),EXPR_FLAG_COMMA_STATEMENT,0);` |
|       216 | 4138 | `					PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,1,0,0,0);` |
|       216 | 4139 | `					PH7_VmSetByteCodeContainer(pGen->pVm,pInstrContainer);` |
|       216 | 4140 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 4141 | `						pGen->pIn = pSavedIn;` |
|       ! 0 | 4142 | `						pGen->pEnd = pSavedEnd;` |
|       ! 0 | 4143 | `						return SXERR_ABORT;` |
|         - | 4144 | `					}` |
|       216 | 4145 | `					SySetPut(&sAttr.aArgs,(const void *)&sArgRec);` |
|       106 | 4146 | `				}` |
|       216 | 4147 | `				pIn = pArgStop;` |
|       216 | 4148 | `				if( pIn < pArgsEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|        84 | 4149 | `					pIn++;` |
|        41 | 4150 | `				}` |
|         4 | 4151 | `			}` |
|       136 | 4152 | `			pIn = (pArgsEnd < pEnd) ? &pArgsEnd[1] : pEnd;` |
|        66 | 4153 | `		}` |
|       669 | 4154 | `		SySetPut(pOut,(const void *)&sAttr);` |
|       669 | 4155 | `		if( pIn < pEnd && (pIn->nType & PH7_TK_COMMA) ){` |
|         5 | 4156 | `			pIn++;` |
|         5 | 4157 | `			continue;` |
|         - | 4158 | `		}` |
|       665 | 4159 | `		break;` |
|       ! 0 | 4160 | `	}` |
|       665 | 4161 | `	pGen->pIn = pSavedIn;` |
|       665 | 4162 | `	pGen->pEnd = pSavedEnd;` |
|       665 | 4163 | `	return SXRET_OK;` |
|       336 | 4164 | `}` |
|         - | 4165 | `/*` |
|         - | 4166 | ` * Hand the pending attribute groups (if any) to a declaration: compile` |
|         - | 4167 | ` * every recorded group into pOut and clear the pending list.` |
|         - | 4168 | ` */` |
|    199763 | 4169 | `PH7_PRIVATE sxi32 GenStateConsumeAttrs(ph7_gen_state *pGen,SySet *pOut)` |
|         5 | 4170 | `{` |
|    199768 | 4171 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aPendingAttrs);` |
|         - | 4172 | `	sxu32 n;` |
|         - | 4173 | `	sxi32 rc;` |
|    200384 | 4174 | `	for( n = 0 ; n < SySetUsed(&pGen->aPendingAttrs) ; n++ ){` |
|       621 | 4175 | `		rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|       621 | 4176 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 4177 | `			return SXERR_ABORT;` |
|         - | 4178 | `		}` |
|       313 | 4179 | `	}` |
|    199768 | 4180 | `	SySetReset(&pGen->aPendingAttrs);` |
|    199768 | 4181 | `	return SXRET_OK;` |
|     99743 | 4182 | `}` |
|         - | 4183 | `/*` |
|         - | 4184 | ` * Compile the attribute groups keyed to the given token (a parameter's` |
|         - | 4185 | ` * first token inside a signature) into pOut. Parameters are parsed from` |
|         - | 4186 | ` * the main token stream, so the sidecar indexes map directly.` |
|         - | 4187 | ` */` |
|    280305 | 4188 | `PH7_PRIVATE sxi32 GenStateCollectParamAttrs(ph7_gen_state *pGen,SyToken *pTok,SySet *pOut)` |
|         5 | 4189 | `{` |
|    280310 | 4190 | `	SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|    280310 | 4191 | `	ph7_trivia *aT = (ph7_trivia *)SySetBasePtr(&pGen->aTrivia);` |
|    280310 | 4192 | `	sxu32 nT = SySetUsed(&pGen->aTrivia);` |
|         - | 4193 | `	sxu32 nIdx, n;` |
|         - | 4194 | `	sxi32 rc;` |
|    280305 | 4195 | `	if( nT < 1 \|\| pGen->pTokenSet == 0` |
|      2109 | 4196 | `	 \|\| pTok < pBase \|\| pTok >= &pBase[SySetUsed(pGen->pTokenSet)] ){` |
|    278206 | 4197 | `		return SXRET_OK;` |
|         - | 4198 | `	}` |
|      2109 | 4199 | `	nIdx = (sxu32)(pTok - pBase);` |
|     14887 | 4200 | `	for( n = 0 ; n < nT ; n++ ){` |
|     12783 | 4201 | `		if( aT[n].nTokIdx == nIdx && aT[n].iKind == PH7_TRIVIA_ATTR ){` |
|        50 | 4202 | `			rc = GenStateCompileAttrSpan(&(*pGen),&aT[n],pOut);` |
|        50 | 4203 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 4204 | `				return SXERR_ABORT;` |
|         - | 4205 | `			}` |
|        23 | 4206 | `		}` |
|      6394 | 4207 | `	}` |
|      2109 | 4208 | `	return SXRET_OK;` |
|    139849 | 4209 | `}` |
|         - | 4210 | `/*` |
|         - | 4211 | ` * ---------------------------------------------------------------------------` |
|         - | 4212 | ` * Where php's OWN attributes may be written.` |
|         - | 4213 | ` *` |
|         - | 4214 | ` * php's seven internal attribute classes each carry a target mask and a` |
|         - | 4215 | ` * validator, and the engine runs them where the declaration COMPILES: a` |
|         - | 4216 | `` * misplaced `#[\Attribute]`, `#[\Override]` or `#[\NoDiscard]` is a fatal at`` |
|         - | 4217 | ` * the line it sits on, before anything else in the file runs. A USERLAND` |
|         - | 4218 | ` * attribute is different — php checks its mask only when someone asks for it,` |
|         - | 4219 | `` * at `newInstance()` — so this table is closed on purpose and unknown names go`` |
|         - | 4220 | ` * unchecked, which is php's behaviour and not an omission.` |
|         - | 4221 | ` *` |
|         - | 4222 | ` * The masks are the same seven the classes declare (see VmInstallAttributes);` |
|         - | 4223 | ` * they are repeated here because the compiler runs before any class exists.` |
|         - | 4224 | ` * None of the seven is IS_REPEATABLE, so a second one is php's own refusal.` |
|         - | 4225 | ` * ---------------------------------------------------------------------------` |
|         - | 4226 | ` */` |
|         - | 4227 | `/* php's Attribute::TARGET_* names, in bit order (TARGET_CLASS is bit 0). */` |
|         - | 4228 | `static const char *const azGenAttrTarget[] = {` |
|         - | 4229 | `	"class","function","method","property","class constant","parameter","constant"` |
|         - | 4230 | `};` |
|         - | 4231 | `static const struct {` |
|         - | 4232 | `	const char *zName;` |
|         - | 4233 | `	int iMask;` |
|         - | 4234 | `} aGenInternalAttr[] = {` |
|         - | 4235 | `	{ "Attribute",              1  },` |
|         - | 4236 | `	{ "Deprecated",             87 },` |
|         - | 4237 | `	{ "AllowDynamicProperties", 1  },` |
|         - | 4238 | `	{ "SensitiveParameter",     32 },` |
|         - | 4239 | `	{ "ReturnTypeWillChange",   4  },` |
|         - | 4240 | `	{ "Override",               12 },` |
|         - | 4241 | `	{ "NoDiscard",              6  },` |
|         - | 4242 | `};` |
|         - | 4243 | `/*` |
|         - | 4244 | ` * The extra validator php gives three of them, asked only once the target is` |
|         - | 4245 | ` * known to be a CLASS: the mask says "a class" and these say WHICH kinds.` |
|         - | 4246 | ` * Answers php's noun for the refused kind, or 0 when the class is acceptable.` |
|         - | 4247 | ` */` |
|        98 | 4248 | `static const char * GenStateAttrClassRefusal(const char *zAttr,sxi32 iFlags)` |
|         4 | 4249 | `{` |
|         - | 4250 | `	/* zAttr is a row of aGenInternalAttr, so an exact compare is the whole test. */` |
|       102 | 4251 | `	int bAttr = SyStrncmp(zAttr,"Attribute",sizeof("Attribute")) == 0;` |
|       102 | 4252 | `	int bDyn  = SyStrncmp(zAttr,"AllowDynamicProperties",sizeof("AllowDynamicProperties")) == 0;` |
|       102 | 4253 | `	int bDep  = SyStrncmp(zAttr,"Deprecated",sizeof("Deprecated")) == 0;` |
|       102 | 4254 | `	if( !bAttr && !bDyn && !bDep ){` |
|       ! 0 | 4255 | `		return 0;` |
|         - | 4256 | `	}` |
|       102 | 4257 | `	if( iFlags & PH7_CLASS_INTERFACE ){ return "interface"; }` |
|        96 | 4258 | `	if( iFlags & PH7_CLASS_ENUM ){ return "enum"; }` |
|        90 | 4259 | `	if( iFlags & PH7_CLASS_TRAIT ){` |
|         - | 4260 | `		/* php 8.5 DOES mark a deprecated trait; the other two refuse one. */` |
|         7 | 4261 | `		return bDep ? 0 : "trait";` |
|         - | 4262 | `	}` |
|        84 | 4263 | `	if( bAttr ){` |
|         - | 4264 | `		/* An attribute class must be instantiable. */` |
|        62 | 4265 | `		return (iFlags & PH7_CLASS_ABSTRACT) ? "abstract class" : 0;` |
|         - | 4266 | `	}` |
|        26 | 4267 | `	if( bDyn ){` |
|         - | 4268 | `		/* A readonly class has no dynamic property to allow. */` |
|        22 | 4269 | `		return (iFlags & PH7_CLASS_READONLY) ? "readonly class" : 0;` |
|         - | 4270 | `	}` |
|         5 | 4271 | `	return "class";   /* #[\Deprecated] on any other class kind */` |
|        53 | 4272 | `}` |
|         - | 4273 | `/*` |
|         - | 4274 | ` * Validate one declaration's attribute set against php's placement rules.` |
|         - | 4275 | ` *` |
|         - | 4276 | ` * iTarget is the single Attribute::TARGET_* bit php NAMES for this declaration` |
|         - | 4277 | ` * and iAccept the mask it accepts, which differ in exactly one place: a PROMOTED` |
|         - | 4278 | ` * constructor parameter is a parameter and a property both, so it takes either` |
|         - | 4279 | ` * bit while still reporting "parameter". pClassName/iClassFlags describe the` |
|         - | 4280 | ` * subject when the target is a class (0 and 0 otherwise).` |
|         - | 4281 | ` */` |
|    479836 | 4282 | `PH7_PRIVATE sxi32 GenStateCheckAttrPlacement(ph7_gen_state *pGen,SySet *pAttrs,` |
|         - | 4283 | `	int iTarget,int iAccept,const SyString *pClassName,sxi32 iClassFlags)` |
|         5 | 4284 | `{` |
|    479841 | 4285 | `	ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|         - | 4286 | `	sxu32 n,k;` |
|    480423 | 4287 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; ++n ){` |
|       663 | 4288 | `		SyString *pName = &aAttr[n].sName;` |
|         - | 4289 | `		sxu32 iRow;` |
|      3671 | 4290 | `		for( iRow = 0 ; iRow < SX_ARRAYSIZE(aGenInternalAttr) ; ++iRow ){` |
|      3524 | 4291 | `			if( pName->nByte == (sxu32)SyStrlen(aGenInternalAttr[iRow].zName)` |
|      2132 | 4292 | `			 && SyStrnicmp(pName->zString,aGenInternalAttr[iRow].zName,pName->nByte) == 0 ){` |
|       521 | 4293 | `				break;` |
|         - | 4294 | `			}` |
|      1509 | 4295 | `		}` |
|       663 | 4296 | `		if( iRow >= SX_ARRAYSIZE(aGenInternalAttr) ){` |
|       147 | 4297 | `			continue;   /* a userland attribute: judged at newInstance(), not here */` |
|         - | 4298 | `		}` |
|       521 | 4299 | `		if( (aGenInternalAttr[iRow].iMask & iAccept) == 0 ){` |
|         - | 4300 | `			SyBlob sAllowed;` |
|         - | 4301 | `			int iBit;` |
|         - | 4302 | `			sxi32 rc;` |
|        47 | 4303 | `			SyBlobInit(&sAllowed,&pGen->pVm->sAllocator);` |
|       369 | 4304 | `			for( iBit = 0 ; iBit < (int)SX_ARRAYSIZE(azGenAttrTarget) ; iBit++ ){` |
|       323 | 4305 | `				if( (aGenInternalAttr[iRow].iMask & (1 << iBit)) == 0 ){` |
|       239 | 4306 | `					continue;` |
|         - | 4307 | `				}` |
|        85 | 4308 | `				if( SyBlobLength(&sAllowed) > 0 ){` |
|        39 | 4309 | `					SyBlobAppend(&sAllowed,", ",sizeof(", ")-1);` |
|        19 | 4310 | `				}` |
|       127 | 4311 | `				SyBlobAppend(&sAllowed,azGenAttrTarget[iBit],` |
|        84 | 4312 | `					(sxu32)SyStrlen(azGenAttrTarget[iBit]));` |
|        43 | 4313 | `			}` |
|        47 | 4314 | `			SyBlobAppend(&sAllowed,"",sizeof(char));   /* NUL for the %s below */` |
|       171 | 4315 | `			for( iBit = 0 ; iBit < (int)SX_ARRAYSIZE(azGenAttrTarget) ; iBit++ ){` |
|       171 | 4316 | `				if( iTarget == (1 << iBit) ){` |
|        47 | 4317 | `					break;` |
|         - | 4318 | `				}` |
|        63 | 4319 | `			}` |
|        70 | 4320 | `			rc = PH7_GenCompileError(pGen,E_ERROR,aAttr[n].nLine,` |
|        23 | 4321 | `				"Attribute \"%z\" cannot target %s (allowed targets: %s)",pName,` |
|        23 | 4322 | `				iBit < (int)SX_ARRAYSIZE(azGenAttrTarget) ? azGenAttrTarget[iBit] : "",` |
|        23 | 4323 | `				SyBlobData(&sAllowed));` |
|        47 | 4324 | `			SyBlobRelease(&sAllowed);` |
|        47 | 4325 | `			return rc;` |
|         - | 4326 | `		}` |
|         - | 4327 | `		/* ...then repetition, which is what php checks second: the FIRST of a` |
|         - | 4328 | `		 * misplaced pair reports its target instead. */` |
|       475 | 4329 | `		for( k = 0 ; k < n ; ++k ){` |
|         6 | 4330 | `			if( aAttr[k].sName.nByte == pName->nByte` |
|         7 | 4331 | `			 && SyStrnicmp(aAttr[k].sName.zString,pName->zString,pName->nByte) == 0 ){` |
|        10 | 4332 | `				return PH7_GenCompileError(pGen,E_ERROR,aAttr[n].nLine,` |
|         3 | 4333 | `					"Attribute \"%z\" must not be repeated",pName);` |
|         - | 4334 | `			}` |
|       ! 0 | 4335 | `		}` |
|       469 | 4336 | `		if( iTarget == 1 && pClassName ){` |
|       151 | 4337 | `			const char *zRefused = GenStateAttrClassRefusal(aGenInternalAttr[iRow].zName,` |
|        49 | 4338 | `				iClassFlags);` |
|       102 | 4339 | `			if( zRefused ){` |
|        37 | 4340 | `				return PH7_GenCompileError(pGen,E_ERROR,aAttr[n].nLine,` |
|        24 | 4341 | `					"Cannot apply #[\\%s] to %s %z",aGenInternalAttr[iRow].zName,` |
|        12 | 4342 | `					zRefused,pClassName);` |
|         - | 4343 | `			}` |
|        37 | 4344 | `		}` |
|       225 | 4345 | `	}` |
|    479765 | 4346 | `	return SXRET_OK;` |
|    239471 | 4347 | `}` |
|         - | 4348 | `/*` |
|         - | 4349 | `` * php 8.5's `(void)` cast is a STATEMENT prefix, not an expression operator:`` |
|         - | 4350 | `` * `$x = (void) f();` and `return (void) f();` are parse errors there too, and`` |
|         - | 4351 | ` * the only thing it does is say that dropping the answer is DELIBERATE, which` |
|         - | 4352 | ` * silences a #[\NoDiscard] callee. The lexer already assembled the three tokens` |
|         - | 4353 | ` * into one (PH7_TK_VOID_CAST); this consumes it and answers 1.` |
|         - | 4354 | ` */` |
|    997168 | 4355 | `PH7_PRIVATE int GenStateTakeVoidCast(ph7_gen_state *pGen)` |
|         5 | 4356 | `{` |
|    997173 | 4357 | `	if( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_VOID_CAST) ){` |
|        19 | 4358 | `		pGen->pIn++;` |
|        19 | 4359 | `		return 1;` |
|         - | 4360 | `	}` |
|    997155 | 4361 | `	return 0;` |
|    497680 | 4362 | `}` |
|         - | 4363 | `/*` |
|         - | 4364 | `` * php's grammar takes a `(void)` cast at the head of an expression STATEMENT and`` |
|         - | 4365 | `` * at the head of each element of a `for` clause list — `for ((void) f(), $i = 0;`` |
|         - | 4366 | `` * $i < 1; $i++, (void) g())` is all valid, while `for ($i = (void) f();;)` is`` |
|         - | 4367 | ` * not. The statement head is consumed by GenStateTakeVoidCast; a clause is one` |
|         - | 4368 | ` * expression with comma operators in it, so its element heads are marked HERE,` |
|         - | 4369 | ` * before it compiles: the token becomes the no-op cast operator parse.c declares,` |
|         - | 4370 | `` * and every other `(void)` in the clause stays unrecognized, which is php's own`` |
|         - | 4371 | ` * refusal. Nothing is moved or removed — the token stream is shared with the` |
|         - | 4372 | ` * rest of the file.` |
|         - | 4373 | ` */` |
|    144415 | 4374 | `PH7_PRIVATE int GenStateEnableClauseVoidCasts(ph7_gen_state *pGen,int bLastToo)` |
|         5 | 4375 | `{` |
|    144420 | 4376 | `	SyToken *pTok = pGen->pIn,*pLastMark = 0;` |
|    144420 | 4377 | `	int iDepth = 0,bHead = 1,bCommaAfter = 0;` |
|    824948 | 4378 | `	for( ; pTok < pGen->pEnd ; pTok++ ){` |
|    776811 | 4379 | `		if( pTok->nType & (PH7_TK_LPAREN\|PH7_TK_OSB\|PH7_TK_OCB) ){` |
|     23824 | 4380 | `			iDepth++;` |
|    764885 | 4381 | `		}else if( pTok->nType & (PH7_TK_RPAREN\|PH7_TK_CSB\|PH7_TK_CCB) ){` |
|     23824 | 4382 | `			iDepth--;` |
|    741066 | 4383 | `		}else if( iDepth == 0 && (pTok->nType & PH7_TK_SEMI) ){` |
|         - | 4384 | `			/* The three clauses share one token range (only the post one is` |
|         - | 4385 | `			 * delimited), so this scan stops where its own clause does. */` |
|     48075 | 4386 | `			break;` |
|    632895 | 4387 | `		}else if( iDepth == 0 && (pTok->nType & PH7_TK_COMMA) ){` |
|        14 | 4388 | `			bHead = 1;` |
|        14 | 4389 | `			bCommaAfter = 1;` |
|        14 | 4390 | `			continue;` |
|    632883 | 4391 | `		}else if( iDepth == 0 && bHead && (pTok->nType & PH7_TK_VOID_CAST) ){` |
|         9 | 4392 | `			pTok->nType \|= PH7_TK_OP;` |
|         9 | 4393 | `			pTok->pUserData = (void *)PH7_ExprExtractOperator(&pTok->sData,0);` |
|         9 | 4394 | `			pLastMark = pTok;` |
|         9 | 4395 | `			bCommaAfter = 0;` |
|         4 | 4396 | `		}` |
|    680521 | 4397 | `		bHead = 0;` |
|    339779 | 4398 | `	}` |
|         - | 4399 | `	/* The CONDITION clause's last element is the condition VALUE, so php refuses a` |
|         - | 4400 | ``	 * `(void)` on that one and only that one: `for (;(void) f();)` is a parse error`` |
|         - | 4401 | ``	 * where `for (;(void) f(), $i < 1;)` is fine. */`` |
|    144420 | 4402 | `	if( !bLastToo && pLastMark && !bCommaAfter ){` |
|         3 | 4403 | `		pLastMark->nType &= ~(sxu32)PH7_TK_OP;` |
|         3 | 4404 | `		pLastMark->pUserData = 0;` |
|         3 | 4405 | `		return 1;` |
|         - | 4406 | `	}` |
|    144418 | 4407 | `	return 0;` |
|     72109 | 4408 | `}` |
|         - | 4409 | `/*` |
|         - | 4410 | ` * The statement is about to throw its expression's value away. When that value` |
|         - | 4411 | ` * came straight out of a CALL, mark the call: php's !RETURN_VALUE_USED, which is` |
|         - | 4412 | `` * what a #[\NoDiscard] callee reads. `f() + 1;` drops the ADD's result, not the`` |
|         - | 4413 | ` * call's, so only the last instruction is looked at.` |
|         - | 4414 | ` */` |
|   1093122 | 4415 | `PH7_PRIVATE void GenStateMarkDiscardedCall(ph7_gen_state *pGen)` |
|         5 | 4416 | `{` |
|   1093127 | 4417 | `	VmInstr *pInstr = PH7_VmPeekInstr(pGen->pVm);` |
|   1093127 | 4418 | `	if( pInstr && pInstr->iOp == PH7_OP_CALL ){` |
|    155794 | 4419 | `		pInstr->bDiscard = 1;` |
|     77596 | 4420 | `	}` |
|   1093127 | 4421 | `}` |
|         - | 4422 | `/* TRUE when the cursor has run past the LAST token of a chunk that met the end of` |
|         - | 4423 | ``  * the file. A statement slice can end early (a single statement inside a `for` `` |
|         - | 4424 | `` * header), so `pIn >= pEnd` alone is not the question. */`` |
|        58 | 4425 | `static int GenStateAtChunkEof(ph7_gen_state *pGen)` |
|         3 | 4426 | `{` |
|         - | 4427 | `	SyToken *pBase;` |
|        61 | 4428 | `	if( !pGen->bChunkAtEof \|\| pGen->pTokenSet == 0 ){` |
|        10 | 4429 | `		return 0;` |
|         - | 4430 | `	}` |
|        51 | 4431 | `	pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        51 | 4432 | `	return pGen->pIn >= &pBase[SySetUsed(pGen->pTokenSet)];` |
|        32 | 4433 | `}` |
|         - | 4434 | `/*` |
|         - | 4435 | ` * php's grammar wants a TERMINATOR after a statement, and the end of the file is` |
|         - | 4436 | `` * not one: `<?php echo "a"` is `syntax error, unexpected end of file, expecting`` |
|         - | 4437 | `` * "," or ";"` there, while PHL ran it and exited 0. (A `?>` IS a terminator, which`` |
|         - | 4438 | `` * is why `<?php echo "a" ?>` is legal in both engines and why the check only`` |
|         - | 4439 | ` * applies to a chunk that met the end of the FILE.)` |
|         - | 4440 | ` *` |
|         - | 4441 | `` * A statement that ends in `}` -- a block, a declaration, a braced control`` |
|         - | 4442 | `` * structure -- needs nothing, and neither does one whose `;` the loop just`` |
|         - | 4443 | ` * stepped over; everything else was left unfinished.` |
|         - | 4444 | ` *` |
|         - | 4445 | ` * Answers the "expecting" clause php names for the statement that ran out, or 0` |
|         - | 4446 | ` * when php names none. php reports the set its parser was in, which for a` |
|         - | 4447 | ` * statement is decided by the KEYWORD it opened with -- the comma-list statements` |
|         - | 4448 | `` * may take another element, `return`/`break`/`continue`/`goto`/`unset` and a`` |
|         - | 4449 | `` * do-while may not, `namespace` still wants its block -- except when the last`` |
|         - | 4450 | `` * token consumed was an alternative-syntax `end*`, whose own `;` is what is`` |
|         - | 4451 | ` * missing.` |
|         - | 4452 | ` */` |
|        50 | 4453 | `static const char * GenStateEofExpecting(SyToken *pStmt,SyToken *pLast)` |
|         1 | 4454 | `{` |
|         - | 4455 | `	sxu32 nKw;` |
|        51 | 4456 | `	if( pLast && (pLast->nType & PH7_TK_KEYWORD) ){` |
|         6 | 4457 | `		nKw = (sxu32)SX_PTR_TO_INT(pLast->pUserData);` |
|         6 | 4458 | `		if( nKw == PH7_TKWRD_ENDIF \|\| nKw == PH7_TKWRD_ENDWHILE \|\| nKw == PH7_TKWRD_ENDFOR` |
|         2 | 4459 | `		 \|\| nKw == PH7_TKWRD_END4EACH \|\| nKw == PH7_TKWRD_ENDSWITCH ){` |
|         4 | 4460 | `			return "\";\"";` |
|         - | 4461 | `		}` |
|         1 | 4462 | `	}` |
|        47 | 4463 | `	if( pStmt == 0 \|\| (pStmt->nType & PH7_TK_KEYWORD) == 0 ){` |
|        13 | 4464 | `		return 0;` |
|         - | 4465 | `	}` |
|        34 | 4466 | `	nKw = (sxu32)SX_PTR_TO_INT(pStmt->pUserData);` |
|        34 | 4467 | `	if( nKw == PH7_TKWRD_ECHO \|\| nKw == PH7_TKWRD_GLOBAL \|\| nKw == PH7_TKWRD_STATIC` |
|        23 | 4468 | `	 \|\| nKw == PH7_TKWRD_CONST \|\| nKw == PH7_TKWRD_USE ){` |
|        16 | 4469 | `		return "\",\" or \";\"";` |
|         - | 4470 | `	}` |
|        18 | 4471 | `	if( nKw == PH7_TKWRD_RETURN \|\| nKw == PH7_TKWRD_BREAK \|\| nKw == PH7_TKWRD_CONTINUE` |
|        14 | 4472 | `	 \|\| nKw == PH7_TKWRD_GOTO \|\| nKw == PH7_TKWRD_UNSET \|\| nKw == PH7_TKWRD_DO ){` |
|        10 | 4473 | `		return "\";\"";` |
|         - | 4474 | `	}` |
|         8 | 4475 | `	if( nKw == PH7_TKWRD_NAMESPACE ){` |
|         2 | 4476 | `		return "\"{\"";` |
|         - | 4477 | `	}` |
|         6 | 4478 | `	return 0;` |
|        26 | 4479 | `}` |
|         - | 4480 | ``/* Does this script spell `__halt_compiler` at all, in any case? A cheap scan`` |
|         - | 4481 | ` * that keeps the token pass below off every ordinary file. */` |
|     37480 | 4482 | `static int GenStateMentionsHalt(const char *zIn,sxu32 nIn)` |
|         5 | 4483 | `{` |
|         - | 4484 | `	static const char zWord[] = "__halt_compiler";` |
|     37485 | 4485 | `	sxu32 nWord = (sxu32)sizeof(zWord)-1;` |
|         - | 4486 | `	sxu32 i;` |
| 149010014 | 4487 | `	for( i = 0 ; i + nWord <= nIn ; ++i ){` |
| 148972579 | 4488 | `		if( zIn[i] != '_' ){` |
| 147664232 | 4489 | `			continue;` |
|         - | 4490 | `		}` |
|   1308352 | 4491 | `		if( SyStrnicmp(&zIn[i],zWord,nWord) == 0 ){` |
|        47 | 4492 | `			return 1;` |
|         - | 4493 | `		}` |
|    652961 | 4494 | `	}` |
|     37440 | 4495 | `	return 0;` |
|     18734 | 4496 | `}` |
|         - | 4497 | ``/* Is this token the `__halt_compiler` identifier? It is not a keyword in this`` |
|         - | 4498 | ` * lexer (the generated table takes nothing longer than twelve bytes), so it` |
|         - | 4499 | ` * arrives as an ordinary identifier and is recognised by NAME -- case` |
|         - | 4500 | ` * insensitively, as php's own scanner does. */` |
|   2438499 | 4501 | `static int GenStateIsHaltCompiler(SyToken *pTok,SyToken *pEnd)` |
|         5 | 4502 | `{` |
|   2433675 | 4503 | `	return pTok < pEnd` |
|   2438499 | 4504 | `	    && (pTok->nType & PH7_TK_ID)` |
|   1293130 | 4505 | `	    && pTok->sData.nByte == sizeof("__halt_compiler")-1` |
|   3660163 | 4506 | `	    && SyStrnicmp(pTok->sData.zString,"__halt_compiler",sizeof("__halt_compiler")-1) == 0;` |
|         5 | 4507 | `}` |
|         - | 4508 | `/*` |
|         - | 4509 | `` * The pre-scan behind `__COMPILER_HALT_OFFSET__`: find the halt statement in`` |
|         - | 4510 | `` * whichever PHP chunk holds it and remember the byte just past its `;`. The`` |
|         - | 4511 | ` * chunks are tokenized a second time here -- the compile below tokenizes each` |
|         - | 4512 | ` * one as it reaches it -- because the constant's value has to be known before` |
|         - | 4513 | ` * the first statement compiles. Only a file that spells the identifier gets` |
|         - | 4514 | ` * here at all.` |
|         - | 4515 | ` *` |
|         - | 4516 | `` * A `__halt_compiler` in a scope php refuses is still found: the statement`` |
|         - | 4517 | ` * compiler raises php's fatal when it reaches it, and the offset is never read.` |
|         - | 4518 | ` */` |
|        45 | 4519 | `static void GenStateScanHaltOffset(ph7_gen_state *pGen,SySet *pRawToken,const char *zFileBase)` |
|         2 | 4520 | `{` |
|        47 | 4521 | `	SyToken *pRaw = (SyToken *)SySetBasePtr(pRawToken);` |
|        47 | 4522 | `	SyToken *pRawEnd = &pRaw[SySetUsed(pRawToken)];` |
|         - | 4523 | `	SySet aTok,aTriv;` |
|        47 | 4524 | `	SySetInit(&aTok,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|        47 | 4525 | `	SySetInit(&aTriv,&pGen->pVm->sAllocator,sizeof(SyToken));` |
|       141 | 4526 | `	for( ; pRaw < pRawEnd && pGen->bHaltSeen == 0 ; pRaw++ ){` |
|         - | 4527 | `		SyToken *pTok,*pEnd;` |
|        96 | 4528 | `		if( (pRaw->nType & PH7_TOKEN_PHP) == 0 ){` |
|        49 | 4529 | `			continue;` |
|         - | 4530 | `		}` |
|        49 | 4531 | `		SySetReset(&aTok);` |
|        49 | 4532 | `		SySetReset(&aTriv);` |
|        72 | 4533 | `		PH7_TokenizePHP(SyStringData(&pRaw->sData),SyStringLength(&pRaw->sData),` |
|        23 | 4534 | `			pRaw->nLine,&aTok,&aTriv);` |
|        49 | 4535 | `		pTok = (SyToken *)SySetBasePtr(&aTok);` |
|        49 | 4536 | `		pEnd = &pTok[SySetUsed(&aTok)];` |
|      7664 | 4537 | `		for( ; pTok < pEnd ; pTok++ ){` |
|      7651 | 4538 | `			if( !GenStateIsHaltCompiler(pTok,pEnd) ){` |
|      7617 | 4539 | `				continue;` |
|         - | 4540 | `			}` |
|        34 | 4541 | `			if( pTok + 3 < pEnd` |
|        32 | 4542 | `			 && (pTok[1].nType & PH7_TK_LPAREN)` |
|        30 | 4543 | `			 && (pTok[2].nType & PH7_TK_RPAREN)` |
|        31 | 4544 | `			 && (pTok[3].nType & PH7_TK_SEMI) ){` |
|        31 | 4545 | `				const char *zSemi = SyStringData(&pTok[3].sData);` |
|        31 | 4546 | `				if( zSemi > zFileBase ){` |
|        31 | 4547 | `					pGen->nHaltOffset = (sxu32)((zSemi - zFileBase) + 1);` |
|        31 | 4548 | `					pGen->bHaltSeen = 1;` |
|        15 | 4549 | `				}` |
|        15 | 4550 | `			}` |
|        35 | 4551 | `			break;` |
|       ! 0 | 4552 | `		}` |
|        25 | 4553 | `	}` |
|        47 | 4554 | `	SySetRelease(&aTok);` |
|        47 | 4555 | `	SySetRelease(&aTriv);` |
|        47 | 4556 | `}` |
|         - | 4557 | `/*` |
|         - | 4558 | ` * ---------------------------------------------------------------------------` |
|         - | 4559 | ``  * `__halt_compiler();` `` |
|         - | 4560 | ` *` |
|         - | 4561 | ` * php's scanner STOPS at it: the rest of the file is not code and is never` |
|         - | 4562 | ` * output either, which is what lets a .phar carry a binary archive in the bytes` |
|         - | 4563 | ` * behind its stub. Three rules come with it, all php's:` |
|         - | 4564 | ` *` |
|         - | 4565 | ` *   - it is only legal at the OUTERMOST scope -- inside a function, a class or` |
|         - | 4566 | `` *     even a plain `if` block it is a compile-time fatal, not a parse error;`` |
|         - | 4567 | ` *   - the parentheses and the semicolon are part of the construct, and php's` |
|         - | 4568 | ` *     parser names what it wanted when one is missing;` |
|         - | 4569 | `` *   - `__COMPILER_HALT_OFFSET__` expands to the byte just past that `;`.`` |
|         - | 4570 | ` *` |
|         - | 4571 | ` * It is NOT a keyword in this lexer (the generated table takes nothing longer` |
|         - | 4572 | ` * than twelve bytes), so it arrives as an ordinary identifier in statement` |
|         - | 4573 | ` * position and is recognised by name -- case-insensitively, as php does.` |
|         - | 4574 | ` * ---------------------------------------------------------------------------` |
|         - | 4575 | ` */` |
|        96 | 4576 | `static sxi32 GenStateCompileHaltCompiler(ph7_gen_state *pGen)` |
|         1 | 4577 | `{` |
|        97 | 4578 | `	sxu32 nLine = pGen->pIn->nLine;` |
|        97 | 4579 | `	if( pGen->pCurrent != &pGen->sGlobal ){` |
|         - | 4580 | `		/* php's own sentence: a FATAL rather than a parse error, and one its` |
|         - | 4581 | `		 * PARSER makes -- so it prints no stack trace under it. */` |
|        67 | 4582 | `		pGen->iFatalTrace = PH7_FATAL_TRACE_NONE;` |
|        67 | 4583 | `		return PH7_GenCompileError(&(*pGen),E_ERROR,nLine,` |
|         - | 4584 | `			"__HALT_COMPILER() can only be used from the outermost scope");` |
|         - | 4585 | `	}` |
|        31 | 4586 | `	pGen->pIn++;` |
|        31 | 4587 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_LPAREN) == 0 ){` |
|         3 | 4588 | `		return PH7_GenSyntaxError(&(*pGen),` |
|         2 | 4589 | `			pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\"(\"");` |
|         - | 4590 | `	}` |
|         - | 4591 | ``	/* php's scanner ran out INSIDE the parentheses: it names the `(` it never`` |
|         - | 4592 | `	 * closed rather than the token it wanted, and reports it where the INPUT` |
|         - | 4593 | ``	 * ends rather than where the `(` is. */`` |
|         - | 4594 | `#define PHL_HALT_EOF_LINE (pGen->bChunkAtEof && pGen->nChunkEofLine > nLine \` |
|         - | 4595 | `	? pGen->nChunkEofLine : nLine)` |
|        29 | 4596 | `	if( pGen->pIn >= pGen->pEnd ){` |
|       ! 0 | 4597 | `		return PH7_GenCompileError(&(*pGen),E_PARSE,PHL_HALT_EOF_LINE,` |
|       ! 0 | 4598 | `			"Unclosed '(' on line %u",nLine);` |
|         - | 4599 | `	}` |
|        29 | 4600 | `	pGen->pIn++;` |
|        29 | 4601 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_RPAREN) == 0 ){` |
|         4 | 4602 | `		return pGen->pIn >= pGen->pEnd` |
|         2 | 4603 | `			? PH7_GenCompileError(&(*pGen),E_PARSE,PHL_HALT_EOF_LINE,` |
|         1 | 4604 | `				"Unclosed '(' on line %u",nLine)` |
|         2 | 4605 | `			: PH7_GenSyntaxError(&(*pGen),pGen->pIn,"\")\"");` |
|         - | 4606 | `	}` |
|        27 | 4607 | `	pGen->pIn++;` |
|        27 | 4608 | `	if( pGen->pIn >= pGen->pEnd \|\| (pGen->pIn->nType & PH7_TK_SEMI) == 0 ){` |
|       ! 0 | 4609 | `		return PH7_GenSyntaxError(&(*pGen),` |
|       ! 0 | 4610 | `			pGen->pIn < pGen->pEnd ? pGen->pIn : 0,"\";\"");` |
|         - | 4611 | `	}` |
|        27 | 4612 | `	pGen->pIn++;` |
|         - | 4613 | `	/* Nothing after it is code, in this chunk or in any that follows. */` |
|        27 | 4614 | `	pGen->pIn = pGen->pEnd;` |
|        27 | 4615 | `	pGen->bHalted = 1;` |
|         - | 4616 | `#undef PHL_HALT_EOF_LINE` |
|        27 | 4617 | `	return SXRET_OK;` |
|        49 | 4618 | `}` |
|   2171107 | 4619 | `PH7_PRIVATE sxi32 GenStateCompileChunk(` |
|         - | 4620 | `	ph7_gen_state *pGen, /* Code generator state */` |
|         - | 4621 | `	sxi32 iFlags         /* Compile flags */` |
|         - | 4622 | `	)` |
|         5 | 4623 | `{` |
|         - | 4624 | `	ProcLangConstruct xCons;` |
|         - | 4625 | `	sxi32 rc;` |
|   2171112 | 4626 | `	rc = SXRET_OK; /* Prevent compiler warning */` |
|   1377014 | 4627 | `	for(;;){` |
|   2464125 | 4628 | `		int bStmtIsDeclare = 0;` |
|   2464125 | 4629 | `		int bStmtIsNamespace = 0;` |
|         - | 4630 | `		int bStmtIsNop;` |
|         - | 4631 | `		/* Whether php's grammar wants a TERMINATOR after this statement: a block, a` |
|         - | 4632 | `		 * declaration and a LABEL end themselves, everything else -- an expression` |
|         - | 4633 | ``		 * statement included, even one that ends in the `}` of a closure or a match`` |
|         - | 4634 | `		 * -- has to be closed. */` |
|   2464125 | 4635 | `		int bStmtWantsSemi = 1;` |
|         - | 4636 | `		SyToken *pStmtStart;` |
|   2464125 | 4637 | `		if( pGen->pIn >= pGen->pEnd ){` |
|         - | 4638 | `			/* No more input to process */` |
|     33281 | 4639 | `			break;` |
|         - | 4640 | `		}` |
|   2430849 | 4641 | `		pStmtStart = pGen->pIn; /* The keyword this statement opened with, for the` |
|         - | 4642 | `		                         * end-of-input check below */` |
|         - | 4643 | `		/* Bind a directly-preceding docblock to this statement */` |
|   2430849 | 4644 | `		GenStateSetPendingDoc(&(*pGen));` |
|   2430849 | 4645 | `		if( SySetUsed(&pGen->aPendingAttrs) > 0 ){` |
|         - | 4646 | `			/* php: a statement-position attribute group must be followed by a` |
|         - | 4647 | ``			 * declaration (function/class-like/const) — `#[A] $x = 1;` is a`` |
|         - | 4648 | `` 			 * parse error, never a silent discard. `static`/`fn`/`function` `` |
|         - | 4649 | ``			 * cover bare closure-expression statements; `readonly`/`enum` are`` |
|         - | 4650 | `			 * context-sensitive IDs handled by the modified-class/enum scans. */` |
|       389 | 4651 | `			int bAttrTarget = 0;` |
|       384 | 4652 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd)` |
|       384 | 4653 | `			 \|\| GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|        17 | 4654 | `				bAttrTarget = 1;` |
|       381 | 4655 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|       373 | 4656 | `				sxu32 nKw = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|       368 | 4657 | `				if( nKw == PH7_TKWRD_FUNCTION \|\| nKw == PH7_TKWRD_CLASS` |
|        94 | 4658 | `				 \|\| nKw == PH7_TKWRD_INTERFACE \|\| nKw == PH7_TKWRD_TRAIT` |
|        11 | 4659 | `				 \|\| nKw == PH7_TKWRD_ABSTRACT \|\| nKw == PH7_TKWRD_FINAL` |
|         8 | 4660 | `				 \|\| nKw == PH7_TKWRD_CONST \|\| nKw == PH7_TKWRD_STATIC` |
|         5 | 4661 | `				 \|\| nKw == PH7_TKWRD_FN ){` |
|       373 | 4662 | `					bAttrTarget = 1;` |
|       184 | 4663 | `				}` |
|       184 | 4664 | `			}` |
|       389 | 4665 | `			if( !bAttrTarget ){` |
|       ! 0 | 4666 | `				rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|         - | 4667 | `					"syntax error, unexpected token \"%z\" after attribute group; expecting a declaration",` |
|       ! 0 | 4668 | `					&pGen->pIn->sData);` |
|       ! 0 | 4669 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 4670 | `					break;` |
|         - | 4671 | `				}` |
|       ! 0 | 4672 | `				SySetReset(&pGen->aPendingAttrs);` |
|       ! 0 | 4673 | `			}` |
|       192 | 4674 | `		}` |
|         - | 4675 | ``		/* Peek to detect a top-level `declare` so the strict_types lock`` |
|         - | 4676 | `		 * below doesn't fire before the directive has a chance to run. */` |
|   2430849 | 4677 | `		if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   1434325 | 4678 | `			sxu32 nPeek = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|   1434325 | 4679 | `			if( nPeek == PH7_TKWRD_DECLARE ){` |
|        69 | 4680 | `				bStmtIsDeclare = 1;` |
|   1434293 | 4681 | `			}else if( nPeek == PH7_TKWRD_NAMESPACE && !GenStateIsNsRelName(pGen->pIn,pGen->pEnd) ){` |
|         - | 4682 | `				/* A namespace DECLARATION asks the lock itself -- php's rule is that` |
|         - | 4683 | `				 * the first one must be the first statement, nops and declares` |
|         - | 4684 | `				 * aside -- and then sets it, since it is code for the declares` |
|         - | 4685 | `				 * after it. */` |
|       403 | 4686 | `				bStmtIsNamespace = 1;` |
|       199 | 4687 | `			}` |
|    716142 | 4688 | `		}` |
|         - | 4689 | `` 		/* php's zend_is_first_statement walks past a null statement: an empty `;` `` |
|         - | 4690 | ``		 * before `declare(strict_types=1)` or before `namespace` is not code. */`` |
|   2430849 | 4691 | `		bStmtIsNop = (pGen->pIn->nType & PH7_TK_SEMI) != 0;` |
|   2430849 | 4692 | `		if( !bStmtIsDeclare && !bStmtIsNamespace && !bStmtIsNop && pGen->pCurrent == &pGen->sGlobal ){` |
|         - | 4693 | `			/* Any non-declare top-level statement locks the strict_types` |
|         - | 4694 | `			 * directive: it's now too late for declare(strict_types=1). */` |
|    292670 | 4695 | `			pGen->bStrictTypesLocked = 1;` |
|    145947 | 4696 | `		}` |
|   2430844 | 4697 | `		if( pGen->pCurrent == &pGen->sGlobal && pGen->bNsBracketed && !pGen->bInNsBlock` |
|        55 | 4698 | `		 && !bStmtIsNamespace && !bStmtIsNop && !GenStateIsHaltCompiler(pGen->pIn,pGen->pEnd) ){` |
|         - | 4699 | `			/* php's zend_verify_namespace: once a file has used the bracketed form,` |
|         - | 4700 | ``			 * every statement outside a block but another `namespace` (or the halt)`` |
|         - | 4701 | `			 * is a compile fatal. */` |
|         5 | 4702 | `			rc = PH7_GenCompileError(pGen,E_ERROR,pGen->pIn->nLine,` |
|         - | 4703 | `				"No code may exist outside of namespace {}");` |
|         5 | 4704 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 4705 | `				break;` |
|         - | 4706 | `			}` |
|         2 | 4707 | `		}` |
|   2430849 | 4708 | `		if( GenStateIsHaltCompiler(pGen->pIn,pGen->pEnd) ){` |
|         - | 4709 | `			/* Everything from here on is DATA. php's own scanner stops in exactly` |
|         - | 4710 | `			 * the same place, which is what lets a .phar carry its archive in the` |
|         - | 4711 | `			 * bytes after its stub. */` |
|        97 | 4712 | `			rc = GenStateCompileHaltCompiler(&(*pGen));` |
|        97 | 4713 | `			break;` |
|         - | 4714 | `		}` |
|   2430753 | 4715 | `		if( pGen->pIn->nType & PH7_TK_OCB /* '{' */ ){` |
|         - | 4716 | `			/* Compile block */` |
|        29 | 4717 | `			bStmtWantsSemi = 0;` |
|        29 | 4718 | `			rc = PH7_CompileBlock(&(*pGen),0);` |
|        29 | 4719 | `			if( rc == SXERR_ABORT ){` |
|       ! 0 | 4720 | `				break;` |
|         - | 4721 | `			}` |
|        17 | 4722 | `		}else{` |
|   2430729 | 4723 | `			xCons = 0;` |
|   2430729 | 4724 | `			if( GenStateStartsModifiedClass(pGen->pIn,pGen->pEnd) ){` |
|         - | 4725 | ``				/* `final`/`abstract`/`readonly` (any order) before `class`. Handled`` |
|         - | 4726 | `` 				 * here rather than the keyword-only dispatcher because `readonly` `` |
|         - | 4727 | `				 * is a context-sensitive ID and combos need a full-run scan. */` |
|       233 | 4728 | `				xCons = PH7_CompileClassModifiers;` |
|   2430615 | 4729 | `			}else if( GenStateStartsEnumDecl(pGen->pIn,pGen->pEnd) ){` |
|         - | 4730 | ``				/* `enum Name …` (PHP 8.1) — `enum` is a context-sensitive ID,`` |
|         - | 4731 | `				 * so it is detected here rather than the keyword dispatcher. */` |
|       145 | 4732 | `				xCons = PH7_CompileEnum;` |
|   2430431 | 4733 | `			}else if( GenStateStartsClosureExpr(pGen->pIn,pGen->pEnd) ){` |
|         - | 4734 | ``				/* `function () {…};` / `fn (…) => …;` at STATEMENT position is an`` |
|         - | 4735 | ``				 * expression statement in php, not a declaration — the `(` where a`` |
|         - | 4736 | `				 * named function has its name is what says so. */` |
|        16 | 4737 | `				xCons = 0;` |
|   2430354 | 4738 | `			}else if( GenStateIsNsRelName(pGen->pIn,pGen->pEnd) ){` |
|         - | 4739 | ``				/* A statement that STARTS with php's `namespace\X` name operator`` |
|         - | 4740 | ``				 * (`namespace\Cee::m();`) is an expression, not a namespace`` |
|         - | 4741 | ``				 * DECLARATION — the glued `\` is what tells the two apart. */`` |
|        10 | 4742 | `				xCons = 0;` |
|   2430343 | 4743 | `			}else if( pGen->pIn->nType & PH7_TK_KEYWORD ){` |
|   1434109 | 4744 | `				sxu32 nKeyword = (sxu32)SX_PTR_TO_INT(pGen->pIn->pUserData);` |
|         - | 4745 | `				/* Try to extract a language construct handler */` |
|   1434109 | 4746 | `				xCons = GenStateGetStatementHandler(nKeyword,(&pGen->pIn[1] < pGen->pEnd) ? &pGen->pIn[1] : 0);` |
|   1434109 | 4747 | `				if( xCons == 0 && GenStateisLangConstruct(nKeyword) == FALSE ){` |
|        13 | 4748 | `					rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|         - | 4749 | `						"Syntax error: Unexpected keyword '%z'",` |
|         8 | 4750 | `						&pGen->pIn->sData);` |
|         9 | 4751 | `					if( rc == SXERR_ABORT ){` |
|       ! 0 | 4752 | `						break;` |
|         - | 4753 | `					}` |
|         - | 4754 | `					/* Synchronize with the first semi-colon and avoid compiling` |
|         - | 4755 | `					 * this erroneous statement.` |
|         - | 4756 | `					 */` |
|         9 | 4757 | `					xCons = PH7_ErrorRecover;` |
|         4 | 4758 | `				}` |
|   1712269 | 4759 | `			}else if( (pGen->pIn->nType & PH7_TK_ID) && (&pGen->pIn[1] < pGen->pEnd)` |
|    149816 | 4760 | `				&& (pGen->pIn[1].nType & PH7_TK_COLON /*':'*/) ){` |
|         - | 4761 | `				/* Label found [i.e: Out: ],point to the routine responsible of compiling it */` |
|       225 | 4762 | `				xCons = PH7_CompileLabel;` |
|       110 | 4763 | `			}` |
|   2430729 | 4764 | `			if( xCons == 0 ){` |
|         - | 4765 | `				/* Assume an expression an try to compile it. A leading php 8.5` |
|         - | 4766 | ``				 * `(void)` cast is consumed here — statement head is one of the two`` |
|         - | 4767 | `				 * places its grammar takes one — and says the answer is dropped` |
|         - | 4768 | `				 * DELIBERATELY, so the call below is not marked. */` |
|    997173 | 4769 | `				int bVoid = GenStateTakeVoidCast(&(*pGen));` |
|    997173 | 4770 | `				if( bVoid && pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|         - | 4771 | `` 					/* php's grammar wants an expression after the cast: `(void);` `` |
|         - | 4772 | ``					 * is `syntax error, unexpected token ";"` there. */`` |
|         3 | 4773 | `					rc = PH7_GenCompileError(pGen,E_PARSE,pGen->pIn->nLine,` |
|         - | 4774 | `						"syntax error, unexpected token \";\"");` |
|         2 | 4775 | `				}else{` |
|    997171 | 4776 | `					rc = PH7_CompileExpr(&(*pGen),0,0);` |
|    997171 | 4777 | `					if( rc != SXERR_EMPTY ){` |
|    996859 | 4778 | `						if( !bVoid ){` |
|    996845 | 4779 | `							GenStateMarkDiscardedCall(&(*pGen));` |
|    497511 | 4780 | `						}` |
|         - | 4781 | `						/* Pop l-value */` |
|    996859 | 4782 | `						PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|    497518 | 4783 | `					}` |
|         - | 4784 | `				}` |
|    497680 | 4785 | `			}else{` |
|         - | 4786 | `				/* Go compile the sucker */` |
|   1433561 | 4787 | `				rc = xCons(&(*pGen));` |
|   1433556 | 4788 | `				if( xCons == PH7_CompileLabel` |
|   2149099 | 4789 | `				 \|\| ( pGen->pTokenSet` |
|   1433336 | 4790 | `				   && pGen->pIn > (SyToken *)SySetBasePtr(pGen->pTokenSet)` |
|   1433335 | 4791 | `				   && (pGen->pIn[-1].nType & PH7_TK_CCB/*'}'*/) ) ){` |
|         - | 4792 | `					/* A label, and any construct that ends with its own block, close` |
|         - | 4793 | `					 * themselves. (An EXPRESSION statement never does, which is why` |
|         - | 4794 | ``					 * this asks the construct and not just the last token: the `}` of`` |
|         - | 4795 | ``					 * `$f = function () {}` is not a terminator.) */`` |
|    889129 | 4796 | `					bStmtWantsSemi = 0;` |
|    443937 | 4797 | `				}` |
|         - | 4798 | `			}` |
|   2430729 | 4799 | `			if( rc == SXERR_ABORT ){` |
|         - | 4800 | `				/* Request to abort compilation */` |
|        93 | 4801 | `				break;` |
|         - | 4802 | `			}` |
|         - | 4803 | `		}` |
|         - | 4804 | `		/* Ignore trailing semi-colons ';' */` |
|         - | 4805 | `		{` |
|         - | 4806 | ``			/* Terminated when a `;` is sitting there for the loop to step over, or`` |
|         - | 4807 | `			 * when the construct consumed its own (the alternative-syntax bodies` |
|         - | 4808 | ``			 * take the `;` after their `endif`/`endwhile`/… themselves). */`` |
|   2430665 | 4809 | `			int bTerminated = (pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI)) != 0;` |
|   2430660 | 4810 | `			if( !bTerminated && pGen->pTokenSet` |
|    890058 | 4811 | `			 && pGen->pIn > (SyToken *)SySetBasePtr(pGen->pTokenSet)` |
|    890063 | 4812 | `			 && (pGen->pIn[-1].nType & PH7_TK_SEMI) ){` |
|       929 | 4813 | `				bTerminated = 1;` |
|       462 | 4814 | `			}` |
|   3971267 | 4815 | `			while( pGen->pIn < pGen->pEnd && (pGen->pIn->nType & PH7_TK_SEMI) ){` |
|   1540607 | 4816 | `				pGen->pIn++;` |
|         5 | 4817 | `			}` |
|   2430660 | 4818 | `			if( !bTerminated && bStmtWantsSemi && pGen->nErr < 1 && GenStateAtChunkEof(&(*pGen))` |
|        59 | 4819 | `			 && pGen->pIn > (SyToken *)SySetBasePtr(pGen->pTokenSet) ){` |
|         - | 4820 | `				/* (GenStateAtChunkEof already answered no for a NULL token set.) */` |
|         - | 4821 | `				/* Ran out of input with the statement still open. */` |
|        51 | 4822 | `				rc = PH7_GenSyntaxError(&(*pGen),0,GenStateEofExpecting(pStmtStart,&pGen->pIn[-1]));` |
|        51 | 4823 | `				if( rc == SXERR_ABORT ){` |
|       ! 0 | 4824 | `					break;` |
|         - | 4825 | `				}` |
|        25 | 4826 | `			}` |
|         - | 4827 | `		}` |
|   2430665 | 4828 | `		if( iFlags & PH7_COMPILE_SINGLE_STMT ){` |
|         - | 4829 | `			/* Compile a single statement and return */` |
|   2137652 | 4830 | `			break;` |
|         - | 4831 | `		}` |
|         - | 4832 | `		/* LOOP ONE */` |
|         - | 4833 | `		/* LOOP TWO */` |
|         - | 4834 | `		/* LOOP THREE */` |
|         - | 4835 | `		/* LOOP FOUR */` |
|         5 | 4836 | `	}` |
|         - | 4837 | `	/* Return compilation status */` |
|   2171112 | 4838 | `	return rc;` |
|         5 | 4839 | `}` |
|         - | 4840 | `/*` |
|         - | 4841 | `` * TRUE when the SOURCE bytes of a double-quoted string interpolate -- `$name`,`` |
|         - | 4842 | `` * `${`, or `{$` -- which is what decides whether php's scanner produced ONE`` |
|         - | 4843 | ` * string token for it or an opening quote followed by parts. A backslash escapes` |
|         - | 4844 | `` * whatever follows it, so `"\\$b"` does not interpolate.`` |
|         - | 4845 | ` */` |
|        24 | 4846 | `static int GenStateDqInterpolates(SyString *pStr)` |
|         1 | 4847 | `{` |
|        25 | 4848 | `	const unsigned char *z = (const unsigned char *)pStr->zString;` |
|        25 | 4849 | `	const unsigned char *zEnd = &z[pStr->nByte];` |
|        55 | 4850 | `	while( z < zEnd ){` |
|        41 | 4851 | `		if( z[0] == '\\' ){` |
|         5 | 4852 | `			z += 2;` |
|         5 | 4853 | `			continue;` |
|         - | 4854 | `		}` |
|        36 | 4855 | `		if( z[0] == '$' && &z[1] < zEnd` |
|         8 | 4856 | `		 && (z[1] == '{' \|\| z[1] >= 0x80 \|\| SyisAlpha(z[1]) \|\| z[1] == '_') ){` |
|         7 | 4857 | `			return 1;` |
|         - | 4858 | `		}` |
|        31 | 4859 | `		if( z[0] == '{' && &z[1] < zEnd && z[1] == '$' ){` |
|         5 | 4860 | `			return 1;` |
|         - | 4861 | `		}` |
|        27 | 4862 | `		z++;` |
|         1 | 4863 | `	}` |
|        15 | 4864 | `	return 0;` |
|        13 | 4865 | `}` |
|         - | 4866 | `/*` |
|         - | 4867 | ` * Compile a Raw PHP chunk.` |
|         - | 4868 | ` * If something goes wrong while compiling the PHP chunk,this function` |
|         - | 4869 | ` * takes care of generating the appropriate error message.` |
|         - | 4870 | ` */` |
|     33426 | 4871 | `static sxi32 PH7_CompilePHP(` |
|         - | 4872 | `	ph7_gen_state *pGen,  /* Code generator state */` |
|         - | 4873 | `	SySet *pTokenSet,     /* Token set */` |
|         - | 4874 | `	int is_expr           /* TRUE if we are dealing with a simple expression */` |
|         - | 4875 | `	)` |
|         5 | 4876 | `{` |
|     33431 | 4877 | `	SyToken *pScript = pGen->pRawIn; /* Script to compile */` |
|         - | 4878 | `	sxi32 rc;` |
|         - | 4879 | `	/* Reset the token set (and its trivia sidecar) */` |
|     33431 | 4880 | `	SySetReset(&(*pTokenSet));` |
|     33431 | 4881 | `	SySetReset(&pGen->aTrivia);` |
|         - | 4882 | `	/* Mark as the default token set */` |
|     33431 | 4883 | `	pGen->pTokenSet = &(*pTokenSet);` |
|         - | 4884 | `	/* Advance the stream cursor */` |
|     33431 | 4885 | `	pGen->pRawIn++;` |
|         - | 4886 | `	/* Tokenize the PHP chunk first */` |
|     33431 | 4887 | `	PH7_TokenizePHP(SyStringData(&pScript->sData),SyStringLength(&pScript->sData),pScript->nLine,&(*pTokenSet),&pGen->aTrivia);` |
|         - | 4888 | ``	/* The raw tokenizer marked whether this chunk was closed by a `?>`; only one`` |
|         - | 4889 | `	 * that met the end of the FILE can leave a statement unterminated. */` |
|     33431 | 4890 | `	pGen->bChunkAtEof = (sxi8)(SX_PTR_TO_INT(pScript->pUserData) == 0);` |
|         - | 4891 | `	/* Point to the head and tail of the token stream. */` |
|     33431 | 4892 | `	pGen->pIn  = (SyToken *)SySetBasePtr(pTokenSet);` |
|     33431 | 4893 | `	pGen->pEnd = &pGen->pIn[SySetUsed(pTokenSet)];` |
|         - | 4894 | ``	/* php REMOVED `(real)` in its SCANNER, so the refusal belongs to the chunk and`` |
|         - | 4895 | ``	 * not to the expression the cast sits in: `strlen(real)` -- where the token is`` |
|         - | 4896 | `	 * never an operator at all -- reports the same sentence, and reports it as a` |
|         - | 4897 | ``	 * PARSE error rather than the fatal `(unset)` gets from the compiler. */`` |
|         - | 4898 | `	{` |
|         - | 4899 | `		SyToken *pTok;` |
|     33431 | 4900 | `		sxi32 nBraceOpen = 0;` |
|  26942859 | 4901 | `		for( pTok = pGen->pIn ; pTok < pGen->pEnd ; pTok++ ){` |
|  26909433 | 4902 | `			if( pTok->nType & PH7_TK_OCB ){` |
|   1026123 | 4903 | `				nBraceOpen++;` |
|  26395646 | 4904 | `			}else if( pTok->nType & PH7_TK_CCB ){` |
|   1026105 | 4905 | `				nBraceOpen--;` |
|    512322 | 4906 | `			}` |
|  13432980 | 4907 | `		}` |
|     33431 | 4908 | `		if( nBraceOpen > 0 ){` |
|         - | 4909 | ``			/* A `{` this chunk never closes: php's parser reports THAT at the end of`` |
|         - | 4910 | `			 * the file, ahead of any statement it left open and ahead of a string or` |
|         - | 4911 | `			 * heredoc the scanner was still inside. Stand the end-of-input questions` |
|         - | 4912 | ``			 * down and let the block compiler say its own `Unclosed '{'`. */`` |
|        17 | 4913 | `			pGen->bChunkAtEof = 0;` |
|         7 | 4914 | `		}` |
|  26942823 | 4915 | `		for( pTok = pGen->pIn ; pTok < pGen->pEnd ; pTok++ ){` |
|  26909425 | 4916 | `			if( pTok->nType & PH7_TK_ALIAS_CAST ){` |
|         - | 4917 | `				/* php 8.5 announces the four alias SPELLINGS -- (integer), (boolean),` |
|         - | 4918 | `				 * (double), (binary) -- from its SCANNER, not from the compiler: the` |
|         - | 4919 | ``				 * sentence comes out for `strlen(integer)`, where the token is never a`` |
|         - | 4920 | `				 * cast at all, and it comes out ahead of a parse error further down the` |
|         - | 4921 | `				 * file. It is one line per OCCURRENCE in the source, not per execution:` |
|         - | 4922 | `				 * a cast inside a function nobody calls still announces itself, and one` |
|         - | 4923 | `				 * inside a loop announces itself once. Nothing about the cast changes --` |
|         - | 4924 | `				 * the spelling is what is deprecated -- so this only reports, and the` |
|         - | 4925 | `				 * walk carries on to the rest of the chunk.` |
|         - | 4926 | `				 *` |
|         - | 4927 | `				 * Each of the four is the only alias of its target, so the canonical` |
|         - | 4928 | `				 * token text the lexer left behind names both halves of the sentence. */` |
|         - | 4929 | `				static const struct { const char *zCanon; int nCanon; const char *zAlias; } aAlias[] = {` |
|         - | 4930 | `					{ "(int)",    5, "integer" }, { "(bool)",   6, "boolean" },` |
|         - | 4931 | `					{ "(float)",  7, "double"  }, { "(string)", 8, "binary"  }` |
|         - | 4932 | `				};` |
|         - | 4933 | `				sxu32 i;` |
|       106 | 4934 | `				for( i = 0 ; i < SX_ARRAYSIZE(aAlias) ; ++i ){` |
|       104 | 4935 | `					if( pTok->sData.nByte == (sxu32)aAlias[i].nCanon` |
|        73 | 4936 | `					 && SyMemcmp((const void *)pTok->sData.zString,` |
|        57 | 4937 | `					             (const void *)aAlias[i].zCanon,pTok->sData.nByte) == 0 ){` |
|        59 | 4938 | `						PH7_GenCompileError(pGen,8192 /* E_DEPRECATED */,pTok->nLine,` |
|         - | 4939 | `							"Non-canonical cast (%s) is deprecated, use the %s cast instead",` |
|        38 | 4940 | `							aAlias[i].zAlias,aAlias[i].zCanon);` |
|        40 | 4941 | `						break;` |
|         - | 4942 | `					}` |
|        35 | 4943 | `				}` |
|        19 | 4944 | `			}` |
|  26909420 | 4945 | `			if( (pTok->nType & PH7_TK_OP) && pTok->sData.nByte == sizeof("(real)")-1` |
|   2084412 | 4946 | `			 && SyMemcmp((const void *)pTok->sData.zString,(const void *)"(real)",sizeof("(real)")-1) == 0 ){` |
|         5 | 4947 | `				return PH7_GenCompileError(pGen,E_PARSE,pTok->nLine,` |
|         - | 4948 | `					"The (real) cast has been removed, use (float) instead");` |
|         - | 4949 | `			}` |
|  26909416 | 4950 | `			if( (pTok->nType & PH7_TK_UNTERM)` |
|  13432982 | 4951 | `			 && (pTok->nType & (PH7_TK_DSTR\|PH7_TK_HEREDOC\|PH7_TK_NOWDOC))` |
|        27 | 4952 | `			 && pGen->bChunkAtEof == 0 ){` |
|         - | 4953 | `				/* php's parser reaches the end of file with the string still open and` |
|         - | 4954 | `				 * reports the UNCLOSED BRACE first -- the scanner is mid-interpolation` |
|         - | 4955 | `				 * there and has nothing of its own to say. (A single-quoted string and` |
|         - | 4956 | `				 * a block comment do: their sentences win over the brace, which is why` |
|         - | 4957 | `				 * only these three yield.) Leave it to the compile below. */` |
|         3 | 4958 | `				continue;` |
|         - | 4959 | `			}` |
|  26909419 | 4960 | `			if( pTok->nType & PH7_TK_UNTERM ){` |
|         - | 4961 | `				/* A quote, heredoc or block comment the input ran out under. php` |
|         - | 4962 | `				 * refuses the file for each; this used to take the rest of it as the` |
|         - | 4963 | ``				 * lexeme's body and RUN the program (`<?php echo 'a` printed `a`).`` |
|         - | 4964 | `				 * The wording is php's own per shape -- its scanner reports what it` |
|         - | 4965 | `				 * was still waiting for. */` |
|        24 | 4966 | `				if( pTok->nType & PH7_TK_SSTR ){` |
|         - | 4967 | `					/* php's single-quoted scanner hands the parser the CONTENT it` |
|         - | 4968 | `					 * had read, and the parser names that. */` |
|         6 | 4969 | `					return PH7_GenCompileError(pGen,E_PARSE,pTok->nLine,` |
|         2 | 4970 | `						"syntax error, unexpected string content \"%z\"",&pTok->sData);` |
|         - | 4971 | `				}` |
|        20 | 4972 | `				if( pTok->nType & PH7_TK_DSTR ){` |
|        12 | 4973 | `					const char *zExp = pTok->sData.nByte < 1` |
|         - | 4974 | `						? "variable or string content or \"${\" or \"{$\""` |
|         7 | 4975 | `						: (GenStateDqInterpolates(&pTok->sData) ? 0` |
|         - | 4976 | `						                                       : "variable or \"${\" or \"{$\"");` |
|         4 | 4977 | `					return zExp` |
|         6 | 4978 | `						? PH7_GenCompileError(pGen,E_PARSE,pTok->nLine,` |
|         2 | 4979 | `							"syntax error, unexpected end of file, expecting %s",zExp)` |
|         8 | 4980 | `						: PH7_GenCompileError(pGen,E_PARSE,pTok->nLine,` |
|         - | 4981 | `							"syntax error, unexpected end of file");` |
|         - | 4982 | `				}` |
|        12 | 4983 | `				if( pTok->nType & (PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){` |
|         - | 4984 | `					/* An EMPTY body, or one that interpolates, leaves php's parser` |
|         - | 4985 | `					 * with nothing to expect but the end it just met. */` |
|        15 | 4986 | `					int bSet = pTok->sData.nByte > 0` |
|         8 | 4987 | `						&& !((pTok->nType & PH7_TK_HEREDOC) && GenStateDqInterpolates(&pTok->sData));` |
|         4 | 4988 | `					return bSet` |
|         4 | 4989 | `						? PH7_GenCompileError(pGen,E_PARSE,pTok->nLine,` |
|         - | 4990 | `							"syntax error, unexpected end of file, "` |
|         - | 4991 | `							"expecting variable or heredoc end or \"${\" or \"{$\"")` |
|         8 | 4992 | `						: PH7_GenCompileError(pGen,E_PARSE,pTok->nLine,` |
|         - | 4993 | `							"syntax error, unexpected end of file");` |
|         - | 4994 | `				}` |
|         - | 4995 | `				/* A block comment, whose sentence names where it began. */` |
|         6 | 4996 | `				return PH7_GenCompileError(pGen,E_PARSE,pTok->nLine,` |
|         2 | 4997 | `					"Unterminated comment starting line %u",pTok->nLine);` |
|         - | 4998 | `			}` |
|  13432961 | 4999 | `		}` |
|         - | 5000 | `	}` |
|     33403 | 5001 | `	if( is_expr ){` |
|       ! 0 | 5002 | `		rc = SXERR_EMPTY;` |
|       ! 0 | 5003 | `		if( pGen->pIn < pGen->pEnd ){` |
|         - | 5004 | `			/* A simple expression,compile it */` |
|       ! 0 | 5005 | `			rc = PH7_CompileExpr(pGen,0,0);` |
|       ! 0 | 5006 | `		}` |
|         - | 5007 | `		/* Emit the DONE instruction */` |
|       ! 0 | 5008 | `		PH7_VmEmitInstr(pGen->pVm,PH7_OP_DONE,(rc != SXERR_EMPTY ? 1 : 0),0,0,0);` |
|       ! 0 | 5009 | `		return SXRET_OK;` |
|         - | 5010 | `	}` |
|     33403 | 5011 | `	if( pGen->pIn < pGen->pEnd && ( pGen->pIn->nType & PH7_TK_EQUAL ) ){` |
|         - | 5012 | `		static const sxu32 nKeyID = PH7_TKWRD_ECHO;` |
|         - | 5013 | `		/*` |
|         - | 5014 | `		 * Shortcut syntax for the 'echo' language construct.` |
|         - | 5015 | `		 * According to the PHP reference manual:` |
|         - | 5016 | `		 *  echo() also has a shortcut syntax, where you can` |
|         - | 5017 | `		 *  immediately follow` |
|         - | 5018 | `		 *  the opening tag with an equals sign as follows:` |
|         - | 5019 | `		 *  <?= 4+5?> is the same as <?echo 4+5?>` |
|         - | 5020 | `		 * Symisc extension:` |
|         - | 5021 | `		 *   This short syntax works with all PHP opening` |
|         - | 5022 | `		 *   tags unlike the default PHP engine that handle` |
|         - | 5023 | `		 *   only short tag.` |
|         - | 5024 | `		 */` |
|         - | 5025 | `		/* Ticket 1433-009: Emulate the 'echo' call */` |
|         3 | 5026 | `		pGen->pIn->nType = PH7_TK_KEYWORD;` |
|         3 | 5027 | `		pGen->pIn->pUserData = SX_INT_TO_PTR(nKeyID);` |
|         3 | 5028 | `		SyStringInitFromBuf(&pGen->pIn->sData,"echo",sizeof("echo")-1);` |
|         - | 5029 | `		/* This synthesized echo is compiled as an EXPRESSION, which is otherwise a` |
|         - | 5030 | `		 * parse error; allow it for the duration of this one compile. */` |
|         3 | 5031 | `		pGen->nExprEchoOk++;` |
|         3 | 5032 | `		rc = PH7_CompileExpr(pGen,0,0);` |
|         3 | 5033 | `		pGen->nExprEchoOk--;` |
|         3 | 5034 | `		if( rc == SXERR_ABORT ){` |
|       ! 0 | 5035 | `			return SXERR_ABORT;` |
|         - | 5036 | `		}` |
|         3 | 5037 | `		if( rc != SXERR_EMPTY ){` |
|         3 | 5038 | `			PH7_VmEmitInstr(pGen->pVm,PH7_OP_POP,1,0,0,0);` |
|         1 | 5039 | `		}` |
|         3 | 5040 | `		return SXRET_OK;` |
|         - | 5041 | `	}` |
|         - | 5042 | `	/* Compile the PHP chunk */` |
|     33401 | 5043 | `	rc = GenStateCompileChunk(pGen,0);` |
|         - | 5044 | `	/* Fix exceptions jumps */` |
|     33401 | 5045 | `	GenStateFixJumps(pGen->pCurrent,PH7_OP_THROW,PH7_VmInstrLength(pGen->pVm));` |
|         - | 5046 | `	/* Fix gotos now, the jump destination is resolved */` |
|     33401 | 5047 | `	if( SXERR_ABORT == GenStateFixGoto(&(*pGen),0) ){` |
|         3 | 5048 | `		rc = SXERR_ABORT;` |
|         1 | 5049 | `	}` |
|         - | 5050 | `	/* Reset container */` |
|     33401 | 5051 | `	SySetReset(&pGen->aGoto);` |
|     33401 | 5052 | `	SySetReset(&pGen->aLabel);` |
|     33401 | 5053 | `	SySetReset(&pGen->aNullsafeJmp);` |
|         - | 5054 | `	/* Compilation result */` |
|     33401 | 5055 | `	return rc;` |
|     16707 | 5056 | `}` |
|         - | 5057 | `/*` |
|         - | 5058 | ` * Compile a raw chunk. The raw chunk can contain PHP code embedded` |
|         - | 5059 | ` * in HTML, XML and so on. This function handle all the stuff.` |
|         - | 5060 | ` * This is the only compile interface exported from this file.` |
|         - | 5061 | ` */` |
|     37490 | 5062 | `PH7_PRIVATE sxi32 PH7_CompileScript(` |
|         - | 5063 | `	ph7_vm *pVm,        /* Generate PH7 byte-codes for this Virtual Machine */` |
|         - | 5064 | `	SyString *pScript,  /* Script to compile */` |
|         - | 5065 | `	sxi32 iFlags        /* Compile flags */` |
|         - | 5066 | `	)` |
|         5 | 5067 | `{` |
|         - | 5068 | `	SySet aPhpToken,aRawToken;` |
|         - | 5069 | `	ph7_gen_state *pCodeGen;` |
|         - | 5070 | `	ph7_value *pRawObj;` |
|         - | 5071 | `	sxu32 nObjIdx;` |
|         - | 5072 | `	sxi32 nRawObj;` |
|         - | 5073 | `	int is_expr;` |
|         - | 5074 | `	sxi8 bSavedStrict;` |
|         - | 5075 | `	sxi8 bSavedStrictLocked;` |
|         - | 5076 | `	sxi8 bSavedNsNamed,bSavedNsBracketed,bSavedInNsBlock;` |
|         - | 5077 | `	sxi8 bSavedHalted,bSavedHaltSeen;` |
|         - | 5078 | `	sxu32 nSavedHaltOffset;` |
|         - | 5079 | `	const char *zSavedScriptBase;` |
|         - | 5080 | `	const char *zFileBase;` |
|         - | 5081 | `	SyToken *pSavedIn,*pSavedEnd;` |
|         - | 5082 | `	sxi32 rc;` |
|     37495 | 5083 | `	sxu32 nBaseLine = 1;` |
|     37495 | 5084 | `	if( pScript->nByte < 1 ){` |
|         - | 5085 | `		/* Nothing to compile */` |
|        12 | 5086 | `		return PH7_OK;` |
|         - | 5087 | `	}` |
|         - | 5088 | `	/* Kept before the shebang skip below: php counts __COMPILER_HALT_OFFSET__` |
|         - | 5089 | `	 * from the first byte on DISK, shebang line included. */` |
|     37485 | 5090 | `	zFileBase = pScript->zString;` |
|         - | 5091 | `	/* php skips a "#!" shebang on the first line of a CLI script: consume it` |
|         - | 5092 | `	 * (including its newline) so it is not echoed as inline text, and bump the` |
|         - | 5093 | `	 * base line to 2 so the code below still reports php-matching line numbers. */` |
|     37485 | 5094 | `	if( pScript->nByte >= 2 && pScript->zString[0]=='#' && pScript->zString[1]=='!' ){` |
|         6 | 5095 | `		const char *z = pScript->zString;` |
|         6 | 5096 | `		const char *zEnd = &z[pScript->nByte];` |
|        78 | 5097 | `		while( z < zEnd && z[0] != '\n' ){ z++; }` |
|         6 | 5098 | `		if( z < zEnd ){ z++; } /* consume the newline too */` |
|         6 | 5099 | `		pScript->nByte -= (sxu32)(z - pScript->zString);` |
|         6 | 5100 | `		pScript->zString = z;` |
|         6 | 5101 | `		nBaseLine = 2;` |
|         6 | 5102 | `		if( pScript->nByte < 1 ){` |
|       ! 0 | 5103 | `			return PH7_OK;` |
|         - | 5104 | `		}` |
|         2 | 5105 | `	}` |
|         - | 5106 | `	/* Each compiled file has its own strict_types scope. Save the outer` |
|         - | 5107 | `	 * file's flags so include/require restore them on return. */` |
|     37485 | 5108 | `	pCodeGen = &pVm->sCodeGen;` |
|         - | 5109 | ``	/* aPhpToken below is a LOCAL token set: once it is released at `cleanup`, any`` |
|         - | 5110 | `	 * pGen->pIn/pEnd still pointing into it DANGLE. PH7_VmEmitInstr reads pIn to stamp` |
|         - | 5111 | `	 * each instruction's source line, and instructions are still emitted after this` |
|         - | 5112 | `	 * function returns (VmEvalChunk's trailing OP_DONE) -- a use-after-free ASan caught` |
|         - | 5113 | `	 * immediately. Save the caller's cursor and restore it on the way out, so an` |
|         - | 5114 | `	 * enclosing compile keeps its (live) tokens and the top level goes back to NULL. */` |
|     37485 | 5115 | `	pSavedIn = pCodeGen->pIn;` |
|     37485 | 5116 | `	pSavedEnd = pCodeGen->pEnd;` |
|     37485 | 5117 | `	bSavedStrict = pCodeGen->bStrictTypes;` |
|     37485 | 5118 | `	bSavedStrictLocked = pCodeGen->bStrictTypesLocked;` |
|     37485 | 5119 | `	pCodeGen->bStrictTypes = 0;` |
|     37485 | 5120 | `	pCodeGen->bStrictTypesLocked = 0;` |
|         - | 5121 | `	/* The namespace placement state is per FILE as well (php's file_context). */` |
|     37485 | 5122 | `	bSavedNsNamed = pCodeGen->bNsNamed;` |
|     37485 | 5123 | `	bSavedNsBracketed = pCodeGen->bNsBracketed;` |
|     37485 | 5124 | `	bSavedInNsBlock = pCodeGen->bInNsBlock;` |
|     37485 | 5125 | `	pCodeGen->bNsNamed = 0;` |
|     37485 | 5126 | `	pCodeGen->bNsBracketed = 0;` |
|     37485 | 5127 | `	pCodeGen->bInNsBlock = 0;` |
|         - | 5128 | `	/* The halt is per-FILE too, and an include compiles inside its includer. */` |
|     37485 | 5129 | `	bSavedHalted = pCodeGen->bHalted;` |
|     37485 | 5130 | `	bSavedHaltSeen = pCodeGen->bHaltSeen;` |
|     37485 | 5131 | `	nSavedHaltOffset = pCodeGen->nHaltOffset;` |
|     37485 | 5132 | `	zSavedScriptBase = pCodeGen->zScriptBase;` |
|     37485 | 5133 | `	pCodeGen->bHalted = 0;` |
|     37485 | 5134 | `	pCodeGen->bHaltSeen = 0;` |
|     37485 | 5135 | `	pCodeGen->nHaltOffset = 0;` |
|     37485 | 5136 | `	pCodeGen->zScriptBase = zFileBase;` |
|         - | 5137 | `	/* Initialize the tokens containers */` |
|     37485 | 5138 | `	SySetInit(&aRawToken,&pVm->sAllocator,sizeof(SyToken));` |
|     37485 | 5139 | `	SySetInit(&aPhpToken,&pVm->sAllocator,sizeof(SyToken));` |
|     37485 | 5140 | `	SySetAlloc(&aPhpToken,0xc0);` |
|     37485 | 5141 | `	is_expr = 0;` |
|     37485 | 5142 | `	if( iFlags & PH7_PHP_ONLY ){` |
|         - | 5143 | `		SyToken sTmp;` |
|         - | 5144 | `		/* PHP only: -*/` |
|      8654 | 5145 | `		sTmp.nLine = 1;` |
|      8654 | 5146 | `		sTmp.nType = PH7_TOKEN_PHP;` |
|      8654 | 5147 | `		sTmp.pUserData = 0;` |
|      8654 | 5148 | `		SyStringDupPtr(&sTmp.sData,pScript);` |
|      8654 | 5149 | `		SySetPut(&aRawToken,(const void *)&sTmp);` |
|      8654 | 5150 | `		if( iFlags & PH7_PHP_EXPR ){` |
|         - | 5151 | `			/* A simple PHP expression */` |
|       ! 0 | 5152 | `			is_expr = 1;` |
|       ! 0 | 5153 | `		}` |
|      4324 | 5154 | `	}else{` |
|         - | 5155 | `		/* Tokenize raw text */` |
|     28836 | 5156 | `		SySetAlloc(&aRawToken,32);` |
|     28836 | 5157 | `		PH7_TokenizeRawText(pScript->zString,pScript->nByte,&aRawToken,nBaseLine);` |
|         - | 5158 | `	}` |
|         - | 5159 | ``	/* Where the end of INPUT sits. php reports `unexpected end of file` at the line`` |
|         - | 5160 | `	 * the file ENDS on, which is past the last token whenever anything follows it --` |
|         - | 5161 | `	 * a trailing newline always does -- and the chunk a statement was cut off in` |
|         - | 5162 | `	 * does not carry that: the raw splitter hands it the source up to its last byte` |
|         - | 5163 | `	 * of code, newline excluded. Counted here, where the whole script is still in` |
|         - | 5164 | `	 * hand, and read back by PH7_GenSyntaxError's end-of-file branch. */` |
|         - | 5165 | `	{` |
|         - | 5166 | `		sxu32 i;` |
|     37485 | 5167 | `		pCodeGen->nChunkEofLine = nBaseLine;` |
| 149527446 | 5168 | `		for( i = 0 ; i < pScript->nByte ; ++i ){` |
| 149489966 | 5169 | `			if( pScript->zString[i] == '\n' ){` |
|    232792 | 5170 | `				pCodeGen->nChunkEofLine++;` |
|    115969 | 5171 | `			}` |
|  74624574 | 5172 | `		}` |
|         - | 5173 | `	}` |
|         - | 5174 | `	/* Process high-level tokens */` |
|     37485 | 5175 | `	pCodeGen->pRawIn = (SyToken *)SySetBasePtr(&aRawToken);` |
|     37485 | 5176 | `	pCodeGen->pRawEnd = &pCodeGen->pRawIn[SySetUsed(&aRawToken)];` |
|         - | 5177 | `	/*` |
|         - | 5178 | ``	 * Where `__halt_compiler();` sits, decided BEFORE anything compiles: the`` |
|         - | 5179 | `	 * constant it defines may be read ahead of the statement that sets it (and` |
|         - | 5180 | `	 * in an earlier chunk than the one holding it), exactly as it may under php,` |
|         - | 5181 | `	 * whose compiler registers the constant for the whole file. The scan costs a` |
|         - | 5182 | `	 * second tokenization of every PHP chunk, so it only runs when the file` |
|         - | 5183 | `	 * contains the identifier at all -- which no ordinary program does.` |
|         - | 5184 | `	 */` |
|     37485 | 5185 | `	if( GenStateMentionsHalt(pScript->zString,pScript->nByte) ){` |
|        47 | 5186 | `		GenStateScanHaltOffset(pCodeGen,&aRawToken,zFileBase);` |
|        22 | 5187 | `	}` |
|     37485 | 5188 | `	rc = PH7_OK;` |
|     37485 | 5189 | `	if( is_expr ){` |
|         - | 5190 | `		/* Compile the expression */` |
|       ! 0 | 5191 | `		rc = PH7_CompilePHP(pCodeGen,&aPhpToken,TRUE);` |
|       ! 0 | 5192 | `		goto cleanup;` |
|         - | 5193 | `	}` |
|     37485 | 5194 | `	nObjIdx = 0;` |
|         - | 5195 | `	/* Start the compilation process */` |
|     33149 | 5196 | `	for(;;){` |
|     99644 | 5197 | `		if( pCodeGen->pRawIn >= pCodeGen->pRawEnd ){` |
|     37367 | 5198 | `			break; /* No more tokens to process */` |
|         - | 5199 | `		}` |
|     62282 | 5200 | `		if( pCodeGen->pRawIn->nType & PH7_TOKEN_PHP ){` |
|         - | 5201 | `			/* Compile the PHP chunk */` |
|     33431 | 5202 | `			rc = PH7_CompilePHP(pCodeGen,&aPhpToken,FALSE);` |
|     33431 | 5203 | `			if( rc == SXERR_ABORT ){` |
|        97 | 5204 | `				break;` |
|         - | 5205 | `			}` |
|     33339 | 5206 | `			if( pCodeGen->bHalted ){` |
|         - | 5207 | ``				/* `__halt_compiler();` -- the rest of the FILE is data, inline`` |
|         - | 5208 | `				 * text between later chunks included. */` |
|        27 | 5209 | `				break;` |
|         - | 5210 | `			}` |
|     33313 | 5211 | `			continue;` |
|         - | 5212 | `		}` |
|         - | 5213 | `		/* Raw chunk: [i.e: HTML, XML, etc.] */` |
|     28856 | 5214 | `		nRawObj = 0;` |
|     57723 | 5215 | `		while( (pCodeGen->pRawIn < pCodeGen->pRawEnd) && (pCodeGen->pRawIn->nType != PH7_TOKEN_PHP) ){` |
|         - | 5216 | `			/* Consume the raw chunk without any processing */` |
|     28872 | 5217 | `			pRawObj = PH7_ReserveConstObj(&(*pVm),&nObjIdx);` |
|     28872 | 5218 | `			if( pRawObj == 0 ){` |
|       ! 0 | 5219 | `				rc = SXERR_MEM;` |
|       ! 0 | 5220 | `				break;` |
|         - | 5221 | `			}` |
|         - | 5222 | `			/* Mark as constant and emit the load constant instruction */` |
|     28872 | 5223 | `			PH7_MemObjInitFromString(pVm,pRawObj,&pCodeGen->pRawIn->sData);` |
|     28872 | 5224 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_LOADC,0,nObjIdx,0,0);` |
|     28872 | 5225 | `			++nRawObj;` |
|     28872 | 5226 | `			pCodeGen->pRawIn++; /* Next chunk */` |
|         5 | 5227 | `		}` |
|     28856 | 5228 | `		if( nRawObj > 0 ){` |
|         - | 5229 | `			/* Emit the consume instruction */` |
|     28856 | 5230 | `			PH7_VmEmitInstr(&(*pVm),PH7_OP_CONSUME,nRawObj,0,0,0);` |
|     14420 | 5231 | `		}` |
|     18734 | 5232 | `	}` |
|     18751 | 5233 | `cleanup:` |
|         - | 5234 | `	/* Drop the cursor into the token set BEFORE the set is freed (see above). */` |
|     37485 | 5235 | `	pCodeGen->pIn = pSavedIn;` |
|     37485 | 5236 | `	pCodeGen->pEnd = pSavedEnd;` |
|     37485 | 5237 | `	SySetRelease(&aRawToken);` |
|     37485 | 5238 | `	SySetRelease(&aPhpToken);` |
|         - | 5239 | `	/* Restore outer file's strict_types scope */` |
|     37485 | 5240 | `	pCodeGen->bStrictTypes = bSavedStrict;` |
|     37485 | 5241 | `	pCodeGen->bStrictTypesLocked = bSavedStrictLocked;` |
|     37485 | 5242 | `	pCodeGen->bNsNamed = bSavedNsNamed;` |
|     37485 | 5243 | `	pCodeGen->bNsBracketed = bSavedNsBracketed;` |
|     37485 | 5244 | `	pCodeGen->bInNsBlock = bSavedInNsBlock;` |
|         - | 5245 | `	/* ...and its halt state. */` |
|     37485 | 5246 | `	pCodeGen->bHalted = bSavedHalted;` |
|     37485 | 5247 | `	pCodeGen->bHaltSeen = bSavedHaltSeen;` |
|     37485 | 5248 | `	pCodeGen->nHaltOffset = nSavedHaltOffset;` |
|     37485 | 5249 | `	pCodeGen->zScriptBase = zSavedScriptBase;` |
|     37485 | 5250 | `	return rc;` |
|     18739 | 5251 | `}` |
|         - | 5252 | `/*` |
|         - | 5253 | ` * Utility routines.Initialize the code generator.` |
|         - | 5254 | ` */` |
|      7925 | 5255 | `PH7_PRIVATE sxi32 PH7_InitCodeGenerator(` |
|         - | 5256 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 5257 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 5258 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 5259 | `	)` |
|         5 | 5260 | `{` |
|      7930 | 5261 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 5262 | `	/* Zero the structure */` |
|      7930 | 5263 | `	SyZero(pGen,sizeof(ph7_gen_state));` |
|         - | 5264 | `	/* Initial state */` |
|      7930 | 5265 | `	pGen->pVm  = &(*pVm);` |
|      7930 | 5266 | `	pGen->xErr = xErr;` |
|      7930 | 5267 | `	pGen->pErrData = pErrData;` |
|      7930 | 5268 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|      7930 | 5269 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|      7930 | 5270 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|      7930 | 5271 | `	SySetInit(&pGen->aScope,&pVm->sAllocator,sizeof(GenScope));` |
|      7930 | 5272 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|      7930 | 5273 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|      7930 | 5274 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      7930 | 5275 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|      7930 | 5276 | `	SyHashInit(&pGen->hLiteral,&pVm->sAllocator,0,0);` |
|      7930 | 5277 | `	SyHashInit(&pGen->hVar,&pVm->sAllocator,0,0);` |
|         - | 5278 | `	/* Error log buffer */` |
|      7930 | 5279 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|      7930 | 5280 | `	SyBlobInit(&pGen->sFirstErr,&pVm->sAllocator);` |
|         - | 5281 | `	/* General purpose working buffer */` |
|      7930 | 5282 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|         - | 5283 | `	/* Namespace state */` |
|      7930 | 5284 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|      7930 | 5285 | `	GenStateInitUseImports(&(*pGen),&(*pVm));` |
|      7930 | 5286 | `	GenStateInitSeenSymbols(&(*pGen),&(*pVm));` |
|         - | 5287 | `	/* Create the global scope */` |
|      7930 | 5288 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(&(*pVm)),0);` |
|         - | 5289 | `	/* Point to the global scope */` |
|      7930 | 5290 | `	pGen->pCurrent = &pGen->sGlobal;` |
|      7930 | 5291 | `	return SXRET_OK;` |
|         5 | 5292 | `}` |
|         - | 5293 | `/*` |
|         - | 5294 | ` * Utility routines. Reset the code generator to it's initial state.` |
|         - | 5295 | ` */` |
|     44171 | 5296 | `PH7_PRIVATE sxi32 PH7_ResetCodeGenerator(` |
|         - | 5297 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 5298 | `	ProcConsumer xErr, /* Error log consumer callabck  */` |
|         - | 5299 | `	void *pErrData     /* Last argument to xErr() */` |
|         - | 5300 | `	)` |
|         5 | 5301 | `{` |
|     44176 | 5302 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 5303 | `	GenBlock *pBlock,*pParent;` |
|         - | 5304 | `	/* Reset state */` |
|     44176 | 5305 | `	SySetReset(&pGen->aLabel);` |
|     44176 | 5306 | `	SySetReset(&pGen->aGoto);` |
|     44176 | 5307 | `	SySetReset(&pGen->aNullsafeJmp);` |
|     44176 | 5308 | `	SySetReset(&pGen->aTrivia);` |
|     44176 | 5309 | `	SySetReset(&pGen->aPendingAttrs);` |
|     44176 | 5310 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|     44176 | 5311 | `	SyBlobRelease(&pGen->sErrBuf);` |
|     44176 | 5312 | `	SyBlobRelease(&pGen->sFirstErr);` |
|     44176 | 5313 | `	SyBlobRelease(&pGen->sWorker);` |
|     44176 | 5314 | `	SyBlobRelease(&pGen->sNamespace);` |
|     44176 | 5315 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|     44176 | 5316 | `	GenStateResetUseImports(&(*pGen),&(*pVm));` |
|         - | 5317 | `	/* A fresh compile unit has declared nothing yet. */` |
|     44176 | 5318 | `	GenStateResetSeenSymbols(&(*pGen),&(*pVm));` |
|         - | 5319 | `	/* Note: pGen->hVar and pGen->hLiteral are intentionally NOT reset here.` |
|         - | 5320 | `	 * They intern variable names and literal strings that are referenced by` |
|         - | 5321 | `	 * compiled bytecode (pInstr->p3) and runtime frame hash tables (pFrame->hVar).` |
|         - | 5322 | `	 * Releasing them would either leak the interned strings or require freeing` |
|         - | 5323 | `	 * memory still in use.  The entries use pool memory but are bounded by the` |
|         - | 5324 | `	 * number of unique names, which is acceptable. */` |
|         - | 5325 | `	/* Point to the global scope */` |
|     44176 | 5326 | `	pBlock = pGen->pCurrent;` |
|     44176 | 5327 | `	while( pBlock->pParent != 0 ){` |
|       ! 0 | 5328 | `		pParent = pBlock->pParent;` |
|       ! 0 | 5329 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 5330 | `		pBlock = pParent;` |
|       ! 0 | 5331 | `	}` |
|     44176 | 5332 | `	pGen->xErr = xErr;` |
|     44176 | 5333 | `	pGen->pErrData = pErrData;` |
|     44176 | 5334 | `	pGen->pCurrent = &pGen->sGlobal;` |
|     44176 | 5335 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|     44176 | 5336 | `	pGen->pIn = pGen->pEnd = 0;` |
|     44176 | 5337 | `	pGen->nErr = 0;` |
|     44176 | 5338 | `	pGen->nFatal = 0;` |
|     44176 | 5339 | `	pGen->nFirstErrLine = 0;` |
|     44176 | 5340 | `	pGen->bParseThrows = 0;` |
|     44176 | 5341 | `	pGen->iFatalTrace = PH7_FATAL_TRACE_COMPILE;` |
|         - | 5342 | `	/* Clear the class-body context (a prior compile aborted mid-class-body would` |
|         - | 5343 | `	 * otherwise leave these live for the next eval/include on this VM). */` |
|     44176 | 5344 | `	pGen->pCurClass = 0;` |
|     44176 | 5345 | `	pGen->iInMemberDefault = 0;` |
|     44176 | 5346 | `	return SXRET_OK;` |
|         5 | 5347 | `}` |
|         - | 5348 | `/*` |
|         - | 5349 | ` * Save the code generator's compile-position state and hand the live generator a` |
|         - | 5350 | ` * fresh, empty one for a NESTED compilation unit.` |
|         - | 5351 | ` *` |
|         - | 5352 | ` * A require/include normally runs at execution time, when no compile is in flight,` |
|         - | 5353 | ` * so VmEvalChunk can call PH7_ResetCodeGenerator to wipe the generator. But an` |
|         - | 5354 | `` * autoload can fire in the MIDDLE of compiling a class: resolving `class Child`` |
|         - | 5355 | `` * extends Base` calls PH7_VmExtractClass, which triggers the autoloader, whose`` |
|         - | 5356 | `` * body `require`s Base's file — a nested compile while the outer one is mid-token.`` |
|         - | 5357 | ` * PH7_ResetCodeGenerator would then blow away the outer compile's cursor` |
|         - | 5358 | ` * (pIn/pEnd), current block, use-imports, labels, etc.; the outer parse resumes` |
|         - | 5359 | ` * with pIn == NULL and reports a bogus "Expected '{' after class 'Child'". (Common` |
|         - | 5360 | ` * in every real framework: Composer autoloads parent classes on demand.)` |
|         - | 5361 | ` *` |
|         - | 5362 | ` * This snapshots the position/scope fields into *pSaved (a caller-stack` |
|         - | 5363 | ` * ph7_gen_state used purely as storage) and re-initializes the live generator's` |
|         - | 5364 | ` * position containers to FRESH, empty ones WITHOUT releasing the outer's (the` |
|         - | 5365 | ` * snapshot now owns those). hLiteral/hNumLiteral/hVar are intentionally left LIVE` |
|         - | 5366 | ` * and shared — compiled bytecode interns names/literals into them and they grow` |
|         - | 5367 | ` * monotonically (exactly why PH7_ResetCodeGenerator keeps them); restoring an old` |
|         - | 5368 | ` * copy of those headers after a nested grow would use-after-free the bucket array.` |
|         - | 5369 | ` */` |
|        10 | 5370 | `PH7_PRIVATE void PH7_CompilerSaveState(ph7_vm *pVm,ph7_gen_state *pSaved,ProcConsumer xErr,void *pErrData)` |
|         2 | 5371 | `{` |
|        12 | 5372 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 5373 | `	/* Shallow-copy every field; the position containers below are then replaced` |
|         - | 5374 | `	 * with fresh ones on the live struct, so *pSaved keeps the outer's memory. */` |
|        12 | 5375 | `	*pSaved = *pGen;` |
|        12 | 5376 | `	SySetInit(&pGen->aLabel,&pVm->sAllocator,sizeof(Label));` |
|        12 | 5377 | `	SySetInit(&pGen->aGoto,&pVm->sAllocator,sizeof(JumpFixup));` |
|        12 | 5378 | `	SySetInit(&pGen->aNullsafeJmp,&pVm->sAllocator,sizeof(sxu32));` |
|        12 | 5379 | `	SySetInit(&pGen->aLoopParent,&pVm->sAllocator,sizeof(sxu32));` |
|        12 | 5380 | `	SySetInit(&pGen->aScope,&pVm->sAllocator,sizeof(GenScope));` |
|        12 | 5381 | `	SySetInit(&pGen->aTrivia,&pVm->sAllocator,sizeof(ph7_trivia));` |
|        12 | 5382 | `	SySetInit(&pGen->aPendingAttrs,&pVm->sAllocator,sizeof(ph7_trivia));` |
|        12 | 5383 | `	SyBlobInit(&pGen->sWorker,&pVm->sAllocator);` |
|        12 | 5384 | `	SyBlobInit(&pGen->sErrBuf,&pVm->sAllocator);` |
|        12 | 5385 | `	SyBlobInit(&pGen->sFirstErr,&pVm->sAllocator);` |
|        12 | 5386 | `	SyBlobInit(&pGen->sNamespace,&pVm->sAllocator);` |
|        12 | 5387 | `	GenStateInitUseImports(&(*pGen),&(*pVm));` |
|        12 | 5388 | `	GenStateInitSeenSymbols(&(*pGen),&(*pVm));` |
|         - | 5389 | `	/* Fresh global scope for the nested unit (address of the embedded sGlobal is` |
|         - | 5390 | `	 * stable, so any outer block still parented to it stays valid across restore). */` |
|        12 | 5391 | `	GenStateInitBlock(pGen,&pGen->sGlobal,GEN_BLOCK_GLOBAL,PH7_VmInstrLength(pVm),0);` |
|        12 | 5392 | `	pGen->pCurrent = &pGen->sGlobal;` |
|        12 | 5393 | `	pGen->pIn = pGen->pEnd = 0;` |
|        12 | 5394 | `	pGen->pRawIn = pGen->pRawEnd = 0;` |
|        12 | 5395 | `	pGen->pTokenSet = 0;` |
|        12 | 5396 | `	pGen->nErr = 0;` |
|        12 | 5397 | `	pGen->nFatal = 0;` |
|        12 | 5398 | `	pGen->nFirstErrLine = 0;` |
|        12 | 5399 | `	pGen->bParseThrows = 0;` |
|        12 | 5400 | `	pGen->iFatalTrace = PH7_FATAL_TRACE_COMPILE;` |
|        12 | 5401 | `	pGen->nLoopId = pGen->nCurLoopId = 0;` |
|        12 | 5402 | `	pGen->nCommaExprOk = 0;` |
|        12 | 5403 | `	pGen->zClauseCloser = 0;` |
|        12 | 5404 | `	pGen->bInGenerator = 0;` |
|        12 | 5405 | `	pGen->bStrictTypes = 0;` |
|        12 | 5406 | `	pGen->bStrictTypesLocked = 0;` |
|        12 | 5407 | `	pGen->bNsNamed = 0;` |
|        12 | 5408 | `	pGen->bNsBracketed = 0;` |
|        12 | 5409 | `	pGen->bInNsBlock = 0;` |
|         - | 5410 | `	/* The nested unit is a fresh top-level compile: it is not lexically inside the` |
|         - | 5411 | `	 * outer's class body nor its member default, so a __TRAIT__ in the nested file` |
|         - | 5412 | `	 * must not inherit the outer's trait. (Restore below carries the outer's values` |
|         - | 5413 | `	 * back, so only the nested unit sees these zeros.) */` |
|        12 | 5414 | `	pGen->pCurClass = 0;` |
|        12 | 5415 | `	pGen->iInMemberDefault = 0;` |
|        12 | 5416 | `	SyStringInitFromBuf(&pGen->sPendingDoc,0,0);` |
|        12 | 5417 | `	pGen->xErr = xErr;` |
|        12 | 5418 | `	pGen->pErrData = pErrData;` |
|        12 | 5419 | `}` |
|         - | 5420 | `/*` |
|         - | 5421 | ` * Restore the outer compile-position state saved by PH7_CompilerSaveState,` |
|         - | 5422 | ` * releasing the nested unit's position containers first. The shared` |
|         - | 5423 | ` * hLiteral/hNumLiteral/hVar tables (grown by the nested compile) are carried` |
|         - | 5424 | ` * forward, NOT rolled back to the snapshot's stale headers.` |
|         - | 5425 | ` */` |
|        10 | 5426 | `PH7_PRIVATE void PH7_CompilerRestoreState(ph7_vm *pVm,ph7_gen_state *pSaved)` |
|         2 | 5427 | `{` |
|        12 | 5428 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - | 5429 | `	GenBlock *pBlock,*pParent;` |
|         - | 5430 | `	SyHash hVar,hLiteral,hNumLiteral;` |
|         - | 5431 | `	/* Free any nested blocks left open (e.g. an aborted nested compile), then the` |
|         - | 5432 | `	 * nested global block's own fixup sets. */` |
|        12 | 5433 | `	pBlock = pGen->pCurrent;` |
|        12 | 5434 | `	while( pBlock && pBlock->pParent != 0 ){` |
|       ! 0 | 5435 | `		pParent = pBlock->pParent;` |
|       ! 0 | 5436 | `		GenStateFreeBlock(pBlock);` |
|       ! 0 | 5437 | `		pBlock = pParent;` |
|       ! 0 | 5438 | `	}` |
|        12 | 5439 | `	GenStateReleaseBlock(&pGen->sGlobal);` |
|         - | 5440 | `	/* Release the nested unit's position containers. */` |
|        12 | 5441 | `	SySetRelease(&pGen->aLabel);` |
|        12 | 5442 | `	SySetRelease(&pGen->aGoto);` |
|        12 | 5443 | `	SySetRelease(&pGen->aNullsafeJmp);` |
|        12 | 5444 | `	SySetRelease(&pGen->aLoopParent);` |
|        12 | 5445 | `	SySetRelease(&pGen->aScope);` |
|        12 | 5446 | `	SySetRelease(&pGen->aTrivia);` |
|        12 | 5447 | `	SySetRelease(&pGen->aPendingAttrs);` |
|        12 | 5448 | `	SyBlobRelease(&pGen->sWorker);` |
|        12 | 5449 | `	SyBlobRelease(&pGen->sErrBuf);` |
|        12 | 5450 | `	SyBlobRelease(&pGen->sFirstErr);` |
|        12 | 5451 | `	SyBlobRelease(&pGen->sNamespace);` |
|        12 | 5452 | `	SyHashRelease(&pGen->hUseImports);` |
|        12 | 5453 | `	SyHashRelease(&pGen->hUseFuncImports);` |
|        12 | 5454 | `	SyHashRelease(&pGen->hUseConstImports);` |
|        12 | 5455 | `	GenStateReleaseSeenSymbols(&(*pGen));` |
|         - | 5456 | `	/* Preserve the (possibly grown) shared intern tables across the restore. */` |
|        12 | 5457 | `	hVar = pGen->hVar;` |
|        12 | 5458 | `	hLiteral = pGen->hLiteral;` |
|        12 | 5459 | `	hNumLiteral = pGen->hNumLiteral;` |
|        12 | 5460 | `	*pGen = *pSaved;` |
|        12 | 5461 | `	pGen->hVar = hVar;` |
|        12 | 5462 | `	pGen->hLiteral = hLiteral;` |
|        12 | 5463 | `	pGen->hNumLiteral = hNumLiteral;` |
|        12 | 5464 | `}` |
|         - | 5465 | `/*` |
|         - | 5466 | ` * Raise php's parse error for an unexpected token: E_PARSE with the exact text` |
|         - | 5467 | ` * php's parser prints, e.g.` |
|         - | 5468 | ` *` |
|         - | 5469 | ` *   syntax error, unexpected token ";", expecting "{"` |
|         - | 5470 | ` *   syntax error, unexpected identifier "invalid", expecting "("` |
|         - | 5471 | ` *   syntax error, unexpected end of file` |
|         - | 5472 | ` *` |
|         - | 5473 | ` * php names the token by CLASS, not just by text: an identifier, a variable and` |
|         - | 5474 | ` * a number each get their own noun, while everything else (keywords, operators,` |
|         - | 5475 | ` * punctuation) is a "token". zExpecting is the optional ", expecting ..." tail` |
|         - | 5476 | ` * — pass NULL when the site cannot say what it wanted (php often can't either).` |
|         - | 5477 | ` * pTok == NULL means the input ran out: "unexpected end of file".` |
|         - | 5478 | ` *` |
|         - | 5479 | ` * PHL's hand-written recursive-descent parser has no bison expectation sets, so` |
|         - | 5480 | ` * a site can only claim an "expecting" clause it genuinely knows; every clause` |
|         - | 5481 | ` * emitted here was verified against php 8.5.7 for the construct in question.` |
|         - | 5482 | ` */` |
|         - | 5483 | `/*` |
|         - | 5484 | `` * Rebuild the `<<<LABEL` marker of a heredoc/nowdoc token from the source the`` |
|         - | 5485 | ` * token's BODY points into: the header always sits immediately above it. Answers` |
|         - | 5486 | ` * FALSE when no marker is found within reach, in which case the caller falls back` |
|         - | 5487 | ` * to the generic noun rather than guessing.` |
|         - | 5488 | ` */` |
|         8 | 5489 | `static int GenStateHeredocMarker(SyString *pBody,SyString *pOut)` |
|         1 | 5490 | `{` |
|         9 | 5491 | `	const unsigned char *z = (const unsigned char *)pBody->zString;` |
|         - | 5492 | `	const unsigned char *zLabelEnd;` |
|         - | 5493 | `	/* Walk the header BACKWARDS from the body, which begins one byte past the` |
|         - | 5494 | `	 * terminator of the marker's own line: line terminator, trailing blanks, the` |
|         - | 5495 | ``	 * closing quote, the LABEL, the opening quote, leading blanks, `<<<`. Every`` |
|         - | 5496 | `	 * step stops on a byte the next step owns, so the walk cannot leave the` |
|         - | 5497 | ``	 * header -- `<` is not a label byte and a label is what sits above the body. */`` |
|         9 | 5498 | `	z--;` |
|         9 | 5499 | `	if( z[0] == '\n' ){` |
|         9 | 5500 | `		z--;` |
|         9 | 5501 | `		if( z[0] == '\r' ){` |
|       ! 0 | 5502 | `			z--;` |
|       ! 0 | 5503 | `		}` |
|         4 | 5504 | `	}` |
|         9 | 5505 | `	while( z[0] == ' ' \|\| z[0] == '\t' ){` |
|       ! 0 | 5506 | `		z--;` |
|       ! 0 | 5507 | `	}` |
|         9 | 5508 | `	if( z[0] == '"' \|\| z[0] == '\'' ){` |
|         5 | 5509 | `		z--;` |
|         2 | 5510 | `	}` |
|         9 | 5511 | `	zLabelEnd = &z[1];` |
|        33 | 5512 | `	while( z[0] >= 0x80 \|\| SyisAlphaNum(z[0]) \|\| z[0] == '_' ){` |
|        25 | 5513 | `		z--;` |
|         1 | 5514 | `	}` |
|         9 | 5515 | `	if( zLabelEnd == &z[1] ){` |
|       ! 0 | 5516 | `		return 0; /* No label: not a header this routine can read back */` |
|         - | 5517 | `	}` |
|         9 | 5518 | `	if( z[0] == '"' \|\| z[0] == '\'' ){` |
|         5 | 5519 | `		z--;` |
|         2 | 5520 | `	}` |
|        15 | 5521 | `	while( z[0] == ' ' \|\| z[0] == '\t' ){` |
|         7 | 5522 | `		z--;` |
|         1 | 5523 | `	}` |
|         9 | 5524 | `	if( !(z[0] == '<' && z[-1] == '<' && z[-2] == '<') ){` |
|       ! 0 | 5525 | `		return 0;` |
|         - | 5526 | `	}` |
|         - | 5527 | ``	/* php's token text runs from `<<<` to the end of the LABEL -- the opening`` |
|         - | 5528 | `	 * quote of a nowdoc is inside it, the closing one is not. */` |
|         9 | 5529 | `	SyStringInitFromBuf(pOut,&z[-2],(sxu32)(zLabelEnd - &z[-2]));` |
|         9 | 5530 | `	return 1;` |
|         5 | 5531 | `}` |
|       472 | 5532 | `PH7_PRIVATE sxi32 PH7_GenSyntaxError(` |
|         - | 5533 | `	ph7_gen_state *pGen,   /* Code generator state */` |
|         - | 5534 | `	SyToken *pTok,         /* Offending token, or NULL for end of file */` |
|         - | 5535 | `	const char *zExpecting /* ", expecting <this>" tail, or NULL */` |
|         - | 5536 | `	)` |
|         5 | 5537 | `{` |
|         - | 5538 | ``	/* php's noun for the offending token. An ALPHA-stream operator (`and`, `or`,`` |
|         - | 5539 | ``	 * `xor`, `new`, `clone`, `instanceof`) is lexed ID\|OP here but php calls it a`` |
|         - | 5540 | `	 * TOKEN, like every other reserved word — only a real identifier gets the` |
|         - | 5541 | `	 * "identifier" noun. */` |
|       477 | 5542 | `	const char *zNoun = "token";` |
|         - | 5543 | `	sxu32 nLine;` |
|       477 | 5544 | `	if( pTok == 0 && pGen->pTokenSet ){` |
|         - | 5545 | `		/* The caller ran out of tokens inside its own slice — but a statement's slice stops` |
|         - | 5546 | `		 * BEFORE its terminator, so the token php actually names (typically the ';') is the` |
|         - | 5547 | `		 * one sitting just past the slice, still inside the chunk's token stream. Reach for` |
|         - | 5548 | `		 * it before concluding "end of file". */` |
|       203 | 5549 | `		SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|       203 | 5550 | `		SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|       203 | 5551 | `		if( pGen->pEnd >= pBase && pGen->pEnd < pStreamEnd ){` |
|       140 | 5552 | `			pTok = pGen->pEnd;` |
|        68 | 5553 | `		}` |
|        99 | 5554 | `	}` |
|       477 | 5555 | `	nLine = pTok ? pTok->nLine : (pGen->pIn > (SyToken *)SySetBasePtr(pGen->pTokenSet) ? pGen->pIn[-1].nLine : 1);` |
|       477 | 5556 | `	if( pTok == 0 && pGen->bChunkAtEof && pGen->nChunkEofLine > nLine ){` |
|         - | 5557 | `		/* End of INPUT is reported where it sits, not where the last token ended. */` |
|         7 | 5558 | `		nLine = pGen->nChunkEofLine;` |
|         3 | 5559 | `	}` |
|       477 | 5560 | `	if( pTok == 0 ){` |
|        96 | 5561 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        31 | 5562 | `			zExpecting ? "syntax error, unexpected end of file, expecting %s"` |
|         - | 5563 | `			           : "syntax error, unexpected end of file",` |
|        31 | 5564 | `			zExpecting);` |
|         - | 5565 | `	}` |
|       415 | 5566 | `	if( pTok->nType & PH7_TK_FQNAME ){` |
|         7 | 5567 | `		zNoun = "fully qualified name";` |
|       412 | 5568 | `	}else if( (pTok->nType & PH7_TK_ID) && (pTok->nType & PH7_TK_OP) == 0 ){` |
|        49 | 5569 | `		zNoun = "identifier";` |
|       387 | 5570 | `	}else if( pTok->nType & PH7_TK_DOLLAR ){` |
|        13 | 5571 | `		zNoun = "variable";` |
|         - | 5572 | `		/* The '$' is its own token and carries only "$" as text; the NAME is the` |
|         - | 5573 | ``		 * token after it. php names the whole variable, so `$x` was being reported`` |
|         - | 5574 | ``		 * as the nameless `variable "$"`. Stitch the two back together. */`` |
|        13 | 5575 | `		if( pGen->pTokenSet ){` |
|        13 | 5576 | `			SyToken *pBase = (SyToken *)SySetBasePtr(pGen->pTokenSet);` |
|        13 | 5577 | `			SyToken *pStreamEnd = &pBase[SySetUsed(pGen->pTokenSet)];` |
|        13 | 5578 | `			SyToken *pName = &pTok[1];` |
|        10 | 5579 | `			if( pTok >= pBase && pName < pStreamEnd` |
|        10 | 5580 | `				&& (pName->nType & (PH7_TK_ID\|PH7_TK_KEYWORD))` |
|        13 | 5581 | `				&& pName->sData.nByte > 0 ){` |
|        13 | 5582 | `				SyBlobReset(&pGen->sWorker);` |
|        13 | 5583 | `				SyBlobAppend(&pGen->sWorker,"$",sizeof(char));` |
|        13 | 5584 | `				SyBlobAppend(&pGen->sWorker,pName->sData.zString,pName->sData.nByte);` |
|         - | 5585 | `				{` |
|         - | 5586 | `					SyString sVar;` |
|        13 | 5587 | `					SyStringInitFromBuf(&sVar,SyBlobData(&pGen->sWorker),` |
|         - | 5588 | `						SyBlobLength(&pGen->sWorker));` |
|        13 | 5589 | `					if( zExpecting ){` |
|        15 | 5590 | `						return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|         - | 5591 | `							"syntax error, unexpected %s \"%z\", expecting %s",` |
|         4 | 5592 | `							zNoun,&sVar,zExpecting);` |
|         - | 5593 | `					}` |
|         4 | 5594 | `					return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|         1 | 5595 | `						"syntax error, unexpected %s \"%z\"",zNoun,&sVar);` |
|         - | 5596 | `				}` |
|         - | 5597 | `			}` |
|       ! 0 | 5598 | `		}` |
|       355 | 5599 | `	}else if( pTok->nType & PH7_TK_INTEGER ){` |
|        32 | 5600 | `		zNoun = "integer";` |
|       341 | 5601 | `	}else if( pTok->nType & PH7_TK_REAL ){` |
|         - | 5602 | ``		/* php's noun, which is not the type name: `float` is what the CAST is`` |
|         - | 5603 | ``		 * called, `floating-point number` what a stray literal is called. */`` |
|         7 | 5604 | `		zNoun = "floating-point number";` |
|       324 | 5605 | `	}else if( pTok->nType & (PH7_TK_SSTR\|PH7_TK_DSTR) ){` |
|         - | 5606 | `		/* php names a string literal by the QUOTE it was written with, and prints` |
|         - | 5607 | `		 * the SOURCE bytes between the quotes -- escapes unresolved, which is what` |
|         - | 5608 | `		 * the token already holds here. A double-quoted string that INTERPOLATES is` |
|         - | 5609 | `		 * not one token in php at all: its scanner emits the opening quote on its` |
|         - | 5610 | `		 * own, so the parser has nothing to quote and the noun stands alone. */` |
|        19 | 5611 | `		if( (pTok->nType & PH7_TK_DSTR) && GenStateDqInterpolates(&pTok->sData) ){` |
|         5 | 5612 | `			if( zExpecting ){` |
|         7 | 5613 | `				return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|         2 | 5614 | `					"syntax error, unexpected double-quote mark, expecting %s",zExpecting);` |
|         - | 5615 | `			}` |
|       ! 0 | 5616 | `			return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|         - | 5617 | `				"syntax error, unexpected double-quote mark");` |
|         - | 5618 | `		}` |
|        15 | 5619 | `		zNoun = (pTok->nType & PH7_TK_SSTR) ? "single-quoted string" : "double-quoted string";` |
|       310 | 5620 | `	}else if( pTok->nType & (PH7_TK_HEREDOC\|PH7_TK_NOWDOC) ){` |
|         - | 5621 | ``		/* php names the OPENING marker -- `<<<EOT`, or `<<<'EOT` for a nowdoc, the`` |
|         - | 5622 | `		 * closing quote dropped because the token text ends at the label -- and` |
|         - | 5623 | `		 * reports it on the line AFTER the marker's, its scanner having consumed` |
|         - | 5624 | `		 * that line's terminator before the token is handed over. The token here` |
|         - | 5625 | `		 * carries the BODY, so the marker is read back off the source it points` |
|         - | 5626 | `		 * into. */` |
|         - | 5627 | `		SyString sMark;` |
|         9 | 5628 | `		if( GenStateHeredocMarker(&pTok->sData,&sMark) ){` |
|         9 | 5629 | `			if( zExpecting ){` |
|        13 | 5630 | `				return PH7_GenCompileError(pGen,E_PARSE,nLine + 1,` |
|         4 | 5631 | `					"syntax error, unexpected heredoc start \"%z\", expecting %s",&sMark,zExpecting);` |
|         - | 5632 | `			}` |
|       ! 0 | 5633 | `			return PH7_GenCompileError(pGen,E_PARSE,nLine + 1,` |
|         - | 5634 | `				"syntax error, unexpected heredoc start \"%z\"",&sMark);` |
|         - | 5635 | `		}` |
|       ! 0 | 5636 | `	}` |
|       393 | 5637 | `	if( zExpecting ){` |
|       275 | 5638 | `		return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|        90 | 5639 | `			"syntax error, unexpected %s \"%z\", expecting %s",zNoun,&pTok->sData,zExpecting);` |
|         - | 5640 | `	}` |
|       317 | 5641 | `	return PH7_GenCompileError(pGen,E_PARSE,nLine,` |
|       104 | 5642 | `		"syntax error, unexpected %s \"%z\"",zNoun,&pTok->sData);` |
|       241 | 5643 | `}` |
|         - | 5644 | `/*` |
|         - | 5645 | ` * Generate a compile-time error message.` |
|         - | 5646 | ` * If the error count limit is reached (usually 15 error message)` |
|         - | 5647 | ` * this function return SXERR_ABORT.In that case upper-layers must` |
|         - | 5648 | ` * abort compilation immediately.` |
|         - | 5649 | ` */` |
|         - | 5650 | `/*` |
|         - | 5651 | `` * php's `Stack trace:` block under a compile-time FATAL: the activations that are`` |
|         - | 5652 | ` * live at the refusal -- the enclosing functions, and the include/require/eval that` |
|         - | 5653 | `` * loaded the unit being compiled -- then the `#N {main}` marker. A parse error gets`` |
|         - | 5654 | ` * none: that one is the parser's own refusal and php reports it as an E_PARSE.` |
|         - | 5655 | ` *` |
|         - | 5656 | ` * A refusal raised while the VM is still INITIALIZING is the main script's own` |
|         - | 5657 | ` * compile: there is no runtime state to walk (and no object pool to build the array` |
|         - | 5658 | ` * in), and php's answer there is the bare bottom marker.` |
|         - | 5659 | ` */` |
|       774 | 5660 | `PH7_PRIVATE void PH7_GenAppendFatalTrace(ph7_vm *pVm,SyBlob *pOut,int iTraceKind)` |
|         4 | 5661 | `{` |
|         - | 5662 | `	ph7_value *pTrace;` |
|       778 | 5663 | `	if( pVm == 0 \|\| iTraceKind == PH7_FATAL_TRACE_NONE ){` |
|        40 | 5664 | `		return;` |
|         - | 5665 | `	}` |
|       742 | 5666 | `	SyBlobAppend(pOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|       742 | 5667 | `	if( pVm->nMagic == PH7_VM_INIT ){` |
|       720 | 5668 | `		SyBlobAppend(pOut,"#0 {main}",sizeof("#0 {main}")-1);` |
|       720 | 5669 | `		return;` |
|         - | 5670 | `	}` |
|        25 | 5671 | `	pTrace = ph7_new_array(&(*pVm));` |
|        25 | 5672 | `	if( pTrace == 0 ){` |
|       ! 0 | 5673 | `		SyBlobAppend(pOut,"#0 {main}",sizeof("#0 {main}")-1);` |
|       ! 0 | 5674 | `		return;` |
|         - | 5675 | `	}` |
|         - | 5676 | `	/* php's fatal trace carries no argument list. Nor, unless this is one of the` |
|         - | 5677 | `	 * refusals php makes at RUN time, the include/require/eval that loaded the unit` |
|         - | 5678 | `	 * being compiled: php raises a compile error before it pushes that activation. */` |
|        25 | 5679 | `	VmBuildBacktrace(&(*pVm),0x2 \| (iTraceKind == PH7_FATAL_TRACE_RUNTIME ? 0 : 0x4),0,pTrace);` |
|        25 | 5680 | `	PH7_VmTraceToString(&(*pVm),pTrace,TRUE,pOut);` |
|        25 | 5681 | `	ph7_release_value(&(*pVm),pTrace);` |
|       391 | 5682 | `}` |
|      1828 | 5683 | `PH7_PRIVATE sxi32 PH7_GenCompileError(ph7_gen_state *pGen,sxi32 nErrType,sxu32 nLine,const char *zFormat,...)` |
|         5 | 5684 | `{` |
|      1833 | 5685 | `	SyBlob *pWorker = &pGen->sErrBuf;` |
|         - | 5686 | `	SyBlob sLocal;` |
|      1833 | 5687 | `	int bLocal = 0;` |
|      1833 | 5688 | `	const char *zErr = "Error";` |
|         - | 5689 | `	SyString *pFile;` |
|         - | 5690 | `	va_list ap;` |
|      1833 | 5691 | `	sxu32 nBare = 0;` |
|         - | 5692 | `	sxi32 rc;` |
|      1833 | 5693 | `	if( pGen->xErr == 0 && nErrType != E_ERROR && nErrType != E_PARSE ){` |
|         - | 5694 | `		/* Nobody is logging -- an eval()'d chunk -- and this is NOT a refusal. The` |
|         - | 5695 | `		 * generator's buffer is that chunk's one-message store, holding the text a` |
|         - | 5696 | `		 * ParseError will carry, so a deprecation or a warning must neither displace` |
|         - | 5697 | `		 * it nor append to it. php still prints these from inside an eval, so build` |
|         - | 5698 | `		 * the sentence somewhere of our own and emit it. */` |
|         3 | 5699 | `		SyBlobInit(&sLocal,&pGen->pVm->sAllocator);` |
|         3 | 5700 | `		pWorker = &sLocal;` |
|         3 | 5701 | `		bLocal = 1;` |
|         1 | 5702 | `	}` |
|         - | 5703 | `	/* Reset the working buffer. NOT when nobody is logging: there the buffer is a` |
|         - | 5704 | `	 * one-message store eval() reads its ParseError text out of, and php stops at` |
|         - | 5705 | `	 * the FIRST error where this generator carries on to a budget of fifteen -- so` |
|         - | 5706 | ``	 * resetting handed eval the LAST message. `eval('echo 1 foo;')` reported`` |
|         - | 5707 | ``	 * `unexpected token ";"`, the synchronizer's own complaint, where php names the`` |
|         - | 5708 | ``	 * `identifier "foo"` it choked on. */`` |
|      1833 | 5709 | `	if( pGen->xErr ){` |
|      1691 | 5710 | `		SyBlobReset(pWorker);` |
|       843 | 5711 | `	}` |
|         - | 5712 | `	/* Peek the processed file path if available */` |
|      1833 | 5713 | `	pFile = (SyString *)SySetPeek(&pGen->pVm->aFiles);` |
|      1833 | 5714 | `	if( nErrType == E_ERROR \|\| nErrType == E_PARSE ){` |
|         - | 5715 | `		/* Increment the error counter. A PARSE error is every bit as fatal as an` |
|         - | 5716 | `		 * E_ERROR one: php compiles nothing, runs nothing and exits 255. Counting` |
|         - | 5717 | `		 * only E_ERROR let a parse error print its diagnostic and then fall through` |
|         - | 5718 | `		 * into execution with a 0 exit status. */` |
|      1709 | 5719 | `		pGen->nErr++;` |
|      1709 | 5720 | `		if( nErrType == E_ERROR ){` |
|         - | 5721 | `			/* php's E_COMPILE_ERROR: an uncatchable fatal, where a parse error is a` |
|         - | 5722 | `			 * catchable ParseError. The include path reads this to tell them apart. */` |
|       906 | 5723 | `			pGen->nFatal++;` |
|       451 | 5724 | `		}` |
|      1709 | 5725 | `		if( pGen->nErr == 1 ){` |
|         - | 5726 | `			/* Keep the FIRST refusal's bare text and line: it is the one php reports,` |
|         - | 5727 | `			 * and it is the message an include's ParseError carries. */` |
|         - | 5728 | `			va_list apF;` |
|      1355 | 5729 | `			SyBlobReset(&pGen->sFirstErr);` |
|      1355 | 5730 | `			va_start(apF,zFormat);` |
|      1355 | 5731 | `			SyBlobFormatAp(&pGen->sFirstErr,zFormat,apF);` |
|      1355 | 5732 | `			va_end(apF);` |
|      1355 | 5733 | `			pGen->nFirstErrLine = nLine;` |
|       680 | 5734 | `		}else{` |
|         - | 5735 | `			/* php stops at the first one. This generator recovers and carries on so` |
|         - | 5736 | `			 * that the rest of the unit is still walked (a later pass needs the` |
|         - | 5737 | `			 * symbols), but everything it says after the first refusal is its own` |
|         - | 5738 | `			 * recovery talking -- and printing it put diagnostics on the user's` |
|         - | 5739 | `			 * screen that php, having stopped, never reaches.` |
|         - | 5740 | `			 *` |
|         - | 5741 | `			 * The recovery is still bounded, and now SILENTLY: the old limit` |
|         - | 5742 | ``			 * announced itself with a `Error count limit reached` line of PH7's own`` |
|         - | 5743 | `			 * invention, which no php prints and which would land on top of the one` |
|         - | 5744 | `			 * diagnostic php does. The unit has already failed and its first message` |
|         - | 5745 | `			 * is already recorded, so there is nothing left to say. */` |
|       358 | 5746 | `			return (pGen->nErr > 15) ? SXERR_ABORT : SXRET_OK;` |
|         - | 5747 | `		}` |
|       675 | 5748 | `	}` |
|      1479 | 5749 | `	if( nErrType == E_PARSE && pGen->bParseThrows ){` |
|         - | 5750 | `		/* An include/require unit: php's parser throws a ParseError rather than` |
|         - | 5751 | `		 * printing, and the text only reaches the screen if nobody catches it.` |
|         - | 5752 | `		 * The caller (VmEvalChunk) raises it from sFirstErr. */` |
|         8 | 5753 | `		return SXRET_OK;` |
|         - | 5754 | `	}` |
|      1473 | 5755 | `	if( pGen->xErr == 0 && !bLocal ){` |
|         - | 5756 | `		/* No consumer — but keep the BARE message in the error buffer so a caller` |
|         - | 5757 | `		 * that needs the text can read it back. eval() compiles with logging off` |
|         - | 5758 | `		 * (a parse error there is php's catchable ParseError, not a printed` |
|         - | 5759 | `		 * diagnostic) and needs exactly this string for the exception message. The` |
|         - | 5760 | `		 * first message stands: everything after it is this generator's recovery` |
|         - | 5761 | `		 * talking, and php never got that far. */` |
|       100 | 5762 | `		if( SyBlobLength(pWorker) < 1 ){` |
|       100 | 5763 | `			va_start(ap,zFormat);` |
|       100 | 5764 | `			SyBlobFormatAp(pWorker,zFormat,ap);` |
|       100 | 5765 | `			va_end(ap);` |
|        48 | 5766 | `		}` |
|       100 | 5767 | `		return SXRET_OK;` |
|         - | 5768 | `	}` |
|      1377 | 5769 | `	switch(nErrType){` |
|       768 | 5770 | `	case E_ERROR:   zErr = "Fatal error"; break;` |
|        63 | 5771 | `	case E_WARNING: zErr = "Warning";     break;` |
|        30 | 5772 | `	case 128 /* E_COMPILE_WARNING */: zErr = "Warning"; break;` |
|       488 | 5773 | `	case E_PARSE:   zErr = "Parse error"; break;` |
|       ! 0 | 5774 | `	case E_NOTICE:  zErr = "Notice";      break;` |
|       ! 0 | 5775 | `	case E_USER_ERROR:   zErr = "User error";   break;` |
|       ! 0 | 5776 | `	case E_USER_WARNING: zErr = "User warning"; break;` |
|       ! 0 | 5777 | `	case E_USER_NOTICE:  zErr = "User notice";  break;` |
|        40 | 5778 | `	case 8192 /* E_DEPRECATED */: zErr = "Deprecated"; break;` |
|       ! 0 | 5779 | `	default:` |
|       ! 0 | 5780 | `		break;` |
|         - | 5781 | `	}` |
|      1377 | 5782 | `	rc = SXRET_OK;` |
|         - | 5783 | ``	/* The BODY only -- `<message> in <file> on line <line>` plus a fatal's trace.`` |
|         - | 5784 | `	 * The label and the two copies php wraps it in are the emitter's` |
|         - | 5785 | `	 * (PH7_VmEmitCompileDiagnostic): a compile diagnostic owes the same log/display` |
|         - | 5786 | `	 * pair a runtime one does, and this used to write a single log-shaped copy to` |
|         - | 5787 | `	 * the engine's compile-error consumer whatever the ini said. */` |
|      1377 | 5788 | `	va_start(ap,zFormat);` |
|      1377 | 5789 | `	SyBlobFormatAp(pWorker,zFormat,ap);` |
|      1377 | 5790 | `	va_end(ap);` |
|         - | 5791 | `	/* Where php's own sentence ENDS. Everything appended below is the location` |
|         - | 5792 | `	 * tail and a fatal's trace, which belong to the printed copies alone -- the` |
|         - | 5793 | `	 * user handler and error_get_last() get the sentence and the line as` |
|         - | 5794 | `	 * separate fields, the way a runtime diagnostic hands them over. */` |
|      1377 | 5795 | `	nBare = SyBlobLength(pWorker);` |
|      1377 | 5796 | `	if( pFile ){` |
|      1377 | 5797 | `		SyBlobFormat(pWorker," in %.*s on line %u",pFile->nByte,pFile->zString,nLine);` |
|       686 | 5798 | `	}` |
|      1377 | 5799 | `	if( nErrType == E_ERROR ){` |
|       768 | 5800 | `		PH7_GenAppendFatalTrace(pGen->pVm,pWorker,pGen->iFatalTrace);` |
|       382 | 5801 | `	}` |
|         - | 5802 | `	/* iFatalTrace is a ONE-SHOT: a site that raises one of php's non-compiler` |
|         - | 5803 | `	 * refusals sets it just before the call and this consumes it, so no site has to` |
|         - | 5804 | `	 * remember to put it back and none of them can leak it onto a later refusal. */` |
|      1377 | 5805 | `	pGen->iFatalTrace = PH7_FATAL_TRACE_COMPILE;` |
|      1377 | 5806 | `	if( SyBlobLength(pWorker) > 0 ){` |
|         - | 5807 | `		/* php's error_reporting BIT for this diagnostic. The compiler's own` |
|         - | 5808 | `		 * refusals are E_COMPILE_ERROR and E_PARSE rather than E_ERROR, and a` |
|         - | 5809 | `		 * site that means php's E_COMPILE_WARNING says 128 outright; everything` |
|         - | 5810 | `		 * else (a compile-time E_WARNING, which is what php raises for the` |
|         - | 5811 | ``		 * `continue`-targeting-switch and magic-visibility rules) passes its own`` |
|         - | 5812 | `		 * level through. */` |
|         - | 5813 | `		sxi32 iPhpErr;` |
|      1377 | 5814 | `		switch( nErrType ){` |
|       768 | 5815 | `		case E_ERROR: iPhpErr = 64 /* E_COMPILE_ERROR */; break;` |
|       488 | 5816 | `		case E_PARSE: iPhpErr = 4  /* E_PARSE */;         break;` |
|       129 | 5817 | `		default:      iPhpErr = nErrType;                 break;` |
|         - | 5818 | `		}` |
|      2063 | 5819 | `		PH7_VmEmitCompileDiagnostic(pGen->pVm,iPhpErr,zErr,` |
|      1372 | 5820 | `			(const char *)SyBlobData(pWorker),SyBlobLength(pWorker),` |
|      1372 | 5821 | `			(const char *)SyBlobData(pWorker),nBare,nLine);` |
|       686 | 5822 | `	}` |
|      1377 | 5823 | `	if( bLocal ){` |
|         3 | 5824 | `		SyBlobRelease(&sLocal);` |
|         1 | 5825 | `	}` |
|      1377 | 5826 | `	return rc;` |
|       919 | 5827 | `}` |
|         - | 5828 |  |
